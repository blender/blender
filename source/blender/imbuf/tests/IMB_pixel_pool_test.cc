/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <array>
#include <cstring>
#include <thread>

#include "testing/testing.h"

#include "BKE_gtest_base.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "intern/pixel_pool.hh"

namespace blender::imbuf::tests {

static constexpr size_t buffer_size = 8 * 1024 * 1024;

class PixelPoolTest : public bke::BlenderGTestBase {
 protected:
  void SetUp() override
  {
    /* Start each test with an empty pool, and space for four buffers. */
    IMB_pixel_pool_set_limit(0);
    IMB_pixel_pool_set_limit(4 * buffer_size);
  }

  void TearDown() override
  {
    IMB_pixel_pool_set_limit(0);
    EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
  }
};

TEST_F(PixelPoolTest, ReuseAndInitializeData)
{
  /* Allocate storage, set data to non-zero, release to the pool. */
  auto first = pixel_pool_allocate(buffer_size, 0, false, true);
  void *address = first.data;
  memset(first.data, 123, buffer_size);
  first.sharing_info.reset();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size);

  /* Request same size image, but with cleared data: should get the same pointer,
   * and the data should be cleared. */
  auto second = pixel_pool_allocate(buffer_size, 0, true, true);
  EXPECT_EQ(second.data, address);
  const auto *bytes = static_cast<const uint8_t *>(second.data);
  EXPECT_TRUE(std::all_of(bytes, bytes + buffer_size, [](uint8_t v) { return v == 0; }));
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
}

TEST_F(PixelPoolTest, SmallOrTooLargeShouldNotGetPooled)
{
  /* Requests without "use pool" flag, below size threshold, or above the budget should not get
   * pooled. */
  pixel_pool_allocate(buffer_size, 0, false, false);
  pixel_pool_allocate(buffer_size - 1, 0, false, true);
  pixel_pool_allocate(4 * buffer_size + 1, 0, false, true);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
}

TEST_F(PixelPoolTest, LastOwnerAndWeakReferences)
{
  /* Keep a second strong owner and a weak reference to the original allocation. */
  auto first = pixel_pool_allocate(buffer_size, 0, false, true);
  void *address = first.data;
  ImplicitSharingPtr<> other = first.sharing_info;
  const ImplicitSharingInfo *weak = first.sharing_info.get();
  weak->add_weak_user();

  /* Storage becomes available only after the last strong owner releases it. */
  first.sharing_info.reset();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
  other.reset();
  EXPECT_TRUE(weak->is_expired());
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size);

  /* Reused storage gets new sharing info while the old weak reference remains expired. */
  auto second = pixel_pool_allocate(buffer_size, 0, false, true);
  EXPECT_EQ(second.data, address);
  EXPECT_NE(second.sharing_info.get(), weak);
  weak->remove_weak_user_and_delete_if_last();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
}

TEST_F(PixelPoolTest, AlignmentAndSizeCompatibility)
{
  /* Get one aligned buffer. */
  auto first = pixel_pool_allocate(buffer_size, 64, false, true);
  void *address = first.data;
  EXPECT_EQ(uintptr_t(address) % 64, 0);
  first.sharing_info.reset();

  /* Different sizes or alignments must leave the cached buffer untouched. */
  auto different_alignment = pixel_pool_allocate(buffer_size, 256, false, true);
  auto different_size = pixel_pool_allocate(buffer_size * 2, 64, false, true);
  EXPECT_NE(different_alignment.data, address);
  EXPECT_NE(different_size.data, address);

  /* An exact match reuses the cached buffer. */
  auto same = pixel_pool_allocate(buffer_size, 64, false, true);
  EXPECT_EQ(same.data, address);
}

