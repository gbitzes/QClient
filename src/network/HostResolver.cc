//------------------------------------------------------------------------------
// File: HostResolver.cc
// Author: Georgios Bitzes - CERN
//------------------------------------------------------------------------------

/************************************************************************
 * qclient - A simple redis C++ client with support for redirects       *
 * Copyright (C) 2019 CERN/Switzerland                                  *
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

#include "qclient/network/HostResolver.hh"
#include "qclient/GlobalInterceptor.hh"
#include "qclient/Logger.hh"
#include "qclient/Status.hh"
#include "qclient/GlobalInterceptor.hh"
#include <arpa/inet.h>
#include <string.h>

#define SSTR(message) static_cast<std::ostringstream&>(std::ostringstream().flush() << message).str()

namespace qclient {

//------------------------------------------------------------------------------
// Protocol type as string
//------------------------------------------------------------------------------
std::string protocolTypeToString(ProtocolType prot) {
  switch(prot) {
    case ProtocolType::kIPv4: {
      return "IPv4";
    }
    case ProtocolType::kIPv6: {
      return "IPv6";
    }
  }

  return "unknown protocol";
}

//------------------------------------------------------------------------------
// Socket type as string
//------------------------------------------------------------------------------
std::string socketTypeToString(SocketType sock) {
  switch(sock) {
    case SocketType::kStream: {
      return "stream";
    }
    case SocketType::kDatagram: {
      return "datagram";
    }
  }

  return "unknown socket";
}

//------------------------------------------------------------------------------
// Empty constructor
//------------------------------------------------------------------------------
ServiceEndpoint::ServiceEndpoint() {}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
ServiceEndpoint::ServiceEndpoint(ProtocolType protocol, SocketType socket,
  const std::vector<char> addr, const std::string &original)
: protocolType(protocol), socketType(socket), address(addr),
  originalHostname(original) { }

//------------------------------------------------------------------------------
// Constructor, taking the IP address as text and a port, not sockaddr bytes
//------------------------------------------------------------------------------
ServiceEndpoint::ServiceEndpoint(ProtocolType protocol, SocketType socket,
    const std::string &addr, int port, const std::string &original)
: protocolType(protocol), socketType(socket), originalHostname(original) {

  if(protocolType == ProtocolType::kIPv4) {
    struct sockaddr_in out;
    memset(&out, 0, sizeof(struct sockaddr_in));
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    inet_pton(AF_INET, addr.c_str(), &(out.sin_addr));

    address.resize(sizeof(struct sockaddr_in));
    memcpy(address.data(), &out, sizeof(struct sockaddr_in));
  }
  else if(protocolType == ProtocolType::kIPv6) {
    struct sockaddr_in6 out;
    memset(&out, 0, sizeof(sockaddr_in6));
    out.sin6_family = AF_INET6;
    out.sin6_port = htons(port);
    inet_pton(AF_INET6, addr.c_str(), &(out.sin6_addr));

    address.resize(sizeof(struct sockaddr_in6));
    memcpy(address.data(), &out, sizeof(struct sockaddr_in6));
  }
}

//------------------------------------------------------------------------------
// Get stored protocol type
//------------------------------------------------------------------------------
ProtocolType ServiceEndpoint::getProtocolType() const {
  return protocolType;
}

//------------------------------------------------------------------------------
// Get socket type
//------------------------------------------------------------------------------
SocketType ServiceEndpoint::getSocketType() const {
  return socketType;
}

//------------------------------------------------------------------------------
// Get raw address bytes (the form ::connect expects)
//------------------------------------------------------------------------------
const std::vector<char>& ServiceEndpoint::getAddressBytes() const {
  return address;
}

//----------------------------------------------------------------------------
// Get printable address (ie 127.0.0.1) that a human would expect
//----------------------------------------------------------------------------
std::string ServiceEndpoint::getPrintableAddress() const {
  char buffer[INET6_ADDRSTRLEN];

  switch(protocolType) {
    case ProtocolType::kIPv4: {
      const struct sockaddr_in* sockaddr = (const struct sockaddr_in*)(address.data());
      inet_ntop(AF_INET, &(sockaddr->sin_addr), buffer, INET6_ADDRSTRLEN);
      break;
    }
    case ProtocolType::kIPv6: {
      const struct sockaddr_in6* sockaddr = (const struct sockaddr_in6*)(address.data());
      inet_ntop(AF_INET6, &(sockaddr->sin6_addr), buffer, INET6_ADDRSTRLEN);
      break;
    }
  }

  return buffer;
}

//------------------------------------------------------------------------------
// Get service port number
//------------------------------------------------------------------------------
uint16_t ServiceEndpoint::getPort() const {

  switch(protocolType) {
    case ProtocolType::kIPv4: {
      const struct sockaddr_in* sockaddr = (const struct sockaddr_in*)(address.data());
      return htons(sockaddr->sin_port);
    }
    case ProtocolType::kIPv6: {
      const struct sockaddr_in6* sockaddr = (const struct sockaddr_in6*)(address.data());
      return ntohs(sockaddr->sin6_port);
    }
  }

  return 0; // should never happen
}

//------------------------------------------------------------------------------
// Describe as a string
//----------------------------------------------- ------------------------------
std::string ServiceEndpoint::getString() const {
  std::ostringstream ss;
  ss << "[" << getPrintableAddress() << "]" << ":" << getPort() << " ("  << protocolTypeToString(protocolType) << "," <<
    socketTypeToString(socketType) << " resolved from " << originalHostname << ")";

  return ss.str();
}

//----------------------------------------------------------------------------
// Get ai_family to pass to ::connect
//----------------------------------------------------------------------------
int ServiceEndpoint::getAiFamily() const {
  switch(protocolType) {
    case ProtocolType::kIPv4: {
      return AF_INET;
    }
    case ProtocolType::kIPv6: {
      return AF_INET6;
    }
  }

  return 0;
}

//------------------------------------------------------------------------------
// Get ai_socktype to pass to ::socket
//------------------------------------------------------------------------------
int ServiceEndpoint::getAiSocktype() const {
  switch(socketType) {
    case SocketType::kStream: {
      return SOCK_STREAM;
    }
    case SocketType::kDatagram: {
      return SOCK_DGRAM;
    }
  }

  return 0;
}

//------------------------------------------------------------------------------
// Get ai_protocol to pass to ::socket
//------------------------------------------------------------------------------
int ServiceEndpoint::getAiProtocol() const {
  switch(socketType) {
    case SocketType::kStream: {
      return IPPROTO_TCP;
    }
    case SocketType::kDatagram: {
      return IPPROTO_UDP;
    }
  }

  return 0;
}

//------------------------------------------------------------------------------
// Recover original hostname, the one we passed to HostResolver
//------------------------------------------------------------------------------
std::string ServiceEndpoint::getOriginalHostname() const {
  return originalHostname;
}

//------------------------------------------------------------------------------
// Equality operator
//------------------------------------------------------------------------------
bool ServiceEndpoint::operator==(const ServiceEndpoint& other) const {
  return protocolType     == other.protocolType &&
         socketType       == other.socketType   &&
         address          == other.address      &&
         originalHostname == other.originalHostname;
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
HostResolver::HostResolver(Logger *log) : logger(log) { }

//------------------------------------------------------------------------------
// Resolve, while taking into account intercepts as well
//------------------------------------------------------------------------------
std::vector<ServiceEndpoint> HostResolver::resolve(const std::string &host, int port, Status &st) {
  Endpoint translated = GlobalInterceptor::translate(Endpoint(host, port));
  return resolveNoIntercept(translated.getHost(), translated.getPort(), st);
}

//------------------------------------------------------------------------------
// Main resolve function: How many service endpoints match the given
// hostname and port pair?
//------------------------------------------------------------------------------
std::vector<ServiceEndpoint> HostResolver::resolveNoIntercept(const std::string &host, int port, Status &st) {
  if(!fakeMap.empty()) {
    return resolveFake(host, port, st);
  }

  std::vector<ServiceEndpoint> output;

  struct addrinfo hints, *servinfo, *p;

  int rv;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  if ((rv = getaddrinfo(host.c_str(), std::to_string(port).c_str(),
                        &hints, &servinfo)) != 0) {
    st = Status(rv, SSTR("error when resolving '" << host << "': " << gai_strerror(rv)));
    return output;
  }

  //----------------------------------------------------------------------------
  // getaddrinfo was successful: Loop through all results, build list of
  // service endpoints
  //----------------------------------------------------------------------------
  for (p = servinfo; p != NULL; p = p->ai_next) {
    std::vector<char> addr(p->ai_addrlen);
    memcpy(addr.data(), p->ai_addr, addr.size());

    ProtocolType protocolType = ProtocolType::kIPv4;

    if(p->ai_family == AF_INET) {
      protocolType = ProtocolType::kIPv4;
    }
    else if(p->ai_family == AF_INET6) {
      protocolType = ProtocolType::kIPv6;
    }
    else {
      QCLIENT_LOG(logger, LogLevel::kWarn, "Encountered unknown network family during resolution of " << host << ":" << port << " - neither IPv4, nor IPv6!");
      continue;
    }

    SocketType socketType = SocketType::kStream;

    if(p->ai_socktype == SOCK_STREAM) {
      socketType = SocketType::kStream;
    }
    else if(p->ai_socktype == SOCK_DGRAM) {
      socketType = SocketType::kDatagram;
    }
    else {
      QCLIENT_LOG(logger, LogLevel::kWarn, "Encountered unknown socket type during resolution of " << host << ":" << port << " - neither stream, nor datagram!");
      continue;
    }

    output.emplace_back(protocolType, socketType, addr, host);
  }

  freeaddrinfo(servinfo);
  st = Status();
  return output;
}

//----------------------------------------------------------------------------
// Feed fake data - once you call this, _all_ responses will be faked
//----------------------------------------------------------------------------
void HostResolver::feedFake(const std::string &host, int port, const std::vector<ServiceEndpoint> &out) {
  std::lock_guard<std::mutex> lock(mtx);
  fakeMap[std::pair<std::string, int>(host, port)] = out;
}

//------------------------------------------------------------------------------
// Resolve function that only returns fake data
//------------------------------------------------------------------------------
std::vector<ServiceEndpoint> HostResolver::resolveFake(const std::string &host, int port,
    Status &st) {

  std::lock_guard<std::mutex> lock(mtx);
  auto it = fakeMap.find(std::pair<std::string, int>(host, port));
  if(it != fakeMap.end()) {
    st = Status();
    return it->second;
  }

  st = Status(ENOENT, "Unable to resolve");
  return {};
}



}

