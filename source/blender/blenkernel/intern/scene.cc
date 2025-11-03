/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_rotation.h"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLO_readfile.hh"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_bpath.hh"
#include "BKE_callbacks.hh"
#include "BKE_collection.hh"
#include "BKE_colortools.hh"
#include "BKE_curveprofile.h"
#include "BKE_duplilist.hh"
#include "BKE_editmesh.hh"
#include "BKE_effect.h"
#include "BKE_fcurve.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_mesh_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"
#include "BKE_pointcache.h"
#include "BKE_preview_image.hh"
#include "BKE_rigidbody.h"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_screen.hh"
#include "BKE_sound.hh"
#include "BKE_unit.hh"
#include "BKE_workspace.hh"

#include "ANIM_action.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"
#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"

#include "RNA_access.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "BLO_read_write.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "DRW_engine.hh"

#include "bmesh.hh"

using blender::bke::CompositorRuntime;
using blender::bke::SceneRuntime;
using blender::bke::SequencerRuntime;

CompositorRuntime::~CompositorRuntime()
{
  if (preview_depsgraph) {
    DEG_graph_free(preview_depsgraph);
  }
}

SequencerRuntime::~SequencerRuntime()
{
  DEG_graph_free(depsgraph);
}

CurveMapping *BKE_sculpt_default_cavity_curve()

{
  CurveMapping *cumap = BKE_curvemapping_add(1, 0, 0, 1, 1);

  cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  cumap->preset = CURVE_PRESET_LINE;

  BKE_curvemap_reset(cumap->cm, &cumap->clipr, cumap->preset, CurveMapSlopeType::Positive);
  BKE_curvemapping_changed(cumap, false);
  BKE_curvemapping_init(cumap);

  return cumap;
}

CurveMapping *BKE_paint_default_curve()
{
  CurveMapping *cumap = BKE_curvemapping_add(1, 0, 0, 1, 1);
  BKE_curvemap_reset(cumap->cm, &cumap->clipr, CURVE_PRESET_LINE, CurveMapSlopeType::Positive);
  BKE_curvemapping_init(cumap);

  return cumap;
}

static void scene_init_data(ID *id)
{
  Scene *scene = (Scene *)id;
  const char *colorspace_name;
  SceneRenderView *srv;
  CurveMapping *mblur_shutter_curve;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(scene, id));

  MEMCPY_STRUCT_AFTER(scene, DNA_struct_default_get(Scene), id);

  STRNCPY(scene->r.bake.filepath, U.renderdir);

  mblur_shutter_curve = &scene->r.mblur_shutter_curve;
  BKE_curvemapping_set_defaults(mblur_shutter_curve, 1, 0.0f, 0.0f, 1.0f, 1.0f, HD_AUTO);
  BKE_curvemapping_init(mblur_shutter_curve);
  BKE_curvemap_reset(mblur_shutter_curve->cm,
                     &mblur_shutter_curve->clipr,
                     CURVE_PRESET_MAX,
                     CurveMapSlopeType::PositiveNegative);

  scene->toolsettings = DNA_struct_default_alloc(ToolSettings);

  scene->toolsettings->autokey_mode = uchar(U.autokey_mode);

  scene->toolsettings->unified_paint_settings.curve_rand_hue = BKE_paint_default_curve();
  scene->toolsettings->unified_paint_settings.curve_rand_saturation = BKE_paint_default_curve();
  scene->toolsettings->unified_paint_settings.curve_rand_value = BKE_paint_default_curve();

  /* Grease pencil multi-frame falloff curve. */
  scene->toolsettings->gp_sculpt.cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  CurveMapping *gp_falloff_curve = scene->toolsettings->gp_sculpt.cur_falloff;
  BKE_curvemapping_init(gp_falloff_curve);
  BKE_curvemap_reset(gp_falloff_curve->cm,
                     &gp_falloff_curve->clipr,
                     CURVE_PRESET_GAUSS,
                     CurveMapSlopeType::Positive);

  scene->toolsettings->gp_sculpt.cur_primitive = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  CurveMapping *gp_primitive_curve = scene->toolsettings->gp_sculpt.cur_primitive;
  BKE_curvemapping_init(gp_primitive_curve);
  BKE_curvemap_reset(gp_primitive_curve->cm,
                     &gp_primitive_curve->clipr,
                     CURVE_PRESET_BELL,
                     CurveMapSlopeType::Positive);

  scene->unit.system = USER_UNIT_METRIC;
  scene->unit.scale_length = 1.0f;
  scene->unit.length_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_LENGTH));
  scene->unit.mass_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_MASS));
  scene->unit.time_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_TIME));
  scene->unit.temperature_unit = uchar(
      BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_TEMPERATURE));

  {
    ParticleEditSettings *pset;
    pset = &scene->toolsettings->particle;
    for (size_t i = 1; i < ARRAY_SIZE(pset->brush); i++) {
      pset->brush[i] = pset->brush[0];
    }
    pset->brush[PE_BRUSH_CUT].strength = 1.0f;
  }

  STRNCPY_UTF8(scene->r.engine, RE_engine_id_BLENDER_EEVEE);

  STRNCPY(scene->r.pic, U.renderdir);

  /* NOTE: in header_info.c the scene copy happens...,
   * if you add more to renderdata it has to be checked there. */

  /* multiview - stereo */
  BKE_scene_add_render_view(scene, STEREO_LEFT_NAME);
  srv = static_cast<SceneRenderView *>(scene->r.views.first);
  STRNCPY(srv->suffix, STEREO_LEFT_SUFFIX);

  BKE_scene_add_render_view(scene, STEREO_RIGHT_NAME);
  srv = static_cast<SceneRenderView *>(scene->r.views.last);
  STRNCPY(srv->suffix, STEREO_RIGHT_SUFFIX);

  BKE_sound_reset_scene_runtime(scene);

  /* color management */
  colorspace_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_SEQUENCER);

  BKE_color_managed_display_settings_init(&scene->display_settings);
  BKE_color_managed_view_settings_init(&scene->view_settings, &scene->display_settings, "AgX");
  STRNCPY_UTF8(scene->sequencer_colorspace_settings.name, colorspace_name);

  BKE_image_format_init(&scene->r.im_format);
  BKE_image_format_init(&scene->r.bake.im_format);

  /* Curve Profile */
  scene->toolsettings->custom_bevel_profile_preset = BKE_curveprofile_add(PROF_PRESET_LINE);

  /* Sequencer */
  scene->toolsettings->sequencer_tool_settings = blender::seq::tool_settings_init();

  for (size_t i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
    scene->orientation_slots[i].index_custom = -1;
  }

  /* Master Collection */
  scene->master_collection = BKE_collection_master_add(scene);

  BKE_view_layer_add(scene, DATA_("ViewLayer"), nullptr, VIEWLAYER_ADD_NEW);

  scene->runtime = MEM_new<SceneRuntime>(__func__);
}

static void scene_copy_data(Main *bmain,
                            std::optional<Library *> owner_library,
                            ID *id_dst,
                            const ID *id_src,
                            const int flag)
{
  Scene *scene_dst = (Scene *)id_dst;
  const Scene *scene_src = (const Scene *)id_src;
  /* Never handle user-count here for own sub-data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;
  /* Always need allocation of the embedded ID data. */
  const int flag_embedded_id_data = flag_subdata & ~LIB_ID_CREATE_NO_ALLOCATE;

  scene_dst->ed = nullptr;
  scene_dst->depsgraph_hash = nullptr;
  scene_dst->fps_info = nullptr;

  /* Master Collection */
  if (scene_src->master_collection) {
    BKE_id_copy_in_lib(bmain,
                       owner_library,
                       &scene_src->master_collection->id,
                       &scene_dst->id,
                       reinterpret_cast<ID **>(&scene_dst->master_collection),
                       flag_embedded_id_data);
  }

  /* View Layers */
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene_src->view_layers) {
    BKE_view_layer_synced_ensure(scene_src, view_layer);
  }
  BLI_duplicatelist(&scene_dst->view_layers, &scene_src->view_layers);
  for (ViewLayer *view_layer_src = static_cast<ViewLayer *>(scene_src->view_layers.first),
                 *view_layer_dst = static_cast<ViewLayer *>(scene_dst->view_layers.first);
       view_layer_src;
       view_layer_src = view_layer_src->next, view_layer_dst = view_layer_dst->next)
  {
    BKE_view_layer_copy_data(scene_dst, scene_src, view_layer_dst, view_layer_src, flag_subdata);
  }

  BKE_copy_time_markers(scene_dst->markers, scene_src->markers, flag);

  BLI_duplicatelist(&(scene_dst->transform_spaces), &(scene_src->transform_spaces));
  BLI_duplicatelist(&(scene_dst->r.views), &(scene_src->r.views));
  BKE_keyingsets_copy(&(scene_dst->keyingsets), &(scene_src->keyingsets));

  if (scene_src->rigidbody_world) {
    scene_dst->rigidbody_world = BKE_rigidbody_world_copy(scene_src->rigidbody_world,
                                                          flag_subdata);
  }

  /* copy color management settings */
  BKE_color_managed_display_settings_copy(&scene_dst->display_settings,
                                          &scene_src->display_settings);
  BKE_color_managed_view_settings_copy(&scene_dst->view_settings, &scene_src->view_settings);
  BKE_color_managed_colorspace_settings_copy(&scene_dst->sequencer_colorspace_settings,
                                             &scene_src->sequencer_colorspace_settings);

  BKE_image_format_copy(&scene_dst->r.im_format, &scene_src->r.im_format);
  BKE_image_format_copy(&scene_dst->r.bake.im_format, &scene_src->r.bake.im_format);

  BKE_curvemapping_copy_data(&scene_dst->r.mblur_shutter_curve, &scene_src->r.mblur_shutter_curve);

  /* tool settings */
  scene_dst->toolsettings = BKE_toolsettings_copy(scene_src->toolsettings, flag_subdata);

  if (scene_src->display.shading.prop) {
    scene_dst->display.shading.prop = IDP_CopyProperty(scene_src->display.shading.prop);
  }

  BKE_sound_reset_scene_runtime(scene_dst);

  /* Copy sequencer, this is local data! */
  if (scene_src->ed) {
    scene_dst->ed = MEM_callocN<Editing>(__func__);
    scene_dst->ed->cache_flag = scene_src->ed->cache_flag;
    scene_dst->ed->show_missing_media_flag = scene_src->ed->show_missing_media_flag;
    scene_dst->ed->proxy_storage = scene_src->ed->proxy_storage;
    STRNCPY(scene_dst->ed->proxy_dir, scene_src->ed->proxy_dir);
    blender::seq::seqbase_duplicate_recursive(bmain,
                                              scene_src,
                                              scene_dst,
                                              &scene_dst->ed->seqbase,
                                              &scene_src->ed->seqbase,
                                              blender::seq::StripDuplicate::All,
                                              flag_subdata);
    BLI_duplicatelist(&scene_dst->ed->channels, &scene_src->ed->channels);
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&scene_dst->id, &scene_src->id);
  }
  else {
    scene_dst->preview = nullptr;
  }

  BKE_scene_copy_data_eevee(scene_dst, scene_src);

  scene_dst->runtime = MEM_new<SceneRuntime>(__func__);
}

static void scene_free_markers(Scene *scene, bool do_id_user)
{
  LISTBASE_FOREACH_MUTABLE (TimeMarker *, marker, &scene->markers) {
    if (marker->prop != nullptr) {
      IDP_FreePropertyContent_ex(marker->prop, do_id_user);
      MEM_freeN(marker->prop);
    }
    MEM_freeN(marker);
  }
}

static void scene_free_data(ID *id)
{
  Scene *scene = (Scene *)id;
  const bool do_id_user = false;

  blender::seq::editing_free(scene, do_id_user);

  BKE_keyingsets_free(&scene->keyingsets);

  BLI_assert_msg(scene->nodetree == nullptr,
                 "Pointer should not be valid after blend file reading.");

  if (scene->rigidbody_world) {
    /* Prevent rigidbody freeing code to follow other IDs pointers, this should never be allowed
     * nor necessary from here, and with new undo code, those pointers may be fully invalid or
     * worse, pointing to data actually belonging to new BMain! */
    scene->rigidbody_world->constraints = nullptr;
    scene->rigidbody_world->group = nullptr;
    BKE_rigidbody_free_world(scene);
  }

  scene_free_markers(scene, do_id_user);
  BLI_freelistN(&scene->transform_spaces);
  BLI_freelistN(&scene->r.views);

  BKE_toolsettings_free(scene->toolsettings);
  scene->toolsettings = nullptr;

  BKE_scene_free_depsgraph_hash(scene);

  MEM_SAFE_FREE(scene->fps_info);

  BKE_sound_destroy_scene(scene);

  BKE_color_managed_view_settings_free(&scene->view_settings);
  BKE_image_format_free(&scene->r.im_format);
  BKE_image_format_free(&scene->r.bake.im_format);

  BKE_previewimg_free(&scene->preview);
  BKE_curvemapping_free_data(&scene->r.mblur_shutter_curve);

  LISTBASE_FOREACH_MUTABLE (ViewLayer *, view_layer, &scene->view_layers) {
    BLI_remlink(&scene->view_layers, view_layer);
    BKE_view_layer_free_ex(view_layer, do_id_user);
  }

  /* Master Collection */
  /* TODO: what to do with do_id_user? it's also true when just
   * closing the file which seems wrong? should decrement users
   * for objects directly in the master collection? then other
   * collections in the scene need to do it too? */
  if (scene->master_collection) {
    BKE_collection_free_data(scene->master_collection);
    BKE_libblock_free_data_py(&scene->master_collection->id);
    MEM_freeN(scene->master_collection);
    scene->master_collection = nullptr;
  }

  if (scene->display.shading.prop) {
    IDP_FreeProperty(scene->display.shading.prop);
    scene->display.shading.prop = nullptr;
  }

  /* These are freed on `do_versions`. */
  BLI_assert(scene->layer_properties == nullptr);

  MEM_delete(scene->runtime);
}

static void scene_foreach_rigidbodyworldSceneLooper(RigidBodyWorld * /*rbw*/,
                                                    ID **id_pointer,
                                                    void *user_data,
                                                    const LibraryForeachIDCallbackFlag cb_flag)
{
  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
      data, BKE_lib_query_foreachid_process(data, id_pointer, cb_flag));
}

/**
 * This code is shared by both the regular `foreach_id` looper, and the code trying to restore or
 * preserve ID pointers like brushes across undo-steps.
 */
enum eSceneForeachUndoPreserveProcess {
  /* Undo when preserving tool-settings from old scene, we also want to try to preserve that ID
   * pointer from its old scene's value. */
  SCENE_FOREACH_UNDO_RESTORE,
  /* Undo when preserving tool-settings from old scene, we want to keep the new value of that ID
   * pointer. */
  SCENE_FOREACH_UNDO_NO_RESTORE,
};

static void scene_foreach_toolsettings_id_pointer_process(
    ID **id_p,
    const eSceneForeachUndoPreserveProcess action,
    BlendLibReader *reader,
    ID **id_old_p,
    const uint cb_flag)
{
  switch (action) {
    case SCENE_FOREACH_UNDO_RESTORE: {
      ID *id_old = *id_old_p;
      /* Old data has not been remapped to new values of the pointers, if we want to keep the old
       * pointer here we need its new address. */
      ID *id_old_new = id_old != nullptr ? BLO_read_get_new_id_address_from_session_uid(
                                               reader, id_old->session_uid) :
                                           nullptr;
      /* The new address may be the same as the old one, in which case there is nothing to do. */
      if (id_old_new == id_old) {
        break;
      }
      if (id_old_new != nullptr) {
        BLI_assert(id_old == id_old_new->orig_id);
        *id_old_p = id_old_new;
        if (cb_flag & IDWALK_CB_USER) {
          id_us_plus_no_lib(id_old_new);
          id_us_min(id_old);
        }
        break;
      }

      /* We failed to find a new valid pointer for the previous ID, just keep the current one as
       * if we had been under #SCENE_FOREACH_UNDO_NO_RESTORE case.
       *
       * There is a nasty twist here though: a previous call to 'undo_preserve' on the Scene ID may
       * have modified it, even though the undo step detected it as unmodified. In such case, the
       * value of `*id_p` may end up also pointing to an invalid (no more in newly read Main) ID,
       * so it also needs to be checked from its `session_uid`. */
      ID *id = *id_p;
      ID *id_new = id != nullptr ?
                       BLO_read_get_new_id_address_from_session_uid(reader, id->session_uid) :
                       nullptr;
      if (id_new != id) {
        *id_p = id_new;
        if (cb_flag & IDWALK_CB_USER) {
          id_us_plus_no_lib(id_new);
          id_us_min(id);
        }
      }
      std::swap(*id_p, *id_old_p);
      break;
    }
    case SCENE_FOREACH_UNDO_NO_RESTORE:
      /* Counteract the swap of the whole ToolSettings container struct. */
      std::swap(*id_p, *id_old_p);
      break;
  }
}

