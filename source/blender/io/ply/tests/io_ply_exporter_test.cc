/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BLI_string.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"

#include "DEG_depsgraph.h"

#include "IO_ply.h"
#include "intern/ply_data.hh"

#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_export_load_plydata.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"

#include <fstream>

namespace blender::io::ply {

class ply_export_test : public BlendfileLoadingBaseTest {
 public:
  bool load_file_and_depsgraph(const std::string &filepath,
                               const eEvaluationMode eval_mode = DAG_EVAL_VIEWPORT)
  {
    if (!blendfile_load(filepath.c_str())) {
      return false;
    }
    depsgraph_create(eval_mode);
    return true;
  }

 protected:
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

  std::string get_temp_ply_filename(const std::string &filename)
  {
    return std::string(BKE_tempdir_session()) + SEP_STR + filename;
  }
};

static std::unique_ptr<PlyData> load_cube(PLYExportParams &params)
{
  std::unique_ptr<PlyData> plyData = std::make_unique<PlyData>();
  plyData->vertices = {
      {1.122082, 1.122082, 1.122082},
      {1.122082, 1.122082, -1.122082},
      {1.122082, -1.122082, 1.122082},
      {1.122082, -1.122082, -1.122082},
      {-1.122082, 1.122082, 1.122082},
      {-1.122082, 1.122082, -1.122082},
      {-1.122082, -1.122082, 1.122082},
      {-1.122082, -1.122082, -1.122082},
  };

  plyData->face_sizes = {4, 4, 4, 4, 4, 4};
  plyData->face_vertices = {0, 2, 6, 4, 3, 7, 6, 2, 7, 5, 4, 6,
                            5, 7, 3, 1, 1, 3, 2, 0, 5, 1, 0, 4};

  if (params.export_normals)
    plyData->vertex_normals = {
        {-0.5773503, -0.5773503, -0.5773503},
        {-0.5773503, -0.5773503, 0.5773503},
        {-0.5773503, 0.5773503, -0.5773503},
        {-0.5773503, 0.5773503, 0.5773503},
        {0.5773503, -0.5773503, -0.5773503},
        {0.5773503, -0.5773503, 0.5773503},
        {0.5773503, 0.5773503, -0.5773503},
        {0.5773503, 0.5773503, 0.5773503},
    };

  return plyData;
}

/* The following is relative to BKE_tempdir_base.
 * Use Latin Capital Letter A with Ogonek, Cyrillic Capital Letter Zhe
 * at the end, to test I/O on non-English file names. */
const char *const temp_file_path = "output\xc4\x84\xd0\x96.ply";

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

static char read(std::ifstream &file)
{
  char return_val;
  file.read((char *)&return_val, sizeof(return_val));
  return return_val;
}

static std::vector<char> read_temp_file_in_vectorchar(const std::string &file_path)
{
  std::vector<char> res;
  std::ifstream infile(file_path, std::ios::binary);
  while (true) {
    uint64_t c = read(infile);
    if (!infile.eof()) {
      res.push_back(c);
    }
    else {
      break;
    }
  }
  return res;
}

TEST_F(ply_export_test, WriteHeaderAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = true;
  _params.export_normals = false;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_header(*buffer.get(), *plyData.get(), _params);

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  StringRef version = BKE_blender_version_string();

  std::string expected =
      "ply\n"
      "format ascii 1.0\n"
      "comment Created in Blender version " +
      version +
      "\n"
      "element vertex 8\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "element face 6\n"
      "property list uchar uint vertex_indices\n"
      "end_header\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(ply_export_test, WriteHeaderBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = false;
  _params.export_normals = false;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_header(*buffer.get(), *plyData.get(), _params);

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  StringRef version = BKE_blender_version_string();

  std::string expected =
      "ply\n"
      "format binary_little_endian 1.0\n"
      "comment Created in Blender version " +
      version +
      "\n"
      "element vertex 8\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "element face 6\n"
      "property list uchar uint vertex_indices\n"
      "end_header\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(ply_export_test, WriteVerticesAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = true;
  _params.export_normals = false;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_vertices(*buffer.get(), *plyData.get());

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  std::string expected =
      "1.122082 1.122082 1.122082\n"
      "1.122082 1.122082 -1.122082\n"
      "1.122082 -1.122082 1.122082\n"
      "1.122082 -1.122082 -1.122082\n"
      "-1.122082 1.122082 1.122082\n"
      "-1.122082 1.122082 -1.122082\n"
      "-1.122082 -1.122082 1.122082\n"
      "-1.122082 -1.122082 -1.122082\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(ply_export_test, WriteVerticesBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = false;
  _params.export_normals = false;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_vertices(*buffer.get(), *plyData.get());

  buffer->close_file();

  std::vector<char> result = read_temp_file_in_vectorchar(filePath);

  std::vector<char> expected({
      0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0,
      0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F,
      0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0,
      0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F,
      0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0,
      0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F,
      0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF,
  });

  ASSERT_EQ(result.size(), expected.size());

  for (int i = 0; i < result.size(); i++) {
    ASSERT_EQ(result[i], expected[i]);
  }
}

TEST_F(ply_export_test, WriteFacesAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = true;
  _params.export_normals = false;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_faces(*buffer.get(), *plyData.get());

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  std::string expected =
      "4 0 2 6 4\n"
      "4 3 7 6 2\n"
      "4 7 5 4 6\n"
      "4 5 7 3 1\n"
      "4 1 3 2 0\n"
      "4 5 1 0 4\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(ply_export_test, WriteFacesBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = false;
  _params.export_normals = false;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_faces(*buffer.get(), *plyData.get());

  buffer->close_file();

  std::vector<char> result = read_temp_file_in_vectorchar(filePath);

  std::vector<char> expected({
      0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x04, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
      0x02, 0x00, 0x00, 0x00, 0x04, 0x07, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x05, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
      0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00,
      0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x05, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  });

  ASSERT_EQ(result.size(), expected.size());

  for (int i = 0; i < result.size(); i++) {
    ASSERT_EQ(result[i], expected[i]);
  }
}

TEST_F(ply_export_test, WriteVertexNormalsAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = true;
  _params.export_normals = true;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_vertices(*buffer.get(), *plyData.get());

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  std::string expected =
      "1.122082 1.122082 1.122082 -0.5773503 -0.5773503 -0.5773503\n"
      "1.122082 1.122082 -1.122082 -0.5773503 -0.5773503 0.5773503\n"
      "1.122082 -1.122082 1.122082 -0.5773503 0.5773503 -0.5773503\n"
      "1.122082 -1.122082 -1.122082 -0.5773503 0.5773503 0.5773503\n"
      "-1.122082 1.122082 1.122082 0.5773503 -0.5773503 -0.5773503\n"
      "-1.122082 1.122082 -1.122082 0.5773503 -0.5773503 0.5773503\n"
      "-1.122082 -1.122082 1.122082 0.5773503 0.5773503 -0.5773503\n"
      "-1.122082 -1.122082 -1.122082 0.5773503 0.5773503 0.5773503\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(ply_export_test, WriteVertexNormalsBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params = {};
  _params.ascii_format = false;
  _params.export_normals = true;
  _params.vertex_colors = PLY_VERTEX_COLOR_NONE;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_vertices(*buffer.get(), *plyData.get());

  buffer->close_file();

  std::vector<char> result = read_temp_file_in_vectorchar(filePath);

  std::vector<char> expected({
      0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x3B, 0xCD, 0x13,
      0xBF, 0x3B, 0xCD, 0x13, 0xBF, 0x3B, 0xCD, 0x13, 0xBF, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0,
      0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x3B, 0xCD, 0x13, 0xBF, 0x3B, 0xCD, 0x13, 0xBF, 0x3B,
      0xCD, 0x13, 0x3F, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F,
      0x3B, 0xCD, 0x13, 0xBF, 0x3B, 0xCD, 0x13, 0x3F, 0x3B, 0xCD, 0x13, 0xBF, 0x62, 0xA0, 0x8F,
      0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x3B, 0xCD, 0x13, 0xBF, 0x3B, 0xCD,
      0x13, 0x3F, 0x3B, 0xCD, 0x13, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F, 0x62,
      0xA0, 0x8F, 0x3F, 0x3B, 0xCD, 0x13, 0x3F, 0x3B, 0xCD, 0x13, 0xBF, 0x3B, 0xCD, 0x13, 0xBF,
      0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x3B, 0xCD, 0x13,
      0x3F, 0x3B, 0xCD, 0x13, 0xBF, 0x3B, 0xCD, 0x13, 0x3F, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0,
      0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0x3F, 0x3B, 0xCD, 0x13, 0x3F, 0x3B, 0xCD, 0x13, 0x3F, 0x3B,
      0xCD, 0x13, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF, 0x62, 0xA0, 0x8F, 0xBF,
      0x3B, 0xCD, 0x13, 0x3F, 0x3B, 0xCD, 0x13, 0x3F, 0x3B, 0xCD, 0x13, 0x3F,
  });

  ASSERT_EQ(result.size(), expected.size());

  for (int i = 0; i < result.size(); i++) {
    ASSERT_EQ(result[i], expected[i]);
  }
}

class ply_exporter_ply_data_test : public ply_export_test {
 public:
  PlyData load_ply_data_from_blendfile(const std::string &blendfile, PLYExportParams &params)
  {
    PlyData data;
    if (!load_file_and_depsgraph(blendfile)) {
      return data;
    }

    load_plydata(data, depsgraph, params);

    return data;
  }
};

TEST_F(ply_exporter_ply_data_test, CubeLoadPLYData)
{
  PLYExportParams params = {};
  PlyData plyData = load_ply_data_from_blendfile("io_tests/blend_geometry/cube_all_data.blend",
                                                 params);
  EXPECT_EQ(plyData.vertices.size(), 8);
  EXPECT_EQ(plyData.uv_coordinates.size(), 0);
}
TEST_F(ply_exporter_ply_data_test, CubeLoadPLYDataUV)
{
  PLYExportParams params = {};
  params.export_uv = true;
  PlyData plyData = load_ply_data_from_blendfile("io_tests/blend_geometry/cube_all_data.blend",
                                                 params);
  EXPECT_EQ(plyData.vertices.size(), 8);
  EXPECT_EQ(plyData.uv_coordinates.size(), 8);
}
TEST_F(ply_exporter_ply_data_test, CubeLooseEdgesLoadPLYData)
{
  PLYExportParams params = {};
  params.forward_axis = IO_AXIS_Y;
  params.up_axis = IO_AXIS_Z;
  params.global_scale = 1.0f;
  PlyData plyData = load_ply_data_from_blendfile(
      "io_tests/blend_geometry/cube_loose_edges_verts.blend", params);
  float3 exp_vertices[] = {
      {1, 1, 1},
      {1, 1, -1},
      {1, -1, 1},
      {1, -1, -1},
      {-1, 1, 1},
      {-1, 1, -1},
      {-1, -1, 1},
      {-1, -1, -1},
  };
  std::pair<int, int> exp_edges[] = {{7, 6}, {6, 4}};
  uint32_t exp_face_sizes[] = {4, 4};
  uint32_t exp_faces[] = {5, 1, 3, 7, 5, 4, 0, 1};
  EXPECT_EQ(plyData.vertices.size(), ARRAY_SIZE(exp_vertices));
  EXPECT_EQ(plyData.uv_coordinates.size(), 0);
  EXPECT_EQ(plyData.edges.size(), ARRAY_SIZE(exp_edges));
  EXPECT_EQ(plyData.face_sizes.size(), ARRAY_SIZE(exp_face_sizes));
  EXPECT_EQ(plyData.face_vertices.size(), ARRAY_SIZE(exp_faces));
  EXPECT_EQ_ARRAY(exp_vertices, plyData.vertices.data(), ARRAY_SIZE(exp_vertices));
  EXPECT_EQ_ARRAY(exp_edges, plyData.edges.data(), ARRAY_SIZE(exp_edges));
  EXPECT_EQ_ARRAY(exp_face_sizes, plyData.face_sizes.data(), ARRAY_SIZE(exp_face_sizes));
  EXPECT_EQ_ARRAY(exp_faces, plyData.face_vertices.data(), ARRAY_SIZE(exp_faces));
}
TEST_F(ply_exporter_ply_data_test, CubeLooseEdgesLoadPLYDataUV)
{
  PLYExportParams params = {};
  params.forward_axis = IO_AXIS_Y;
  params.up_axis = IO_AXIS_Z;
  params.global_scale = 1.0f;
  params.export_uv = true;
  PlyData plyData = load_ply_data_from_blendfile(
      "io_tests/blend_geometry/cube_loose_edges_verts.blend", params);
  float3 exp_vertices[] = {
      {-1, 1, -1},
      {1, 1, -1},
      {1, -1, -1},
      {-1, -1, -1},
      {-1, 1, -1},
      {-1, 1, 1},
      {1, 1, 1},
      {1, -1, 1},
      {-1, -1, 1},
  };
  float2 exp_uv[] = {
      {0.125f, 0.5f},
      {0.375f, 0.5f},
      {0.375f, 0.75f},
      {0.125f, 0.75f},
      {0.375f, 0.25f},
      {0.625f, 0.25f},
      {0.625f, 0.5f},
      {0, 0},
      {0, 0},
  };
  std::pair<int, int> exp_edges[] = {{3, 8}, {8, 5}};
  uint32_t exp_face_sizes[] = {4, 4};
  uint32_t exp_faces[] = {0, 1, 2, 3, 4, 5, 6, 1};
  EXPECT_EQ(plyData.vertices.size(), 9);
  EXPECT_EQ(plyData.uv_coordinates.size(), 9);
  EXPECT_EQ(plyData.edges.size(), ARRAY_SIZE(exp_edges));
  EXPECT_EQ(plyData.face_sizes.size(), ARRAY_SIZE(exp_face_sizes));
  EXPECT_EQ(plyData.face_vertices.size(), ARRAY_SIZE(exp_faces));
  EXPECT_EQ_ARRAY(exp_vertices, plyData.vertices.data(), ARRAY_SIZE(exp_vertices));
  EXPECT_EQ_ARRAY(exp_uv, plyData.uv_coordinates.data(), ARRAY_SIZE(exp_uv));
  EXPECT_EQ_ARRAY(exp_edges, plyData.edges.data(), ARRAY_SIZE(exp_edges));
  EXPECT_EQ_ARRAY(exp_face_sizes, plyData.face_sizes.data(), ARRAY_SIZE(exp_face_sizes));
  EXPECT_EQ_ARRAY(exp_faces, plyData.face_vertices.data(), ARRAY_SIZE(exp_faces));
}

TEST_F(ply_exporter_ply_data_test, SuzanneLoadPLYDataUV)
{
  PLYExportParams params = {};
  params.export_uv = true;
  PlyData plyData = load_ply_data_from_blendfile("io_tests/blend_geometry/suzanne_all_data.blend",
                                                 params);
  EXPECT_EQ(plyData.uv_coordinates.size(), 542);
}

}  // namespace blender::io::ply
