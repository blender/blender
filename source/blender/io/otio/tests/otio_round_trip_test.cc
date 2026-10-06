/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "MEM_guardedalloc.h"

#include "BKE_anim_data.hh"
#include "BKE_appdir.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_sound.hh"

#include "BLI_fileops.hh"
#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_vector.hh"

#include "BLO_readfile.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "SEQ_add.hh"
#include "SEQ_channels.hh"
#include "SEQ_connect.hh"
#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"
#include "SEQ_time.hh"

#include "IO_otio.hh"
#include "otio_import.hh"
#include "otio_metadata.hh"

#include "opentimelineio/anyVector.h"
#include "opentimelineio/clip.h"
#include "opentimelineio/effect.h"
#include "opentimelineio/externalReference.h"
#include "opentimelineio/gap.h"
#include "opentimelineio/generatorReference.h"
#include "opentimelineio/imageSequenceReference.h"
#include "opentimelineio/linearTimeWarp.h"
#include "opentimelineio/marker.h"
#include "opentimelineio/missingReference.h"
#include "opentimelineio/serialization.h"
#include "opentimelineio/stack.h"
#include "opentimelineio/timeline.h"
#include "opentimelineio/track.h"
#include "opentimelineio/transition.h"

namespace blender::io::otio {

class OTIOTest : public BlendfileLoadingBaseTest {
 protected:
  Main *bmain = nullptr;
  bContext *C = nullptr;
  ReportList reports{};
  Vector<std::unique_ptr<bContextStore>> context_store;
  std::string directory;
  std::string otio_path;

  static void SetUpTestCase()
  {
    BlendfileLoadingBaseTest::SetUpTestCase();
    BKE_sound_init_once();
  }

  static void TearDownTestCase()
  {
    BKE_sound_exit_once();
    BlendfileLoadingBaseTest::TearDownTestCase();
  }

  void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();
    bmain = BKE_main_new();
    C = CTX_create();
    CTX_data_main_set(C, bmain);
    BKE_reports_init(&reports, RPT_STORE);
    BKE_tempdir_init(nullptr);
    directory = std::string(BKE_tempdir_session()) + SEP_STR + "io_otio";
    ASSERT_TRUE(BLI_dir_create_recursive(directory.c_str()));
    otio_path = directory + SEP_STR + "test.otio";
  }

  void TearDown() override
  {
    BLI_delete(directory.c_str(), true, true);
    BKE_reports_free(&reports);
    CTX_free(C);
    BKE_main_free(bmain);
    BlendfileLoadingBaseTest::TearDown();
  }

  Scene *new_scene(const char *name = "Timeline")
  {
    Scene *scene = BKE_scene_add(bmain, name);
    scene->r.frs_sec = 24;
    scene->r.frs_sec_base = 1.0f;
    scene->r.sfra = 1;
    scene->r.efra = 100;
    seq::editing_ensure(scene);
    return scene;
  }

  void set_sequencer_scene(Scene *scene)
  {
    PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
    CTX_store_set(C, CTX_store_add(context_store, "sequencer_scene", &scene_ptr));
  }

  void export_otio(Scene *scene)
  {
    set_sequencer_scene(scene);
    OTIOExportParams params;
    STRNCPY(params.filepath, otio_path.c_str());
    params.reports = &reports;
    OTIO_export(C, params);
    ASSERT_TRUE(BLI_exists(otio_path.c_str()));
  }

  Scene *import_otio()
  {
    set_sequencer_scene(new_scene("Import"));
    OTIOImportParams params;
    STRNCPY(params.filepath, otio_path.c_str());
    params.reports = &reports;
    return OTIO_import(C, params);
  }

  Scene *round_trip(Scene *scene)
  {
    export_otio(scene);
    Scene *imported = import_otio();
    EXPECT_NE(imported, nullptr);
    return imported;
  }

  SerializableObject::Retainer<Timeline> read_otio()
  {
    return dynamic_cast<Timeline *>(read_otio_file_with_workarounds(otio_path.c_str()));
  }

  void write_otio(Timeline *timeline)
  {
    ASSERT_TRUE(timeline->to_json_file(otio_path));
  }

  bool has_report(const char *text)
  {
    for (const Report &report : reports.list) {
      if (strstr(report.message, text)) {
        return true;
      }
    }
    return false;
  }

  std::string asset_path(const char *relative_path)
  {
    char path[FILE_MAX];
    BLI_path_join(path, sizeof(path), tests::flags_test_asset_dir().c_str(), relative_path);
    return path;
  }

  std::string copy_image(const char *filename)
  {
    const std::string path = directory + SEP_STR + filename;
    EXPECT_EQ(
        BLI_copy(asset_path("io_tests/blend_scene/textures/alpha.png").c_str(), path.c_str()), 0);
    return path;
  }

  static PointerRNA rna(Scene *scene, Strip *strip)
  {
    return RNA_pointer_create_discrete(&scene->id, RNA_Strip, strip);
  }