/**
 * Special handling is needed here, as `scene_foreach_toolsettings` (and its dependency
 * `scene_foreach_paint`) are also used by `scene_undo_preserve`,
 * where `LibraryForeachIDData *data` is nullptr.
 */
#define BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P( \
    _data, _id_p, _do_undo_restore, _action, _reader, _id_old_p, _cb_flag) \
  { \
    if (_do_undo_restore) { \
      scene_foreach_toolsettings_id_pointer_process( \
          (ID **)(_id_p), _action, _reader, (ID **)(_id_old_p), _cb_flag); \
    } \
    else { \
      BLI_assert((_data) != nullptr); \
      BKE_LIB_FOREACHID_PROCESS_IDSUPER_P(_data, _id_p, _cb_flag); \
    } \
  } \
  (void)0

#define BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL( \
    _data, _do_undo_restore, _func_call) \
  { \
    if (_do_undo_restore) { \
      _func_call; \
    } \
    else { \
      BLI_assert((_data) != nullptr); \
      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(_data, _func_call); \
    } \
  } \
  (void)0

static void scene_foreach_paint(LibraryForeachIDData *data,
                                Paint *paint,
                                const bool do_undo_restore,
                                BlendLibReader *reader,
                                Paint *paint_old)
{
  /* `paint` may be nullptr in 'undo_preserve' case, when the relevant sub-data does not exist in
   * newly read toolsettings, but does exist in old existing ones.
   *
   * This function should never be called in case the old toolsettings do not have the relevant
   * `paint_old` data. */
  BLI_assert(paint_old != nullptr);

  Brush *brush_tmp = nullptr;
  Brush **brush_p = paint ? &paint->brush : &brush_tmp;
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    brush_p,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_RESTORE,
                                                    reader,
                                                    &paint_old->brush,
                                                    IDWALK_CB_NOP);

  Brush *eraser_brush_tmp = nullptr;
  Brush **eraser_brush_p = paint ? &paint->eraser_brush : &eraser_brush_tmp;
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    eraser_brush_p,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_RESTORE,
                                                    reader,
                                                    &paint_old->eraser_brush,
                                                    IDWALK_CB_NOP);

  Palette *palette_tmp = nullptr;
  Palette **palette_p = paint ? &paint->palette : &palette_tmp;
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    palette_p,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_RESTORE,
                                                    reader,
                                                    &paint_old->palette,
                                                    IDWALK_CB_USER);
}

static void scene_foreach_toolsettings(LibraryForeachIDData *data,
                                       ToolSettings *toolsett,
                                       const bool do_undo_restore,
                                       BlendLibReader *reader,
                                       ToolSettings *toolsett_old)
{
  /* In regular foreach_id case, only one set of data is processed, both pointers are expected to
   * be the same.
   *
   * In undo_preserve case, both pointers may be different (see #lib_link_all for why they may be
   * the same in some cases). */
  BLI_assert(do_undo_restore || (toolsett == toolsett_old));
  BLI_assert(!ELEM(nullptr, toolsett, toolsett_old));

  /* NOTE: In 'undo_preserve' case, the 'old' data is the source of truth here, since it is the one
   * that will be re-used in newly read Main and therefore needs valid, existing in new Main, ID
   * pointers. */

  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    &toolsett->particle.scene,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_NO_RESTORE,
                                                    reader,
                                                    &toolsett_old->particle.scene,
                                                    IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    &toolsett->particle.object,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_NO_RESTORE,
                                                    reader,
                                                    &toolsett_old->particle.object,
                                                    IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    &toolsett->particle.shape_object,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_NO_RESTORE,
                                                    reader,
                                                    &toolsett_old->particle.shape_object,
                                                    IDWALK_CB_NOP);

  scene_foreach_paint(
      data, &toolsett->imapaint.paint, do_undo_restore, reader, &toolsett_old->imapaint.paint);
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    &toolsett->imapaint.stencil,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_RESTORE,
                                                    reader,
                                                    &toolsett_old->imapaint.stencil,
                                                    IDWALK_CB_USER);
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    &toolsett->imapaint.clone,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_RESTORE,
                                                    reader,
                                                    &toolsett_old->imapaint.clone,
                                                    IDWALK_CB_USER);
  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                    &toolsett->imapaint.canvas,
                                                    do_undo_restore,
                                                    SCENE_FOREACH_UNDO_RESTORE,
                                                    reader,
                                                    &toolsett_old->imapaint.canvas,
                                                    IDWALK_CB_USER);

  Paint *paint, *paint_old;

  if (toolsett_old->vpaint) {
    paint = toolsett->vpaint ? &toolsett->vpaint->paint : nullptr;
    paint_old = &toolsett_old->vpaint->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }
  if (toolsett_old->wpaint) {
    paint = toolsett->wpaint ? &toolsett->wpaint->paint : nullptr;
    paint_old = &toolsett_old->wpaint->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }
  if (toolsett_old->sculpt) {
    paint = toolsett->sculpt ? &toolsett->sculpt->paint : nullptr;
    paint_old = &toolsett_old->sculpt->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));

    /* WARNING: Handling this object pointer is fairly intricated, to support both 'regular'
     * foreach_id processing (in which case both sets of data, current and old, are the same), and
     * the restore-after-undo cases. It does not have a helper, because so far it is the only case
     * of having to deal with non-'paint' data in a sub-toolsett struct. */
    Object *gravity_object = toolsett->sculpt ? toolsett->sculpt->gravity_object : nullptr;
    Object *gravity_object_old = toolsett_old->sculpt->gravity_object;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(data,
                                                      &gravity_object,
                                                      do_undo_restore,
                                                      SCENE_FOREACH_UNDO_NO_RESTORE,
                                                      reader,
                                                      &gravity_object_old,
                                                      IDWALK_CB_NOP);
    if (toolsett->sculpt) {
      toolsett->sculpt->gravity_object = gravity_object;
    }
    /* Do not re-assign `gravity_object_old` object if both current and old data are the same
     * (foreach_id case), that would nullify assignment above, making remapping cases fail. */
    if (toolsett_old != toolsett) {
      toolsett_old->sculpt->gravity_object = gravity_object_old;
    }
  }
  if (toolsett_old->gp_paint) {
    paint = toolsett->gp_paint ? &toolsett->gp_paint->paint : nullptr;
    paint_old = &toolsett_old->gp_paint->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }
  if (toolsett_old->gp_vertexpaint) {
    paint = toolsett->gp_vertexpaint ? &toolsett->gp_vertexpaint->paint : nullptr;
    paint_old = &toolsett_old->gp_vertexpaint->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }
  if (toolsett_old->gp_sculptpaint) {
    paint = toolsett->gp_sculptpaint ? &toolsett->gp_sculptpaint->paint : nullptr;
    paint_old = &toolsett_old->gp_sculptpaint->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }
  if (toolsett_old->gp_weightpaint) {
    paint = toolsett->gp_weightpaint ? &toolsett->gp_weightpaint->paint : nullptr;
    paint_old = &toolsett_old->gp_weightpaint->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }
  if (toolsett_old->curves_sculpt) {
    paint = toolsett->curves_sculpt ? &toolsett->curves_sculpt->paint : nullptr;
    paint_old = &toolsett_old->curves_sculpt->paint;
    BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL(
        data,
        do_undo_restore,
        scene_foreach_paint(data, paint, do_undo_restore, reader, paint_old));
  }

  BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER_P(
      data,
      &toolsett->gp_sculpt.guide.reference_object,
      do_undo_restore,
      SCENE_FOREACH_UNDO_NO_RESTORE,
      reader,
      &toolsett_old->gp_sculpt.guide.reference_object,
      IDWALK_CB_NOP);
}

#undef BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_IDSUPER
#undef BKE_LIB_FOREACHID_UNDO_PRESERVE_PROCESS_FUNCTION_CALL

static void scene_foreach_layer_collection(LibraryForeachIDData *data,
                                           ListBase *lb,
                                           const bool is_master)
{
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);

  LISTBASE_FOREACH (LayerCollection *, lc, lb) {
    if ((data_flags & IDWALK_NO_ORIG_POINTERS_ACCESS) == 0 && lc->collection != nullptr) {
      BLI_assert(is_master == ((lc->collection->id.flag & ID_FLAG_EMBEDDED_DATA) != 0));
    }
    const LibraryForeachIDCallbackFlag cb_flag = is_master ? IDWALK_CB_EMBEDDED_NOT_OWNING :
                                                             IDWALK_CB_NOP;
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, lc->collection, cb_flag | IDWALK_CB_DIRECT_WEAK_LINK);
    scene_foreach_layer_collection(data, &lc->layer_collections, false);
  }
}

static bool strip_foreach_member_id_cb(Strip *strip, void *user_data)
{
  LibraryForeachIDData *data = static_cast<LibraryForeachIDData *>(user_data);

/* Only for deprecated data. */
#define FOREACHID_PROCESS_ID_NOCHECK(_data, _id_super, _cb_flag) \
  { \
    BKE_lib_query_foreachid_process((_data), reinterpret_cast<ID **>(&(_id_super)), (_cb_flag)); \
    if (BKE_lib_query_foreachid_iter_stop(_data)) { \
      return false; \
    } \
  } \
  ((void)0)

#define FOREACHID_PROCESS_IDSUPER(_data, _id_super, _cb_flag) \
  { \
    CHECK_TYPE(&((_id_super)->id), ID *); \
    FOREACHID_PROCESS_ID_NOCHECK(_data, _id_super, _cb_flag); \
  } \
  ((void)0)

  FOREACHID_PROCESS_IDSUPER(data, strip->scene, IDWALK_CB_NEVER_SELF);
  FOREACHID_PROCESS_IDSUPER(data, strip->scene_camera, IDWALK_CB_NOP);
  FOREACHID_PROCESS_IDSUPER(data, strip->clip, IDWALK_CB_USER);
  FOREACHID_PROCESS_IDSUPER(data, strip->mask, IDWALK_CB_USER);
  FOREACHID_PROCESS_IDSUPER(data, strip->sound, IDWALK_CB_USER);
  IDP_foreach_property(strip->prop, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
    BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
  });
  IDP_foreach_property(strip->system_properties, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
    BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
  });
  /* TODO: This could use `seq::foreach_strip_modifier_id`, but because `FOREACHID_PROCESS_IDSUPER`
   * doesn't take IDs but "ID supers", it makes it a bit more cumbersome. */
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    FOREACHID_PROCESS_IDSUPER(data, smd->mask_id, IDWALK_CB_USER);
    if (smd->type == eSeqModifierType_Compositor) {
      auto *modifier_data = reinterpret_cast<SequencerCompositorModifierData *>(smd);
      FOREACHID_PROCESS_IDSUPER(data, modifier_data->node_group, IDWALK_CB_USER);
    }
  }

  if (strip->type == STRIP_TYPE_TEXT && strip->effectdata) {
    TextVars *text_data = static_cast<TextVars *>(strip->effectdata);
    FOREACHID_PROCESS_IDSUPER(data, text_data->text_font, IDWALK_CB_USER);
  }

#undef FOREACHID_PROCESS_IDSUPER
#undef FOREACHID_PROCESS_ID_NOCHECK

  return true;
}

static void scene_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Scene *scene = reinterpret_cast<Scene *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->camera, IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->world, IDWALK_CB_USER);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->set, IDWALK_CB_NEVER_SELF);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->clip, IDWALK_CB_USER);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->gpd, IDWALK_CB_USER);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->r.bake.cage_object, IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scene->compositing_node_group, IDWALK_CB_USER);

  if (scene->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&scene->nodetree));
  }
  if (scene->ed) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, blender::seq::foreach_strip(&scene->ed->seqbase, strip_foreach_member_id_cb, data));
  }

  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                          BKE_keyingsets_foreach_id(data, &scene->keyingsets));

  /* This pointer can be nullptr during old files reading, better be safe than sorry. */
  if (scene->master_collection != nullptr) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&scene->master_collection));
  }

  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, view_layer->mat_override, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, view_layer->world_override, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data,
        IDP_foreach_property(view_layer->id_properties, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
          BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
        }));
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data,
        IDP_foreach_property(
            view_layer->system_properties, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
              BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
            }));

    /* FIXME: Although ideally this should always have access to synced data, this is not always
     * the case (FOREACH_ID can be called in context where re-syncing is blocked, while effectively
     * modifying the view-layer or collections data, see e.g. #id_delete code which remaps all
     * deleted ID usages to null).
     *
     * There is no obvious solution to this problem, so for now working around with some 'band-aid'
     * special code and asserts.
     *
     * In the future, there may be need for a new `IDWALK_CB` flag to mark existing pointer values
     * as unsafe to access in such cases. */
    const bool is_synced = BKE_view_layer_synced_ensure(scene, view_layer);
    if (!is_synced) {
      BLI_assert_msg((flag & IDWALK_RECURSE) == 0,
                     "foreach_id should never recurse in case it cannot ensure that all "
                     "view-layers are in synced with their collections");
    }
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_unsynced_get(view_layer)) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(
          data,
          base->object,
          IDWALK_CB_NOP | IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE | IDWALK_CB_DIRECT_WEAK_LINK);
    }

    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, scene_foreach_layer_collection(data, &view_layer->layer_collections, true));

    LISTBASE_FOREACH (FreestyleModuleConfig *, fmc, &view_layer->freestyle_config.modules) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fmc->script, IDWALK_CB_NOP);
    }

    LISTBASE_FOREACH (FreestyleLineSet *, fls, &view_layer->freestyle_config.linesets) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fls->group, IDWALK_CB_USER);
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fls->linestyle, IDWALK_CB_USER);
    }
  }

  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, marker->camera, IDWALK_CB_NOP);
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, IDP_foreach_property(marker->prop, IDP_TYPE_FILTER_ID, [&](IDProperty *prop) {
          BKE_lib_query_idpropertiesForeachIDLink_callback(prop, data);
        }));
  }

  ToolSettings *toolsett = scene->toolsettings;
  if (toolsett) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, scene_foreach_toolsettings(data, toolsett, false, nullptr, toolsett));
  }

  if (scene->rigidbody_world) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data,
        BKE_rigidbody_world_id_loop(
            scene->rigidbody_world, scene_foreach_rigidbodyworldSceneLooper, data));
  }

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    LISTBASE_FOREACH_MUTABLE (Base *, base_legacy, &scene->base) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, base_legacy->object, IDWALK_CB_NOP);
    }

    LISTBASE_FOREACH (SceneRenderLayer *, srl, &scene->r.layers) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, srl->mat_override, IDWALK_CB_USER);
      LISTBASE_FOREACH (FreestyleModuleConfig *, fmc, &srl->freestyleConfig.modules) {
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fmc->script, IDWALK_CB_NOP);
      }
      LISTBASE_FOREACH (FreestyleLineSet *, fls, &srl->freestyleConfig.linesets) {
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fls->linestyle, IDWALK_CB_USER);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, fls->group, IDWALK_CB_USER);
      }
    }
  }
}

static bool strip_foreach_path_callback(Strip *strip, void *user_data)
{
  if (STRIP_HAS_PATH(strip)) {
    StripElem *se = strip->data->stripdata;
    BPathForeachPathData *bpath_data = (BPathForeachPathData *)user_data;

    if (ELEM(strip->type, STRIP_TYPE_MOVIE, STRIP_TYPE_SOUND_RAM) && se) {
      BKE_bpath_foreach_path_dirfile_fixed_process(bpath_data,
                                                   strip->data->dirpath,
                                                   sizeof(strip->data->dirpath),
                                                   se->filename,
                                                   sizeof(se->filename));
    }
    else if ((strip->type == STRIP_TYPE_IMAGE) && se) {
      /* NOTE: An option not to loop over all strips could be useful? */
      uint len = uint(MEM_allocN_len(se)) / uint(sizeof(*se));
      uint i;

      if (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE) {
        /* only operate on one path */
        len = std::min(1u, len);
      }

      for (i = 0; i < len; i++, se++) {
        BKE_bpath_foreach_path_dirfile_fixed_process(bpath_data,
                                                     strip->data->dirpath,
                                                     sizeof(strip->data->dirpath),
                                                     se->filename,
                                                     sizeof(se->filename));
      }
    }
    else {
      /* simple case */
      BKE_bpath_foreach_path_fixed_process(
          bpath_data, strip->data->dirpath, sizeof(strip->data->dirpath));
    }
  }
  return true;
}

