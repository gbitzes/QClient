//------------------------------------------------------------------------------
// File: QCallback.hh
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

#ifndef QCLIENT_QCALLBACK_H
#define QCLIENT_QCALLBACK_H

#include "qclient/Reply.hh"

namespace qclient {

class QCallback {
public:
  QCallback() {}
  virtual ~QCallback() {}

  // It is NOT safe to issue further requests to the driving QClient if reply
  // is nullptr, as it could be in the process of shutting down!
  virtual void handleResponse(redisReplyPtr &&reply) = 0;
};

}

#endif
