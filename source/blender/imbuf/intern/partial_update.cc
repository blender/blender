/* SPDX-FileCopyrightText: 2024-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>
#include <optional>

#include "IMB_imbuf_types.hh"
#include "IMB_partial_update.hh"

#include "BLI_bit_vector.hh"
#include "BLI_math_base_c.hh"
#include "BLI_mutex.hh"
#include "BLI_rect.hh"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "atomic_ops.h"

namespace blender {

namespace imbuf::partial_update {

/**
 * Max number of changesets to keep in history.
 *
 * Longer history needs more memory and processing, but lets more consumers benefit from partial
 * changes, when they don't redraw/update on every frame.
 */
constexpr int MAX_HISTORY_LEN = 4;

/**
 * Get the chunk number for the given pixel coordinate.
 *
 * As chunks are squares this can be used for both x and y axis.
 */
static int chunk_number_for_pixel(int pixel_offset)
{
  int chunk_offset = pixel_offset / CHUNK_SIZE;
  if (pixel_offset < 0) {
    chunk_offset -= 1;
  }
  return chunk_offset;
}

using ChangesetID = int64_t;

/** Dirty chunks of a buffer at a given resolution. */
struct Changeset {
 private:
  /** Dirty flag for each chunk. */
  BitVector<> chunk_modified_flags_;
  /** True if any chunk is modified. */
  bool has_modified_chunks_ = false;

 public:
  /** Global changeset ID for these changes. */
  ChangesetID changeset_id = 0;
  /** Width of the buffer in pixels. */
  int buffer_width = 0;
  /** Height of the buffer in pixels. */
  int buffer_height = 0;
  /** Number of chunks along the x-axis. */
  int chunk_x_len = 0;
  /** Number of chunks along the y-axis. */
  int chunk_y_len = 0;

  void clear()
  {
    init_chunks(chunk_x_len, chunk_y_len);
  }

  /**
   * Update the chunk grid to match the buffer resolution.
   *
   * \returns true: resolution has been updated.
   *          false: resolution was unchanged.
   */
  bool update_resolution(const ImBuf *image_buffer)
  {
    if (buffer_width == image_buffer->x && buffer_height == image_buffer->y) {
      return false;
    }

    buffer_width = image_buffer->x;
    buffer_height = image_buffer->y;

    const int new_chunk_x_len = divide_ceil_u(buffer_width, CHUNK_SIZE);
    const int new_chunk_y_len = divide_ceil_u(buffer_height, CHUNK_SIZE);
    init_chunks(new_chunk_x_len, new_chunk_y_len);
    return true;
  }

  void mark_region(const rcti &updated_region)
  {
    int start_x_chunk = chunk_number_for_pixel(updated_region.xmin);
    int end_x_chunk = chunk_number_for_pixel(updated_region.xmax - 1);
    int start_y_chunk = chunk_number_for_pixel(updated_region.ymin);
    int end_y_chunk = chunk_number_for_pixel(updated_region.ymax - 1);

    /* Clamp to chunks inside the buffer. */
    start_x_chunk = max_ii(0, start_x_chunk);
    start_y_chunk = max_ii(0, start_y_chunk);
    end_x_chunk = min_ii(chunk_x_len - 1, end_x_chunk);
    end_y_chunk = min_ii(chunk_y_len - 1, end_y_chunk);

    /* Early exit when nothing overlaps the buffer. */
    if (start_x_chunk >= chunk_x_len || start_y_chunk >= chunk_y_len || end_x_chunk < 0 ||
        end_y_chunk < 0)
    {
      return;
    }

    mark_chunks_modified(start_x_chunk, start_y_chunk, end_x_chunk, end_y_chunk);
  }

  void mark_chunks_modified(int start_x_chunk, int start_y_chunk, int end_x_chunk, int end_y_chunk)
  {
    for (int chunk_y = start_y_chunk; chunk_y <= end_y_chunk; chunk_y++) {
      for (int chunk_x = start_x_chunk; chunk_x <= end_x_chunk; chunk_x++) {
        const int chunk_index = chunk_y * chunk_x_len + chunk_x;
        chunk_modified_flags_[chunk_index].set();
      }
    }
    has_modified_chunks_ = true;
  }

  bool has_modified_chunks() const
  {
    return has_modified_chunks_;
  }

  void init_chunks(int chunk_x_len_, int chunk_y_len_)
  {
    chunk_x_len = chunk_x_len_;
    chunk_y_len = chunk_y_len_;
    const int chunk_len = chunk_x_len * chunk_y_len;
    const int previous_chunk_len = chunk_modified_flags_.size();

    chunk_modified_flags_.resize(chunk_len);
    /* Fast exit: when the changeset was already empty there is nothing to reset. */
    if (!has_modified_chunks()) {
      return;
    }
    for (int index = 0; index < min_ii(chunk_len, previous_chunk_len); index++) {
      chunk_modified_flags_[index].reset();
    }
    has_modified_chunks_ = false;
  }

