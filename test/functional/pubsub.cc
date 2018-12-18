//------------------------------------------------------------------------------
// File: pubsub.cc
// Author: Georgios Bitzes <georgios.bitzes@cern.ch>
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

#include "test-config.hh"
#include "qclient/BaseSubscriber.hh"
#include "qclient/pubsub/MessageQueue.hh"
#include <gtest/gtest.h>

using namespace qclient;

TEST(BaseSubscriber, BasicSanity) {
  std::shared_ptr<MessageQueue> listener { new MessageQueue() };

  Members members(testconfig.host, testconfig.port);
  BaseSubscriber subscriber(members, listener, {} );
  subscriber.subscribe( {"pickles"} );

  std::this_thread::sleep_for(std::chrono::seconds(1)); // TODO: fix
  ASSERT_EQ(listener->size(), 1u);

  Message* item = nullptr;
  auto it = listener->begin();

  item = it.getItemBlockOrNull();
  ASSERT_NE(item, nullptr);

  ASSERT_EQ(item->getMessageType(), MessageType::kSubscribe);
  ASSERT_EQ(item->getChannel(), "pickles");
  ASSERT_EQ(item->getActiveSubscriptions(), 1);

  it.next();
  listener->pop_front();

  // TODO: uncomment the code below
  // ASSERT_EQ(listener->size(), 0u);
  // subscriber.subscribe( {"test-2"} );
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  // ASSERT_EQ(listener->size(), 1u);

  // item = it.getItemBlockOrNull();
  // ASSERT_NE(item, nullptr);

  // ASSERT_EQ(item->getMessageType(), MessageType::kPatternSubscribe);
  // ASSERT_EQ(item->getPattern(), "test-*");
  // ASSERT_EQ(item->getActiveSubscriptions(), 2);

  // it.next();
  // listener->pop_front();
}