TEST_F(PixelPoolTest, BudgetReachedEvictsOldest)
{
  /* Allocate three buffers with a budget that can cache only two. */
  IMB_pixel_pool_set_limit(2 * buffer_size);
  auto a = pixel_pool_allocate(buffer_size, 0, false, true);
  auto b = pixel_pool_allocate(buffer_size, 0, false, true);
  auto c = pixel_pool_allocate(buffer_size, 0, false, true);
  void *b_address = b.data;
  void *c_address = c.data;

  /* Returning all three evicts the first buffer returned. */
  a.sharing_info.reset();
  b.sharing_info.reset();
  c.sharing_info.reset();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 2 * buffer_size);

  /* Matching requests take the most recently returned buffer first. */
  auto newest = pixel_pool_allocate(buffer_size, 0, false, true);
  auto older = pixel_pool_allocate(buffer_size, 0, false, true);
  EXPECT_EQ(newest.data, c_address);
  EXPECT_EQ(older.data, b_address);
}

TEST_F(PixelPoolTest, ReduceBudgetAndDisableWithLiveOwners)
{
  /* Keep one buffer in use and return a larger buffer to the pool. */
  auto live = pixel_pool_allocate(buffer_size, 0, false, true);
  pixel_pool_allocate(buffer_size * 2, 0, false, true);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size * 2);

  /* Reducing the budget immediately evicts storage that no longer fits. */
  IMB_pixel_pool_set_limit(buffer_size);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);

  /* Disabling the pool also prevents the live buffer from being cached later. */
  IMB_pixel_pool_set_limit(0);
  live.sharing_info.reset();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
}

TEST_F(PixelPoolTest, ClearPreservesLiveOwners)
{
  /* Clear unused storage while another buffer is still in use. */
  auto live = pixel_pool_allocate(buffer_size, 0, false, true);
  pixel_pool_allocate(buffer_size, 0, false, true);
  IMB_pixel_pool_clear();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);

  /* Clearing keeps pooling enabled, so the live buffer can still return to the pool. */
  live.sharing_info.reset();
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size);
}

TEST_F(PixelPoolTest, TrimPreservesCapAndLiveOwners)
{
  /* Return two buffers in order, keeping a third buffer in use. */
  auto oldest = pixel_pool_allocate(buffer_size, 0, false, true);
  auto newest = pixel_pool_allocate(buffer_size, 0, false, true);
  auto live = pixel_pool_allocate(buffer_size, 0, false, true);
  void *newest_address = newest.data;
  oldest.sharing_info.reset();
  newest.sharing_info.reset();

  /* Trim removes oldest, preserves most recently returned. */
  IMB_pixel_pool_trim(buffer_size);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size);
  auto reused = pixel_pool_allocate(buffer_size, 0, false, true);
  EXPECT_EQ(reused.data, newest_address);

  /* Trim to zero still leaves pooling enabled, with original cap. */
  reused.sharing_info.reset();
  IMB_pixel_pool_trim(0);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);
  live.sharing_info.reset();
  pixel_pool_allocate(2 * buffer_size, 0, false, true);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 3 * buffer_size);
}

TEST_F(PixelPoolTest, ImBufCopyOnWrite)
{
  /* Duplicating an ImBuf initially shares its pixel storage. */
  ImBuf *first = IMB_allocImBuf(2048, 1024, ImBufFlags::ByteData | ImBufFlags::PoolPixels);
  first->byte_data_for_write()[0] = 42;
  ImBuf *copy = IMB_dupImBuf(first);
  EXPECT_EQ(first->byte_data(), copy->byte_data());

  /* Writing to the copy separates its pixels and preserves the original image. */
  copy->byte_data_for_write()[0] = 99;
  EXPECT_NE(first->byte_data(), copy->byte_data());
  EXPECT_EQ(first->byte_data()[0], 42);
  EXPECT_EQ(copy->byte_data()[0], 99);

  /* Both allocations return to the pool when their images are freed. */
  IMB_freeImBuf(first);
  IMB_freeImBuf(copy);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 2 * buffer_size);

  /* A new ImBuf clears the reused storage as usual. */
  ImBuf *initialized = IMB_allocImBuf(2048, 1024, ImBufFlags::ByteData | ImBufFlags::PoolPixels);
  EXPECT_EQ(initialized->byte_data()[0], 0);
  IMB_freeImBuf(initialized);
}

