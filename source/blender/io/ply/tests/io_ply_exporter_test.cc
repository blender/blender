#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_math_base.hh"
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

#include "intern/ply_data.hh"
#include "ply_export.hh"
#include "io_ply_exporter_test.hh"

namespace blender::io::ply {

enum PLYFileType { ASCII, BINARY };

class PlyExportTest : public BlendfileLoadingBaseTest {
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

TEST(ply_exporter_writer, header)
{
  /* Because testing doesn't fully initialize Blender, we need the following. */
  BKE_tempdir_init(nullptr);
  std::string out_file_path = blender::tests::flags_test_release_dir() + "/" + temp_file_path;
  {
    PLYExportParamsDefault _export;
    std::unique_ptr<FileBuffer> writer = init_writer(_export.params, out_file_path);
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
// TEST_F(PlyExportTest, PLYImportBunny)
// {
//   Expectation expect[] = {
//       {"OBCube", PLYFileType::ASCII, &cube, 8, 6, 12, float3(1, 1, -1), float3(-1, 1, 1)},
//       {"bunny2", PLYFileType::BINARY_LITTLE_ENDIAN, nullptr, 1623, 1000, 1513}};
//   import_and_check("bunny2.ply", expect, 2);
// }



}  // namespace blender::io::ply