  Strip *add_effect(Scene *scene,
                    ListBaseT<Strip> &seqbase,
                    const StripType type,
                    const char *name,
                    const int channel,
                    const int start,
                    const int length,
                    Strip *input1 = nullptr,
                    Strip *input2 = nullptr)
  {
    seq::LoadData load_data;
    seq::add_load_data_init(&load_data, name, nullptr, start, channel);
    load_data.effect.type = type;
    load_data.effect.length = length;
    load_data.effect.input1 = input1;
    load_data.effect.input2 = input2;
    return seq::add_effect_strip(scene, &seqbase, &load_data);
  }

  Strip *add_color(Scene *scene, const char *name, const int channel, const int start, int length)
  {
    return add_effect(scene, scene->ed->seqbase, STRIP_TYPE_COLOR, name, channel, start, length);
  }

  Strip *add_images(Scene *scene,
                    const char *name,
                    const int channel,
                    const int start,
                    const Span<const char *> filenames)
  {
    seq::LoadData load_data;
    seq::add_load_data_init(&load_data, name, copy_image(filenames[0]).c_str(), start, channel);
    load_data.image.count = filenames.size();
    Strip *strip = seq::add_image_strip(bmain, scene, &scene->ed->seqbase, &load_data);
    seq::add_image_set_directory(strip, directory.c_str());
    for (const int i : filenames.index_range()) {
      copy_image(filenames[i]);
      seq::add_image_load_file(scene, strip, i, filenames[i]);
    }
    return strip;
  }

  static std::string dump(Scene *scene, Strip *strip)
  {
    PointerRNA ptr = rna(scene, strip);
    return serialize_json_to_string(rna_to_dictionary(ptr));
  }

  static void expect_same_strips(Scene *scene_a,
                                 ListBaseT<Strip> &strips_a,
                                 Scene *scene_b,
                                 ListBaseT<Strip> &strips_b)
  {
    EXPECT_EQ(BLI_listbase_count(&strips_a), BLI_listbase_count(&strips_b));
    for (Strip &a : strips_a) {
      Strip *b = static_cast<Strip *>(BLI_findstring(&strips_b, a.name, offsetof(Strip, name)));
      ASSERT_NE(b, nullptr) << a.name + 2;
      EXPECT_EQ(a.type, b->type) << a.name + 2;
      EXPECT_EQ(a.channel, b->channel) << a.name + 2;
      EXPECT_EQ(a.left_handle(), b->left_handle()) << a.name + 2;
      EXPECT_EQ(a.right_handle(scene_a), b->right_handle(scene_b)) << a.name + 2;
      EXPECT_EQ(a.flag & SEQ_MUTE, b->flag & SEQ_MUTE) << a.name + 2;
      EXPECT_EQ(dump(scene_a, &a), dump(scene_b, b)) << a.name + 2;
      if (a.type == STRIP_TYPE_META) {
        expect_same_strips(scene_a, a.seqbase, scene_b, b->seqbase);
      }
    }
  }

  void expect_round_trip(Scene *scene)
  {
    Scene *imported = round_trip(scene);
    ASSERT_NE(imported, nullptr);
    expect_same_strips(scene, scene->ed->seqbase, imported, imported->ed->seqbase);
  }
};

TEST_F(OTIOTest, TimelineSettings)
{
  Scene *scene = new_scene();
  scene->r.frs_sec = 30;
  scene->r.frs_sec_base = 1.001f;
  TimeMarker *marker = MEM_new<TimeMarker>(__func__);
  marker->frame = 10;
  STRNCPY(marker->name, "Marker");
  BLI_addtail(&scene->markers, marker);

  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);
  EXPECT_EQ(imported->r.frs_sec, 30);
  EXPECT_FLOAT_EQ(imported->r.frs_sec_base, 1.001f);
  EXPECT_EQ(imported->r.sfra, 1);
  ASSERT_EQ(BLI_listbase_count(&imported->markers), 1);
  EXPECT_EQ(imported->markers.first()->frame, 10);
  EXPECT_STREQ(imported->markers.first()->name, "Marker");
  EXPECT_TRUE(read_otio()->tracks()->children().empty());
}

TEST_F(OTIOTest, GeneratorsAndModifiers)
{
  Scene *scene = new_scene();
  Strip *color = add_color(scene, "Color", 1, 1, 10);
  color->flag |= SEQ_MUTE;
  PointerRNA color_ptr = rna(scene, color);
  RNA_float_set(&color_ptr, "blend_alpha", 0.5f);
  RNA_enum_set(&color_ptr, "blend_type", STRIP_BLEND_ADD);

  Strip *text = add_effect(scene, scene->ed->seqbase, STRIP_TYPE_TEXT, "Text", 2, 5, 20);
  PointerRNA text_ptr = rna(scene, text);
  RNA_string_set(&text_ptr, "text", "Hello");
  RNA_float_set(&text_ptr, "font_size", 42.0f);
  RNA_float_set(&text_ptr, "outline_width", 0.25f);

  StripModifierData *curves = seq::modifier_new(color, "Curves", eSeqModifierType_Curves);
  CurveMapping &mapping = reinterpret_cast<CurvesModifierData *>(curves)->curve_mapping;
  BKE_curvemap_insert(&mapping.cm[0], 0.5f, 0.75f);
  BKE_curvemapping_changed(&mapping, false);
  StripModifierData *bright = seq::modifier_new(color, "Bright", eSeqModifierType_BrightContrast);
  reinterpret_cast<BrightContrastModifierData *>(bright)->bright = 0.3f;
  bright->flag |= STRIP_MODIFIER_FLAG_MUTE;

  expect_round_trip(scene);
}

