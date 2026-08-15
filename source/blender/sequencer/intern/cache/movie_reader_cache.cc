/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "movie_reader_cache.hh"

#include <atomic>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_mutex.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_vector.hh"

#include "BKE_main.hh"

#include "MEM_guardedalloc.h"

#include "SEQ_sequencer.hh"

#include "IMB_imbuf.hh"

#include "MOV_read.hh"

#include "multiview.hh"
#include "proxy.hh"

namespace blender::seq {

/* Give each top-level render a unique timestamp. Keep the active timestamp thread-local so nested
 * scene renders inherit it without concurrent renders (e.g. prefetch) changing it. */
static std::atomic<uint64_t> next_timestamp = 0;
static thread_local uint64_t current_timestamp = 0;

struct MovieReaderKey {
  std::string source_filepath;
  std::string multiview_filepath;
  std::string colorspace;
  std::string proxy_dir;
  std::string multiview_suffix;
  ImBufFlags flags = ImBufFlags::Zero;
  int stream_index = 0;

  bool operator==(const MovieReaderKey &other) const = default;
};

struct MovieReaderCacheEntry {
  MovieReaderKey key;
  MovieReader *reader = nullptr;
  int frame_index = -1;
  uint64_t timestamp = 0;
  bool is_accessed = false;
  bool invalidated = false;

  ~MovieReaderCacheEntry()
  {
    MOV_close(reader);
  }
};

struct MovieReaderCache {
  static constexpr int64_t max_entries = 8;
  static constexpr uint64_t stale_after_timestamps = 8;
  static constexpr int backward_seek_penalty = 32;

  Mutex mutex_;
  Vector<std::unique_ptr<MovieReaderCacheEntry>> entries_;
  uint64_t timestamp_counter_ = 0;

  MovieReaderCache() = default;
  ~MovieReaderCache() = default;