static void scene_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  Scene *scene = (Scene *)id;
  if (scene->ed != nullptr) {
    blender::seq::foreach_strip(&scene->ed->seqbase, strip_foreach_path_callback, bpath_data);
  }
}

static void scene_foreach_working_space_color(ID *id, const IDTypeForeachColorFunctionCallback &fn)
{
  Scene *scene = (Scene *)id;

  BKE_paint_settings_foreach_mode(scene->toolsettings, [&fn](Paint *paint) {
    fn.single(paint->unified_paint_settings.color);
    fn.single(paint->unified_paint_settings.secondary_color);
  });
}

static void scene_foreach_cache(ID *id,
                                IDTypeForeachCacheFunctionCallback function_callback,
                                void *user_data)
{
  Scene *scene = (Scene *)id;
  if (scene->ed != nullptr) {
    IDCacheKey key;
    key.id_session_uid = id->session_uid;
    /* Preserve VSE thumbnail cache across global undo steps. */
    key.identifier = offsetof(Editing, runtime.thumbnail_cache);
    function_callback(id, &key, (void **)&scene->ed->runtime.thumbnail_cache, 0, user_data);
  }
}

static void scene_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Scene *sce = (Scene *)id;
  const bool is_write_undo = BLO_write_is_undo(writer);

  if (is_write_undo) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    /* XXX This UI data should not be stored in Scene at all... */
    sce->cursor = View3DCursor{};
  }

  /* Todo(#140111): Forward compatibility support will be removed in 6.0. Do not initialize the
   * address of `scene->nodetree` anymore. */
  if (sce->compositing_node_group && !is_write_undo) {
    /* #Scene::nodetree is written for forward compatibility.
     * The pointer must be valid before writing the scene. */
    /* We need a valid, unique (within that Scene ID) memory address as 'UID' of the written
     * embedded node tree. The simplest and safest solution to obtain this is to actually allocate
     * a dummy byte. */
    sce->nodetree = reinterpret_cast<bNodeTree *>(MEM_mallocN(1, "dummy pointer"));
  }

  /* Todo(#140111): Forward compatibility support will be removed in 6.0. Remove mapping between
   * `scene->use_nodes` and `scene->r.scemode`. */
  if (sce->compositing_node_group && sce->r.scemode & R_DOCOMP) {
    sce->use_nodes = true;
  }

  /* write LibData */
  BLO_write_id_struct(writer, Scene, id_address, &sce->id);
  BKE_id_blend_write(writer, &sce->id);

  BKE_keyingsets_blend_write(writer, &sce->keyingsets);

  /* direct data */
  ToolSettings *ts = sce->toolsettings;

  BLO_write_struct(writer, ToolSettings, ts);

  if (ts->unified_paint_settings.curve_rand_hue) {
    BKE_curvemapping_blend_write(writer, ts->unified_paint_settings.curve_rand_hue);
  }

  if (ts->unified_paint_settings.curve_rand_saturation) {
    BKE_curvemapping_blend_write(writer, ts->unified_paint_settings.curve_rand_saturation);
  }

  if (ts->unified_paint_settings.curve_rand_value) {
    BKE_curvemapping_blend_write(writer, ts->unified_paint_settings.curve_rand_value);
  }

  if (ts->vpaint) {
    BLO_write_struct(writer, VPaint, ts->vpaint);
    BKE_paint_blend_write(writer, &ts->vpaint->paint);
  }
  if (ts->wpaint) {
    BLO_write_struct(writer, VPaint, ts->wpaint);
    BKE_paint_blend_write(writer, &ts->wpaint->paint);
  }
  if (ts->sculpt) {
    BLO_write_struct(writer, Sculpt, ts->sculpt);
    if (ts->sculpt->automasking_cavity_curve) {
      BKE_curvemapping_blend_write(writer, ts->sculpt->automasking_cavity_curve);
    }
    if (ts->sculpt->automasking_cavity_curve_op) {
      BKE_curvemapping_blend_write(writer, ts->sculpt->automasking_cavity_curve_op);
    }

    BKE_paint_blend_write(writer, &ts->sculpt->paint);
  }
  if (ts->uvsculpt.curve_distance_falloff) {
    BKE_curvemapping_blend_write(writer, ts->uvsculpt.curve_distance_falloff);
  }
  if (ts->gp_paint) {
    BLO_write_struct(writer, GpPaint, ts->gp_paint);
    BKE_paint_blend_write(writer, &ts->gp_paint->paint);
  }
  if (ts->gp_vertexpaint) {
    BLO_write_struct(writer, GpVertexPaint, ts->gp_vertexpaint);
    BKE_paint_blend_write(writer, &ts->gp_vertexpaint->paint);
  }
  if (ts->gp_sculptpaint) {
    BLO_write_struct(writer, GpSculptPaint, ts->gp_sculptpaint);
    BKE_paint_blend_write(writer, &ts->gp_sculptpaint->paint);
  }
  if (ts->gp_weightpaint) {
    BLO_write_struct(writer, GpWeightPaint, ts->gp_weightpaint);
    BKE_paint_blend_write(writer, &ts->gp_weightpaint->paint);
  }
  if (ts->curves_sculpt) {
    BLO_write_struct(writer, CurvesSculpt, ts->curves_sculpt);
    BKE_paint_blend_write(writer, &ts->curves_sculpt->paint);
  }
  /* write grease-pencil custom ipo curve to file */
  if (ts->gp_interpolate.custom_ipo) {
    BKE_curvemapping_blend_write(writer, ts->gp_interpolate.custom_ipo);
  }
  /* write grease-pencil multi-frame falloff curve to file */
  if (ts->gp_sculpt.cur_falloff) {
    BKE_curvemapping_blend_write(writer, ts->gp_sculpt.cur_falloff);
  }
  /* write grease-pencil primitive curve to file */
  if (ts->gp_sculpt.cur_primitive) {
    BKE_curvemapping_blend_write(writer, ts->gp_sculpt.cur_primitive);
  }
  /* Write the curve profile to the file. */
  if (ts->custom_bevel_profile_preset) {
    BKE_curveprofile_blend_write(writer, ts->custom_bevel_profile_preset);
  }
  if (ts->sequencer_tool_settings) {
    BLO_write_struct(writer, SequencerToolSettings, ts->sequencer_tool_settings);
  }

  BKE_paint_blend_write(writer, &ts->imapaint.paint);

  Editing *ed = sce->ed;
  if (ed) {
    BLO_write_struct(writer, Editing, ed);

    blender::seq::blend_write(writer, &ed->seqbase);
    LISTBASE_FOREACH (SeqTimelineChannel *, channel, &ed->channels) {
      BLO_write_struct(writer, SeqTimelineChannel, channel);
    }
    /* new; meta stack too, even when its nasty restore code */
    LISTBASE_FOREACH (MetaStack *, ms, &ed->metastack) {
      BLO_write_struct(writer, MetaStack, ms);
    }
  }

  /* writing dynamic list of TimeMarkers to the blend file */
  BKE_time_markers_blend_write(writer, sce->markers);

  /* writing dynamic list of TransformOrientations to the blend file */
  LISTBASE_FOREACH (TransformOrientation *, ts, &sce->transform_spaces) {
    BLO_write_struct(writer, TransformOrientation, ts);
  }

  /* writing MultiView to the blend file */
  LISTBASE_FOREACH (SceneRenderView *, srv, &sce->r.views) {
    BLO_write_struct(writer, SceneRenderView, srv);
  }

  /* Todo(#140111): Forward compatibility support will be removed in 6.0. Do not write an embedded
   * nodetree at `scene->nodetree` anymore. */
  if (sce->compositing_node_group && !is_write_undo) {
    BLO_Write_IDBuffer temp_embedded_id_buffer{sce->compositing_node_group->id, writer};
    bNodeTree *temp_nodetree = reinterpret_cast<bNodeTree *>(temp_embedded_id_buffer.get());
    temp_nodetree->id.flag |= ID_FLAG_EMBEDDED_DATA;
    temp_nodetree->owner_id = &sce->id;
    temp_nodetree->id.lib = sce->id.lib;
    /* Set deprecated chunksize for forward compatibility. */
    temp_nodetree->chunksize = 256;
    BLO_write_struct_at_address(writer, bNodeTree, sce->nodetree, temp_nodetree);
    blender::bke::node_tree_blend_write(writer, temp_nodetree);
    MEM_freeN(reinterpret_cast<void *>(sce->nodetree));
    sce->nodetree = nullptr;
  }

  BKE_color_managed_view_settings_blend_write(writer, &sce->view_settings);
  BKE_image_format_blend_write(writer, &sce->r.im_format);
  BKE_image_format_blend_write(writer, &sce->r.bake.im_format);

  /* writing RigidBodyWorld data to the blend file */
  if (sce->rigidbody_world) {
    /* Set deprecated pointers to prevent crashes of older Blenders */
    sce->rigidbody_world->pointcache = sce->rigidbody_world->shared->pointcache;
    sce->rigidbody_world->ptcaches = sce->rigidbody_world->shared->ptcaches;
    BLO_write_struct(writer, RigidBodyWorld, sce->rigidbody_world);

    BLO_write_struct(writer, RigidBodyWorld_Shared, sce->rigidbody_world->shared);
    BLO_write_struct(writer, EffectorWeights, sce->rigidbody_world->effector_weights);
    BKE_ptcache_blend_write(writer, &(sce->rigidbody_world->shared->ptcaches));
  }

  BKE_previewimg_blend_write(writer, sce->preview);
  BKE_curvemapping_curves_blend_write(writer, &sce->r.mblur_shutter_curve);

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    BKE_view_layer_blend_write(writer, sce, view_layer);
  }

  if (sce->master_collection) {
    BLO_Write_IDBuffer temp_embedded_id_buffer{sce->master_collection->id, writer};
    Collection *temp_collection = reinterpret_cast<Collection *>(temp_embedded_id_buffer.get());
    BKE_collection_blend_write_prepare_nolib(writer, temp_collection);
    BLO_write_struct_at_address(writer, Collection, sce->master_collection, temp_collection);
    BKE_collection_blend_write_nolib(writer, temp_collection);
  }

  BKE_screen_view3d_shading_blend_write(writer, &sce->display.shading);

  /* Freed on `do_versions()`. */
  BLI_assert(sce->layer_properties == nullptr);
}

static void direct_link_paint_helper(BlendDataReader *reader, const Scene *scene, Paint **paint)
{
  /* TODO: is this needed. */
  BLO_read_struct(reader, Paint, paint);

  if (*paint) {
    BKE_paint_blend_read_data(reader, scene, *paint);
  }
}

static void link_recurs_seq(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_struct_list(reader, Strip, lb);

  LISTBASE_FOREACH_MUTABLE (Strip *, strip, lb) {
    /* Sanity check. */
    if (!blender::seq::is_valid_strip_channel(strip)) {
      BLI_freelinkN(lb, strip);
      BLO_read_data_reports(reader)->count.sequence_strips_skipped++;
    }
    else if (strip->seqbase.first) {
      link_recurs_seq(reader, &strip->seqbase);
    }
  }
}

