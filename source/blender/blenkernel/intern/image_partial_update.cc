/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * \file
 * \ingroup bke
 *
 * To reduce the overhead of image processing this file contains a mechanism to detect areas of the
 * image that are changed. These areas are organized in chunks. Changes that happen over time are
 * organized in changesets.
 *
 * A common use case is to update #GPUTexture for drawing where only that part is uploaded that
 * only changed.
 *
 * Usage:
 *
 * \code{.cc}
 * Image *image = ...;
 * ImBuf *image_buffer = ...;
 *
 * // Partial_update_user should be kept for the whole session where the changes needs to be
 * // tracked. Keep this instance alive as long as you need to track image changes.
 *
 * PartialUpdateUser *partial_update_user = BKE_image_partial_update_create(image);
 *
 * ...
 *
 * switch (BKE_image_partial_update_collect_changes(image, image_buffer))
 * {
 * case ePartialUpdateCollectResult::FullUpdateNeeded:
 *  // Unable to do partial updates. Perform a full update.
 *  break;
 * case ePartialUpdateCollectResult::PartialChangesDetected:
 *  PartialUpdateRegion change;
 *  while (BKE_image_partial_update_get_next_change(partial_update_user, &change) ==
 *         ePartialUpdateIterResult::ChangeAvailable){
 *  // Do something with the change.
 *  }
 *  case ePartialUpdateCollectResult::NoChangesDetected:
 *    break;
 * }
 *
 * ...
 *
 * // Free partial_update_user.
 * BKE_image_partial_update_free(partial_update_user);
 * \endcode
 */

#include <optional>

#include "BKE_image.h"
#include "BKE_image_partial_update.hh"

#include "DNA_image_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_vector.hh"

namespace blender::bke::image::partial_update {

/** \brief Size of chunks to track changes. */
constexpr int CHUNK_SIZE = 256;

/**
 * \brief Max number of changesets to keep in history.
 *
 * A higher number would need more memory and processing
 * to calculate a changeset, but would lead to do partial updates for requests that don't happen
 * every frame.
 *
 * A to small number would lead to more full updates when changes couldn't be reconstructed from
 * the available history.
 */
constexpr int MAX_HISTORY_LEN = 4;

/**
 * \brief get the chunk number for the give pixel coordinate.
 *
 * As chunks are squares the this member can be used for both x and y axis.
 */
static int chunk_number_for_pixel(int pixel_offset)
{
  int chunk_offset = pixel_offset / CHUNK_SIZE;
  if (pixel_offset < 0) {
    chunk_offset -= 1;
  }
  return chunk_offset;
}

struct PartialUpdateRegisterImpl;
struct PartialUpdateUserImpl;

/**
 * Wrap PartialUpdateUserImpl to its C-struct (PartialUpdateUser).
 */
static PartialUpdateUser *wrap(PartialUpdateUserImpl *user)
{
  return static_cast<PartialUpdateUser *>(static_cast<void *>(user));
}

/**
 * Unwrap the PartialUpdateUser C-struct to its CPP counterpart (PartialUpdateUserImpl).
 */
static PartialUpdateUserImpl *unwrap(PartialUpdateUser *user)
{
  return static_cast<PartialUpdateUserImpl *>(static_cast<void *>(user));
}

/**
 * Wrap PartialUpdateRegisterImpl to its C-struct (PartialUpdateRegister).
 */
static PartialUpdateRegister *wrap(PartialUpdateRegisterImpl *partial_update_register)
{
  return static_cast<PartialUpdateRegister *>(static_cast<void *>(partial_update_register));
}

/**
 * Unwrap the PartialUpdateRegister C-struct to its CPP counterpart (PartialUpdateRegisterImpl).
 */
static PartialUpdateRegisterImpl *unwrap(PartialUpdateRegister *partial_update_register)
{
  return static_cast<PartialUpdateRegisterImpl *>(static_cast<void *>(partial_update_register));
}

using ChangesetID = int64_t;
constexpr ChangesetID UnknownChangesetID = -1;

struct PartialUpdateUserImpl {
  /** \brief last changeset id that was seen by this user. */
  ChangesetID last_changeset_id = UnknownChangesetID;

