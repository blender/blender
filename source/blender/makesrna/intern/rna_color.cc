/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_color_types.h"
#include "DNA_texture_types.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

const EnumPropertyItem rna_enum_color_space_convert_default_items[] = {
    {0,
     "NONE",
     0,
     "None",
     "Do not perform any color transform on load, treat colors as in scene linear space "
     "already"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "RNA_access.hh"
#  include "RNA_path.hh"

#  include "DNA_image_types.h"
#  include "DNA_material_types.h"
#  include "DNA_movieclip_types.h"
#  include "DNA_node_types.h"
#  include "DNA_object_types.h"
#  include "DNA_particle_types.h"
#  include "DNA_sequence_types.h"

#  include "MEM_guardedalloc.h"

#  include "BKE_colorband.hh"
#  include "BKE_colortools.hh"
#  include "BKE_image.hh"
#  include "BKE_linestyle.h"
#  include "BKE_main_invariants.hh"
#  include "BKE_movieclip.h"
#  include "BKE_node.hh"
#  include "BKE_node_legacy_types.hh"
#  include "BKE_node_tree_update.hh"

#  include "DEG_depsgraph.hh"

#  include "ED_node.hh"

#  include "IMB_colormanagement.hh"
#  include "IMB_imbuf.hh"

#  include "MOV_read.hh"

#  include "SEQ_iterator.hh"
#  include "SEQ_relations.hh"
#  include "SEQ_thumbnail_cache.hh"

struct SeqCurveMappingUpdateData {
  Scene *scene;
  CurveMapping *curve;
};

static bool seq_update_modifier_curve(Strip *strip, void *user_data)
{
  /* Invalidate cache of any strips that have modifiers using this
   * curve mapping. */
  SeqCurveMappingUpdateData *data = static_cast<SeqCurveMappingUpdateData *>(user_data);
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    if (smd->type == eSeqModifierType_Curves) {
      CurvesModifierData *cmd = reinterpret_cast<CurvesModifierData *>(smd);
      if (&cmd->curve_mapping == data->curve) {
        blender::seq::relations_invalidate_cache(data->scene, strip);
      }
    }
  }
  return true;
}

static void seq_notify_curve_update(CurveMapping *curve, ID *id)
{
  if (id && GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->ed) {
      SeqCurveMappingUpdateData data{scene, curve};
      blender::seq::foreach_strip(&scene->ed->seqbase, seq_update_modifier_curve, &data);
    }
  }
}

static int rna_CurveMapping_curves_length(PointerRNA *ptr)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;
  int len;

  for (len = 0; len < CM_TOT; len++) {
    if (!cumap->cm[len].curve) {
      break;
    }
  }

  return len;
}

static void rna_CurveMapping_curves_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;

  rna_iterator_array_begin(
      iter, ptr, cumap->cm, sizeof(CurveMap), rna_CurveMapping_curves_length(ptr), 0, nullptr);
}

static void rna_CurveMapping_clip_set(PointerRNA *ptr, bool value)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;

  /* Clipping is always done for wrapped curves, so don't allow user to change it. */
  if (cumap->flag & CUMA_USE_WRAPPING) {
    return;
  }

  if (value) {
    cumap->flag |= CUMA_DO_CLIP;
  }
  else {
    cumap->flag &= ~CUMA_DO_CLIP;
  }

  BKE_curvemapping_changed(cumap, false);
}

static void rna_CurveMapping_black_level_set(PointerRNA *ptr, const float *values)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;
  cumap->black[0] = values[0];
  cumap->black[1] = values[1];
  cumap->black[2] = values[2];
  BKE_curvemapping_set_black_white(cumap, nullptr, nullptr);
}

static void rna_CurveMapping_white_level_set(PointerRNA *ptr, const float *values)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;
  cumap->white[0] = values[0];
  cumap->white[1] = values[1];
  cumap->white[2] = values[2];
  BKE_curvemapping_set_black_white(cumap, nullptr, nullptr);
}

static void rna_CurveMapping_tone_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  /* Film-like tone only works with the combined curve, which is the fourth curve, so if the user
   * changed to film-like make the combined curve current, as we now hide the rest of the curves
   * since they no longer have an effect. */
  CurveMapping *curve_mapping = (CurveMapping *)ptr->data;
  if (curve_mapping->tone == CURVE_TONE_FILMLIKE) {
    curve_mapping->cur = 3;
  }

  seq_notify_curve_update(curve_mapping, ptr->owner_id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_CurveMapping_extend_update(Main * /*bmain*/,
                                           Scene * /*scene*/,
                                           PointerRNA * /*ptr*/)
{
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_CurveMapping_clipminx_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;

  *min = -100.0f;
  *max = cumap->clipr.xmax;
}

static void rna_CurveMapping_clipminy_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;

  *min = -100.0f;
  *max = cumap->clipr.ymax;
}

static void rna_CurveMapping_clipmaxx_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;

  *min = cumap->clipr.xmin;
  *max = 100.0f;
}

static void rna_CurveMapping_clipmaxy_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  CurveMapping *cumap = (CurveMapping *)ptr->data;

  *min = cumap->clipr.ymin;
  *max = 100.0f;
}

static std::optional<std::string> rna_ColorRamp_path(const PointerRNA *ptr)
{
  /* handle the cases where a single data-block may have 2 ramp types */
  if (ptr->owner_id) {
    ID *id = ptr->owner_id;

    switch (GS(id->name)) {
      case ID_NT: {
        bNodeTree *ntree = (bNodeTree *)id;
        bNode *node;

        for (node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
          if (ELEM(node->type_legacy, SH_NODE_VALTORGB, TEX_NODE_VALTORGB)) {
            if (node->storage == ptr->data) {
              /* all node color ramp properties called 'color_ramp'
               * prepend path from ID to the node
               */
              PointerRNA node_ptr = RNA_pointer_create_discrete(id, &RNA_Node, node);
              std::string node_path = RNA_path_from_ID_to_struct(&node_ptr).value_or("");
              return fmt::format("{}.color_ramp", node_path);
            }
          }
        }
        break;
      }

      case ID_LS: {
        /* may be nullptr */
        return BKE_linestyle_path_to_color_ramp((FreestyleLineStyle *)id, (ColorBand *)ptr->data);
      }

      default:
        /* everything else just uses 'color_ramp' */
        return "color_ramp";
    }
  }
  else {
    /* everything else just uses 'color_ramp' */
    return "color_ramp";
  }

  return std::nullopt;
}