TEST_F(OTIOTest, MediaStrips)
{
#if !defined(WITH_FFMPEG) || !defined(WITH_AUDASPACE)
  GTEST_SKIP() << "Movie or sound support is unavailable";
#endif
  Scene *scene = new_scene();
  seq::LoadData load_data;
  seq::add_load_data_init(&load_data,
                          "Movie",
                          asset_path("sequence_editing/ffmpeg/media/grad_h264.mp4").c_str(),
                          5,
                          1);
  Strip *movie = seq::add_movie_strip(bmain, scene, &scene->ed->seqbase, &load_data);
  movie->handles_set(scene, movie->left_handle() + 2, movie->right_handle(scene) - 3);

  seq::add_load_data_init(&load_data, "Sound", asset_path("sound/pink_panther.ogg").c_str(), 1, 2);
  Strip *sound = seq::add_sound_strip(bmain, scene, &scene->ed->seqbase, &load_data);
  sound->volume = 0.5f;
  EQCurveMappingData *graph = seq::sound_equalizermodifier_add_graph(
      reinterpret_cast<SoundEqualizerModifierData *>(
          seq::modifier_new(sound, "EQ", eSeqModifierType_SoundEqualizer)),
      100.0f,
      1000.0f);
  BKE_curvemap_insert(&graph->curve_mapping.cm[0], 500.0f, 3.0f);

  add_images(scene, "Still", 3, 20, {"still.png"});
  expect_round_trip(scene);
}

TEST_F(OTIOTest, ImageSequences)
{
  Scene *scene = new_scene();
  add_images(scene, "Sequence", 1, 1, {"shot.0101.png", "shot.0103.png", "shot.0105.png"});
  add_images(scene, "Unnamed", 2, 1, {"a.png", "b.png"});
  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);

  SerializableObject::Retainer<Timeline> timeline = read_otio();
  auto *reference = dynamic_cast<ImageSequenceReference *>(
      timeline->find_clips()[0]->media_reference());
  ASSERT_NE(reference, nullptr);
  EXPECT_EQ(reference->name_prefix(), "shot.");
  EXPECT_EQ(reference->start_frame(), 101);
  EXPECT_EQ(reference->frame_step(), 2);
  EXPECT_EQ(reference->frame_zero_padding(), 4);
  EXPECT_TRUE(has_report("1 image sequence(s) with non-sequential filenames"));

  Strip *sequence = seq::lookup_strip_by_name(imported->ed, "Sequence");
  ASSERT_NE(sequence, nullptr);
  EXPECT_EQ(sequence->data->stripdata_num, 3);
  EXPECT_STREQ(sequence->data->stripdata[2].filename, "shot.0105.png");
  EXPECT_EQ(seq::lookup_strip_by_name(imported->ed, "Unnamed"), nullptr);
}

TEST_F(OTIOTest, Layout)
{
  Scene *scene = new_scene();
  add_color(scene, "Before Start", 1, -5, 10);
  add_color(scene, "A", 1, 10, 10);
  add_color(scene, "B", 1, 40, 10);

  seq::LoadData load_data;
  seq::add_load_data_init(&load_data, "Meta", nullptr, 25, 1);
  Strip *meta = seq::add_meta_strip(scene, &scene->ed->seqbase, &load_data);
  add_effect(scene, meta->seqbase, STRIP_TYPE_COLOR, "Inner", 1, 25, 10);
  seq::time_update_meta_strip_range(scene, meta);
  meta->handles_set(scene, 27, 33);

  add_color(scene, "High Channel", 7, 30, 5);
  SeqTimelineChannel *channel = seq::channel_get_by_index(&scene->ed->channels, 7);
  STRNCPY(channel->name, "Titles");
  channel->flag |= SEQ_CHANNEL_MUTE;

  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);
  expect_same_strips(scene, scene->ed->seqbase, imported, imported->ed->seqbase);
  const SeqTimelineChannel *imported_channel = seq::channel_get_by_index(&imported->ed->channels,
                                                                         7);
  EXPECT_STREQ(imported_channel->name, "Titles");
  EXPECT_TRUE(imported_channel->is_muted());
}

