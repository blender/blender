/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_AUDASPACE)

#  include <memory>

#  include "BLI_utility_mixins.hh"

namespace aud {
class IReader;
class ISound;
}  // namespace aud

namespace blender::bke {

struct SoundReaderCacheState;

class SoundReaderCache;

/** Exclusive access to a sound reader. The reader returns to its cache on destruction. */
class SoundReaderLease : NonCopyable {
 private:
  /**
   * Keep the shared cache state alive so a lease can return its reader after the outer cache has
   * been replaced or destroyed.
   */
  std::shared_ptr<SoundReaderCacheState> state_;
  std::shared_ptr<aud::IReader> reader_;

  SoundReaderLease(std::shared_ptr<SoundReaderCacheState> state,
                   std::shared_ptr<aud::IReader> reader);
  void release() noexcept;

  friend SoundReaderCache;

 public:
  SoundReaderLease() = default;
  ~SoundReaderLease();

  SoundReaderLease(SoundReaderLease &&other) noexcept;
  SoundReaderLease &operator=(SoundReaderLease &&other) noexcept;

  explicit operator bool() const;
  aud::IReader *operator->() const;

  /**
   * Destroy the reader instead of returning it to the cache.
   *
   * This is used when a failed reader operation may have left its state invalid.
   */
  void discard() noexcept;
};

/**
 * Runtime-only reusable reader state associated with a #bSound runtime.
 *
 * Active readers belong to leases, not the idle-reader vector.
 */
class SoundReaderCache : NonCopyable {
 private:
  std::shared_ptr<SoundReaderCacheState> state_;

 public:
  explicit SoundReaderCache(std::shared_ptr<aud::ISound> sound);

  /** Acquire a reader. Returns an empty lease when reader creation returns null. */
  SoundReaderLease acquire();
};

}  // namespace blender::bke

#endif