static void scene_blend_read_data(BlendDataReader *reader, ID *id)
{
  Scene *sce = (Scene *)id;

  sce->depsgraph_hash = nullptr;
  sce->fps_info = nullptr;

  sce->customdata_mask = CustomData_MeshMasks{};
  sce->customdata_mask_modal = CustomData_MeshMasks{};

  BKE_sound_reset_scene_runtime(sce);

  /* set users to one by default, not in lib-link, this will increase it for compo nodes */
  id_us_ensure_real(&sce->id);

  sce->runtime = MEM_new<SceneRuntime>(__func__);

  BLO_read_struct_list(reader, Base, &(sce->base));

  BLO_read_struct_list(reader, KeyingSet, &sce->keyingsets);
  BKE_keyingsets_blend_read_data(reader, &sce->keyingsets);

  BLO_read_struct(reader, Base, &sce->basact);

  BLO_read_struct(reader, ToolSettings, &sce->toolsettings);
  if (sce->toolsettings) {
    UnifiedPaintSettings *ups = &sce->toolsettings->unified_paint_settings;

    BLO_read_struct(reader, CurveMapping, &ups->curve_rand_hue);
    if (ups->curve_rand_hue) {
      BKE_curvemapping_blend_read(reader, ups->curve_rand_hue);
      BKE_curvemapping_init(ups->curve_rand_hue);
    }

    BLO_read_struct(reader, CurveMapping, &ups->curve_rand_saturation);
    if (ups->curve_rand_saturation) {
      BKE_curvemapping_blend_read(reader, ups->curve_rand_saturation);
      BKE_curvemapping_init(ups->curve_rand_saturation);
    }

    BLO_read_struct(reader, CurveMapping, &ups->curve_rand_value);
    if (ups->curve_rand_value) {
      BKE_curvemapping_blend_read(reader, ups->curve_rand_value);
      BKE_curvemapping_init(ups->curve_rand_value);
    }

    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->sculpt);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->vpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->wpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_paint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_vertexpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_sculptpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->gp_weightpaint);
    direct_link_paint_helper(reader, sce, (Paint **)&sce->toolsettings->curves_sculpt);

    BKE_paint_blend_read_data(reader, sce, &sce->toolsettings->imapaint.paint);

    sce->toolsettings->particle.paintcursor = nullptr;
    sce->toolsettings->particle.scene = nullptr;
    sce->toolsettings->particle.object = nullptr;
    sce->toolsettings->gp_sculpt.paintcursor = nullptr;
    if (sce->toolsettings->uvsculpt.curve_distance_falloff) {
      BLO_read_struct(reader, CurveMapping, &sce->toolsettings->uvsculpt.curve_distance_falloff);
      BKE_curvemapping_blend_read(reader, sce->toolsettings->uvsculpt.curve_distance_falloff);
      BKE_curvemapping_init(sce->toolsettings->uvsculpt.curve_distance_falloff);
    }

    if (sce->toolsettings->sculpt) {
      BLO_read_struct(reader, CurveMapping, &sce->toolsettings->sculpt->automasking_cavity_curve);
      BLO_read_struct(
          reader, CurveMapping, &sce->toolsettings->sculpt->automasking_cavity_curve_op);

      if (sce->toolsettings->sculpt->automasking_cavity_curve) {
        BKE_curvemapping_blend_read(reader, sce->toolsettings->sculpt->automasking_cavity_curve);
        BKE_curvemapping_init(sce->toolsettings->sculpt->automasking_cavity_curve);
      }

      if (sce->toolsettings->sculpt->automasking_cavity_curve_op) {
        BKE_curvemapping_blend_read(reader,
                                    sce->toolsettings->sculpt->automasking_cavity_curve_op);
        BKE_curvemapping_init(sce->toolsettings->sculpt->automasking_cavity_curve_op);
      }

      BKE_sculpt_cavity_curves_ensure(sce->toolsettings->sculpt);
    }

    /* Relink grease pencil interpolation curves. */
    BLO_read_struct(reader, CurveMapping, &sce->toolsettings->gp_interpolate.custom_ipo);
    if (sce->toolsettings->gp_interpolate.custom_ipo) {
      BKE_curvemapping_blend_read(reader, sce->toolsettings->gp_interpolate.custom_ipo);
    }
    /* Relink grease pencil multi-frame falloff curve. */
    BLO_read_struct(reader, CurveMapping, &sce->toolsettings->gp_sculpt.cur_falloff);
    if (sce->toolsettings->gp_sculpt.cur_falloff) {
      BKE_curvemapping_blend_read(reader, sce->toolsettings->gp_sculpt.cur_falloff);
    }
    /* Relink grease pencil primitive curve. */
    BLO_read_struct(reader, CurveMapping, &sce->toolsettings->gp_sculpt.cur_primitive);
    if (sce->toolsettings->gp_sculpt.cur_primitive) {
      BKE_curvemapping_blend_read(reader, sce->toolsettings->gp_sculpt.cur_primitive);
    }

    /* Relink toolsettings curve profile. */
    BLO_read_struct(reader, CurveProfile, &sce->toolsettings->custom_bevel_profile_preset);
    if (sce->toolsettings->custom_bevel_profile_preset) {
      BKE_curveprofile_blend_read(reader, sce->toolsettings->custom_bevel_profile_preset);
    }

    BLO_read_data_address(reader, &sce->toolsettings->paint_mode.canvas_image);
    BLO_read_struct(reader, SequencerToolSettings, &sce->toolsettings->sequencer_tool_settings);
  }

  if (sce->ed) {
    BLO_read_struct(reader, Editing, &sce->ed);
    Editing *ed = sce->ed;

    ed->act_strip = static_cast<Strip *>(
        BLO_read_get_new_data_address_no_us(reader, ed->act_strip, sizeof(Strip)));
    ed->current_meta_strip = static_cast<Strip *>(
        BLO_read_get_new_data_address_no_us(reader, ed->current_meta_strip, sizeof(Strip)));
    ed->prefetch_job = nullptr;
    ed->runtime.strip_lookup = nullptr;
    ed->runtime.media_presence = nullptr;
    ed->runtime.thumbnail_cache = nullptr;
    ed->runtime.intra_frame_cache = nullptr;
    ed->runtime.source_image_cache = nullptr;
    ed->runtime.final_image_cache = nullptr;
    ed->runtime.preview_cache = nullptr;

    /* recursive link sequences, lb will be correctly initialized */
    link_recurs_seq(reader, &ed->seqbase);

    /* Read in sequence member data. */
    blender::seq::blend_read(reader, &ed->seqbase);
    BLO_read_struct_list(reader, SeqTimelineChannel, &ed->channels);

    /* stack */
    BLO_read_struct_list(reader, MetaStack, &(ed->metastack));

    LISTBASE_FOREACH (MetaStack *, ms, &ed->metastack) {
      BLO_read_struct(reader, Strip, &ms->parent_strip);

      ms->old_strip = static_cast<Strip *>(
          BLO_read_get_new_data_address_no_us(reader, ms->old_strip, sizeof(Strip)));
    }
  }

  /* Runtime */
  sce->r.mode &= ~R_NO_CAMERA_SWITCH;

  BKE_time_markers_blend_read(reader, sce->markers);

  BLO_read_struct_list(reader, TransformOrientation, &(sce->transform_spaces));
  BLO_read_struct_list(reader, SceneRenderLayer, &(sce->r.layers));
  BLO_read_struct_list(reader, SceneRenderView, &(sce->r.views));

  LISTBASE_FOREACH (SceneRenderLayer *, srl, &sce->r.layers) {
    BLO_read_struct(reader, IDProperty, &srl->prop);
    IDP_BlendDataRead(reader, &srl->prop);
    BLO_read_struct_list(reader, FreestyleModuleConfig, &(srl->freestyleConfig.modules));
    BLO_read_struct_list(reader, FreestyleLineSet, &(srl->freestyleConfig.linesets));
  }

  BKE_color_managed_view_settings_blend_read_data(reader, &sce->view_settings);
  BKE_image_format_blend_read_data(reader, &sce->r.im_format);
  BKE_image_format_blend_read_data(reader, &sce->r.bake.im_format);

  BLO_read_struct(reader, RigidBodyWorld, &sce->rigidbody_world);
  RigidBodyWorld *rbw = sce->rigidbody_world;
  if (rbw) {
    BLO_read_struct(reader, RigidBodyWorld_Shared, &rbw->shared);

    if (rbw->shared == nullptr) {
      /* Link deprecated caches if they exist, so we can use them for versioning.
       * We should only do this when rbw->shared == nullptr, because those pointers
       * are always set (for compatibility with older Blenders). We mustn't link
       * the same pointcache twice. */
      BKE_ptcache_blend_read_data(reader, &rbw->ptcaches, &rbw->pointcache, false);

      /* make sure simulation starts from the beginning after loading file */
      if (rbw->pointcache) {
        rbw->ltime = float(rbw->pointcache->startframe);
      }
    }
    else {
      /* link caches */
      BKE_ptcache_blend_read_data(reader, &rbw->shared->ptcaches, &rbw->shared->pointcache, false);

      /* make sure simulation starts from the beginning after loading file */
      if (rbw->shared->pointcache) {
        rbw->ltime = float(rbw->shared->pointcache->startframe);
      }
    }

    BKE_rigidbody_world_init_runtime(rbw);
    rbw->objects = nullptr;
    rbw->numbodies = 0;

    /* set effector weights */
    BLO_read_struct(reader, EffectorWeights, &rbw->effector_weights);
    if (!rbw->effector_weights) {
      rbw->effector_weights = BKE_effector_add_weights(nullptr);
    }
  }

  BLO_read_struct(reader, PreviewImage, &sce->preview);
  BKE_previewimg_blend_read(reader, sce->preview);

  BKE_curvemapping_blend_read(reader, &sce->r.mblur_shutter_curve);

  /* insert into global old-new map for reading without UI (link_global accesses it again) */
  BLO_read_glob_list(reader, &sce->view_layers);
  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    BKE_view_layer_blend_read_data(reader, view_layer);
  }

  BKE_screen_view3d_shading_blend_read_data(reader, &sce->display.shading);

  BLO_read_struct(reader, IDProperty, &sce->layer_properties);
  IDP_BlendDataRead(reader, &sce->layer_properties);
}

/* patch for missing scene IDs, can't be in do-versions */
static void scene_blend_read_after_liblink(BlendLibReader *reader, ID *id)
{
  Scene *sce = reinterpret_cast<Scene *>(id);

  LISTBASE_FOREACH_MUTABLE (Base *, base_legacy, &sce->base) {
    if (base_legacy->object == nullptr) {
      BLO_reportf_wrap(BLO_read_lib_reports(reader),
                       RPT_WARNING,
                       RPT_("LIB: object lost from scene: '%s'"),
                       sce->id.name + 2);
      BLI_remlink(&sce->base, base_legacy);
      if (base_legacy == sce->basact) {
        sce->basact = nullptr;
      }
      MEM_freeN(base_legacy);
    }
  }

  LISTBASE_FOREACH (ViewLayer *, view_layer, &sce->view_layers) {
    BKE_view_layer_blend_read_after_liblink(reader, id, view_layer);
  }

#ifdef USE_SETSCENE_CHECK
  if (sce->set != nullptr) {
    sce->flag |= SCE_READFILE_LIBLINK_NEED_SETSCENE_CHECK;
  }
#endif
  if (ID_IS_LINKED(sce)) {
    /* Linked scenes never have NLA tweak mode enabled. This works in concert with code in
     * BKE_animdata_blend_read_data, which also ensures that linked AnimData structs are never
     * linked in NLA tweak mode. */
    sce->flag &= ~SCE_NLA_EDIT_ON;
  }
}

static void scene_undo_preserve(BlendLibReader *reader, ID *id_new, ID *id_old)
{
  Scene *scene_new = (Scene *)id_new;
  Scene *scene_old = (Scene *)id_old;

  std::swap(scene_old->cursor, scene_new->cursor);
  if (scene_new->toolsettings != nullptr && scene_old->toolsettings != nullptr) {
    /* First try to restore ID pointers that can be and should be preserved (like brushes or
     * palettes), and counteract the swap of the whole ToolSettings structs below for the others
     * (like object ones). */
    scene_foreach_toolsettings(
        nullptr, scene_new->toolsettings, true, reader, scene_old->toolsettings);
    blender::dna::shallow_swap(*scene_old->toolsettings, *scene_new->toolsettings);
  }
}

static void scene_lib_override_apply_post(ID *id_dst, ID * /*id_src*/)
{
  Scene *scene = (Scene *)id_dst;

  if (scene->rigidbody_world != nullptr) {
    PTCacheID pid;
    BKE_ptcache_id_from_rigidbody(&pid, nullptr, scene->rigidbody_world);
    LISTBASE_FOREACH (PointCache *, point_cache, pid.ptcaches) {
      point_cache->flag |= PTCACHE_FLAG_INFO_DIRTY;
    }
  }
}

constexpr IDTypeInfo get_type_info()
{
  IDTypeInfo info{};
  info.id_code = ID_SCE;
  info.id_filter = FILTER_ID_SCE;
  info.dependencies_id_types = (FILTER_ID_OB | FILTER_ID_WO | FILTER_ID_SCE | FILTER_ID_MC |
                                FILTER_ID_MA | FILTER_ID_GR | FILTER_ID_TXT | FILTER_ID_LS |
                                FILTER_ID_MSK | FILTER_ID_SO | FILTER_ID_GD_LEGACY | FILTER_ID_BR |
                                FILTER_ID_PAL | FILTER_ID_IM | FILTER_ID_NT);
  info.main_listbase_index = INDEX_ID_SCE;
  info.struct_size = sizeof(Scene);
  info.name = "Scene";
  info.name_plural = "scenes";
  info.translation_context = BLT_I18NCONTEXT_ID_SCENE;
  info.flags = IDTYPE_FLAGS_NEVER_UNUSED;
  info.asset_type_info = nullptr;

  info.init_data = scene_init_data;
  info.copy_data = scene_copy_data;
  info.free_data = scene_free_data;
  /* For now default `BKE_lib_id_make_local_generic()` should work, may need more work though to
   * support all possible corner cases. */
  info.make_local = nullptr;
  info.foreach_id = scene_foreach_id;
  info.foreach_cache = scene_foreach_cache;
  info.foreach_path = scene_foreach_path;
  info.foreach_working_space_color = scene_foreach_working_space_color;
  info.owner_pointer_get = nullptr;

  info.blend_write = scene_blend_write;
  info.blend_read_data = scene_blend_read_data;
  info.blend_read_after_liblink = scene_blend_read_after_liblink;

  info.blend_read_undo_preserve = scene_undo_preserve;

  info.lib_override_apply_post = scene_lib_override_apply_post;
  return info;
}
IDTypeInfo IDType_ID_SCE = get_type_info();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene member functions
 */

double Scene::frames_per_second() const
{
  return double(this->r.frs_sec) / double(this->r.frs_sec_base);
}

/** \} */

const char *RE_engine_id_BLENDER_EEVEE = "BLENDER_EEVEE";
const char *RE_engine_id_BLENDER_EEVEE_NEXT = "BLENDER_EEVEE_NEXT";
const char *RE_engine_id_BLENDER_WORKBENCH = "BLENDER_WORKBENCH";
const char *RE_engine_id_CYCLES = "CYCLES";

static void remove_sequencer_fcurves(Scene *sce)
{
  using namespace blender;

  std::optional<std::pair<animrig::Action *, animrig::Slot *>> action_and_slot =
      animrig::get_action_slot_pair(sce->id);
  if (!action_and_slot) {
    return;
  }

  animrig::Channelbag *channelbag = channelbag_for_action_slot(*action_and_slot->first,
                                                               action_and_slot->second->handle);
  if (!channelbag) {
    return;
  }

  /* Create a copy of the F-Curve pointers, so iteration is safe while they are removed. */
  Vector<FCurve *> fcurves = channelbag->fcurves();

  for (FCurve *fcurve : fcurves) {
    if ((fcurve->rna_path) && strstr(fcurve->rna_path, "sequence_editor.strips_all")) {
      channelbag->fcurve_remove(*fcurve);
    }
  }
}

ToolSettings *BKE_toolsettings_copy(ToolSettings *toolsettings, const int flag)
{
  if (toolsettings == nullptr) {
    return nullptr;
  }
  ToolSettings *ts = static_cast<ToolSettings *>(MEM_dupallocN(toolsettings));
  if (toolsettings->vpaint) {
    ts->vpaint = static_cast<VPaint *>(MEM_dupallocN(toolsettings->vpaint));
    BKE_paint_copy(&toolsettings->vpaint->paint, &ts->vpaint->paint, flag);
  }
  if (toolsettings->wpaint) {
    ts->wpaint = static_cast<VPaint *>(MEM_dupallocN(toolsettings->wpaint));
    BKE_paint_copy(&toolsettings->wpaint->paint, &ts->wpaint->paint, flag);
  }
  if (toolsettings->sculpt) {
    ts->sculpt = static_cast<Sculpt *>(MEM_dupallocN(toolsettings->sculpt));
    BKE_paint_copy(&toolsettings->sculpt->paint, &ts->sculpt->paint, flag);

    if (toolsettings->sculpt->automasking_cavity_curve) {
      ts->sculpt->automasking_cavity_curve = BKE_curvemapping_copy(
          toolsettings->sculpt->automasking_cavity_curve);
      BKE_curvemapping_init(ts->sculpt->automasking_cavity_curve);
    }

    if (toolsettings->sculpt->automasking_cavity_curve_op) {
      ts->sculpt->automasking_cavity_curve_op = BKE_curvemapping_copy(
          toolsettings->sculpt->automasking_cavity_curve_op);
      BKE_curvemapping_init(ts->sculpt->automasking_cavity_curve_op);
    }
  }
  if (toolsettings->uvsculpt.curve_distance_falloff) {
    ts->uvsculpt.curve_distance_falloff = BKE_curvemapping_copy(
        toolsettings->uvsculpt.curve_distance_falloff);
    BKE_curvemapping_init(ts->uvsculpt.curve_distance_falloff);
  }
  if (toolsettings->gp_paint) {
    ts->gp_paint = static_cast<GpPaint *>(MEM_dupallocN(toolsettings->gp_paint));
    BKE_paint_copy(&toolsettings->gp_paint->paint, &ts->gp_paint->paint, flag);
  }
  if (toolsettings->gp_vertexpaint) {
    ts->gp_vertexpaint = static_cast<GpVertexPaint *>(MEM_dupallocN(toolsettings->gp_vertexpaint));
    BKE_paint_copy(&toolsettings->gp_vertexpaint->paint, &ts->gp_vertexpaint->paint, flag);
  }
  if (toolsettings->gp_sculptpaint) {
    ts->gp_sculptpaint = static_cast<GpSculptPaint *>(MEM_dupallocN(toolsettings->gp_sculptpaint));
    BKE_paint_copy(&toolsettings->gp_sculptpaint->paint, &ts->gp_sculptpaint->paint, flag);
  }
  if (toolsettings->gp_weightpaint) {
    ts->gp_weightpaint = static_cast<GpWeightPaint *>(MEM_dupallocN(toolsettings->gp_weightpaint));
    BKE_paint_copy(&toolsettings->gp_weightpaint->paint, &ts->gp_weightpaint->paint, flag);
  }
  if (toolsettings->curves_sculpt) {
    ts->curves_sculpt = static_cast<CurvesSculpt *>(MEM_dupallocN(toolsettings->curves_sculpt));
    BKE_paint_copy(&toolsettings->curves_sculpt->paint, &ts->curves_sculpt->paint, flag);
  }

  /* Color jitter curves in unified paint settings. */
  ts->unified_paint_settings.curve_rand_hue = BKE_curvemapping_copy(
      toolsettings->unified_paint_settings.curve_rand_hue);
  ts->unified_paint_settings.curve_rand_saturation = BKE_curvemapping_copy(
      toolsettings->unified_paint_settings.curve_rand_saturation);
  ts->unified_paint_settings.curve_rand_value = BKE_curvemapping_copy(
      toolsettings->unified_paint_settings.curve_rand_value);

  BKE_paint_copy(&toolsettings->imapaint.paint, &ts->imapaint.paint, flag);
  ts->particle.paintcursor = nullptr;
  ts->particle.scene = nullptr;
  ts->particle.object = nullptr;

  /* duplicate Grease Pencil interpolation curve */
  ts->gp_interpolate.custom_ipo = BKE_curvemapping_copy(toolsettings->gp_interpolate.custom_ipo);
  /* Duplicate Grease Pencil multi-frame falloff. */
  ts->gp_sculpt.cur_falloff = BKE_curvemapping_copy(toolsettings->gp_sculpt.cur_falloff);
  ts->gp_sculpt.cur_primitive = BKE_curvemapping_copy(toolsettings->gp_sculpt.cur_primitive);

  ts->custom_bevel_profile_preset = BKE_curveprofile_copy(
      toolsettings->custom_bevel_profile_preset);

  ts->sequencer_tool_settings = blender::seq::tool_settings_copy(
      toolsettings->sequencer_tool_settings);
  return ts;
}