TEST_F(OTIOTest, MixedMeta)
{
#ifndef WITH_AUDASPACE
  GTEST_SKIP() << "Sound support is unavailable";
#endif
  Scene *scene = new_scene();
  seq::LoadData load_data;
  seq::add_load_data_init(&load_data, "Meta", nullptr, 10, 1);
  Strip *meta = seq::add_meta_strip(scene, &scene->ed->seqbase, &load_data);
  add_effect(scene, meta->seqbase, STRIP_TYPE_COLOR, "Picture", 1, 10, 10);
  seq::add_load_data_init(
      &load_data, "Sound", asset_path("sound/pink_panther.ogg").c_str(), 10, 2);
  Strip *sound = seq::add_sound_strip(bmain, scene, &meta->seqbase, &load_data);
  sound->handles_set(scene, 10, 20);
  seq::time_update_meta_strip_range(scene, meta);
  meta->handles_set(scene, 10, 20);

  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);

  SerializableObject::Retainer<Timeline> timeline = read_otio();
  ASSERT_EQ(timeline->video_tracks().size(), 1);
  ASSERT_EQ(timeline->audio_tracks().size(), 1);
  EXPECT_NE(dynamic_cast<Stack *>(timeline->video_tracks()[0]->children().back().value), nullptr);
  EXPECT_NE(dynamic_cast<Stack *>(timeline->audio_tracks()[0]->children().back().value), nullptr);

  ASSERT_EQ(BLI_listbase_count(&imported->ed->seqbase), 2);
  for (Strip &imported_meta : imported->ed->seqbase) {
    EXPECT_EQ(imported_meta.type, STRIP_TYPE_META);
    EXPECT_EQ(imported_meta.left_handle(), 10);
    EXPECT_EQ(imported_meta.right_handle(imported), 20);
    ASSERT_EQ(BLI_listbase_count(&imported_meta.seqbase), 1);
    EXPECT_EQ(imported_meta.seqbase.first()->left_handle(), 10);
  }
}

TEST_F(OTIOTest, SoundSubframeOffset)
{
#ifndef WITH_AUDASPACE
  GTEST_SKIP() << "Sound support is unavailable";
#endif
  Scene *scene = new_scene();
  const double fps = scene->frames_per_second();
  const std::pair<const char *, float> cases[] = {{"Small", 0.3f}, {"Large", 0.7f}};
  for (const int i : IndexRange(2)) {
    seq::LoadData load_data;
    seq::add_load_data_init(
        &load_data, cases[i].first, asset_path("sound/pink_panther.ogg").c_str(), 1, i + 1);
    Strip *sound = seq::add_sound_strip(bmain, scene, &scene->ed->seqbase, &load_data);
    sound->handles_set(scene, 6, 26);
    sound->sound_offset = cases[i].second / fps;
  }

  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);

  SerializableObject::Retainer<Timeline> timeline = read_otio();
  for (const auto &[name, offset] : cases) {
    const Strip *original = seq::lookup_strip_by_name(scene->ed, name);
    const Strip *strip = seq::lookup_strip_by_name(imported->ed, name);
    ASSERT_NE(strip, nullptr) << name;
    EXPECT_EQ(strip->left_handle(), original->left_handle()) << name;
    EXPECT_EQ(strip->right_handle(imported), original->right_handle(scene)) << name;
    EXPECT_NEAR(strip->content_start() + strip->sound_offset * fps,
                original->content_start() + original->sound_offset * fps,
                1e-4)
        << name;

    for (const SerializableObject::Retainer<Clip> &clip : timeline->find_clips()) {
      if (clip->name() == name) {
        EXPECT_NEAR(clip->source_range()->start_time().value_rescaled_to(fps), 5.0 - offset, 1e-4)
            << name;
      }
    }
  }
}

TEST_F(OTIOTest, Transitions)
{
  Scene *scene = new_scene();
  ListBaseT<Strip> &seqbase = scene->ed->seqbase;
  Strip *a = add_color(scene, "A", 1, 1, 10);
  Strip *b = add_color(scene, "B", 1, 16, 10);
  add_effect(scene, seqbase, STRIP_TYPE_CROSS, "Cross", 1, 11, 5, a, b);

  Strip *c = add_color(scene, "C", 2, 30, 10);
  Strip *d = add_color(scene, "D", 2, 44, 10);
  Strip *wipe = add_effect(scene, seqbase, STRIP_TYPE_WIPE, "Wipe", 2, 40, 4, c, d);
  PointerRNA wipe_ptr = rna(scene, wipe);
  RNA_float_set(&wipe_ptr, "angle", 0.5f);
  RNA_enum_set(&wipe_ptr, "transition_type", SEQ_WIPE_CLOCK);
  wipe->flag &= ~SEQ_USE_EFFECT_DEFAULT_FADE;
  wipe->effect_fader = 0.25f;

  expect_round_trip(scene);
}

TEST_F(OTIOTest, TransitionAcrossChannels)
{
  Scene *scene = new_scene();
  Strip *a = add_color(scene, "A", 1, 1, 20);
  Strip *b = add_color(scene, "B", 2, 10, 20);
  add_effect(scene, scene->ed->seqbase, STRIP_TYPE_CROSS, "Cross", 3, 10, 11, a, b);

  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);
  EXPECT_TRUE(has_report("Transition strip 'Cross' was not exported"));
  EXPECT_EQ(seq::lookup_strip_by_name(imported->ed, "Cross"), nullptr);
  EXPECT_EQ(BLI_listbase_count(&imported->ed->seqbase), 2);
}

