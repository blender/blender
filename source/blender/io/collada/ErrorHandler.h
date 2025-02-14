/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "COLLADASaxFWLIErrorHandler.h"

/** \brief Handler class for parser errors
 */
class ErrorHandler : public COLLADASaxFWL::IErrorHandler {
 public:
  /** Constructor. */
  ErrorHandler();

  /** handle any error thrown by the parser. */
  bool handleError(const COLLADASaxFWL::IError *error) override;
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
