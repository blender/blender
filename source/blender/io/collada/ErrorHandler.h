/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <algorithm> /* sort() */
#include <map>
#include <string>
#include <vector>

#include "COLLADASaxFWLIErrorHandler.h"

/** \brief Handler class for parser errors
 */
class ErrorHandler : public COLLADASaxFWL::IErrorHandler {
 public:
  /** Constructor. */
  ErrorHandler();

  /** handle any error thrown by the parser. */
  bool virtual handleError(const COLLADASaxFWL::IError *error);
  /** True if there was an error during parsing. */
  bool hasError()
  {
    return mError;
  }

 private:
  /** Disable default copy constructor. */
  ErrorHandler(const ErrorHandler &pre);
  /** Disable default assignment operator. */
  const ErrorHandler &operator=(const ErrorHandler &pre);
  /** Hold error status. */
  bool mError;
};