TEST_F(OTIOTest, Effects)
{
  Scene *scene = new_scene();
  ListBaseT<Strip> &seqbase = scene->ed->seqbase;
  Strip *a = add_color(scene, "A", 1, 1, 20);
  Strip *b = add_color(scene, "B", 3, 1, 20);

  Strip *speed = add_effect(scene, seqbase, STRIP_TYPE_SPEED, "Speed", 2, 1, 20, a);
  PointerRNA speed_ptr = rna(scene, speed);
  RNA_enum_set(&speed_ptr, "speed_control", SEQ_SPEED_MULTIPLY);
  RNA_float_set(&speed_ptr, "speed_factor", 2.0f);

  Strip *blur = add_effect(scene, seqbase, STRIP_TYPE_GAUSSIAN_BLUR, "Blur", 4, 1, 20, b);
  PointerRNA blur_ptr = rna(scene, blur);
  RNA_float_set(&blur_ptr, "size_x", 7.0f);

  Strip *over = add_effect(scene, seqbase, STRIP_TYPE_ALPHAOVER, "Over", 5, 1, 20, a, b);
  over->effect_fader = 0.5f;
  over->flag &= ~SEQ_USE_EFFECT_DEFAULT_FADE;

  expect_round_trip(scene);
}

TEST_F(OTIOTest, SceneStrip)
{
  Scene *scene = new_scene();
  seq::LoadData load_data;
  seq::add_load_data_init(&load_data, "Shot", nullptr, 1, 1);
  load_data.scene = new_scene("Shot Scene");
  seq::add_scene_strip(scene, &scene->ed->seqbase, &load_data);

  Scene *imported = round_trip(scene);
  ASSERT_NE(imported, nullptr);
  EXPECT_TRUE(has_report("1 scene strip(s) found"));
  EXPECT_NE(dynamic_cast<MissingReference *>(read_otio()->find_clips()[0]->media_reference()),
            nullptr);
  expect_same_strips(scene, scene->ed->seqbase, imported, imported->ed->seqbase);
}

TEST_F(OTIOTest, ForeignMetadata)
{
  auto timeline = SerializableObject::Retainer<Timeline>(
      new Timeline("Foreign", RationalTime(0, 24)));
  auto track = SerializableObject::Retainer<Track>(new Track());
  AnyDictionary resolve;
  resolve["Clip Color"] = std::string("Orange");
  auto clip = SerializableObject::Retainer<Clip>(new Clip(
      "Red",
      new GeneratorReference("", "COLOR", TimeRange(RationalTime(0, 24), RationalTime(10, 24))),
      TimeRange(RationalTime(0, 24), RationalTime(10, 24))));
  clip->metadata()["Resolve_OTIO"] = resolve;
  track->append_child(clip);
  timeline->tracks()->append_child(track);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  Strip *strip = seq::lookup_strip_by_name(imported->ed, "Red");
  ASSERT_NE(strip, nullptr);
  EXPECT_EQ(strip->type, STRIP_TYPE_COLOR);
  IDProperty *group = IDP_GetPropertyFromGroup(strip->prop, "otio_metadata");
  ASSERT_NE(group, nullptr);
  EXPECT_NE(IDP_GetPropertyFromGroup(group, "Resolve_OTIO"), nullptr);

  export_otio(imported);
  AnyDictionary exported = read_otio()->find_clips()[0]->metadata();
  EXPECT_EQ(std::any_cast<std::string>(
                std::any_cast<AnyDictionary>(exported["Resolve_OTIO"])["Clip Color"]),
            "Orange");
}

TEST_F(OTIOTest, ForeignTiming)
{
  auto timeline = SerializableObject::Retainer<Timeline>(
      new Timeline("Foreign", RationalTime(90000, 25)));
  auto track = SerializableObject::Retainer<Track>(new Track());
  track->append_child(new Gap(RationalTime(10, 25)));
  track->append_child(
      new Clip("Still",
               new ExternalReference("file:///a%20b/still.png",
                                     TimeRange(RationalTime(1000, 25), RationalTime(100, 25))),
               TimeRange(RationalTime(1010, 25), RationalTime(40, 50))));
  timeline->tracks()->append_child(track);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  EXPECT_EQ(imported->r.frs_sec, 25);
  EXPECT_EQ(imported->r.sfra, 90000);
  Strip *strip = seq::lookup_strip_by_name(imported->ed, "Still");
  ASSERT_NE(strip, nullptr);
  EXPECT_EQ(strip->type, STRIP_TYPE_IMAGE);
  EXPECT_EQ(strip->left_handle(), 90010);
  EXPECT_EQ(strip->right_handle(imported), 90030);
  EXPECT_EQ(BLI_path_cmp(strip->data->dirpath, "/a b/"), 0);
}

static AnyDictionary resolve_effect(const std::string &name,
                                    const std::vector<std::pair<std::string, std::any>> &values)
{
  AnyVector parameters;
  for (const auto &[id, value] : values) {
    AnyDictionary parameter;
    parameter["Parameter ID"] = id;
    parameter["Parameter Value"] = value;
    parameters.push_back(parameter);
  }
  AnyDictionary effect;
  effect["Effect Name"] = name;
  effect["Enabled"] = true;
  effect["Parameters"] = parameters;
  return effect;
}

