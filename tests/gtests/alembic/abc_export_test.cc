#include "testing/testing.h"

// Keep first since utildefines defines AT which conflicts with fucking STL
#include "intern/abc_util.h"
#include "intern/abc_exporter.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "DNA_scene_types.h"
}

class TestableAbcExporter : public AbcExporter {
public:
	TestableAbcExporter(Scene *scene, const char *filename, ExportSettings &settings)
	    : AbcExporter(scene, filename, settings)
	{}

	void getShutterSamples(unsigned int nr_of_samples,
	                       bool time_relative,
	                       std::vector<double> &samples)
	{
		AbcExporter::getShutterSamples(nr_of_samples, time_relative, samples);
	}

	void getFrameSet(unsigned int nr_of_samples,
	                 std::set<double> &frames) {
		AbcExporter::getFrameSet(nr_of_samples, frames);
	}

};


TEST(abc_export, TimeSamplesFullShutter) {
	ExportSettings settings;
	settings.frame_start = 31.0;
	settings.frame_end = 223.0;
	settings.shutter_open = 0.0;
	settings.shutter_close = 1.0;

	/* Fake a 25 FPS scene with a nonzero base (because that's sometimes forgotten) */
	Scene scene;
	scene.r.frs_sec = 50;
	scene.r.frs_sec_base = 2;

	TestableAbcExporter exporter(&scene, "somefile.abc", settings);
	std::vector<double> samples;

	/* test 5 samples per frame */
	exporter.getShutterSamples(5, true, samples);
	EXPECT_EQ(5, samples.size());
	EXPECT_NEAR(1.240, samples[0], 1e-5f);
	EXPECT_NEAR(1.248, samples[1], 1e-5f);
	EXPECT_NEAR(1.256, samples[2], 1e-5f);
	EXPECT_NEAR(1.264, samples[3], 1e-5f);
	EXPECT_NEAR(1.272, samples[4], 1e-5f);

	/* test same, but using frame number offset instead of time */
	exporter.getShutterSamples(5, false, samples);
	EXPECT_EQ(5, samples.size());
	EXPECT_NEAR(0.0, samples[0], 1e-5f);
	EXPECT_NEAR(0.2, samples[1], 1e-5f);
	EXPECT_NEAR(0.4, samples[2], 1e-5f);
	EXPECT_NEAR(0.6, samples[3], 1e-5f);
	EXPECT_NEAR(0.8, samples[4], 1e-5f);

	/* use the same setup to test getFrameSet() */
	std::set<double> frames;
	exporter.getFrameSet(5, frames);
	EXPECT_EQ(965, frames.size());
	EXPECT_EQ(1, frames.count(31.0));
	EXPECT_EQ(1, frames.count(31.2));
	EXPECT_EQ(1, frames.count(31.4));
	EXPECT_EQ(1, frames.count(31.6));
	EXPECT_EQ(1, frames.count(31.8));
}


TEST(abc_export, TimeSamples180degShutter) {
	ExportSettings settings;
	settings.frame_start = 31.0;
	settings.frame_end = 223.0;
	settings.shutter_open = -0.25;
	settings.shutter_close = 0.25;

	/* Fake a 25 FPS scene with a nonzero base (because that's sometimes forgotten) */
	Scene scene;
	scene.r.frs_sec = 50;
	scene.r.frs_sec_base = 2;

	TestableAbcExporter exporter(&scene, "somefile.abc", settings);
	std::vector<double> samples;

	/* test 5 samples per frame */
	exporter.getShutterSamples(5, true, samples);
	EXPECT_EQ(5, samples.size());
	EXPECT_NEAR(1.230, samples[0], 1e-5f);
	EXPECT_NEAR(1.234, samples[1], 1e-5f);
	EXPECT_NEAR(1.238, samples[2], 1e-5f);
	EXPECT_NEAR(1.242, samples[3], 1e-5f);
	EXPECT_NEAR(1.246, samples[4], 1e-5f);

	/* test same, but using frame number offset instead of time */
	exporter.getShutterSamples(5, false, samples);
	EXPECT_EQ(5, samples.size());
	EXPECT_NEAR(-0.25, samples[0], 1e-5f);
	EXPECT_NEAR(-0.15, samples[1], 1e-5f);
	EXPECT_NEAR(-0.05, samples[2], 1e-5f);
	EXPECT_NEAR( 0.05, samples[3], 1e-5f);
	EXPECT_NEAR( 0.15, samples[4], 1e-5f);

	/* Use the same setup to test getFrameSet().
	 * Here only a few numbers are tested, due to rounding issues. */
	std::set<double> frames;
	exporter.getFrameSet(5, frames);
	EXPECT_EQ(965, frames.size());
	EXPECT_EQ(1, frames.count(30.75));
	EXPECT_EQ(1, frames.count(30.95));
	EXPECT_EQ(1, frames.count(31.15));
}