static std::optional<std::string> rna_ColorRampElement_path(const PointerRNA *ptr)
{
  PointerRNA ramp_ptr;
  PropertyRNA *prop;
  std::optional<std::string> path;
  int index;

  /* helper macro for use here to try and get the path
   * - this calls the standard code for getting a path to a texture...
   */

#  define COLRAMP_GETPATH \
    { \
      prop = RNA_struct_find_property(&ramp_ptr, "elements"); \
      if (prop) { \
        index = RNA_property_collection_lookup_index(&ramp_ptr, prop, ptr); \
        if (index != -1) { \
          std::string texture_path = rna_ColorRamp_path(&ramp_ptr).value_or(""); \
          path = fmt::format("{}.elements[{}]", texture_path, index); \
        } \
      } \
    } \
    (void)0

  /* determine the path from the ID-block to the ramp */
  /* FIXME: this is a very slow way to do it, but it will have to suffice... */
  if (ptr->owner_id) {
    ID *id = ptr->owner_id;

    switch (GS(id->name)) {
      case ID_NT: {
        bNodeTree *ntree = (bNodeTree *)id;
        bNode *node;

        for (node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
          if (ELEM(node->type_legacy, SH_NODE_VALTORGB, TEX_NODE_VALTORGB)) {
            ramp_ptr = RNA_pointer_create_discrete(id, &RNA_ColorRamp, node->storage);
            COLRAMP_GETPATH;
          }
        }
        break;
      }
      case ID_LS: {
        ListBase listbase;
        LinkData *link;

        BKE_linestyle_modifier_list_color_ramps((FreestyleLineStyle *)id, &listbase);
        for (link = (LinkData *)listbase.first; link; link = link->next) {
          ramp_ptr = RNA_pointer_create_discrete(id, &RNA_ColorRamp, link->data);
          COLRAMP_GETPATH;
        }
        BLI_freelistN(&listbase);
        break;
      }

      default: /* everything else should have a "color_ramp" property */
      {
        /* create pointer to the ID block, and try to resolve "color_ramp" pointer */
        ramp_ptr = RNA_id_pointer_create(id);
        if (RNA_path_resolve(&ramp_ptr, "color_ramp", &ramp_ptr, &prop)) {
          COLRAMP_GETPATH;
        }
        break;
      }
    }
  }

  /* cleanup the macro we defined */
#  undef COLRAMP_GETPATH

  return path;
}

static void rna_ColorRamp_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  if (ptr->owner_id) {
    ID *id = ptr->owner_id;

    switch (GS(id->name)) {
      case ID_MA: {
        Material *ma = (Material *)ptr->owner_id;

        DEG_id_tag_update(&ma->id, 0);
        WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, ma);
        break;
      }
      case ID_NT: {
        bNodeTree *ntree = (bNodeTree *)id;
        bNode *node;

        for (node = static_cast<bNode *>(ntree->nodes.first); node; node = node->next) {
          if (ELEM(node->type_legacy, SH_NODE_VALTORGB, TEX_NODE_VALTORGB)) {
            BKE_ntree_update_tag_node_property(ntree, node);
            BKE_main_ensure_invariants(*bmain, ntree->id);
          }
        }
        break;
      }
      case ID_TE: {
        Tex *tex = (Tex *)ptr->owner_id;

        DEG_id_tag_update(&tex->id, 0);
        WM_main_add_notifier(NC_TEXTURE, tex);
        break;
      }
      case ID_LS: {
        FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->owner_id;

        WM_main_add_notifier(NC_LINESTYLE, linestyle);
        break;
      }
      /* Color Ramp for particle display is owned by the object (see #54422) */
      case ID_OB:
      case ID_PA: {
        ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

        WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, part);
      }
      default:
        break;
    }
  }
}

static void rna_ColorRamp_eval(ColorBand *coba, float position, float color[4])
{
  BKE_colorband_evaluate(coba, position, color);
}

static CBData *rna_ColorRampElement_new(ColorBand *coba, ReportList *reports, float position)
{
  CBData *element = BKE_colorband_element_add(coba, position);

  if (element == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Unable to add element to colorband (limit %d)", MAXCOLORBAND);
  }

  return element;
}

static void rna_ColorRampElement_remove(ColorBand *coba,
                                        ReportList *reports,
                                        PointerRNA *element_ptr)
{
  CBData *element = static_cast<CBData *>(element_ptr->data);
  int index = int(element - coba->data);
  if (!BKE_colorband_element_remove(coba, index)) {
    BKE_report(reports, RPT_ERROR, "Element not found in element collection or last element");
    return;
  }

  element_ptr->invalidate();
}

static void rna_CurveMap_remove_point(CurveMap *cuma, ReportList *reports, PointerRNA *point_ptr)
{
  CurveMapPoint *point = static_cast<CurveMapPoint *>(point_ptr->data);
  if (BKE_curvemap_remove_point(cuma, point) == false) {
    BKE_report(reports, RPT_ERROR, "Unable to remove curve point");
    return;
  }

  point_ptr->invalidate();
}

static void rna_Scopes_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scopes *s = (Scopes *)ptr->data;
  s->ok = 0;
}

static const ColorManagedDisplaySettings *rna_display_settings_from_view_settings(
    const PointerRNA *ptr, const Scene *scene = nullptr)
{
  /* Assumes view_settings and display_settings are stored next to each other. */
  PointerRNA parent_ptr = ptr->parent();
  if (parent_ptr.data) {
    PointerRNA display_ptr = RNA_pointer_get(&parent_ptr, "display_settings");
    if (display_ptr.type == &RNA_ColorManagedDisplaySettings) {
      return display_ptr.data_as<const ColorManagedDisplaySettings>();
    }
  }

  if (ptr->owner_id && GS(ptr->owner_id) == ID_SCE) {
    return &reinterpret_cast<const Scene *>(ptr->owner_id)->display_settings;
  }

  if (scene) {
    /* Shouldn't be necessary and is not correct in general, but just in case. */
    return &scene->display_settings;
  }

  return nullptr;
}