TEST_F(OTIOTest, MarkerSchemaVersion2)
{
  auto timeline = SerializableObject::Retainer<Timeline>(
      new Timeline("Markers", RationalTime(0, 24)));
  timeline->tracks()->markers().push_back(
      new Marker("Old", TimeRange(RationalTime(5, 24), RationalTime(1, 24)), std::nullopt));
  std::string json = timeline->to_json_string();
  const std::string schema = "\"Marker.3\"";
  const size_t schema_start = json.find(schema);
  ASSERT_NE(schema_start, std::string::npos);
  json.replace(schema_start, schema.size(), "\"Marker.2\"");
  fstream stream(otio_path, std::ios::out);
  stream << json;
  stream.close();

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  ASSERT_EQ(BLI_listbase_count(&imported->markers), 1);
  EXPECT_EQ(imported->markers.first()->frame, 5);
  EXPECT_STREQ(imported->markers.first()->name, "Old");
}

TEST_F(OTIOTest, ResolveMetadata)
{
  const TimeRange range(RationalTime(0, 24), RationalTime(10, 24));
  auto *generator = new GeneratorReference("Solid Color", "Solid Color", range);
  generator->parameters()["Resolve_OTIO"] = AnyVector{
      resolve_effect("Solid Color", {{"color", std::string("#ff8000")}})};
  auto clip = SerializableObject::Retainer<Clip>(new Clip("Solid Color", generator, range));
  AnyDictionary clip_resolve;
  clip_resolve["Link Group ID"] = int64_t(1);
  clip->metadata()["Resolve_OTIO"] = clip_resolve;

  auto add_effect = [&](const AnyDictionary &resolve) {
    auto *effect = new otio::Effect("", "Resolve Effect");
    effect->metadata()["Resolve_OTIO"] = resolve;
    clip->effects().push_back(effect);
  };
  add_effect(resolve_effect("Transform",
                            {{"transformationZoomX", 2.0},
                             {"transformationPan", 0.25},
                             {"transformationRotationAngle", 90.0}}));
  add_effect(resolve_effect("Composite", {{"composite mode", int64_t(5)}, {"opacity", 50.0}}));

  auto track = SerializableObject::Retainer<Track>(new Track("V1"));
  AnyDictionary track_resolve;
  track_resolve["Locked"] = true;
  track->metadata()["Resolve_OTIO"] = track_resolve;
  track->append_child(clip);
  auto timeline = SerializableObject::Retainer<Timeline>(new Timeline("Resolve"));
  timeline->tracks()->append_child(track);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  Strip *strip = seq::lookup_strip_by_name(imported->ed, "Solid Color");
  ASSERT_NE(strip, nullptr);
  ASSERT_EQ(strip->type, STRIP_TYPE_COLOR);
  const SolidColorVars *color = static_cast<const SolidColorVars *>(strip->effectdata);
  EXPECT_FLOAT_EQ(color->col[0], 1.0f);
  EXPECT_NEAR(color->col[1], 128.0f / 255.0f, 1e-5f);
  EXPECT_FLOAT_EQ(color->col[2], 0.0f);
  EXPECT_FLOAT_EQ(strip->data->transform->scale_x, 2.0f);
  EXPECT_FLOAT_EQ(strip->data->transform->scale_y, 1.0f);
  EXPECT_FLOAT_EQ(strip->data->transform->xofs, 0.25f * imported->r.xsch);
  EXPECT_NEAR(strip->data->transform->rotation, M_PI_2, 1e-5f);
  EXPECT_EQ(strip->blend_mode, STRIP_BLEND_SCREEN);
  EXPECT_FLOAT_EQ(strip->blend_opacity, 50.0f);
  EXPECT_STREQ(seq::channel_get_by_index(&imported->ed->channels, 1)->name, "V1");
  EXPECT_TRUE(seq::channel_get_by_index(&imported->ed->channels, 1)->is_locked());
  EXPECT_NE(IDP_GetPropertyFromGroup(strip->prop, "otio_metadata"), nullptr);
}

TEST_F(OTIOTest, ReverseTimeWarp)
{
  const TimeRange range(RationalTime(0, 24), RationalTime(10, 24));
  auto track = SerializableObject::Retainer<Track>(new Track());
  for (const double time_scalar : {-1.0, -2.0}) {
    auto *clip = new Clip(
        std::to_string(time_scalar), new GeneratorReference("", "COLOR", range), range);
    clip->effects().push_back(new LinearTimeWarp("", "", time_scalar));
    track->append_child(clip);
  }
  auto timeline = SerializableObject::Retainer<Timeline>(new Timeline("Reverse"));
  timeline->tracks()->append_child(track);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  ASSERT_EQ(BLI_listbase_count(&imported->ed->seqbase), 3);
  int speed_strips = 0;
  for (Strip &strip : imported->ed->seqbase) {
    if (strip.type == STRIP_TYPE_SPEED) {
      EXPECT_FLOAT_EQ(static_cast<SpeedControlVars *>(strip.effectdata)->speed_fader, 2.0f);
      speed_strips++;
    }
    else {
      EXPECT_TRUE(strip.flag & SEQ_REVERSE_FRAMES);
    }
  }
  EXPECT_EQ(speed_strips, 1);
}

