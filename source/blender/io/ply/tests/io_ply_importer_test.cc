/* SPDX-FileCopyrightText: 2023-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_path_utils.hh"

#include "ply_import.hh"
#include "ply_import_buffer.hh"
#include "ply_import_data.hh"

namespace blender::io::ply {

/* Extensive tests for PLY importing are in `io_ply_import_test.py`.
 * The tests here are only for testing PLY reader buffer refill behavior,
 * by using a very small buffer size on purpose. */

TEST(ply_import, BufferRefillTest)
{
  std::string ply_path_a = blender::tests::flags_test_asset_dir() +
                           SEP_STR "io_tests" SEP_STR "ply" SEP_STR + "ASCII_wireframe_cube.ply";
  std::string ply_path_b = blender::tests::flags_test_asset_dir() +
                           SEP_STR "io_tests" SEP_STR "ply" SEP_STR + "wireframe_cube.ply";

  /* Use a small read buffer size to test buffer refilling behavior. */
  constexpr size_t buffer_size = 50;
  PlyReadBuffer infile_a(ply_path_a.c_str(), buffer_size);
  PlyReadBuffer infile_b(ply_path_b.c_str(), buffer_size);
  PlyHeader header_a, header_b;
  const char *header_err_a = read_header(infile_a, header_a);
  const char *header_err_b = read_header(infile_b, header_b);
  if (header_err_a != nullptr || header_err_b != nullptr) {
    fprintf(stderr, "Failed to read PLY header\n");
    ADD_FAILURE();
    return;
  }
  std::unique_ptr<PlyData> data_a = import_ply_data(infile_a, header_a);
  std::unique_ptr<PlyData> data_b = import_ply_data(infile_b, header_b);
  if (!data_a->error.empty() || !data_b->error.empty()) {
    fprintf(stderr, "Failed to read PLY data\n");
    ADD_FAILURE();
    return;
  }

  /* Check whether the edges list matches expectations. */
  std::pair<int, int> exp_edges[] = {{2, 0},
                                     {0, 1},
                                     {1, 3},
                                     {3, 2},
                                     {6, 2},
                                     {3, 7},
                                     {7, 6},
                                     {4, 6},
                                     {7, 5},
                                     {5, 4},
                                     {0, 4},
                                     {5, 1}};
  EXPECT_EQ(12, data_a->edges.size());
  EXPECT_EQ(12, data_b->edges.size());
  EXPECT_EQ_ARRAY(exp_edges, data_a->edges.data(), 12);
  EXPECT_EQ_ARRAY(exp_edges, data_b->edges.data(), 12);
}

//@TODO: now we put vertex color attribute first, maybe put position first?
//@TODO: test with vertex element having list properties
//@TODO: test with edges starting with non-vertex index properties
//@TODO: test various malformed headers
//@TODO: UVs with: s,t; u,v; texture_u,texture_v; texture_s,texture_t (from miniply)
//@TODO: colors with: r,g,b in addition to red,green,blue (from miniply)

}  // namespace blender::io::ply
