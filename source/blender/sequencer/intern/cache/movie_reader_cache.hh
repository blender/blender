/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 *
 * Cache open movie readers for reuse across rendered frames.
 * - Readers are keyed by source file path and decode settings, not by strip. So neighboring
 *   strips that use the same media can use the same reader.
 * - A reader is reserved after use until the next top-level frame render, so multiple strips using
 *   the same media do not seek one reader back and forth within the same frame.
 * - From the available matching readers, prefer the one closest to the requested source frame,
 *   with a penalty for readers that need to seek backwards.
 * - Frame-independent queries can reuse any free matching reader. Querying an initialized reader
 *   does not change its decode position. Such queries may temporarily grow the cache beyond its
 *   soft size limit; stale entries are removed when subsequent frames are rendered.
 * - Cache size uses #MovieReaderCache::max_entries as a soft limit. Readers above it are removed
 *   after they have not been used for #MovieReaderCache::stale_after_timestamps render timestamps.
 */

#pragma once

#include <cstdint>

#include "IMB_imbuf_enums.h"

namespace blender {

struct ImBuf;
struct MovieReader;
struct Scene;
struct Strip;

namespace seq {

struct MovieReaderCache;
struct MovieReaderCacheEntry;

MovieReaderCache *movie_reader_cache_create();
void movie_reader_cache_destroy(MovieReaderCache *cache);
void movie_reader_cache_timestamp_bump();
void movie_reader_cache_invalidate(Scene &scene, const Strip &strip);
void movie_reader_cache_clear(Scene &scene);

/** Scoped exclusive access to a movie reader. Keep alive while decoding or querying the reader. */
class MovieReaderAccessor {
  MovieReaderCache *cache_ = nullptr;
  MovieReaderCacheEntry *entry_ = nullptr;

 public:
  MovieReaderAccessor() = default;
  MovieReaderAccessor(const MovieReaderAccessor &) = delete;
  MovieReaderAccessor(MovieReaderAccessor &&other) noexcept;

  ~MovieReaderAccessor();

  MovieReaderAccessor &operator=(const MovieReaderAccessor &) = delete;
  MovieReaderAccessor &operator=(MovieReaderAccessor &&other) noexcept;
  explicit operator bool() const;

  ImBuf *decode_frame(int frame_index, IMB_Proxy_Size proxy_size);
  bool uses_multiview_filepath() const;
  MovieReader *reader();
  const MovieReader *reader() const;

 private:
  friend struct MovieReaderCache;

  MovieReaderAccessor(MovieReaderCache *cache, MovieReaderCacheEntry *entry);
  void release();
};

/** Acquire from `cache_scene`, using `key_scene` and `strip` to build the reader key. */
MovieReaderAccessor movie_reader_cache_acquire(Scene &cache_scene,
                                               const Scene &key_scene,
                                               const Strip &strip,
                                               int frame_index);
MovieReaderAccessor movie_reader_cache_acquire_view(
    Scene &cache_scene, const Scene &key_scene, const Strip &strip, int view_id, int frame_index);
/** Acquire an initialized free primary reader, falling back to the strip's base filepath. */
MovieReaderAccessor movie_reader_cache_acquire_any(const Scene &scene, const Strip &strip);
/** Return whether all required movie readers can be opened. */
bool movie_reader_cache_can_produce_frames(const Scene &scene, const Strip &strip);

}  // namespace seq
}  // namespace blender
