/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "sound_reader_cache.hh"

#include "testing/testing.h"

#if defined(WITH_AUDASPACE)

#  include <memory>
#  include <utility>

#  include <IReader.h>
#  include <ISound.h>
#  include <util/Buffer.h>
#  include <util/StreamBuffer.h>

namespace blender::bke::tests {

struct ReaderControl {
  int readers_created = 0;
  int live_readers = 0;
};

class CountingSound : public aud::ISound {
 private:
  std::shared_ptr<aud::ISound> sound;

 public:
  std::shared_ptr<ReaderControl> control = std::make_shared<ReaderControl>();

  explicit CountingSound(std::shared_ptr<aud::ISound> sound) : sound(std::move(sound)) {}

  std::shared_ptr<aud::IReader> createReader() override
  {
    this->control->readers_created++;
    std::shared_ptr<aud::IReader> reader = this->sound->createReader();
    this->control->live_readers++;
    aud::IReader *reader_pointer = reader.get();
    return std::shared_ptr<aud::IReader>(
        reader_pointer,
        [reader = std::move(reader), control = this->control](aud::IReader *) mutable {
          reader.reset();
          control->live_readers--;
        });
  }
};

static std::shared_ptr<CountingSound> create_test_sound()
{
  auto buffer = std::make_shared<aud::Buffer>(sizeof(aud::sample_t));
  auto stream = std::make_shared<aud::StreamBuffer>(
      buffer, aud::Specs{aud::RATE_48000, aud::CHANNELS_MONO});
  return std::make_shared<CountingSound>(std::move(stream));
}

TEST(sound_reader_cache, ReusesReaderAndLimitsIdleReaders)
{
  const std::shared_ptr<CountingSound> sound = create_test_sound();
  SoundReaderCache cache(sound);

  {
    SoundReaderLease lease = cache.acquire();
    ASSERT_TRUE(lease);
  }
  {
    SoundReaderLease lease = cache.acquire();
    ASSERT_TRUE(lease);
  }
  EXPECT_EQ(sound->control->readers_created, 1);

  SoundReaderLease first = cache.acquire();
  SoundReaderLease second = cache.acquire();
  SoundReaderLease third = cache.acquire();
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_TRUE(third);
  EXPECT_EQ(sound->control->readers_created, 3);

  first = {};
  second = {};
  third = {};
  EXPECT_EQ(sound->control->live_readers, 2);

  SoundReaderLease reused_first = cache.acquire();
  SoundReaderLease reused_second = cache.acquire();
  ASSERT_TRUE(reused_first);
  ASSERT_TRUE(reused_second);
  EXPECT_EQ(sound->control->readers_created, 3);
}

TEST(sound_reader_cache, DiscardDestroysReader)
{
  const std::shared_ptr<CountingSound> sound = create_test_sound();
  SoundReaderCache cache(sound);
  {
    SoundReaderLease lease = cache.acquire();
    ASSERT_TRUE(lease);
    lease.discard();
    EXPECT_FALSE(lease);
  }
  EXPECT_EQ(sound->control->live_readers, 0);

  SoundReaderLease replacement = cache.acquire();
  ASSERT_TRUE(replacement);
  EXPECT_EQ(sound->control->readers_created, 2);
}

TEST(sound_reader_cache, MoveOperationsReturnEachReaderOnce)
{
  const std::shared_ptr<CountingSound> sound = create_test_sound();
  SoundReaderCache cache(sound);
  SoundReaderLease first = cache.acquire();
  ASSERT_TRUE(first);

  SoundReaderLease moved(std::move(first));
  EXPECT_FALSE(first);
  SoundReaderLease second = cache.acquire();
  ASSERT_TRUE(second);
  moved = std::move(second);
  EXPECT_FALSE(second);

  moved = {};
  SoundReaderLease reused_first = cache.acquire();
  SoundReaderLease reused_second = cache.acquire();
  ASSERT_TRUE(reused_first);
  ASSERT_TRUE(reused_second);
  EXPECT_EQ(sound->control->readers_created, 2);
}

TEST(sound_reader_cache, LeaseOutlivesCache)
{
  const std::shared_ptr<CountingSound> sound = create_test_sound();
  auto cache = std::make_shared<SoundReaderCache>(sound);
  SoundReaderLease lease = cache->acquire();
  ASSERT_TRUE(lease);

  cache.reset();
  EXPECT_EQ(sound->control->live_readers, 1);
  lease = {};
  EXPECT_EQ(sound->control->live_readers, 0);
}

}  // namespace blender::bke::tests

#endif
