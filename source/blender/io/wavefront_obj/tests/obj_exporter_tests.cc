/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>
#include <ios>
#include <memory>
#include <string>
#include <system_error>

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_main.hh"

#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLO_readfile.hh"

#include "DEG_depsgraph.hh"

#include "obj_export_file_writer.hh"
#include "obj_export_mesh.hh"
#include "obj_export_nurbs.hh"
#include "obj_exporter.hh"

#include "obj_exporter_tests.hh"

namespace blender::io::obj {
/* Set this true to keep comparison-failing test output in temp file directory. */
constexpr bool save_failing_test_output = false;

/* This is also the test name. */
class OBJExportTest : public BlendfileLoadingBaseTest {
 public:
  /**
   * \param filepath: relative to "tests" directory.
   */
  bool load_file_and_depsgraph(const std::string &filepath,
                               const eEvaluationMode eval_mode = DAG_EVAL_VIEWPORT)
  {
    if (!blendfile_load(filepath.c_str())) {
      return false;
    }
    depsgraph_create(eval_mode);
    return true;
  }
};

const std::string all_objects_file = "io_tests" SEP_STR "blend_scene" SEP_STR "all_objects.blend";

TEST_F(OBJExportTest, filter_objects_curves_as_mesh)
{
  OBJExportParamsDefault _export;
  if (!load_file_and_depsgraph(all_objects_file)) {
    ADD_FAILURE();
    return;
  }
  auto [objmeshes, objcurves]{filter_supported_objects(depsgraph, _export.params)};
  EXPECT_EQ(objmeshes.size(), 21);
  EXPECT_EQ(objcurves.size(), 0);
}

TEST_F(OBJExportTest, filter_objects_curves_as_nurbs)
{
  OBJExportParamsDefault _export;
  if (!load_file_and_depsgraph(all_objects_file)) {
    ADD_FAILURE();
    return;
  }
  _export.params.export_curves_as_nurbs = true;
  auto [objmeshes, objcurves]{filter_supported_objects(depsgraph, _export.params)};
  EXPECT_EQ(objmeshes.size(), 18);
  EXPECT_EQ(objcurves.size(), 3);
}

TEST_F(OBJExportTest, filter_objects_selected)
{
  OBJExportParamsDefault _export;
  if (!load_file_and_depsgraph(all_objects_file)) {
    ADD_FAILURE();
    return;
  }
  _export.params.export_selected_objects = true;
  _export.params.export_curves_as_nurbs = true;
  auto [objmeshes, objcurves]{filter_supported_objects(depsgraph, _export.params)};
  EXPECT_EQ(objmeshes.size(), 1);
  EXPECT_EQ(objcurves.size(), 0);
}

TEST(obj_exporter_utils, append_negative_frame_to_filename)
{
  const char path_original[FILE_MAX] = SEP_STR "my_file.obj";
  const char path_truth[FILE_MAX] = SEP_STR "my_file-0012.obj";
  const int frame = -12;
  char path_with_frame[FILE_MAX] = {0};
  const bool ok = append_frame_to_filename(path_original, frame, path_with_frame);
  EXPECT_TRUE(ok);
  EXPECT_STREQ(path_with_frame, path_truth);
}

TEST(obj_exporter_utils, append_positive_frame_to_filename)
{
  const char path_original[FILE_MAX] = SEP_STR "my_file.obj";
  const char path_truth[FILE_MAX] = SEP_STR "my_file0012.obj";
  const int frame = 12;
  char path_with_frame[FILE_MAX] = {0};
  const bool ok = append_frame_to_filename(path_original, frame, path_with_frame);
  EXPECT_TRUE(ok);
  EXPECT_STREQ(path_with_frame, path_truth);
}

TEST(obj_exporter_utils, append_large_positive_frame_to_filename)
{
  const char path_original[FILE_MAX] = SEP_STR "my_file.obj";
  const char path_truth[FILE_MAX] = SEP_STR "my_file1234567.obj";
  const int frame = 1234567;
  char path_with_frame[FILE_MAX] = {0};
  const bool ok = append_frame_to_filename(path_original, frame, path_with_frame);
  EXPECT_TRUE(ok);
  EXPECT_STREQ(path_with_frame, path_truth);
}

static std::string read_temp_file_in_string(const std::string &file_path)
{
  std::string res;
  size_t buffer_len;
  void *buffer = BLI_file_read_text_as_mem(file_path.c_str(), 0, &buffer_len);
  if (buffer != nullptr) {
    res.assign((const char *)buffer, buffer_len);
    MEM_freeN(buffer);
  }
  return res;
}

class ObjExporterWriterTest : public testing::Test {
 protected:
  void SetUp() override
  {
    BKE_tempdir_init(nullptr);
  }

