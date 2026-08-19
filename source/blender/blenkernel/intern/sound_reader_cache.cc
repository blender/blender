/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "sound_reader_cache.hh"

#if defined(WITH_AUDASPACE)

#  include <utility>

#  include "BLI_mutex.hh"
#  include "BLI_vector.hh"

#  include <IReader.h>
#  include <ISound.h>

namespace blender::bke {

struct SoundReaderCacheState {
  static constexpr int max_idle_readers = 2;

  explicit SoundReaderCacheState(std::shared_ptr<aud::ISound> sound) : sound(std::move(sound)) {}

  std::shared_ptr<aud::ISound> sound;
  Mutex mutex;
  Vector<std::shared_ptr<aud::IReader>, max_idle_readers> idle_readers;

  void release(std::shared_ptr<aud::IReader> reader) noexcept
  {
    if (!reader) {
      return;
    }
    std::lock_guard lock{this->mutex};
    if (this->idle_readers.size() < max_idle_readers) {
      this->idle_readers.append(std::move(reader));
    }
  }
};

SoundReaderLease::SoundReaderLease(std::shared_ptr<SoundReaderCacheState> state,
                                   std::shared_ptr<aud::IReader> reader)
    : state_(std::move(state)), reader_(std::move(reader))
{
}

SoundReaderLease::~SoundReaderLease()
{
  this->release();
}

SoundReaderLease::SoundReaderLease(SoundReaderLease &&other) noexcept
    : state_(std::move(other.state_)), reader_(std::move(other.reader_))
{
}

SoundReaderLease &SoundReaderLease::operator=(SoundReaderLease &&other) noexcept
{
  if (this != &other) {
    this->release();
    this->state_ = std::move(other.state_);
    this->reader_ = std::move(other.reader_);
  }
  return *this;
}

SoundReaderLease::operator bool() const
{
  return bool(this->reader_);
}

aud::IReader *SoundReaderLease::operator->() const
{
  return this->reader_.get();
}

void SoundReaderLease::discard() noexcept
{
  this->reader_.reset();
  this->state_.reset();
}

void SoundReaderLease::release() noexcept
{
  if (this->state_) {
    this->state_->release(std::move(this->reader_));
    this->state_.reset();
  }
}

SoundReaderCache::SoundReaderCache(std::shared_ptr<aud::ISound> sound)
    : state_(std::make_shared<SoundReaderCacheState>(std::move(sound)))
{
}

SoundReaderLease SoundReaderCache::acquire()
{
  std::shared_ptr<SoundReaderCacheState> state = this->state_;
  std::shared_ptr<aud::IReader> reader;
  {
    std::lock_guard lock{state->mutex};
    if (!state->idle_readers.is_empty()) {
      reader = state->idle_readers.pop_last();
    }
  }

  if (!reader && state->sound) {
    reader = state->sound->createReader();
  }
  if (!reader) {
    return {};
  }
  return SoundReaderLease(std::move(state), std::move(reader));
}

}  // namespace blender::bke

#endif
