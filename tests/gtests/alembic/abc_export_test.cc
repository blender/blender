#include "testing/testing.h"

// Keep first since utildefines defines AT which conflicts with STL
#include "exporter/abc_exporter.h"
#include "intern/abc_util.h"

extern "C" {
#include "BKE_main.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "DNA_scene_types.h"
}

#include "DEG_depsgraph.h"

class TestableAbcExporter : public AbcExporter {
 public:
  TestableAbcExporter(Main *bmain, const char *filename, ExportSettings &settings)
      : AbcExporter(bmain, filename, settings)
  {
  }

  void getShutterSamples(unsigned int nr_of_samples,
                         bool time_relative,
                         std::vector<double> &samples)
  {
    AbcExporter::getShutterSamples(nr_of_samples, time_relative, samples);
  }

  void getFrameSet(unsigned int nr_of_samples, std::set<double> &frames)
  {
    AbcExporter::getFrameSet(nr_of_samples, frames);
  }
};

class AlembicExportTest : public testing::Test {
 protected:
  ExportSettings settings;
  Scene scene;
  Depsgraph *depsgraph;
  TestableAbcExporter *exporter;
  Main *bmain;

  virtual void SetUp()
  {
    settings.frame_start = 31.0;
    settings.frame_end = 223.0;

    /* Fake a 25 FPS scene with a nonzero base (because that's sometimes forgotten) */
    scene.r.frs_sec = 50;
    scene.r.frs_sec_base = 2;

    bmain = BKE_main_new();

    /* TODO(sergey): Pass scene layer somehow? */
    ViewLayer *view_layer = (ViewLayer *)scene.view_layers.first;
    settings.depsgraph = depsgraph = DEG_graph_new(bmain, &scene, view_layer, DAG_EVAL_VIEWPORT);

    settings.scene = &scene;
    settings.view_layer = view_layer;

    exporter = NULL;
  }

  virtual void TearDown()
  {
    BKE_main_free(bmain);
    DEG_graph_free(depsgraph);
    delete exporter;
  }

  // Call after setting up the settings.
  void createExporter()
  {
    exporter = new TestableAbcExporter(bmain, "somefile.abc", settings);
  }
};

TEST_F(AlembicExportTest, TimeSamplesFullShutter)
{
  settings.shutter_open = 0.0;
  settings.shutter_close = 1.0;

  createExporter();
  std::vector<double> samples;

  /* test 5 samples per frame */
  exporter->getShutterSamples(5, true, samples);
  EXPECT_EQ(5, samples.size());
  EXPECT_NEAR(1.240, samples[0], 1e-5f);
  EXPECT_NEAR(1.248, samples[1], 1e-5f);
  EXPECT_NEAR(1.256, samples[2], 1e-5f);
  EXPECT_NEAR(1.264, samples[3], 1e-5f);
  EXPECT_NEAR(1.272, samples[4], 1e-5f);

  /* test same, but using frame number offset instead of time */
  exporter->getShutterSamples(5, false, samples);
  EXPECT_EQ(5, samples.size());
  EXPECT_NEAR(0.0, samples[0], 1e-5f);
  EXPECT_NEAR(0.2, samples[1], 1e-5f);
  EXPECT_NEAR(0.4, samples[2], 1e-5f);
  EXPECT_NEAR(0.6, samples[3], 1e-5f);
  EXPECT_NEAR(0.8, samples[4], 1e-5f);

  /* use the same setup to test getFrameSet() */
  std::set<double> frames;
  exporter->getFrameSet(5, frames);
  EXPECT_EQ(965, frames.size());
  EXPECT_EQ(1, frames.count(31.0));
  EXPECT_EQ(1, frames.count(31.2));
  EXPECT_EQ(1, frames.count(31.4));
  EXPECT_EQ(1, frames.count(31.6));
  EXPECT_EQ(1, frames.count(31.8));
}

TEST_F(AlembicExportTest, TimeSamples180degShutter)
{
  settings.shutter_open = -0.25;
  settings.shutter_close = 0.25;

  createExporter();
  std::vector<double> samples;

  /* test 5 samples per frame */
  exporter->getShutterSamples(5, true, samples);
  EXPECT_EQ(5, samples.size());
  EXPECT_NEAR(1.230, samples[0], 1e-5f);
  EXPECT_NEAR(1.234, samples[1], 1e-5f);
  EXPECT_NEAR(1.238, samples[2], 1e-5f);
  EXPECT_NEAR(1.242, samples[3], 1e-5f);
  EXPECT_NEAR(1.246, samples[4], 1e-5f);

  /* test same, but using frame number offset instead of time */
  exporter->getShutterSamples(5, false, samples);
  EXPECT_EQ(5, samples.size());
  EXPECT_NEAR(-0.25, samples[0], 1e-5f);
  EXPECT_NEAR(-0.15, samples[1], 1e-5f);
  EXPECT_NEAR(-0.05, samples[2], 1e-5f);
  EXPECT_NEAR(0.05, samples[3], 1e-5f);
  EXPECT_NEAR(0.15, samples[4], 1e-5f);

  /* Use the same setup to test getFrameSet().
   * Here only a few numbers are tested, due to rounding issues. */
  std::set<double> frames;
  exporter->getFrameSet(5, frames);
  EXPECT_EQ(965, frames.size());
  EXPECT_EQ(1, frames.count(30.75));
  EXPECT_EQ(1, frames.count(30.95));
  EXPECT_EQ(1, frames.count(31.15));
}
