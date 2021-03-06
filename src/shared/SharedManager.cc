//------------------------------------------------------------------------------
// File: SharedManager.cc
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

#include "qclient/shared/SharedManager.hh"
#include "qclient/QClient.hh"
#include "qclient/pubsub/Subscriber.hh"
#include "qclient/pubsub/Message.hh"
#include "qclient/shared/TransientSharedHash.hh"

namespace qclient {

//------------------------------------------------------------------------------
// Constructor - supply necessary information for connecting to a QDB
// instance.
//
// We will always use RESP3 push types, which means this class is currently
// only compatible with QuarkDB, not official redis!
//------------------------------------------------------------------------------
SharedManager::SharedManager(const qclient::Members &members,
  qclient::SubscriptionOptions &&subscriptionOptions) {

  subscriptionOptions.usePushTypes = true;
  logger = subscriptionOptions.logger;
  subscriber.reset(new Subscriber(members, std::move(subscriptionOptions)));
  qcl = subscriber->getQcl();
}

//------------------------------------------------------------------------------
// Empty constructor, simulation mode.
//------------------------------------------------------------------------------
SharedManager::SharedManager() {
  subscriber.reset(new Subscriber());
}

//------------------------------------------------------------------------------
// Publish the given message. You probably should not call this directly,
// it's used by our dependent shared data structures to publish
// modifications.
//------------------------------------------------------------------------------
void SharedManager::publish(const std::string &channel, const std::string &payload) {
  if(qcl) {
    //--------------------------------------------------------------------------
    // Real mode
    //--------------------------------------------------------------------------
    qcl->exec("PUBLISH", channel, payload);
  }
  else {
    //--------------------------------------------------------------------------
    // Simulation
    //--------------------------------------------------------------------------
    subscriber->feedFakeMessage(Message::createMessage(channel, payload));
  }
}

//------------------------------------------------------------------------------
// Make a transient shared hash based on the given channel
//------------------------------------------------------------------------------
std::unique_ptr<TransientSharedHash> SharedManager::makeTransientSharedHash(const std::string &channel,
  const std::shared_ptr<SharedHashSubscriber> &hashSub) {
  return std::unique_ptr<TransientSharedHash>(new TransientSharedHash(this, channel, subscriber->subscribe(channel), hashSub));
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
SharedManager::~SharedManager() {}

//------------------------------------------------------------------------------
// Get pointer to underlying QClient object - lifetime is tied to this
// SharedManager.
//------------------------------------------------------------------------------
qclient::QClient* SharedManager::getQClient() {
  return qcl;
}

//------------------------------------------------------------------------------
// Get pointer to underlying Subscriber object - lifetime is tied to this
// SharedManager.
//------------------------------------------------------------------------------
qclient::Subscriber* SharedManager::getSubscriber() {
  return subscriber.get();
}

//------------------------------------------------------------------------------
// Get logger object
//------------------------------------------------------------------------------
std::shared_ptr<Logger> SharedManager::getLogger() {
  return logger;
}

}