void BKE_toolsettings_free(ToolSettings *toolsettings)
{
  if (toolsettings == nullptr) {
    return;
  }
  if (toolsettings->vpaint) {
    BKE_paint_free(&toolsettings->vpaint->paint);
    MEM_freeN(toolsettings->vpaint);
  }
  if (toolsettings->wpaint) {
    BKE_paint_free(&toolsettings->wpaint->paint);
    MEM_freeN(toolsettings->wpaint);
  }
  if (toolsettings->sculpt) {
    if (toolsettings->sculpt->automasking_cavity_curve) {
      BKE_curvemapping_free(toolsettings->sculpt->automasking_cavity_curve);
    }
    if (toolsettings->sculpt->automasking_cavity_curve_op) {
      BKE_curvemapping_free(toolsettings->sculpt->automasking_cavity_curve_op);
    }

    BKE_paint_free(&toolsettings->sculpt->paint);
    MEM_freeN(toolsettings->sculpt);
  }
  if (toolsettings->uvsculpt.curve_distance_falloff) {
    BKE_curvemapping_free(toolsettings->uvsculpt.curve_distance_falloff);
  }
  if (toolsettings->gp_paint) {
    BKE_paint_free(&toolsettings->gp_paint->paint);
    MEM_freeN(toolsettings->gp_paint);
  }
  if (toolsettings->gp_vertexpaint) {
    BKE_paint_free(&toolsettings->gp_vertexpaint->paint);
    MEM_freeN(toolsettings->gp_vertexpaint);
  }
  if (toolsettings->gp_sculptpaint) {
    BKE_paint_free(&toolsettings->gp_sculptpaint->paint);
    MEM_freeN(toolsettings->gp_sculptpaint);
  }
  if (toolsettings->gp_weightpaint) {
    BKE_paint_free(&toolsettings->gp_weightpaint->paint);
    MEM_freeN(toolsettings->gp_weightpaint);
  }
  if (toolsettings->curves_sculpt) {
    BKE_paint_free(&toolsettings->curves_sculpt->paint);
    MEM_freeN(toolsettings->curves_sculpt);
  }
  BKE_paint_free(&toolsettings->imapaint.paint);

  /* Color jitter curves in unified paint settings. */
  if (toolsettings->unified_paint_settings.curve_rand_hue) {
    BKE_curvemapping_free(toolsettings->unified_paint_settings.curve_rand_hue);
  }
  if (toolsettings->unified_paint_settings.curve_rand_saturation) {
    BKE_curvemapping_free(toolsettings->unified_paint_settings.curve_rand_saturation);
  }
  if (toolsettings->unified_paint_settings.curve_rand_value) {
    BKE_curvemapping_free(toolsettings->unified_paint_settings.curve_rand_value);
  }

  /* free Grease Pencil interpolation curve */
  if (toolsettings->gp_interpolate.custom_ipo) {
    BKE_curvemapping_free(toolsettings->gp_interpolate.custom_ipo);
  }
  /* free Grease Pencil multi-frame falloff curve */
  if (toolsettings->gp_sculpt.cur_falloff) {
    BKE_curvemapping_free(toolsettings->gp_sculpt.cur_falloff);
  }
  if (toolsettings->gp_sculpt.cur_primitive) {
    BKE_curvemapping_free(toolsettings->gp_sculpt.cur_primitive);
  }

  if (toolsettings->custom_bevel_profile_preset) {
    BKE_curveprofile_free(toolsettings->custom_bevel_profile_preset);
  }

  if (toolsettings->sequencer_tool_settings) {
    blender::seq::tool_settings_free(toolsettings->sequencer_tool_settings);
  }

  MEM_freeN(toolsettings);
}

void BKE_scene_copy_data_eevee(Scene *sce_dst, const Scene *sce_src)
{
  /* Copy eevee data between scenes. */
  sce_dst->eevee = sce_src->eevee;
}

Scene *BKE_scene_duplicate(Main *bmain, Scene *sce, eSceneCopyMethod type)
{
  Scene *sce_copy;

  /* TODO: this should/could most likely be replaced by call to more generic code at some point...
   * But for now, let's keep it well isolated here. */
  if (type == SCE_COPY_EMPTY) {
    ListBase rv;

    sce_copy = BKE_scene_add(bmain, sce->id.name + 2);

    rv = sce_copy->r.views;
    BKE_curvemapping_free_data(&sce_copy->r.mblur_shutter_curve);
    sce_copy->r = sce->r;
    sce_copy->r.views = rv;
    sce_copy->unit = sce->unit;
    sce_copy->physics_settings = sce->physics_settings;
    sce_copy->audio = sce->audio;
    BKE_scene_copy_data_eevee(sce_copy, sce);

    if (sce->id.properties) {
      sce_copy->id.properties = IDP_CopyProperty(sce->id.properties);
    }
    if (sce->id.system_properties) {
      sce_copy->id.system_properties = IDP_CopyProperty(sce->id.system_properties);
    }

    BKE_sound_destroy_scene(sce_copy);

    /* copy color management settings */
    BKE_color_managed_display_settings_copy(&sce_copy->display_settings, &sce->display_settings);
    BKE_color_managed_view_settings_copy(&sce_copy->view_settings, &sce->view_settings);
    BKE_color_managed_colorspace_settings_copy(&sce_copy->sequencer_colorspace_settings,
                                               &sce->sequencer_colorspace_settings);

    BKE_image_format_copy(&sce_copy->r.im_format, &sce->r.im_format);
    BKE_image_format_copy(&sce_copy->r.bake.im_format, &sce->r.bake.im_format);

    BKE_curvemapping_copy_data(&sce_copy->r.mblur_shutter_curve, &sce->r.mblur_shutter_curve);

    /* viewport display settings */
    sce_copy->display = sce->display;

    /* tool settings */
    BKE_toolsettings_free(sce_copy->toolsettings);
    sce_copy->toolsettings = BKE_toolsettings_copy(sce->toolsettings, 0);

    BKE_sound_reset_scene_runtime(sce_copy);

    /* grease pencil */
    sce_copy->gpd = nullptr;

    sce_copy->preview = nullptr;

    return sce_copy;
  }

  eDupli_ID_Flags duplicate_flags = (eDupli_ID_Flags)(U.dupflag | USER_DUP_OBJECT);

  sce_copy = (Scene *)BKE_id_copy(bmain, (ID *)sce);
  id_us_min(&sce_copy->id);
  id_us_ensure_real(&sce_copy->id);

  /* Scene duplication is always root of duplication currently, and never a subprocess.
   *
   * Keep these around though, as this allow the rest of the duplication code to stay in sync with
   * the layout and behavior as the other duplicate functions (see e.g. #BKE_collection_duplicate
   * or #BKE_object_duplicate).
   *
   * TOOD: At some point it would be nice to deduplicate this logic and move common behavior into
   * generic ID management code, with IDType callbacks for specific duplication behavior only. */
  const bool is_subprocess = false;
  const bool is_root_id = true;
  const int copy_flags = LIB_ID_COPY_DEFAULT;

  if (!is_subprocess) {
    BKE_main_id_newptr_and_tag_clear(bmain);
  }

  if (is_root_id) {
    /* In case root duplicated ID is linked, assume we want to get a local copy of it and
     * duplicate all expected linked data. */
    if (ID_IS_LINKED(sce)) {
      duplicate_flags = (duplicate_flags | USER_DUP_LINKED_ID);
    }
  }

  /* Usages of the duplicated scene also need to be remapped in new duplicated IDs. */
  ID_NEW_SET(sce, sce_copy);

  /* Extra actions, most notably SCE_FULL_COPY also duplicates several 'children' datablocks. */

  BKE_animdata_duplicate_id_action(bmain, &sce_copy->id, duplicate_flags);

  /* Exception for the compositor; Before 5.0, creating a linked copy of the scene created a new
   * compositing node tree with a Render Layers node that referred to the new scene.
   * To preserve this behavior, we make a full copy when creating a linked copy as well as a full
   * copy of the scene.*/
  BKE_id_copy_for_duplicate(
      bmain, reinterpret_cast<ID *>(sce->compositing_node_group), duplicate_flags, copy_flags);

  if (type == SCE_COPY_FULL) {
    /* Copy Freestyle LineStyle datablocks. */
    LISTBASE_FOREACH (ViewLayer *, view_layer_dst, &sce_copy->view_layers) {
      LISTBASE_FOREACH (FreestyleLineSet *, lineset, &view_layer_dst->freestyle_config.linesets) {
        BKE_id_copy_for_duplicate(bmain, (ID *)lineset->linestyle, duplicate_flags, copy_flags);
      }
    }

    /* Full copy of world (included animations) */
    BKE_id_copy_for_duplicate(bmain, (ID *)sce->world, duplicate_flags, copy_flags);

    /* Full copy of GreasePencil. */
    BKE_id_copy_for_duplicate(bmain, (ID *)sce->gpd, duplicate_flags, copy_flags);

    /* Deep-duplicate collections and objects (using preferences' settings for which sub-data to
     * duplicate along the object itself). */
    BKE_collection_duplicate(bmain,
                             nullptr,
                             nullptr,
                             sce_copy->master_collection,
                             duplicate_flags,
                             LIB_ID_DUPLICATE_IS_SUBPROCESS);

    /* Rigid body world collections may not be instantiated as scene's collections, ensure they
     * also get properly duplicated. */
    if (sce_copy->rigidbody_world != nullptr) {
      if (sce_copy->rigidbody_world->group != nullptr) {
        BKE_collection_duplicate(bmain,
                                 nullptr,
                                 nullptr,
                                 sce_copy->rigidbody_world->group,
                                 duplicate_flags,
                                 LIB_ID_DUPLICATE_IS_SUBPROCESS);
      }
      if (sce_copy->rigidbody_world->constraints != nullptr) {
        BKE_collection_duplicate(bmain,
                                 nullptr,
                                 nullptr,
                                 sce_copy->rigidbody_world->constraints,
                                 duplicate_flags,
                                 LIB_ID_DUPLICATE_IS_SUBPROCESS);
      }
    }
  }
  else {
    /* Remove sequencer if not full copy */
    /* XXX Why in Hell? :/ */
    remove_sequencer_fcurves(sce_copy);
    blender::seq::editing_free(sce_copy, true);
  }

  /* The final step is to ensure that all of the newly duplicated IDs are used by other newly
   * duplicated IDs, and some standard cleanup & updates. */
  if (!is_subprocess) {
    /* This code will follow into all ID links using an ID tagged with ID_TAG_NEW. */
    /* Unfortunate, but with some types (e.g. meshes), an object is considered in Edit mode if
     * its obdata contains edit mode runtime data. This can be the case of all newly duplicated
     * objects, as even though duplicate code move the object back in Object mode, they are still
     * using the original obdata ID, leading to them being falsely detected as being in Edit
     * mode, and therefore not remapping their obdata to the newly duplicated one. See #139715.
     */
    BKE_libblock_relink_to_newid(
        bmain, &sce_copy->id, ID_REMAP_FORCE_OBDATA_IN_EDITMODE | ID_REMAP_SKIP_USER_CLEAR);

#ifndef NDEBUG
    /* Call to `BKE_libblock_relink_to_newid` above is supposed to have cleared all those
     * flags. */
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      BLI_assert((id_iter->tag & ID_TAG_NEW) == 0);
    }
    FOREACH_MAIN_ID_END;
#endif

    /* Cleanup. */
    BKE_main_id_newptr_and_tag_clear(bmain);

    BKE_main_collection_sync(bmain);
  }

  return sce_copy;
}

void BKE_scene_groups_relink(Scene *sce)
{
  if (sce->rigidbody_world) {
    BKE_rigidbody_world_groups_relink(sce->rigidbody_world);
  }
}

bool BKE_scene_can_be_removed(const Main *bmain, const Scene *scene)
{
  /* Linked scenes can always be removed. */
  if (ID_IS_LINKED(scene)) {
    return true;
  }
  /* Local scenes can only be removed, when there is at least one local scene left. */
  LISTBASE_FOREACH (Scene *, other_scene, &bmain->scenes) {
    if (ID_IS_LINKED(other_scene)) {
      /* Once the first linked scene is reached, there is no more local ones to check, so at this
       * point there is no other local scene and the given one cannot be deleted. */
      break;
    }
    if (other_scene != scene) {
      return true;
    }
  }
  return false;
}

Scene *BKE_scene_find_replacement(const Main &bmain,
                                  const Scene &scene,
                                  blender::FunctionRef<bool(const Scene &scene)> scene_validate_cb)
{
  UNUSED_VARS_NDEBUG(bmain);
  BLI_assert(BLI_findindex(&bmain.scenes, &scene) >= 0);

  /* Simply return a closest neighbor scene, unless a validate callback is provided and it rejects
   * the iterated scene. */
  for (Scene *scene_iter = static_cast<Scene *>(scene.id.prev); scene_iter != nullptr;
       scene_iter = static_cast<Scene *>(scene_iter->id.prev))
  {
    if (scene_validate_cb && !scene_validate_cb(*scene_iter)) {
      continue;
    }
    return scene_iter;
  }

  for (Scene *scene_iter = static_cast<Scene *>(scene.id.next); scene_iter != nullptr;
       scene_iter = static_cast<Scene *>(scene_iter->id.next))
  {
    if (scene_validate_cb && !scene_validate_cb(*scene_iter)) {
      continue;
    }
    return scene_iter;
  }

  return nullptr;
}

Scene *BKE_scene_add(Main *bmain, const char *name)
{
  Scene *sce = BKE_id_new<Scene>(bmain, name);
  id_us_min(&sce->id);
  id_us_ensure_real(&sce->id);

  return sce;
}

bool BKE_scene_object_find(Scene *scene, Object *ob)
{
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    if (BLI_findptr(BKE_view_layer_object_bases_get(view_layer), ob, offsetof(Base, object))) {
      return true;
    }
  }
  return false;
}

Object *BKE_scene_object_find_by_name(const Scene *scene, const char *name)
{
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      if (STREQ(base->object->id.name + 2, name)) {
        return base->object;
      }
    }
  }
  return nullptr;
}

void BKE_scene_set_background(Main *bmain, Scene *scene)
{
  /* check for cyclic sets, for reading old files but also for definite security (py?) */
  BKE_scene_validate_setscene(bmain, scene);

  /* Deselect objects (for data select). */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    ob->flag &= ~SELECT;
  }

  /* copy layers and flags from bases to objects */
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      /* collection patch... */
      BKE_scene_object_base_flag_sync_from_base(base);
    }
  }
  /* No full animation update, this to enable render code to work
   * (render code calls own animation updates). */
}

Scene *BKE_scene_set_name(Main *bmain, const char *name)
{
  Scene *sce = (Scene *)BKE_libblock_find_name(bmain, ID_SCE, name);
  if (sce) {
    BKE_scene_set_background(bmain, sce);
    printf("Scene switch for render: '%s' in file: '%s'\n", name, BKE_main_blendfile_path(bmain));
    return sce;
  }

  printf("Can't find scene: '%s' in file: '%s'\n", name, BKE_main_blendfile_path(bmain));
  return nullptr;
}