  /** \brief regions that have been updated. */
  Vector<PartialUpdateRegion> updated_regions;

#ifdef NDEBUG
  /** \brief reference to image to validate correct API usage. */
  const void *debug_image_;
#endif

  /**
   * \brief Clear the list of updated regions.
   *
   * Updated regions should be cleared at the start of #BKE_image_partial_update_collect_changes so
   * the
   */
  void clear_updated_regions()
  {
    updated_regions.clear();
  }
};

/**
 * \brief Dirty chunks of an ImageTile.
 *
 * Internally dirty tiles are grouped together in change sets to make sure that the correct
 * answer can be built for different users reducing the amount of merges.
 */
struct TileChangeset {
 private:
  /** \brief Dirty flag for each chunk. */
  std::vector<bool> chunk_dirty_flags_;
  /** \brief are there dirty/ */
  bool has_dirty_chunks_ = false;

 public:
  /** \brief Width of the tile in pixels. */
  int tile_width;
  /** \brief Height of the tile in pixels. */
  int tile_height;
  /** \brief Number of chunks along the x-axis. */
  int chunk_x_len;
  /** \brief Number of chunks along the y-axis. */
  int chunk_y_len;

  TileNumber tile_number;

  void clear()
  {
    init_chunks(chunk_x_len, chunk_y_len);
  }

  /**
   * \brief Update the resolution of the tile.
   *
   * \returns true: resolution has been updated.
   *          false: resolution was unchanged.
   */
  bool update_resolution(const ImBuf *image_buffer)
  {
    if (tile_width == image_buffer->x && tile_height == image_buffer->y) {
      return false;
    }

    tile_width = image_buffer->x;
    tile_height = image_buffer->y;

    int chunk_x_len = (tile_width + CHUNK_SIZE - 1) / CHUNK_SIZE;
    int chunk_y_len = (tile_height + CHUNK_SIZE - 1) / CHUNK_SIZE;
    init_chunks(chunk_x_len, chunk_y_len);
    return true;
  }

  void mark_region(const rcti *updated_region)
  {
    int start_x_chunk = chunk_number_for_pixel(updated_region->xmin);
    int end_x_chunk = chunk_number_for_pixel(updated_region->xmax - 1);
    int start_y_chunk = chunk_number_for_pixel(updated_region->ymin);
    int end_y_chunk = chunk_number_for_pixel(updated_region->ymax - 1);

    /* Clamp tiles to tiles in image. */
    start_x_chunk = max_ii(0, start_x_chunk);
    start_y_chunk = max_ii(0, start_y_chunk);
    end_x_chunk = min_ii(chunk_x_len - 1, end_x_chunk);
    end_y_chunk = min_ii(chunk_y_len - 1, end_y_chunk);

    /* Early exit when no tiles need to be updated. */
    if (start_x_chunk >= chunk_x_len) {
      return;
    }
    if (start_y_chunk >= chunk_y_len) {
      return;
    }
    if (end_x_chunk < 0) {
      return;
    }
    if (end_y_chunk < 0) {
      return;
    }

    mark_chunks_dirty(start_x_chunk, start_y_chunk, end_x_chunk, end_y_chunk);
  }

  void mark_chunks_dirty(int start_x_chunk, int start_y_chunk, int end_x_chunk, int end_y_chunk)
  {
    for (int chunk_y = start_y_chunk; chunk_y <= end_y_chunk; chunk_y++) {
      for (int chunk_x = start_x_chunk; chunk_x <= end_x_chunk; chunk_x++) {
        int chunk_index = chunk_y * chunk_x_len + chunk_x;
        chunk_dirty_flags_[chunk_index] = true;
      }
    }
    has_dirty_chunks_ = true;
  }

