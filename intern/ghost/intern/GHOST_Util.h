/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <functional>

/**
 * RAII wrapper for typical C `void *` custom data.
 * Used for exception safe custom-data handling during constructor calls.
 */
struct GHOST_C_CustomDataWrapper {
  using FreeFn = std::function<void(void *)>;

  void *custom_data_;
  FreeFn free_fn_;

  GHOST_C_CustomDataWrapper(void *custom_data, FreeFn free_fn)
      : custom_data_(custom_data), free_fn_(free_fn)
  {
  }
  ~GHOST_C_CustomDataWrapper()
  {
    if (free_fn_ != nullptr && custom_data_ != nullptr) {
      free_fn_(custom_data_);
    }
  }
};
