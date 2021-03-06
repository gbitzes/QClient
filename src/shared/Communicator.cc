//------------------------------------------------------------------------------
// File: Communicator.cc
// Author: Georgios Bitzes - CERN
//------------------------------------------------------------------------------

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

#include "qclient/shared/Communicator.hh"
#include "qclient/pubsub/Subscriber.hh"
#include "qclient/pubsub/Message.hh"
#include "SharedSerialization.hh"
#include "qclient/SSTR.hh"
#include "qclient/utils/Macros.hh"
#include "qclient/utils/SteadyClock.hh"
#include "qclient/Debug.hh"

namespace qclient {

//------------------------------------------------------------------------------
// Convenience class for point-to-point request / response messaging
//------------------------------------------------------------------------------
Communicator::Communicator(Subscriber* subscriber, const std::string &channel,
  SteadyClock* clock, std::chrono::milliseconds retryInterval,
  std::chrono::seconds deadline)
: mSubscriber(subscriber), mChannel(channel), mClock(clock), mQcl(mSubscriber->getQcl()),
  mRetryInterval(retryInterval), mHardDeadline(deadline) {

  mSubscription = mSubscriber->subscribe(mChannel);

  using namespace std::placeholders;
  mSubscription->attachCallback(std::bind(&Communicator::processIncoming, this, _1));

  if(!mClock || !mClock->isFake()) {
    mThread.reset(&Communicator::backgroundThread, this);
  }
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Communicator::~Communicator() {
  mPendingVault.setBlockingMode(false);
}

//------------------------------------------------------------------------------
// Cleanup and retry thread
//------------------------------------------------------------------------------
void Communicator::backgroundThread(ThreadAssistant &assistant) {
  while(!assistant.terminationRequested()) {
    std::chrono::steady_clock::time_point earliestRetry;
    if(!mPendingVault.getEarliestRetry(earliestRetry)) {
      // Pending vault empty, sleep
      mPendingVault.blockUntilNonEmpty();
      continue;
    }

    std::chrono::steady_clock::time_point now = SteadyClock::now(mClock);
    if(earliestRetry+mRetryInterval > now) {
      // Not there yet, need to wait a bit more
      std::chrono::milliseconds nextRetryIn = std::chrono::duration_cast<std::chrono::milliseconds>(now - (earliestRetry+mRetryInterval));
      assistant.wait_for(nextRetryIn);
      continue;
    }

    std::string channel, contents, id;
    if(runNextToRetry(channel, contents, id) && mQcl) {
      // Go
      mQcl->exec("PUBLISH", channel, serializeCommunicatorRequest(id, contents));
    }
  }
}

//------------------------------------------------------------------------------
// Issue a request on the given channel
//------------------------------------------------------------------------------
std::future<CommunicatorReply> Communicator::issue(const std::string &contents) {
  std::string unused;
  return issue(contents, unused);
}

//------------------------------------------------------------------------------
// Issue a request on the given channel, retrieve ID too
//------------------------------------------------------------------------------
std::future<CommunicatorReply> Communicator::issue(const std::string &contents, std::string &id) {
  PendingRequestVault::InsertOutcome outcome = mPendingVault.insert(mChannel,
    contents, SteadyClock::now(mClock));

  id = outcome.id;

  if(mQcl) {
    mQcl->exec("PUBLISH", mChannel, serializeCommunicatorRequest(outcome.id, contents));
  }

  return std::move(outcome.fut);
}

//------------------------------------------------------------------------------
// Run next-to-retry pass
//
// Return value:
// - False: Nothing to retry
// - True: We have something to retry
//------------------------------------------------------------------------------
bool Communicator::runNextToRetry(std::string &channel, std::string &contents, std::string &id) {
  mPendingVault.expire(SteadyClock::now(mClock) - mHardDeadline);

  std::chrono::steady_clock::time_point earliestRetry;
  if(!mPendingVault.getEarliestRetry(earliestRetry)) {
    // Empty, nothing to retry
    return false;
  }

  // Are we at least mRetryInterval ahead of last retry?
  if(earliestRetry+mRetryInterval > SteadyClock::now(mClock)) {
    return false;
  }

  // Let's do it
  return mPendingVault.retryFrontItem(SteadyClock::now(mClock), channel, contents, id);
}

//------------------------------------------------------------------------------
// Process incoming message
//------------------------------------------------------------------------------
void Communicator::processIncoming(Message &&msg) {
  if(msg.getMessageType() != MessageType::kMessage) return;
  if(msg.getChannel() != mChannel) return;

  std::string uuid;
  CommunicatorReply reply;
  if(parseCommunicatorReply(msg.getPayload(), reply, uuid)) {
    mPendingVault.satisfy(uuid, std::move(reply));
  }
}

}