int BKE_scene_base_iter_next(
    Depsgraph *depsgraph, SceneBaseIter *iter, Scene **scene, int val, Base **base, Object **ob)
{
  bool run_again = true;

  /* init */
  if (val == 0) {
    iter->phase = F_START;
    iter->dupob = nullptr;
    iter->dupob_index = -1;
    iter->duplilist.clear();
    iter->dupli_refob = nullptr;
  }
  else {
    /* run_again is set when a duplilist has been ended */
    while (run_again) {
      run_again = false;

      /* the first base */
      if (iter->phase == F_START) {
        ViewLayer *view_layer = (depsgraph) ? DEG_get_evaluated_view_layer(depsgraph) :
                                              BKE_view_layer_context_active_PLACEHOLDER(*scene);
        BKE_view_layer_synced_ensure(*scene, view_layer);
        *base = static_cast<Base *>(BKE_view_layer_object_bases_get(view_layer)->first);
        if (*base) {
          *ob = (*base)->object;
          iter->phase = F_SCENE;
        }
        else {
          /* exception: empty scene layer */
          while ((*scene)->set) {
            (*scene) = (*scene)->set;
            ViewLayer *view_layer_set = BKE_view_layer_default_render(*scene);
            BKE_view_layer_synced_ensure(*scene, view_layer_set);
            ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer_set);
            if (object_bases->first) {
              *base = static_cast<Base *>(object_bases->first);
              *ob = (*base)->object;
              iter->phase = F_SCENE;
              break;
            }
          }
        }
      }
      else {
        if (*base && iter->phase != F_DUPLI) {
          *base = (*base)->next;
          if (*base) {
            *ob = (*base)->object;
          }
          else {
            if (iter->phase == F_SCENE) {
              /* (*scene) is finished, now do the set */
              while ((*scene)->set) {
                (*scene) = (*scene)->set;
                ViewLayer *view_layer_set = BKE_view_layer_default_render(*scene);
                BKE_view_layer_synced_ensure(*scene, view_layer_set);
                ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer_set);
                if (object_bases->first) {
                  *base = static_cast<Base *>(object_bases->first);
                  *ob = (*base)->object;
                  break;
                }
              }
            }
          }
        }
      }

      if (*base == nullptr) {
        iter->phase = F_START;
      }
      else {
        if (iter->phase != F_DUPLI) {
          if (depsgraph && (*base)->object->transflag & OB_DUPLI) {
            /* Collections cannot be duplicated for meta-balls yet,
             * this enters eternal loop because of
             * makeDispListMBall getting called inside of collection_duplilist */
            if ((*base)->object->instance_collection == nullptr) {
              object_duplilist(depsgraph, (*scene), (*base)->object, nullptr, iter->duplilist);

              iter->dupob = iter->duplilist.is_empty() ? nullptr : &iter->duplilist.first();
              iter->dupob_index = 0;

              if (!iter->dupob) {
                iter->duplilist.clear();
                iter->dupob_index = -1;
              }
              iter->dupli_refob = nullptr;
            }
          }
        }
        /* handle dupli's */
        if (iter->dupob) {
          (*base)->flag_legacy |= OB_FROMDUPLI;
          *ob = iter->dupob->ob;
          iter->phase = F_DUPLI;

          if (iter->dupli_refob != *ob) {
            if (iter->dupli_refob) {
              /* Restore previous object's real matrix. */
              copy_m4_m4(iter->dupli_refob->runtime->object_to_world.ptr(), iter->omat);
            }
            /* Backup new object's real matrix. */
            iter->dupli_refob = *ob;
            copy_m4_m4(iter->omat, iter->dupli_refob->object_to_world().ptr());
          }
          copy_m4_m4((*ob)->runtime->object_to_world.ptr(), iter->dupob->mat);

          if (++iter->dupob_index < iter->duplilist.size()) {
            iter->dupob = &iter->duplilist[iter->dupob_index];
          }
          else {
            iter->dupob = nullptr;
            iter->dupob_index = -1;
          }
        }
        else if (iter->phase == F_DUPLI) {
          iter->phase = F_SCENE;
          (*base)->flag_legacy &= ~OB_FROMDUPLI;

          if (iter->dupli_refob) {
            /* Restore last object's real matrix. */
            copy_m4_m4(iter->dupli_refob->runtime->object_to_world.ptr(), iter->omat);
            iter->dupli_refob = nullptr;
          }

          iter->duplilist.clear();
          run_again = true;
        }
      }
    }
  }

  return iter->phase;
}

bool BKE_scene_has_view_layer(const Scene *scene, const ViewLayer *layer)
{
  return BLI_findindex(&scene->view_layers, layer) != -1;
}

Scene *BKE_scene_find_from_collection(const Main *bmain, const Collection *collection)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, layer, &scene->view_layers) {
      if (BKE_view_layer_has_collection(layer, collection)) {
        return scene;
      }
    }
  }

  return nullptr;
}

Object *BKE_scene_camera_switch_find(Scene *scene)
{
  if (scene->r.mode & R_NO_CAMERA_SWITCH) {
    return nullptr;
  }

  const int ctime = int(BKE_scene_ctime_get(scene));
  int frame = -(MAXFRAME + 1);
  int min_frame = MAXFRAME + 1;
  Object *camera = nullptr;
  Object *first_camera = nullptr;

  LISTBASE_FOREACH (TimeMarker *, m, &scene->markers) {
    if (m->camera && (m->camera->visibility_flag & OB_HIDE_RENDER) == 0) {
      if ((m->frame <= ctime) && (m->frame > frame)) {
        camera = m->camera;
        frame = m->frame;

        if (frame == ctime) {
          break;
        }
      }

      if (m->frame < min_frame) {
        first_camera = m->camera;
        min_frame = m->frame;
      }
    }
  }

  if (camera == nullptr) {
    /* If there's no marker to the left of current frame,
     * use camera from left-most marker to solve all sort
     * of Schrodinger uncertainties.
     */
    return first_camera;
  }

  return camera;
}

bool BKE_scene_camera_switch_update(Scene *scene)
{
  Object *camera = BKE_scene_camera_switch_find(scene);
  if (camera && (camera != scene->camera)) {
    scene->camera = camera;
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_PARAMETERS);
    return true;
  }
  return false;
}

const char *BKE_scene_find_marker_name(const Scene *scene, int frame)
{
  const ListBase *markers = &scene->markers;
  const TimeMarker *m1, *m2;

  /* search through markers for match */
  for (m1 = static_cast<const TimeMarker *>(markers->first),
      m2 = static_cast<const TimeMarker *>(markers->last);
       m1 && m2;
       m1 = m1->next, m2 = m2->prev)
  {
    if (m1->frame == frame) {
      return m1->name;
    }

    if (m1 == m2) {
      break;
    }

    if (m2->frame == frame) {
      return m2->name;
    }
  }

  return nullptr;
}

const char *BKE_scene_find_last_marker_name(const Scene *scene, int frame)
{
  const TimeMarker *best_marker = nullptr;
  int best_frame = -MAXFRAME * 2;
  LISTBASE_FOREACH (const TimeMarker *, marker, &scene->markers) {
    if (marker->frame == frame) {
      return marker->name;
    }

    if (marker->frame > best_frame && marker->frame < frame) {
      best_marker = marker;
      best_frame = marker->frame;
    }
  }

  return best_marker ? best_marker->name : nullptr;
}

float BKE_scene_frame_snap_by_seconds(const Scene *scene,
                                      const double interval_in_seconds,
                                      const float frame)
{
  BLI_assert(interval_in_seconds > 0);
  BLI_assert(scene->frames_per_second() > 0);

  const double interval_in_frames = scene->frames_per_second() * interval_in_seconds;
  const double second_prev = interval_in_frames * floor(frame / interval_in_frames);
  const double second_next = second_prev + ceil(interval_in_frames);
  const double delta_prev = frame - second_prev;
  const double delta_next = second_next - frame;
  return float((delta_prev < delta_next) ? second_prev : second_next);
}

void BKE_scene_remove_rigidbody_object(Main *bmain, Scene *scene, Object *ob, const bool free_us)
{
  /* remove rigid body constraint from world before removing object */
  if (ob->rigidbody_constraint) {
    BKE_rigidbody_remove_constraint(bmain, scene, ob, free_us);
  }
  /* remove rigid body object from world before removing object */
  if (ob->rigidbody_object) {
    BKE_rigidbody_remove_object(bmain, scene, ob, free_us);
  }
}

bool BKE_scene_validate_setscene(Main *bmain, Scene *sce)
{
  Scene *sce_iter;
  int a, totscene;

  if (sce->set == nullptr) {
    return true;
  }
  totscene = BLI_listbase_count(&bmain->scenes);

  for (a = 0, sce_iter = sce; sce_iter->set; sce_iter = sce_iter->set, a++) {
    /* more iterations than scenes means we have a cycle */
    if (a > totscene) {
      /* The tested scene gets zeroed, that's typically current scene. */
      sce->set = nullptr;
      return false;
    }
  }

  return true;
}

float BKE_scene_ctime_get(const Scene *scene)
{
  return BKE_scene_frame_to_ctime(scene, scene->r.cfra);
}

float BKE_scene_frame_to_ctime(const Scene *scene, const int frame)
{
  float ctime = frame;
  ctime += scene->r.subframe;
  ctime *= scene->r.framelen;

  return ctime;
}

float BKE_scene_frame_get(const Scene *scene)
{
  return scene->r.cfra + scene->r.subframe;
}

void BKE_scene_frame_set(Scene *scene, float frame)
{
  double intpart;
  scene->r.subframe = modf(double(frame), &intpart);
  scene->r.cfra = int(intpart);
}

/* -------------------------------------------------------------------- */
/** \name Scene Orientation Slots
 * \{ */

TransformOrientationSlot *BKE_scene_orientation_slot_get(Scene *scene, int slot_index)
{
  if ((scene->orientation_slots[slot_index].flag & SELECT) == 0) {
    slot_index = SCE_ORIENT_DEFAULT;
  }
  return &scene->orientation_slots[slot_index];
}

TransformOrientationSlot *BKE_scene_orientation_slot_get_from_flag(Scene *scene, int flag)
{
  BLI_assert(flag && !(flag & ~(V3D_GIZMO_SHOW_OBJECT_TRANSLATE | V3D_GIZMO_SHOW_OBJECT_ROTATE |
                                V3D_GIZMO_SHOW_OBJECT_SCALE)));
  int slot_index = SCE_ORIENT_DEFAULT;
  if (flag & V3D_GIZMO_SHOW_OBJECT_TRANSLATE) {
    slot_index = SCE_ORIENT_TRANSLATE;
  }
  else if (flag & V3D_GIZMO_SHOW_OBJECT_ROTATE) {
    slot_index = SCE_ORIENT_ROTATE;
  }
  else if (flag & V3D_GIZMO_SHOW_OBJECT_SCALE) {
    slot_index = SCE_ORIENT_SCALE;
  }
  return BKE_scene_orientation_slot_get(scene, slot_index);
}

void BKE_scene_orientation_slot_set_index(TransformOrientationSlot *orient_slot, int orientation)
{
  const bool is_custom = orientation >= V3D_ORIENT_CUSTOM;
  orient_slot->type = is_custom ? V3D_ORIENT_CUSTOM : orientation;
  orient_slot->index_custom = is_custom ? (orientation - V3D_ORIENT_CUSTOM) : -1;
}

int BKE_scene_orientation_slot_get_index(const TransformOrientationSlot *orient_slot)
{
  return (orient_slot->type == V3D_ORIENT_CUSTOM) ?
             (orient_slot->type + orient_slot->index_custom) :
             orient_slot->type;
}

int BKE_scene_orientation_get_index(Scene *scene, int slot_index)
{
  TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get(scene, slot_index);
  return BKE_scene_orientation_slot_get_index(orient_slot);
}

int BKE_scene_orientation_get_index_from_flag(Scene *scene, int flag)
{
  TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get_from_flag(scene, flag);
  return BKE_scene_orientation_slot_get_index(orient_slot);
}

/** \} */

static bool check_rendered_viewport_visible(Main *bmain)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    Scene *scene = window->scene;
    RenderEngineType *type = RE_engines_find(scene->r.engine);

    if (type->draw_engine || !type->render) {
      continue;
    }

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      View3D *v3d = static_cast<View3D *>(area->spacedata.first);
      if (area->spacetype != SPACE_VIEW3D) {
        continue;
      }
      if (v3d->shading.type == OB_RENDER) {
        return true;
      }
    }
  }
  return false;
}

/* TODO(@ideasman42): shouldn't we be able to use 'DEG_get_view_layer' here?
 * Currently this is nullptr on load, so don't. */
static void prepare_mesh_for_viewport_render(Main *bmain,
                                             const Scene *scene,
                                             ViewLayer *view_layer)
{
  /* This is needed to prepare mesh to be used by the render
   * engine from the viewport rendering. We do loading here
   * so all the objects which shares the same mesh datablock
   * are nicely tagged for update and updated.
   *
   * This makes it so viewport render engine doesn't need to
   * call loading of the edit data for the mesh objects.
   */
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit) {
    Mesh *mesh = static_cast<Mesh *>(obedit->data);
    if ((obedit->type == OB_MESH) &&
        ((obedit->id.recalc & ID_RECALC_ALL) || (mesh->id.recalc & ID_RECALC_ALL)))
    {
      if (check_rendered_viewport_visible(bmain)) {
        BMesh *bm = mesh->runtime->edit_mesh->bm;
        BMeshToMeshParams params{};
        params.calc_object_remap = true;
        params.update_shapekey_indices = true;
        BM_mesh_bm_to_me(bmain, bm, mesh, &params);
        DEG_id_tag_update(&mesh->id, 0);
      }
    }
  }
}

void BKE_scene_update_sound(Depsgraph *depsgraph, Main *bmain)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  const int recalc = scene->id.recalc;
  BKE_sound_ensure_scene(scene);
  if (recalc & ID_RECALC_FRAME_CHANGE) {
    BKE_sound_seek_scene(bmain, scene);
  }
  if (recalc & ID_RECALC_AUDIO_FPS) {
    BKE_sound_update_fps(bmain, scene);
  }
  if (recalc & ID_RECALC_AUDIO_VOLUME) {
    BKE_sound_set_scene_volume(scene, scene->audio.volume);
  }
  if (recalc & ID_RECALC_AUDIO_MUTE) {
    const bool is_mute = (DEG_get_mode(depsgraph) == DAG_EVAL_VIEWPORT) &&
                         (scene->audio.flag & AUDIO_MUTE);
    BKE_sound_mute_scene(scene, is_mute);
  }
  if (recalc & ID_RECALC_AUDIO_LISTENER) {
    BKE_sound_update_scene_listener(scene);
  }
  BKE_sound_update_scene(depsgraph, scene);
}

void BKE_scene_update_tag_audio_volume(Depsgraph * /*depsgraph*/, Scene *scene)
{
  BLI_assert(DEG_is_evaluated(scene));
  /* The volume is actually updated in BKE_scene_update_sound(), from either
   * scene_graph_update_tagged() or from BKE_scene_graph_update_for_newframe(). */
  scene->id.recalc |= ID_RECALC_AUDIO_VOLUME;
}

/* TODO(sergey): This actually should become view_layer_graph or so.
 * Same applies to update_for_newframe.
 *
 * If only_if_tagged is truth then the function will do nothing if the dependency graph is up
 * to date already.
 */
static void scene_graph_update_tagged(Depsgraph *depsgraph, Main *bmain, bool only_if_tagged)
{
  if (only_if_tagged && DEG_is_fully_evaluated(depsgraph)) {
    return;
  }

  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  bool used_multiple_passes = false;

  bool run_callbacks = DEG_id_type_any_updated(depsgraph);
  if (run_callbacks) {
    BKE_callback_exec_id(bmain, &scene->id, BKE_CB_EVT_DEPSGRAPH_UPDATE_PRE);
  }

  for (int pass = 0; pass < 2; pass++) {
    /* (Re-)build dependency graph if needed. */
    DEG_graph_relations_update(depsgraph);
    /* Uncomment this to check if graph was properly tagged for update. */
    // DEG_debug_graph_relations_validate(depsgraph, bmain, scene, view_layer);
    /* Flush editing data if needed. */
    prepare_mesh_for_viewport_render(bmain, scene, view_layer);
    /* Update all objects: drivers, matrices, etc. flags set
     * by depsgraph or manual, no layer check here, gets correct flushed. */
    DEG_evaluate_on_refresh(depsgraph, DEG_EVALUATE_SYNC_WRITEBACK_YES);
    /* Update sound system. */
    BKE_scene_update_sound(depsgraph, bmain);
    /* Notify python about depsgraph update. */
    if (run_callbacks) {
      BKE_callback_exec_id_depsgraph(
          bmain, &scene->id, depsgraph, BKE_CB_EVT_DEPSGRAPH_UPDATE_POST);

      /* It is possible that the custom callback modified scene and removed some IDs from the main
       * database. In this case DEG_editors_update() will crash because it iterates over all IDs
       * which depsgraph was built for.
       *
       * The solution is to update relations prior to this call, avoiding access to freed IDs.
       * Should be safe because relations update is supposed to preserve flags of all IDs which are
       * still a part of the dependency graph. If an ID is kicked out of the dependency graph it
       * should also be fine because when/if it's added to another dependency graph it will need to
       * be tagged for an update anyway.
       *
       * If there are no relations changed by the callback this call will do nothing. */
      DEG_graph_relations_update(depsgraph);
    }

    /* If user callback did not tag anything for update we can skip second iteration.
     * Otherwise we update scene once again, but without running callbacks to bring
     * scene to a fully evaluated state with user modifications taken into account. */
    if (DEG_is_fully_evaluated(depsgraph)) {
      break;
    }

    /* Clear recalc flags for second pass, but back them up for editors update. */
    const bool backup = true;
    DEG_ids_clear_recalc(depsgraph, backup);
    used_multiple_passes = true;
    run_callbacks = false;
  }

  /* Inform editors about changes, using recalc flags from both passes. */
  if (used_multiple_passes) {
    DEG_ids_restore_recalc(depsgraph);
  }
  const bool is_time_update = false;
  DEG_editors_update(depsgraph, is_time_update);

  const bool backup = false;
  DEG_ids_clear_recalc(depsgraph, backup);
}