static ColorManagedViewSettings *rna_view_settings_from_display_settings(PointerRNA *ptr)
{
  /* Assumes view_settings and display_settings are stored next to each other. */
  PointerRNA parent_ptr = ptr->parent();
  if (parent_ptr.data) {
    PointerRNA view_ptr = RNA_pointer_get(&parent_ptr, "view_settings");
    if (view_ptr.type == &RNA_ColorManagedViewSettings) {
      return view_ptr.data_as<ColorManagedViewSettings>();
    }
  }

  return nullptr;
}

static int rna_ColorManagedDisplaySettings_display_device_get(PointerRNA *ptr)
{
  ColorManagedDisplaySettings *display = (ColorManagedDisplaySettings *)ptr->data;

  return IMB_colormanagement_display_get_named_index(display->display_device);
}

static void rna_ColorManagedDisplaySettings_display_device_set(PointerRNA *ptr, int value)
{
  ColorManagedDisplaySettings *display = (ColorManagedDisplaySettings *)ptr->data;
  const char *name = IMB_colormanagement_display_get_indexed_name(value);

  if (name) {
    STRNCPY_UTF8(display->display_device, name);
  }
}

static const EnumPropertyItem *rna_ColorManagedDisplaySettings_display_device_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_display_items_add(&items, &totitem);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;

  return items;
}

static void rna_display_and_view_settings_node_update(Main *bmain, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  if (id && GS(id->name) == ID_NT) {
    /* Find a node ancestor and tag it. */
    PointerRNA node_ptr = ptr->parent();
    while (node_ptr.data && !RNA_struct_is_a(node_ptr.type, &RNA_Node)) {
      node_ptr = node_ptr.parent();
    }

    if (node_ptr.data) {
      bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
      bNode *node = node_ptr.data_as<bNode>();
      BKE_ntree_update_tag_node_property(ntree, node);
      BKE_main_ensure_invariants(*bmain, ntree->id);
    }
  }
}

static void rna_ColorManagedDisplaySettings_display_device_update(Main *bmain,
                                                                  Scene * /*scene*/,
                                                                  PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  if (id && GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;

    IMB_colormanagement_validate_settings(&scene->display_settings, &scene->view_settings);

    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);

    /* Color management can be baked into shaders, need to refresh. */
    for (Material *ma = static_cast<Material *>(bmain->materials.first); ma;
         ma = static_cast<Material *>(ma->id.next))
    {
      DEG_id_tag_update(&ma->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
  else {
    ColorManagedViewSettings *view_settings = rna_view_settings_from_display_settings(ptr);
    if (view_settings) {
      IMB_colormanagement_validate_settings(ptr->data_as<const ColorManagedDisplaySettings>(),
                                            view_settings);
      if (ptr->owner_id) {
        DEG_id_tag_update(ptr->owner_id, 0);
      }
    }

    rna_display_and_view_settings_node_update(bmain, ptr);
  }
}

static int rna_ColorManagedViewSettings_view_transform_get(PointerRNA *ptr)
{
  ColorManagedViewSettings *view = (ColorManagedViewSettings *)ptr->data;
  return IMB_colormanagement_view_get_id_by_name(view->view_transform);
}

static void rna_ColorManagedViewSettings_view_transform_set(PointerRNA *ptr, int value)
{
  ColorManagedViewSettings *view = (ColorManagedViewSettings *)ptr->data;

  const char *view_name = IMB_colormanagement_view_get_name_by_id(value);
  if (!view_name) {
    return;
  }

  STRNCPY_UTF8(view->view_transform, view_name);

  const char *look_name = IMB_colormanagement_look_validate_for_view(view_name, view->look);
  if (look_name) {
    STRNCPY_UTF8(view->look, look_name);
  }
}

static const EnumPropertyItem *rna_ColorManagedViewSettings_view_transform_itemf(
    bContext *C, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free)
{
  const ColorManagedDisplaySettings *display_settings = rna_display_settings_from_view_settings(
      ptr, CTX_data_scene(C));

  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_view_items_add(&items, &totitem, display_settings->display_device);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;
  return items;
}

static int rna_ColorManagedViewSettings_look_get(PointerRNA *ptr)
{
  ColorManagedViewSettings *view = (ColorManagedViewSettings *)ptr->data;

  return IMB_colormanagement_look_get_named_index(view->look);
}

static void rna_ColorManagedViewSettings_look_set(PointerRNA *ptr, int value)
{
  ColorManagedViewSettings *view = (ColorManagedViewSettings *)ptr->data;

  const char *name = IMB_colormanagement_look_get_indexed_name(value);

  if (name) {
    STRNCPY_UTF8(view->look, name);
  }
}

static const EnumPropertyItem *rna_ColorManagedViewSettings_look_itemf(bContext * /*C*/,
                                                                       PointerRNA *ptr,
                                                                       PropertyRNA * /*prop*/,
                                                                       bool *r_free)
{
  ColorManagedViewSettings *view = (ColorManagedViewSettings *)ptr->data;
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_look_items_add(&items, &totitem, view->view_transform);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;
  return items;
}

static void rna_ColorManagedViewSettings_use_curves_set(PointerRNA *ptr, bool value)
{
  ColorManagedViewSettings *view_settings = (ColorManagedViewSettings *)ptr->data;

  if (value) {
    view_settings->flag |= COLORMANAGE_VIEW_USE_CURVES;

    if (view_settings->curve_mapping == nullptr) {
      view_settings->curve_mapping = BKE_curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
    }
  }
  else {
    view_settings->flag &= ~COLORMANAGE_VIEW_USE_CURVES;
  }
}

static void rna_ColorManagedViewSettings_whitepoint_get(PointerRNA *ptr, float value[3])
{
  const ColorManagedViewSettings *view_settings = (ColorManagedViewSettings *)ptr->data;
  IMB_colormanagement_get_whitepoint(view_settings->temperature, view_settings->tint, value);
}

static void rna_ColorManagedViewSettings_whitepoint_set(PointerRNA *ptr, const float value[3])
{
  ColorManagedViewSettings *view_settings = (ColorManagedViewSettings *)ptr->data;
  IMB_colormanagement_set_whitepoint(value, view_settings->temperature, view_settings->tint);
}

static bool rna_ColorManagedViewSettings_is_hdr_get(PointerRNA *ptr)
{
  ColorManagedViewSettings *view_settings = (ColorManagedViewSettings *)ptr->data;
  if (GS(ptr->owner_id->name) != ID_SCE) {
    return false;
  }
  const Scene *scene = reinterpret_cast<const Scene *>(ptr->owner_id);
  if (&scene->view_settings != view_settings) {
    return false;
  }
  return IMB_colormanagement_display_is_hdr(&scene->display_settings,
                                            view_settings->view_transform);
}

static bool rna_ColorManagedViewSettings_support_emulation_get(PointerRNA *ptr)
{
  ColorManagedViewSettings *view_settings = (ColorManagedViewSettings *)ptr->data;
  if (GS(ptr->owner_id->name) != ID_SCE) {
    return false;
  }
  const Scene *scene = reinterpret_cast<const Scene *>(ptr->owner_id);
  if (&scene->view_settings != view_settings) {
    return false;
  }
  return IMB_colormanagement_display_support_emulation(&scene->display_settings,
                                                       view_settings->view_transform);
}

static int rna_ViewSettings_only_view_look_editable(const PointerRNA *ptr, const char **r_info)
{
  ColorManagedViewSettings *view_settings = (ColorManagedViewSettings *)ptr->data;

  if (view_settings->flag & COLORMANAGE_VIEW_ONLY_VIEW_LOOK) {
    if (r_info) {
      *r_info = N_("Only view transform and look can be edited for these settings");
    }
    return 0;
  }

  return PROP_EDITABLE;
}

static bool rna_ColorManagedColorspaceSettings_is_data_get(PointerRNA *ptr)
{
  ColorManagedColorspaceSettings *colorspace = (ColorManagedColorspaceSettings *)ptr->data;
  const char *data_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA);
  return STREQ(colorspace->name, data_name);
}

static void rna_ColorManagedColorspaceSettings_is_data_set(PointerRNA *ptr, bool value)
{
  ColorManagedColorspaceSettings *colorspace = (ColorManagedColorspaceSettings *)ptr->data;
  if (value) {
    const char *data_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA);
    STRNCPY_UTF8(colorspace->name, data_name);
  }
}

