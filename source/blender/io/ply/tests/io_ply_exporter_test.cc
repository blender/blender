/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BLI_color_types.hh"
#include "BLI_fileops.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_string.hh"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"

#include "DEG_depsgraph.hh"

#include "IO_ply.hh"
#include "intern/ply_data.hh"

#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_export_load_plydata.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"
#include "ply_import.hh"
#include "ply_import_buffer.hh"
#include "ply_import_data.hh"

#include <fstream>

namespace blender::io::ply {

class PLYExportTest : public BlendfileLoadingBaseTest {
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

    BKE_tempdir_init(nullptr);
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

  if (params.export_normals) {
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
  }

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
  char *buffer = BLI_file_read_text_as_mem(file_path.c_str(), 0, &buffer_len);
  if (buffer != nullptr) {
    res.assign(buffer, buffer_len);
    MEM_delete(buffer);
  }
  return res;
}

static char read(std::ifstream &file)
{
  char return_val;
  file.read(&return_val, sizeof(return_val));
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

TEST_F(PLYExportTest, WriteHeaderAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = true;
  _params.export_normals = false;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_header(*buffer, *plyData, _params);

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

TEST_F(PLYExportTest, WriteHeaderBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = false;
  _params.export_normals = false;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_header(*buffer, *plyData, _params);

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

TEST_F(PLYExportTest, WriteVerticesAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = true;
  _params.export_normals = false;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_vertices(*buffer, *plyData);

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

TEST_F(PLYExportTest, WriteVerticesBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = false;
  _params.export_normals = false;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_vertices(*buffer, *plyData);

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

TEST_F(PLYExportTest, WriteFacesAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = true;
  _params.export_normals = false;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_faces(*buffer, *plyData);

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

TEST_F(PLYExportTest, WriteFacesBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = false;
  _params.export_normals = false;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_faces(*buffer, *plyData);

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

TEST_F(PLYExportTest, WriteVertexNormalsAscii)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = true;
  _params.export_normals = true;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_vertices(*buffer, *plyData);

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

TEST_F(PLYExportTest, WriteVertexNormalsBinary)
{
  std::string filePath = get_temp_ply_filename(temp_file_path);
  PLYExportParams _params;
  _params.ascii_format = false;
  _params.export_normals = true;
  _params.vertex_colors = ePLYVertexColorMode::None;
  STRNCPY(_params.filepath, filePath.c_str());

  std::unique_ptr<PlyData> plyData = load_cube(_params);

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_vertices(*buffer, *plyData);

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

TEST_F(PLYExportTest, WriteByteColorsRoundedAndClamped)
{
  const std::string filepath = get_temp_ply_filename(temp_file_path);
  PlyData data;
  data.vertices.append({0, 0, 0});
  data.vertex_colors.append({-0.1f, 0.5f, 1.1f, 0.5f});

  FileBufferAscii buffer(filepath.c_str());
  write_vertices(buffer, data);
  buffer.close_file();

  EXPECT_EQ(read_temp_file_in_string(filepath), "0 0 0 0 128 255 128\n");
}

TEST_F(PLYExportTest, WriteFloatColors)
{
  const std::string filepath = get_temp_ply_filename(temp_file_path);
  PlyData data;
  data.vertices.append({0, 0, 0});
  data.vertex_colors.append({-0.25f, 0.5f, 2.0f, 1.5f});
  data.vertex_colors_are_float = true;

  PLYExportParams params;
  params.ascii_format = true;
  FileBufferAscii buffer(filepath.c_str());
  write_header(buffer, data, params);
  write_vertices(buffer, data);
  buffer.close_file();

  const std::string result = read_temp_file_in_string(filepath);
  EXPECT_NE(result.find("property float red\nproperty float green\nproperty float blue\n"
                        "property float alpha\n"),
            std::string::npos);
  EXPECT_NE(result.find("end_header\n0 0 0 -0.25 0.5 2 1.5\n"), std::string::npos);
}

TEST_F(PLYExportTest, WriteIntegerAttribute)
{
  const std::string filepath = get_temp_ply_filename(temp_file_path);
  PlyData data;
  data.vertices.append({0, 0, 0});
  PlyCustomAttribute attr("id", 1, CPPType::get<int32_t>());
  attr.data_for_write().typed<int32_t>()[0] = 16777217; /* Can' be represented as a float. */
  data.vertex_custom_attr.append(attr);

  PLYExportParams params;
  params.ascii_format = true;
  FileBufferAscii buffer(filepath.c_str());
  write_header(buffer, data, params);
  write_vertices(buffer, data);
  buffer.close_file();

  const std::string result = read_temp_file_in_string(filepath);
  EXPECT_NE(result.find("property int id\n"), std::string::npos);
  EXPECT_NE(result.find("end_header\n0 0 0 16777217\n"), std::string::npos);
}

TEST_F(PLYExportTest, ByteColorAttributeRoundTrip)
{
  const ColorGeometry4b colors[] = {{10, 20, 30, 40}, {250, 128, 0, 255}};
  PlyData data;
  data.vertices.append({0, 0, 0});
  data.vertices.append({1, 0, 0});
  PlyCustomAttribute color_attr("col", 2, CPPType::get<ColorGeometry4b>());
  color_attr.data_for_write().typed<ColorGeometry4b>().copy_from(colors);
  data.vertex_custom_attr.append(color_attr);

  /* A float attribute with a color-like suffix must stay a float attribute. */
  PlyCustomAttribute float_attr("foo_r", 2);
  float_attr.data_for_write().typed<float>()[1] = 0.5f;
  data.vertex_custom_attr.append(float_attr);

  for (const bool ascii : {true, false}) {
    const std::string filepath = get_temp_ply_filename(temp_file_path);
    PLYExportParams params;
    params.ascii_format = ascii;
    std::unique_ptr<FileBuffer> buffer;
    if (ascii) {
      buffer = std::make_unique<FileBufferAscii>(filepath.c_str());
    }
    else {
      buffer = std::make_unique<FileBufferBinary>(filepath.c_str());
    }
    write_header(*buffer, data, params);
    write_vertices(*buffer, data);
    buffer->close_file();

    PlyReadBuffer infile(filepath.c_str());
    PlyHeader header;
    ASSERT_EQ(read_header(infile, header), nullptr);
    const std::unique_ptr<PlyData> imported = import_ply_data(infile, header);
    ASSERT_TRUE(imported->error.empty());
    ASSERT_EQ(imported->vertex_custom_attr.size(), 2);

    const PlyCustomAttribute &imported_color = imported->vertex_custom_attr[0];
    EXPECT_EQ(imported_color.name, "col");
    ASSERT_TRUE(imported_color.type().is<ColorGeometry4b>());
    EXPECT_EQ_SPAN<ColorGeometry4b>(Span<ColorGeometry4b>(colors),
                                    imported_color.data().typed<ColorGeometry4b>());

    const PlyCustomAttribute &imported_float = imported->vertex_custom_attr[1];
    EXPECT_EQ(imported_float.name, "foo_r");
    ASSERT_TRUE(imported_float.type().is<float>());
    EXPECT_EQ(imported_float.data().typed<float>()[1], 0.5f);
  }
}

static PlyData typed_attributes_ply_data()
{
  PlyData data;
  data.vertices.append({0, 0, 0});
  PlyCustomAttribute attr_i8("a", 1, CPPType::get<int8_t>());
  attr_i8.data_for_write().typed<int8_t>()[0] = -5;
  data.vertex_custom_attr.append(attr_i8);
  PlyCustomAttribute attr_s2("b", 1, CPPType::get<short2>());
  attr_s2.data_for_write().typed<short2>()[0] = short2(-300, 7);
  data.vertex_custom_attr.append(attr_s2);
  PlyCustomAttribute attr_bool("c", 1, CPPType::get<bool>());
  attr_bool.data_for_write().typed<bool>()[0] = true;
  data.vertex_custom_attr.append(attr_bool);
  PlyCustomAttribute attr_quat("q", 1, CPPType::get<math::Quaternion>());
  attr_quat.data_for_write().typed<math::Quaternion>()[0] = math::Quaternion(1, 2, 3, 4);
  data.vertex_custom_attr.append(attr_quat);
  return data;
}

TEST_F(PLYExportTest, WriteTypedAttributesAscii)
{
  const std::string filepath = get_temp_ply_filename(temp_file_path);
  const PlyData data = typed_attributes_ply_data();
  PLYExportParams params;
  params.ascii_format = true;
  FileBufferAscii buffer(filepath.c_str());
  write_header(buffer, data, params);
  write_vertices(buffer, data);
  buffer.close_file();

  const std::string result = read_temp_file_in_string(filepath);
  EXPECT_NE(result.find("property char a\n"
                        "property short b_x\n"
                        "property short b_y\n"
                        "property uchar c\n"
                        "property float q_x\n"
                        "property float q_y\n"
                        "property float q_z\n"
                        "property float q_w\n"),
            std::string::npos);
  /* Quaternion is (w=1, x=2, y=3, z=4), written as x, y, z, w. */
  EXPECT_NE(result.find("end_header\n0 0 0 -5 -300 7 1 2 3 4 1\n"), std::string::npos);
}

TEST_F(PLYExportTest, WriteTypedAttributesBinary)
{
  const std::string filepath = get_temp_ply_filename(temp_file_path);
  const PlyData data = typed_attributes_ply_data();
  PLYExportParams params;
  params.ascii_format = false;
  FileBufferBinary buffer(filepath.c_str());
  write_header(buffer, data, params);
  write_vertices(buffer, data);
  buffer.close_file();

  std::vector<char> expected;
  auto append = [&](const auto value) {
    const char *bytes = reinterpret_cast<const char *>(&value);
    expected.insert(expected.end(), bytes, bytes + sizeof(value));
  };
  append(float3(0, 0, 0));
  append(int8_t(-5));
  append(int16_t(-300));
  append(int16_t(7));
  append(uint8_t(1));
  append(float4(2, 3, 4, 1));

  const std::vector<char> result = read_temp_file_in_vectorchar(filepath);
  ASSERT_GE(result.size(), expected.size());
  EXPECT_EQ(std::vector<char>(result.end() - expected.size(), result.end()), expected);
}

TEST_F(PLYExportTest, AppendCustomAttributeMergesAcrossMeshes)
{
  Vector<PlyCustomAttribute> attrs;
  const int map_a[] = {1, 0};
  const int map_b[] = {0};
  append_custom_attribute(
      "id", VArray<int>::from_container(Vector<int>{10, 20}), map_a, 0, "A", nullptr, attrs);
  append_custom_attribute(
      "id", VArray<int>::from_container(Vector<int>{30}), map_b, 2, "B", nullptr, attrs);
  /* Mesh without the attribute: will get filled with zeroes. */
  attrs[0].resize(4);
  ASSERT_EQ(attrs.size(), 1);
  ASSERT_TRUE(attrs[0].type().is<int>());
  EXPECT_EQ_SPAN<int>(Span<int>({20, 10, 30, 0}), attrs[0].data().typed<int>());
}

TEST_F(PLYExportTest, AppendCustomAttributeSkipsTypeMismatch)
{
  Vector<PlyCustomAttribute> attrs;
  const int map[] = {0};
  append_custom_attribute(
      "foo", VArray<int>::from_container(Vector<int>{7}), map, 0, "A", nullptr, attrs);
  append_custom_attribute(
      "foo", VArray<float>::from_container(Vector<float>{0.5f}), map, 1, "B", nullptr, attrs);
  attrs[0].resize(2);
  ASSERT_EQ(attrs.size(), 1);
  ASSERT_TRUE(attrs[0].type().is<int>());
  EXPECT_EQ_SPAN<int>(Span<int>({7, 0}), attrs[0].data().typed<int>());
}

TEST_F(PLYExportTest, AppendCustomAttributeSkipsDuplicatePropertyNames)
{
  Vector<PlyCustomAttribute> attrs;
  const int map[] = {0};
  append_custom_attribute("foo",
                          VArray<float3>::from_container(Vector<float3>{float3(1, 2, 3)}),
                          map,
                          0,
                          "A",
                          nullptr,
                          attrs);
  /* Would also write a "foo_x" property. */
  append_custom_attribute(
      "foo_x", VArray<float>::from_container(Vector<float>{4}), map, 0, "A", nullptr, attrs);
  /* Would clash with the always written position property. */
  append_custom_attribute(
      "x", VArray<float>::from_container(Vector<float>{5}), map, 0, "A", nullptr, attrs);
  /* Optional standard properties are only checked once all meshes are loaded. */
  append_custom_attribute(
      "red", VArray<float>::from_container(Vector<float>{5}), map, 0, "A", nullptr, attrs);
  /* A scalar named like a component of an existing vector attribute, added before it. */
  append_custom_attribute(
      "bar_y", VArray<float>::from_container(Vector<float>{6}), map, 0, "A", nullptr, attrs);
  append_custom_attribute("bar",
                          VArray<float2>::from_container(Vector<float2>{float2(7, 8)}),
                          map,
                          0,
                          "A",
                          nullptr,
                          attrs);
  ASSERT_EQ(attrs.size(), 3);
  EXPECT_EQ(attrs[0].name, "foo");
  EXPECT_EQ(attrs[1].name, "red");
  EXPECT_EQ(attrs[2].name, "bar_y");
}

class PLYExportPLYDataTest : public PLYExportTest {
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

TEST_F(PLYExportPLYDataTest, CubeLoadPLYData)
{
  PLYExportParams params;
  params.export_uv = false;
  PlyData plyData = load_ply_data_from_blendfile("io_tests/blend_geometry/cube_all_data.blend",
                                                 params);
  EXPECT_EQ(plyData.vertices.size(), 8);
  EXPECT_EQ(plyData.uv_coordinates.size(), 0);
}
TEST_F(PLYExportPLYDataTest, CubeLoadPLYDataUV)
{
  PLYExportParams params;
  params.export_uv = true;
  PlyData plyData = load_ply_data_from_blendfile("io_tests/blend_geometry/cube_all_data.blend",
                                                 params);
  EXPECT_EQ(plyData.vertices.size(), 8);
  EXPECT_EQ(plyData.uv_coordinates.size(), 8);
}
TEST_F(PLYExportPLYDataTest, CubeLooseEdgesLoadPLYData)
{
  PLYExportParams params;
  params.export_uv = false;
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
TEST_F(PLYExportPLYDataTest, CubeLooseEdgesLoadPLYDataUV)
{
  PLYExportParams params;
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

TEST_F(PLYExportPLYDataTest, CubesVertexAttrs)
{
  PLYExportParams params;
  params.export_uv = true;
  params.export_attributes = true;
  PlyData plyData = load_ply_data_from_blendfile(
      "io_tests/blend_geometry/cubes_vertex_attrs.blend", params);
  EXPECT_EQ(plyData.vertices.size(), 28);
  EXPECT_EQ(plyData.vertex_custom_attr.size(), 4); /* Float, Color, ByteColor, Int2D. */
  EXPECT_EQ(custom_attribute_properties(plyData).size(), 11);
  EXPECT_EQ(plyData.vertex_custom_attr[0].size(), 28);
}

TEST_F(PLYExportPLYDataTest, SuzanneLoadPLYDataUV)
{
  PLYExportParams params;
  params.export_uv = true;
  PlyData plyData = load_ply_data_from_blendfile("io_tests/blend_geometry/suzanne_all_data.blend",
                                                 params);
  EXPECT_EQ(plyData.uv_coordinates.size(), 542);
}

}  // namespace blender::io::ply
