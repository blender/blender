/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
