// ----------------------------------------------------------------------
// File: AssistedThread.hh
// Author: Georgios Bitzes - CERN
// ----------------------------------------------------------------------

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

#ifndef __QCLIENT_ASSISTED_THREAD_H__
#define __QCLIENT_ASSISTED_THREAD_H__

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>

namespace qclient {

class AssistedThread;
class ThreadAssistant {
public:
  ThreadAssistant(bool flag) : stopFlag(flag) {}

  void reset() {
    stopFlag = false;
  }

  void requestTermination() {
    std::lock_guard<std::mutex> lock(mtx);
    stopFlag = true;
    notifier.notify_all();
  }

  bool terminationRequested() {
    return stopFlag;
  }

  template<typename T>
  void wait_for(T duration) {
    std::unique_lock<std::mutex> lock(mtx);

    if(stopFlag) return;
    notifier.wait_for(lock, duration);
  }

  template<typename T>
  void wait_until(T duration) {
    std::unique_lock<std::mutex> lock(mtx);

    if(stopFlag) return;
    notifier.wait_until(lock, duration);
  }

private:
  friend class AssistedThread;

  std::atomic<bool> stopFlag;
  std::mutex mtx;
  std::condition_variable notifier;
};

class AssistedThread {
public:
  // null constructor, no underlying thread
  AssistedThread() : assistant(true), joined(true) { }

  // universal references, perfect forwarding, variadic template
  // (C++ is intensifying)
  template<typename... Args>
  AssistedThread(Args&&... args) : assistant(false), joined(false), th(std::forward<Args>(args)..., std::ref(assistant)) {
  }

  // No assignment
  AssistedThread& operator=(const AssistedThread&) = delete;
  AssistedThread& operator=(AssistedThread&& src) = delete;

  template<typename... Args>
  void reset(Args&&... args) {
    join();

    assistant.reset();
    joined = false;
    th = std::thread(std::forward<Args>(args)..., std::ref(assistant));
  }

  virtual ~AssistedThread() {
    join();
  }

  void stop() {
    if(joined) return;
    assistant.requestTermination();
  }

  void join() {
    if(joined) return;
    stop();

    blockUntilThreadJoins();
  }

  // Different meaning than join, which explicitly asks the thread to
  // terminate. Here, we simply wait until the thread exits on its own.
  void blockUntilThreadJoins() {
    if(joined) return;

    th.join();
    joined = true;
  }

private:
  ThreadAssistant assistant;

  std::atomic<bool> joined;
  std::thread th;
};

}

#endif
