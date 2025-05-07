/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "BLI_fileops.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_memory_cache_file_load.hh"
#include "BLI_mutex.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

namespace blender::memory_cache {

/**
 * A key used to identify data loaded from one or more files.
 */
class LoadFileKey : public GenericKey {
 private:
  /** The files to load from. */
  Vector<std::string> file_paths_;
  /**
   * The key used to identify the loader. The same files might be loaded with different loaders
   * which can result in different data that needs to be cached separately.
   */
  std::shared_ptr<const GenericKey> loader_key_;

 public:
  LoadFileKey(Vector<std::string> file_paths, std::shared_ptr<const GenericKey> loader_key)
      : file_paths_(std::move(file_paths)), loader_key_(std::move(loader_key))
  {
  }

  Span<std::string> file_paths() const
  {
    return file_paths_;
  }

  uint64_t hash() const override
  {
    return get_default_hash(file_paths_, *loader_key_);
  }

  friend bool operator==(const LoadFileKey &a, const LoadFileKey &b)
  {
    return a.file_paths_ == b.file_paths_ && *a.loader_key_ == *b.loader_key_;
  }

  bool equal_to(const GenericKey &other) const override
  {
    if (const auto *other_typed = dynamic_cast<const LoadFileKey *>(&other)) {
      return *this == *other_typed;
    }
    return false;
  }

  std::unique_ptr<GenericKey> to_storable() const override
  {
    /* Currently #LoadFileKey is always storable, i.e. it owns all the data it references. A
     * potential future optimization could be to support just referencing the paths and loader key,
     * but that causes some boilerplate now that is not worth it. */
    return std::make_unique<LoadFileKey>(*this);
  }
};

static std::optional<int64_t> get_file_modification_time(const StringRefNull path)
{
  BLI_stat_t stat;
  if (BLI_stat(path.c_str(), &stat) == -1) {
    return std::nullopt;
  }
  return stat.st_mtime;
}

struct FileStatMap {
  Mutex mutex;
  Map<std::string, std::optional<int64_t>> map;
};

static FileStatMap &get_file_stat_map()
{
  static FileStatMap file_stat_map;
  return file_stat_map;
}

static void invalidate_outdated_caches_if_necessary(const Span<StringRefNull> file_paths)
{
  FileStatMap &file_stat_map = get_file_stat_map();

  /* Retrieve the file modification times before the lock because there is no need for the lock
   * yet. While not guaranteed, retrieving the modification time is often optimized by the OS so
   * that no actual access to the hard drive is necessary. */
  Array<std::optional<int64_t>> new_times(file_paths.size());
  for (const int i : file_paths.index_range()) {
    new_times[i] = get_file_modification_time(file_paths[i]);
  }

  std::lock_guard lock{file_stat_map.mutex};

  /* Find all paths that have changed on disk. */
  VectorSet<StringRefNull> outdated_paths;
  for (const int i : file_paths.index_range()) {
    const StringRefNull path = file_paths[i];
    const std::optional<int64_t> new_time = new_times[i];
    std::optional<int64_t> &old_time = file_stat_map.map.lookup_or_add_as(path, new_time);
    if (old_time != new_time) {
      outdated_paths.add(path);
      old_time = new_time;
    }
  }
  /* If any referenced file was changed, invalidate the caches that use it. */
  if (!outdated_paths.is_empty()) {
    /* Isolate because a mutex is locked. */
    threading::isolate_task([&]() {
      /* Invalidation is done while the mutex is locked so that other threads won't see the old
       * cached value anymore after we've detected that it's outdated. */
      memory_cache::remove_if([&](const GenericKey &other_key) {
        if (const auto *other_key_typed = dynamic_cast<const LoadFileKey *>(&other_key)) {
          const Span<std::string> other_key_paths = other_key_typed->file_paths();
          return std::any_of(
              other_key_paths.begin(), other_key_paths.end(), [&](const StringRefNull path) {
                return outdated_paths.contains(path);
              });
        }
        return false;
      });
    });
  }
}

std::shared_ptr<CachedValue> get_loaded_base(const GenericKey &loader_key,
                                             Span<StringRefNull> file_paths,
                                             FunctionRef<std::unique_ptr<CachedValue>()> load_fn)
{
  invalidate_outdated_caches_if_necessary(file_paths);
  const LoadFileKey key{file_paths, loader_key.to_storable()};
  return memory_cache::get_base(key, load_fn);
}

}  // namespace blender::memory_cache
