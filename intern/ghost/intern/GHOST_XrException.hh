/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <exception>
#include <string>

class GHOST_XrException : public std::exception {
  friend class GHOST_XrContext;

 public:
  GHOST_XrException(const char *msg, int result = 0) : std::exception(), msg_(msg), result_(result)
  {
  }

  const char *what() const noexcept override
  {
    return msg_.data();
  }

 private:
  std::string msg_;
  int result_;
};
