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
  GHOST_XrException(const char *msg, int result = 0)
      : std::exception(), m_msg(msg), m_result(result)
  {
  }

  const char *what() const noexcept override
  {
    return m_msg.data();
  }

 private:
  std::string m_msg;
  int m_result;
};