  void TearDown() override
  {
    BKE_tempdir_session_purge();
  }

  std::string get_temp_obj_filename()
  {
    /* Use Latin Capital Letter A with Ogonek, Cyrillic Capital Letter Zhe
     * at the end, to test I/O on non-English file names. */
    const char *const temp_file_path = "output\xc4\x84\xd0\x96.OBJ";

    return std::string(BKE_tempdir_session()) + SEP_STR + std::string(temp_file_path);
  }

  std::unique_ptr<OBJWriter> init_writer(const OBJExportParams &params,
                                         const std::string out_filepath)
  {
    try {
      auto writer = std::make_unique<OBJWriter>(out_filepath.c_str(), params);
      return writer;
    }
    catch (const std::system_error &ex) {
      std::cerr << ex.code().category().name() << ": " << ex.what() << ": " << ex.code().message()
                << std::endl;
      return nullptr;
    }
  }
};

TEST_F(ObjExporterWriterTest, header)
{
  /* Because testing doesn't fully initialize Blender, we need the following. */
  BKE_tempdir_init(nullptr);
  std::string out_file_path = get_temp_obj_filename();
  {
    OBJExportParamsDefault _export;
    std::unique_ptr<OBJWriter> writer = init_writer(_export.params, out_file_path);
    if (!writer) {
      ADD_FAILURE();
      return;
    }
    writer->write_header();
  }
  const std::string result = read_temp_file_in_string(out_file_path);
  using namespace std::string_literals;
  ASSERT_EQ(result, "# Blender "s + BKE_blender_version_string() + "\n" + "# www.blender.org\n");
}

TEST_F(ObjExporterWriterTest, mtllib)
{
  std::string out_file_path = get_temp_obj_filename();
  {
    OBJExportParamsDefault _export;
    std::unique_ptr<OBJWriter> writer = init_writer(_export.params, out_file_path);
    if (!writer) {
      ADD_FAILURE();
      return;
    }
    writer->write_mtllib_name("/Users/blah.mtl");
    writer->write_mtllib_name("\\C:\\blah.mtl");
  }
  const std::string result = read_temp_file_in_string(out_file_path);
  ASSERT_EQ(result, "mtllib blah.mtl\nmtllib blah.mtl\n");
}

TEST(obj_exporter_writer, format_handler_buffer_chunking)
{
  /* Use a tiny buffer chunk size, so that the test below ends up creating several blocks. */
  FormatHandler h(16);
  h.write_obj_object("abc");
  h.write_obj_object("abcd");
  h.write_obj_object("abcde");
  h.write_obj_object("abcdef");
  h.write_obj_object("012345678901234567890123456789abcd");
  h.write_obj_object("123");
  h.write_obj_curve_begin();
  h.write_obj_newline();
  h.write_obj_nurbs_parm_begin();
  h.write_obj_newline();

  size_t got_blocks = h.get_block_count();
  ASSERT_EQ(got_blocks, 7);

  std::string got_string = h.get_as_string();
  using namespace std::string_literals;
  const char *expected = R"(o abc
o abcd
o abcde
o abcdef
o 012345678901234567890123456789abcd
o 123
curv 0.0 1.0
parm u 0.0
)";
  ASSERT_EQ(got_string, expected);
}

