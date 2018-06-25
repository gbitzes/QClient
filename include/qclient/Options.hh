//------------------------------------------------------------------------------
// File: Options.hh
// Author: Georgios Bitzes - CERN
//------------------------------------------------------------------------------

/************************************************************************
 * qclient - A simple redis C++ client with support for redirects       *
 * Copyright (C) 2018 CERN/Switzerland                                  *
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

#ifndef QCLIENT_OPTIONS_HH
#define QCLIENT_OPTIONS_HH

#include <chrono>
#include <memory>
#include "TlsFilter.hh"

namespace qclient {

class Handshake;

//------------------------------------------------------------------------------
//! This struct specifies how to rate-limit writing into QClient.
//!
//! Since QClient offers an asynchornous API, what happens if we're able to
//! produce requests faster than they can be serviced? The request backlog size
//! will start increasing to infinity, and we'll run out of memory.
//!
//! Specifying a backpressure strategy will prevent that.
//------------------------------------------------------------------------------
class BackpressureStrategy {
public:

  //----------------------------------------------------------------------------
  //! Use this if unsure, should provide a reasonable default value.
  //----------------------------------------------------------------------------
  static BackpressureStrategy Default() {
    return RateLimitPendingRequests();
  }

  //----------------------------------------------------------------------------
  //! Limit pending requests to the specified amount. Once this limit is
  //! reached, attempts to issue more requests will block.
  //----------------------------------------------------------------------------
  static BackpressureStrategy RateLimitPendingRequests(size_t sz = 262144u) {
    BackpressureStrategy ret;
    ret.enabled = true;
    ret.pendingRequestLimit = sz;
    return ret;
  }

  //----------------------------------------------------------------------------
  //! Use this only if you have a good reason to, Default() should work fine
  //! for the vast majority of use cases.
  //----------------------------------------------------------------------------
  static BackpressureStrategy InfinitePendingRequests() {
    BackpressureStrategy ret;
    ret.enabled = false;
    return ret;
  }

  bool active() const {
    return enabled;
  }

  size_t getRequestLimit() const {
    return pendingRequestLimit;
  }

private:
  //----------------------------------------------------------------------------
  //! Private constructor - use static methods above to create an object.
  //----------------------------------------------------------------------------
  BackpressureStrategy() {}

  bool enabled = false;
  size_t pendingRequestLimit = 0u;
};

//------------------------------------------------------------------------------
//! Class RetryStrategy
//------------------------------------------------------------------------------
class RetryStrategy {
private:
  //----------------------------------------------------------------------------
  //! Private constructor, use static methods below to construct an object.
  //----------------------------------------------------------------------------
  RetryStrategy() {}

public:

  enum class Mode {
    kNoRetries = 0,
    kRetryWithTimeout,
    kInfiniteRetries
  };

  //----------------------------------------------------------------------------
  //! No retries.
  //----------------------------------------------------------------------------
  static RetryStrategy NoRetries() {
    RetryStrategy val;
    val.mode = Mode::kNoRetries;
    return val;
  }

  //----------------------------------------------------------------------------
  //! Retry, up until the specified timeout.
  //! NOTE: Timeout is per-connection, not per request.
  //----------------------------------------------------------------------------
  static RetryStrategy WithTimeout(std::chrono::seconds tm) {
    RetryStrategy val;
    val.mode = Mode::kRetryWithTimeout;
    val.timeout = tm;
    return val;
  }

  //----------------------------------------------------------------------------
  //! Infinite number of retries - hang forever if backend is not available.
  //----------------------------------------------------------------------------
  static RetryStrategy InfiniteRetries() {
    RetryStrategy val;
    val.mode = Mode::kInfiniteRetries;
    return val;
  }

  Mode getMode() const {
    return mode;
  }

  std::chrono::seconds getTimeout() const {
    return timeout;
  }

  bool active() const {
    return mode != Mode::kNoRetries;
  }

private:
  Mode mode { Mode::kNoRetries };

  //----------------------------------------------------------------------------
  //! Timeout is per-connection, not per request. Only applies if mode
  //! is kRetryWithTimeout.
  //----------------------------------------------------------------------------
  std::chrono::seconds timeout {0};
};


//------------------------------------------------------------------------------
//! QClient Options class.
//------------------------------------------------------------------------------
class Options {
public:
  //----------------------------------------------------------------------------
  //! If enabled, QClient will try to transparently handle -MOVED redirects.
  //----------------------------------------------------------------------------
  bool transparentRedirects = false;

  //----------------------------------------------------------------------------
  //! Specifies how to handle failing rqeuests. Default is NoRetries, meaning
  //! if the connection drops, some requests receive redisReplyPtr == nullptr,
  //! and it is up to the user to resubmit them.
  //!
  //! An alternative would be RetryStrategy::WithTimeout(60), which will
  //! resubmit the request for up to 60 seconds, before returning
  //! redisReplyPtr == nullptr for any particular request.
  //----------------------------------------------------------------------------
  RetryStrategy retryStrategy = RetryStrategy::NoRetries();

  //----------------------------------------------------------------------------
  //! Specifies whether to rate-limit writing into QClient. If there are
  //! too many un-acknowledged pending requests, attempting to issue more will
  //! block.
  //!
  //! Default is a maximum of 262144 pending requests in-flight.
  //----------------------------------------------------------------------------
  BackpressureStrategy backpressureStrategy = BackpressureStrategy::Default();

  //----------------------------------------------------------------------------
  //! Specifies whether to use TLS - default is off.
  //----------------------------------------------------------------------------
  TlsConfig tlsconfig = {};

  //----------------------------------------------------------------------------
  //! Specifies the handshake to use. A handshake is a sequence of redis
  //! commands sent before any other on a particular connection. If the
  //! connection drops and reconnects, the handshake will run again.
  //!
  //! Ideal for things like AUTH.
  //----------------------------------------------------------------------------
  std::unique_ptr<Handshake> handshake = {};

  //----------------------------------------------------------------------------
  //! Fluent interface: Chain a handshake. Explicit transfer of ownership to
  //! this object.
  //!
  //! If given handshake is nullptr, nothing is done.
  //! If there's no existing handshake, the given handshake is set to be the
  //! top-level one.
  //----------------------------------------------------------------------------
  qclient::Options& chainHandshake(std::unique_ptr<Handshake> handshake);

  //----------------------------------------------------------------------------
  //! Fluent interface: Chain HMAC handshake. If password is empty, any existing
  //! handshake is left untouched.
  //----------------------------------------------------------------------------
  qclient::Options& chainHmacHandshake(const std::string &password);

  //----------------------------------------------------------------------------
  //! Fluent interface: Enable transparent redirects
  //----------------------------------------------------------------------------
  qclient::Options& withTransparentRedirects();

  //----------------------------------------------------------------------------
  //! Fluent interface: Disable transparent redirects
  //----------------------------------------------------------------------------
  qclient::Options& withoutTransparentRedirects();

  //----------------------------------------------------------------------------
  //! Fluent interface: Setting backpressure strategy
  //----------------------------------------------------------------------------
  qclient::Options& withBackpressureStrategy(const BackpressureStrategy& str);

  //----------------------------------------------------------------------------
  //! Fluent interface: Setting retry strategy
  //----------------------------------------------------------------------------
  qclient::Options& withRetryStrategy(const RetryStrategy& str);
};

}

#endif
