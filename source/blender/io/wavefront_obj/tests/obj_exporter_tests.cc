/* Apache License, Version 2.0 */

#include <gtest/gtest.h>
#include <ios>
#include <memory>
#include <string>
#include <system_error>

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"

#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "DEG_depsgraph.h"

#include "obj_export_file_writer.hh"
#include "obj_export_mesh.hh"
#include "obj_export_nurbs.hh"
#include "obj_exporter.hh"

#include "obj_exporter_tests.hh"

namespace blender::io::obj {
/* Set this true to keep comparison-failing test output in temp file directory. */
constexpr bool save_failing_test_output = false;

/* This is also the test name. */
class obj_exporter_test : public BlendfileLoadingBaseTest {
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

const std::string all_objects_file = "io_tests/blend_scene/all_objects.blend";
const std::string all_curve_objects_file = "io_tests/blend_scene/all_curves.blend";

TEST_F(obj_exporter_test, filter_objects_curves_as_mesh)
{
  OBJExportParamsDefault _export;
  if (!load_file_and_depsgraph(all_objects_file)) {
    ADD_FAILURE();
    return;
  }
  auto [objmeshes, objcurves]{filter_supported_objects(depsgraph, _export.params)};
  EXPECT_EQ(objmeshes.size(), 19);
  EXPECT_EQ(objcurves.size(), 0);
}

TEST_F(obj_exporter_test, filter_objects_curves_as_nurbs)
{
  OBJExportParamsDefault _export;
  if (!load_file_and_depsgraph(all_objects_file)) {
    ADD_FAILURE();
    return;
  }
  _export.params.export_curves_as_nurbs = true;
  auto [objmeshes, objcurves]{filter_supported_objects(depsgraph, _export.params)};
  EXPECT_EQ(objmeshes.size(), 18);
  EXPECT_EQ(objcurves.size(), 2);
}

TEST_F(obj_exporter_test, filter_objects_selected)
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
  const char path_original[FILE_MAX] = "/my_file.obj";
  const char path_truth[FILE_MAX] = "/my_file-123.obj";
  const int frame = -123;
  char path_with_frame[FILE_MAX] = {0};
  const bool ok = append_frame_to_filename(path_original, frame, path_with_frame);
  EXPECT_TRUE(ok);
  EXPECT_EQ_ARRAY(path_with_frame, path_truth, BLI_strlen_utf8(path_truth));
}

TEST(obj_exporter_utils, append_positive_frame_to_filename)
{
  const char path_original[FILE_MAX] = "/my_file.obj";
  const char path_truth[FILE_MAX] = "/my_file123.obj";
  const int frame = 123;
  char path_with_frame[FILE_MAX] = {0};
  const bool ok = append_frame_to_filename(path_original, frame, path_with_frame);
  EXPECT_TRUE(ok);
  EXPECT_EQ_ARRAY(path_with_frame, path_truth, BLI_strlen_utf8(path_truth));
}

TEST_F(obj_exporter_test, curve_nurbs_points)
{
  if (!load_file_and_depsgraph(all_curve_objects_file)) {
    ADD_FAILURE();
    return;
  }

  OBJExportParamsDefault _export;
  _export.params.export_curves_as_nurbs = true;
  auto [objmeshes_unused, objcurves]{filter_supported_objects(depsgraph, _export.params)};

  for (auto &objcurve : objcurves) {
    if (all_nurbs_truth.count(objcurve->get_curve_name()) != 1) {
      ADD_FAILURE();
      return;
    }
    const NurbsObject *const nurbs_truth = all_nurbs_truth.at(objcurve->get_curve_name()).get();
    EXPECT_EQ(objcurve->total_splines(), nurbs_truth->total_splines());
    for (int spline_index : IndexRange(objcurve->total_splines())) {
      EXPECT_EQ(objcurve->total_spline_vertices(spline_index),
                nurbs_truth->total_spline_vertices(spline_index));
      EXPECT_EQ(objcurve->get_nurbs_degree(spline_index),
                nurbs_truth->get_nurbs_degree(spline_index));
      EXPECT_EQ(objcurve->total_spline_control_points(spline_index),
                nurbs_truth->total_spline_control_points(spline_index));
    }
  }
}

TEST_F(obj_exporter_test, curve_coordinates)
{
  if (!load_file_and_depsgraph(all_curve_objects_file)) {
    ADD_FAILURE();
    return;
  }

  OBJExportParamsDefault _export;
  _export.params.export_curves_as_nurbs = true;
  auto [objmeshes_unused, objcurves]{filter_supported_objects(depsgraph, _export.params)};

  for (auto &objcurve : objcurves) {
    if (all_nurbs_truth.count(objcurve->get_curve_name()) != 1) {
      ADD_FAILURE();
      return;
    }
    const NurbsObject *const nurbs_truth = all_nurbs_truth.at(objcurve->get_curve_name()).get();
    EXPECT_EQ(objcurve->total_splines(), nurbs_truth->total_splines());
    for (int spline_index : IndexRange(objcurve->total_splines())) {
      for (int vertex_index : IndexRange(objcurve->total_spline_vertices(spline_index))) {
        EXPECT_V3_NEAR(objcurve->vertex_coordinates(
                           spline_index, vertex_index, _export.params.scaling_factor),
                       nurbs_truth->vertex_coordinates(spline_index, vertex_index),
                       0.000001f);
      }
    }
  }
}

static std::unique_ptr<OBJWriter> init_writer(const OBJExportParams &params,
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

/* The following is relative to BKE_tempdir_base.
 * Use Latin Capital Letter A with Ogonek, Cyrillic Capital Letter Zhe
 * at the end, to test I/O on non-English file names. */
const char *const temp_file_path = "output\xc4\x84\xd0\x96.OBJ";

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

TEST(obj_exporter_writer, header)
{
  /* Because testing doesn't fully initialize Blender, we need the following. */
  BKE_tempdir_init(nullptr);
  std::string out_file_path = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
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
  BLI_delete(out_file_path.c_str(), false, false);
}

TEST(obj_exporter_writer, mtllib)
{
  std::string out_file_path = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
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
  BLI_delete(out_file_path.c_str(), false, false);
}

/* Return true if string #a and string #b are equal after their first newline. */
static bool strings_equal_after_first_lines(const std::string &a, const std::string &b)
{
  /* If `dbg_level` is true then a failing test will print context around the first mismatch. */
  const bool dbg_level = false;
  const size_t a_len = a.size();
  const size_t b_len = b.size();
  const size_t a_next = a.find_first_of('\n');
  const size_t b_next = b.find_first_of('\n');
  if (a_next == std::string::npos || b_next == std::string::npos) {
    if (dbg_level) {
      std::cout << "Couldn't find newline in one of args\n";
    }
    return false;
  }
  if (dbg_level) {
    if (a.compare(a_next, a_len - a_next, b, b_next, b_len - b_next) != 0) {
      for (int i = 0; i < a_len - a_next && i < b_len - b_next; ++i) {
        if (a[a_next + i] != b[b_next + i]) {
          std::cout << "Difference found at pos " << a_next + i << " of a\n";
          std::cout << "a: " << a.substr(a_next + i, 100) << " ...\n";
          std::cout << "b: " << b.substr(b_next + i, 100) << " ... \n";
          return false;
        }
      }
    }
    else {
      return true;
    }
  }
  return a.compare(a_next, a_len - a_next, b, b_next, b_len - b_next) == 0;
}

/* From here on, tests are whole file tests, testing for golden output. */
class obj_exporter_regression_test : public obj_exporter_test {
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
    strncpy(params.filepath, out_file_path.c_str(), FILE_MAX - 1);
    params.blen_filepath = blendfile.c_str();
    export_frame(depsgraph, params, out_file_path.c_str());
    std::string output_str = read_temp_file_in_string(out_file_path);

    std::string golden_file_path = blender::tests::flags_test_asset_dir() + "/" + golden_obj;
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
      std::string golden_mtl_file_path = blender::tests::flags_test_asset_dir() + "/" + golden_mtl;
      std::string golden_mtl_str = read_temp_file_in_string(golden_mtl_file_path);
      ASSERT_TRUE(strings_equal_after_first_lines(output_mtl_str, golden_mtl_str));
      BLI_delete(out_mtl_file_path.c_str(), false, false);
    }
  }
};

