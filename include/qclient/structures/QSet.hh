//------------------------------------------------------------------------------
//! @file QSet.hh
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

#pragma once
#include "qclient/QClient.hh"
#include "qclient/Namespace.hh"
#include "qclient/Utils.hh"
#include "qclient/AsyncHandler.hh"
#include <vector>
#include <set>

QCLIENT_NAMESPACE_BEGIN

//------------------------------------------------------------------------------
//! Class QSet
//------------------------------------------------------------------------------
class QSet
{
public:
  //----------------------------------------------------------------------------
  //! Default constructor
  //----------------------------------------------------------------------------
  QSet(): mClient(nullptr) {}

  //----------------------------------------------------------------------------
  //! Constructor
  //!
  //! @param cl qclient object used for sending requests
  //! @param set_key the key name for the hash
  //----------------------------------------------------------------------------
  QSet(QClient& cl, const std::string& set_key):
    mClient(&cl), mKey(set_key)
  {}

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  QSet(const QSet& other);

  //----------------------------------------------------------------------------
  //! Copy assignment
  //----------------------------------------------------------------------------
  QSet& operator=(const QSet& other);

  //----------------------------------------------------------------------------
  //! Move constructor
  //----------------------------------------------------------------------------
  QSet(QSet&& other) = delete;

  //----------------------------------------------------------------------------
  //! Move assignment
  //----------------------------------------------------------------------------
  QSet& operator=(QSet&& other) = default;

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  ~QSet()
  {
    mClient = nullptr;
  }

  //----------------------------------------------------------------------------
  //! Get the Set key
  //!
  //! @return string
  //----------------------------------------------------------------------------
  std::string getKey() const
  {
    return mKey;
  }

  //----------------------------------------------------------------------------
  //! Set the Set key
  //!
  //! @param new_key new key value
  //----------------------------------------------------------------------------
  void setKey(const std::string& new_key)
  {
    mKey = new_key;
  }

  //----------------------------------------------------------------------------
  //! Set client object
  //!
  //! @param new_client new client
  //----------------------------------------------------------------------------
  void setClient(QClient& new_client)
  {
    mClient = &new_client;
  }

  //----------------------------------------------------------------------------
  //! Get client object
  //!
  //! @param new_client new client
  //----------------------------------------------------------------------------
  inline QClient* getClient()
  {
    return mClient;
  }

  //----------------------------------------------------------------------------
  //! Redis SET add command - synchronous
  //!
  //! @param member value to be added to the set
  //!
  //! @return true if member added, otherwise false
  //----------------------------------------------------------------------------
  bool sadd(const std::string& member);

  //----------------------------------------------------------------------------
  //! Redis SET add command - asynchronous
  //!
  //! @param member value to be added to the set
  //! @param ah async handler
  //----------------------------------------------------------------------------
  void sadd_async(const std::string& member, AsyncHandler* ah);

  //----------------------------------------------------------------------------
  //! Redis SET add command for multiple members - synchronous
  //!
  //! @param lst_elem values to be added to the set
  //!
  //! @return number of elements added to the set
  //----------------------------------------------------------------------------
  long long int sadd(std::list<std::string> lst_elem);

  //----------------------------------------------------------------------------
  //! Redis SET add command for multiple elements - asynchronous
  //!
  //! @param set_elem values to be added to the set
  //! @param ah async handler
  //----------------------------------------------------------------------------
  void sadd_async(const std::set<std::string>& set_elem, AsyncHandler* ah);

  //----------------------------------------------------------------------------
  //! Redis SET add command for multiple elements - asynchronous
  //!
  //! @param set_elem values to be added to the set
  //! @param ah async handler
  //----------------------------------------------------------------------------
  void sadd_async(const std::list<std::string>& set_elem, AsyncHandler* ah);

  //----------------------------------------------------------------------------
  //! Redis SET add command for multiple elements - asynchronous
  //!
  //! @param begin begin iterator
  //! @param end end iterator
  //! @param ah async handler
  //----------------------------------------------------------------------------
  template<typename Iterator>
  void sadd_async(const Iterator& begin, const Iterator& end, AsyncHandler* ah);

  //----------------------------------------------------------------------------
  //! Redis SET remove command - synchronous
  //!
  //! @param member value to be removed from the set
  //!
  //! @return true if member removed, otherwise false
  //----------------------------------------------------------------------------
  bool srem(const std::string& member);