static int rna_ColorManagedColorspaceSettings_colorspace_get(PointerRNA *ptr)
{
  ColorManagedColorspaceSettings *colorspace = (ColorManagedColorspaceSettings *)ptr->data;

  return IMB_colormanagement_colorspace_get_named_index(colorspace->name);
}

static void rna_ColorManagedColorspaceSettings_colorspace_set(PointerRNA *ptr, int value)
{
  ColorManagedColorspaceSettings *colorspace = (ColorManagedColorspaceSettings *)ptr->data;
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    STRNCPY_UTF8(colorspace->name, name);
  }
}

static const EnumPropertyItem *rna_ColorManagedColorspaceSettings_colorspace_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_colorspace_items_add(&items, &totitem);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;

  return items;
}

struct Seq_colorspace_cb_data {
  ColorManagedColorspaceSettings *colorspace_settings;
  Strip *r_seq;
};

/**
 * Color-space could be changed for scene, but also sequencer-strip.
 * If property pointer matches one of strip, set `r_seq`,
 * so not all cached images have to be invalidated.
 */
static bool strip_find_colorspace_settings_cb(Strip *strip, void *user_data)
{
  Seq_colorspace_cb_data *cd = (Seq_colorspace_cb_data *)user_data;
  if (strip->data && &strip->data->colorspace_settings == cd->colorspace_settings) {
    cd->r_seq = strip;
    return false;
  }
  return true;
}

static void rna_ColorManagedColorspaceSettings_reload_update(Main *bmain,
                                                             Scene * /*scene*/,
                                                             PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  if (!id) {
    /* Happens for color space settings on operators. */
    return;
  }

  if (GS(id->name) == ID_IM) {
    Image *ima = (Image *)id;

    DEG_id_tag_update(&ima->id, 0);
    DEG_id_tag_update(&ima->id, ID_RECALC_SOURCE);

    BKE_image_signal(bmain, ima, nullptr, IMA_SIGNAL_COLORMANAGE);

    WM_main_add_notifier(NC_IMAGE | ND_DISPLAY, &ima->id);
    WM_main_add_notifier(NC_IMAGE | NA_EDITED, &ima->id);
  }
  else if (GS(id->name) == ID_MC) {
    MovieClip *clip = (MovieClip *)id;

    DEG_id_tag_update(&clip->id, ID_RECALC_SOURCE);
    blender::seq::relations_invalidate_movieclip_strips(bmain, clip);

    WM_main_add_notifier(NC_MOVIECLIP | ND_DISPLAY, &clip->id);
    WM_main_add_notifier(NC_MOVIECLIP | NA_EDITED, &clip->id);
  }
  else if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    blender::seq::relations_invalidate_scene_strips(bmain, scene);

    if (scene->ed) {
      ColorManagedColorspaceSettings *colorspace_settings = (ColorManagedColorspaceSettings *)
                                                                ptr->data;
      Seq_colorspace_cb_data cb_data = {colorspace_settings, nullptr};

      if (&scene->sequencer_colorspace_settings == colorspace_settings) {
        /* Scene colorspace was changed. */
        blender::seq::cache_cleanup(scene, blender::seq::CacheCleanup::All);
      }
      else {
        /* Strip colorspace was likely changed. */
        blender::seq::foreach_strip(
            &scene->ed->seqbase, strip_find_colorspace_settings_cb, &cb_data);
        Strip *strip = cb_data.r_seq;

        if (strip) {
          blender::seq::relations_strip_free_anim(strip);

          if (strip->data->proxy && strip->data->proxy->anim) {
            MOV_close(strip->data->proxy->anim);
            strip->data->proxy->anim = nullptr;
          }

          blender::seq::relations_invalidate_cache_raw(scene, strip);
        }
      }

      WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
    }
  }
}