TEST_F(PixelPoolTest, CrossThreadRelease)
{
  /* Allocate on this thread and release the last owner on another thread. */
  auto allocation = pixel_pool_allocate(buffer_size, 64, false, true);
  void *address = allocation.data;
  std::thread worker([owner = std::move(allocation.sharing_info)]() mutable { owner.reset(); });
  worker.join();

  /* Storage returned by the worker is available on this thread. */
  auto reused = pixel_pool_allocate(buffer_size, 64, false, true);
  EXPECT_EQ(reused.data, address);
}

TEST_F(PixelPoolTest, ConcurrentCheckoutAndReturn)
{
  /* Repeatedly acquire and release buffers from several threads. */
  std::array<std::thread, 4> workers;
  for (int i = 0; i < workers.size(); i++) {
    workers[i] = std::thread([i]() {
      for (int j = 0; j < 64; j++) {
        auto allocation = pixel_pool_allocate(buffer_size, 64, false, true);
        auto *value = static_cast<int *>(allocation.data);
        /* Each live allocation must retain its value while other threads use the pool. */
        *value = i;
        std::this_thread::yield();
        EXPECT_EQ(*value, i);
      }
    });
  }
  /* After all workers finish, cached storage must remain within budget. */
  for (std::thread &worker : workers) {
    worker.join();
  }
  EXPECT_LE(IMB_pixel_pool_unused_bytes_get(), 4 * buffer_size);
}

TEST_F(PixelPoolTest, FloatStorageAndRotation)
{
  /* Allocate initialized float pixels and mark one pixel for tracking through rotation. */
  ImBuf *image = IMB_allocImBuf(1024, 512, ImBufFlags::FloatData | ImBufFlags::PoolPixels);
  float *pixels = image->float_data_for_write();
  pixels[0] = 2.0f;
  EXPECT_EQ(pixels[1], 0.0f);

  /* Rotation preserves pixel values and returns the old storage to the pool. */
  EXPECT_TRUE(IMB_rotate_orthogonal(image, 90));
  EXPECT_EQ(image->x, 512);
  EXPECT_EQ(image->y, 1024);
  EXPECT_EQ(image->float_data()[size_t(1023) * 512 * 4], 2.0f);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size);

  /* Freeing the image also caches the rotated storage. */
  IMB_freeImBuf(image);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 2 * buffer_size);
}

TEST_F(PixelPoolTest, ImageLoadingOptIn)
{
  /* Encode an image with a known pixel value for both loading paths. */
  ImBuf *source = IMB_allocImBuf(2048, 1024, ImBufFlags::ByteData);
  source->byte_data_for_write()[0] = 42;
  source->byte_data_for_write()[3] = 255;
  const Vector<uint8_t> encoded = IMB_save_image_to_buffer(source, ImBufFlags::Zero);
  IMB_freeImBuf(source);
  ASSERT_FALSE(encoded.is_empty());

  /* Ordinary image loading does not return pixel storage to the pool. */
  const ImBufFlags flags = ImBufFlags::ByteData | ImBufFlags::NoColorspaceConvert;
  ImBuf *ordinary = IMB_load_image_from_memory(
      encoded.data(), encoded.size(), flags, "pool test", nullptr, nullptr);
  ASSERT_NE(ordinary, nullptr);
  EXPECT_EQ(ordinary->byte_data()[0], 42);
  IMB_freeImBuf(ordinary);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), 0);

  /* Opting in preserves decoded pixels and caches their storage when the image is freed. */
  ImBuf *pooled = IMB_load_image_from_memory(encoded.data(),
                                             encoded.size(),
                                             flags | ImBufFlags::PoolPixels,
                                             "pool test",
                                             nullptr,
                                             nullptr);
  ASSERT_NE(pooled, nullptr);
  EXPECT_EQ(pooled->byte_data()[0], 42);
  IMB_freeImBuf(pooled);
  EXPECT_EQ(IMB_pixel_pool_unused_bytes_get(), buffer_size);
}

}  // namespace blender::imbuf::tests
