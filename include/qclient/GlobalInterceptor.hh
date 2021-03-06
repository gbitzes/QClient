//------------------------------------------------------------------------------
// File: GlobalInterceptor.hh
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

#ifndef QCLIENT_GLOBAL_INTERCEPTOR_HH
#define QCLIENT_GLOBAL_INTERCEPTOR_HH

#include "qclient/Members.hh"

namespace qclient {

class GlobalInterceptor {
public:
  //----------------------------------------------------------------------------
  // Add interception for selected endpoint. If QClient has to contact "from"
  // for any reason, it will actually translate that to "to".
  // Used in tests.
  //----------------------------------------------------------------------------
  static void addIntercept(const Endpoint &from, const Endpoint &to);

  //----------------------------------------------------------------------------
  // Clear any existing intercepts.
  //----------------------------------------------------------------------------
  static void clearIntercepts();

  //----------------------------------------------------------------------------
  // Translate an Endpoint. If no intercept exists, it will be returned as-is.
  // Otherwise, the intercepting endpoint will be returned.
  //----------------------------------------------------------------------------
  static Endpoint translate(const Endpoint &target);
};

}

#endif