TEST_F(obj_exporter_regression_test, all_tris)
{
  OBJExportParamsDefault _export;
  compare_obj_export_to_golden("io_tests/blend_geometry/all_tris.blend",
                               "io_tests/obj/all_tris.obj",
                               "io_tests/obj/all_tris.mtl",
                               _export.params);
}

TEST_F(obj_exporter_regression_test, all_quads)
{
  OBJExportParamsDefault _export;
  _export.params.scaling_factor = 2.0f;
  _export.params.export_materials = false;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/all_quads.blend", "io_tests/obj/all_quads.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, fgons)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/fgons.blend", "io_tests/obj/fgons.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, edges)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/edges.blend", "io_tests/obj/edges.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, vertices)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/vertices.blend", "io_tests/obj/vertices.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, nurbs_as_nurbs)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = true;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/nurbs.blend", "io_tests/obj/nurbs.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, nurbs_curves_as_nurbs)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = true;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/nurbs_curves.blend", "io_tests/obj/nurbs_curves.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, nurbs_as_mesh)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  _export.params.export_curves_as_nurbs = false;
  compare_obj_export_to_golden(
      "io_tests/blend_geometry/nurbs.blend", "io_tests/obj/nurbs_mesh.obj", "", _export.params);
}

TEST_F(obj_exporter_regression_test, cube_all_data_triangulated)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  _export.params.export_triangulated_mesh = true;
  compare_obj_export_to_golden("io_tests/blend_geometry/cube_all_data.blend",
                               "io_tests/obj/cube_all_data_triangulated.obj",
                               "",
                               _export.params);
}

TEST_F(obj_exporter_regression_test, cube_normal_edit)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  compare_obj_export_to_golden("io_tests/blend_geometry/cube_normal_edit.blend",
                               "io_tests/obj/cube_normal_edit.obj",
                               "",
                               _export.params);
}

TEST_F(obj_exporter_regression_test, suzanne_all_data)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_materials = false;
  _export.params.export_smooth_groups = true;
  compare_obj_export_to_golden("io_tests/blend_geometry/suzanne_all_data.blend",
                               "io_tests/obj/suzanne_all_data.obj",
                               "",
                               _export.params);
}

TEST_F(obj_exporter_regression_test, all_objects)
{
  OBJExportParamsDefault _export;
  _export.params.forward_axis = OBJ_AXIS_Y_FORWARD;
  _export.params.up_axis = OBJ_AXIS_Z_UP;
  _export.params.export_smooth_groups = true;
  compare_obj_export_to_golden("io_tests/blend_scene/all_objects.blend",
                               "io_tests/obj/all_objects.obj",
                               "io_tests/obj/all_objects.mtl",
                               _export.params);
}

}  // namespace blender::io::obj
