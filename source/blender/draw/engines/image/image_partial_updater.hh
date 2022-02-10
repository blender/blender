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
 *
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BKE_image.h"
#include "BKE_image_partial_update.hh"

struct PartialImageUpdater {
  struct PartialUpdateUser *user;
  const struct Image *image;

  /**
   * \brief Ensure that there is a partial update user for the given image.
   */
  void ensure_image(const Image *new_image)
  {
    if (!is_valid(new_image)) {
      free();
      create(new_image);
    }
  }

  virtual ~PartialImageUpdater()
  {
    free();
  }

 private:
  /**
   * \brief check if the partial update user can still be used for the given image.
   *
   * When switching to a different image the partial update user should be recreated.
   */
  bool is_valid(const Image *new_image) const
  {
    if (image != new_image) {
      return false;
    }

    return user != nullptr;
  }

  void create(const Image *new_image)
  {
    BLI_assert(user == nullptr);
    user = BKE_image_partial_update_create(new_image);
    image = new_image;
  }

  void free()
  {
    if (user != nullptr) {
      BKE_image_partial_update_free(user);
      user = nullptr;
      image = nullptr;
    }
  }
};