void BKE_scene_graph_update_tagged(Depsgraph *depsgraph, Main *bmain)
{
  scene_graph_update_tagged(depsgraph, bmain, false);
}

void BKE_scene_graph_evaluated_ensure(Depsgraph *depsgraph, Main *bmain)
{
  scene_graph_update_tagged(depsgraph, bmain, true);
}

void BKE_scene_graph_update_for_newframe_ex(Depsgraph *depsgraph, const bool clear_recalc)
{
  Scene *scene = DEG_get_input_scene(depsgraph);
  Main *bmain = DEG_get_bmain(depsgraph);
  bool used_multiple_passes = false;

  /* Keep this first. */
  BKE_callback_exec_id(bmain, &scene->id, BKE_CB_EVT_FRAME_CHANGE_PRE);

  for (int pass = 0; pass < 2; pass++) {
    /* Update animated image textures for particles, modifiers, gpu, etc,
     * call this at the start so modifiers with textures don't lag 1 frame.
     */
    BKE_image_editors_update_frame(bmain, scene->r.cfra);
    DEG_graph_relations_update(depsgraph);
    /* Update all objects: drivers, matrices, etc. flags set
     * by depsgraph or manual, no layer check here, gets correct flushed.
     *
     * NOTE: Only update for new frame on first iteration. Second iteration is for ensuring user
     * edits from callback are properly taken into account. Doing a time update on those would
     * lose any possible unkeyed changes made by the handler. */
    if (pass == 0) {
      const float frame = BKE_scene_frame_get(scene);
      DEG_evaluate_on_framechange(depsgraph, frame, DEG_EVALUATE_SYNC_WRITEBACK_YES);
    }
    else {
      DEG_evaluate_on_refresh(depsgraph, DEG_EVALUATE_SYNC_WRITEBACK_YES);
    }
    /* Update sound system animation. */
    BKE_scene_update_sound(depsgraph, bmain);

    /* Notify editors and python about recalc. */
    if (pass == 0) {
      BKE_callback_exec_id_depsgraph(bmain, &scene->id, depsgraph, BKE_CB_EVT_FRAME_CHANGE_POST);

      /* NOTE: Similar to this case in scene_graph_update_tagged(). Need to ensure that
       * DEG_editors_update() doesn't access freed memory of possibly removed ID. */
      DEG_graph_relations_update(depsgraph);
    }

    /* If user callback did not tag anything for update we can skip second iteration.
     * Otherwise we update scene once again, but without running callbacks to bring
     * scene to a fully evaluated state with user modifications taken into account. */
    if (DEG_is_fully_evaluated(depsgraph)) {
      break;
    }

    /* Clear recalc flags for second pass, but back them up for editors update. */
    const bool backup = true;
    DEG_ids_clear_recalc(depsgraph, backup);
    used_multiple_passes = true;
  }

  /* Inform editors about changes, using recalc flags from both passes. */
  if (used_multiple_passes) {
    DEG_ids_restore_recalc(depsgraph);
  }

  const bool is_time_update = true;
  DEG_editors_update(depsgraph, is_time_update);

  /* Clear recalc flags, can be skipped for example renderers that will read these
   * and clear the flags later. */
  if (clear_recalc) {
    const bool backup = false;
    DEG_ids_clear_recalc(depsgraph, backup);
  }
}

void BKE_scene_graph_update_for_newframe(Depsgraph *depsgraph)
{
  BKE_scene_graph_update_for_newframe_ex(depsgraph, true);
}

void BKE_scene_view_layer_graph_evaluated_ensure(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
  DEG_make_active(depsgraph);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

SceneRenderView *BKE_scene_add_render_view(Scene *sce, const char *name)
{
  if (!name) {
    name = DATA_("RenderView");
  }

  SceneRenderView *srv = MEM_callocN<SceneRenderView>(__func__);
  STRNCPY_UTF8(srv->name, name);
  BLI_uniquename(&sce->r.views,
                 srv,
                 DATA_("RenderView"),
                 '.',
                 offsetof(SceneRenderView, name),
                 sizeof(srv->name));
  BLI_addtail(&sce->r.views, srv);

  return srv;
}

bool BKE_scene_remove_render_view(Scene *scene, SceneRenderView *srv)
{
  const int act = BLI_findindex(&scene->r.views, srv);

  if (act == -1) {
    return false;
  }
  if (scene->r.views.first == scene->r.views.last) {
    /* ensure 1 view is kept */
    return false;
  }

  BLI_remlink(&scene->r.views, srv);
  MEM_freeN(srv);

  scene->r.actview = 0;

  return true;
}

/* render simplification */

int get_render_subsurf_level(const RenderData *r, int lvl, bool for_render)
{
  if (r->mode & R_SIMPLIFY) {
    if (for_render) {
      return min_ii(r->simplify_subsurf_render, lvl);
    }

    return min_ii(r->simplify_subsurf, lvl);
  }

  return lvl;
}

int get_render_child_particle_number(const RenderData *r, int child_num, bool for_render)
{
  if (r->mode & R_SIMPLIFY) {
    if (for_render) {
      return int(r->simplify_particles_render * child_num);
    }

    return int(r->simplify_particles * child_num);
  }

  return child_num;
}

Base *_setlooper_base_step(Scene **sce_iter, ViewLayer *view_layer, Base *base)
{
  if (base && base->next) {
    /* Common case, step to the next. */
    return base->next;
  }
  if ((base == nullptr) && (view_layer != nullptr)) {
    /* First time looping, return the scenes first base. */
    /* For the first loop we should get the layer from workspace when available. */
    BKE_view_layer_synced_ensure(*sce_iter, view_layer);
    ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer);
    if (object_bases->first) {
      return static_cast<Base *>(object_bases->first);
    }
    /* No base on this scene layer. */
    goto next_set;
  }
  else {
  next_set:
    /* Reached the end, get the next base in the set. */
    while ((*sce_iter = (*sce_iter)->set)) {
      ViewLayer *view_layer_set = BKE_view_layer_default_render(*sce_iter);
      base = (Base *)BKE_view_layer_object_bases_get(view_layer_set)->first;

      if (base) {
        return base;
      }
    }
  }

  return nullptr;
}

bool BKE_scene_use_shading_nodes_custom(Scene *scene)
{
  RenderEngineType *type = RE_engines_find(scene->r.engine);
  return (type && type->flag & RE_USE_SHADING_NODES_CUSTOM);
}

bool BKE_scene_use_spherical_stereo(Scene *scene)
{
  RenderEngineType *type = RE_engines_find(scene->r.engine);
  return (type && type->flag & RE_USE_SPHERICAL_STEREO);
}

bool BKE_scene_uses_blender_eevee(const Scene *scene)
{
  return STREQ(scene->r.engine, RE_engine_id_BLENDER_EEVEE);
}

bool BKE_scene_uses_blender_workbench(const Scene *scene)
{
  return STREQ(scene->r.engine, RE_engine_id_BLENDER_WORKBENCH);
}

bool BKE_scene_uses_cycles(const Scene *scene)
{
  return STREQ(scene->r.engine, RE_engine_id_CYCLES);
}

bool BKE_scene_uses_shader_previews(const Scene *scene)
{
  return BKE_scene_uses_blender_eevee(scene) || BKE_scene_uses_cycles(scene);
}

/* This enumeration has to match the one defined in the Cycles addon. */
enum eCyclesFeatureSet {
  CYCLES_FEATURES_SUPPORTED = 0,
  CYCLES_FEATURES_EXPERIMENTAL = 1,
};

void BKE_scene_base_flag_to_objects(const Scene *scene, ViewLayer *view_layer)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    BKE_scene_object_base_flag_sync_from_base(base);
  }
}

void BKE_scene_object_base_flag_sync_from_base(Base *base)
{
  Object *ob = base->object;
  ob->base_flag = base->flag;
}

void BKE_scene_disable_color_management(Scene *scene)
{
  ColorManagedDisplaySettings *display_settings = &scene->display_settings;
  ColorManagedViewSettings *view_settings = &scene->view_settings;
  const char *view;
  const char *none_display_name;

  none_display_name = IMB_colormanagement_display_get_none_name();

  STRNCPY_UTF8(display_settings->display_device, none_display_name);

  view = IMB_colormanagement_view_get_raw_or_default_name(display_settings->display_device);

  if (view) {
    STRNCPY_UTF8(view_settings->view_transform, view);
  }
}

bool BKE_scene_check_rigidbody_active(const Scene *scene)
{
  return scene && scene->rigidbody_world && scene->rigidbody_world->group &&
         !(scene->rigidbody_world->flag & RBW_FLAG_MUTED);
}

int BKE_render_num_threads(const RenderData *rd)
{
  int threads;

  /* override set from command line? */
  threads = BLI_system_num_threads_override_get();

  if (threads > 0) {
    return threads;
  }

  /* fixed number of threads specified in scene? */
  if (rd->mode & R_FIXED_THREADS) {
    threads = rd->threads;
  }
  else {
    threads = BLI_system_thread_count();
  }

  return max_ii(threads, 1);
}

int BKE_scene_num_threads(const Scene *scene)
{
  return BKE_render_num_threads(&scene->r);
}

void BKE_render_resolution(const RenderData *r, const bool use_crop, int *r_width, int *r_height)
{
  *r_width = (r->xsch * r->size) / 100;
  *r_height = (r->ysch * r->size) / 100;

  if (use_crop && (r->mode & R_BORDER) && (r->mode & R_CROP)) {
    /* Compute the difference between the integer bounds instead of multiplying by the float
     * border size directly to be consistent with how the render pipeline computes render size, see
     * for instance render_init_from_main. That's because difference in rounding and imprecisions
     * can cause off by one errors. */
    *r_width = int(r->border.xmax * *r_width) - int(r->border.xmin * *r_width);
    *r_height = int(r->border.ymax * *r_height) - int(r->border.ymin * *r_height);
  }
}

int BKE_render_preview_pixel_size(const RenderData *r)
{
  if (r->preview_pixel_size == 0) {
    return (U.pixelsize > 1.5f) ? 2 : 1;
  }
  return r->preview_pixel_size;
}

/******************** multiview *************************/

int BKE_scene_multiview_num_views_get(const RenderData *rd)
{
  int totviews = 0;

  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return 1;
  }

  if (rd->views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
    SceneRenderView *srv = static_cast<SceneRenderView *>(
        BLI_findstring(&rd->views, STEREO_LEFT_NAME, offsetof(SceneRenderView, name)));
    if ((srv && srv->viewflag & SCE_VIEW_DISABLE) == 0) {
      totviews++;
    }

    srv = static_cast<SceneRenderView *>(
        BLI_findstring(&rd->views, STEREO_RIGHT_NAME, offsetof(SceneRenderView, name)));
    if ((srv && srv->viewflag & SCE_VIEW_DISABLE) == 0) {
      totviews++;
    }
  }
  else {
    LISTBASE_FOREACH (SceneRenderView *, srv, &rd->views) {
      if ((srv->viewflag & SCE_VIEW_DISABLE) == 0) {
        totviews++;
      }
    }
  }
  return totviews;
}

bool BKE_scene_multiview_is_stereo3d(const RenderData *rd)
{
  SceneRenderView *srv[2];

  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  srv[0] = (SceneRenderView *)BLI_findstring(
      &rd->views, STEREO_LEFT_NAME, offsetof(SceneRenderView, name));
  srv[1] = (SceneRenderView *)BLI_findstring(
      &rd->views, STEREO_RIGHT_NAME, offsetof(SceneRenderView, name));

  return (srv[0] && ((srv[0]->viewflag & SCE_VIEW_DISABLE) == 0) && srv[1] &&
          ((srv[1]->viewflag & SCE_VIEW_DISABLE) == 0));
}

bool BKE_scene_multiview_is_render_view_active(const RenderData *rd, const SceneRenderView *srv)
{
  if (srv == nullptr) {
    return false;
  }

  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if (srv->viewflag & SCE_VIEW_DISABLE) {
    return false;
  }

  if (rd->views_format == SCE_VIEWS_FORMAT_MULTIVIEW) {
    return true;
  }

  /* SCE_VIEWS_SETUP_BASIC */
  if (STR_ELEM(srv->name, STEREO_LEFT_NAME, STEREO_RIGHT_NAME)) {
    return true;
  }

  return false;
}

bool BKE_scene_multiview_is_render_view_first(const RenderData *rd, const char *viewname)
{
  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return true;
  }

  if ((!viewname) || (!viewname[0])) {
    return true;
  }

  LISTBASE_FOREACH (const SceneRenderView *, srv, &rd->views) {
    if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
      return STREQ(viewname, srv->name);
    }
  }

  return true;
}

bool BKE_scene_multiview_is_render_view_last(const RenderData *rd, const char *viewname)
{
  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return true;
  }

  if ((!viewname) || (!viewname[0])) {
    return true;
  }

  LISTBASE_FOREACH_BACKWARD (const SceneRenderView *, srv, &rd->views) {
    if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
      return STREQ(viewname, srv->name);
    }
  }

  return true;
}

SceneRenderView *BKE_scene_multiview_render_view_findindex(const RenderData *rd, const int view_id)
{
  SceneRenderView *srv;
  size_t nr;

  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return nullptr;
  }

  for (srv = static_cast<SceneRenderView *>(rd->views.first), nr = 0; srv; srv = srv->next) {
    if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
      if (nr++ == view_id) {
        return srv;
      }
    }
  }
  return srv;
}

const char *BKE_scene_multiview_render_view_name_get(const RenderData *rd, const int view_id)
{
  SceneRenderView *srv = BKE_scene_multiview_render_view_findindex(rd, view_id);

  if (srv) {
    return srv->name;
  }

  return "";
}

int BKE_scene_multiview_view_id_get(const RenderData *rd, const char *viewname)
{
  SceneRenderView *srv;
  size_t nr;

  if ((!rd) || ((rd->scemode & R_MULTIVIEW) == 0)) {
    return 0;
  }

  if ((!viewname) || (!viewname[0])) {
    return 0;
  }

  for (srv = static_cast<SceneRenderView *>(rd->views.first), nr = 0; srv; srv = srv->next) {
    if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
      if (STREQ(viewname, srv->name)) {
        return nr;
      }

      nr += 1;
    }
  }

  return 0;
}

void BKE_scene_multiview_filepath_get(const SceneRenderView *srv,
                                      const char *filepath,
                                      char *r_filepath)
{
  BLI_strncpy(r_filepath, filepath, FILE_MAX);
  BLI_path_suffix(r_filepath, FILE_MAX, srv->suffix, "");
}

void BKE_scene_multiview_view_filepath_get(const RenderData *rd,
                                           const char *filepath,
                                           const char *viewname,
                                           char *r_filepath)
{
  SceneRenderView *srv;
  char suffix[FILE_MAX];

  srv = static_cast<SceneRenderView *>(
      BLI_findstring(&rd->views, viewname, offsetof(SceneRenderView, name)));
  if (srv) {
    STRNCPY(suffix, srv->suffix);
  }
  else {
    STRNCPY(suffix, viewname);
  }

  BLI_strncpy(r_filepath, filepath, FILE_MAX);
  BLI_path_suffix(r_filepath, FILE_MAX, suffix, "");
}

const char *BKE_scene_multiview_view_suffix_get(const RenderData *rd, const char *viewname)
{
  SceneRenderView *srv;

  if ((viewname == nullptr) || (viewname[0] == '\0')) {
    return viewname;
  }

  srv = static_cast<SceneRenderView *>(
      BLI_findstring(&rd->views, viewname, offsetof(SceneRenderView, name)));
  if (srv) {
    return srv->suffix;
  }

  return viewname;
}

const char *BKE_scene_multiview_view_id_suffix_get(const RenderData *rd, const int view_id)
{
  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return "";
  }

  const char *viewname = BKE_scene_multiview_render_view_name_get(rd, view_id);
  return BKE_scene_multiview_view_suffix_get(rd, viewname);
}

void BKE_scene_multiview_view_prefix_get(Scene *scene,
                                         const char *filepath,
                                         char *r_prefix,
                                         const char **r_ext)
{
  const char *unused;
  const char delims[] = {'.', '\0'};

  r_prefix[0] = '\0';

  /* Split `filepath` into base name and extension. */
  const size_t basename_len = BLI_str_rpartition(filepath, delims, r_ext, &unused);
  if (*r_ext == nullptr) {
    return;
  }
  BLI_assert(basename_len > 0);

  /* Split base name into prefix and known suffix. */
  LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
    if (BKE_scene_multiview_is_render_view_active(&scene->r, srv)) {
      const size_t suffix_len = strlen(srv->suffix);
      if (basename_len >= suffix_len &&
          STREQLEN(filepath + basename_len - suffix_len, srv->suffix, suffix_len))
      {
        BLI_strncpy(r_prefix, filepath, basename_len - suffix_len + 1);
        break;
      }
    }
  }
}

