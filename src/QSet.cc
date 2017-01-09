//------------------------------------------------------------------------------
//! @file QSet.cc
//! @author Elvin-Alin Sindrilaru <esindril@cern.ch>
//------------------------------------------------------------------------------

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

#include "qclient/QSet.hh"

using namespace std;

QCLIENT_NAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Redis SET add command for multiple members - synchronous
//------------------------------------------------------------------------------
long long int QSet::sadd(std::vector<std::string> vect_members)
{
  (void) vect_members.insert(vect_members.begin(), mKey);
  (void) vect_members.insert(vect_members.begin(), "SADD");
  auto future = mClient->execute(vect_members);
  redisReplyPtr reply = future.get();

  if (!reply) {
    throw std::runtime_error("[FATAL] Error sadd key: " + mKey +
			     " with multiple members: No connection");
  }

  if (reply->type != REDIS_REPLY_INTEGER) {
    throw std::runtime_error("[FATAL] Error sadd key: " + mKey +
			     " with multiple members: Unexpected reply type: " +
			     std::to_string(reply->type));
  }

  return reply->integer;
}

//------------------------------------------------------------------------------
// Redis SET remove command for multiple members - synchronous
//------------------------------------------------------------------------------
long long int QSet::srem(std::vector<std::string> vect_members)
{
  (void) vect_members.insert(vect_members.begin(), mKey);
  (void) vect_members.insert(vect_members.begin(), "SREM");
  auto future  = mClient->execute(vect_members);
  redisReplyPtr reply = future.get();

  if (!reply) {
    throw std::runtime_error("[FATAL] Error srem key: " + mKey +
			     " with multiple members: No connection");
  }

  if (reply->type != REDIS_REPLY_INTEGER) {
    throw std::runtime_error("[FATAL] Error srem key: " + mKey +
			     " with multiple members: Unexpected reply type: " +
			     std::to_string(reply->type));
  }

  return reply->integer;
}

//------------------------------------------------------------------------------
// Redis SET size command - synchronous
//------------------------------------------------------------------------------
long long int QSet::scard()
{
  auto future = mClient->execute({"SCARD", mKey});
  redisReplyPtr reply = future.get();

  if (!reply) {
    throw std::runtime_error("[FATAL] Error scard key: " + mKey +
			     " : No connection");
  }

  if (reply->type != REDIS_REPLY_INTEGER) {
    throw std::runtime_error("[FATAL] Error scard key: " + mKey +
			     " : Unexpected reply type: " +
			     std::to_string(reply->type));
  }

  return reply->integer;
}

//------------------------------------------------------------------------------
// Redis SET smembers command - synchronous
//------------------------------------------------------------------------------
std::set<std::string> QSet::smembers()
{
  auto future = mClient->execute({"SMEMBERS", mKey});
  redisReplyPtr reply = future.get();

  if (!reply) {
    throw std::runtime_error("[FATAL] Error smembers key: " + mKey +
			     " : No connection");
  }

  if (reply->type != REDIS_REPLY_ARRAY) {
    throw std::runtime_error("[FATAL] Error smembers key: " + mKey +
			     " : Unexpected reply type: " +
			     std::to_string(reply->type));
  }

  std::set<std::string> ret;

  for (size_t i = 0; i < reply->elements; ++i) {
    ret.emplace(reply->element[i]->str, reply->element[i]->len);
  }

  return ret;
}

//------------------------------------------------------------------------------
// Redis SET SCAN command - synchronous
//------------------------------------------------------------------------------
std::pair< long long, std::vector<std::string> >
QSet::sscan(long long cursor, long long count)
{
  auto future = mClient->execute({"SSCAN", mKey, std::to_string(cursor),
				  "COUNT", std::to_string(count)
				 });
  redisReplyPtr reply = future.get();

  if (!reply) {
    throw std::runtime_error("[FATAL] Error sscan key: " + mKey +
			     " : No connection");
  }

  // Parse the Redis reply
  long long new_cursor = std::stoll({reply->element[0]->str,
				     static_cast<unsigned int>(reply->element[0]->len)
				    });
  // First element is the new cursor
  std::pair<long long, std::vector<std::string> > retc_pair;
  retc_pair.first = new_cursor;
  // Get arrary part of the response
  redisReply* reply_ptr =  reply->element[1];

  for (unsigned long i = 0; i < reply_ptr->elements; ++i) {
    retc_pair.second.emplace_back(reply_ptr->element[i]->str,
				  static_cast<unsigned int>(reply_ptr->element[i]->len));
  }

  return retc_pair;
}

QCLIENT_NAMESPACE_END