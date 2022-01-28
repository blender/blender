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
 * \ingroup bke
 *
 * To reduce the overhead of image processing this file contains a mechanism to detect areas of the
 * image that are changed. These areas are organized in chunks. Changes that happen over time are
 * organized in changesets.
 *
 * A common usecase is to update GPUTexture for drawing where only that part is uploaded that only
 * changed.
 */

#pragma once

#include "BLI_utildefines.h"

#include "BLI_rect.h"

#include "DNA_image_types.h"

extern "C" {
struct PartialUpdateUser;
struct PartialUpdateRegister;
}

namespace blender::bke::image {

using TileNumber = int;

namespace partial_update {

/* --- image_partial_update.cc --- */
/** Image partial updates. */

/**
 * \brief Result codes of #BKE_image_partial_update_collect_changes.
 */
enum class ePartialUpdateCollectResult {
  /** \brief Unable to construct partial updates. Caller should perform a full update. */
  FullUpdateNeeded,

  /** \brief No changes detected since the last time requested. */
  NoChangesDetected,

  /** \brief Changes detected since the last time requested. */
  PartialChangesDetected,
};

/**
 * \brief A region to update.
 *
 * Data is organized in tiles. These tiles are in texel space (1 unit is a single texel). When
 * tiles are requested they are merged with neighboring tiles.
 */
struct PartialUpdateRegion {
  /** \brief region of the image that has been updated. Region can be bigger than actual changes.
   */
  struct rcti region;

  /**
   * \brief Tile number (UDIM) that this region belongs to.
   */
  TileNumber tile_number;
};

/**
 * \brief Return codes of #BKE_image_partial_update_get_next_change.
 */
enum class ePartialUpdateIterResult {
  /** \brief no tiles left when iterating over tiles. */
  Finished = 0,

  /** \brief a chunk was available and has been loaded. */
  ChangeAvailable = 1,
};

/**
 * \brief collect the partial update since the last request.
 *
 * Invoke #BKE_image_partial_update_get_next_change to iterate over the collected tiles.
 *
 * \returns ePartialUpdateCollectResult::FullUpdateNeeded: called should not use partial updates
 * but recalculate the full image. This result can be expected when called for the first time for a
 * user and when it isn't possible to reconstruct the changes as the internal state doesn't have
 * enough data stored. ePartialUpdateCollectResult::NoChangesDetected: The have been no changes
 * detected since last invoke for the same user.
 * ePartialUpdateCollectResult::PartialChangesDetected: Parts of the image has been updated since
 * last invoke for the same user. The changes can be read by using
 * #BKE_image_partial_update_get_next_change.
 */
ePartialUpdateCollectResult BKE_image_partial_update_collect_changes(
    struct Image *image, struct PartialUpdateUser *user);

ePartialUpdateIterResult BKE_image_partial_update_get_next_change(
    struct PartialUpdateUser *user, struct PartialUpdateRegion *r_region);

/** \brief Abstract class to load tile data when using the PartialUpdateChecker. */
class AbstractTileData {
 protected:
  virtual ~AbstractTileData() = default;

 public:
  /**
   * \brief Load the data for the given tile_number.
   *
   * Invoked when changes are on a different tile compared to the previous tile..
   */
  virtual void init_data(TileNumber tile_number) = 0;
  /**
   * \brief Unload the data that has been loaded.
   *
   * Invoked when changes are on a different tile compared to the previous tile or when finished
   * iterating over the changes.
   */
  virtual void free_data() = 0;
};

/**
 * \brief Class to not load any tile specific data when iterating over changes.
 */
class NoTileData : AbstractTileData {
 public:
  NoTileData(Image *UNUSED(image), ImageUser *UNUSED(image_user))
  {
  }

  void init_data(TileNumber UNUSED(new_tile_number)) override
  {
  }