TEST_F(OTIOTest, ResolveEditMetadata)
{
  const TimeRange range(RationalTime(0, 24), RationalTime(20, 24));
  auto image = SerializableObject::Retainer<Clip>(
      new Clip("Image",
               new ExternalReference(asset_path("sequence_editing/filter/31x17.png"), range),
               range));
  auto add_effect = [&](Clip &clip, const AnyDictionary &resolve) {
    auto *effect = new otio::Effect("", "Resolve Effect");
    effect->metadata()["Resolve_OTIO"] = resolve;
    clip.effects().push_back(effect);
  };
  add_effect(*image, resolve_effect("Retime and Scaling", {{"scale", int64_t(3)}}));
  add_effect(*image,
             resolve_effect("Transform",
                            {{"transformationFlipX", true},
                             {"transformationAnchorPoint", AnyVector{0.1, 0.05}}}));
  AnyDictionary composite = resolve_effect("Composite", {{"opacity", 100.0}});
  AnyDictionary key_start;
  key_start["Value"] = 100.0;
  AnyDictionary key_end;
  key_end["Value"] = 0.0;
  AnyDictionary keys;
  keys["0"] = key_start;
  keys["10"] = key_end;
  std::any_cast<AnyDictionary &>(
      std::any_cast<AnyVector &>(composite["Parameters"])[0])["Key Frames"] = keys;
  add_effect(*image, composite);

  auto *wipe = new Transition(
      "Edge Wipe", "Custom_Transition", RationalTime(5, 24), RationalTime(5, 24));
  AnyDictionary wipe_resolve;
  wipe_resolve["Effects"] = resolve_effect("Edge Wipe", {{"angle", int64_t(45)}});
  wipe->metadata()["Resolve_OTIO"] = wipe_resolve;

  AnyDictionary link_resolve;
  link_resolve["Link Group ID"] = int64_t(7);
  auto *linked_video = new Clip("Linked Video", new GeneratorReference("", "COLOR", range), range);
  linked_video->metadata()["Resolve_OTIO"] = link_resolve;
  auto *linked_other = new Clip("Linked Other", new GeneratorReference("", "COLOR", range), range);
  linked_other->metadata()["Resolve_OTIO"] = link_resolve;

  auto track_1 = SerializableObject::Retainer<Track>(new Track("V1"));
  track_1->append_child(image);
  track_1->append_child(wipe);
  track_1->append_child(linked_video);
  auto *adjustment = new Clip("Adjustment", new MissingReference(), range);
  add_effect(*adjustment, resolve_effect("Effect", {}));
  std::any_cast<AnyDictionary &>(
      adjustment->effects()[0]->metadata()["Resolve_OTIO"])["Type"] = int64_t(74);
  auto track_2 = SerializableObject::Retainer<Track>(new Track("V2"));
  track_2->append_child(adjustment);
  track_2->append_child(linked_other);

  auto *sound = new Clip(
      "Sound", new ExternalReference(asset_path("sound/pink_panther.ogg"), range), range);
  add_effect(*sound,
             resolve_effect("Fairlight Clip Volume and Fades",
                            {{"volume", 0.0}, {"faderIn", 4.0}, {"faderOut", 6.0}}));
  auto track_3 = SerializableObject::Retainer<Track>(
      new Track("A1", std::nullopt, Track::Kind::audio));
  track_3->append_child(sound);

  auto timeline = SerializableObject::Retainer<Timeline>(new Timeline("Resolve"));
  timeline->tracks()->append_child(track_1);
  timeline->tracks()->append_child(track_2);
  timeline->tracks()->append_child(track_3);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  Strip *strip = seq::lookup_strip_by_name(imported->ed, "Image");
  ASSERT_NE(strip, nullptr);
  const float scale = std::max(imported->r.xsch / 31.0f, imported->r.ysch / 17.0f);
  EXPECT_FLOAT_EQ(strip->data->transform->scale_x, scale);
  EXPECT_FLOAT_EQ(strip->data->transform->origin[0],
                  0.5f + 0.1f * imported->r.ysch / (31.0f * scale));
  EXPECT_FLOAT_EQ(strip->data->transform->origin[1],
                  0.5f + 0.05f * imported->r.ysch / (17.0f * scale));
  EXPECT_TRUE(strip->flag & SEQ_FLIPX);

  FCurve *fcurve = BKE_animadata_fcurve_find_by_rna_path(
      BKE_animdata_from_id(&imported->id),
      "sequence_editor.strips_all[\"Image\"].blend_alpha",
      0,
      nullptr,
      nullptr);
  ASSERT_NE(fcurve, nullptr);
  ASSERT_EQ(fcurve->totvert, 2);
  EXPECT_FLOAT_EQ(fcurve->bezt[0].vec[1][0], strip->left_handle());
  EXPECT_FLOAT_EQ(fcurve->bezt[0].vec[1][1], 1.0f);
  EXPECT_FLOAT_EQ(fcurve->bezt[1].vec[1][0], strip->left_handle() + 10);
  EXPECT_FLOAT_EQ(fcurve->bezt[1].vec[1][1], 0.0f);

  Strip *wipe_strip = seq::lookup_strip_by_name(imported->ed, "Edge Wipe");
  ASSERT_NE(wipe_strip, nullptr);
  EXPECT_EQ(wipe_strip->type, STRIP_TYPE_WIPE);
  EXPECT_NEAR(static_cast<WipeVars *>(wipe_strip->effectdata)->angle, M_PI_4, 1e-5f);

  Strip *video = seq::lookup_strip_by_name(imported->ed, "Linked Video");
  Strip *other = seq::lookup_strip_by_name(imported->ed, "Linked Other");
  ASSERT_NE(video, nullptr);
  ASSERT_NE(other, nullptr);
  EXPECT_TRUE(seq::connected_strips_get(video).contains(other));

  Strip *adjustment_strip = seq::lookup_strip_by_name(imported->ed, "Adjustment");
  ASSERT_NE(adjustment_strip, nullptr);
  EXPECT_EQ(adjustment_strip->type, STRIP_TYPE_ADJUSTMENT);

  Strip *sound_strip = seq::lookup_strip_by_name(imported->ed, "Sound");
  ASSERT_NE(sound_strip, nullptr);
  FCurve *volume = BKE_animadata_fcurve_find_by_rna_path(
      BKE_animdata_from_id(&imported->id),
      "sequence_editor.strips_all[\"Sound\"].volume",
      0,
      nullptr,
      nullptr);
  ASSERT_NE(volume, nullptr);
  ASSERT_EQ(volume->totvert, 4);
  const int start = sound_strip->left_handle();
  const int end = sound_strip->right_handle(imported);
  EXPECT_FLOAT_EQ(volume->bezt[0].vec[1][0], start);
  EXPECT_FLOAT_EQ(volume->bezt[0].vec[1][1], 0.0f);
  EXPECT_FLOAT_EQ(volume->bezt[1].vec[1][0], start + 4);
  EXPECT_FLOAT_EQ(volume->bezt[1].vec[1][1], 1.0f);
  EXPECT_FLOAT_EQ(volume->bezt[2].vec[1][0], end - 6);
  EXPECT_FLOAT_EQ(volume->bezt[2].vec[1][1], 1.0f);
  EXPECT_FLOAT_EQ(volume->bezt[3].vec[1][0], end);
  EXPECT_FLOAT_EQ(volume->bezt[3].vec[1][1], 0.0f);
}