/* Return true if string #a and string #b are equal after their first newline. */
static bool strings_equal_after_first_lines(const std::string &a, const std::string &b)
{
  const size_t a_len = a.size();
  const size_t b_len = b.size();
  const size_t a_next = a.find_first_of('\n');
  const size_t b_next = b.find_first_of('\n');
  if (a_next == std::string::npos || b_next == std::string::npos) {
    std::cout << "Couldn't find newline in one of args\n";
    return false;
  }
  if (a.compare(a_next, a_len - a_next, b, b_next, b_len - b_next) != 0) {
    for (int i = 0; i < a_len - a_next && i < b_len - b_next; ++i) {
      if (a[a_next + i] != b[b_next + i]) {
        std::cout << "Difference found at pos " << a_next + i << " of a\n";
        std::cout << "a: " << a.substr(a_next + i, 100) << " ...\n";
        std::cout << "b: " << b.substr(b_next + i, 100) << " ... \n";
        return false;
      }
    }
    return false;
  }
  return true;
}

/* From here on, tests are whole file tests, testing for golden output. */
class OBJExportRegressionTest : public OBJExportTest {
 public:
  /**
   * Export the given blend file with the given parameters and
   * test to see if it matches a golden file (ignoring any difference in Blender version number).
   * \param blendfile: input, relative to "tests" directory.
   * \param golden_obj: expected output, relative to "tests" directory.
   * \param params: the parameters to be used for export.
   */
  void compare_obj_export_to_golden(const std::string &blendfile,
                                    const std::string &golden_obj,
                                    const std::string &golden_mtl,
                                    OBJExportParams &params)
  {
    if (!load_file_and_depsgraph(blendfile)) {
      return;
    }
    /* Because testing doesn't fully initialize Blender, we need the following. */
    BKE_tempdir_init(nullptr);
    std::string tempdir = std::string(BKE_tempdir_base());
    std::string out_file_path = tempdir + BLI_path_basename(golden_obj.c_str());
    STRNCPY(params.filepath, out_file_path.c_str());
    params.blen_filepath = bfile->main->filepath;
    std::string golden_file_path = blender::tests::flags_test_asset_dir() + SEP_STR + golden_obj;
    BLI_path_split_dir_part(
        golden_file_path.c_str(), params.file_base_for_tests, sizeof(params.file_base_for_tests));
    export_frame(depsgraph, params, out_file_path.c_str());
    std::string output_str = read_temp_file_in_string(out_file_path);

    std::string golden_str = read_temp_file_in_string(golden_file_path);
    bool are_equal = strings_equal_after_first_lines(output_str, golden_str);
    if (save_failing_test_output && !are_equal) {
      printf("failing test output in %s\n", out_file_path.c_str());
    }
    ASSERT_TRUE(are_equal);
    if (!save_failing_test_output || are_equal) {
      BLI_delete(out_file_path.c_str(), false, false);
    }
    if (!golden_mtl.empty()) {
      std::string out_mtl_file_path = tempdir + BLI_path_basename(golden_mtl.c_str());
      std::string output_mtl_str = read_temp_file_in_string(out_mtl_file_path);
      std::string golden_mtl_file_path = blender::tests::flags_test_asset_dir() + SEP_STR +
                                         golden_mtl;
      std::string golden_mtl_str = read_temp_file_in_string(golden_mtl_file_path);
      are_equal = strings_equal_after_first_lines(output_mtl_str, golden_mtl_str);
      if (save_failing_test_output && !are_equal) {
        printf("failing test output in %s\n", out_mtl_file_path.c_str());
      }
      ASSERT_TRUE(are_equal);
      if (!save_failing_test_output || are_equal) {
        BLI_delete(out_mtl_file_path.c_str(), false, false);
      }
    }
  }
};

