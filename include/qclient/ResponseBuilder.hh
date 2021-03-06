//------------------------------------------------------------------------------
// File: ResponseBuilder.hh
// Author: Georgios Bitzes - CERN
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

#ifndef QCLIENT_RESPONSE_BUILDER_HH
#define QCLIENT_RESPONSE_BUILDER_HH

#include "qclient/Reply.hh"
#include <vector>

struct redisReader;

namespace qclient {

class ResponseBuilder {
public:
  enum class Status {
    kIncomplete,
    kProtocolError,
    kOk
  };

  ResponseBuilder();

  void feed(const char* buff, size_t len);
  void feed(const std::string &str);

  Status pull(redisReplyPtr &reply);
  void restart();

  // Convenience functions for use in tests. Very inefficient!
  static redisReplyPtr makeInt(int val);
  static redisReplyPtr makeErr(const std::string &msg);
  static redisReplyPtr makeStr(const std::string &msg);
  static redisReplyPtr makeStringArray(const std::vector<std::string> &msg, char prefix='*');
  static redisReplyPtr makePushArray(const std::vector<std::string> &msg);
  static redisReplyPtr makeArr(const std::string &str1, const std::string &str2, int num);
  static redisReplyPtr makePushArr(const std::string &str1, const std::string &str2, const std::string &str3, int num);
  static redisReplyPtr makeStatus(const std::string &msg);

  // Convenience function to quickly parse a redis-encoded string into redisReplyPtr
  static redisReplyPtr parseRedisEncodedString(const std::string &str);

  // Convenience function to quickly parse a redis-encoded string into redis-cli-like
  // description
  static std::string parseAndDescribeRedisEncodedString(const std::string &str);

private:
  struct Deleter {
    void operator()(redisReader *reader);
  };

  std::unique_ptr<redisReader, Deleter> reader;
};

}

#endif