TEST_F(OTIOTest, KdenliveMetadata)
{
  const TimeRange range(RationalTime(0, 24), RationalTime(10, 24));
  AnyDictionary kdenlive;
  kdenlive["color"] = std::string("0xff8000ff");
  AnyDictionary parameters;
  parameters["kdenlive"] = kdenlive;
  auto clip = SerializableObject::Retainer<Clip>(
      new Clip("Orange",
               new GeneratorReference("Orange", "kdenlive:SolidColor", range, parameters),
               range));
  auto track = SerializableObject::Retainer<Track>(new Track());
  track->append_child(clip);
  auto timeline = SerializableObject::Retainer<Timeline>(new Timeline("Kdenlive"));
  timeline->tracks()->append_child(track);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  Strip *strip = seq::lookup_strip_by_name(imported->ed, "Orange");
  ASSERT_NE(strip, nullptr);
  ASSERT_EQ(strip->type, STRIP_TYPE_COLOR);
  const SolidColorVars *color = static_cast<const SolidColorVars *>(strip->effectdata);
  EXPECT_FLOAT_EQ(color->col[0], 1.0f);
  EXPECT_NEAR(color->col[1], 128.0f / 255.0f, 1e-5f);
  EXPECT_FLOAT_EQ(color->col[2], 0.0f);
}

TEST_F(OTIOTest, KdenliveTimeline)
{
  auto timeline = SerializableObject::Retainer<Timeline>(
      new Timeline("Kdenlive", RationalTime(0, 24)));
  auto track = SerializableObject::Retainer<Track>(new Track());
  track->append_child(new Gap(RationalTime(10, 24)));
  track->append_child(
      new Clip("Missing",
               new ExternalReference("file:///missing.mov",
                                     TimeRange(RationalTime(0, 0), RationalTime(120, 0))),
               TimeRange(RationalTime(5, 24), RationalTime(10, 24))));
  timeline->tracks()->append_child(track);
  write_otio(timeline);

  Scene *imported = import_otio();
  ASSERT_NE(imported, nullptr);
  Strip *strip = seq::lookup_strip_by_name(imported->ed, "Missing");
  ASSERT_NE(strip, nullptr);
  EXPECT_FLOAT_EQ(strip->content_start(), 5.0f);
  EXPECT_EQ(strip->left_handle(), 10);
  EXPECT_EQ(strip->right_handle(imported), 20);
}

}  // namespace blender::io::otio