  bool has_dirty_chunks() const
  {
    return has_dirty_chunks_;
  }

  void init_chunks(int chunk_x_len_, int chunk_y_len_)
  {
    chunk_x_len = chunk_x_len_;
    chunk_y_len = chunk_y_len_;
    const int chunk_len = chunk_x_len * chunk_y_len;
    const int previous_chunk_len = chunk_dirty_flags_.size();

    chunk_dirty_flags_.resize(chunk_len);
    /* Fast exit. When the changeset was already empty no need to
     * re-initialize the chunk_validity. */
    if (!has_dirty_chunks()) {
      return;
    }
    for (int index = 0; index < min_ii(chunk_len, previous_chunk_len); index++) {
      chunk_dirty_flags_[index] = false;
    }
    has_dirty_chunks_ = false;
  }

  /** \brief Merge the given changeset into the receiver. */
  void merge(const TileChangeset &other)
  {
    BLI_assert(chunk_x_len == other.chunk_x_len);
    BLI_assert(chunk_y_len == other.chunk_y_len);
    const int chunk_len = chunk_x_len * chunk_y_len;

    for (int chunk_index = 0; chunk_index < chunk_len; chunk_index++) {
      chunk_dirty_flags_[chunk_index] = chunk_dirty_flags_[chunk_index] ||
                                        other.chunk_dirty_flags_[chunk_index];
    }
    has_dirty_chunks_ |= other.has_dirty_chunks_;
  }

  /** \brief has a chunk changed inside this changeset. */
  bool is_chunk_dirty(int chunk_x, int chunk_y) const
  {
    const int chunk_index = chunk_y * chunk_x_len + chunk_x;
    return chunk_dirty_flags_[chunk_index];
  }
};

/** \brief Changeset keeping track of changes for an image */
struct Changeset {
 private:
  Vector<TileChangeset> tiles;

 public:
  /** \brief Keep track if any of the tiles have dirty chunks. */
  bool has_dirty_chunks;

  /**
   * \brief Retrieve the TileChangeset for the given ImageTile.
   *
   * When the TileChangeset isn't found, it will be added.
   */
  TileChangeset &operator[](const ImageTile *image_tile)
  {
    for (TileChangeset &tile_changeset : tiles) {
      if (tile_changeset.tile_number == image_tile->tile_number) {
        return tile_changeset;
      }
    }

    TileChangeset tile_changeset;
    tile_changeset.tile_number = image_tile->tile_number;
    tiles.append_as(tile_changeset);

    return tiles.last();
  }

  /** \brief Does this changeset contain data for the given tile. */
  bool has_tile(const ImageTile *image_tile)
  {
    for (TileChangeset &tile_changeset : tiles) {
      if (tile_changeset.tile_number == image_tile->tile_number) {
        return true;
      }
    }
    return false;
  }

  /** \brief Clear this changeset. */
  void clear()
  {
    tiles.clear();
    has_dirty_chunks = false;
  }
};

/**
 * \brief Partial update changes stored inside the image runtime.
 *
 * The PartialUpdateRegisterImpl will keep track of changes over time. Changes are groups inside
 * TileChangesets.
 */
struct PartialUpdateRegisterImpl {
  /** \brief changeset id of the first changeset kept in #history. */
  ChangesetID first_changeset_id;
  /** \brief changeset id of the top changeset kept in #history. */
  ChangesetID last_changeset_id;

  /** \brief history of changesets. */
  Vector<Changeset> history;
  /** \brief The current changeset. New changes will be added to this changeset. */
  Changeset current_changeset;

