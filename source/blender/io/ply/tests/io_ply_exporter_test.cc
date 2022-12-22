#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BKE_blender_version.h"
#include "BKE_curve.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_fileops.h"
#include "BLI_math_vec_types.hh"

#include "BLO_readfile.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_curve_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "IO_ply.h"
#include "intern/ply_data.hh"

#include "ply_export_data.hh"
#include "ply_export_header.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"

#include <fstream>

namespace blender::io::ply {

class PlyExportTest : public BlendfileLoadingBaseTest {
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
};

std::unique_ptr<PlyData> load_cube()
{
  std::unique_ptr<PlyData> plyData = std::make_unique<PlyData>();
  plyData->vertices = {{1.122082, 1.122082, 1.122082},
                       {-1.122082, 1.122082, 1.122082},
                       {-1.122082, -1.122082, 1.122082},
                       {1.122082, -1.122082, 1.122082},
                       {1.122082, -1.122082, -1.122082},
                       {-1.122082, -1.122082, -1.122082},
                       {-1.122082, 1.122082, -1.122082},
                       {1.122082, 1.122082, -1.122082}};

  plyData->faces = {
      {0, 1, 2, 3}, {4, 3, 2, 5}, {5, 2, 1, 6}, {6, 7, 4, 5}, {7, 0, 3, 4}, {6, 1, 0, 7}};

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

char read(std::ifstream &file)
{
  char returnVal;
  file.read((char *)&returnVal, sizeof(returnVal));
  return returnVal;
}

static std::vector<char> read_temp_file_in_vectorchar(const std::string &file_path)
{
  std::vector<char> res;
  std::ifstream infile(file_path, std::ios::binary);
  while (true) {
    auto c = read(infile);
    if (!infile.eof()) {
      res.push_back(c);
    }
    else {
      break;
    }
  }
  return res;
}

TEST_F(PlyExportTest, WriteHeaderAscii)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  PLYExportParams _params;
  _params.ascii_format = true;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_cube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_header(buffer, plyData, _params);

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

TEST_F(PlyExportTest, WriteHeaderBinary)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  PLYExportParams _params;
  _params.ascii_format = false;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_cube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_header(buffer, plyData, _params);

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

TEST_F(PlyExportTest, WriteVerticesAscii)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  PLYExportParams _params;
  _params.ascii_format = true;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_cube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_vertices(buffer, plyData);

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  std::string expected =
      "1.122082 1.122082 1.122082\n"
      "-1.122082 1.122082 1.122082\n"
      "-1.122082 -1.122082 1.122082\n"
      "1.122082 -1.122082 1.122082\n"
      "1.122082 -1.122082 -1.122082\n"
      "-1.122082 -1.122082 -1.122082\n"
      "-1.122082 1.122082 -1.122082\n"
      "1.122082 1.122082 -1.122082\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(PlyExportTest, WriteVerticesBinary)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  PLYExportParams _params;
  _params.ascii_format = false;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_cube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_vertices(buffer, plyData);

  buffer->close_file();

  std::vector<char> result = read_temp_file_in_vectorchar(filePath);

  std::vector<char> expected(
      {(char)0x62, (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F,
       (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0,
       (char)0x8F, (char)0xBF, (char)0x62, (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62,
       (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF,
       (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF, (char)0x62, (char)0xA0, (char)0x8F,
       (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0,
       (char)0x8F, (char)0xBF, (char)0x62, (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62,
       (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF,
       (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF, (char)0x62, (char)0xA0, (char)0x8F,
       (char)0xBF, (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF, (char)0x62, (char)0xA0,
       (char)0x8F, (char)0xBF, (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF, (char)0x62,
       (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF,
       (char)0x62, (char)0xA0, (char)0x8F, (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F,
       (char)0x3F, (char)0x62, (char)0xA0, (char)0x8F, (char)0xBF});

  ASSERT_EQ(result.size(), expected.size());

  for (int i = 0; i < result.size(); i++) {
    ASSERT_EQ(result[i], expected[i]);
  }
}

TEST_F(PlyExportTest, WriteFacesAscii)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  PLYExportParams _params;
  _params.ascii_format = true;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_cube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_faces(buffer, plyData);

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  StringRef version = BKE_blender_version_string();

  std::string expected =
      "4 0 1 2 3\n"
      "4 4 3 2 5\n"
      "4 5 2 1 6\n"
      "4 6 7 4 5\n"
      "4 7 0 3 4\n"
      "4 6 1 0 7\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

TEST_F(PlyExportTest, WriteFacesBinary)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  PLYExportParams _params;
  _params.ascii_format = false;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_cube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferBinary>(_params.filepath);

  write_faces(buffer, plyData);

  buffer->close_file();

  std::vector<char> result = read_temp_file_in_vectorchar(filePath);

  std::vector<char> expected(
      {(char)0x04, (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x01, (char)0x00,
       (char)0x00, (char)0x00, (char)0x02, (char)0x00, (char)0x00, (char)0x00, (char)0x03,
       (char)0x00, (char)0x00, (char)0x00, (char)0x04, (char)0x04, (char)0x00, (char)0x00,
       (char)0x00, (char)0x03, (char)0x00, (char)0x00, (char)0x00, (char)0x02, (char)0x00,
       (char)0x00, (char)0x00, (char)0x05, (char)0x00, (char)0x00, (char)0x00, (char)0x04,
       (char)0x05, (char)0x00, (char)0x00, (char)0x00, (char)0x02, (char)0x00, (char)0x00,
       (char)0x00, (char)0x01, (char)0x00, (char)0x00, (char)0x00, (char)0x06, (char)0x00,
       (char)0x00, (char)0x00, (char)0x04, (char)0x06, (char)0x00, (char)0x00, (char)0x00,
       (char)0x07, (char)0x00, (char)0x00, (char)0x00, (char)0x04, (char)0x00, (char)0x00,
       (char)0x00, (char)0x05, (char)0x00, (char)0x00, (char)0x00, (char)0x04, (char)0x07,
       (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x00,
       (char)0x03, (char)0x00, (char)0x00, (char)0x00, (char)0x04, (char)0x00, (char)0x00,
       (char)0x00, (char)0x04, (char)0x06, (char)0x00, (char)0x00, (char)0x00, (char)0x01,
       (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x00, (char)0x00,
       (char)0x07, (char)0x00, (char)0x00, (char)0x00});

  ASSERT_EQ(result.size(), expected.size());

  for (int i = 0; i < result.size(); i++) {
    ASSERT_EQ(result[i], expected[i]);
  }
}

}  // namespace blender::io::ply
