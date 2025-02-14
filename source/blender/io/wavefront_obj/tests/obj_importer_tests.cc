/* SPDX-FileCopyrightText: 2023-2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "testing/testing.h"

#include "BLI_string.h"

#include "CLG_log.h"

#include "obj_import_file_reader.hh"

namespace blender::io::obj {

/* Extensive tests for OBJ importing are in `io_obj_import_test.py`.
 * The tests here are only for testing OBJ reader buffer refill behavior,
 * by using a very small buffer size on purpose. */

TEST(obj_import, BufferRefillTest)
{
  CLG_init();

  OBJImportParams params;
  /* nurbs_cyclic.obj file has quite long lines, good to test read buffer refill. */
  std::string obj_path = blender::tests::flags_test_asset_dir() +
                         SEP_STR "io_tests" SEP_STR "obj" SEP_STR + "nurbs_cyclic.obj";
  STRNCPY(params.filepath, obj_path.c_str());

  /* Use a small read buffer size to test buffer refilling behavior. */
  const size_t read_buffer_size = 650;
  OBJParser obj_parser{params, read_buffer_size};

  Vector<std::unique_ptr<Geometry>> all_geometries;
  GlobalVertices global_vertices;
  obj_parser.parse(all_geometries, global_vertices);

  EXPECT_EQ(1, all_geometries.size());
  EXPECT_EQ(GEOM_CURVE, all_geometries[0]->geom_type_);
  EXPECT_EQ(28, global_vertices.vertices.size());
  EXPECT_EQ(31, all_geometries[0]->nurbs_element_.curv_indices.size());
  EXPECT_EQ(35, all_geometries[0]->nurbs_element_.parm.size());

  CLG_exit();
}

}  // namespace blender::io::obj