  void free_data() override
  {
  }
};

/**
 * \brief Load the ImageTile and ImBuf associated with the partial change.
 */
class ImageTileData : AbstractTileData {
 public:
  /**
   * \brief Not owned Image that is being iterated over.
   */
  Image *image;

  /**
   * \brief Local copy of the image user.
   *
   * The local copy is required so we don't change the image user of the caller.
   * We need to change it in order to request data for a specific tile.
   */
  ImageUser image_user = {0};

  /**
   * \brief ImageTile associated with the loaded tile.
   * Data is not owned by this instance but by the `image`.
   */
  ImageTile *tile = nullptr;

  /**
   * \brief ImBuf of the loaded tile.
   *
   * Can be nullptr when the file doesn't exist or when the tile hasn't been initialized.
   */
  ImBuf *tile_buffer = nullptr;

  ImageTileData(Image *image, ImageUser *image_user) : image(image)
  {
    if (image_user != nullptr) {
      this->image_user = *image_user;
    }
  }

  void init_data(TileNumber new_tile_number) override
  {
    image_user.tile = new_tile_number;
    tile = BKE_image_get_tile(image, new_tile_number);
    tile_buffer = BKE_image_acquire_ibuf(image, &image_user, NULL);
  }

  void free_data() override
  {
    BKE_image_release_ibuf(image, tile_buffer, nullptr);
    tile = nullptr;
    tile_buffer = nullptr;
  }
};

template<typename TileData = NoTileData> struct PartialUpdateChecker {

  /**
   * \brief Not owned Image that is being iterated over.
   */
  Image *image;
  ImageUser *image_user;

  /**
   * \brief the collected changes are stored inside the PartialUpdateUser.
   */
  PartialUpdateUser *user;

  struct CollectResult {
    PartialUpdateChecker<TileData> *checker;

    /**
     * \brief Tile specific data.
     */
    TileData tile_data;
    PartialUpdateRegion changed_region;
    ePartialUpdateCollectResult result_code;

   private:
    TileNumber last_tile_number;

   public:
    CollectResult(PartialUpdateChecker<TileData> *checker, ePartialUpdateCollectResult result_code)
        : checker(checker),
          tile_data(checker->image, checker->image_user),
          result_code(result_code)
    {
    }

    const ePartialUpdateCollectResult get_result_code() const
    {
      return result_code;
    }

    /**
     * \brief Load the next changed region.
     *
     * This member function can only be called when partial changes are detected.
     * (`get_result_code()` returns `ePartialUpdateCollectResult::PartialChangesDetected`).
     *
     * When changes for another tile than the previous tile is loaded the #tile_data will be
     * updated.
     */
    ePartialUpdateIterResult get_next_change()
    {
      BLI_assert(result_code == ePartialUpdateCollectResult::PartialChangesDetected);
      ePartialUpdateIterResult result = BKE_image_partial_update_get_next_change(checker->user,
                                                                                 &changed_region);
      switch (result) {
        case ePartialUpdateIterResult::Finished:
          tile_data.free_data();
          return result;

        case ePartialUpdateIterResult::ChangeAvailable:
          if (last_tile_number == changed_region.tile_number) {
            return result;
          }
          tile_data.free_data();
          tile_data.init_data(changed_region.tile_number);
          last_tile_number = changed_region.tile_number;
          return result;

        default:
          BLI_assert_unreachable();
          return result;
      }
    }
  };

 public:
  PartialUpdateChecker(Image *image, ImageUser *image_user, PartialUpdateUser *user)
      : image(image), image_user(image_user), user(user)
  {
  }

  /**
   * \brief Check for new changes since the last time this method was invoked for this #user.
   */
  CollectResult collect_changes()
  {
    ePartialUpdateCollectResult collect_result = BKE_image_partial_update_collect_changes(image,
                                                                                          user);
    return CollectResult(this, collect_result);
  }
};

}  // namespace partial_update
}  // namespace blender::bke::image
