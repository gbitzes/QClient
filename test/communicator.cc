// ----------------------------------------------------------------------
// File: communicator.cc
// Author: Georgios Bitzes - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * qclient - A simple redis C++ client with support for redirects       *
 * Copyright (C) 2020 CERN/Switzerland                                  *
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

#include "qclient/shared/PendingRequestVault.hh"
#include "qclient/shared/Communicator.hh"
#include "qclient/shared/CommunicatorListener.hh"
#include "qclient/pubsub/Subscriber.hh"
#include "qclient/pubsub/Message.hh"
#include "qclient/utils/SteadyClock.hh"
#include "shared/SharedSerialization.hh"
#include "qclient/SSTR.hh"
#include <gtest/gtest.h>

using namespace qclient;

TEST(CommunicatorRequest, Serialization) {
  std::string sourceUuid = "qwerty";
  std::string contents = "uiop";

  std::string payload = serializeCommunicatorRequest(sourceUuid, contents);

  std::string parsedUuid;
  std::string parsedContents;

  ASSERT_TRUE(parseCommunicatorRequest(payload, parsedUuid, parsedContents));
}

TEST(Communicator, IssueWithReply) {
  Subscriber subscriber;
  Communicator communicator(&subscriber, "abc");

  std::string reqid;
  std::future<CommunicatorReply> fut = communicator.issue("1234", reqid);
  ASSERT_EQ(fut.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
  std::cerr << "Assigned RequestID: " << reqid << std::endl;

  CommunicatorReply reply;
  reply.status = 999;
  reply.contents = "AAAA";

  Message msg = Message::createMessage("abc", serializeCommunicatorReply(reqid, reply));
  subscriber.feedFakeMessage(msg);

  CommunicatorReply rep = fut.get();
  ASSERT_EQ(rep.status, 999);
  ASSERT_EQ(rep.contents, "AAAA");
}

TEST(Communicator, WithRetries) {
  Subscriber subscriber;
  SteadyClock steadyClock(true);
  Communicator communicator(&subscriber, "abc", &steadyClock);

  std::string reqid;
  std::future<CommunicatorReply> fut = communicator.issue("987", reqid);
  ASSERT_EQ(fut.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
  std::cerr << "Assigned RequestID: " << reqid << std::endl;

  std::string retryChannel, retryContents, retryID;
  ASSERT_FALSE(communicator.runNextToRetry(retryChannel, retryContents, retryID));

  steadyClock.advance(std::chrono::seconds(9));
  ASSERT_FALSE(communicator.runNextToRetry(retryChannel, retryContents, retryID));

  steadyClock.advance(std::chrono::seconds(1));
  ASSERT_TRUE(communicator.runNextToRetry(retryChannel, retryContents, retryID));

  ASSERT_EQ(retryChannel, "abc");
  ASSERT_EQ(retryContents, "987");
  ASSERT_EQ(retryID, reqid);

  ASSERT_FALSE(communicator.runNextToRetry(retryChannel, retryContents, retryID));
  steadyClock.advance(std::chrono::seconds(10));
  ASSERT_TRUE(communicator.runNextToRetry(retryChannel, retryContents, retryID));

  // Test expiry
  steadyClock.advance(std::chrono::seconds(40));
  ASSERT_FALSE(communicator.runNextToRetry(retryChannel, retryContents, retryID));
  steadyClock.advance(std::chrono::seconds(9000));
  ASSERT_FALSE(communicator.runNextToRetry(retryChannel, retryContents, retryID));
}

TEST(PendingRequestVault, BasicSanity) {
  PendingRequestVault requestVault;
  ASSERT_EQ(requestVault.size(), 0u);

  std::chrono::steady_clock::time_point tp2;
  ASSERT_FALSE(requestVault.getEarliestRetry(tp2));

  std::chrono::steady_clock::time_point tp;
  tp += std::chrono::seconds(1);

  PendingRequestVault::InsertOutcome outcome = requestVault.insert("ch1", "123", tp);
  std::cerr << "RequestID: " << outcome.id << std::endl;

  ASSERT_EQ(requestVault.size(), 1u);
  ASSERT_EQ(outcome.fut.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
  ASSERT_TRUE(requestVault.getEarliestRetry(tp2));
  ASSERT_EQ(tp, tp2);
  requestVault.blockUntilNonEmpty();

  CommunicatorReply reply;
  reply.status = 123;
  reply.contents = "aaa";

  ASSERT_FALSE(requestVault.satisfy("123", std::move(reply)));
  ASSERT_TRUE(requestVault.satisfy(outcome.id, std::move(reply)));

  ASSERT_EQ(requestVault.size(), 0u);
  ASSERT_FALSE(requestVault.getEarliestRetry(tp2));

  CommunicatorReply rep = outcome.fut.get();
  ASSERT_EQ(rep.status, 123);
  ASSERT_EQ(rep.contents, "aaa");
}

TEST(PendingRequestVault, WithRetries) {
  PendingRequestVault requestVault;
  std::chrono::steady_clock::time_point start;

  requestVault.insert("ch1", "123", start+std::chrono::seconds(1));
  requestVault.insert("ch1", "1234", start+std::chrono::seconds(2));

  ASSERT_EQ(requestVault.size(), 2u);
  std::chrono::steady_clock::time_point tp;
  std::string channel, contents, id;

  ASSERT_TRUE(requestVault.getEarliestRetry(tp));
  ASSERT_EQ(start+std::chrono::seconds(1), tp);
  ASSERT_TRUE(requestVault.retryFrontItem(start+std::chrono::seconds(3),
    channel, contents, id));
  ASSERT_EQ(channel, "ch1");
  ASSERT_EQ(contents, "123");

  ASSERT_TRUE(requestVault.getEarliestRetry(tp));
  ASSERT_EQ(start+std::chrono::seconds(2), tp);
  ASSERT_TRUE(requestVault.retryFrontItem(start+std::chrono::seconds(4),
    channel, contents, id));
  ASSERT_EQ(channel, "ch1");
  ASSERT_EQ(contents, "1234");

  ASSERT_TRUE(requestVault.getEarliestRetry(tp));
  ASSERT_EQ(start+std::chrono::seconds(3), tp);
  ASSERT_TRUE(requestVault.retryFrontItem(start+std::chrono::seconds(5),
    channel, contents, id));
  ASSERT_EQ(channel, "ch1");
  ASSERT_EQ(contents, "123");

  ASSERT_TRUE(requestVault.getEarliestRetry(tp));
  ASSERT_EQ(start+std::chrono::seconds(4), tp);
  ASSERT_TRUE(requestVault.retryFrontItem(start+std::chrono::seconds(6),
    channel, contents, id));
  ASSERT_EQ(channel, "ch1");
  ASSERT_EQ(contents, "1234");


  ASSERT_EQ(requestVault.expire(start), 0u);
  ASSERT_EQ(requestVault.expire(start+std::chrono::seconds(1)), 1u);
  ASSERT_EQ(requestVault.size(), 1u);
  ASSERT_EQ(requestVault.expire(start+std::chrono::seconds(1)), 0u);
  ASSERT_EQ(requestVault.expire(start+std::chrono::seconds(2)), 1u);
  ASSERT_EQ(requestVault.size(), 0u);
}

TEST(PendingRequestVault, SingleItemRetry) {
  PendingRequestVault requestVault;
  std::chrono::steady_clock::time_point start;

  requestVault.insert("ch1", "123", start+std::chrono::seconds(1));

  std::string channel, contents, id;
  ASSERT_TRUE(requestVault.retryFrontItem(start+std::chrono::seconds(3),
     channel, contents, id));
  ASSERT_EQ(channel, "ch1");
  ASSERT_EQ(contents, "123");

  CommunicatorReply reply;
  reply.status = 123;
  reply.contents = "aaa";

  ASSERT_TRUE(requestVault.satisfy(id, std::move(reply)));
}

TEST(CommunicatorListener, BasicSanity) {
  Subscriber subscriber;
  CommunicatorListener communicator(&subscriber, "abc");

  ASSERT_EQ(communicator.size(), 0u);
  Message msg = Message::createMessage("abc", serializeCommunicatorRequest("1-2-3-4", "qqq"));
  subscriber.feedFakeMessage(msg);
  ASSERT_EQ(communicator.size(), 1u);

  CommunicatorRequest req = communicator.front();
  communicator.pop_front();

  ASSERT_EQ(req.getID(), "1-2-3-4");
  ASSERT_EQ(req.getContents(), "qqq");

  subscriber.feedFakeMessage(msg);
  ASSERT_EQ(communicator.size(), 0u);
}