  void update_resolution(const ImageTile *image_tile, const ImBuf *image_buffer)
  {
    TileChangeset &tile_changeset = current_changeset[image_tile];
    const bool has_dirty_chunks = tile_changeset.has_dirty_chunks();
    const bool resolution_changed = tile_changeset.update_resolution(image_buffer);

    if (has_dirty_chunks && resolution_changed && !history.is_empty()) {
      mark_full_update();
    }
  }

  void mark_full_update()
  {
    history.clear();
    last_changeset_id++;
    current_changeset.clear();
    first_changeset_id = last_changeset_id;
  }

  void mark_region(const ImageTile *image_tile, const rcti *updated_region)
  {
    TileChangeset &tile_changeset = current_changeset[image_tile];
    tile_changeset.mark_region(updated_region);
    current_changeset.has_dirty_chunks |= tile_changeset.has_dirty_chunks();
  }

  void ensure_empty_changeset()
  {
    if (!current_changeset.has_dirty_chunks) {
      /* No need to create a new changeset when previous changeset does not contain any dirty
       * tiles. */
      return;
    }
    commit_current_changeset();
    limit_history();
  }

  /** \brief Move the current changeset to the history and resets the current changeset. */
  void commit_current_changeset()
  {
    history.append_as(std::move(current_changeset));
    current_changeset.clear();
    last_changeset_id++;
  }

  /** \brief Limit the number of items in the changeset. */
  void limit_history()
  {
    const int num_items_to_remove = max_ii(history.size() - MAX_HISTORY_LEN, 0);
    if (num_items_to_remove == 0) {
      return;
    }
    history.remove(0, num_items_to_remove);
    first_changeset_id += num_items_to_remove;
  }

  /**
   * \brief Check if data is available to construct the update tiles for the given
   * changeset_id.
   *
   * The update tiles can be created when changeset id is between
   */
  bool can_construct(ChangesetID changeset_id)
  {
    if (changeset_id < first_changeset_id) {
      return false;
    }

    if (changeset_id > last_changeset_id) {
      return false;
    }

    return true;
  }

