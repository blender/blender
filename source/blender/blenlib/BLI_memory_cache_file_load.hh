/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_memory_cache.hh"
#include "BLI_string_ref.hh"

namespace blender::memory_cache {

/**
 * Call the given loader function if its result has not been cached yet. The cache key is a
 * combination of loader_key and file_paths. load_fn is responsible for still producing a valid
 * cache value even if a file is not found.
 */
template<typename T>
std::shared_ptr<const T> get_loaded(const GenericKey &loader_key,
                                    Span<StringRefNull> file_paths,
                                    FunctionRef<std::unique_ptr<T>()> load_fn);

std::shared_ptr<CachedValue> get_loaded_base(const GenericKey &loader_key,
                                             Span<StringRefNull> file_paths,
                                             FunctionRef<std::unique_ptr<CachedValue>()> load_fn);

template<typename T>
inline std::shared_ptr<const T> get_loaded(const GenericKey &loader_key,
                                           Span<StringRefNull> file_paths,
                                           FunctionRef<std::unique_ptr<T>()> load_fn)
{
  return std::dynamic_pointer_cast<const T>(get_loaded_base(loader_key, file_paths, load_fn));
}

}  // namespace blender::memory_cache