  //----------------------------------------------------------------------------
  //! Redis SET remove command - asynchronous
  //!
  //! @param member value to be removed from the set
  //! @param ah async handler
  //----------------------------------------------------------------------------
  void srem_async(const std::string& member, AsyncHandler* ah);

  //----------------------------------------------------------------------------
  //! Redis SET remove command for multiple members - synchronous
  //!
  //! @param lst_elem values to be removed from the set
  //!
  //! @return number of elements removed from the set
  //----------------------------------------------------------------------------
  long long int srem(std::list<std::string> lst_elem);

  //----------------------------------------------------------------------------
  //! Redis SET size command - synchronous
  //!
  //! @return size of the set
  //----------------------------------------------------------------------------
  long long int scard();

  //----------------------------------------------------------------------------
  //! Redis SET ismember command - synchronous
  //!
  //! @param member value to be searched in the set
  //!
  //! @return true if member in the set, otherwise false
  //----------------------------------------------------------------------------
  bool sismember(const std::string& member);

  //----------------------------------------------------------------------------
  //! Redis SET members command - synchronous
  //!
  //! @return set containing the members of the set
  //----------------------------------------------------------------------------
  std::set<std::string> smembers();

  //----------------------------------------------------------------------------
  //! Redis SET SSCAN command - synchronous
  //!
  //! @param cursor cursor for current request
  //! @param count max number of elements to return
  //!
  //! @return pair representing the cursor value and a vector of the elements
  //!          returned in the current step
  //----------------------------------------------------------------------------
  std::pair< std::string, std::vector<std::string> >
  sscan(const std::string &cursor, long long count = 1000);

  //----------------------------------------------------------------------------
  //! Iterator class
  //-----------------------------------------------------------------------------
  class Iterator {
  public:
    bool valid() const;
    void next();

    std::string getElement() const;
    size_t requestsSoFar() const;

  private:
    friend class QSet;
    Iterator(QSet &qset, size_t count, const std::string &startCursor);
    void fillFromBackend();

    QSet &qset;
    uint32_t count;
    std::string cursor;
    bool reachedEnd = false;
    std::vector<std::string> results;
    size_t reqs = 0;

    std::vector<std::string>::const_iterator it;
  };

  Iterator getIterator(size_t count = 100000, const std::string &startCursor = "0");

private:
  QClient* mClient; ///< Qclient client object
  std::string mKey; ///< Key of the set object
};

//------------------------------------------------------------------------------
// Set related templated methods implementation
//------------------------------------------------------------------------------
inline void QSet::sadd_async(const std::string& member, AsyncHandler* ah)
{
  ah->Register(mClient, {"SADD", mKey, member});
}

template<typename Iter>
void
QSet::sadd_async(const Iter& begin, const Iter& end, AsyncHandler* ah)
{
  std::vector<std::string> cmd;
  cmd.reserve(std::distance(begin, end) + 2);
  (void) cmd.push_back("SADD");
  (void) cmd.push_back(mKey);
  (void) cmd.insert(cmd.end(), begin, end);
  ah->Register(mClient, cmd);
}

inline bool QSet::sadd(const std::string& member)
{
  redisReplyPtr reply = mClient->exec("SADD", mKey, member).get();

  if ((reply == nullptr) || (reply->type != REDIS_REPLY_INTEGER)) {
    throw std::runtime_error("[FATAL] Error sadd key: " + mKey + " field: "
                             + member + ": Unexpected/null reply");
  }

  return (reply->integer == 1);
}

inline void QSet::srem_async(const std::string& member, AsyncHandler* ah)
{
  ah->Register(mClient, {"SREM", mKey, member});
}

inline bool QSet::srem(const std::string& member)
{
  redisReplyPtr reply = mClient->exec("SREM", mKey, member).get();

  if ((reply == nullptr) || (reply->type != REDIS_REPLY_INTEGER)) {
    throw std::runtime_error("[FATAL] Error srem key: " + mKey + " member: "
                             + member + ": Unexpected/null reply");
  }

  return (reply->integer == 1);
}

inline bool QSet::sismember(const std::string& member)
{
  redisReplyPtr reply = mClient->exec("SISMEMBER", mKey, member).get();

  if ((reply == nullptr) || (reply->type != REDIS_REPLY_INTEGER)) {
    throw std::runtime_error("[FATAL] Error sismember key: " + mKey + " member: "
                             + member + ": Unexpected/null reply");
  }

  return (reply->integer == 1);
}

QCLIENT_NAMESPACE_END