  /**
   * \brief collect all historic changes since a given changeset.
   */
  std::optional<TileChangeset> changed_tile_chunks_since(const ImageTile *image_tile,
                                                         const ChangesetID from_changeset)
  {
    std::optional<TileChangeset> changed_chunks = std::nullopt;
    for (int index = from_changeset - first_changeset_id; index < history.size(); index++) {
      if (!history[index].has_tile(image_tile)) {
        continue;
      }

      TileChangeset &tile_changeset = history[index][image_tile];
      if (!changed_chunks.has_value()) {
        changed_chunks = std::make_optional<TileChangeset>();
        changed_chunks->init_chunks(tile_changeset.chunk_x_len, tile_changeset.chunk_y_len);
        changed_chunks->tile_number = image_tile->tile_number;
      }

      changed_chunks->merge(tile_changeset);
    }
    return changed_chunks;
  }
};

static PartialUpdateRegister *image_partial_update_register_ensure(Image *image)
{
  if (image->runtime.partial_update_register == nullptr) {
    PartialUpdateRegisterImpl *partial_update_register = MEM_new<PartialUpdateRegisterImpl>(
        __func__);
    image->runtime.partial_update_register = wrap(partial_update_register);
  }
  return image->runtime.partial_update_register;
}

ePartialUpdateCollectResult BKE_image_partial_update_collect_changes(Image *image,
                                                                     PartialUpdateUser *user)
{
  PartialUpdateUserImpl *user_impl = unwrap(user);
#ifdef NDEBUG
  BLI_assert(image == user_impl->debug_image_);
#endif

  user_impl->clear_updated_regions();

  PartialUpdateRegisterImpl *partial_updater = unwrap(image_partial_update_register_ensure(image));
  partial_updater->ensure_empty_changeset();

  if (!partial_updater->can_construct(user_impl->last_changeset_id)) {
    user_impl->last_changeset_id = partial_updater->last_changeset_id;
    return ePartialUpdateCollectResult::FullUpdateNeeded;
  }

  /* Check if there are changes since last invocation for the user. */
  if (user_impl->last_changeset_id == partial_updater->last_changeset_id) {
    return ePartialUpdateCollectResult::NoChangesDetected;
  }

  /* Collect changed tiles. */
  LISTBASE_FOREACH (ImageTile *, tile, &image->tiles) {
    std::optional<TileChangeset> changed_chunks = partial_updater->changed_tile_chunks_since(
        tile, user_impl->last_changeset_id);
    /* Check if chunks of this tile are dirty. */
    if (!changed_chunks.has_value()) {
      continue;
    }
    if (!changed_chunks->has_dirty_chunks()) {
      continue;
    }

    /* Convert tiles in the changeset to rectangles that are dirty. */
    for (int chunk_y = 0; chunk_y < changed_chunks->chunk_y_len; chunk_y++) {
      for (int chunk_x = 0; chunk_x < changed_chunks->chunk_x_len; chunk_x++) {
        if (!changed_chunks->is_chunk_dirty(chunk_x, chunk_y)) {
          continue;
        }

        PartialUpdateRegion region;
        region.tile_number = tile->tile_number;
        BLI_rcti_init(&region.region,
                      chunk_x * CHUNK_SIZE,
                      (chunk_x + 1) * CHUNK_SIZE,
                      chunk_y * CHUNK_SIZE,
                      (chunk_y + 1) * CHUNK_SIZE);
        user_impl->updated_regions.append_as(region);
      }
    }
  }

  user_impl->last_changeset_id = partial_updater->last_changeset_id;
  return ePartialUpdateCollectResult::PartialChangesDetected;
}

ePartialUpdateIterResult BKE_image_partial_update_get_next_change(PartialUpdateUser *user,
                                                                  PartialUpdateRegion *r_region)
{
  PartialUpdateUserImpl *user_impl = unwrap(user);
  if (user_impl->updated_regions.is_empty()) {
    return ePartialUpdateIterResult::Finished;
  }
  PartialUpdateRegion region = user_impl->updated_regions.pop_last();
  *r_region = region;
  return ePartialUpdateIterResult::ChangeAvailable;
}

}  // namespace blender::bke::image::partial_update

extern "C" {

using namespace blender::bke::image::partial_update;

/* TODO(@jbakker): cleanup parameter. */
PartialUpdateUser *BKE_image_partial_update_create(const Image *image)
{
  PartialUpdateUserImpl *user_impl = MEM_new<PartialUpdateUserImpl>(__func__);

#ifdef NDEBUG
  user_impl->debug_image_ = image;
#else
  UNUSED_VARS(image);
#endif

  return wrap(user_impl);
}

void BKE_image_partial_update_free(PartialUpdateUser *user)
{
  PartialUpdateUserImpl *user_impl = unwrap(user);
  MEM_delete<PartialUpdateUserImpl>(user_impl);
}

/* --- Image side --- */

void BKE_image_partial_update_register_free(Image *image)
{
  PartialUpdateRegisterImpl *partial_update_register = unwrap(
      image->runtime.partial_update_register);
  if (partial_update_register) {
    MEM_delete<PartialUpdateRegisterImpl>(partial_update_register);
  }
  image->runtime.partial_update_register = nullptr;
}

void BKE_image_partial_update_mark_region(Image *image,
                                          const ImageTile *image_tile,
                                          const ImBuf *image_buffer,
                                          const rcti *updated_region)
{
  PartialUpdateRegisterImpl *partial_updater = unwrap(image_partial_update_register_ensure(image));
  partial_updater->update_resolution(image_tile, image_buffer);
  partial_updater->mark_region(image_tile, updated_region);
}

void BKE_image_partial_update_mark_full_update(Image *image)
{
  PartialUpdateRegisterImpl *partial_updater = unwrap(image_partial_update_register_ensure(image));
  partial_updater->mark_full_update();
}
}
