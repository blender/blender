/* SPDX-FileCopyrightText: 2020-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */


#ifndef __GUARDEDALLOC_TEST_UTIL_H__
#define __GUARDEDALLOC_TEST_UTIL_H__

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

class LockFreeAllocatorTest : public ::testing::Test {
 protected:
  virtual void SetUp()
  {
    MEM_use_lockfree_allocator();
  }
};

class GuardedAllocatorTest : public ::testing::Test {
 protected:
  virtual void SetUp()
  {
    MEM_use_guarded_allocator();
  }
};

#endif  // __GUARDEDALLOC_TEST_UTIL_H__
