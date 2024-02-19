/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "tests/blendfile_loading_base_test.h"

#include "BKE_appdir.hh"
#include "BKE_main.hh"

#include "BLI_fileops.h"
#include "BLI_string.h"

#include "BLO_readfile.h"

#include "DEG_depsgraph.hh"

#include "IO_stl.hh"
#include "stl_export.hh"

namespace blender::io::stl {

/* Set this true to keep comparison-failing test output in temp file directory. */
constexpr bool save_failing_test_output = false;

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

class STLExportTest : public BlendfileLoadingBaseTest {
 public:
  bool load_file_and_depsgraph(const std::string &filepath)
  {
    if (!blendfile_load(filepath.c_str())) {
      return false;
    }
    depsgraph_create(DAG_EVAL_VIEWPORT);
    return true;
  }

 protected:
  STLExportTest()
  {
    _params = {};
    _params.forward_axis = IO_AXIS_Y;
    _params.up_axis = IO_AXIS_Z;
    _params.global_scale = 1.0f;
    _params.apply_modifiers = true;
    _params.ascii_format = true;
  }
  void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();
    BKE_tempdir_init("");
  }

  void TearDown() override
  {
    BlendfileLoadingBaseTest::TearDown();
    BKE_tempdir_session_purge();
  }

  static std::string get_temp_filename(const std::string &filename)
  {
    return std::string(BKE_tempdir_base()) + SEP_STR + filename;
  }

  /**
   * Export the given blend file with the given parameters and
   * test to see if it matches a golden file (ignoring any difference in Blender version number).
   * \param blendfile: input, relative to "tests" directory.
   * \param golden_obj: expected output, relative to "tests" directory.
   * \param params: the parameters to be used for export.
   */
  void compare_to_golden(const std::string &blendfile, const std::string &golden_stl)
  {
    if (!load_file_and_depsgraph(blendfile)) {
      return;
    }

    std::string out_file_path = get_temp_filename(BLI_path_basename(golden_stl.c_str()));
    STRNCPY(_params.filepath, out_file_path.c_str());
    std::string golden_file_path = blender::tests::flags_test_asset_dir() + SEP_STR + golden_stl;
    export_frame(depsgraph, 1.0f, _params);
    std::string output_str = read_temp_file_in_string(out_file_path);

    std::string golden_str = read_temp_file_in_string(golden_file_path);
    bool are_equal = output_str == golden_str;
    if (save_failing_test_output && !are_equal) {
      printf("failing test output in %s\n", out_file_path.c_str());
    }
    ASSERT_TRUE(are_equal);
    if (!save_failing_test_output || are_equal) {
      BLI_delete(out_file_path.c_str(), false, false);
    }
  }

  STLExportParams _params;
};

TEST_F(STLExportTest, all_tris)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "all_tris.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "all_tris.stl");
}

TEST_F(STLExportTest, all_quads)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "all_quads.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "all_quads.stl");
}

TEST_F(STLExportTest, non_uniform_scale)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "non_uniform_scale.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "non_uniform_scale.stl");
}

TEST_F(STLExportTest, cubes_positioned)
{
  compare_to_golden("io_tests" SEP_STR "blend_geometry" SEP_STR "cubes_positioned.blend",
                    "io_tests" SEP_STR "stl" SEP_STR "cubes_positioned.stl");
}

}  // namespace blender::io::stl