TEST_F(OBJExportRegressionTest, all_tris)
{
  OBJExportParamsDefault _export;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "all_tris.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "all_tris.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "all_tris.mtl",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, all_quads)
{
  OBJExportParamsDefault _export;
  _export.params.global_scale = 2.0f;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "all_quads.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "all_quads.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, fgons)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "fgons.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "fgons.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, edges)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "edges.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "edges.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, vertices)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cube_loose_edges_verts.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cube_loose_edges_verts.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cube_loose_edges)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "vertices.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "vertices.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, non_uniform_scale)
{
  OBJExportParamsDefault _export;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "non_uniform_scale.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "non_uniform_scale.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, nurbs_as_nurbs)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "nurbs.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "nurbs.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, nurbs_curves_as_nurbs)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "nurbs_curves.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "nurbs_curves.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, nurbs_as_mesh)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "nurbs.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "nurbs_mesh.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cube_all_data_triangulated)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  _export.params.export_triangulated_mesh = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "cube_all_data.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cube_all_data_triangulated.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cube_normal_edit)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cube_normal_edit.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cube_normal_edit.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cube_vertex_groups)
{
  OBJExportParamsDefault _export;
  _export.params.export_materials = false;
  _export.params.export_normals = false;
  _export.params.export_uv = false;
  _export.params.export_vertex_groups = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cube_vertex_groups.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cube_vertex_groups.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cubes_positioned)
{
  OBJExportParamsDefault _export;
  _export.params.export_materials = false;
  _export.params.global_scale = 2.0f;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cubes_positioned.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cubes_positioned.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cubes_vertex_colors)
{
  OBJExportParamsDefault _export;
  _export.params.export_colors = true;
  _export.params.export_normals = false;
  _export.params.export_uv = false;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cubes_vertex_colors.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cubes_vertex_colors.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cubes_with_textures_strip)
{
  OBJExportParamsDefault _export;
  _export.params.path_mode = PATH_REFERENCE_STRIP;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cubes_with_textures.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cubes_with_textures.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "cubes_with_textures.mtl",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, cubes_with_textures_relative)
{
  OBJExportParamsDefault _export;
  _export.params.path_mode = PATH_REFERENCE_RELATIVE;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "cubes_with_textures.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "cubes_with_textures_rel.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "cubes_with_textures_rel.mtl",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, suzanne_all_data)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_materials = false;
  _export.params.export_smooth_groups = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR
                               "suzanne_all_data.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "suzanne_all_data.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, all_curves)
{
  OBJExportParamsDefault _export;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_scene" SEP_STR "all_curves.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "all_curves.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, all_curves_as_nurbs)
{
  OBJExportParamsDefault _export;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_scene" SEP_STR "all_curves.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "all_curves_as_nurbs.obj",
                               "",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, all_objects)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_smooth_groups = true;
  _export.params.export_colors = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_scene" SEP_STR "all_objects.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "all_objects.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "all_objects.mtl",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, all_objects_mat_groups)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = IO_AXIS_Y;
  _export.params.up_axis = IO_AXIS_Z;
  _export.params.export_smooth_groups = true;
  _export.params.export_material_groups = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_scene" SEP_STR "all_objects.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "all_objects_mat_groups.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "all_objects_mat_groups.mtl",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, materials_without_pbr)
{
  OBJExportParamsDefault _export;
  _export.params.export_normals = false;
  _export.params.path_mode = PATH_REFERENCE_RELATIVE;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "materials_pbr.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "materials_without_pbr.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "materials_without_pbr.mtl",
                               _export.params);
}

TEST_F(OBJExportRegressionTest, materials_pbr)
{
  OBJExportParamsDefault _export;
  _export.params.export_normals = false;
  _export.params.path_mode = PATH_REFERENCE_RELATIVE;
  _export.params.export_pbr_extensions = true;
  compare_obj_export_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "materials_pbr.blend",
                               "io_tests" SEP_STR "obj" SEP_STR "materials_pbr.obj",
                               "io_tests" SEP_STR "obj" SEP_STR "materials_pbr.mtl",
                               _export.params);
}

}  // namespace blender::io::obj
