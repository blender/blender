/* SPDX-FileCopyrightText: 2002-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