void BKE_scene_multiview_videos_dimensions_get(const RenderData *rd,
                                               const ImageFormatData *imf,
                                               const size_t width,
                                               const size_t height,
                                               size_t *r_width,
                                               size_t *r_height)
{
  if ((rd->scemode & R_MULTIVIEW) && imf->views_format == R_IMF_VIEWS_STEREO_3D) {
    IMB_stereo3d_write_dimensions(imf->stereo3d_format.display_mode,
                                  (imf->stereo3d_format.flag & S3D_SQUEEZED_FRAME) != 0,
                                  width,
                                  height,
                                  r_width,
                                  r_height);
  }
  else {
    *r_width = width;
    *r_height = height;
  }
}

int BKE_scene_multiview_num_videos_get(const RenderData *rd, const ImageFormatData *imf)
{
  if (BKE_imtype_is_movie(imf->imtype) == false) {
    return 0;
  }

  if ((rd->scemode & R_MULTIVIEW) == 0) {
    return 1;
  }

  if (imf->views_format == R_IMF_VIEWS_STEREO_3D) {
    return 1;
  }

  /* R_IMF_VIEWS_INDIVIDUAL */
  return BKE_scene_multiview_num_views_get(rd);
}

void BKE_scene_ppm_get(const RenderData *rd, double r_ppm[2])
{
  /* Should not be zero, prevent divide by zero if it is. */
  if (UNLIKELY(rd->ppm_base == 0.0f)) {
    /* Zero PPM should be ignored. */
    r_ppm[0] = 0.0;
    r_ppm[1] = 0.0;
  }
  /* Non-square aspects result in a lower density on one dimension to indicate
   * the image should be stretched to match the original size causing the pixel
   * density to be lower on that dimension. */
  double xasp = 1.0, yasp = 1.0;
  if (rd->xasp < rd->yasp) {
    yasp = double(rd->xasp) / double(rd->yasp);
  }
  else if (rd->xasp > rd->yasp) {
    xasp = double(rd->yasp) / double(rd->xasp);
  }

  const double ppm_base = rd->ppm_base;
  const double ppm_factor = rd->ppm_factor;

  r_ppm[0] = (ppm_factor / ppm_base) * xasp;
  r_ppm[1] = (ppm_factor / ppm_base) * yasp;
}

/* Manipulation of depsgraph storage. */

/* This is a key which identifies depsgraph. */
struct DepsgraphKey {
  const ViewLayer *view_layer;
  /* TODO(sergey): Need to include window somehow (same layer might be in a
   * different states in different windows).
   */
};

static uint depsgraph_key_hash(const void *key_v)
{
  const DepsgraphKey *key = static_cast<const DepsgraphKey *>(key_v);
  uint hash = BLI_ghashutil_ptrhash(key->view_layer);
  /* TODO(sergey): Include hash from other fields in the key. */
  return hash;
}

static bool depsgraph_key_compare(const void *key_a_v, const void *key_b_v)
{
  const DepsgraphKey *key_a = static_cast<const DepsgraphKey *>(key_a_v);
  const DepsgraphKey *key_b = static_cast<const DepsgraphKey *>(key_b_v);
  /* TODO(sergey): Compare rest of. */
  return !(key_a->view_layer == key_b->view_layer);
}

static void depsgraph_key_free(void *key_v)
{
  DepsgraphKey *key = static_cast<DepsgraphKey *>(key_v);
  MEM_freeN(key);
}

static void depsgraph_key_value_free(void *value)
{
  Depsgraph *depsgraph = static_cast<Depsgraph *>(value);
  DEG_graph_free(depsgraph);
}

void BKE_scene_allocate_depsgraph_hash(Scene *scene)
{
  scene->depsgraph_hash = BLI_ghash_new(
      depsgraph_key_hash, depsgraph_key_compare, "Scene Depsgraph Hash");
}

void BKE_scene_ensure_depsgraph_hash(Scene *scene)
{
  if (scene->depsgraph_hash == nullptr) {
    BKE_scene_allocate_depsgraph_hash(scene);
  }
}

void BKE_scene_free_depsgraph_hash(Scene *scene)
{
  if (scene->depsgraph_hash == nullptr) {
    return;
  }
  BLI_ghash_free(scene->depsgraph_hash, depsgraph_key_free, depsgraph_key_value_free);
  scene->depsgraph_hash = nullptr;
}

void BKE_scene_free_view_layer_depsgraph(Scene *scene, ViewLayer *view_layer)
{
  if (scene->depsgraph_hash != nullptr) {
    DepsgraphKey key = {view_layer};
    BLI_ghash_remove(scene->depsgraph_hash, &key, depsgraph_key_free, depsgraph_key_value_free);
  }
}

/* Query depsgraph for a specific contexts. */

static Depsgraph **scene_get_depsgraph_p(Scene *scene,
                                         ViewLayer *view_layer,
                                         const bool allocate_ghash_entry)
{
  /* bmain may be nullptr here! */
  BLI_assert(scene != nullptr);
  BLI_assert(view_layer != nullptr);
  BLI_assert(BKE_scene_has_view_layer(scene, view_layer));

  /* Make sure hash itself exists. */
  if (allocate_ghash_entry) {
    BKE_scene_ensure_depsgraph_hash(scene);
  }
  if (scene->depsgraph_hash == nullptr) {
    return nullptr;
  }

  DepsgraphKey key;
  key.view_layer = view_layer;

  Depsgraph **depsgraph_ptr;
  if (!allocate_ghash_entry) {
    depsgraph_ptr = (Depsgraph **)BLI_ghash_lookup_p(scene->depsgraph_hash, &key);
    return depsgraph_ptr;
  }

  DepsgraphKey **key_ptr;
  if (BLI_ghash_ensure_p_ex(
          scene->depsgraph_hash, &key, (void ***)&key_ptr, (void ***)&depsgraph_ptr))
  {
    return depsgraph_ptr;
  }

  /* Depsgraph was not found in the ghash, but the key still needs allocating. */
  *key_ptr = MEM_callocN<DepsgraphKey>(__func__);
  **key_ptr = key;

  *depsgraph_ptr = nullptr;
  return depsgraph_ptr;
}

static Depsgraph **scene_ensure_depsgraph_p(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  BLI_assert(bmain != nullptr);

  Depsgraph **depsgraph_ptr = scene_get_depsgraph_p(scene, view_layer, true);
  if (depsgraph_ptr == nullptr) {
    /* The scene has no depsgraph hash. */
    return nullptr;
  }
  if (*depsgraph_ptr != nullptr) {
    /* The depsgraph was found, no need to allocate. */
    return depsgraph_ptr;
  }

  /* Allocate a new depsgraph. scene_get_depsgraph_p() already ensured that the pointer is stored
   * in the scene's depsgraph hash. */
  *depsgraph_ptr = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);

  /* TODO(sergey): Would be cool to avoid string format print,
   * but is a bit tricky because we can't know in advance whether
   * we will ever enable debug messages for this depsgraph.
   */
  char name[1024];
  SNPRINTF_UTF8(name, "%s :: %s", scene->id.name, view_layer->name);
  DEG_debug_name_set(*depsgraph_ptr, name);

  /* These viewport depsgraphs communicate changes to the editors. */
  DEG_enable_editors_update(*depsgraph_ptr);

  return depsgraph_ptr;
}

Depsgraph *BKE_scene_get_depsgraph(const Scene *scene, const ViewLayer *view_layer)
{
  BLI_assert(BKE_scene_has_view_layer(scene, view_layer));

  if (scene->depsgraph_hash == nullptr) {
    return nullptr;
  }

  DepsgraphKey key;
  key.view_layer = view_layer;
  return static_cast<Depsgraph *>(BLI_ghash_lookup(scene->depsgraph_hash, &key));
}

Depsgraph *BKE_scene_ensure_depsgraph(Main *bmain, Scene *scene, ViewLayer *view_layer)
{
  Depsgraph **depsgraph_ptr = scene_ensure_depsgraph_p(bmain, scene, view_layer);
  return (depsgraph_ptr != nullptr) ? *depsgraph_ptr : nullptr;
}

static char *scene_undo_depsgraph_gen_key(Scene *scene, ViewLayer *view_layer, char *key_full)
{
  if (key_full == nullptr) {
    key_full = MEM_calloc_arrayN<char>(MAX_ID_NAME + FILE_MAX + MAX_NAME, __func__);
  }

  size_t key_full_offset = BLI_strncpy_rlen(key_full, scene->id.name, MAX_ID_NAME);
  if (ID_IS_LINKED(scene)) {
    key_full_offset += BLI_strncpy_rlen(
        key_full + key_full_offset, scene->id.lib->filepath, FILE_MAX);
  }
  key_full_offset += BLI_strncpy_rlen(key_full + key_full_offset, view_layer->name, MAX_NAME);
  BLI_assert(key_full_offset < MAX_ID_NAME + FILE_MAX + MAX_NAME);

  return key_full;
}

GHash *BKE_scene_undo_depsgraphs_extract(Main *bmain)
{
  GHash *depsgraph_extract = BLI_ghash_new(
      BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, __func__);

  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->depsgraph_hash == nullptr) {
      /* In some cases, e.g. when undo has to perform multiple steps at once, no depsgraph will
       * be built so this pointer may be nullptr. */
      continue;
    }
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      DepsgraphKey key;
      key.view_layer = view_layer;
      Depsgraph **depsgraph = (Depsgraph **)BLI_ghash_lookup_p(scene->depsgraph_hash, &key);

      if (depsgraph != nullptr && *depsgraph != nullptr) {
        char *key_full = scene_undo_depsgraph_gen_key(scene, view_layer, nullptr);

        /* We steal the depsgraph from the scene. */
        BLI_ghash_insert(depsgraph_extract, key_full, *depsgraph);
        *depsgraph = nullptr;
      }
    }
  }

  return depsgraph_extract;
}

void BKE_scene_undo_depsgraphs_restore(Main *bmain, GHash *depsgraph_extract)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      char key_full[MAX_ID_NAME + FILE_MAX + MAX_NAME] = {0};
      scene_undo_depsgraph_gen_key(scene, view_layer, key_full);

      Depsgraph **depsgraph_extract_ptr = (Depsgraph **)BLI_ghash_lookup_p(depsgraph_extract,
                                                                           key_full);
      if (depsgraph_extract_ptr == nullptr) {
        continue;
      }
      BLI_assert(*depsgraph_extract_ptr != nullptr);

      Depsgraph **depsgraph_scene_ptr = scene_get_depsgraph_p(scene, view_layer, true);
      BLI_assert(depsgraph_scene_ptr != nullptr);
      BLI_assert(*depsgraph_scene_ptr == nullptr);

      /* We steal the depsgraph back from our 'extract' storage to the scene. */
      Depsgraph *depsgraph = *depsgraph_extract_ptr;

      DEG_graph_replace_owners(depsgraph, bmain, scene, view_layer);

      DEG_graph_tag_relations_update(depsgraph);

      *depsgraph_scene_ptr = depsgraph;
      *depsgraph_extract_ptr = nullptr;
    }
  }

  BLI_ghash_free(depsgraph_extract, MEM_freeN, depsgraph_key_value_free);
}

/* -------------------------------------------------------------------- */
/** \name Scene Orientation
 * \{ */

void BKE_scene_transform_orientation_remove(Scene *scene, TransformOrientation *orientation)
{
  const int orientation_index = BKE_scene_transform_orientation_get_index(scene, orientation);

  for (int i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
    TransformOrientationSlot *orient_slot = &scene->orientation_slots[i];
    if (orient_slot->index_custom == orientation_index) {
      /* could also use orientation_index-- */
      orient_slot->type = V3D_ORIENT_GLOBAL;
      orient_slot->index_custom = -1;
    }
    else if (orient_slot->index_custom > orientation_index) {
      BLI_assert(orient_slot->type == V3D_ORIENT_CUSTOM);
      orient_slot->index_custom--;
    }
  }

  BLI_freelinkN(&scene->transform_spaces, orientation);
}

TransformOrientation *BKE_scene_transform_orientation_find(const Scene *scene, const int index)
{
  return static_cast<TransformOrientation *>(BLI_findlink(&scene->transform_spaces, index));
}

int BKE_scene_transform_orientation_get_index(const Scene *scene,
                                              const TransformOrientation *orientation)
{
  return BLI_findindex(&scene->transform_spaces, orientation);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Cursor Rotation
 *
 * Matches #BKE_object_rot_to_mat3 and #BKE_object_mat3_to_rot.
 * \{ */

template<> blender::float3x3 View3DCursor::matrix<blender::float3x3>() const
{
  blender::float3x3 mat;
  if (this->rotation_mode > 0) {
    eulO_to_mat3(mat.ptr(), this->rotation_euler, this->rotation_mode);
  }
  else if (this->rotation_mode == ROT_MODE_AXISANGLE) {
    axis_angle_to_mat3(mat.ptr(), this->rotation_axis, this->rotation_angle);
  }
  else {
    float tquat[4];
    normalize_qt_qt(tquat, this->rotation_quaternion);
    quat_to_mat3(mat.ptr(), tquat);
  }
  return mat;
}

blender::math::Quaternion View3DCursor::rotation() const
{
  blender::math::Quaternion quat;
  if (this->rotation_mode > 0) {
    eulO_to_quat(&quat.w, this->rotation_euler, this->rotation_mode);
  }
  else if (this->rotation_mode == ROT_MODE_AXISANGLE) {
    axis_angle_to_quat(&quat.w, this->rotation_axis, this->rotation_angle);
  }
  else {
    normalize_qt_qt(&quat.w, this->rotation_quaternion);
  }
  return quat;
}

void View3DCursor::set_matrix(const blender::float3x3 &mat, const bool use_compat)
{
  BLI_ASSERT_UNIT_M3(mat.ptr());

  switch (this->rotation_mode) {
    case ROT_MODE_QUAT: {
      float quat[4];
      mat3_normalized_to_quat(quat, mat.ptr());
      if (use_compat) {
        float quat_orig[4];
        copy_v4_v4(quat_orig, this->rotation_quaternion);
        quat_to_compatible_quat(this->rotation_quaternion, quat, quat_orig);
      }
      else {
        copy_v4_v4(this->rotation_quaternion, quat);
      }
      break;
    }
    case ROT_MODE_AXISANGLE: {
      mat3_to_axis_angle(this->rotation_axis, &this->rotation_angle, mat.ptr());
      break;
    }
    default: {
      if (use_compat) {
        mat3_to_compatible_eulO(
            this->rotation_euler, this->rotation_euler, this->rotation_mode, mat.ptr());
      }
      else {
        mat3_to_eulO(this->rotation_euler, this->rotation_mode, mat.ptr());
      }
      break;
    }
  }
}

void View3DCursor::set_rotation(const blender::math::Quaternion &quat, bool use_compat)
{
  BLI_ASSERT_UNIT_QUAT(&quat.w);

  switch (this->rotation_mode) {
    case ROT_MODE_QUAT: {
      if (use_compat) {
        float quat_orig[4];
        copy_v4_v4(quat_orig, this->rotation_quaternion);
        quat_to_compatible_quat(this->rotation_quaternion, &quat.w, quat_orig);
      }
      else {
        copy_qt_qt(this->rotation_quaternion, &quat.w);
      }
      break;
    }
    case ROT_MODE_AXISANGLE: {
      quat_to_axis_angle(this->rotation_axis, &this->rotation_angle, &quat.w);
      break;
    }
    default: {
      if (use_compat) {
        quat_to_compatible_eulO(
            this->rotation_euler, this->rotation_euler, this->rotation_mode, &quat.w);
      }
      else {
        quat_to_eulO(this->rotation_euler, this->rotation_mode, &quat.w);
      }
      break;
    }
  }
}

template<> blender::float4x4 View3DCursor::matrix<blender::float4x4>() const
{
  blender::float4x4 mat(this->matrix<blender::float3x3>());
  mat.location() = blender::float3(this->location);
  return mat;
}

void View3DCursor::set_matrix(const blender::float4x4 &mat, const bool use_compat)
{
  this->set_matrix(blender::float3x3(mat), use_compat);
  copy_v3_v3(this->location, mat.location());
}

/** \} */
