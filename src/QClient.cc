// ----------------------------------------------------------------------
// File: QClient.cc
// Author: Georgios Bitzes - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * qclient - A simple redis C++ client with support for redirects       *
 * Copyright (C) 2016 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

#include "qclient/QClient.hh"
#include "qclient/Utils.hh"

#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstream>
#include <iterator>

using namespace qclient;

//------------------------------------------------------------------------------
// The intercepts machinery
//------------------------------------------------------------------------------
std::mutex QClient::interceptsMutex;
std::map<std::pair<std::string, int>, std::pair<std::string, int>>
    QClient::intercepts;


void QClient::addIntercept(const std::string& hostname, int port,
                           const std::string& host2, int port2)
{
  std::lock_guard<std::mutex> lock(interceptsMutex);
  intercepts[std::make_pair(hostname, port)] = std::make_pair(host2, port2);
}

void QClient::clearIntercepts()
{
  std::lock_guard<std::mutex> lock(interceptsMutex);
  intercepts.clear();
}

//------------------------------------------------------------------------------
// QClient class implementation
//------------------------------------------------------------------------------
QClient::QClient(const std::string& host_, const int port_, bool redirects,
                 bool exceptions, std::vector<std::string> handshake)
  : host(host_), port(port_), transparentRedirects(redirects),
    exceptionsEnabled(exceptions), handshakeCommand(handshake)
{
  startEventLoop();
}

QClient::~QClient()
{
  shutdown = true;
  shutdownEventFD.notify();
  eventLoopThread.join();
  cleanup();
}

std::future<redisReplyPtr>
QClient::execute(const std::vector<std::string>& req)
{
  std::uint64_t indx = 0;
  const char* cstr[req.size()];
  size_t sizes[req.size()];

  for (const auto& elem: req) {
    cstr[indx] = elem.data();
    sizes[indx] = elem.size();
    ++indx;
  }

  return execute(req.size(), cstr, sizes);
}

std::future<redisReplyPtr> QClient::execute(const char* buffer,
                                            const size_t len)
{
  std::lock_guard<std::recursive_mutex> lock(mtx);

  // not connected temporarily?
  if (sock <= 0) {
    std::promise<redisReplyPtr> prom;

    // not available at all?
    if(exceptionsEnabled && !available) {
      prom.set_exception(std::make_exception_ptr(std::runtime_error("unavailable")));
    }
    else {
      prom.set_value(redisReplyPtr());
    }

    return prom.get_future();
  }

  if(send(sock, buffer, len, 0) < 0) {
    std::cerr << "qclient: error during send(): " << errno << ", "
              << strerror(errno) << std::endl;
    ::shutdown(sock, SHUT_RDWR);

    std::promise<redisReplyPtr> prom;
    prom.set_value(redisReplyPtr());
    return prom.get_future();
  }

  promises.emplace();
  return promises.back().get_future();
}

std::future<redisReplyPtr> QClient::execute(size_t nchunks, const char** chunks,
                                            const size_t* sizes)
{
  char* buffer = NULL;
  int len = redisFormatCommandArgv(&buffer, nchunks, chunks, sizes);
  std::future<redisReplyPtr> ret = execute(buffer, len);
  free(buffer);
  return ret;
}

void QClient::startEventLoop()
{
  this->connect();
  eventLoopThread = std::thread(&QClient::eventLoop, this);
}

bool QClient::feed(const char* buf, size_t len)
{
  if (len > 0) {
    redisReaderFeed(reader, buf, len);
  }

  while (true) {
    void* reply = NULL;

    if (redisReaderGetReply(reader, &reply) == REDIS_ERR) {
      return false;
    }

    if (reply == NULL) {
      break;
    }

    // new request to process
    if (!promises.empty()) {
      redisReply* rr = (redisReply*) reply;

      if (transparentRedirects && rr->type == REDIS_REPLY_ERROR &&
          strncmp(rr->str, "MOVED ", strlen("MOVED ")) == 0) {
        std::vector<std::string> response = split(std::string(rr->str, rr->len), " ");
        RedisServer redirect;

        if (response.size() == 3 && parseServer(response[2], redirect)) {
          redirectedHost = redirect.host;
          redirectedPort = redirect.port;
          return false;
        }
      }

      promises.front().set_value(redisReplyPtr((redisReply*) reply, freeReplyObject));
      promises.pop();
    }
  }

  return true;
}

void QClient::cleanup()
{
  if (sock > 0) {
    ::shutdown(sock, SHUT_RDWR);
    close(sock);
    sock = -1;
  }

  if (reader != nullptr) {
    redisReaderFree(reader);
    reader = nullptr;
  }

  // return NULL to all pending requests
  while (!promises.empty()) {
    promises.front().set_value(redisReplyPtr());
    promises.pop();
  }
}

