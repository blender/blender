/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fcntl.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif
#include <fmt/format.h>
#include <mutex>
#include <xxhash.h>

#include "BKE_id_hash.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"

#include "BLI_fileops.hh"
#include "BLI_mmap.h"
#include "BLI_mutex.hh"
#include "BLI_set.hh"

namespace blender::bke::id_hash {

static std::optional<Vector<char>> read_file(const StringRefNull path)
{
  blender::fstream stream{path.c_str(), std::ios_base::in | std::ios_base::binary};
  stream.seekg(0, std::ios_base::end);
  const int64_t size = stream.tellg();
  stream.seekg(0, std::ios_base::beg);

  blender::Vector<char> buffer(size);
  stream.read(buffer.data(), size);
  if (stream.bad()) {
    return std::nullopt;
  }

  return buffer;
}

static std::optional<XXH128_hash_t> compute_file_hash_with_file_read(const StringRefNull path)
{
  const std::optional<Vector<char>> buffer = read_file(path);
  if (!buffer) {
    return std::nullopt;
  }
  return XXH3_128bits(buffer->data(), buffer->size());
}

static std::optional<XXH128_hash_t> compute_file_hash_with_memory_map(const StringRefNull path)
{
  const int file = BLI_open(path.c_str(), O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return std::nullopt;
  }
  BLI_SCOPED_DEFER([&]() { close(file); });

  BLI_mmap_file *mmap_file = BLI_mmap_open(file);
  if (!mmap_file) {
    return std::nullopt;
  }
  BLI_SCOPED_DEFER([&]() { BLI_mmap_free(mmap_file); });
  const size_t size = BLI_mmap_get_length(mmap_file);
  const void *data = BLI_mmap_get_pointer(mmap_file);
  const XXH128_hash_t hash = XXH3_128bits(data, size);
  if (BLI_mmap_any_io_error(mmap_file)) {
    return std::nullopt;
  }
  return hash;
}

static std::optional<XXH128_hash_t> compute_file_hash(const StringRefNull path)
{
  /* First try the memory map the file, because it avoids an extra copy. */
  if (const std::optional<XXH128_hash_t> hash = compute_file_hash_with_memory_map(path)) {
    /* Make sure both code paths are tested even if memory mapping should almost always work. */
    BLI_assert(hash->low64 == compute_file_hash_with_file_read(path)->low64);
    return hash;
  }
  if (const std::optional<XXH128_hash_t> hash = compute_file_hash_with_file_read(path)) {
    return hash;
  }
  return std::nullopt;
}

struct CachedFileHash {
  int64_t last_modified = 0;
  XXH128_hash_t hash;
};

static std::optional<XXH128_hash_t> get_source_file_hash(const ID &id, DeepHashErrors &r_errors)
{
  static Map<std::string, CachedFileHash> cache;
  static Mutex mutex;

  const StringRefNull path = id.lib->runtime->filepath_abs;

  BLI_stat_t stat;
  if (BLI_stat(path.c_str(), &stat) == -1) {
    r_errors.missing_files.add_as(path);
    return std::nullopt;
  }

  std::lock_guard lock(mutex);
  if (const CachedFileHash *cached_hash = cache.lookup_ptr_as(path)) {
    if (cached_hash->last_modified == stat.st_mtime) {
      return cached_hash->hash;
    }
  }

  /* The modification time may not be set if the data-block is added as linked data as part of
   * versioning (e.g. in #do_versions_after_setup). */
  if (id.runtime->src_blend_modifification_time != 0) {
    if (stat.st_mtime != id.runtime->src_blend_modifification_time) {
      r_errors.updated_files.add_as(path);
      return std::nullopt;
    }
  }

  if (const std::optional<XXH128_hash_t> hash = compute_file_hash(path)) {
    cache.add_overwrite(path, CachedFileHash{stat.st_mtime, *hash});
    return hash;
  }
  r_errors.missing_files.add_as(path);
  return std::nullopt;
}

static std::optional<XXH128_hash_t> get_id_shallow_hash(const ID &id, DeepHashErrors &r_errors)
{
  BLI_assert(ID_IS_LINKED(&id));
  const StringRefNull id_name = id.name;
  const std::optional<XXH128_hash_t> file_hash = get_source_file_hash(id, r_errors);
  if (!file_hash) {
    return std::nullopt;
  }

  XXH3_state_t *hash_state = XXH3_createState();
  XXH3_128bits_reset(hash_state);
  XXH3_128bits_update(hash_state, id_name.data(), id_name.size());
  XXH3_128bits_update(hash_state, &*file_hash, sizeof(XXH128_hash_t));
  XXH128_hash_t shallow_hash = XXH3_128bits_digest(hash_state);
  XXH3_freeState(hash_state);
  return shallow_hash;
}

static void compute_deep_hash_recursive(const Main &bmain,
                                        const ID &id,
                                        Set<const ID *> &current_stack,
                                        Map<const ID *, IDHash> &r_hashes,
                                        DeepHashErrors &r_errors)
{
  if (r_hashes.contains(&id)) {
    return;
  }
  if (!id.deep_hash.is_null()) {
    r_hashes.add(&id, id.deep_hash);
    return;
  }
  current_stack.add(&id);
  BLI_SCOPED_DEFER([&]() -> void { current_stack.remove(&id); });
  const std::optional<XXH128_hash_t> id_shallow_hash = get_id_shallow_hash(id, r_errors);
  if (!id_shallow_hash) {
    return;
  }

  XXH3_state_t *hash_state = XXH3_createState();
  BLI_SCOPED_DEFER([&hash_state]() -> void { XXH3_freeState(hash_state); })
  XXH3_128bits_reset(hash_state);
  XXH3_128bits_update(hash_state, &*id_shallow_hash, sizeof(XXH128_hash_t));

  bool success = true;
  BKE_library_foreach_ID_link(
      const_cast<Main *>(&bmain),
      const_cast<ID *>(&id),
      [&](LibraryIDLinkCallbackData *cb_data) {
        if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
          /* Loopback pointer (e.g. from a shapekey to its owner geometry ID, or from a collection
           * to its parents) should always be ignored, as they do not represent an actual
           * dependency. The dependency relationship should already have been processed from the
           * owner to its dependency anyway (if applicable). */
          return IDWALK_RET_NOP;
        }
        if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
          /* Embedded data are part of their owner's internal data, and as such already computed as
           * part of the owner's shallow hash. */
          return IDWALK_RET_NOP;
        }
        if (cb_data->cb_flag & IDWALK_CB_HASH_IGNORE) {
          /* This pointer is explicitly ignored for the hash computation. */
          return IDWALK_RET_NOP;
        }
        ID *referenced_id = *cb_data->id_pointer;
        if (!referenced_id) {
          /* Need to update the hash even if there is no id. There is a difference between the case
           * where there is no id and the case where this callback is not called at all.*/
          const int random_data = 452942579;
          XXH3_128bits_update(hash_state, &random_data, sizeof(int));
          return IDWALK_RET_NOP;
        }
        /* All embedded ID usages should already have been excluded above. */
        BLI_assert((referenced_id->flag & ID_FLAG_EMBEDDED_DATA) == 0);
        if (current_stack.contains(referenced_id)) {
          /* Somehow encode that we had a circular reference here. */
          const int random_data = 234632342;
          XXH3_128bits_update(hash_state, &random_data, sizeof(int));
          return IDWALK_RET_NOP;
        }
        compute_deep_hash_recursive(bmain, *referenced_id, current_stack, r_hashes, r_errors);
        const IDHash *referenced_id_hash = r_hashes.lookup_ptr(referenced_id);
        if (!referenced_id_hash) {
          success = false;
          return IDWALK_RET_STOP_ITER;
        }
        XXH3_128bits_update(hash_state, referenced_id_hash->data, sizeof(IDHash));
        return IDWALK_RET_NOP;
      },
      nullptr,
      IDWALK_READONLY);

  if (!success) {
    return;
  }
  IDHash new_deep_hash;
  const XXH128_hash_t new_deep_hash_xxh128 = XXH3_128bits_digest(hash_state);
  static_assert(sizeof(IDHash) == sizeof(XXH128_hash_t));
  memcpy(new_deep_hash.data, &new_deep_hash_xxh128, sizeof(IDHash));
  r_hashes.add(&id, new_deep_hash);
}

IDHashResult compute_linked_id_deep_hashes(const Main &bmain, Span<const ID *> ids)
{
#ifndef NDEBUG
  for (const ID *id : ids) {
    BLI_assert(ID_IS_LINKED(id));
  }
#endif

  if (ids.is_empty()) {
    return ValidDeepHashes{};
  }

  Map<const ID *, IDHash> hashes;
  Set<const ID *> current_stack;
  DeepHashErrors errors;
  for (const ID *id : ids) {
    compute_deep_hash_recursive(bmain, *id, current_stack, hashes, errors);
  }
  if (!errors.missing_files.is_empty() || !errors.updated_files.is_empty()) {
    return errors;
  }
  return ValidDeepHashes{hashes};
}

std::string id_hash_to_hex(const IDHash &hash)
{
  std::string hex_str;
  for (const uint8_t byte : hash.data) {
    hex_str += fmt::format("{:02x}", byte);
  }
  return hex_str;
}

}  // namespace blender::bke::id_hash