static std::optional<std::string> rna_ColorManagedSequencerColorspaceSettings_path(
    const PointerRNA * /*ptr*/)
{
  return "sequencer_colorspace_settings";
}

static void rna_ColorManagement_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  if (!id) {
    return;
  }

  if (GS(id->name) == ID_SCE) {
    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
  }
  else {
    rna_display_and_view_settings_node_update(bmain, ptr);
  }
}

/* this function only exists because #BKE_curvemap_evaluateF uses a 'const' qualifier */
static float rna_CurveMapping_evaluateF(CurveMapping *cumap,
                                        ReportList *reports,
                                        CurveMap *cuma,
                                        float value)
{
  if (&cumap->cm[0] != cuma && &cumap->cm[1] != cuma && &cumap->cm[2] != cuma &&
      &cumap->cm[3] != cuma)
  {
    BKE_report(reports, RPT_ERROR, "CurveMapping does not own CurveMap");
    return 0.0f;
  }

  if (!cuma->table) {
    BKE_curvemapping_init(cumap);
  }
  return BKE_curvemap_evaluateF(cumap, cuma, value);
}

static void rna_CurveMap_initialize(CurveMapping *cumap)
{
  BKE_curvemapping_init(cumap);
}
#else

static void rna_def_curvemappoint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static const EnumPropertyItem prop_handle_type_items[] = {
      {0, "AUTO", 0, "Auto Handle", ""},
      {CUMA_HANDLE_AUTO_ANIM, "AUTO_CLAMPED", 0, "Auto-Clamped Handle", ""},
      {CUMA_HANDLE_VECTOR, "VECTOR", 0, "Vector Handle", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "CurveMapPoint", nullptr);
  RNA_def_struct_ui_text(srna, "CurveMapPoint", "Point of a curve used for a curve mapping");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Location", "X/Y coordinates of the curve point");

  prop = RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, prop_handle_type_items);
  RNA_def_property_ui_text(
      prop, "Handle Type", "Curve interpolation at this point: Bézier or vector");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CUMA_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Selection state of the curve point");
}

static void rna_def_curvemap_points_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "CurveMapPoints");
  srna = RNA_def_struct(brna, "CurveMapPoints", nullptr);
  RNA_def_struct_sdna(srna, "CurveMap");
  RNA_def_struct_ui_text(srna, "Curve Map Point", "Collection of Curve Map Points");

  func = RNA_def_function(srna, "new", "BKE_curvemap_insert");
  RNA_def_function_ui_description(func, "Add point to CurveMap");
  parm = RNA_def_float(func,
                       "position",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Position",
                       "Position to add point",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(
      func, "value", 0.0f, -FLT_MAX, FLT_MAX, "Value", "Value of point", -FLT_MAX, FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "point", "CurveMapPoint", "", "New point");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_CurveMap_remove_point");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete point from CurveMap");
  parm = RNA_def_pointer(func, "point", "CurveMapPoint", "", "PointElement to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_curvemap(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurveMap", nullptr);
  RNA_def_struct_ui_text(srna, "CurveMap", "Curve in a curve mapping");

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "curve", "totpoint");
  RNA_def_property_struct_type(prop, "CurveMapPoint");
  RNA_def_property_ui_text(prop, "Points", "");
  rna_def_curvemap_points_api(brna, prop);
}

static void rna_def_curvemapping(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop, *parm;
  FunctionRNA *func;

  static const EnumPropertyItem tone_items[] = {
      {CURVE_TONE_STANDARD,
       "STANDARD",
       0,
       "Standard",
       "Combined curve is applied to each channel individually, which may result in a change of "
       "hue"},
      {CURVE_TONE_FILMLIKE, "FILMLIKE", 0, "Filmlike", "Keeps the hue constant"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_extend_items[] = {
      {0, "HORIZONTAL", 0, "Horizontal", ""},
      {CUMA_EXTEND_EXTRAPOLATE, "EXTRAPOLATED", 0, "Extrapolated", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "CurveMapping", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "CurveMapping",
      "Curve mapping to map color, vector and scalar values to other values using "
      "a user defined curve");

  prop = RNA_def_property(srna, "tone", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "tone");
  RNA_def_property_enum_items(prop, tone_items);
  RNA_def_property_ui_text(prop, "Tone", "Tone of the curve");
  RNA_def_property_update(prop, 0, "rna_CurveMapping_tone_update");

  prop = RNA_def_property(srna, "use_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CUMA_DO_CLIP);
  RNA_def_property_ui_text(prop, "Clip", "Force the curve view to fit a defined boundary");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_CurveMapping_clip_set");

  prop = RNA_def_property(srna, "clip_min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clipr.xmin");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Clip Min X", "");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_CurveMapping_clipminx_range");

  prop = RNA_def_property(srna, "clip_min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clipr.ymin");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Clip Min Y", "");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_CurveMapping_clipminy_range");

  prop = RNA_def_property(srna, "clip_max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clipr.xmax");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Clip Max X", "");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_CurveMapping_clipmaxx_range");

  prop = RNA_def_property(srna, "clip_max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clipr.ymax");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Clip Max Y", "");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_CurveMapping_clipmaxy_range");

  prop = RNA_def_property(srna, "extend", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, prop_extend_items);
  RNA_def_property_ui_text(prop, "Extend", "Extrapolate the curve or extend it horizontally");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE_LEGACY);
  RNA_def_property_update(prop, 0, "rna_CurveMapping_extend_update");

  prop = RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_CurveMapping_curves_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_CurveMapping_curves_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "CurveMap");
  RNA_def_property_ui_text(prop, "Curves", "");

  prop = RNA_def_property(srna, "black_level", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "black");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 1, 3);
  RNA_def_property_ui_text(
      prop, "Black Level", "For RGB curves, the color that black is mapped to");
  RNA_def_property_float_funcs(prop, nullptr, "rna_CurveMapping_black_level_set", nullptr);

  prop = RNA_def_property(srna, "white_level", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "white");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 1, 3);
  RNA_def_property_ui_text(
      prop, "White Level", "For RGB curves, the color that white is mapped to");
  RNA_def_property_float_funcs(prop, nullptr, "rna_CurveMapping_white_level_set", nullptr);

  func = RNA_def_function(srna, "update", "BKE_curvemapping_changed_all");
  RNA_def_function_ui_description(func, "Update curve mapping after making changes");

  func = RNA_def_function(srna, "reset_view", "BKE_curvemapping_reset_view");
  RNA_def_function_ui_description(func, "Reset the curve mapping grid to its clipping size");

  func = RNA_def_function(srna, "initialize", "rna_CurveMap_initialize");
  RNA_def_function_ui_description(func, "Initialize curve");

  func = RNA_def_function(srna, "evaluate", "rna_CurveMapping_evaluateF");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Evaluate curve at given location");
  parm = RNA_def_pointer(func, "curve", "CurveMap", "curve", "Curve to evaluate");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "position",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Position",
                       "Position to evaluate curve at",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "value",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Value",
                       "Value of curve at given location",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_function_return(func, parm);
}