  MovieReaderAccessor acquire(const MovieReaderKey &key, int frame_index, uint64_t timestamp);
  MovieReaderAccessor acquire_any(const MovieReaderKey &key);
  void invalidate(const std::string &source_filepath);
  void clear();
  void release(MovieReaderCacheEntry &entry);
  void reader_open(MovieReaderCacheEntry &entry);
  bool reader_ensure_initialized(MovieReaderCacheEntry &entry);
  bool remove_oldest_stale_entry();
  void remove_excess_entries();
};

bool MovieReaderCache::remove_oldest_stale_entry()
{
  int64_t oldest_index = -1;
  for (const int64_t i : entries_.index_range()) {
    const MovieReaderCacheEntry &entry = *entries_[i];
    const uint64_t age = timestamp_counter_ > entry.timestamp ?
                             timestamp_counter_ - entry.timestamp :
                             0;
    if (entry.is_accessed || age < stale_after_timestamps) {
      continue;
    }
    if (oldest_index == -1 || entry.timestamp < entries_[oldest_index]->timestamp) {
      oldest_index = i;
    }
  }
  if (oldest_index == -1) {
    return false;
  }
  entries_.remove(oldest_index);
  return true;
}

void MovieReaderCache::remove_excess_entries()
{
  while (entries_.size() > max_entries) {
    if (!remove_oldest_stale_entry()) {
      break;
    }
  }
}

static void movie_reader_source_filepath_get(const Scene &scene,
                                             const Strip &strip,
                                             char r_filepath[FILE_MAX])
{
  BLI_path_join(r_filepath, FILE_MAX, strip.data->dirpath, strip.data->stripdata->filename);
  BLI_path_abs(r_filepath, ID_BLEND_PATH_FROM_GLOBAL(&scene.id));
}

static MovieReaderKey movie_reader_key_get(const Scene &scene,
                                           const Strip &strip,
                                           const std::optional<int> view_id)
{
  /* Readers are shared by source and decode settings, independently of the strip using them. */
  MovieReaderKey key;
  char filepath[FILE_MAX];
  movie_reader_source_filepath_get(scene, strip, filepath);
  key.source_filepath = filepath;

  const bool use_multiview = view_id.has_value() && (strip.flag & SEQ_USE_VIEWS) != 0 &&
                             (scene.r.scemode & R_MULTIVIEW) != 0;
  if (use_multiview && strip.views_format == R_IMF_VIEWS_INDIVIDUAL) {
    char filepath_view[FILE_MAX];
    const char *suffix = nullptr;
    if (seq_multiview_view_filepath_get(
            scene, filepath, *view_id, filepath_view, sizeof(filepath_view), &suffix))
    {
      key.multiview_filepath = filepath_view;
      key.multiview_suffix = suffix;
    }
  }

  key.flags = (strip.flag & SEQ_DEINTERLACE) ? ImBufFlags::Deinterlace : ImBufFlags::Zero;
  key.stream_index = strip.streamindex;
  key.colorspace = strip.data->colorspace_settings.name;

  char proxy_dir[FILE_MAX];
  if (seq_proxy_get_custom_dir(*scene.ed, strip, proxy_dir)) {
    key.proxy_dir = proxy_dir;
  }
  return key;
}

MovieReaderAccessor MovieReaderCache::acquire(const MovieReaderKey &key,
                                              const int frame_index,
                                              const uint64_t timestamp)
{
  /* An entry can be used only once per rendered frame. Penalize backward seeks, as decoding
   * forwards is generally cheaper. */
  std::unique_lock lock(mutex_);
  if (timestamp_counter_ != timestamp) {
    timestamp_counter_ = timestamp;
    remove_excess_entries();
  }
  MovieReaderCacheEntry *best = nullptr;
  int best_score = std::numeric_limits<int>::max();

  for (const std::unique_ptr<MovieReaderCacheEntry> &candidate : entries_) {
    if (candidate->is_accessed || candidate->invalidated ||
        candidate->timestamp == timestamp_counter_ || !(candidate->key == key))
    {
      continue;
    }
    const int distance = frame_index - candidate->frame_index;
    const int score = distance >= 0 ? distance : -distance * backward_seek_penalty;
    if (score < best_score) {
      best = candidate.get();
      best_score = score;
    }
  }

  if (best == nullptr) {
    if (entries_.size() >= max_entries) {
      remove_oldest_stale_entry();
    }
    auto entry = std::make_unique<MovieReaderCacheEntry>();
    entry->key = key;
    best = entry.get();
    entries_.append(std::move(entry));
  }

  best->is_accessed = true;
  best->timestamp = timestamp_counter_;
  lock.unlock();

  reader_open(*best);
  return MovieReaderAccessor(this, best);
}

MovieReaderAccessor MovieReaderCache::acquire_any(const MovieReaderKey &key)
{
  /* Property queries do not seek, so any free matching reader is suitable even if it was used by
   * another strip during the current rendered frame. Prefer an initialized reader, then the most
   * recently used one. Do not evict unrelated readers here; render-frame acquisition handles
   * stale-entry cleanup. */
  std::unique_lock lock(mutex_);
  MovieReaderCacheEntry *best = nullptr;
  bool best_is_initialized = false;
  for (const std::unique_ptr<MovieReaderCacheEntry> &candidate : entries_) {
    if (candidate->is_accessed || candidate->invalidated || !(candidate->key == key)) {
      continue;
    }
    const bool candidate_is_initialized = candidate->reader != nullptr &&
                                          MOV_is_initialized_and_valid(candidate->reader);
    if (best == nullptr || (candidate_is_initialized && !best_is_initialized) ||
        (candidate_is_initialized == best_is_initialized &&
         candidate->timestamp > best->timestamp))
    {
      best = candidate.get();
      best_is_initialized = candidate_is_initialized;
    }
  }

  if (best == nullptr) {
    auto entry = std::make_unique<MovieReaderCacheEntry>();
    entry->key = key;
    best = entry.get();
    entries_.append(std::move(entry));
  }

  best->is_accessed = true;
  /* This access does not decode a rendered frame, so do not reserve the reader for the current
   * render timestamp. */
  lock.unlock();

  reader_open(*best);
  if (!reader_ensure_initialized(*best)) {
    MOV_close(best->reader);
    best->reader = nullptr;
    std::lock_guard failure_lock(mutex_);
    best->invalidated = true;
  }
  return MovieReaderAccessor(this, best);
}

void MovieReaderCache::reader_open(MovieReaderCacheEntry &entry)
{
  if (entry.reader != nullptr) {
    return;
  }

  const MovieReaderKey &key = entry.key;
  char colorspace[IM_MAX_SPACE];
  STRNCPY(colorspace, key.colorspace.c_str());
  const std::string &filepath = key.multiview_filepath.empty() ? key.source_filepath :
                                                                 key.multiview_filepath;
  entry.reader = MOV_open_file(filepath.c_str(), key.flags, key.stream_index, true, colorspace);
  if (entry.reader == nullptr) {
    return;
  }
  if (!key.proxy_dir.empty()) {
    seq_proxy_index_dir_set(entry.reader, key.proxy_dir.c_str());
  }
  if (!key.multiview_suffix.empty()) {
    MOV_set_multiview_suffix(entry.reader, key.multiview_suffix.c_str());
  }

  if (!key.multiview_filepath.empty() && !reader_ensure_initialized(entry)) {
    MOV_close(entry.reader);
    entry.reader = nullptr;
  }
}

bool MovieReaderCache::reader_ensure_initialized(MovieReaderCacheEntry &entry)
{
  if (entry.reader == nullptr) {
    return false;
  }
  if (MOV_is_initialized_and_valid(entry.reader)) {
    return true;
  }

  ImBuf *ibuf = MOV_decode_frame(entry.reader, 0, IMB_PROXY_NONE);
  if (ibuf == nullptr) {
    return false;
  }
  IMB_freeImBuf(ibuf);
  entry.frame_index = 0;
  return MOV_is_initialized_and_valid(entry.reader);
}

void MovieReaderCache::release(MovieReaderCacheEntry &entry)
{
  std::lock_guard lock(mutex_);
  entry.is_accessed = false;
  if (entry.invalidated) {
    for (const int64_t i : entries_.index_range()) {
      if (entries_[i].get() == &entry) {
        entries_.remove(i);
        break;
      }
    }
  }
}

void MovieReaderCache::invalidate(const std::string &source_filepath)
{
  std::lock_guard lock(mutex_);
  for (int64_t i = entries_.size(); i-- > 0;) {
    MovieReaderCacheEntry &entry = *entries_[i];
    if (entry.key.source_filepath != source_filepath) {
      continue;
    }
    if (entry.is_accessed) {
      entry.invalidated = true;
    }
    else {
      entries_.remove(i);
    }
  }
}

void MovieReaderCache::clear()
{
  std::lock_guard lock(mutex_);
  for (int64_t i = entries_.size(); i-- > 0;) {
    MovieReaderCacheEntry &entry = *entries_[i];
    if (entry.is_accessed) {
      entry.invalidated = true;
    }
    else {
      entries_.remove(i);
    }
  }
}

MovieReaderAccessor::MovieReaderAccessor(MovieReaderCache *cache, MovieReaderCacheEntry *entry)
    : cache_(cache), entry_(entry)
{
}

MovieReaderAccessor::MovieReaderAccessor(MovieReaderAccessor &&other) noexcept
    : cache_(other.cache_), entry_(other.entry_)
{
  other.cache_ = nullptr;
  other.entry_ = nullptr;
}

MovieReaderAccessor &MovieReaderAccessor::operator=(MovieReaderAccessor &&other) noexcept
{
  if (this != &other) {
    release();
    cache_ = other.cache_;
    entry_ = other.entry_;
    other.cache_ = nullptr;
    other.entry_ = nullptr;
  }
  return *this;
}

MovieReaderAccessor::~MovieReaderAccessor()
{
  release();
}

void MovieReaderAccessor::release()
{
  if (cache_ != nullptr) {
    cache_->release(*entry_);
    cache_ = nullptr;
    entry_ = nullptr;
  }
}

MovieReaderAccessor::operator bool() const
{
  return entry_ != nullptr && entry_->reader != nullptr;
}

ImBuf *MovieReaderAccessor::decode_frame(const int frame_index, const IMB_Proxy_Size proxy_size)
{
  ImBuf *ibuf = MOV_decode_frame(entry_->reader, frame_index, proxy_size);
  if (ibuf != nullptr) {
    entry_->frame_index = frame_index;
  }
  return ibuf;
}

bool MovieReaderAccessor::uses_multiview_filepath() const
{
  return entry_ != nullptr && !entry_->key.multiview_filepath.empty();
}

MovieReader *MovieReaderAccessor::reader()
{
  return entry_ ? entry_->reader : nullptr;
}

const MovieReader *MovieReaderAccessor::reader() const
{
  return entry_ ? entry_->reader : nullptr;
}

MovieReaderAccessor movie_reader_cache_acquire(Scene &cache_scene,
                                               const Scene &key_scene,
                                               const Strip &strip,
                                               const int frame_index)
{
  return cache_scene.ed->runtime->movie_reader_cache->acquire(
      movie_reader_key_get(key_scene, strip, std::nullopt), frame_index, current_timestamp);
}

MovieReaderAccessor movie_reader_cache_acquire_view(Scene &cache_scene,
                                                    const Scene &key_scene,
                                                    const Strip &strip,
                                                    const int view_id,
                                                    const int frame_index)
{
  return cache_scene.ed->runtime->movie_reader_cache->acquire(
      movie_reader_key_get(key_scene, strip, view_id), frame_index, current_timestamp);
}

static MovieReaderAccessor movie_reader_cache_acquire_any_base(const Scene &scene,
                                                               const Strip &strip)
{
  return scene.ed->runtime->movie_reader_cache->acquire_any(
      movie_reader_key_get(scene, strip, std::nullopt));
}

static MovieReaderAccessor movie_reader_cache_acquire_any_view(const Scene &scene,
                                                               const Strip &strip,
                                                               const int view_id)
{
  return scene.ed->runtime->movie_reader_cache->acquire_any(
      movie_reader_key_get(scene, strip, view_id));
}

MovieReaderAccessor movie_reader_cache_acquire_any(const Scene &scene, const Strip &strip)
{
  MovieReaderAccessor reader = movie_reader_cache_acquire_any_view(scene, strip, 0);
  if (!reader && reader.uses_multiview_filepath()) {
    reader = {};
    return movie_reader_cache_acquire_any_base(scene, strip);
  }
  return reader;
}

bool movie_reader_cache_can_produce_frames(const Scene &scene, const Strip &strip)
{
  const bool use_multiview = (strip.flag & SEQ_USE_VIEWS) != 0 &&
                             (scene.r.scemode & R_MULTIVIEW) != 0;
  if (use_multiview && strip.views_format == R_IMF_VIEWS_INDIVIDUAL) {
    Vector<MovieReaderAccessor> readers;
    const int files_num = seq_multiview_num_files_get(&scene, strip.views_format);
    if (files_num <= 0) {
      return bool(movie_reader_cache_acquire_any_base(scene, strip));
    }

    MovieReaderAccessor primary_reader = movie_reader_cache_acquire_any_view(scene, strip, 0);
    if (!primary_reader.uses_multiview_filepath()) {
      return bool(primary_reader);
    }

    bool all_readers_open = bool(primary_reader);
    readers.append(std::move(primary_reader));
    for (int view_id = 1; view_id < files_num && all_readers_open; view_id++) {
      readers.append(movie_reader_cache_acquire_any_view(scene, strip, view_id));
      all_readers_open = bool(readers.last());
    }
    if (all_readers_open) {
      return true;
    }
  }
  return bool(movie_reader_cache_acquire_any_base(scene, strip));
}

MovieReaderCache *movie_reader_cache_create()
{
  return MEM_new<MovieReaderCache>(__func__);
}

void movie_reader_cache_destroy(MovieReaderCache *cache)
{
  MEM_delete(cache);
}

void movie_reader_cache_timestamp_bump()
{
  current_timestamp = next_timestamp.fetch_add(1, std::memory_order_relaxed) + 1;
}

void movie_reader_cache_invalidate(Scene &scene, const Strip &strip)
{
  char filepath[FILE_MAX];
  movie_reader_source_filepath_get(scene, strip, filepath);
  scene.ed->runtime->movie_reader_cache->invalidate(filepath);
}

void movie_reader_cache_clear(Scene &scene)
{
  scene.ed->runtime->movie_reader_cache->clear();
}

}  // namespace blender::seq
