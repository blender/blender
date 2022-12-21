#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BKE_curve.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_blender_version.h"

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
#include "ply_file_buffer_binary.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_export_header.hh"

namespace blender::io::ply {

class PlyExportTest : public BlendfileLoadingBaseTest {
 public:
  void export_and_check()
  {
  }

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

std::unique_ptr<PlyData> load_scube()
{
  std::unique_ptr<PlyData> plyData = std::make_unique<PlyData>();
  plyData->vertices = {{1, 1, -1},
                      {1, -1, -1},
                      {-1, -1, -1},
                      {-1, 1, -1},
                      {1, 0.999999, 1},
                      {-1, 1, 1},
                      {-1, -1, 1},
                      {0.999999, -1.000001, 1},
                      {1, 1, -1},
                      {1, 0.999999, 1},
                      {0.999999, -1.000001, 1},
                      {1, -1, -1},
                      {1, -1, -1},
                      {0.999999, -1.000001, 1},
                      {-1, -1, 1},
                      {-1, -1, -1},
                      {-1, -1, -1},
                      {-1, -1, 1},
                      {-1, 1, 1},
                      {-1, 1, -1},
                      {1, 0.999999, 1},
                      {1, 1, -1},
                      {-1, 1, -1},
                      {-1, 1, 1}};

  plyData->vertex_normals = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, 1},  {0, 0, 1},
                            {0, 0, 1},  {0, 0, 1},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},
                            {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {-1, 0, 0}, {-1, 0, 0},
                            {-1, 0, 0}, {-1, 0, 0}, {0, 1, 0},  {0, 1, 0},  {0, 1, 0},  {0, 1, 0}};

  plyData->vertex_colors = {{1, 0.8470588235294118, 0, 1},
                           {0, 0.011764705882352941, 1, 1},
                           {0, 0.011764705882352941, 1, 1},
                           {1, 0.8470588235294118, 0, 1},
                           {1, 0.8509803921568627, 0.08627450980392157, 1},
                           {1, 0.8470588235294118, 0, 1},
                           {0, 0.00392156862745098, 1, 1},
                           {0.00392156862745098, 0.00392156862745098, 1, 1},
                           {1, 0.8470588235294118, 0.01568627450980392, 1},
                           {1, 0.8509803921568627, 0.08627450980392157, 1},
                           {0.00392156862745098, 0.00392156862745098, 1, 1},
                           {0, 0.00392156862745098, 1, 1},
                           {0, 0.00392156862745098, 1, 1},
                           {0.00392156862745098, 0.00392156862745098, 1, 1},
                           {0, 0.00392156862745098, 1, 1},
                           {0, 0.00392156862745098, 1, 1},
                           {0, 0.011764705882352941, 1, 1},
                           {0, 0.00392156862745098, 1, 1},
                           {1, 0.8470588235294118, 0, 1},
                           {1, 0.8470588235294118, 0, 1},
                           {1, 0.8509803921568627, 0.08627450980392157, 1},
                           {1, 0.8470588235294118, 0, 1},
                           {1, 0.8470588235294118, 0, 1},
                           {1, 0.8470588235294118, 0, 1}};

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

TEST_F(PlyExportTest, WriteHeader)
{
  std::string filePath = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  printf(("FilePath: " + filePath + "\n\n").c_str());
  PLYExportParams _params;
  _params.ascii_format = false;
  BLI_strncpy(_params.filepath, filePath.c_str(), 1024);

  std::unique_ptr<PlyData> plyData = load_scube();

  std::unique_ptr<FileBuffer> buffer = std::make_unique<FileBufferAscii>(_params.filepath);

  write_header(buffer, plyData, _params);

  buffer->close_file();

  std::string result = read_temp_file_in_string(filePath);

  StringRef version = BKE_blender_version_string();

  std::string expected =
      "ply\n"
      "format ascii 1.0\n"
      "comment Created in Blender version " + version + "\n"
      "element vertex 24\n"
      "property float x\n"
      "property float y\n"
      "property float z\n"
      "element face 6\n"
      "property list uchar uint vertex_indices\n"
      "end_header\n";

  ASSERT_STREQ(result.c_str(), expected.c_str());
}

}  // namespace blender::io::ply