static void rna_def_color_ramp_element(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ColorRampElement", nullptr);
  RNA_def_struct_sdna(srna, "CBData");
  RNA_def_struct_path_func(srna, "rna_ColorRampElement_path");
  RNA_def_struct_ui_text(
      srna, "Color Ramp Element", "Element defining a color at a position in the color ramp");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "r");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "Set color of selected color stop");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");

  prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "a");
  RNA_def_property_ui_text(prop, "Alpha", "Set alpha of selected color stop");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");

  prop = RNA_def_property(srna, "position", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "pos");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_range(prop, 0, 1, 1, 3);
  RNA_def_property_ui_text(prop, "Position", "Set position of selected color stop");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");
}

static void rna_def_color_ramp_element_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "ColorRampElements");
  srna = RNA_def_struct(brna, "ColorRampElements", nullptr);
  RNA_def_struct_sdna(srna, "ColorBand");
  RNA_def_struct_path_func(srna, "rna_ColorRampElement_path");
  RNA_def_struct_ui_text(srna, "Color Ramp Elements", "Collection of Color Ramp Elements");

  /* TODO: make these functions generic in `texture.cc`. */
  func = RNA_def_function(srna, "new", "rna_ColorRampElement_new");
  RNA_def_function_ui_description(func, "Add element to Color Ramp");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_float(
      func, "position", 0.0f, 0.0f, 1.0f, "Position", "Position to add element", 0.0f, 1.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "element", "ColorRampElement", "", "New element");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_ColorRampElement_remove");
  RNA_def_function_ui_description(func, "Delete element from Color Ramp");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "element", "ColorRampElement", "", "Element to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_color_ramp(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem prop_interpolation_items[] = {
      {COLBAND_INTERP_EASE, "EASE", 0, "Ease", ""},
      {COLBAND_INTERP_CARDINAL, "CARDINAL", 0, "Cardinal", ""},
      {COLBAND_INTERP_LINEAR, "LINEAR", 0, "Linear", ""},
      {COLBAND_INTERP_B_SPLINE, "B_SPLINE", 0, "B-Spline", ""},
      {COLBAND_INTERP_CONSTANT, "CONSTANT", 0, "Constant", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_mode_items[] = {
      {COLBAND_BLEND_RGB, "RGB", 0, "RGB", ""},
      {COLBAND_BLEND_HSV, "HSV", 0, "HSV", ""},
      {COLBAND_BLEND_HSL, "HSL", 0, "HSL", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_hsv_items[] = {
      {COLBAND_HUE_NEAR, "NEAR", 0, "Near", ""},
      {COLBAND_HUE_FAR, "FAR", 0, "Far", ""},
      {COLBAND_HUE_CW, "CW", 0, "Clockwise", ""},
      {COLBAND_HUE_CCW, "CCW", 0, "Counter-Clockwise", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ColorRamp", nullptr);
  RNA_def_struct_sdna(srna, "ColorBand");
  RNA_def_struct_path_func(srna, "rna_ColorRamp_path");
  RNA_def_struct_ui_text(srna, "Color Ramp", "Color ramp mapping a scalar value to a color");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_COLOR);
  RNA_def_property_collection_sdna(prop, nullptr, "data", "tot");
  RNA_def_property_struct_type(prop, "ColorRampElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");
  rna_def_color_ramp_element_api(brna, prop);

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "ipotype");
  RNA_def_property_enum_items(prop, prop_interpolation_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Set interpolation between color stops");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");

  prop = RNA_def_property(srna, "hue_interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "ipotype_hue");
  RNA_def_property_enum_items(prop, prop_hsv_items);
  RNA_def_property_ui_text(prop, "Color Interpolation", "Set color interpolation");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");

  prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "color_mode");
  RNA_def_property_enum_items(prop, prop_mode_items);
  RNA_def_property_ui_text(prop, "Color Mode", "Set color mode to use for interpolation");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");

#  if 0 /* use len(elements) */
  prop = RNA_def_property(srna, "total", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "tot");
  /* needs a function to do the right thing when adding elements like colorband_add_cb() */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_range(prop, 0, 31); /* MAXCOLORBAND = 32 */
  RNA_def_property_ui_text(prop, "Total", "Total number of elements");
  RNA_def_property_update(prop, 0, "rna_ColorRamp_update");
#  endif

  func = RNA_def_function(srna, "evaluate", "rna_ColorRamp_eval");
  RNA_def_function_ui_description(func, "Evaluate Color Ramp");
  parm = RNA_def_float(func,
                       "position",
                       1.0f,
                       0.0f,
                       1.0f,
                       "Position",
                       "Evaluate Color Ramp at position",
                       0.0f,
                       1.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return */
  parm = RNA_def_float_color(func,
                             "color",
                             4,
                             nullptr,
                             -FLT_MAX,
                             FLT_MAX,
                             "Color",
                             "Color at given position",
                             -FLT_MAX,
                             FLT_MAX);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

static void rna_def_histogram(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_mode_items[] = {
      {HISTO_MODE_LUMA, "LUMA", 0, "Luma", "Luma"},
      {HISTO_MODE_RGB, "RGB", 0, "RGB", "Red Green Blue"},
      {HISTO_MODE_R, "R", 0, "R", "Red"},
      {HISTO_MODE_G, "G", 0, "G", "Green"},
      {HISTO_MODE_B, "B", 0, "B", "Blue"},
      {HISTO_MODE_ALPHA, "A", 0, "A", "Alpha"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Histogram", nullptr);
  RNA_def_struct_ui_text(srna, "Histogram", "Statistical view of the levels of color in an image");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, prop_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Channels to display in the histogram");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);

  prop = RNA_def_property(srna, "show_line", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", HISTO_FLAG_LINE);
  RNA_def_property_ui_text(prop, "Show Line", "Display lines rather than filled shapes");
  RNA_def_property_ui_icon(prop, ICON_GRAPH, 0);
}

static void rna_def_scopes(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_wavefrm_mode_items[] = {
      {SCOPES_WAVEFRM_LUMA, "LUMA", ICON_COLOR, "Luma", ""},
      {SCOPES_WAVEFRM_RGB_PARADE, "PARADE", ICON_COLOR, "Parade", ""},
      {SCOPES_WAVEFRM_YCC_601, "YCBCR601", ICON_COLOR, "YCbCr (ITU 601)", ""},
      {SCOPES_WAVEFRM_YCC_709, "YCBCR709", ICON_COLOR, "YCbCr (ITU 709)", ""},
      {SCOPES_WAVEFRM_YCC_JPEG, "YCBCRJPG", ICON_COLOR, "YCbCr (JPEG)", ""},
      {SCOPES_WAVEFRM_RGB, "RGB", ICON_COLOR, "Red Green Blue", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_vecscope_mode_items[] = {
      {SCOPES_VECSCOPE_LUMA, "LUMA", ICON_COLOR, "Luma", ""},
      {SCOPES_VECSCOPE_RGB, "RGB", ICON_COLOR, "Red Green Blue", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Scopes", nullptr);
  RNA_def_struct_ui_text(srna, "Scopes", "Scopes for statistical view of an image");

  prop = RNA_def_property(srna, "use_full_resolution", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "Scopes", "sample_full", 1);
  RNA_def_property_ui_text(prop, "Full Sample", "Sample every pixel of the image");
  RNA_def_property_update(prop, 0, "rna_Scopes_update");

  prop = RNA_def_property(srna, "accuracy", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, "Scopes", "accuracy");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 10, 1);
  RNA_def_property_ui_text(
      prop, "Accuracy", "Proportion of original image source pixel lines to sample");
  RNA_def_property_update(prop, 0, "rna_Scopes_update");

  prop = RNA_def_property(srna, "histogram", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, "Scopes", "hist");
  RNA_def_property_struct_type(prop, "Histogram");
  RNA_def_property_ui_text(prop, "Histogram", "Histogram for viewing image statistics");

  prop = RNA_def_property(srna, "waveform_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, "Scopes", "wavefrm_mode");
  RNA_def_property_enum_items(prop, prop_wavefrm_mode_items);
  RNA_def_property_ui_text(prop, "Waveform Mode", "");
  RNA_def_property_update(prop, 0, "rna_Scopes_update");

  prop = RNA_def_property(srna, "waveform_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, "Scopes", "wavefrm_alpha");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Waveform Opacity", "Opacity of the points");

  prop = RNA_def_property(srna, "vectorscope_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, "Scopes", "vecscope_mode");
  RNA_def_property_enum_items(prop, prop_vecscope_mode_items);
  RNA_def_property_ui_text(prop, "Vectorscope Mode", "");
  RNA_def_property_update(prop, 0, "rna_Scopes_update");

  prop = RNA_def_property(srna, "vectorscope_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, "Scopes", "vecscope_alpha");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Vectorscope Opacity", "Opacity of the points");
}

static void rna_def_colormanage(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem display_device_items[] = {
      {0, "NONE", 0, "None", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem emulation_items[] = {
      {COLORMANAGE_DISPLAY_EMULATION_OFF,
       "OFF",
       0,
       "Off",
       "Directly output image as produced by OpenColorIO. This is not correct in general, but "
       "may be used when the system configuration and actual display device is known to match "
       "the chosen display"},
      {COLORMANAGE_DISPLAY_EMULATION_AUTO,
       "AUTO",
       0,
       "Automatic",
       "Display images consistent with most other applications, to preview images and video for "
       "export. A best effort is made to emulate the chosen display on the actual display "
       "device."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem look_items[] = {
      {0, "NONE", 0, "None", "Do not modify image in an artistic manner"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem view_transform_items[] = {
      {0,
       "NONE",
       0,
       "None",
       "Do not perform any color transform on display, use old non-color managed technique for "
       "display"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* ** Display Settings ** */
  srna = RNA_def_struct(brna, "ColorManagedDisplaySettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_ColorManagedDisplaySettings_path");
  RNA_def_struct_ui_text(
      srna, "ColorManagedDisplaySettings", "Color management specific to display device");

  prop = RNA_def_property(srna, "display_device", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, display_device_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ColorManagedDisplaySettings_display_device_get",
                              "rna_ColorManagedDisplaySettings_display_device_set",
                              "rna_ColorManagedDisplaySettings_display_device_itemf");
  RNA_def_property_ui_text(
      prop,
      "Display",
      "Display name. For viewing, this is the display device that will be emulated by limiting "
      "the gamut and HDR colors. For image and video output, this is the display space used for "
      "writing.");
  RNA_def_property_update(
      prop, NC_WINDOW, "rna_ColorManagedDisplaySettings_display_device_update");

  prop = RNA_def_property(srna, "emulation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, emulation_items);
  RNA_def_property_ui_text(
      prop,
      "Display Emulation",
      "Control how images in the chosen display are mapped to the physical display");
  RNA_def_property_update(
      prop, NC_WINDOW, "rna_ColorManagedDisplaySettings_display_device_update");

  /* ** View Settings ** */
  srna = RNA_def_struct(brna, "ColorManagedViewSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_ColorManagedViewSettings_path");
  RNA_def_struct_ui_text(srna,
                         "ColorManagedViewSettings",
                         "Color management settings used for displaying images on the display");

  prop = RNA_def_property(srna, "look", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, look_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ColorManagedViewSettings_look_get",
                              "rna_ColorManagedViewSettings_look_set",
                              "rna_ColorManagedViewSettings_look_itemf");
  RNA_def_property_ui_text(
      prop, "Look", "Additional transform applied before view transform for artistic needs");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");

  prop = RNA_def_property(srna, "view_transform", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, view_transform_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ColorManagedViewSettings_view_transform_get",
                              "rna_ColorManagedViewSettings_view_transform_set",
                              "rna_ColorManagedViewSettings_view_transform_itemf");
  RNA_def_property_ui_text(prop, "View", "View used when converting image to a display space");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");

  prop = RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "exposure");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -32.0f, 32.0f);
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Exposure",
      "Exposure (stops) applied before display transform, multiplying by 2^exposure");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "gamma");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 5.0f);
  RNA_def_property_ui_text(
      prop,
      "Gamma",
      "Additional gamma encoding after display transform, for output with custom gamma");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "curve_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_mapping");
  RNA_def_property_ui_text(prop, "Curve", "Color curve mapping applied before display transform");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");

  prop = RNA_def_property(srna, "use_curve_mapping", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", COLORMANAGE_VIEW_USE_CURVES);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_ColorManagedViewSettings_use_curves_set");
  RNA_def_property_ui_text(prop, "Use Curves", "Use RGB curved for pre-display transformation");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "use_white_balance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", COLORMANAGE_VIEW_USE_WHITE_BALANCE);
  RNA_def_property_ui_text(
      prop, "Use White Balance", "Perform chromatic adaption from a different white point");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "white_balance_temperature", PROP_FLOAT, PROP_COLOR_TEMPERATURE);
  RNA_def_property_float_sdna(prop, nullptr, "temperature");
  RNA_def_property_float_default(prop, 6500.0f);
  RNA_def_property_range(prop, 1800.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 2000.0f, 11000.0f, 100, 0);
  RNA_def_property_ui_text(prop, "Temperature", "Color temperature of the scene's white point");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "white_balance_tint", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "tint");
  RNA_def_property_float_default(prop, 10.0f);
  RNA_def_property_range(prop, -500.0f, 500.0f);
  RNA_def_property_ui_range(prop, -150.0f, 150.0f, 1, 1);
  RNA_def_property_ui_text(
      prop, "Tint", "Color tint of the scene's white point (the default of 10 matches daylight)");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "white_balance_whitepoint", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_ColorManagedViewSettings_whitepoint_get",
                               "rna_ColorManagedViewSettings_whitepoint_set",
                               nullptr);
  RNA_def_property_ui_text(prop,
                           "White Point",
                           "The color which gets mapped to white "
                           "(automatically converted to/from temperature and tint)");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagement_update");
  RNA_def_property_editable_func(prop, "rna_ViewSettings_only_view_look_editable");

  prop = RNA_def_property(srna, "is_hdr", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is HDR", "The display and view transform supports high dynamic range colors");
  RNA_def_property_boolean_funcs(prop, "rna_ColorManagedViewSettings_is_hdr_get", nullptr);

  prop = RNA_def_property(srna, "support_emulation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Support Emulation",
      "The display and view transform supports automatic emulation for another display device, "
      "using the display color spaces mechanism in OpenColorIO v2 configurations");
  RNA_def_property_boolean_funcs(
      prop, "rna_ColorManagedViewSettings_support_emulation_get", nullptr);

  /* ** Color-space ** */
  srna = RNA_def_struct(brna, "ColorManagedInputColorspaceSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_ColorManagedInputColorspaceSettings_path");
  RNA_def_struct_ui_text(
      srna, "ColorManagedInputColorspaceSettings", "Input color space settings");

  prop = RNA_def_property(srna, "name", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, rna_enum_color_space_convert_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ColorManagedColorspaceSettings_colorspace_get",
                              "rna_ColorManagedColorspaceSettings_colorspace_set",
                              "rna_ColorManagedColorspaceSettings_colorspace_itemf");
  RNA_def_property_ui_text(
      prop,
      "Input Color Space",
      "Color space in the image file, to convert to and from when saving and loading the image");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagedColorspaceSettings_reload_update");

  prop = RNA_def_property(srna, "is_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_ColorManagedColorspaceSettings_is_data_get",
                                 "rna_ColorManagedColorspaceSettings_is_data_set");
  RNA_def_property_ui_text(
      prop,
      "Is Data",
      "Treat image as non-color data without color management, like normal or displacement maps");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagedColorspaceSettings_reload_update");

  //
  srna = RNA_def_struct(brna, "ColorManagedSequencerColorspaceSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_ColorManagedSequencerColorspaceSettings_path");
  RNA_def_struct_ui_text(
      srna, "ColorManagedSequencerColorspaceSettings", "Input color space settings");

  prop = RNA_def_property(srna, "name", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_convert_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ColorManagedColorspaceSettings_colorspace_get",
                              "rna_ColorManagedColorspaceSettings_colorspace_set",
                              "rna_ColorManagedColorspaceSettings_colorspace_itemf");
  RNA_def_property_ui_text(prop, "Color Space", "Color space that the sequencer operates in");
  RNA_def_property_update(prop, NC_WINDOW, "rna_ColorManagedColorspaceSettings_reload_update");
}

void RNA_def_color(BlenderRNA *brna)
{
  rna_def_curvemappoint(brna);
  rna_def_curvemap(brna);
  rna_def_curvemapping(brna);
  rna_def_color_ramp_element(brna);
  rna_def_color_ramp(brna);
  rna_def_histogram(brna);
  rna_def_scopes(brna);
  rna_def_colormanage(brna);
}

#endif
