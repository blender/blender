/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 by Blender Foundation.
 */
#ifndef __BLENDFILE_LOADING_BASE_TEST_H__
#define __BLENDFILE_LOADING_BASE_TEST_H__

#include "DEG_depsgraph.h"
#include "testing/testing.h"

struct BlendFileData;
struct Depsgraph;

class BlendfileLoadingBaseTest : public testing::Test {
 protected:
  struct BlendFileData *bfile = nullptr;
  struct Depsgraph *depsgraph = nullptr;

 public:
  virtual ~BlendfileLoadingBaseTest();

  /* Sets up Blender just enough to not crash on loading
   * a blendfile and constructing a depsgraph. */
  static void SetUpTestCase();
  static void TearDownTestCase();

 protected:
  /* Frees the depsgraph & blendfile. */
  virtual void TearDown();

  /* Loads a blend file from the lib/tests directory from SVN.
   * Returns 'ok' flag (true=good, false=bad) and sets this->bfile.
   * Fails the test if the file cannot be loaded (still returns though).
   * Requires the CLI argument --test-asset-dir to point to ../../lib/tests.
   *
   * WARNING: only files saved with Blender 2.80+ can be loaded. Since Blender
   * is only partially initialised (most importantly, without window manager),
   * the space types are not registered, so any versioning code that handles
   * those will SEGFAULT.
   */
  bool blendfile_load(const char *filepath);
  /* Free bfile if it is not nullptr. */
  void blendfile_free();

  /* Create a depsgraph. Assumes a blend file has been loaded to this->bfile. */
  void depsgraph_create(eEvaluationMode depsgraph_evaluation_mode);
  /* Free the depsgraph if it's not nullptr. */
  void depsgraph_free();
};

#endif /* __BLENDFILE_LOADING_BASE_TEST_H__ */