  /** Merge the given changeset into the receiver. */
  void merge(const Changeset &other)
  {
    BLI_assert(chunk_x_len == other.chunk_x_len);
    BLI_assert(chunk_y_len == other.chunk_y_len);
    const int chunk_len = chunk_x_len * chunk_y_len;

    for (int chunk_index = 0; chunk_index < chunk_len; chunk_index++) {
      chunk_modified_flags_[chunk_index].set(chunk_modified_flags_[chunk_index] ||
                                             other.chunk_modified_flags_[chunk_index]);
    }
    has_modified_chunks_ |= other.has_modified_chunks_;
  }

  /** Has the given chunk changed inside this changeset. */
  bool is_modified(int chunk_x, int chunk_y) const
  {
    const int chunk_index = chunk_y * chunk_x_len + chunk_x;
    return chunk_modified_flags_[chunk_index];
  }

  BitVector<> extract_modified_flags()
  {
    return std::move(chunk_modified_flags_);
  }
};

/** Global change ID tracking.
 *
 * This is global so that consumers can store only changeset IDs and detect changes even
 * as image buffers change. If they woudl store #ImBuf pointers, the same #ImBuf address
 * might get reused for another UDIM tile or pass. */
static uint64_t g_change_id_counter = 0;

/** Tracker of changes to an image buffer over time. */
struct Tracker {
  /** Changeset id of the first changeset kept in #history. */
  ChangesetID first_changeset_id = 0;
  /** Changeset id of the top changeset kept in #history. */
  ChangesetID last_changeset_id = 0;
  /** Changeset id at which the buffer resolution last changed. */
  ChangesetID last_resize_changeset_id = 0;
  /** Buffer resolution the tracker last observed. */
  int buffer_width = 0, buffer_height = 0;

  /** History of changesets. */
  Vector<Changeset> history;
  /** The current changeset. New changes will be added to this changeset. */
  Changeset current_changeset;

  Tracker(const ChangesetID change_id)
      : first_changeset_id(change_id), last_changeset_id(change_id)
  {
  }

  void update_resolution(const ImBuf *image_buffer)
  {
    current_changeset.update_resolution(image_buffer);

    if (buffer_width == image_buffer->x && buffer_height == image_buffer->y) {
      return;
    }

    const bool had_resolution = buffer_width != 0 || buffer_height != 0;
    buffer_width = image_buffer->x;
    buffer_height = image_buffer->y;

    if (had_resolution) {
      mark_full_update();
      last_resize_changeset_id = last_changeset_id;
    }
  }

  void mark_full_update()
  {
    history.clear();
    current_changeset.clear();
    last_changeset_id = IMB_partial_update_changeset_id_next();
    first_changeset_id = last_changeset_id;
  }

  void mark_region(const rcti &updated_region)
  {
    current_changeset.mark_region(updated_region);
  }

  void ensure_empty_changeset()
  {
    if (!current_changeset.has_modified_chunks()) {
      /* No need to create a new changeset when previous changeset does not contain any
       * modified chunks. */
      return;
    }
    commit_current_changeset();
    limit_history();
  }

  /** \brief Move the current changeset to the history and resets the current changeset. */
  void commit_current_changeset()
  {
    last_changeset_id = IMB_partial_update_changeset_id_next();
    current_changeset.changeset_id = last_changeset_id;
    history.append_as(std::move(current_changeset));
    current_changeset = Changeset();
  }

  /** Limit the number of changesets kept in history. */
  void limit_history()
  {
    const int num_items_to_remove = max_ii(history.size() - MAX_HISTORY_LEN, 0);
    if (num_items_to_remove == 0) {
      return;
    }
    /* Changes up to and including the last removed changeset can no longer be reconstructed. */
    first_changeset_id = history[num_items_to_remove - 1].changeset_id;
    history.remove(0, num_items_to_remove);
  }

  /**
   * Collect all historic changes since a given changeset.
   */
  std::optional<Changeset> changed_chunks_since(const ChangesetID from_changeset)
  {
    std::optional<Changeset> changed_chunks = std::nullopt;
    for (const Changeset &changeset : history) {
      if (changeset.changeset_id <= from_changeset || !changeset.has_modified_chunks()) {
        continue;
      }
      if (!changed_chunks.has_value()) {
        changed_chunks = std::make_optional<Changeset>();
        changed_chunks->init_chunks(changeset.chunk_x_len, changeset.chunk_y_len);
      }
      changed_chunks->merge(changeset);
    }
    return changed_chunks;
  }
};

void Changes::set_all_chunks_modified()
{
  chunk_x_len = divide_ceil_u(buffer_width, CHUNK_SIZE);
  chunk_y_len = divide_ceil_u(buffer_height, CHUNK_SIZE);
  modified_chunks.resize(int64_t(chunk_x_len) * chunk_y_len);
  modified_chunks.fill(true);
}