void QClient::connectTCP()
{
  struct addrinfo hints, *servinfo, *p;
  int tmpsock, rv;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  if ((rv = getaddrinfo(targetHost.c_str(), std::to_string(targetPort).c_str(),
                        &hints, &servinfo)) != 0) {
    std::cerr << "qclient: error when resolving " << targetHost << ": " <<
              gai_strerror(rv) << std::endl;
    return;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((tmpsock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      continue;
    }

    if (::connect(tmpsock, p->ai_addr, p->ai_addrlen) == -1) {
      close(tmpsock);
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    available = false;
    return;
  }

  available = true;
  sock = tmpsock; // atomic set
}

void QClient::connect()
{
  std::unique_lock<std::recursive_mutex> lock(mtx);
  cleanup();
  targetHost = host;
  targetPort = port;
  processRedirection();
  discoverIntercept();
  reader = redisReaderCreate();
  connectTCP();

  if (!handshakeCommand.empty()) {
    execute(handshakeCommand);
  }
}

void QClient::eventLoop()
{
  const size_t BUFFER_SIZE = 1024 * 2;
  char buffer[BUFFER_SIZE];
  signal(SIGPIPE, SIG_IGN);
  std::chrono::milliseconds backoff(1);

  while (true) {
    std::unique_lock<std::recursive_mutex> lock(mtx);
    struct pollfd polls[2];
    polls[0].fd = shutdownEventFD.getFD();
    polls[0].events = POLLIN;
    polls[1].fd = sock;
    polls[1].events = POLLIN;

    while (sock > 0) {
      lock.unlock();
      poll(polls, 2, 1);
      lock.lock();

      if (shutdown) {
        break;
      }

      // legit connection, reset backoff
      backoff = std::chrono::milliseconds(1);

      if (polls[1].revents != 0) {
        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);

        if (bytes <= 0) {
          break;
        }

        if (!feed(buffer, bytes)) {
          break;
        }
      }
    }

    if (shutdown) {
      feed(NULL, 0);
      break;
    }

    lock.unlock();
    std::this_thread::sleep_for(backoff);

    if (backoff < std::chrono::milliseconds(2048)) {
      backoff++;
    }

    this->connect();
  }
}

void QClient::processRedirection()
{
  if (!redirectedHost.empty() && redirectedPort > 0) {
    std::cerr << "qclient: redirecting to " << redirectedHost << ":" <<
              redirectedPort << std::endl;
    targetHost = redirectedHost;
    targetPort = redirectedPort;
    redirectionActive = true;
  } else if (redirectionActive) {
    std::cerr << "qclient: redirecting back to original host " << host << ":" <<
              port << std::endl;
    redirectionActive = false;
  }

  redirectedHost.clear();
  redirectedPort = -1;
}

void QClient::discoverIntercept()
{
  // If this (host, port) pair is being intercepted, redirect to a different
  // (host, port) pair instead.
  std::lock_guard<std::mutex> lock(interceptsMutex);
  auto it = intercepts.find(std::make_pair(targetHost, targetPort));

  if (it != intercepts.end()) {
    targetHost = it->second.first;
    targetPort = it->second.second;
  }
}

//------------------------------------------------------------------------------
// Wrapper function for exists command
//------------------------------------------------------------------------------
long long int
QClient::exists(const std::string& key)
{
  redisReplyPtr reply = HandleResponse({"EXISTS", key});

  if (reply->type != REDIS_REPLY_INTEGER) {
    throw std::runtime_error("[FATAL] Error exists key: " + key +
                             ": Unexpected reply type: " +
                             std::to_string(reply->type));
  }

  return reply->integer;
}

//------------------------------------------------------------------------------
// Wrapper function for del async command
//------------------------------------------------------------------------------
AsyncResponseType
QClient::del_async(const std::string& key)
{
  std::vector<std::string> cmd {"DEL", key};
  return std::make_pair(execute(cmd), std::move(cmd));
}

//------------------------------------------------------------------------------
// Wrapper function for del command
//------------------------------------------------------------------------------
long long int
QClient::del(const std::string& key)
{
  redisReplyPtr reply = HandleResponse(del_async(key));

  if (reply->type != REDIS_REPLY_INTEGER) {
    throw std::runtime_error("[FATAL] Error del key: " + key +
                             ": Unexpected reply type: " +
                             std::to_string(reply->type));
  }

  return reply->integer;
}

//------------------------------------------------------------------------------
// Handle response
//------------------------------------------------------------------------------
redisReplyPtr
QClient::HandleResponse(AsyncResponseType resp)
{
  int num_retries = 5;
  std::chrono::milliseconds delay(10);
  redisReplyPtr reply = resp.first.get();

  // Handle transient errors by resubmitting the request
  while (!reply && (num_retries > 0)) {
    auto futur = execute(resp.second);
    reply = futur.get();
    --num_retries;
    std::this_thread::sleep_for(delay);
  }

  if (!reply) {
    throw std::runtime_error("[FATAL] Unable to connect to KV-backend");
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    auto cmd = resp.second;
    std::ostringstream oss;
    std::copy(cmd.begin(), cmd.end(),
              std::ostream_iterator<std::string>(oss, " "));
    throw std::runtime_error("[FATAL] Error REDIS_REPLY_ERROR for command: "
                             + oss.str());
  }

  return reply;
}

//------------------------------------------------------------------------------
// Handle response - convenience function
//------------------------------------------------------------------------------
redisReplyPtr
QClient::HandleResponse(std::vector<std::string> cmd)
{
  auto future = execute(cmd);
  return HandleResponse(std::make_pair(std::move(future),
                                       std::move(cmd)));
}
