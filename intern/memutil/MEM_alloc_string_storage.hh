/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup intern_memutil
 *
 * Implement a static storage for complex, non-static allocation strings passed MEM_guardedalloc
 * functions.
 */

#include <any>
#include <cassert>
#include <string>
#include <unordered_map>

namespace intern::memutil {

/**
 * A 'static' storage of allocation strings, with a simple API to set and retrieve them.
 *
 * This is a templated wrapper around a std::unordered_map, to allow custom key types.
 */
template<typename keyT, template<typename> typename hashT> class AllocStringStorage {
  std::unordered_map<keyT, std::string, hashT<keyT>> storage_;

 public:
  /**
   * Check whether the given key exists in the storage.
   *
   * \return `true` if the \a key is found in storage, false otherwise.
   */
  bool contains(const keyT &key)
  {
    return storage_.count(key) != 0;
  }

  /**
   * Return the alloc string for the given key in the storage.
   *
   * \return A pointer to the stored string if \a key is found, `nullptr` otherwise.
   */
  const char *find(const keyT &key)
  {
    if (storage_.count(key) != 0) {
      return storage_[key].c_str();
    }
    return nullptr;
  }

  /**
   * Insert the given alloc string in the storage, at the given key, and return a pointer
   * to the stored string.
   *
   * \param alloc_string: The alloc string to store at \a key.
   * \return A pointer to the inserted stored string.
   */
  const char *insert(const keyT &key, std::string alloc_string)
  {
#ifndef NDEBUG
    assert(storage_.count(key) == 0);
#endif
    return (storage_[key] = std::move(alloc_string)).c_str();
  }
};

namespace internal {

/**
 * The main container for all #AllocStringStorage.
 */
class AllocStringStorageContainer {
  std::unordered_map<std::string, std::any> storage_;

 public:
  /**
   * Create if necessary, and return the #AllocStringStorage for the given \a storage_identifier.
   *
   * The template arguments allow to define the type of key used for the mapping to allocation
   * strings.
   */
  template<typename keyT, template<typename> typename hashT>
  std::any &ensure_storage(const std::string &storage_identifier)
  {
    if (storage_.count(storage_identifier) == 0) {
      AllocStringStorage<keyT, hashT> storage_for_identifier;
      return (storage_[storage_identifier] = std::make_any<AllocStringStorage<keyT, hashT>>(
                  std::move(storage_for_identifier)));
    }
    return storage_[storage_identifier];
  }
};

/**
 * Ensure that the static AllocStringStorageContainer is defined and created, and return a
 * reference to it.
 */
AllocStringStorageContainer &ensure_storage_container();

}  // namespace internal

/**
 * Return a reference to the AllocStringStorage static data matching the given \a
 * storage_identifier, creating it if needed.
 *
 * \note The storage is `thread_local` data, so access to it is thread-safe as long as it is not
 * shared between threads by the user code.
 */
template<typename keyT, template<typename> typename hashT>
AllocStringStorage<keyT, hashT> &alloc_string_storage_get(const std::string &storage_identifier)
{
  internal::AllocStringStorageContainer &storage_container = internal::ensure_storage_container();
  std::any &storage = storage_container.ensure_storage<keyT, hashT>(storage_identifier);
  return std::any_cast<AllocStringStorage<keyT, hashT> &>(storage);
}

}  // namespace intern::memutil
