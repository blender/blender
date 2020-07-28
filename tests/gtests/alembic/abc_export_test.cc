#include "testing/testing.h"

// Keep first since utildefines defines AT which conflicts with STL
#include "exporter/abc_archive.h"
#include "intern/abc_util.h"

#include "BKE_main.h"
#include "BLI_fileops.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"

namespace blender {
namespace io {
namespace alembic {

class AlembicExportTest : public testing::Test {
 protected:
  ABCArchive *abc_archive;

  AlembicExportParams params;
  Scene scene;
  Depsgraph *depsgraph;
  Main *bmain;

  virtual void SetUp()
  {
    abc_archive = nullptr;

    /* Fake a 25 FPS scene with a nonzero base (because that's sometimes forgotten) */
    scene.r.frs_sec = 50;
    scene.r.frs_sec_base = 2;
    strcpy(scene.id.name, "SCTestScene");

    bmain = BKE_main_new();

    /* TODO(sergey): Pass scene layer somehow? */
    ViewLayer *view_layer = (ViewLayer *)scene.view_layers.first;
    depsgraph = DEG_graph_new(bmain, &scene, view_layer, DAG_EVAL_RENDER);
  }

  virtual void TearDown()
  {
    BKE_main_free(bmain);
    DEG_graph_free(depsgraph);
    deleteArchive();
  }

  // Call after setting up the parameters.
  void createArchive()
  {
    if (abc_archive != nullptr) {
      deleteArchive();
    }
    abc_archive = new ABCArchive(bmain, &scene, params, "somefile.abc");
  }

  void deleteArchive()
  {
    delete abc_archive;
    if (BLI_exists("somefile.abc")) {
      BLI_delete("somefile.abc", false, false);
    }
    abc_archive = nullptr;
  }
};

TEST_F(AlembicExportTest, TimeSamplesFullShutterUniform)
{
  /* Test 5 samples per frame, for 2 frames. */
  params.shutter_open = 0.0;
  params.shutter_close = 1.0;
  params.frame_start = 31.0;
  params.frame_end = 32.0;
  params.frame_samples_xform = params.frame_samples_shape = 5;
  createArchive();
  std::vector<double> frames(abc_archive->frames_begin(), abc_archive->frames_end());
  EXPECT_EQ(10, frames.size());
  EXPECT_NEAR(31.0, frames[0], 1e-5);
  EXPECT_NEAR(31.2, frames[1], 1e-5);
  EXPECT_NEAR(31.4, frames[2], 1e-5);
  EXPECT_NEAR(31.6, frames[3], 1e-5);
  EXPECT_NEAR(31.8, frames[4], 1e-5);
  EXPECT_NEAR(32.0, frames[5], 1e-5);
  EXPECT_NEAR(32.2, frames[6], 1e-5);
  EXPECT_NEAR(32.4, frames[7], 1e-5);
  EXPECT_NEAR(32.6, frames[8], 1e-5);
  EXPECT_NEAR(32.8, frames[9], 1e-5);

  for (double frame : frames) {
    EXPECT_TRUE(abc_archive->is_xform_frame(frame));
    EXPECT_TRUE(abc_archive->is_shape_frame(frame));
  }
}

TEST_F(AlembicExportTest, TimeSamplesFullShutterDifferent)
{
  /* Test 3 samples per frame for transforms, and 2 per frame for shapes, for 2 frames. */
  params.shutter_open = 0.0;
  params.shutter_close = 1.0;
  params.frame_start = 31.0;
  params.frame_end = 32.0;
  params.frame_samples_xform = 3;
  params.frame_samples_shape = 2;
  createArchive();
  std::vector<double> frames(abc_archive->frames_begin(), abc_archive->frames_end());
  EXPECT_EQ(8, frames.size());
  EXPECT_NEAR(31.0, frames[0], 1e-5);  // transform + shape
  EXPECT_TRUE(abc_archive->is_xform_frame(frames[0]));
  EXPECT_TRUE(abc_archive->is_shape_frame(frames[0]));
  EXPECT_NEAR(31.33333, frames[1], 1e-5);  // transform
  EXPECT_TRUE(abc_archive->is_xform_frame(frames[1]));
  EXPECT_FALSE(abc_archive->is_shape_frame(frames[1]));
  EXPECT_NEAR(31.5, frames[2], 1e-5);  // shape
  EXPECT_FALSE(abc_archive->is_xform_frame(frames[2]));
  EXPECT_TRUE(abc_archive->is_shape_frame(frames[2]));
  EXPECT_NEAR(31.66666, frames[3], 1e-5);  // transform
  EXPECT_TRUE(abc_archive->is_xform_frame(frames[3]));
  EXPECT_FALSE(abc_archive->is_shape_frame(frames[3]));
  EXPECT_NEAR(32.0, frames[4], 1e-5);  // transform + shape
  EXPECT_TRUE(abc_archive->is_xform_frame(frames[4]));
  EXPECT_TRUE(abc_archive->is_shape_frame(frames[4]));
  EXPECT_NEAR(32.33333, frames[5], 1e-5);  // transform
  EXPECT_TRUE(abc_archive->is_xform_frame(frames[5]));
  EXPECT_FALSE(abc_archive->is_shape_frame(frames[5]));
  EXPECT_NEAR(32.5, frames[6], 1e-5);  // shape
  EXPECT_FALSE(abc_archive->is_xform_frame(frames[6]));
  EXPECT_TRUE(abc_archive->is_shape_frame(frames[6]));
  EXPECT_NEAR(32.66666, frames[7], 1e-5);  // transform
  EXPECT_TRUE(abc_archive->is_xform_frame(frames[7]));
  EXPECT_FALSE(abc_archive->is_shape_frame(frames[7]));
}

TEST_F(AlembicExportTest, TimeSamples180degShutter)
{
  /* Test 5 samples per frame, for 2 frames. */
  params.shutter_open = -0.25;
  params.shutter_close = 0.25;
  params.frame_start = 31.0;
  params.frame_end = 32.0;
  params.frame_samples_xform = params.frame_samples_shape = 5;
  createArchive();
  std::vector<double> frames(abc_archive->frames_begin(), abc_archive->frames_end());
  EXPECT_EQ(10, frames.size());
  EXPECT_NEAR(31 - 0.25, frames[0], 1e-5);
  EXPECT_NEAR(31 - 0.15, frames[1], 1e-5);
  EXPECT_NEAR(31 - 0.05, frames[2], 1e-5);
  EXPECT_NEAR(31 + 0.05, frames[3], 1e-5);
  EXPECT_NEAR(31 + 0.15, frames[4], 1e-5);
  EXPECT_NEAR(32 - 0.25, frames[5], 1e-5);
  EXPECT_NEAR(32 - 0.15, frames[6], 1e-5);
  EXPECT_NEAR(32 - 0.05, frames[7], 1e-5);
  EXPECT_NEAR(32 + 0.05, frames[8], 1e-5);
  EXPECT_NEAR(32 + 0.15, frames[9], 1e-5);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