Vector<rcti> Changes::modified_regions() const
{
  BLI_assert(modified_chunks.size() == int64_t(chunk_x_len) * chunk_y_len);

  Vector<rcti> regions;
  BitVector<> remaining = modified_chunks;

  for (int start_y = 0; start_y < chunk_y_len; start_y++) {
    for (int start_x = 0; start_x < chunk_x_len;) {
      if (!remaining[start_y * chunk_x_len + start_x]) {
        start_x++;
        continue;
      }

      /* Extend right. */
      int end_x = start_x;
      while (end_x < chunk_x_len && remaining[start_y * chunk_x_len + end_x]) {
        end_x++;
      }

      /* Extend up. */
      int end_y = start_y + 1;
      while (end_y < chunk_y_len) {
        bool row_matches = true;
        for (int x = start_x; x < end_x; x++) {
          if (!remaining[end_y * chunk_x_len + x]) {
            row_matches = false;
            break;
          }
        }
        if (!row_matches) {
          break;
        }
        end_y++;
      }

      /* Consume all chunks covered by the region. */
      for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
          remaining[y * chunk_x_len + x].reset();
        }
      }

      rcti region;
      BLI_rcti_init(&region,
                    start_x * CHUNK_SIZE,
                    min_ii(end_x * CHUNK_SIZE, buffer_width),
                    start_y * CHUNK_SIZE,
                    min_ii(end_y * CHUNK_SIZE, buffer_height));
      regions.append(region);

      start_x = end_x;
    }
  }
  return regions;
}

static Tracker &tracker_ensure(ImBuf *ibuf)
{
  if (ibuf->partial_update == nullptr) {
    /* Allocate new changeset ID for new tracker, needed to disambiguate two ImBufs that happened
     * to be allocated at the same memory address.
     *
     * The GPU changeset ID is used if there is already a GPU texture, so GPU texture consumers
     * do not get unnecessarily updated. */
    const int64_t gpu_changeset_id = ibuf->gpu.partial_update_changeset;
    const int64_t changeset_id = gpu_changeset_id >= 0 ? gpu_changeset_id :
                                                         IMB_partial_update_changeset_id_next();
    ibuf->partial_update = MEM_new<Tracker>(__func__, changeset_id);
  }
  return *ibuf->partial_update;
}

}  // namespace imbuf::partial_update

using namespace imbuf::partial_update;

/**
 * Check if update tracking is needed, when either there is a tracker or a
 * GPU texture exists. To avoid unnecessary memory/performance overhead.
 */
static bool imb_track_updates(const ImBuf *ibuf)
{
  return ibuf->partial_update != nullptr || ibuf->gpu.texture != nullptr;
}

int64_t IMB_partial_update_changeset_id_next()
{
  return int64_t(atomic_add_and_fetch_uint64(&g_change_id_counter, 1));
}

int64_t IMB_partial_update_changeset_id_current()
{
  return int64_t(atomic_load_uint64(&g_change_id_counter));
}

void IMB_partial_update_mark_region(ImBuf *ibuf, const rcti &region)
{
  if (!imb_track_updates(ibuf)) {
    return;
  }
  std::scoped_lock lock(ibuf->partial_update_mutex);
  Tracker &tracker = tracker_ensure(ibuf);
  tracker.update_resolution(ibuf);
  tracker.mark_region(region);
}

void IMB_partial_update_mark_full(ImBuf *ibuf)
{
  if (!imb_track_updates(ibuf)) {
    return;
  }
  std::scoped_lock lock(ibuf->partial_update_mutex);
  Tracker &tracker = tracker_ensure(ibuf);
  tracker.mark_full_update();
}

void IMB_mark_dirty(ImBuf *ibuf)
{
  ibuf->userflags |= IB_BITMAPDIRTY;
}

void IMB_partial_update_flush(ImBuf *ibuf)
{
  std::scoped_lock lock(ibuf->partial_update_mutex);
  Tracker &tracker = tracker_ensure(ibuf);
  tracker.update_resolution(ibuf);
  tracker.ensure_empty_changeset();
}

Changes IMB_partial_update_collect(ImBuf *ibuf, const int64_t last_changeset_id)
{
  std::scoped_lock lock(ibuf->partial_update_mutex);
  Tracker &tracker = tracker_ensure(ibuf);

  Changes changes;
  changes.buffer_width = ibuf->x;
  changes.buffer_height = ibuf->y;

  if (last_changeset_id < tracker.first_changeset_id) {
    const bool resized = tracker.last_resize_changeset_id != 0 &&
                         last_changeset_id < tracker.last_resize_changeset_id;
    changes.kind = resized ? Changes::Kind::Resized : Changes::Kind::Full;
    return changes;
  }

  if (last_changeset_id >= tracker.last_changeset_id) {
    changes.kind = Changes::Kind::None;
    return changes;
  }

  std::optional<Changeset> changed = tracker.changed_chunks_since(last_changeset_id);
  if (!changed.has_value() || !changed->has_modified_chunks()) {
    changes.kind = Changes::Kind::None;
    return changes;
  }

  changes.kind = Changes::Kind::Partial;
  changes.chunk_x_len = changed->chunk_x_len;
  changes.chunk_y_len = changed->chunk_y_len;
  changes.modified_chunks = changed->extract_modified_flags();
  return changes;
}

void IMB_partial_update_free(ImBuf *ibuf)
{
  if (ibuf->partial_update != nullptr) {
    MEM_delete(ibuf->partial_update);
    ibuf->partial_update = nullptr;
  }
}

}  // namespace blender
