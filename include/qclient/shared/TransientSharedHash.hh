//------------------------------------------------------------------------------
// File: TransientSharedHash.hh
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

#ifndef QCLIENT_TRANSIENT_SHARED_HASH_HH
#define QCLIENT_TRANSIENT_SHARED_HASH_HH

#include "qclient/utils/Macros.hh"
#include <map>
#include <shared_mutex>

namespace qclient {

//------------------------------------------------------------------------------
//! A "shared hash" class with no persistency whatsoever, and a "meh" approach
//! to consistency.
//!
//! Use for high-volume, low-value information, such as statistics
//! or heartbeats.
//!
//! Uses a simple pub-sub channel for communication.
//------------------------------------------------------------------------------
class SharedManager; class Logger;
class Subscription; class Message;

class TransientSharedHash {
public:
  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  ~TransientSharedHash();

  //----------------------------------------------------------------------------
  //! Set key to the given value.
  //----------------------------------------------------------------------------
  void set(const std::string &key, const std::string &value);

  //----------------------------------------------------------------------------
  //! Set a batch of key-value pairs.
  //----------------------------------------------------------------------------
  void set(const std::map<std::string, std::string> &batch);

  //----------------------------------------------------------------------------
  //! Get key, if it exists
  //----------------------------------------------------------------------------
  bool get(const std::string &key, std::string &value) const;

private:
  friend class SharedManager;

  //----------------------------------------------------------------------------
  //! Private constructor - use SharedManager to instantiate this object.
  //----------------------------------------------------------------------------
  TransientSharedHash(SharedManager *sharedManager, const std::string &channel,
    std::unique_ptr<qclient::Subscription> sub);

  //----------------------------------------------------------------------------
  //! Process incoming message
  //----------------------------------------------------------------------------
  void processIncoming(Message &&msg);

  //----------------------------------------------------------------------------
  //! Member variables
  //----------------------------------------------------------------------------
  SharedManager *sharedManager;
  std::shared_ptr<Logger> logger;

  std::string channel;
  std::unique_ptr<qclient::Subscription> subscription;

  mutable std::mutex contentsMtx;
  std::map<std::string, std::string> contents;

};

}

#endif

