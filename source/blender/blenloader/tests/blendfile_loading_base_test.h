/* SPDX-FileCopyrightText: 2019 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DEG_depsgraph.h"
#include "testing/testing.h"

struct BlendFileData;
struct Depsgraph;

class BlendfileLoadingBaseTest : public testing::Test {
 protected:
  struct BlendFileData *bfile = nullptr;
  struct Depsgraph *depsgraph = nullptr;

 public:
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
   * is only partially initialized (most importantly, without window manager),
   * the space types are not registered, so any versioning code that handles
   * those will SEGFAULT.
   */
  bool blendfile_load(const char *filepath);
  /* Free bfile if it is not nullptr. */
  void blendfile_free();

  /* Create a depsgraph. Assumes a blend file has been loaded to this->bfile. */
  virtual void depsgraph_create(eEvaluationMode depsgraph_evaluation_mode);
  /* Free the depsgraph if it's not nullptr. */
  virtual void depsgraph_free();
};
