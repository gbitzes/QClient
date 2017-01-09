//------------------------------------------------------------------------------
// File: hash.cc
// Author: Elvin-Alin Sindrilaru <esindril@cern.ch>
//------------------------------------------------------------------------------

/************************************************************************
 * quarkdb - a redis-like highly available key-value store              *
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

#include <gtest/gtest.h>
#include "qclient/QHash.hh"
#include <algorithm>

using namespace qclient;
static std::string sHost = "localhost";
static int sPort = 7777;

//------------------------------------------------------------------------------
// Test HASH interface - synchronous
//------------------------------------------------------------------------------
TEST(QHash, HashSync) {
  QClient cl{sHost, sPort};
  std::string hash_key = "redox_test:hash";
  QHash qhash{cl,hash_key};
  std::vector<std::string> fields {"val1", "val2", "val3"};
  std::vector<int> ivalues {10, 20, 30};
  std::vector<float> fvalues {100.0, 200.0, 300.0};
  std::vector<std::string> svalues {"1000", "2000", "3000"};
  ASSERT_EQ(0, qhash.hlen());
  ASSERT_TRUE(qhash.hset(fields[0], fvalues[0]));
  ASSERT_FLOAT_EQ(fvalues[0], std::stof(qhash.hget(fields[0])));
  ASSERT_FLOAT_EQ(100.0005, qhash.hincrbyfloat(fields[0], 0.0005));
  ASSERT_TRUE(qhash.hexists(fields[0]));
  ASSERT_TRUE(qhash.hdel(fields[0]));

  ASSERT_FALSE(qhash.hexists(fields[1]));
  ASSERT_TRUE(qhash.hsetnx(fields[1], svalues[1]));
  ASSERT_TRUE(qhash.hsetnx(fields[1], svalues[1]));
  ASSERT_EQ(svalues[1], qhash.hget(fields[1]));
  ASSERT_TRUE(qhash.hdel(fields[1]));

  ASSERT_TRUE(qhash.hset(fields[2], ivalues[2]));
  ASSERT_TRUE(qhash.hset(fields[1], ivalues[1]));
  ASSERT_EQ(35, qhash.hincrby(fields[2], 5));
  ASSERT_TRUE(qhash.hdel(fields[2]));
  ASSERT_TRUE(qhash.hsetnx(fields[2], ivalues[2]));
  ASSERT_TRUE(qhash.hsetnx(fields[0], ivalues[0]));
  ASSERT_EQ(3, qhash.hlen());

  // Test the hkeys command
  std::vector<std::string> resp = qhash.hkeys();

  for (auto&& elem: resp) {
    ASSERT_TRUE(std::find(fields.begin(), fields.end(), elem) != fields.end());
  }

  // Test the hvals command
  resp = qhash.hvals();

  for (auto&& elem: resp) {
    ASSERT_TRUE(std::find(ivalues.begin(), ivalues.end(), std::stoi(elem)) != ivalues.end());
  }

  // Test the hgetall command
  resp = qhash.hgetall();

  for (auto it = resp.begin(); it != resp.end(); ++it) {
    ASSERT_TRUE(std::find(fields.begin(), fields.end(), *it) != fields.end());
    ++it;
    ASSERT_TRUE(std::find(ivalues.begin(), ivalues.end(), std::stoi(*it)) != ivalues.end());
  }

  ASSERT_TRUE(qhash.hget("dummy_field").empty());
  std::future<redisReplyPtr> future = cl.execute({"DEL", hash_key});
  ASSERT_EQ(1, future.get()->integer);

  // Test hscan command
  std::unordered_map<int, int> map;
  std::unordered_map<int, int> ret_map;

  for (int i = 0; i < 3000; ++i) {
    map.emplace(i, i);
    ASSERT_EQ(1, qhash.hset(std::to_string(i), i));
  }

  std::string cursor = "0";
  long long count = 1000;
  std::pair<std::string, std::unordered_map<std::string, std::string> > reply;
  reply = qhash.hscan(cursor, count);
  cursor = reply.first;

  for (auto&& elem: reply.second) {
    ASSERT_TRUE(map[std::stoi(elem.first)] == std::stoi(elem.second));
    ret_map.emplace(std::stoi(elem.first), std::stoi(elem.second));
  }

  while (cursor != "0") {
    reply = qhash.hscan(cursor, count);
    cursor = reply.first;

    for (auto&& elem: reply.second) {
      ASSERT_TRUE(map[std::stoi(elem.first)] == std::stoi(elem.second));
      ret_map.emplace(std::stoi(elem.first), std::stoi(elem.second));
    }
  }

  ASSERT_TRUE(map.size() == ret_map.size());
  auto fut = cl.execute({"DEL", hash_key});
  ASSERT_EQ(1, fut.get()->integer);
}

/*
//------------------------------------------------------------------------------
// Test HASH interface - asynchronous
//------------------------------------------------------------------------------
TEST_F(QHash, HashAsync) {
  connect();
  std::string hash_key = "redox_test:hash_async";
  QHash qhash(rdx,hash_key);
  ASSERT_EQ(0, qhash.hlen());
  std::string field, value;
  std::list<std::string> lst_errors;
  std::atomic<std::uint64_t> num_async_req {0};
  std::condition_variable wait_cv;
  std::mutex mutex;
  std::uint64_t num_elem = 100;
  auto callback_set = [&](Command<int>& c) {
    if (!c.ok()) {
      if (c.cmd_.size() >= 3) {
	std::string cmd_field = c.cmd_[2];
	lst_errors.emplace(lst_errors.end(), cmd_field);
      }
    }

    if (!--num_async_req) {
      wait_cv.notify_one();
    }

    c.free();
  };

  // Push asynchronously num_elem
  for (std::uint64_t i = 0; i < num_elem; ++i) {
    num_async_req++;
    field = "field" + std::to_string(i);
    value = std::to_string(i);
    qhash.hset(field, value, callback_set);
  }

  {
    // Wait for all the async requests
    std::unique_lock<std::mutex> lock(mutex);
    while (num_async_req)
      wait_cv.wait(lock);
  }

  ASSERT_EQ(0, lst_errors.size());

  // Get map length asynchronously
  long long int length = 0;
  auto callback_len = [&](Command<long long int>& c) {
    if (!c.ok()) {
      length = -1;
    }
    else {
      length = c.reply();
    }

    c.free();
    wait_cv.notify_one();
  };

  qhash.hlen(callback_len);

  {
    // Wait for length async response
    std::unique_lock<std::mutex> lock(mutex);
    wait_cv.wait(lock);
  }

  ASSERT_EQ(num_elem, length);

  // Delete asynchronously all elements
  auto callback_del = [&](Command<int>& c) {
    if (!c.ok()) {
      if (c.cmd_.size() >= 3) {
	std::string cmd_field = c.cmd_[2];
	lst_errors.emplace(lst_errors.end(), cmd_field);
      }
    }

    if (!--num_async_req) {
      wait_cv.notify_one();
    }
  };

  for (std::uint64_t i = 0; i <= num_elem; ++i) {
    num_async_req++;
    field = "field" + std::to_string(i);
    qhash.hdel(field, callback_del);
  }

  {
    // Wait for all the async requests
    std::unique_lock<std::mutex> lock(mutex);
    while (num_async_req)
      wait_cv.wait(lock);
  }

  ASSERT_EQ(0, lst_errors.size());
  ASSERT_EQ(0, qhash.hlen());
  rdx.disconnect();
}

//------------------------------------------------------------------------------
// Test Set class - synchronous
//------------------------------------------------------------------------------
TEST_F(QSet, SetSync) {
  connect();
  std::string set_key = "redox_test:set_sync";
  RedoxSet rdx_set(rdx,set_key);
  std::vector<std::string> members = {"200", "300", "400"};

  ASSERT_EQ(members.size(), rdx_set.sadd(members));
  ASSERT_TRUE(rdx_set.sadd("100"));
  ASSERT_FALSE(rdx_set.sadd("400"));
  (void) members.push_back("100");
  ASSERT_TRUE(rdx_set.sismember("300"));
  ASSERT_FALSE(rdx_set.sismember("1500"));
  ASSERT_EQ(4, rdx_set.scard());
  set<std::string> ret_members = rdx_set.smembers();

  for (const auto& elem: members)
    ASSERT_TRUE(ret_members.find(elem) != ret_members.end());

  ASSERT_TRUE(rdx_set.srem("100"));
  ASSERT_EQ(3, rdx_set.srem( members));
  ASSERT_FALSE(rdx_set.srem("100"));
  ASSERT_EQ(0, rdx_set.scard());

  // Test SSCAN functionality
  members.clear();

  for (int i = 0; i < 3000; ++i) {
    members.push_back(std::to_string(i));
    ASSERT_TRUE(rdx_set.sadd(std::to_string(i)));
  }

  long long cursor = 0;
  long long count = 1000;
  std::pair< long long, std::vector<std::string> > reply;

  do {
    reply = rdx_set.sscan(cursor, count);
    cursor = reply.first;

    for (auto&& elem: reply.second) {
      ASSERT_TRUE(std::find(members.begin(), members.end(), elem) !=
		  members.end());
    }
  }
  while (cursor);

  print_and_check_sync(rdx.commandSync<int>({"DEL", set_key}), 1);
  rdx.disconnect();
}

//------------------------------------------------------------------------------
// Test Set class - asynchronous
//------------------------------------------------------------------------------
TEST_F(QSet, SetAsync) {
  connect();
  std::string set_key = "redox_test:set_async";
  RedoxSet rdx_set(rdx,set_key);

  std::vector<std::string> members = {"200", "300", "400"};
  std::list<std::string> lst_errors;
  std::atomic<std::uint64_t> num_asyn_req {0};
  std::mutex mutex;
  std::condition_variable cond_var;
  auto callback = [&](Command<int>& c) {
    if ((c.ok() && c.reply() != 1) || !c.ok()) {
      std::ostringstream oss;
      oss << "Failed command: " << c.cmd() << " error: " << c.lastError();
      lst_errors.emplace(lst_errors.end(), oss.str());
    }

    if (--num_asyn_req == 0) {
      cond_var.notify_one();
    }
  };

  std::string value;
  // Add some elements
  for (auto i = 0; i < 100; ++i) {
    value = "val" + std::to_string(i);
    num_asyn_req++;
    rdx_set.sadd(value, callback);
  }

  // Wait for all the replies
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (num_asyn_req) {
      cond_var.wait(lock);
    }
  }

  ASSERT_EQ(0, lst_errors.size());
  // Add some more elements that will trigger some errors
  for (auto i = 90; i < 110; ++i) {
    value = "val" + std::to_string(i);
    num_asyn_req++;
    rdx_set.sadd(value, callback);
  }

  // Wait for all the replies
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (num_asyn_req) {
      cond_var.wait(lock);
    }
  }

  ASSERT_EQ(10, lst_errors.size());
  ASSERT_EQ(110, rdx_set.scard());
  lst_errors.clear();

  // Remove all elements
  for (auto i = 0; i < 110; ++i)
  {
    num_asyn_req++;
    value = "val" + std::to_string(i);
    rdx_set.srem(value, callback);
  }

  // Wait for all the replies
  {
    std::unique_lock<std::mutex> lock(mutex);
    while (num_asyn_req) {
      cond_var.wait(lock);
    }
  }

  ASSERT_EQ(0, lst_errors.size());
  print_and_check_sync(rdx.commandSync<int>({"DEL", set_key}), 0);
  rdx.disconnect();
}
*/
