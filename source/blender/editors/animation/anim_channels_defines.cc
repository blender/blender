/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cstdio>

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_keyframing.hh"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_userdef_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_nla.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"

#include "ANIM_fcurve.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "anim_intern.hh"

using namespace blender;

/* *********************************************** */
/* XXX constant defines to be moved elsewhere? */

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD 100.0f

/* size of indent steps */
#define INDENT_STEP_SIZE (0.35f * U.widget_unit)

/* get the pointer used for some flag */
#define GET_ACF_FLAG_PTR(ptr, type) ((*(type) = sizeof(ptr)), &(ptr))

/* *********************************************** */
/* Generic Functions (Type independent) */

/* Draw Backdrop ---------------------------------- */

/* get backdrop color for top-level widgets (Scene, Object and ActionSlot only) */
static void acf_generic_root_color(bAnimContext * /*ac*/,
                                   bAnimListElem * /*ale*/,
                                   float r_color[3])
{
  /* darker blue for top-level widgets */
  UI_GetThemeColor3fv(TH_DOPESHEET_CHANNELOB, r_color);
}

/* backdrop for top-level widgets (Scene and Object only) */
static void acf_generic_root_backdrop(bAnimContext *ac,
                                      bAnimListElem *ale,
                                      float yminc,
                                      float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short expanded = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[3];

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  /* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
  UI_draw_roundbox_corner_set((expanded) ? UI_CNR_TOP_LEFT :
                                           (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT));

  rctf box;
  box.xmin = offset;
  box.xmax = v2d->cur.xmax + EXTRA_SCROLL_PAD;
  box.ymin = yminc;
  box.ymax = ymaxc;
  UI_draw_roundbox_3fv_alpha(&box, true, 8, color, 1.0f);
}

/* get backdrop color for data expanders under top-level Scene/Object */
static void acf_generic_dataexpand_color(bAnimContext * /*ac*/,
                                         bAnimListElem * /*ale*/,
                                         float r_color[3])
{
  /* lighter color than top-level widget */
  UI_GetThemeColor3fv(TH_DOPESHEET_CHANNELSUBOB, r_color);
}

/* backdrop for data expanders under top-level Scene/Object */
static void acf_generic_dataexpand_backdrop(bAnimContext *ac,
                                            bAnimListElem *ale,
                                            float yminc,
                                            float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[3];

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor3fv(color);

  /* no rounded corner - just rectangular box */
  immRectf(pos, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc);

  immUnbindProgram();
}

/* helper method to test if group colors should be drawn */
static bool acf_show_channel_colors()
{
  return (U.animation_flag & USER_ANIM_SHOW_CHANNEL_GROUP_COLORS) != 0;
}

/* get backdrop color for generic channels */
static void acf_generic_channel_color(bAnimContext *ac, bAnimListElem *ale, float r_color[3])
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  short indent = (acf->get_indent_level) ? acf->get_indent_level(ac, ale) : 0;

  /* FIXME: what happens when the indentation is 1 greater than what it should be
   * (due to grouping)? */
  const int colorOffset = 10 - 10 * indent;
  UI_GetThemeColorShade3fv(TH_CHANNEL, colorOffset, r_color);
}

/* backdrop for generic channels */
static void acf_generic_channel_backdrop(bAnimContext *ac,
                                         bAnimListElem *ale,
                                         float yminc,
                                         float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[3];

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor3fv(color);

  /* no rounded corners - just rectangular box */
  immRectf(pos, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc);

  immUnbindProgram();
}

/* Indentation + Offset ------------------------------------------- */

/* indentation level is always the value in the name */
static short acf_generic_indentation_0(bAnimContext * /*ac*/, bAnimListElem * /*ale*/)
{
  return 0;
}
static short acf_generic_indentation_1(bAnimContext * /*ac*/, bAnimListElem * /*ale*/)
{
  return 1;
}
#if 0 /* XXX not used */
static short acf_generic_indentation_2(bAnimContext *ac, bAnimListElem *ale)
{
  return 2;
}
#endif

/* indentation which varies with the grouping status */
static short acf_generic_indentation_flexible(bAnimContext * /*ac*/, bAnimListElem *ale)
{
  if (ale->type != ANIMTYPE_FCURVE) {
    return 0;
  }

  /* Grouped F-Curves need extra level of indentation. */
  const FCurve *fcu = static_cast<const FCurve *>(ale->data);
  if (fcu->grp) {
    return 1;
  }

  return 0;
}

/* basic offset for channels derived from indentation */
static short acf_generic_basic_offset(bAnimContext *ac, bAnimListElem *ale)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

  if (acf && acf->get_indent_level) {
    return acf->get_indent_level(ac, ale) * INDENT_STEP_SIZE;
  }
  return 0;
}

/* offset based on nodetree type */
static short acf_nodetree_rootType_offset(bNodeTree *ntree)
{
  if (ntree) {
    switch (ntree->type) {
      case NTREE_SHADER:
        /* 1 additional level (i.e. is indented one level in from material,
         * so shift all right by one step)
         */
        return INDENT_STEP_SIZE;

      case NTREE_COMPOSIT:
        /* no additional levels needed */
        return 0;

      case NTREE_TEXTURE:
        /* 2 additional levels */
        return INDENT_STEP_SIZE * 2;
    }
  }

  /* unknown */
  return 0;
}

/* offset for groups + grouped entities */
static short acf_generic_group_offset(bAnimContext *ac, bAnimListElem *ale)
{
  short offset = acf_generic_basic_offset(ac, ale);

  if (ale->id) {

    /* Action Editor. */
    if (ac->datatype == ANIMCONT_ACTION) {
      /* For Action Editor mode, we have a limited set of channel types we need
       * to account for, so we can handle them very simply here in one place. */
      switch (ale->type) {
        case ANIMTYPE_FCURVE:
        case ANIMTYPE_GROUP: {
          const bAction *action = reinterpret_cast<bAction *>(ale->fcurve_owner_id);
          if (action->wrap().is_action_layered()) {
            offset += short(0.35f * U.widget_unit);
          }
          break;
        }

        case ANIMTYPE_SUMMARY:
        case ANIMTYPE_ACTION_SLOT:
          break;

        /* There should be no types except the above in Action Editor mode. */
        default:
          BLI_assert_unreachable();
          break;
      }

      return offset;
    }

    /* Other editors. */

    /* texture animdata */
    if (GS(ale->id->name) == ID_TE) {
      offset += U.widget_unit;
    }
    /* materials and particles animdata */
    else if (ELEM(GS(ale->id->name), ID_MA, ID_PA)) {
      offset += short(0.7f * U.widget_unit);
    }
    /* If not in Action Editor mode, action-groups (and their children)
     * must carry some offset too. */
    else {
      offset += short(0.7f * U.widget_unit);
    }

    /* nodetree animdata */
    if (GS(ale->id->name) == ID_NT) {
      offset += acf_nodetree_rootType_offset(reinterpret_cast<bNodeTree *>(ale->id));
    }
  }

  /* offset is just the normal type - i.e. based on indentation */
  return offset;
}

/* Name ------------------------------------------- */

/* name for ID block entries */
static void acf_generic_idblock_name(bAnimListElem *ale, char *name)
{
  ID *id = static_cast<ID *>(ale->data); /* data pointed to should be an ID block */

  /* just copy the name... */
  if (id && name) {
    BLI_strncpy_utf8(name, id->name + 2, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for ID block entries */
static bool acf_generic_idblock_name_prop(bAnimListElem *ale,
                                          PointerRNA *r_ptr,
                                          PropertyRNA **r_prop)
{
  *r_ptr = RNA_id_pointer_create(static_cast<ID *>(ale->data));
  *r_prop = RNA_struct_name_property(r_ptr->type);

  return (*r_prop != nullptr);
}

/* name property for ID block entries which are just subheading "fillers" */
static bool acf_generic_idfill_name_prop(bAnimListElem *ale,
                                         PointerRNA *r_ptr,
                                         PropertyRNA **r_prop)
{
  /* actual ID we're representing is stored in ale->data not ale->id, as id gives the owner */
  *r_ptr = RNA_id_pointer_create(static_cast<ID *>(ale->data));
  *r_prop = RNA_struct_name_property(r_ptr->type);

  return (*r_prop != nullptr);
}

/* Settings ------------------------------------------- */

/* check if some setting exists for this object-based data-expander (datablock only) */
static bool acf_generic_dataexpand_setting_valid(bAnimContext *ac,
                                                 bAnimListElem * /*ale*/,
                                                 eAnimChannel_Settings setting)
{
  switch (setting) {
    /* expand is always supported */
    case ACHANNEL_SETTING_EXPAND:
      return true;

    /* mute is only supported for NLA */
    case ACHANNEL_SETTING_MUTE:
      return ((ac) && (ac->spacetype == SPACE_NLA));

    /* Select is ok for most `ds*` channels (e.g. `dsmat`) */
    case ACHANNEL_SETTING_SELECT:
      return true;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return true;

    /* other flags are never supported */
    default:
      return false;
  }
}

/* *********************************************** */
/* Type Specific Functions + Defines */

/* Animation Summary ----------------------------------- */

/* get backdrop color for summary widget */
static void acf_summary_color(bAnimContext *ac, bAnimListElem *ale, float r_color[3])
{
  /* Only draw the summary line backdrop when it is expanded. If the entire dope sheet is
   * just one line, there is no need for any distinction between lines, and the red-ish
   * color is only going to be a distraction. */
  const bool is_expanded = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND);
  UI_GetThemeColor3fv(is_expanded ? TH_ANIM_ACTIVE : TH_HEADER, r_color);
}

/* backdrop for summary widget */
static void acf_summary_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  float color[3];

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  /* rounded corners on LHS only
   * - top and bottom
   * - special hack: make the top a bit higher, since we are first...
   */
  UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);

  rctf box;
  box.xmin = 0;
  box.xmax = v2d->cur.xmax + EXTRA_SCROLL_PAD;
  box.ymin = yminc - 2;
  box.ymax = ymaxc;
  UI_draw_roundbox_3fv_alpha(&box, true, 8, color, 1.0f);
}

/* name for summary entries */
static void acf_summary_name(bAnimListElem * /*ale*/, char *name)
{
  if (name) {
    BLI_strncpy_utf8(name, IFACE_("Summary"), ANIM_CHAN_NAME_SIZE);
  }
}

/* check if some setting exists for this channel */
static bool acf_summary_setting_valid(bAnimContext * /*ac*/,
                                      bAnimListElem * /*ale*/,
                                      eAnimChannel_Settings setting)
{
  /* only expanded is supported, as it is used for hiding all stuff which the summary covers */
  return (setting == ACHANNEL_SETTING_EXPAND);
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_summary_setting_flag(bAnimContext * /*ac*/,
                                    eAnimChannel_Settings setting,
                                    bool *r_neg)
{
  if (setting == ACHANNEL_SETTING_EXPAND) {
    /* expanded */
    *r_neg = true;
    return ADS_FLAG_SUMMARY_COLLAPSED;
  }

  /* unsupported */
  *r_neg = false;
  return 0;
}

/* get pointer to the setting */
static void *acf_summary_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *r_type)
{
  bAnimContext *ac = static_cast<bAnimContext *>(ale->data);

  /* If data is valid, return pointer to active dope-sheet's relevant flag
   * - this is restricted to DopeSheet/Action Editor only. */
  if ((ac->sl) && (ac->spacetype == SPACE_ACTION) && (setting == ACHANNEL_SETTING_EXPAND)) {
    SpaceAction *saction = reinterpret_cast<SpaceAction *>(ac->sl);
    bDopeSheet *ads = &saction->ads;

    /* return pointer to DopeSheet's flag */
    return GET_ACF_FLAG_PTR(ads->flag, r_type);
  }

  /* can't return anything useful - unsupported */
  *r_type = 0;
  return nullptr;
}

/** All animation summary (dope-sheet only) type define. */
static bAnimChannelType ACF_SUMMARY = {
    /*channel_type_name*/ "Summary",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_summary_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_summary_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ nullptr,

    /*name*/ acf_summary_name,
    /*name_prop*/ nullptr,
    /*icon*/ nullptr,

    /*has_setting*/ acf_summary_setting_valid,
    /*setting_flag*/ acf_summary_setting_flag,
    /*setting_ptr*/ acf_summary_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Scene ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_scene_icon(bAnimListElem * /*ale*/)
{
  return ICON_SCENE_DATA;
}

/* check if some setting exists for this channel */
static bool acf_scene_setting_valid(bAnimContext *ac,
                                    bAnimListElem * /*ale*/,
                                    eAnimChannel_Settings setting)
{
  switch (setting) {
    /* muted only in NLA */
    case ACHANNEL_SETTING_MUTE:
      return ((ac) && (ac->spacetype == SPACE_NLA));

    /* visible only in Graph Editor */
    case ACHANNEL_SETTING_VISIBLE:
      return ((ac) && (ac->spacetype == SPACE_GRAPH));

    /* only select and expand supported otherwise */
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return false;

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_scene_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return SCE_DS_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *r_neg = true;
      return SCE_DS_COLLAPSED;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_scene_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Scene *scene = static_cast<Scene *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return GET_ACF_FLAG_PTR(scene->flag, r_type);

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(scene->flag, r_type);

    case ACHANNEL_SETTING_MUTE:    /* mute (only in NLA) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (scene->adt) {
        return GET_ACF_FLAG_PTR(scene->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Scene type define. */
static bAnimChannelType ACF_SCENE = {
    /*channel_type_name*/ "Scene",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_root_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_root_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ nullptr,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_scene_icon,

    /*has_setting*/ acf_scene_setting_valid,
    /*setting_flag*/ acf_scene_setting_flag,
    /*setting_ptr*/ acf_scene_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Object ------------------------------------------- */

static int acf_object_icon(bAnimListElem *ale)
{
  Base *base = static_cast<Base *>(ale->data);
  Object *ob = base->object;

  /* icon depends on object-type */
  switch (ob->type) {
    case OB_LAMP:
      return ICON_OUTLINER_OB_LIGHT;
    case OB_MESH:
      return ICON_OUTLINER_OB_MESH;
    case OB_CAMERA:
      return ICON_OUTLINER_OB_CAMERA;
    case OB_CURVES_LEGACY:
      return ICON_OUTLINER_OB_CURVE;
    case OB_MBALL:
      return ICON_OUTLINER_OB_META;
    case OB_LATTICE:
      return ICON_OUTLINER_OB_LATTICE;
    case OB_SPEAKER:
      return ICON_OUTLINER_OB_SPEAKER;
    case OB_LIGHTPROBE:
      return ICON_OUTLINER_OB_LIGHTPROBE;
    case OB_ARMATURE:
      return ICON_OUTLINER_OB_ARMATURE;
    case OB_FONT:
      return ICON_OUTLINER_OB_FONT;
    case OB_SURF:
      return ICON_OUTLINER_OB_SURFACE;
    case OB_CURVES:
      return ICON_OUTLINER_OB_CURVES;
    case OB_POINTCLOUD:
      return ICON_OUTLINER_OB_POINTCLOUD;
    case OB_VOLUME:
      return ICON_OUTLINER_OB_VOLUME;
    case OB_EMPTY:
      return ICON_OUTLINER_OB_EMPTY;
    case OB_GREASE_PENCIL:
      return ICON_OUTLINER_OB_GREASEPENCIL;
    default:
      return ICON_OBJECT_DATA;
  }
}

/* name for object */
static void acf_object_name(bAnimListElem *ale, char *name)
{
  Base *base = static_cast<Base *>(ale->data);
  Object *ob = base->object;

  /* just copy the name... */
  if (ob && name) {
    BLI_strncpy_utf8(name, ob->id.name + 2, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for object */
static bool acf_object_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  *r_ptr = RNA_id_pointer_create(ale->id);
  *r_prop = RNA_struct_name_property(r_ptr->type);

  return (*r_prop != nullptr);
}

/* check if some setting exists for this channel */
static bool acf_object_setting_valid(bAnimContext *ac,
                                     bAnimListElem *ale,
                                     eAnimChannel_Settings setting)
{
  Base *base = static_cast<Base *>(ale->data);
  Object *ob = base->object;

  switch (setting) {
    /* muted only in NLA */
    case ACHANNEL_SETTING_MUTE:
      return ((ac) && (ac->spacetype == SPACE_NLA));

    /* visible only in Graph Editor */
    case ACHANNEL_SETTING_VISIBLE:
      return ((ac) && (ac->spacetype == SPACE_GRAPH) && (ob->adt));

    /* only select and expand supported otherwise */
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return ((ac) && (ac->spacetype == SPACE_GRAPH) && (ob->adt));

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_object_setting_flag(bAnimContext * /*ac*/,
                                   eAnimChannel_Settings setting,
                                   bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return BASE_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *r_neg = true;
      return OB_ADS_COLLAPSED;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_object_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings setting,
                                    short *r_type)
{
  Base *base = static_cast<Base *>(ale->data);
  Object *ob = base->object;

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return GET_ACF_FLAG_PTR(base->flag, r_type);

    case ACHANNEL_SETTING_EXPAND:                   /* expanded */
      return GET_ACF_FLAG_PTR(ob->nlaflag, r_type); /* XXX */

    case ACHANNEL_SETTING_MUTE:           /* mute (only in NLA) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (ob->adt) {
        return GET_ACF_FLAG_PTR(ob->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Object type define. */
static bAnimChannelType ACF_OBJECT = {
    /*channel_type_name*/ "Object",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_root_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_root_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ nullptr,

    /*name*/ acf_object_name,
    /*name_prop*/ acf_object_name_prop,
    /*icon*/ acf_object_icon,

    /*has_setting*/ acf_object_setting_valid,
    /*setting_flag*/ acf_object_setting_flag,
    /*setting_ptr*/ acf_object_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Group ------------------------------------------- */

/* get backdrop color for group widget */
static void acf_group_color(bAnimContext * /*ac*/, bAnimListElem *ale, float r_color[3])
{
  if (ale->flag & AGRP_ACTIVE) {
    UI_GetThemeColor3fv(TH_GROUP_ACTIVE, r_color);
  }
  else {
    UI_GetThemeColor3fv(TH_GROUP, r_color);
  }
}

/* backdrop for group widget */
static void acf_group_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short expanded = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[3];

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  /* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
  UI_draw_roundbox_corner_set(expanded ? UI_CNR_TOP_LEFT : (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT));

  rctf box;
  box.xmin = offset;
  box.xmax = v2d->cur.xmax + EXTRA_SCROLL_PAD;
  box.ymin = yminc;
  box.ymax = ymaxc;
  UI_draw_roundbox_3fv_alpha(&box, true, 8, color, 1.0f);
}

/* name for group entries */
static void acf_group_name(bAnimListElem *ale, char *name)
{
  bActionGroup *agrp = static_cast<bActionGroup *>(ale->data);

  /* just copy the name... */
  if (agrp && name) {
    BLI_strncpy_utf8(name, agrp->name, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for group entries */
static bool acf_group_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  *r_ptr = RNA_pointer_create_discrete(ale->fcurve_owner_id, &RNA_ActionGroup, ale->data);
  *r_prop = RNA_struct_name_property(r_ptr->type);

  return (*r_prop != nullptr);
}

/* check if some setting exists for this channel */
static bool acf_group_setting_valid(bAnimContext *ac,
                                    bAnimListElem * /*ale*/,
                                    eAnimChannel_Settings setting)
{
  /* for now, all settings are supported, though some are only conditionally */
  switch (setting) {
    /* unsupported */
    case ACHANNEL_SETTING_SOLO:   /* Only available in NLA Editor for tracks */
    case ACHANNEL_SETTING_PINNED: /* Only for NLA actions */
      return false;

    /* conditionally supported */
    case ACHANNEL_SETTING_VISIBLE: /* Only available in Graph Editor */
      return (ac->spacetype == SPACE_GRAPH);

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH);

    default: /* always supported */
      return true;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_group_setting_flag(bAnimContext *ac, eAnimChannel_Settings setting, bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return AGRP_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
    {
      /* NOTE: Graph Editor uses a different flag to everywhere else for this,
       * allowing different collapsing of groups there, since sharing the flag
       * proved to be a hazard for workflows...
       */
      return (ac->spacetype == SPACE_GRAPH) ? AGRP_EXPANDED_G : /* Graph Editor case */
                                              AGRP_EXPANDED;                                 /* DopeSheet and elsewhere */
    }

    case ACHANNEL_SETTING_MUTE: /* muted */
      return AGRP_MUTED;

    case ACHANNEL_SETTING_MOD_OFF: /* muted */
      *r_neg = true;
      return AGRP_MODIFIERS_OFF;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return AGRP_PROTECTED;

    case ACHANNEL_SETTING_VISIBLE: /* visibility - graph editor */
      *r_neg = true;
      return AGRP_NOTVISIBLE;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default:
      /* this shouldn't happen */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_group_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings /*setting*/,
                                   short *r_type)
{
  bActionGroup *agrp = static_cast<bActionGroup *>(ale->data);

  /* all flags are just in agrp->flag for now... */
  return GET_ACF_FLAG_PTR(agrp->flag, r_type);
}

static bool get_actiongroup_color(const bActionGroup *agrp, uint8_t r_color[3])
{
  if (!agrp) {
    return false;
  }

  const int8_t color_index = agrp->customCol;
  if (color_index == 0) {
    return false;
  }

  const ThemeWireColor *wire_color;
  if (color_index < 0) {
    wire_color = &agrp->cs;
  }
  else {
    const bTheme *btheme = UI_GetTheme();
    wire_color = &btheme->tarm[(color_index - 1)];
  }

  r_color[0] = wire_color->solid[0];
  r_color[1] = wire_color->solid[1];
  r_color[2] = wire_color->solid[2];
  return true;
}

static bool acf_group_channel_color(const bAnimListElem *ale, uint8_t r_color[3])
{
  const bActionGroup *agrp = static_cast<const bActionGroup *>(ale->data);
  return get_actiongroup_color(agrp, r_color);
}

/** Group type define. */
static bAnimChannelType ACF_GROUP = {
    /*channel_type_name*/ "Group",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_group_color,
    /*get_channel_color*/ acf_group_channel_color,
    /*draw_backdrop*/ acf_group_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_group_name,
    /*name_prop*/ acf_group_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ acf_group_setting_valid,
    /*setting_flag*/ acf_group_setting_flag,
    /*setting_ptr*/ acf_group_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* F-Curve ------------------------------------------- */

/* name for fcurve entries */
static void acf_fcurve_name(bAnimListElem *ale, char *name)
{
  using namespace blender::animrig;

  FCurve *fcurve = static_cast<FCurve *>(ale->data);

  /* Clear the error flag. It'll be set again when an error situation is detected. */
  fcurve->flag &= ~FCURVE_DISABLED;

  if (ale->fcurve_owner_id && GS(ale->fcurve_owner_id->name) == ID_AC &&
      ale->slot_handle != Slot::unassigned)
  {
    /* F-Curve comes from a layered Action. This means that we cannot trust `ale->id` or
     * `ale->adt`, because in the Action Editor those are set to whatever object has this Action
     * assigned. This F-Curve may be for a different slot, though, and thus might be animating a
     * entirely different ID type. */
    const Action &action = reinterpret_cast<bAction *>(ale->fcurve_owner_id)->wrap();
    const Slot *slot = action.slot_for_handle(ale->slot_handle);
    if (!slot) {
      /* Defer to the default F-Curve resolution, but without the animated ID
       * pointer, as it's likely to be wrong anyway. */
      getname_anim_fcurve(name, nullptr, fcurve);
      return;
    }

    /* If the animated ID this ALE is for is a user of the slot, try to resolve the path on that.
     * If this fails, mark the F-Curve as problematic. This code is here so that
     * getname_anim_fcurve_for_slot() can do its best to find a label of the animated property,
     * independently of the "error line" shown in the dope sheet, at the cost of one extra call to
     * RNA_path_resolve_property(). See #129490. */
    if (slot->users(*ale->bmain).contains(ale->id)) {
      PointerRNA id_ptr = RNA_id_pointer_create(ale->id);
      PointerRNA ptr;
      PropertyRNA *prop;
      if (!RNA_path_resolve_property(&id_ptr, fcurve->rna_path, &ptr, &prop)) {
        fcurve->flag |= FCURVE_DISABLED;
      }
    }

    BLI_assert(ale->bmain);
    const std::string fcurve_name = getname_anim_fcurve_for_slot(*ale->bmain, *slot, *fcurve);
    const size_t num_chars_copied = fcurve_name.copy(name, std::string::npos);
    name[num_chars_copied] = '\0';

    return;
  }

  getname_anim_fcurve(name, ale->id, fcurve);
}

/* "name" property for fcurve entries */
static bool acf_fcurve_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  FCurve *fcu = static_cast<FCurve *>(ale->data);

  /* Ctrl-Click Usability Convenience Hack:
   * For disabled F-Curves, allow access to the RNA Path
   * as our "name" so that user can perform quick fixes
   */
  if (fcu->flag & FCURVE_DISABLED) {
    *r_ptr = RNA_pointer_create_discrete(ale->fcurve_owner_id, &RNA_FCurve, ale->data);
    *r_prop = RNA_struct_find_property(r_ptr, "data_path");
  }
  else {
    /* for "normal" F-Curves - no editable name, but *prop may not be set properly yet... */
    *r_prop = nullptr;
  }

  return (*r_prop != nullptr);
}

/* check if some setting exists for this channel */
static bool acf_fcurve_setting_valid(bAnimContext *ac,
                                     bAnimListElem * /*ale*/,
                                     eAnimChannel_Settings setting)
{
  switch (setting) {
    /* unsupported */
    case ACHANNEL_SETTING_SOLO:   /* Solo Flag is only for NLA */
    case ACHANNEL_SETTING_EXPAND: /* F-Curves are not containers */
    case ACHANNEL_SETTING_PINNED: /* This is only for NLA Actions */
      return false;

    case ACHANNEL_SETTING_VISIBLE: /* Only available in Graph Editor */
      return (ac->spacetype == SPACE_GRAPH);

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return false;

    /* always available */
    default:
      return true;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_fcurve_setting_flag(bAnimContext * /*ac*/,
                                   eAnimChannel_Settings setting,
                                   bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return FCURVE_SELECTED;

    case ACHANNEL_SETTING_MUTE: /* muted */
      return FCURVE_MUTED;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return FCURVE_PROTECTED;

    case ACHANNEL_SETTING_VISIBLE: /* visibility - graph editor */
      return FCURVE_VISIBLE;

    case ACHANNEL_SETTING_MOD_OFF:
      *r_neg = true;
      return FCURVE_MOD_OFF;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_fcurve_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings /*setting*/,
                                    short *r_type)
{
  FCurve *fcu = static_cast<FCurve *>(ale->data);

  /* all flags are just in agrp->flag for now... */
  return GET_ACF_FLAG_PTR(fcu->flag, r_type);
}

static bool acf_fcurve_channel_color(const bAnimListElem *ale, uint8_t r_color[3])
{
  const FCurve *fcu = static_cast<const FCurve *>(ale->data);
  return get_actiongroup_color(fcu->grp, r_color);
}

/** F-Curve type define. */
static bAnimChannelType ACF_FCURVE = {
    /*channel_type_name*/ "F-Curve",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_generic_channel_color,
    /*get_channel_color*/ acf_fcurve_channel_color,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_flexible,
    /* XXX rename this to f-curves only? */
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_fcurve_name,
    /*name_prop*/ acf_fcurve_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ acf_fcurve_setting_valid,
    /*setting_flag*/ acf_fcurve_setting_flag,
    /*setting_ptr*/ acf_fcurve_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* NLA Control FCurves Expander ----------------------- */

/* get backdrop color for nla controls widget */
static void acf_nla_controls_color(bAnimContext * /*ac*/,
                                   bAnimListElem * /*ale*/,
                                   float r_color[3])
{
  /* TODO: give this its own theme setting? */
  UI_GetThemeColorShade3fv(TH_GROUP, 55, r_color);
}

/* backdrop for nla controls expander widget */
static void acf_nla_controls_backdrop(bAnimContext *ac,
                                      bAnimListElem *ale,
                                      float yminc,
                                      float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short expanded = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_EXPAND) != 0;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[3];

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  /* rounded corners on LHS only - top only when expanded, but bottom too when collapsed */
  UI_draw_roundbox_corner_set(expanded ? UI_CNR_TOP_LEFT : (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT));

  rctf box;
  box.xmin = offset;
  box.xmax = v2d->cur.xmax + EXTRA_SCROLL_PAD;
  box.ymin = yminc;
  box.ymax = ymaxc;
  UI_draw_roundbox_3fv_alpha(&box, true, 5, color, 1.0f);
}

/* name for nla controls expander entries */
static void acf_nla_controls_name(bAnimListElem * /*ale*/, char *name)
{
  BLI_strncpy_utf8(name, IFACE_("NLA Strip Controls"), ANIM_CHAN_NAME_SIZE);
}

/* check if some setting exists for this track */
static bool acf_nla_controls_setting_valid(bAnimContext * /*ac*/,
                                           bAnimListElem * /*ale*/,
                                           eAnimChannel_Settings setting)
{
  /* for now, all settings are supported, though some are only conditionally */
  switch (setting) {
    /* supported */
    case ACHANNEL_SETTING_EXPAND:
      return true;

      /* TODO: selected? */

    default: /* unsupported */
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_nla_controls_setting_flag(bAnimContext * /*ac*/,
                                         eAnimChannel_Settings setting,
                                         bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *r_neg = true;
      return ADT_NLA_SKEYS_COLLAPSED;

    default:
      /* this shouldn't happen */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_nla_controls_setting_ptr(bAnimListElem *ale,
                                          eAnimChannel_Settings /*setting*/,
                                          short *r_type)
{
  AnimData *adt = static_cast<AnimData *>(ale->data);

  /* all flags are just in adt->flag for now... */
  return GET_ACF_FLAG_PTR(adt->flag, r_type);
}

static int acf_nla_controls_icon(bAnimListElem * /*ale*/)
{
  return ICON_NLA;
}

/** NLA Control F-Curves expander type define. */
static bAnimChannelType ACF_NLACONTROLS = {
    /*channel_type_name*/ "NLA Controls Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_nla_controls_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_nla_controls_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_nla_controls_name,
    /*name_prop*/ nullptr,
    /*icon*/ acf_nla_controls_icon,

    /*has_setting*/ acf_nla_controls_setting_valid,
    /*setting_flag*/ acf_nla_controls_setting_flag,
    /*setting_ptr*/ acf_nla_controls_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* NLA Control F-Curve -------------------------------- */

/* name for nla control fcurve entries */
static void acf_nla_curve_name(bAnimListElem *ale, char *name)
{
  NlaStrip *strip = static_cast<NlaStrip *>(ale->owner);
  FCurve *fcu = static_cast<FCurve *>(ale->data);
  PropertyRNA *prop;

  /* try to get RNA property that this shortened path (relative to the strip) refers to */
  prop = RNA_struct_type_find_property(&RNA_NlaStrip, fcu->rna_path);
  if (prop) {
    /* "name" of this strip displays the UI identifier + the name of the NlaStrip */
    BLI_snprintf_utf8(
        name, ANIM_CHAN_NAME_SIZE, "%s (%s)", RNA_property_ui_name(prop), strip->name);
  }
  else {
    /* unknown property... */
    BLI_snprintf_utf8(name, ANIM_CHAN_NAME_SIZE, "%s[%d]", fcu->rna_path, fcu->array_index);
  }
}

/** NLA Control F-Curve type define. */
static bAnimChannelType ACF_NLACURVE = {
    /*channel_type_name*/ "NLA Control F-Curve",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_generic_channel_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_nla_curve_name,
    /*name_prop*/ acf_fcurve_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ acf_fcurve_setting_valid,
    /*setting_flag*/ acf_fcurve_setting_flag,
    /*setting_ptr*/ acf_fcurve_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Object Animation Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_fillanim_icon(bAnimListElem * /*ale*/)
{
  return ICON_ACTION;
}

/* check if some setting exists for this channel */
static bool acf_fillanim_setting_valid(bAnimContext * /*ac*/,
                                       bAnimListElem * /*ale*/,
                                       eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_fillanim_setting_flag(bAnimContext * /*ac*/,
                                     eAnimChannel_Settings setting,
                                     bool *r_neg)
{
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_EXPAND:
      return ADT_UI_EXPANDED;

    default:
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_fillanim_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *r_type)
{
  AnimData *adt = ale->adt;
  BLI_assert(adt);

  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
      return GET_ACF_FLAG_PTR(adt->flag, r_type);

    case ACHANNEL_SETTING_EXPAND:
      return GET_ACF_FLAG_PTR(adt->flag, r_type);

    default:
      return nullptr;
  }
}

/* TODO: merge this with the regular Action expander. */
/** Object's Layered Action expander type define. */
static bAnimChannelType ACF_FILLANIM = {
    /*channel_type_name*/ "Ob-Layered-Action Filler",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_fillanim_icon,

    /*has_setting*/ acf_fillanim_setting_valid,
    /*setting_flag*/ acf_fillanim_setting_flag,
    /*setting_ptr*/ acf_fillanim_setting_ptr,
    /*setting_post_update*/ nullptr,
};

static void acf_action_slot_name(bAnimListElem *ale, char *r_name)
{
  const animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
  if (!slot) {
    /* Trying to getting the slot's name without a slot is a bug. */
    BLI_assert_unreachable();
    BLI_strncpy_utf8(r_name, "-nil-", ANIM_CHAN_NAME_SIZE);
    return;
  }

  BLI_assert(ale->bmain);
  const int num_users = slot->users(*ale->bmain).size();
  const char *display_name = slot->identifier_without_prefix().c_str();

  BLI_assert(num_users >= 0);
  switch (num_users) {
    case 0:
      BLI_snprintf_utf8(r_name, ANIM_CHAN_NAME_SIZE, "%s (unassigned)", display_name);
      break;
    case 1:
      BLI_strncpy_utf8(r_name, display_name, ANIM_CHAN_NAME_SIZE);
      break;
    default:
      BLI_snprintf_utf8(r_name, ANIM_CHAN_NAME_SIZE, "%s (%d)", display_name, num_users);
      break;
  }
}
static bool acf_action_slot_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
  BLI_assert(GS(ale->fcurve_owner_id->name) == ID_AC);

  *r_ptr = RNA_pointer_create_discrete(ale->fcurve_owner_id, &RNA_ActionSlot, slot);
  *r_prop = RNA_struct_find_property(r_ptr, "name_display");

  return (*r_prop != nullptr);
}

static int acf_action_slot_icon(bAnimListElem * /*ale*/)
{
  return ICON_ACTION_SLOT;
}

static int acf_action_slot_idtype_icon(bAnimListElem *ale)
{
  animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
  return UI_icon_from_idcode(slot->idtype);
}

static bool acf_action_slot_setting_valid(bAnimContext * /*ac*/,
                                          bAnimListElem * /*ale*/,
                                          const eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    default:
      return false;
  }
}

static int acf_action_slot_setting_flag(bAnimContext * /*ac*/,
                                        eAnimChannel_Settings setting,
                                        bool *r_neg)
{
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
      return int(animrig::Slot::Flags::Selected);
    case ACHANNEL_SETTING_EXPAND:
      return int(animrig::Slot::Flags::Expanded);
    default:
      return 0;
  }
}
static void *acf_action_slot_setting_ptr(bAnimListElem *ale,
                                         eAnimChannel_Settings /*setting*/,
                                         short *r_type)
{
  animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
  return GET_ACF_FLAG_PTR(slot->slot_flags, r_type);
}

static bAnimChannelType ACF_ACTION_SLOT = {
    /*channel_type_name*/ "Action Slot",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_root_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_root_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_action_slot_name,
    /*name_prop*/ acf_action_slot_name_prop,
    /*icon*/ acf_action_slot_icon,

    /*has_setting*/ acf_action_slot_setting_valid,
    /*setting_flag*/ acf_action_slot_setting_flag,
    /*setting_ptr*/ acf_action_slot_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Object Action Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_fillactd_icon(bAnimListElem * /*ale*/)
{
  return ICON_ACTION;
}

/* check if some setting exists for this channel */
static bool acf_fillactd_setting_valid(bAnimContext * /*ac*/,
                                       bAnimListElem * /*ale*/,
                                       eAnimChannel_Settings setting)
{
  switch (setting) {
    /* only select and expand supported */
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_fillactd_setting_flag(bAnimContext * /*ac*/,
                                     eAnimChannel_Settings setting,
                                     bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *r_neg = true;
      return ACT_COLLAPSED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_fillactd_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *r_type)
{
  bAction *act = static_cast<bAction *>(ale->data);
  AnimData *adt = ale->adt;

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      if (adt) {
        return GET_ACF_FLAG_PTR(adt->flag, r_type);
      }
      return nullptr;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(act->flag, r_type);

    default: /* unsupported */
      return nullptr;
  }
}

/** Object action expander type define. */
static bAnimChannelType ACF_FILLACTD = {
    /*channel_type_name*/ "Ob-Action Filler",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_fillactd_icon,

    /*has_setting*/ acf_fillactd_setting_valid,
    /*setting_flag*/ acf_fillactd_setting_flag,
    /*setting_ptr*/ acf_fillactd_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Drivers Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_filldrivers_icon(bAnimListElem * /*ale*/)
{
  return ICON_DRIVER;
}

static void acf_filldrivers_name(bAnimListElem * /*ale*/, char *name)
{
  BLI_strncpy_utf8(name, IFACE_("Drivers"), ANIM_CHAN_NAME_SIZE);
}

/* check if some setting exists for this channel */
/* TODO: this could be made more generic */
static bool acf_filldrivers_setting_valid(bAnimContext * /*ac*/,
                                          bAnimListElem * /*ale*/,
                                          eAnimChannel_Settings setting)
{
  switch (setting) {
    /* only expand supported */
    case ACHANNEL_SETTING_EXPAND:
      return true;

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_filldrivers_setting_flag(bAnimContext * /*ac*/,
                                        eAnimChannel_Settings setting,
                                        bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *r_neg = true;
      return ADT_DRIVERS_COLLAPSED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_filldrivers_setting_ptr(bAnimListElem *ale,
                                         eAnimChannel_Settings setting,
                                         short *r_type)
{
  AnimData *adt = static_cast<AnimData *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(adt->flag, r_type);

    default: /* unsupported */
      return nullptr;
  }
}

/** Drivers expander type define. */
static bAnimChannelType ACF_FILLDRIVERS = {
    /*channel_type_name*/ "Drivers Filler",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_filldrivers_name,
    /*name_prop*/ nullptr,
    /*icon*/ acf_filldrivers_icon,

    /*has_setting*/ acf_filldrivers_setting_valid,
    /*setting_flag*/ acf_filldrivers_setting_flag,
    /*setting_ptr*/ acf_filldrivers_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Material Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsmat_icon(bAnimListElem * /*ale*/)
{
  return ICON_MATERIAL_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsmat_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MA_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmat_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Material *ma = static_cast<Material *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(ma->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (ma->adt) {
        return GET_ACF_FLAG_PTR(ma->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Material expander type define. */
static bAnimChannelType ACF_DSMAT = {
    /*channel_type_name*/ "Material Data Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsmat_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsmat_setting_flag,
    /*setting_ptr*/ acf_dsmat_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Light Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dslight_icon(bAnimListElem * /*ale*/)
{
  return ICON_LIGHT_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dslight_setting_flag(bAnimContext * /*ac*/,
                                    eAnimChannel_Settings setting,
                                    bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LA_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dslight_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *r_type)
{
  Light *la = static_cast<Light *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(la->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (la->adt) {
        return GET_ACF_FLAG_PTR(la->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Light expander type define. */
static bAnimChannelType ACF_DSLIGHT = {
    /*channel_type_name*/ "Light Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dslight_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dslight_setting_flag,
    /*setting_ptr*/ acf_dslight_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Texture Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dstex_icon(bAnimListElem * /*ale*/)
{
  return ICON_TEXTURE_DATA;
}

/* offset for texture expanders */
/* FIXME: soon to be obsolete? */
static short acf_dstex_offset(bAnimContext * /*ac*/, bAnimListElem * /*ale*/)
{
  return 14; /* XXX: simply include this in indentation instead? */
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dstex_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return TEX_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dstex_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Tex *tex = static_cast<Tex *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(tex->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (tex->adt) {
        return GET_ACF_FLAG_PTR(tex->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Texture expander type define. */
static bAnimChannelType ACF_DSTEX = {
    /*channel_type_name*/ "Texture Data Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_dstex_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_dstex_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dstex_setting_flag,
    /*setting_ptr*/ acf_dstex_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Camera Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dscachefile_icon(bAnimListElem *ale)
{
  UNUSED_VARS(ale);
  return ICON_FILE;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dscachefile_setting_flag(bAnimContext *ac,
                                        eAnimChannel_Settings setting,
                                        bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return CACHEFILE_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }

  UNUSED_VARS(ac);
}

/* get pointer to the setting */
static void *acf_dscachefile_setting_ptr(bAnimListElem *ale,
                                         eAnimChannel_Settings setting,
                                         short *r_type)
{
  CacheFile *cache_file = static_cast<CacheFile *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(cache_file->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (cache_file->adt) {
        return GET_ACF_FLAG_PTR(cache_file->adt->flag, r_type);
      }

      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** CacheFile expander type define.. */
static bAnimChannelType ACF_DSCACHEFILE = {
    /*channel_type_name*/ "Cache File Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_dscachefile_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dscachefile_setting_flag,
    /*setting_ptr*/ acf_dscachefile_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Camera Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dscam_icon(bAnimListElem * /*ale*/)
{
  return ICON_CAMERA_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dscam_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return CAM_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dscam_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Camera *ca = static_cast<Camera *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(ca->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (ca->adt) {
        return GET_ACF_FLAG_PTR(ca->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Camera expander type define. */
static bAnimChannelType ACF_DSCAM = {
    /*channel_type_name*/ "Camera Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_dscam_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dscam_setting_flag,
    /*setting_ptr*/ acf_dscam_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Curve Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dscur_icon(bAnimListElem *ale)
{
  Curve *cu = static_cast<Curve *>(ale->data);
  switch (cu->ob_type) {
    case OB_FONT:
      return ICON_FONT_DATA;
    case OB_SURF:
      return ICON_SURFACE_DATA;
    default:
      return ICON_CURVE_DATA;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dscur_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return CU_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dscur_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Curve *cu = static_cast<Curve *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(cu->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (cu->adt) {
        return GET_ACF_FLAG_PTR(cu->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Curve expander type define. */
static bAnimChannelType ACF_DSCUR = {
    /*channel_type_name*/ "Curve Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dscur_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dscur_setting_flag,
    /*setting_ptr*/ acf_dscur_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Shape Key Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsskey_icon(bAnimListElem * /*ale*/)
{
  return ICON_SHAPEKEY_DATA;
}

/* check if some setting exists for this channel */
static bool acf_dsskey_setting_valid(bAnimContext *ac,
                                     bAnimListElem * /*ale*/,
                                     eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    /* mute is only supported for NLA */
    case ACHANNEL_SETTING_MUTE:
      return ((ac) && (ac->spacetype == SPACE_NLA));

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsskey_setting_flag(bAnimContext * /*ac*/,
                                   eAnimChannel_Settings setting,
                                   bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return KEY_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsskey_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings setting,
                                    short *r_type)
{
  Key *key = static_cast<Key *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(key->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (key->adt) {
        return GET_ACF_FLAG_PTR(key->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Shape-key expander type define. */
static bAnimChannelType ACF_DSSKEY = {
    /*channel_type_name*/ "Shape Key Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsskey_icon,

    /*has_setting*/ acf_dsskey_setting_valid,
    /*setting_flag*/ acf_dsskey_setting_flag,
    /*setting_ptr*/ acf_dsskey_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* World Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dswor_icon(bAnimListElem * /*ale*/)
{
  return ICON_WORLD_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dswor_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return WO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dswor_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  World *wo = static_cast<World *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(wo->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (wo->adt) {
        return GET_ACF_FLAG_PTR(wo->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** World expander type define. */
static bAnimChannelType ACF_DSWOR = {
    /*channel_type_name*/ "World Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_dswor_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dswor_setting_flag,
    /*setting_ptr*/ acf_dswor_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Particle Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dspart_icon(bAnimListElem * /*ale*/)
{
  return ICON_PARTICLE_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dspart_setting_flag(bAnimContext * /*ac*/,
                                   eAnimChannel_Settings setting,
                                   bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return PART_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dspart_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings setting,
                                    short *r_type)
{
  ParticleSettings *part = static_cast<ParticleSettings *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(part->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (part->adt) {
        return GET_ACF_FLAG_PTR(part->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Particle expander type define. */
static bAnimChannelType ACF_DSPART = {
    /*channel_type_name*/ "Particle Data Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dspart_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dspart_setting_flag,
    /*setting_ptr*/ acf_dspart_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* MetaBall Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsmball_icon(bAnimListElem * /*ale*/)
{
  return ICON_META_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsmball_setting_flag(bAnimContext * /*ac*/,
                                    eAnimChannel_Settings setting,
                                    bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MB_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmball_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *r_type)
{
  MetaBall *mb = static_cast<MetaBall *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(mb->flag2, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (mb->adt) {
        return GET_ACF_FLAG_PTR(mb->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Meta-ball expander type define. */
static bAnimChannelType ACF_DSMBALL = {
    /*channel_type_name*/ "Metaball Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsmball_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsmball_setting_flag,
    /*setting_ptr*/ acf_dsmball_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Armature Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsarm_icon(bAnimListElem * /*ale*/)
{
  return ICON_ARMATURE_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsarm_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return ARM_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsarm_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  bArmature *arm = static_cast<bArmature *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(arm->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (arm->adt) {
        return GET_ACF_FLAG_PTR(arm->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Armature expander type define. */
static bAnimChannelType ACF_DSARM = {
    /*channel_type_name*/ "Armature Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsarm_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsarm_setting_flag,
    /*setting_ptr*/ acf_dsarm_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* NodeTree Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsntree_icon(bAnimListElem * /*ale*/)
{
  return ICON_NODETREE;
}

/* offset for nodetree expanders */
static short acf_dsntree_offset(bAnimContext *ac, bAnimListElem *ale)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(ale->data);
  short offset = acf_generic_basic_offset(ac, ale);

  offset += acf_nodetree_rootType_offset(ntree);

  return offset;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsntree_setting_flag(bAnimContext * /*ac*/,
                                    eAnimChannel_Settings setting,
                                    bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return NTREE_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsntree_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *r_type)
{
  bNodeTree *ntree = static_cast<bNodeTree *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(ntree->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (ntree->adt) {
        return GET_ACF_FLAG_PTR(ntree->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Node tree expander type define. */
static bAnimChannelType ACF_DSNTREE = {
    /*channel_type_name*/ "Node Tree Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_dsntree_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsntree_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsntree_setting_flag,
    /*setting_ptr*/ acf_dsntree_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* LineStyle Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dslinestyle_icon(bAnimListElem * /*ale*/)
{
  return ICON_LINE_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dslinestyle_setting_flag(bAnimContext * /*ac*/,
                                        eAnimChannel_Settings setting,
                                        bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LS_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dslinestyle_setting_ptr(bAnimListElem *ale,
                                         eAnimChannel_Settings setting,
                                         short *r_type)
{
  FreestyleLineStyle *linestyle = static_cast<FreestyleLineStyle *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(linestyle->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (linestyle->adt) {
        return GET_ACF_FLAG_PTR(linestyle->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Line Style expander type define. */
static bAnimChannelType ACF_DSLINESTYLE = {
    /*channel_type_name*/ "Line Style Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dslinestyle_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dslinestyle_setting_flag,
    /*setting_ptr*/ acf_dslinestyle_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Mesh Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsmesh_icon(bAnimListElem * /*ale*/)
{
  return ICON_MESH_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsmesh_setting_flag(bAnimContext * /*ac*/,
                                   eAnimChannel_Settings setting,
                                   bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return ME_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmesh_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings setting,
                                    short *r_type)
{
  Mesh *mesh = static_cast<Mesh *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(mesh->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (mesh->adt) {
        return GET_ACF_FLAG_PTR(mesh->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Mesh expander type define. */
static bAnimChannelType ACF_DSMESH = {
    /*channel_type_name*/ "Mesh Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /* XXX: this only works for compositing. */
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsmesh_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsmesh_setting_flag,
    /*setting_ptr*/ acf_dsmesh_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Lattice Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dslat_icon(bAnimListElem * /*ale*/)
{
  return ICON_LATTICE_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dslat_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LT_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dslat_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Lattice *lt = static_cast<Lattice *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(lt->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (lt->adt) {
        return GET_ACF_FLAG_PTR(lt->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Lattice expander type define. */
static bAnimChannelType ACF_DSLAT = {
    /*channel_type_name*/ "Lattice Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /* XXX: this only works for compositing. */
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dslat_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dslat_setting_flag,
    /*setting_ptr*/ acf_dslat_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Speaker Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsspk_icon(bAnimListElem * /*ale*/)
{
  return ICON_SPEAKER;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsspk_setting_flag(bAnimContext * /*ac*/,
                                  eAnimChannel_Settings setting,
                                  bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return SPK_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsspk_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings setting,
                                   short *r_type)
{
  Speaker *spk = static_cast<Speaker *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(spk->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (spk->adt) {
        return GET_ACF_FLAG_PTR(spk->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Speaker expander type define. */
static bAnimChannelType ACF_DSSPK = {
    /*channel_type_name*/ "Speaker Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsspk_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsspk_setting_flag,
    /*setting_ptr*/ acf_dsspk_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Curves Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dscurves_icon(bAnimListElem * /*ale*/)
{
  return ICON_CURVES_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dscurves_setting_flag(bAnimContext * /*ac*/,
                                     eAnimChannel_Settings setting,
                                     bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return VO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dscurves_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *r_type)
{
  Curves *curves = static_cast<Curves *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(curves->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (curves->adt) {
        return GET_ACF_FLAG_PTR(curves->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Curves expander type define. */
static bAnimChannelType ACF_DSCURVES = {
    /*channel_type_name*/ "Curves Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dscurves_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dscurves_setting_flag,
    /*setting_ptr*/ acf_dscurves_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* PointCloud Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dspointcloud_icon(bAnimListElem * /*ale*/)
{
  return ICON_POINTCLOUD_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dspointcloud_setting_flag(bAnimContext * /*ac*/,
                                         eAnimChannel_Settings setting,
                                         bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return VO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dspointcloud_setting_ptr(bAnimListElem *ale,
                                          eAnimChannel_Settings setting,
                                          short *r_type)
{
  PointCloud *pointcloud = static_cast<PointCloud *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(pointcloud->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (pointcloud->adt) {
        return GET_ACF_FLAG_PTR(pointcloud->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Point-cloud expander type define. */
static bAnimChannelType ACF_DSPOINTCLOUD = {
    /*channel_type_name*/ "PointCloud Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dspointcloud_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dspointcloud_setting_flag,
    /*setting_ptr*/ acf_dspointcloud_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Volume Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsvolume_icon(bAnimListElem * /*ale*/)
{
  return ICON_VOLUME_DATA;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsvolume_setting_flag(bAnimContext * /*ac*/,
                                     eAnimChannel_Settings setting,
                                     bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return VO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsvolume_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *r_type)
{
  Volume *volume = static_cast<Volume *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(volume->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (volume->adt) {
        return GET_ACF_FLAG_PTR(volume->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Volume expander type define. */
static bAnimChannelType ACF_DSVOLUME = {
    /*channel_type_name*/ "Volume Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsvolume_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsvolume_setting_flag,
    /*setting_ptr*/ acf_dsvolume_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* LightProbe Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dslightprobe_icon(bAnimListElem *ale)
{
  const LightProbe *probe = static_cast<LightProbe *>(ale->data);
  switch (probe->type) {
    case LIGHTPROBE_TYPE_SPHERE:
      return ICON_LIGHTPROBE_SPHERE;
    case LIGHTPROBE_TYPE_PLANE:
      return ICON_LIGHTPROBE_PLANE;
    case LIGHTPROBE_TYPE_VOLUME:
      return ICON_LIGHTPROBE_VOLUME;
    default:
      return ICON_LIGHTPROBE_SPHERE;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dslightprobe_setting_flag(bAnimContext * /*ac*/,
                                         eAnimChannel_Settings setting,
                                         bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LIGHTPROBE_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dslightprobe_setting_ptr(bAnimListElem *ale,
                                          eAnimChannel_Settings setting,
                                          short *r_type)
{
  LightProbe *probe = static_cast<LightProbe *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(probe->flag, r_type);

    case ACHANNEL_SETTING_SELECT:         /* selected */
    case ACHANNEL_SETTING_MUTE:           /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE:        /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE: /* pin */
      if (probe->adt) {
        return GET_ACF_FLAG_PTR(probe->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Light Probe expander type define. */
static bAnimChannelType ACF_DSLIGHTPROBE = {
    /*channel_type_name*/ "LightProbe Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dslightprobe_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dslightprobe_setting_flag,
    /*setting_ptr*/ acf_dslightprobe_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* GPencil Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsgpencil_icon(bAnimListElem * /*ale*/)
{
  return ICON_OUTLINER_DATA_GREASEPENCIL;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsgpencil_setting_flag(bAnimContext * /*ac*/,
                                      eAnimChannel_Settings setting,
                                      bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GP_DATA_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsgpencil_setting_ptr(bAnimListElem *ale,
                                       eAnimChannel_Settings setting,
                                       short *r_type)
{
  bGPdata *gpd = static_cast<bGPdata *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(gpd->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (gpd->adt) {
        return GET_ACF_FLAG_PTR(gpd->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Grease-pencil expander type define. */
static bAnimChannelType ACF_DSGPENCIL = {
    /*channel_type_name*/ "GPencil DS Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idblock_name_prop,
    /*icon*/ acf_dsgpencil_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsgpencil_setting_flag,
    /*setting_ptr*/ acf_dsgpencil_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* World Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dsmclip_icon(bAnimListElem * /*ale*/)
{
  return ICON_SEQUENCE;
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_dsmclip_setting_flag(bAnimContext * /*ac*/,
                                    eAnimChannel_Settings setting,
                                    bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MCLIP_DATA_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *r_neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmclip_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *r_type)
{
  MovieClip *clip = static_cast<MovieClip *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(clip->flag, r_type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (clip->adt != nullptr) {
        return GET_ACF_FLAG_PTR(clip->adt->flag, r_type);
      }
      return nullptr;

    default: /* unsupported */
      return nullptr;
  }
}

/** Movie-clip expander type define. */
static bAnimChannelType ACF_DSMCLIP = {
    /*channel_type_name*/ "Movieclip Expander",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_generic_dataexpand_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_dataexpand_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_dsmclip_icon,

    /*has_setting*/ acf_generic_dataexpand_setting_valid,
    /*setting_flag*/ acf_dsmclip_setting_flag,
    /*setting_ptr*/ acf_dsmclip_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* ShapeKey Entry  ------------------------------------------- */

/* name for ShapeKey */
static void acf_shapekey_name(bAnimListElem *ale, char *name)
{
  KeyBlock *kb = static_cast<KeyBlock *>(ale->data);

  /* just copy the name... */
  if (kb && name) {
    /* if the KeyBlock had a name, use it, otherwise use the index */
    if (kb->name[0]) {
      BLI_strncpy_utf8(name, kb->name, ANIM_CHAN_NAME_SIZE);
    }
    else {
      BLI_snprintf_utf8(name, ANIM_CHAN_NAME_SIZE, IFACE_("Key %d"), ale->index);
    }
  }
}

/* name property for ShapeKey entries */
static bool acf_shapekey_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  KeyBlock *kb = static_cast<KeyBlock *>(ale->data);

  /* if the KeyBlock had a name, use it, otherwise use the index */
  if (kb && kb->name[0]) {
    *r_ptr = RNA_pointer_create_discrete(ale->id, &RNA_ShapeKey, kb);
    *r_prop = RNA_struct_name_property(r_ptr->type);

    return (*r_prop != nullptr);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_shapekey_setting_valid(bAnimContext * /*ac*/,
                                       bAnimListElem * /*ale*/,
                                       eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted */
    case ACHANNEL_SETTING_PROTECT: /* protected */
      return true;

    /* nothing else is supported */
    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_shapekey_setting_flag(bAnimContext * /*ac*/,
                                     eAnimChannel_Settings setting,
                                     bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_MUTE: /* mute */
      return KEYBLOCK_MUTE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return KEYBLOCK_SEL;

    case ACHANNEL_SETTING_PROTECT: /* locked */
      return KEYBLOCK_LOCKED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_shapekey_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *r_type)
{
  KeyBlock *kb = static_cast<KeyBlock *>(ale->data);

  /* Clear extra return data first. */
  *r_type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted */
    case ACHANNEL_SETTING_PROTECT: /* protected */
      return GET_ACF_FLAG_PTR(kb->flag, r_type);

    default: /* unsupported */
      return nullptr;
  }
}

/** Shape-key expander type define. */
static bAnimChannelType ACF_SHAPEKEY = {
    /*channel_type_name*/ "Shape Key",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_generic_channel_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_shapekey_name,
    /*name_prop*/ acf_shapekey_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ acf_shapekey_setting_valid,
    /*setting_flag*/ acf_shapekey_setting_flag,
    /*setting_ptr*/ acf_shapekey_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* GPencil Datablock (Legacy) ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_gpd_icon(bAnimListElem * /*ale*/)
{
  return ICON_OUTLINER_DATA_GREASEPENCIL;
}

/* check if some setting exists for this channel */
static bool acf_gpd_setting_valid(bAnimContext * /*ac*/,
                                  bAnimListElem * /*ale*/,
                                  eAnimChannel_Settings setting)
{
  switch (setting) {
    /* only select and expand supported */
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    default:
      return false;
  }
}

/* GPencil Layer (Legacy) ------------------------------------------- */

/* name for grease pencil layer entries */
static void acf_gpl_name_legacy(bAnimListElem *ale, char *name)
{
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

  if (gpl && name) {
    BLI_strncpy_utf8(name, gpl->info, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for grease pencil layer entries */
static bool acf_gpl_name_prop_legacy(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  if (ale->data) {
    *r_ptr = RNA_pointer_create_discrete(ale->id, &RNA_AnnotationLayer, ale->data);
    *r_prop = RNA_struct_name_property(r_ptr->type);

    return (*r_prop != nullptr);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_gpl_setting_valid_legacy(bAnimContext * /*ac*/,
                                         bAnimListElem * /*ale*/,
                                         eAnimChannel_Settings setting)
{
  switch (setting) {
    /* unsupported */
    case ACHANNEL_SETTING_EXPAND: /* gpencil layers are more like F-Curves than groups */
    case ACHANNEL_SETTING_SOLO:   /* nla editor only */
    case ACHANNEL_SETTING_MOD_OFF:
    case ACHANNEL_SETTING_PINNED: /* nla actions only */
      return false;

    /* always available */
    default:
      return true;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_gpl_setting_flag_legacy(bAnimContext * /*ac*/,
                                       eAnimChannel_Settings setting,
                                       bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return GP_LAYER_SELECT;

    case ACHANNEL_SETTING_MUTE: /* animation muting - similar to frame lock... */
      return GP_LAYER_FRAMELOCK;

    case ACHANNEL_SETTING_VISIBLE: /* visibility of the layers (NOT muting) */
      *r_neg = true;
      return GP_LAYER_HIDE;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return GP_LAYER_LOCKED;

    default: /* unsupported */
      return 0;
  }
}

static bool acf_gpl_channel_color(const bAnimListElem *ale, uint8_t r_color[3])
{
  const bGPDlayer *gpl = static_cast<const bGPDlayer *>(ale->data);
  rgb_float_to_uchar(r_color, gpl->color);
  return true;
}

/* get pointer to the setting */
static void *acf_gpl_setting_ptr_legacy(bAnimListElem *ale,
                                        eAnimChannel_Settings /*setting*/,
                                        short *r_type)
{
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

  /* all flags are just in gpl->flag for now... */
  return GET_ACF_FLAG_PTR(gpl->flag, r_type);
}

/** Grease-pencil layer type define. */
static bAnimChannelType ACF_GPL_LEGACY = {
    /*channel_type_name*/ "GPencil Layer",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_generic_channel_color,
    /*get_channel_color*/ acf_gpl_channel_color,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_flexible,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_gpl_name_legacy,
    /*name_prop*/ acf_gpl_name_prop_legacy,
    /*icon*/ nullptr,

    /*has_setting*/ acf_gpl_setting_valid_legacy,
    /*setting_flag*/ acf_gpl_setting_flag_legacy,
    /*setting_ptr*/ acf_gpl_setting_ptr_legacy,
    /*setting_post_update*/ nullptr,
};

/* Grease Pencil Animation functions ------------------------------------------- */

namespace blender::ed::animation::greasepencil {

/* Get pointer to the setting */
static void *data_block_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings /*setting*/,
                                    short *r_type)
{
  GreasePencil *grease_pencil = (GreasePencil *)ale->data;

  return GET_ACF_FLAG_PTR(grease_pencil->flag, r_type);
}
static void datablock_color(bAnimContext *ac, bAnimListElem * /*ale*/, float r_color[3])
{
  if (ac->datatype == ANIMCONT_GPENCIL) {
    UI_GetThemeColorShade3fv(TH_DOPESHEET_CHANNELSUBOB, 20, r_color);
  }
  else {
    UI_GetThemeColor3fv(TH_DOPESHEET_CHANNELSUBOB, r_color);
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int data_block_setting_flag(bAnimContext * /*ac*/,
                                   eAnimChannel_Settings setting,
                                   bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* Selected */
      return AGRP_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* Expanded */
      return GREASE_PENCIL_ANIM_CHANNEL_EXPANDED;

    default:
      /* This shouldn't happen */
      BLI_assert_msg(true, "Unexpected channel flag");
      return 0;
  }
}

/* Offset of the channel, defined by its depth in the tree hierarchy. */
static short layer_offset(bAnimContext *ac, bAnimListElem *ale)
{
  GreasePencilLayerTreeNode *node = static_cast<GreasePencilLayerTreeNode *>(ale->data);

  short offset = acf_generic_basic_offset(ac, ale);
  offset += node->wrap().depth() * short(0.7f * U.widget_unit);

  return offset;
}

/* Name for grease pencil layer entries. */
static void layer_name(bAnimListElem *ale, char *name)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ale->data);

  if (layer && name) {
    BLI_strncpy_utf8(name, layer->wrap().name().c_str(), ANIM_CHAN_NAME_SIZE);
  }
}

/* Name property for grease pencil layer entries.
 * Common for layers & layer groups.
 */
static bool layer_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  if (ale->data == nullptr) {
    return false;
  }

  *r_ptr = RNA_pointer_create_discrete(ale->id, &RNA_GreasePencilLayer, ale->data);
  *r_prop = RNA_struct_name_property(r_ptr->type);

  return (*r_prop != nullptr);
}

static bool layer_setting_valid(bAnimContext * /*ac*/,
                                bAnimListElem * /*ale*/,
                                eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_EXPAND:
    case ACHANNEL_SETTING_SOLO: /* NLA editor only. */
    case ACHANNEL_SETTING_MOD_OFF:
    case ACHANNEL_SETTING_PINNED: /* NLA actions only. */
      return false;

    default:
      return true;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid.
 * Common for layers & layer groups.
 */
static int layer_setting_flag(bAnimContext * /*ac*/, eAnimChannel_Settings setting, bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* Layer selected. */
      return GP_LAYER_TREE_NODE_SELECT;

    case ACHANNEL_SETTING_MUTE: /* Animation muting. */
      return GP_LAYER_TREE_NODE_MUTE;

    case ACHANNEL_SETTING_VISIBLE: /* Visibility of the layers. */
      *r_neg = true;
      return GP_LAYER_TREE_NODE_HIDE;

    case ACHANNEL_SETTING_PROTECT: /* Layer locked. */
      return GP_LAYER_TREE_NODE_LOCKED;

    case ACHANNEL_SETTING_EXPAND: /* Layer expanded (for layer groups). */
      return GP_LAYER_TREE_NODE_EXPANDED;

    default: /* Unsupported. */
      return 0;
  }
}
/* Get pointer to the setting. */
static void *layer_setting_ptr(bAnimListElem *ale,
                               eAnimChannel_Settings /*setting*/,
                               short *r_type)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ale->data);
  return GET_ACF_FLAG_PTR(layer->base.flag, r_type);
}

static bool layer_channel_color(const bAnimListElem *ale, uint8_t r_color[3])
{
  using namespace bke::greasepencil;
  GreasePencilLayerTreeNode &node = *static_cast<GreasePencilLayerTreeNode *>(ale->data);
  rgb_float_to_uchar(r_color, node.color);
  return true;
}

static int layer_group_icon(bAnimListElem *ale)
{
  using namespace bke::greasepencil;
  const LayerGroup &group = *static_cast<LayerGroup *>(ale->data);
  int icon = ICON_GREASEPENCIL_LAYER_GROUP;
  if (group.color_tag != LAYERGROUP_COLOR_NONE) {
    icon = ICON_LAYERGROUP_COLOR_01 + group.color_tag;
  }
  return icon;
}

static void layer_group_color(bAnimContext * /*ac*/, bAnimListElem * /*ale*/, float r_color[3])
{
  UI_GetThemeColor3fv(TH_GROUP, r_color);
}

/* Name for grease pencil layer entries */
static void layer_group_name(bAnimListElem *ale, char *name)
{
  GreasePencilLayerTreeGroup *layer_group = static_cast<GreasePencilLayerTreeGroup *>(ale->data);

  if (layer_group && name) {
    BLI_strncpy_utf8(name, layer_group->wrap().name().c_str(), ANIM_CHAN_NAME_SIZE);
  }
}

/* Get pointer to the setting. */
static void *layer_group_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings /*setting*/,
                                     short *r_type)
{
  GreasePencilLayerTreeGroup *layer_group = static_cast<GreasePencilLayerTreeGroup *>(ale->data);
  return GET_ACF_FLAG_PTR(layer_group->base.flag, r_type);
}

/* Check if some setting exists for this channel. */
static bool layer_group_setting_valid(bAnimContext * /*ac*/,
                                      bAnimListElem * /*ale*/,
                                      eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
    case ACHANNEL_SETTING_PROTECT:
    case ACHANNEL_SETTING_VISIBLE:
      return true;

    default:
      return false;
  }
}

}  // namespace blender::ed::animation::greasepencil

using namespace blender::ed::animation;

/* Grease Pencil Datablock ------------------------------------------- */
static bAnimChannelType ACF_GPD = {
    /*channel_type_name*/ "Grease Pencil Datablock",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ greasepencil::datablock_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_group_backdrop,
    /*get_indent_level*/ acf_generic_indentation_1,
    /*get_offset*/ acf_generic_basic_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_gpd_icon,

    /*has_setting*/ acf_gpd_setting_valid,
    /*setting_flag*/ greasepencil::data_block_setting_flag,
    /*setting_ptr*/ greasepencil::data_block_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Grease Pencil Layer ------------------------------------------- */
static bAnimChannelType ACF_GPL = {
    /*channel_type_name*/ "Grease Pencil Layer",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_generic_channel_color,
    /*get_channel_color*/ greasepencil::layer_channel_color,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_flexible,
    /*get_offset*/ greasepencil::layer_offset,

    /*name*/ greasepencil::layer_name,
    /*name_prop*/ greasepencil::layer_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ greasepencil::layer_setting_valid,
    /*setting_flag*/ greasepencil::layer_setting_flag,
    /*setting_ptr*/ greasepencil::layer_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Grease Pencil Layer Group -------------------------------- */
static bAnimChannelType ACF_GPLGROUP = {
    /*channel_type_name*/ "Grease Pencil Layer Group",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ greasepencil::layer_group_color,
    /*get_channel_color*/ greasepencil::layer_channel_color,
    /*draw_backdrop*/ acf_group_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ greasepencil::layer_offset,

    /*name*/ greasepencil::layer_group_name,
    /*name_prop*/ greasepencil::layer_name_prop,
    /*icon*/ greasepencil::layer_group_icon,

    /*has_setting*/ greasepencil::layer_group_setting_valid,
    /*setting_flag*/ greasepencil::layer_setting_flag,
    /*setting_ptr*/ greasepencil::layer_group_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Mask Datablock ------------------------------------------- */

/* get backdrop color for mask datablock widget */
static void acf_mask_color(bAnimContext * /*ac*/, bAnimListElem * /*ale*/, float r_color[3])
{
  /* these are ID-blocks, but not exactly standalone... */
  UI_GetThemeColorShade3fv(TH_DOPESHEET_CHANNELSUBOB, 20, r_color);
}

/* TODO: just get this from RNA? */
static int acf_mask_icon(bAnimListElem * /*ale*/)
{
  return ICON_MOD_MASK;
}

/* check if some setting exists for this channel */
static bool acf_mask_setting_valid(bAnimContext * /*ac*/,
                                   bAnimListElem * /*ale*/,
                                   eAnimChannel_Settings setting)
{
  switch (setting) {
    /* only select and expand supported */
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_EXPAND:
      return true;

    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_mask_setting_flag(bAnimContext * /*ac*/, eAnimChannel_Settings setting, bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return AGRP_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MASK_ANIMF_EXPAND;

    default:
      /* this shouldn't happen */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_mask_setting_ptr(bAnimListElem *ale,
                                  eAnimChannel_Settings /*setting*/,
                                  short *r_type)
{
  Mask *mask = static_cast<Mask *>(ale->data);

  /* all flags are just in mask->flag for now... */
  return GET_ACF_FLAG_PTR(mask->flag, r_type);
}

/** Mask data-block type define. */
static bAnimChannelType ACF_MASKDATA = {
    /*channel_type_name*/ "Mask Datablock",
    /*channel_role*/ ACHANNEL_ROLE_EXPANDER,

    /*get_backdrop_color*/ acf_mask_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_group_backdrop,
    /*get_indent_level*/ acf_generic_indentation_0,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_generic_idblock_name,
    /*name_prop*/ acf_generic_idfill_name_prop,
    /*icon*/ acf_mask_icon,

    /*has_setting*/ acf_mask_setting_valid,
    /*setting_flag*/ acf_mask_setting_flag,
    /*setting_ptr*/ acf_mask_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* Mask Layer ------------------------------------------- */

/* name for grease pencil layer entries */
static void acf_masklay_name(bAnimListElem *ale, char *name)
{
  MaskLayer *masklay = static_cast<MaskLayer *>(ale->data);

  if (masklay && name) {
    BLI_strncpy_utf8(name, masklay->name, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for grease pencil layer entries */
static bool acf_masklay_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  if (ale->data) {
    *r_ptr = RNA_pointer_create_discrete(ale->id, &RNA_MaskLayer, ale->data);
    *r_prop = RNA_struct_name_property(r_ptr->type);

    return (*r_prop != nullptr);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_masklay_setting_valid(bAnimContext * /*ac*/,
                                      bAnimListElem * /*ale*/,
                                      eAnimChannel_Settings setting)
{
  switch (setting) {
    /* unsupported */
    case ACHANNEL_SETTING_EXPAND:  /* mask layers are more like F-Curves than groups */
    case ACHANNEL_SETTING_VISIBLE: /* graph editor only */
    case ACHANNEL_SETTING_SOLO:    /* nla editor only */
    case ACHANNEL_SETTING_MOD_OFF:
    case ACHANNEL_SETTING_PINNED: /* nla actions only */
    case ACHANNEL_SETTING_MUTE:
      return false;

    /* always available */
    default:
      return true;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_masklay_setting_flag(bAnimContext * /*ac*/,
                                    eAnimChannel_Settings setting,
                                    bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return MASK_LAYERFLAG_SELECT;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return MASK_LAYERFLAG_LOCKED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_masklay_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings /*setting*/,
                                     short *r_type)
{
  MaskLayer *masklay = static_cast<MaskLayer *>(ale->data);

  /* all flags are just in masklay->flag for now... */
  return GET_ACF_FLAG_PTR(masklay->flag, r_type);
}

/** Mask layer type define. */
static bAnimChannelType ACF_MASKLAYER = {
    /*channel_type_name*/ "Mask Layer",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_generic_channel_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_flexible,
    /*get_offset*/ acf_generic_group_offset,

    /*name*/ acf_masklay_name,
    /*name_prop*/ acf_masklay_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ acf_masklay_setting_valid,
    /*setting_flag*/ acf_masklay_setting_flag,
    /*setting_ptr*/ acf_masklay_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* NLA Track ----------------------------------------------- */

/* get backdrop color for nla track channels */
static void acf_nlatrack_color(bAnimContext * /*ac*/, bAnimListElem *ale, float r_color[3])
{
  NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
  AnimData *adt = ale->adt;
  bool nonSolo = false;

  /* is track enabled for solo drawing? */
  if ((adt) && (adt->flag & ADT_NLA_SOLO_TRACK)) {
    if ((nlt->flag & NLATRACK_SOLO) == 0) {
      /* tag for special non-solo handling */
      nonSolo = true;
    }
  }

  /* set color for nla track */
  UI_GetThemeColorShade3fv(TH_NLA_TRACK, ((nonSolo == false) ? 20 : -20), r_color);
}

/* name for nla track entries */
static void acf_nlatrack_name(bAnimListElem *ale, char *name)
{
  NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);

  if (nlt && name) {
    BLI_strncpy_utf8(name, nlt->name, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for nla track entries */
static bool acf_nlatrack_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  if (ale->data) {
    *r_ptr = RNA_pointer_create_discrete(ale->id, &RNA_NlaTrack, ale->data);
    *r_prop = RNA_struct_name_property(r_ptr->type);

    return (*r_prop != nullptr);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_nlatrack_setting_valid(bAnimContext * /*ac*/,
                                       bAnimListElem *ale,
                                       eAnimChannel_Settings setting)
{
  NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
  AnimData *adt = ale->adt;

  /* visibility of settings depends on various states... */
  switch (setting) {
    /* always supported */
    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_SOLO:
      return true;

    /* conditionally supported... */
    case ACHANNEL_SETTING_PROTECT:
    case ACHANNEL_SETTING_MUTE:
      /* if this track is active and we're tweaking it, don't draw these toggles */
      if (((nlt->flag & NLATRACK_ACTIVE) && (nlt->flag & NLATRACK_DISABLED)) == 0) {
        /* is track enabled for solo drawing? */
        if ((adt) && (adt->flag & ADT_NLA_SOLO_TRACK)) {
          if (nlt->flag & NLATRACK_SOLO) {
            /* ok - we've got a solo track, and this is it */
            return true;
          }
          /* not ok - we've got a solo track, but this isn't it, so make it more obvious */
          return false;
        }

        /* Ok - no tracks are soloed, and this isn't being tweaked. */
        return true;
      }
      /* unsupported - this track is being tweaked */
      return false;

    /* unsupported */
    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_nlatrack_setting_flag(bAnimContext * /*ac*/,
                                     eAnimChannel_Settings setting,
                                     bool *neg)
{
  /* Clear extra return data first. */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return NLATRACK_SELECTED;

    case ACHANNEL_SETTING_MUTE: /* muted */
      return NLATRACK_MUTED;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return NLATRACK_PROTECTED;

    case ACHANNEL_SETTING_SOLO: /* solo */
      return NLATRACK_SOLO;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_nlatrack_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings /*setting*/,
                                      short *r_type)
{
  NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
  return GET_ACF_FLAG_PTR(nlt->flag, r_type);
}

static void acf_nlatrack_setting_post_update(Main &bmain,
                                             const bAnimListElem & /*ale*/,
                                             const eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_MUTE:
    case ACHANNEL_SETTING_SOLO:
      /* Changing these settings can change whether data-blocks are animated at all. */
      DEG_relations_tag_update(&bmain);
      break;

    case ACHANNEL_SETTING_SELECT:
    case ACHANNEL_SETTING_PROTECT:
    case ACHANNEL_SETTING_EXPAND:
    case ACHANNEL_SETTING_VISIBLE:
    case ACHANNEL_SETTING_PINNED:
    case ACHANNEL_SETTING_MOD_OFF:
    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      break;
  }
}

/** NLA track type define. */
static bAnimChannelType ACF_NLATRACK = {
    /*channel_type_name*/ "NLA Track",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,

    /*get_backdrop_color*/ acf_nlatrack_color,
    /*get_channel_color*/ nullptr,
    /*draw_backdrop*/ acf_generic_channel_backdrop,
    /*get_indent_level*/ acf_generic_indentation_flexible,
    /*get_offset*/ acf_generic_group_offset, /* XXX? */

    /*name*/ acf_nlatrack_name,
    /*name_prop*/ acf_nlatrack_name_prop,
    /*icon*/ nullptr,

    /*has_setting*/ acf_nlatrack_setting_valid,
    /*setting_flag*/ acf_nlatrack_setting_flag,
    /*setting_ptr*/ acf_nlatrack_setting_ptr,
    /*setting_post_update*/ acf_nlatrack_setting_post_update,
};

/* NLA Action ----------------------------------------------- */

/* icon for action depends on whether it's in tweaking mode */
static int acf_nlaaction_icon(bAnimListElem *ale)
{
  AnimData *adt = ale->adt;

  /* indicate tweaking-action state by changing the icon... */
  if ((adt) && (adt->flag & ADT_NLA_EDIT_ON)) {
    return ICON_ACTION_TWEAK;
  }

  return ICON_ACTION;
}

/* Backdrop color for nla action track
 * Although this can't be used directly for NLA Action drawing,
 * it is still needed for use behind the RHS toggles
 */
static void acf_nlaaction_color(bAnimContext * /*ac*/, bAnimListElem *ale, float r_color[3])
{
  float color[4];

  /* Action Line
   *   The alpha values action_get_color returns are only useful for drawing
   *   strips backgrounds but here we're doing track list backgrounds instead
   *   so we ignore that and use our own when needed
   */
  nla_action_get_color(ale->adt, static_cast<bAction *>(ale->data), color);

  /* NOTE: since the return types only allow rgb, we cannot do the alpha-blending we'd
   * like for the solo-drawing case. Hence, this method isn't actually used for drawing
   * most of the track...
   */
  copy_v3_v3(r_color, color);
}

/* backdrop for nla action track */
static void acf_nlaaction_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  AnimData *adt = ale->adt;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[4];

  /* Action Line
   *   The alpha values action_get_color returns are only useful for drawing
   *   strips backgrounds but here we're doing track list backgrounds instead
   *   so we ignore that and use our own when needed
   */
  nla_action_get_color(adt, (bAction *)ale->data, color);

  if (adt && (adt->flag & ADT_NLA_EDIT_ON)) {
    color[3] = 1.0f;
  }
  else {
    color[3] = (adt && (adt->flag & ADT_NLA_SOLO_TRACK)) ? 0.3f : 1.0f;
  }

  /* only on top left corner, to show that this track sits on top of the preceding ones
   * while still linking into the action line strip to the right
   */
  UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT);

  /* draw slightly shifted up vertically to look like it has more separation from other tracks,
   * but we then need to slightly shorten it so that it doesn't look like it overlaps
   */
  rctf box;
  box.xmin = offset;
  box.xmax = v2d->cur.xmax;
  box.ymin = yminc + NLATRACK_SKIP;
  box.ymax = ymaxc + NLATRACK_SKIP - 1;
  UI_draw_roundbox_4fv(&box, true, 8, color);
}

/* name for nla action entries */
static void acf_nlaaction_name(bAnimListElem *ale, char *name)
{
  bAction *act = static_cast<bAction *>(ale->data);

  if (name) {
    if (act) {
      /* TODO: add special decoration when doing this in tweaking mode? */
      BLI_strncpy_utf8(name, act->id.name + 2, ANIM_CHAN_NAME_SIZE);
    }
    else {
      BLI_strncpy_utf8(name, IFACE_("<No Action>"), ANIM_CHAN_NAME_SIZE);
    }
  }
}

/* name property for nla action entries */
static bool acf_nlaaction_name_prop(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop)
{
  if (ale->data) {
    *r_ptr = RNA_pointer_create_discrete(ale->fcurve_owner_id, &RNA_Action, ale->data);
    *r_prop = RNA_struct_name_property(r_ptr->type);

    return (*r_prop != nullptr);
  }

  return false;
}

/* check if some setting exists for this track */
static bool acf_nlaaction_setting_valid(bAnimContext * /*ac*/,
                                        bAnimListElem *ale,
                                        eAnimChannel_Settings setting)
{
  AnimData *adt = ale->adt;

  /* visibility of settings depends on various states... */
  switch (setting) {
    /* conditionally supported */
    case ACHANNEL_SETTING_PINNED: /* pinned - map/unmap */
      if ((adt) && (adt->flag & ADT_NLA_EDIT_ON)) {
        /* This should only appear in tweak-mode. */
        return true;
      }
      else {
        return false;
      }
    case ACHANNEL_SETTING_SELECT: /* selected */
      return true;

      /* unsupported */
    default:
      return false;
  }
}

/* Get the appropriate flag(s) for the setting when it is valid. */
static int acf_nlaaction_setting_flag(bAnimContext * /*ac*/,
                                      eAnimChannel_Settings setting,
                                      bool *r_neg)
{
  /* Clear extra return data first. */
  *r_neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_PINNED: /* pinned - map/unmap */
      *r_neg = true;              /* XXX */
      return ADT_NLA_EDIT_NOMAP;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_nlaaction_setting_ptr(bAnimListElem *ale,
                                       eAnimChannel_Settings /*setting*/,
                                       short *r_type)
{
  AnimData *adt = ale->adt;
  return GET_ACF_FLAG_PTR(adt->flag, r_type);
}

/* nla action type define */
static bAnimChannelType ACF_NLAACTION = {
    /*channel_type_name*/ "NLA Active Action",
    /*channel_role*/ ACHANNEL_ROLE_CHANNEL,
    /* NOTE: the backdrop handles this too, since it needs special hacks. */
    /*get_backdrop_color*/ acf_nlaaction_color,
    /*get_channel_color*/ nullptr,

    /*draw_backdrop*/ acf_nlaaction_backdrop,
    /*get_indent_level*/ acf_generic_indentation_flexible,
    /*get_offset*/ acf_generic_group_offset, /* XXX? */

    /*name*/ acf_nlaaction_name,
    /*name_prop*/ acf_nlaaction_name_prop,
    /*icon*/ acf_nlaaction_icon,

    /*has_setting*/ acf_nlaaction_setting_valid,
    /*setting_flag*/ acf_nlaaction_setting_flag,
    /*setting_ptr*/ acf_nlaaction_setting_ptr,
    /*setting_post_update*/ nullptr,
};

/* *********************************************** */
/* Type Registration and General Access */

/* These globals only ever get directly accessed in this file */
static bAnimChannelType *animchannelTypeInfo[ANIMTYPE_NUM_TYPES];
static short ACF_INIT = 1; /* when non-zero, the list needs to be updated */

/* Initialize type info definitions */
static void ANIM_init_channel_typeinfo_data()
{
  int type = 0;

  /* start initializing if necessary... */
  if (ACF_INIT) {
    ACF_INIT = 0;

    /* NOTE: need to keep the order of these synchronized with the definition of
     * channel types (eAnim_ChannelType) in ED_anim_api.hh
     */
    animchannelTypeInfo[type++] = nullptr; /* None */
    animchannelTypeInfo[type++] = nullptr; /* AnimData */
    animchannelTypeInfo[type++] = nullptr; /* Special */

    animchannelTypeInfo[type++] = &ACF_SUMMARY; /* Motion Summary */

    animchannelTypeInfo[type++] = &ACF_SCENE;  /* Scene */
    animchannelTypeInfo[type++] = &ACF_OBJECT; /* Object */
    animchannelTypeInfo[type++] = &ACF_GROUP;  /* Group */
    animchannelTypeInfo[type++] = &ACF_FCURVE; /* F-Curve */

    animchannelTypeInfo[type++] = &ACF_NLACONTROLS; /* NLA Control FCurve Expander */
    animchannelTypeInfo[type++] = &ACF_NLACURVE;    /* NLA Control FCurve Channel */

    animchannelTypeInfo[type++] = &ACF_FILLANIM;    /* Object's Layered Action Expander */
    animchannelTypeInfo[type++] = &ACF_ACTION_SLOT; /* Action Slot Expander */
    animchannelTypeInfo[type++] = &ACF_FILLACTD;    /* Object Action Expander */
    animchannelTypeInfo[type++] = &ACF_FILLDRIVERS; /* Drivers Expander */

    animchannelTypeInfo[type++] = &ACF_DSMAT;        /* Material Channel */
    animchannelTypeInfo[type++] = &ACF_DSLIGHT;      /* Light Channel */
    animchannelTypeInfo[type++] = &ACF_DSCAM;        /* Camera Channel */
    animchannelTypeInfo[type++] = &ACF_DSCACHEFILE;  /* CacheFile Channel */
    animchannelTypeInfo[type++] = &ACF_DSCUR;        /* Curve Channel */
    animchannelTypeInfo[type++] = &ACF_DSSKEY;       /* ShapeKey Channel */
    animchannelTypeInfo[type++] = &ACF_DSWOR;        /* World Channel */
    animchannelTypeInfo[type++] = &ACF_DSNTREE;      /* NodeTree Channel */
    animchannelTypeInfo[type++] = &ACF_DSPART;       /* Particle Channel */
    animchannelTypeInfo[type++] = &ACF_DSMBALL;      /* MetaBall Channel */
    animchannelTypeInfo[type++] = &ACF_DSARM;        /* Armature Channel */
    animchannelTypeInfo[type++] = &ACF_DSMESH;       /* Mesh Channel */
    animchannelTypeInfo[type++] = &ACF_DSTEX;        /* Texture Channel */
    animchannelTypeInfo[type++] = &ACF_DSLAT;        /* Lattice Channel */
    animchannelTypeInfo[type++] = &ACF_DSLINESTYLE;  /* LineStyle Channel */
    animchannelTypeInfo[type++] = &ACF_DSSPK;        /* Speaker Channel */
    animchannelTypeInfo[type++] = &ACF_DSGPENCIL;    /* GreasePencil Channel */
    animchannelTypeInfo[type++] = &ACF_DSMCLIP;      /* MovieClip Channel */
    animchannelTypeInfo[type++] = &ACF_DSCURVES;     /* Curves Channel */
    animchannelTypeInfo[type++] = &ACF_DSPOINTCLOUD; /* PointCloud Channel */
    animchannelTypeInfo[type++] = &ACF_DSVOLUME;     /* Volume Channel */
    animchannelTypeInfo[type++] = &ACF_DSLIGHTPROBE; /* Light Probe */

    animchannelTypeInfo[type++] = &ACF_SHAPEKEY; /* ShapeKey */

    animchannelTypeInfo[type++] = &ACF_GPL_LEGACY; /* Grease Pencil Layer (Legacy) */

    animchannelTypeInfo[type++] = &ACF_GPD;      /* Grease Pencil Datablock. */
    animchannelTypeInfo[type++] = &ACF_GPLGROUP; /* Grease Pencil Layer Group. */
    animchannelTypeInfo[type++] = &ACF_GPL;      /* Grease Pencil Layer. */

    animchannelTypeInfo[type++] = &ACF_MASKDATA;  /* Mask Datablock */
    animchannelTypeInfo[type++] = &ACF_MASKLAYER; /* Mask Layer */

    animchannelTypeInfo[type++] = &ACF_NLATRACK;  /* NLA Track */
    animchannelTypeInfo[type++] = &ACF_NLAACTION; /* NLA Action */

    BLI_assert_msg(animchannelTypeInfo[ANIMTYPE_FILLACT_LAYERED] == &ACF_FILLANIM,
                   "ANIMTYPE_FILLACT_LAYERED does not match ACF_FILLANIM");
    BLI_assert_msg(animchannelTypeInfo[ANIMTYPE_ACTION_SLOT] == &ACF_ACTION_SLOT,
                   "ANIMTYPE_ACTION_SLOT does not match ACF_ACTION_SLOT");
  }
}

const bAnimChannelType *ANIM_channel_get_typeinfo(const bAnimListElem *ale)
{
  /* Sanity checks. */
  if (ale == nullptr) {
    return nullptr;
  }

  /* init the typeinfo if not available yet... */
  ANIM_init_channel_typeinfo_data();

  BLI_assert(ale->type < ANIMTYPE_NUM_TYPES);
  if (ale->type >= ANIMTYPE_NUM_TYPES) {
    return nullptr;
  }

  return animchannelTypeInfo[ale->type];
}

/* --------------------------- */

static blender::StringRefNull setting_name(const eAnimChannel_Settings setting)
{
  switch (setting) {
    case ACHANNEL_SETTING_SELECT:
      return "SELECT";
    case ACHANNEL_SETTING_PROTECT:
      return "PROTECT";
    case ACHANNEL_SETTING_MUTE:
      return "MUTE";
    case ACHANNEL_SETTING_EXPAND:
      return "EXPAND";
    case ACHANNEL_SETTING_VISIBLE:
      return "VISIBLE";
    case ACHANNEL_SETTING_SOLO:
      return "SOLO";
    case ACHANNEL_SETTING_PINNED:
      return "PINNED";
    case ACHANNEL_SETTING_MOD_OFF:
      return "MOD_OFF";
    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return "ALWAYS_VISIBLE";
  }

  return "unknown setting";
}

void ANIM_channel_debug_print_info(bAnimContext &ac, bAnimListElem *ale, short indent_level)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

  /* print indents */
  for (; indent_level > 0; indent_level--) {
    printf("  ");
  }

  /* print info */
  if (!ale) {
    printf("<Invalid channel - nullptr>\n");
    return;
  }

  if (!acf) {
    printf("ChanType: <Unknown - %d>\n", ale->type);
    return;
  }

  /* Get UI name. */
  char name[ANIM_CHAN_NAME_SIZE];
  if (acf->name) {
    acf->name(ale, name);
  }
  else {
    STRNCPY_UTF8(name, "<No name>");
  }

  printf("ChanType: <%-25s> Name: \"%s\"\n       ", acf->channel_type_name, name);

  /* Print settings. */
  blender::Vector<eAnimChannel_Settings> settings = {
      ACHANNEL_SETTING_SELECT,
      ACHANNEL_SETTING_PROTECT,
      ACHANNEL_SETTING_MUTE,
      ACHANNEL_SETTING_EXPAND,
      ACHANNEL_SETTING_VISIBLE,
      ACHANNEL_SETTING_SOLO,
      ACHANNEL_SETTING_PINNED,
      ACHANNEL_SETTING_MOD_OFF,
      ACHANNEL_SETTING_ALWAYS_VISIBLE,
  };
  for (const eAnimChannel_Settings setting : settings) {
    if (!acf->has_setting(&ac, ale, setting)) {
      continue;
    }

    bool is_neg = false;
    const int setting_flag = acf->setting_flag(&ac, setting, &is_neg);

    short setting_type = 0;
    const void *setting_ptr = acf->setting_ptr(ale, setting, &setting_type);

    bool setting_value = false;
    switch (setting_type) {
      case sizeof(int): {
        const int as_int = *static_cast<const int *>(setting_ptr);
        setting_value = bool(as_int & setting_flag) != is_neg;
        break;
      }
      case sizeof(short): {
        const short as_short = *static_cast<const short *>(setting_ptr);
        setting_value = bool(as_short & short(setting_flag)) != is_neg;
        break;
      }
      default:
        BLI_assert_unreachable();
    }

    std::string show_name = setting_name(setting);
    if (!setting_value) {
      BLI_str_tolower_ascii(show_name.data(), show_name.length());
    }
    printf("%-15s", show_name.c_str());
  }
  printf("\n");
}

bAction *ANIM_channel_action_get(const bAnimListElem *ale)
{
  if (ale->datatype == ALE_ACT) {
    return static_cast<bAction *>(ale->key_data);
  }

  if (ELEM(ale->type, ANIMTYPE_GROUP, ANIMTYPE_FCURVE)) {
    ID *owner = ale->fcurve_owner_id;

    if (owner && GS(owner->name) == ID_AC) {
      return reinterpret_cast<bAction *>(owner);
    }
  }

  return nullptr;
}

/* --------------------------- */

short ANIM_channel_setting_get(bAnimContext *ac, bAnimListElem *ale, eAnimChannel_Settings setting)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

  /* 1) check that the setting exists for the current context */
  if ((acf) && (!acf->has_setting || acf->has_setting(ac, ale, setting))) {
    /* 2) get pointer to check for flag in, and the flag to check for */
    short ptrsize;
    bool negflag;
    int flag;
    void *ptr;

    flag = acf->setting_flag(ac, setting, &negflag);
    ptr = acf->setting_ptr(ale, setting, &ptrsize);

    /* check if flag is enabled */
    if (ptr && flag) {
      switch (ptrsize) {
        case sizeof(int): /* integer pointer for setting */
        {
          const int *val = (int *)ptr;

          if (negflag) {
            return ((*val) & flag) == 0;
          }
          return ((*val) & flag) != 0;
        }
        case sizeof(short): /* short pointer for setting */
        {
          const short *val = (short *)ptr;

          if (negflag) {
            return ((*val) & flag) == 0;
          }
          return ((*val) & flag) != 0;
        }
        case sizeof(char): /* char pointer for setting */
        {
          const char *val = (char *)ptr;

          if (negflag) {
            return ((*val) & flag) == 0;
          }
          return ((*val) & flag) != 0;
        }
      }
    }
  }

  /* not found... */
  return -1;
}

/* Quick macro for use in ANIM_channel_setting_set -
 * set flag for setting according the mode given. */
#define ACF_SETTING_SET(sval, sflag, smode) \
  { \
    if (negflag) { \
      if (smode == ACHANNEL_SETFLAG_INVERT) { \
        (sval) ^= (sflag); \
      } \
      else if (smode == ACHANNEL_SETFLAG_ADD) { \
        (sval) &= ~(sflag); \
      } \
      else { \
        (sval) |= (sflag); \
      } \
    } \
    else { \
      if (smode == ACHANNEL_SETFLAG_INVERT) { \
        (sval) ^= (sflag); \
      } \
      else if (smode == ACHANNEL_SETFLAG_ADD) { \
        (sval) |= (sflag); \
      } \
      else { \
        (sval) &= ~(sflag); \
      } \
    } \
  } \
  (void)0

void ANIM_channel_setting_set(bAnimContext *ac,
                              bAnimListElem *ale,
                              eAnimChannel_Settings setting,
                              eAnimChannels_SetFlag mode)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

  /* 1) check that the setting exists for the current context */
  if ((acf) && (!acf->has_setting || acf->has_setting(ac, ale, setting))) {
    /* 2) get pointer to check for flag in, and the flag to check for */
    short ptrsize;
    bool negflag;
    int flag;
    void *ptr;

    flag = acf->setting_flag(ac, setting, &negflag);
    ptr = acf->setting_ptr(ale, setting, &ptrsize);

    /* check if flag is enabled */
    if (ptr && flag) {
      switch (ptrsize) {
        case sizeof(int): /* integer pointer for setting */
        {
          int *val = (int *)ptr;
          ACF_SETTING_SET(*val, flag, mode);
          break;
        }
        case sizeof(short): /* short pointer for setting */
        {
          short *val = (short *)ptr;
          ACF_SETTING_SET(*val, flag, mode);
          break;
        }
        case sizeof(char): /* char pointer for setting */
        {
          char *val = (char *)ptr;
          ACF_SETTING_SET(*val, flag, mode);
          break;
        }
      }

      if (acf->setting_post_update) {
        BLI_assert(ale);
        BLI_assert(ac->bmain);
        acf->setting_post_update(*ac->bmain, *ale, setting);
      }
    }
  }
}

/* --------------------------- */

/** Size of icons. */
#define ICON_WIDTH (0.85f * U.widget_unit)
/** Width of sliders. */
#define SLIDER_WIDTH (4 * U.widget_unit)
/** Min-width of rename text-boxes. */
#define RENAME_TEXT_MIN_WIDTH (U.widget_unit)
/** Width of graph editor color bands. */
#define GRAPH_COLOR_BAND_WIDTH (0.3f * U.widget_unit)
/** Extra offset for the visibility icons in the graph editor. */
#define GRAPH_ICON_VISIBILITY_OFFSET (GRAPH_COLOR_BAND_WIDTH * 1.5f)

#define CHANNEL_COLOR_RECT_WIDTH (0.5f * ICON_WIDTH)
#define CHANNEL_COLOR_RECT_MARGIN (2.0f * UI_SCALE_FAC)

/* Helper - Check if a channel needs renaming */
static bool achannel_is_being_renamed(const bAnimContext *ac,
                                      const bAnimChannelType *acf,
                                      size_t channel_index)
{
  if (acf->name_prop && ac->ads) {
    /* if rename index matches, this channel is being renamed */
    if (ac->ads->renameIndex == channel_index + 1) {
      return true;
    }
  }

  /* not being renamed */
  return false;
}

/**
 * Check if the animation channel is an item that either is or belongs to a
 * disconnected action slot.
 */
static bool achannel_is_part_of_disconnected_slot(const bAnimListElem *ale)
{
  BLI_assert(ale->bmain != nullptr);
  if (ale->bmain == nullptr) {
    return false;
  }

  switch (ale->type) {
    case ANIMTYPE_ACTION_SLOT: {
      const animrig::Slot &slot = static_cast<const ActionSlot *>(ale->data)->wrap();

      return slot.users(*ale->bmain).is_empty();
    }

    case ANIMTYPE_GROUP:
    case ANIMTYPE_FCURVE: {
      if (ale->fcurve_owner_id == nullptr || GS(ale->fcurve_owner_id->name) != ID_AC) {
        return false;
      }

      const animrig::Action &action =
          reinterpret_cast<const bAction *>(ale->fcurve_owner_id)->wrap();
      if (action.is_action_legacy()) {
        return false;
      }

      const animrig::Slot *slot = action.slot_for_handle(ale->slot_handle);
      if (slot == nullptr) {
        return false;
      }

      return slot->users(*ale->bmain).is_empty();
    }

    /* No other types are currently drawn as children of action slots. */
    default:
      return false;
  }
}

/** Check if the animation channel name should be underlined in red due to errors. */
static bool achannel_is_broken(const bAnimListElem *ale)
{
  switch (ale->type) {
    case ANIMTYPE_FCURVE:
    case ANIMTYPE_NLACURVE: {
      const FCurve *fcu = static_cast<const FCurve *>(ale->data);

      /* The channel is disabled (has a bad rna path), or it's a driver that failed to evaluate. */
      return (fcu->flag & FCURVE_DISABLED) ||
             (fcu->driver != nullptr && (fcu->driver->flag & DRIVER_FLAG_INVALID));
    }
    default:
      return false;
  }
}

float ANIM_UI_get_keyframe_scale_factor()
{
  bTheme *btheme = UI_GetTheme();
  const float yscale_fac = btheme->space_action.keyframe_scale_fac;

  /* clamp to avoid problems with uninitialized values... */
  if (yscale_fac < 0.1f) {
    return 1.0f;
  }
  return yscale_fac;
}

float ANIM_UI_get_channel_height()
{
  return 0.8f * ANIM_UI_get_keyframe_scale_factor() * U.widget_unit;
}

float ANIM_UI_get_channel_skip()
{
  return 0.1f * U.widget_unit;
}

float ANIM_UI_get_first_channel_top(View2D *v2d)
{
  return UI_view2d_scale_get_y(v2d) * -UI_TIME_SCRUB_MARGIN_Y - ANIM_UI_get_channel_skip();
}

float ANIM_UI_get_channel_step()
{
  return ANIM_UI_get_channel_height() + ANIM_UI_get_channel_skip();
}

float ANIM_UI_get_channels_total_height(View2D *v2d, const int item_count)
{
  return -ANIM_UI_get_first_channel_top(v2d) + ANIM_UI_get_channel_step() * (item_count + 1);
}

float ANIM_UI_get_channel_name_width()
{
  return 10 * U.widget_unit;
}

float ANIM_UI_get_channel_button_width()
{
  return 0.8f * U.widget_unit;
}

void ANIM_channel_draw(
    bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc, size_t channel_index)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short selected, offset;
  float y, ymid, ytext;

  /* sanity checks - don't draw anything */
  if (ELEM(nullptr, acf, ale)) {
    return;
  }

  /* get initial offset */
  if (acf->get_offset) {
    offset = acf->get_offset(ac, ale);
  }
  else {
    offset = 0;
  }

  /* calculate appropriate y-coordinates for icon buttons */
  y = (ymaxc - yminc) / 2 + yminc;
  ymid = y - 0.5f * ICON_WIDTH;
  /* y-coordinates for text is only 4 down from middle */
  ytext = y - 0.2f * U.widget_unit;

  /* check if channel is selected */
  if (acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT)) {
    selected = ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT);
  }
  else {
    selected = 0;
  }

  /* set blending again, as may not be set in previous step */
  GPU_blend(GPU_BLEND_ALPHA);

  /* step 1) draw backdrop ........................................... */
  if (acf->draw_backdrop) {
    acf->draw_backdrop(ac, ale, yminc, ymaxc);
  }

  /* step 2) draw expand widget ....................................... */
  if (acf->has_setting(ac, ale, ACHANNEL_SETTING_EXPAND)) {
    /* just skip - drawn as widget now */
    offset += ICON_WIDTH;
  }
  else if (!ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
    /* A bit of padding when there is no expand widget. */
    offset += short(0.2f * U.widget_unit);
  }

  /* step 3) draw icon ............................................... */
  if (acf->icon) {
    UI_icon_draw(offset, ymid, acf->icon(ale));
    offset += ICON_WIDTH;
  }

  /* step 4) draw special toggles  .................................
   * - in Graph Editor, check-boxes for visibility in curves area
   * - in NLA Editor, glowing dots for solo/not solo...
   * - in Grease Pencil mode, color swatches for layer color
   */
  if (ac->sl) {
    if (ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH) &&
        (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE) ||
         acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE)) &&
        !ELEM(ale->type,
              ANIMTYPE_GPLAYER,
              ANIMTYPE_DSGPENCIL,
              ANIMTYPE_GREASE_PENCIL_LAYER,
              ANIMTYPE_GREASE_PENCIL_LAYER_GROUP))
    {
      /* for F-Curves, draw color-preview of curve left to the visibility icon */
      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        FCurve *fcu = static_cast<FCurve *>(ale->data);
        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

        /* F-Curve channels need to have a special 'color code' box drawn,
         * which is colored with whatever color the curve has stored.
         */

        /* If the curve is hidden, make the rect less opaque. */
        float rect_alpha = (fcu->flag & FCURVE_VISIBLE) ? 1 : 0.3f;
        immUniformColor3fvAlpha(fcu->color, rect_alpha);

        immRectf(pos, offset, yminc, offset + GRAPH_COLOR_BAND_WIDTH, ymaxc);
        immUnbindProgram();
      }

      /* turn off blending, since not needed anymore... */
      GPU_blend(GPU_BLEND_NONE);

      /* icon is drawn as widget now... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE)) {
        if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
          offset += ICON_WIDTH + GRAPH_ICON_VISIBILITY_OFFSET;
        }
        else {
          offset += ICON_WIDTH;
        }
      }
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE)) {
        offset += ICON_WIDTH;
      }
    }
  }

  /* step 5) draw name ............................................... */
  /* Don't draw this if renaming... */
  if (acf->name && !achannel_is_being_renamed(ac, acf, channel_index)) {
    const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
    char name[ANIM_CHAN_NAME_SIZE]; /* hopefully this will be enough! */
    uchar col[4];

    /* set text color */
    /* XXX: if active, highlight differently? */

    if (selected) {
      UI_GetThemeColor4ubv(TH_TEXT_HI, col);
    }
    else {
      UI_GetThemeColor4ubv(TH_TEXT, col);
    }

    /* Gray out disconnected action slots and their children. */
    if (!selected && achannel_is_part_of_disconnected_slot(ale)) {
      col[3] = col[3] / 3 * 2;
    }

    /* get name */
    acf->name(ale, name);

    offset += 3;
    UI_fontstyle_draw_simple(fstyle, offset, ytext, name, col);

    /* draw red underline if channel is disabled */
    if (achannel_is_broken(ale)) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      /* FIXME: replace hardcoded color here, and check on extents! */
      immUniformColor3f(1.0f, 0.0f, 0.0f);

      GPU_line_width(2.0f);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, float(offset), yminc);
      immVertex2f(pos, v2d->cur.xmax, yminc);
      immEnd();

      immUnbindProgram();
    }
  }

  /* step 6) draw backdrops behind mute+protection toggles + (sliders) ....................... */
  /*  - Reset offset - now goes from RHS of panel.
   *  - Exception for graph editor, which needs extra space for the scroll bar.
   */
  if (ac->spacetype == SPACE_GRAPH &&
      ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE, ANIMTYPE_GROUP))
  {
    offset = V2D_SCROLL_WIDTH;
  }
  else {
    offset = 0;
  }

  /* TODO: when drawing sliders, make those draw instead of these toggles if not enough space */

  if (v2d) {
    short draw_sliders = 0;
    float ymin_ofs = 0.0f;
    float color[3];
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    /* get and set backdrop color */
    acf->get_backdrop_color(ac, ale, color);
    immUniformColor3fv(color);

    /* check if we need to show the sliders */
    if ((ac->sl) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH)) {
      switch (ac->spacetype) {
        case SPACE_ACTION: {
          SpaceAction *saction = reinterpret_cast<SpaceAction *>(ac->sl);
          draw_sliders = (saction->flag & SACTION_SLIDERS);
          break;
        }
        case SPACE_GRAPH: {
          SpaceGraph *sipo = reinterpret_cast<SpaceGraph *>(ac->sl);
          draw_sliders = (sipo->flag & SIPO_SLIDERS);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
    }

    /* check if there's enough space for the toggles if the sliders are drawn too */
    if (!(draw_sliders) || (BLI_rcti_size_x(&v2d->mask) > ANIM_UI_get_channel_button_width() / 2))
    {
      /* NOTE: The comments here match the comments in ANIM_channel_draw_widgets(), as that
       * function and this one are strongly coupled. */

      /* Little channel color rectangle. */
      if (acf_show_channel_colors()) {
        offset += CHANNEL_COLOR_RECT_WIDTH + 2 * CHANNEL_COLOR_RECT_MARGIN;
      }

      /* solo... */
      if ((ac->spacetype == SPACE_NLA) && acf->has_setting(ac, ale, ACHANNEL_SETTING_SOLO)) {
        /* A touch of padding because the star icon is so wide. */
        offset += short(1.2f * ICON_WIDTH);
      }

      /* protect... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PROTECT)) {
        offset += ICON_WIDTH;
      }

      /* mute... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MUTE)) {
        offset += ICON_WIDTH;
      }

      /* grease pencil visibility... */
      if (ELEM(ale->type,
               ANIMTYPE_GPLAYER,
               ANIMTYPE_GREASE_PENCIL_LAYER,
               ANIMTYPE_GREASE_PENCIL_LAYER_GROUP))
      {
        offset += ICON_WIDTH;
      }

      /* modifiers toggle... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MOD_OFF)) {
        offset += ICON_WIDTH;
      }

      /* pinned... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PINNED)) {
        offset += ICON_WIDTH;
      }

      /* NOTE: technically, NLA Action "pushdown" should be here too,
       * but there are no sliders there. */

      /* NLA action tracks have slightly different spacing requirements... */
      if (ale->type == ANIMTYPE_NLAACTION) {
        ymin_ofs = NLATRACK_SKIP;
      }
    }

    /* Draw slider:
     * - Even if we can draw sliders for this view,
     *   we must also check that the channel-type supports them
     *   (only F-Curves really can support them for now).
     * - Slider should start before the toggles (if they're visible)
     *   to keep a clean line down the side.
     */
    if ((draw_sliders) && ELEM(ale->type,
                               ANIMTYPE_FCURVE,
                               ANIMTYPE_NLACURVE,
                               ANIMTYPE_SHAPEKEY,
                               ANIMTYPE_GPLAYER,
                               ANIMTYPE_GREASE_PENCIL_LAYER,
                               ANIMTYPE_GREASE_PENCIL_LAYER_GROUP))
    {
      /* adjust offset */
      offset += SLIDER_WIDTH;
    }

    /* Finally draw a backdrop rect behind these:
     * - Starts from the point where the first toggle/slider starts.
     * - Ends past the space that might be reserved for a scroller.
     */
    immRectf(pos,
             v2d->cur.xmax - float(offset),
             yminc + ymin_ofs,
             v2d->cur.xmax + EXTRA_SCROLL_PAD,
             ymaxc);

    immUnbindProgram();
  }
}

/* ------------------ */

/* callback for (normal) widget settings - send notifiers */
static void achannel_setting_widget_cb(bContext *C, void *ale_npoin, void *setting_wrap)
{
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  const bAnimListElem *ale_setting = static_cast<bAnimListElem *>(ale_npoin);
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale_setting);
  if (!acf) {
    /* Any channel with settings should have a type, because it is the type that
     * tells the system which settings are supported. */
    BLI_assert_unreachable();
    return;
  }

  /* When the button in the UI changes the setting, it does NOT call `ANIM_channel_setting_set()`,
   * but actually manipulates the data directly via a pointer (see `ui_but_value_set()` in
   * `source/blender/editors/interface/interface.cc`).
   *
   * As a result, `setting_post_update()` was not called yet and we need to call it here. */
  if (acf->setting_post_update) {
    const eAnimChannel_Settings setting = eAnimChannel_Settings(POINTER_AS_INT(setting_wrap));
    Main *bmain = CTX_data_main(C);

    BLI_assert(ale_setting);
    BLI_assert(bmain);
    acf->setting_post_update(*bmain, *ale_setting, setting);
  }
}

/* callback for widget settings that need flushing */
static void achannel_setting_flush_widget_cb(bContext *C, void *ale_npoin, void *setting_wrap)
{
  bAnimListElem *ale_setting = static_cast<bAnimListElem *>(ale_npoin);
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  const eAnimChannel_Settings setting = eAnimChannel_Settings(POINTER_AS_INT(setting_wrap));
  short on = 0;

  /* Before flushing, just do the regular notification & callback. */
  achannel_setting_widget_cb(C, ale_npoin, setting_wrap);

  /* verify that we have a channel to operate on. */
  if (!ale_setting) {
    return;
  }
  if (ELEM(ale_setting->type, ANIMTYPE_GREASE_PENCIL_LAYER, ANIMTYPE_GREASE_PENCIL_LAYER_GROUP)) {
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  if (ale_setting->type == ANIMTYPE_GPLAYER) {
    /* draw cache updates for settings that affect the visible strokes */
    if (setting == ACHANNEL_SETTING_VISIBLE) {
      bGPdata *gpd = reinterpret_cast<bGPdata *>(ale_setting->id);
      DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }

    /* UI updates */
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  /* Tag for full animation update, so that the settings will have an effect. */
  if (ale_setting->id) {
    DEG_id_tag_update(ale_setting->id, ID_RECALC_ANIMATION);
  }
  if (ale_setting->adt && ale_setting->adt->action) {
    /* Action is its own datablock, so has to be tagged specifically. */
    DEG_id_tag_update(&ale_setting->adt->action->id, ID_RECALC_ANIMATION);
  }

  /* verify animation context */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  /* Don't flush setting changes to grease pencil layers in a layer group. */
  if (ale_setting->type == ANIMTYPE_GREASE_PENCIL_LAYER_GROUP) {
    return;
  }

  /* check if the setting is on... */
  on = ANIM_channel_setting_get(&ac, ale_setting, eAnimChannel_Settings(setting));

  /* on == -1 means setting not found... */
  if (on == -1) {
    return;
  }

  /* get all channels that can possibly be chosen - but ignore hierarchy */
  filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS;
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* call API method to flush the setting */
  ANIM_flush_setting_anim_channels(
      &ac, &anim_data, ale_setting, eAnimChannel_Settings(setting), eAnimChannels_SetFlag(on));

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);
}

/* callback for wrapping NLA Track "solo" toggle logic */
static void achannel_nlatrack_solo_widget_cb(bContext *C, void *ale_poin, void *setting_wrap)
{
  bAnimListElem *ale = static_cast<bAnimListElem *>(ale_poin);
  AnimData *adt = ale->adt;
  NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);

  /* Before the special handling, just do the regular notification & callback. */
  achannel_setting_widget_cb(C, ale_poin, setting_wrap);

  /* Toggle 'solo' mode. There are several complications here which need explaining:
   * - The method call is needed to perform a few additional validation operations
   *   to ensure that the mode is applied properly
   * - BUT, since the button already toggles the value, we need to un-toggle it
   *   before the API call gets to it, otherwise it will end up clearing the result
   *   again!
   */
  nlt->flag ^= NLATRACK_SOLO;
  BKE_nlatrack_solo_toggle(adt, nlt);

  DEG_id_tag_update(ale->id, ID_RECALC_ANIMATION);
}

/* callback for widget sliders - insert keyframes */
static void achannel_setting_slider_cb(bContext *C, void *id_poin, void *fcu_poin)
{
  ID *id = static_cast<ID *>(id_poin);
  AnimData *adt = BKE_animdata_from_id(id);
  FCurve *fcu = static_cast<FCurve *>(fcu_poin);

  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ToolSettings *ts = scene->toolsettings;
  ListBase nla_cache = {nullptr, nullptr};
  PointerRNA ptr;
  PropertyRNA *prop;
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;
  bool done = false;
  float cfra;

  /* Get RNA pointer */
  PointerRNA id_ptr = RNA_id_pointer_create(id);

  /* Get NLA context for value remapping */
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, float(scene->r.cfra));
  NlaKeyframingContext *nla_context = BKE_animsys_get_nla_keyframing_context(
      &nla_cache, &id_ptr, adt, &anim_eval_context);

  /* get current frame and apply NLA-mapping to it (if applicable) */
  cfra = BKE_nla_tweakedit_remap(adt, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);

  /* Get flags for keyframing. */
  flag = blender::animrig::get_keyframing_flags(scene);

  /* try to resolve the path stored in the F-Curve */
  if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
    /* set the special 'replace' flag if on a keyframe */
    if (blender::animrig::fcurve_frame_has_keyframe(fcu, cfra)) {
      flag |= INSERTKEY_REPLACE;
    }

    /* insert a keyframe for this F-Curve */
    done = blender::animrig::insert_keyframe_direct(reports,
                                                    ptr,
                                                    prop,
                                                    fcu,
                                                    &anim_eval_context,
                                                    eBezTriple_KeyframeType(ts->keyframe_type),
                                                    nla_context,
                                                    flag);

    if (done) {
      if (adt->action != nullptr) {
        DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
      }
      DEG_id_tag_update(id, ID_RECALC_ANIMATION_NO_FLUSH);
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
    }
  }

  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
}

/* callback for shapekey widget sliders - insert keyframes */
static void achannel_setting_slider_shapekey_cb(bContext *C, void *key_poin, void *kb_poin)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, float(scene->r.cfra));

  Key *key = static_cast<Key *>(key_poin);
  KeyBlock *kb = static_cast<KeyBlock *>(kb_poin);
  PointerRNA id_ptr = RNA_id_pointer_create(reinterpret_cast<ID *>(key));
  std::optional<std::string> rna_path = BKE_keyblock_curval_rnapath_get(key, kb);

  /* Since this is only ever called from moving a slider for an existing
   * shapekey in the shapekey animation editor, it shouldn't be possible for the
   * RNA path to fail to resolve. */
  BLI_assert(rna_path.has_value());

  /* TODO: should this use the flags from user settings? For now leaving as-is,
   * since it was already this way and might have a reason for it. This is
   * basically a UX question about how the shape key animation editor should
   * behave. */
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

  animrig::insert_keyframes(bmain,
                            &id_ptr,
                            std::nullopt,
                            {{*rna_path}},
                            std::nullopt,
                            anim_eval_context,
                            eBezTriple_KeyframeType(scene->toolsettings->keyframe_type),
                            flag);
}

/* callback for NLA Control Curve widget sliders - insert keyframes */
static void achannel_setting_slider_nla_curve_cb(bContext *C, void * /*id_poin*/, void *fcu_poin)
{
  // ID *id = (ID *)id_poin;
  FCurve *fcu = static_cast<FCurve *>(fcu_poin);

  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;
  bool done = false;
  float cfra;

  /* get current frame - *no* NLA mapping should be done */
  cfra = float(scene->r.cfra);

  /* get flags for keyframing */
  flag = blender::animrig::get_keyframing_flags(scene);

  /* Get pointer and property from the slider -
   * this should all match up with the NlaStrip required. */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (fcu && prop) {
    /* set the special 'replace' flag if on a keyframe */
    if (blender::animrig::fcurve_frame_has_keyframe(fcu, cfra)) {
      flag |= INSERTKEY_REPLACE;
    }

    /* insert a keyframe for this F-Curve */
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                      cfra);
    done = blender::animrig::insert_keyframe_direct(reports,
                                                    ptr,
                                                    prop,
                                                    fcu,
                                                    &anim_eval_context,
                                                    eBezTriple_KeyframeType(ts->keyframe_type),
                                                    nullptr,
                                                    flag);

    if (done) {
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
    }
  }
}

/* Draw a widget for some setting */
static void draw_setting_widget(bAnimContext *ac,
                                bAnimListElem *ale,
                                const bAnimChannelType *acf,
                                uiBlock *block,
                                const int xpos,
                                const int ypos,
                                const eAnimChannel_Settings setting)
{
  bool usetoggle = true;
  int icon;
  std::optional<StringRef> tooltip;

  /* get the flag and the pointer to that flag */
  bool negflag;
  const int flag = acf->setting_flag(ac, setting, &negflag);

  short ptrsize;
  void *ptr = acf->setting_ptr(ale, setting, &ptrsize);

  if (!ptr || !flag) {
    return;
  }

  const bool enabled = ANIM_channel_setting_get(ac, ale, setting);

  /* get the base icon for the setting */
  switch (setting) {
    case ACHANNEL_SETTING_VISIBLE: /* visibility eyes */
      // icon = (enabled ? ICON_HIDE_OFF : ICON_HIDE_ON);
      icon = ICON_HIDE_ON;

      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        tooltip = TIP_("F-Curve visibility in Graph Editor");
      }
      else if (ELEM(ale->type, ANIMTYPE_GPLAYER, ANIMTYPE_GREASE_PENCIL_LAYER)) {
        tooltip = TIP_("Grease Pencil layer is visible in the viewport");
      }
      else {
        tooltip = TIP_("Toggle visibility of Channels in Graph Editor for editing");
      }
      break;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      icon = ICON_UNPINNED;
      tooltip = TIP_("Display channel regardless of object selection");
      break;

    case ACHANNEL_SETTING_MOD_OFF: /* modifiers disabled */
      icon = ICON_MODIFIER_OFF;
      tooltip = TIP_("Enable F-Curve modifiers");
      break;

    case ACHANNEL_SETTING_EXPAND: /* expanded triangle */
      // icon = (enabled ? ICON_TRIA_DOWN : ICON_TRIA_RIGHT);
      icon = ICON_RIGHTARROW;
      tooltip = TIP_("Make channels grouped under this channel visible");
      break;

    case ACHANNEL_SETTING_SOLO: /* NLA Tracks only */
      // icon = (enabled ? ICON_SOLO_OFF : ICON_SOLO_ON);
      icon = ICON_SOLO_OFF;
      tooltip = TIP_(
          "NLA Track is the only one evaluated in this animation data-block, with all others "
          "muted");
      break;

      /* --- */

    case ACHANNEL_SETTING_PROTECT: /* protected lock */
      /* TODO: what about when there's no protect needed? */
      // icon = (enabled ? ICON_LOCKED : ICON_UNLOCKED);
      icon = ICON_UNLOCKED;

      if (ale->datatype != ALE_NLASTRIP) {
        tooltip = TIP_("Editability of keyframes for this channel");
      }
      else {
        tooltip = TIP_("Editability of NLA Strips in this track");
      }
      break;

    case ACHANNEL_SETTING_MUTE: /* muted speaker */
      icon = (enabled ? ICON_CHECKBOX_DEHLT : ICON_CHECKBOX_HLT);
      usetoggle = false;

      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        tooltip = TIP_("Does F-Curve contribute to result");
      }
      else if ((ac) && (ac->spacetype == SPACE_NLA) && (ale->type != ANIMTYPE_NLATRACK)) {
        tooltip = TIP_(
            "Temporarily disable NLA stack evaluation (i.e. only the active action is evaluated)");
      }
      else if (ELEM(ale->type, ANIMTYPE_GPLAYER, ANIMTYPE_GREASE_PENCIL_LAYER)) {
        tooltip = TIP_(
            "Show all keyframes during animation playback and enable all frames for editing "
            "(uncheck to use only the current keyframe during animation playback and editing)");
      }
      else {
        tooltip = TIP_("Do channels contribute to result (toggle channel muting)");
      }
      break;

    case ACHANNEL_SETTING_PINNED: /* pin icon */
      // icon = (enabled ? ICON_PINNED : ICON_UNPINNED);
      icon = ICON_UNPINNED;

      if (ale->type == ANIMTYPE_NLAACTION) {
        tooltip = TIP_("Display action without any time remapping (when unpinned)");
      }
      else {
        /* TODO: there are no other tools which require the 'pinning' concept yet */
        tooltip = std::nullopt;
      }
      break;

    default:
      tooltip = std::nullopt;
      icon = 0;
      break;
  }

  /* type of button */
  ButType butType;
  if (usetoggle) {
    if (negflag) {
      butType = ButType::IconToggleN;
    }
    else {
      butType = ButType::IconToggle;
    }
  }
  else {
    if (negflag) {
      butType = ButType::ToggleN;
    }
    else {
      butType = ButType::Toggle;
    }
  }

  /* draw button for setting */
  uiBut *but = nullptr;
  switch (ptrsize) {
    case sizeof(int): /* integer pointer for setting */
      but = uiDefIconButBitI(block,
                             butType,
                             flag,
                             0,
                             icon,
                             xpos,
                             ypos,
                             ICON_WIDTH,
                             ICON_WIDTH,
                             static_cast<int *>(ptr),
                             0,
                             0,
                             tooltip);
      break;

    case sizeof(short): /* short pointer for setting */
      but = uiDefIconButBitS(block,
                             butType,
                             flag,
                             0,
                             icon,
                             xpos,
                             ypos,
                             ICON_WIDTH,
                             ICON_WIDTH,
                             static_cast<short *>(ptr),
                             0,
                             0,
                             tooltip);
      break;

    case sizeof(char): /* char pointer for setting */
      but = uiDefIconButBitC(block,
                             butType,
                             flag,
                             0,
                             icon,
                             xpos,
                             ypos,
                             ICON_WIDTH,
                             ICON_WIDTH,
                             static_cast<char *>(ptr),
                             0,
                             0,
                             tooltip);
      break;
  }
  if (!but) {
    return;
  }

  /* Set callback to send relevant notifiers and/or perform type-specific updates */
  {
    uiButHandleNFunc button_callback;
    switch (setting) {
      /* Settings needing flushing up/down hierarchy. */
      case ACHANNEL_SETTING_VISIBLE: /* Graph Editor - "visibility" toggles. */
      case ACHANNEL_SETTING_PROTECT: /* General - protection flags. */
      case ACHANNEL_SETTING_MUTE:    /* General - muting flags. */
      case ACHANNEL_SETTING_PINNED:  /* NLA Actions - "map/no-map". */
      case ACHANNEL_SETTING_MOD_OFF:
      case ACHANNEL_SETTING_ALWAYS_VISIBLE:
        button_callback = achannel_setting_flush_widget_cb;
        break;

      /* settings needing special attention */
      case ACHANNEL_SETTING_SOLO: /* NLA Tracks - Solo toggle */
        button_callback = achannel_nlatrack_solo_widget_cb;
        break;

      /* no flushing */
      case ACHANNEL_SETTING_EXPAND: /* expanding - cannot flush,
                                     * otherwise all would open/close at once */
      default:
        button_callback = achannel_setting_widget_cb;
        break;
    }
    UI_but_funcN_set(but, button_callback, MEM_dupallocN(ale), POINTER_FROM_INT(setting));
  }

  if ((ale->fcurve_owner_id != nullptr && !BKE_id_is_editable(ac->bmain, ale->fcurve_owner_id)) ||
      (ale->fcurve_owner_id == nullptr && ale->id != nullptr &&
       !BKE_id_is_editable(ac->bmain, ale->id)))
  {
    if (setting != ACHANNEL_SETTING_EXPAND) {
      UI_but_disable(but, "Cannot edit this property from a linked data-block");
    }
  }

  /* Post-button-creation modifications of the button. */
  switch (setting) {
    case ACHANNEL_SETTING_MOD_OFF:
      /* Deactivate the button when there are no FCurve modifiers. */
      if (ale->datatype == ALE_FCURVE) {
        const FCurve *fcu = static_cast<const FCurve *>(ale->key_data);
        if (BLI_listbase_is_empty(&fcu->modifiers)) {
          UI_but_flag_enable(but, UI_BUT_INACTIVE);
        }
      }
      break;

    default:
      break;
  }
}

static void draw_grease_pencil_layer_widgets(bAnimListElem *ale,
                                             uiBlock *block,
                                             const rctf *rect,
                                             short &offset,
                                             const short channel_height,
                                             const int array_index)
{
  using namespace blender::bke::greasepencil;
  Layer *layer = static_cast<Layer *>(ale->data);

  if (layer == nullptr) {
    return;
  }

  /* Reset slider offset, in order to add special grease pencil icons. */
  offset += SLIDER_WIDTH;

  /* Create the RNA pointers. */
  PointerRNA ptr = RNA_pointer_create_discrete(ale->id, &RNA_GreasePencilLayer, ale->data);
  PointerRNA id_ptr = RNA_id_pointer_create(ale->id);

  /* Layer onion skinning switch. */
  offset -= ICON_WIDTH;
  UI_block_emboss_set(block, blender::ui::EmbossType::None);
  PropertyRNA *onion_skinning_prop = RNA_struct_find_property(&ptr, "use_onion_skinning");

  const std::optional<std::string> onion_skinning_rna_path = RNA_path_from_ID_to_property(
      &ptr, onion_skinning_prop);
  if (RNA_path_resolve_property(
          &id_ptr, onion_skinning_rna_path->c_str(), &ptr, &onion_skinning_prop))
  {
    uiDefAutoButR(block,
                  &ptr,
                  onion_skinning_prop,
                  array_index,
                  "",
                  ICON_ONIONSKIN_OFF,
                  offset,
                  rect->ymin,
                  ICON_WIDTH,
                  channel_height);
  }

  /* Mask layer. */
  offset -= ICON_WIDTH;
  UI_block_emboss_set(block, blender::ui::EmbossType::None);
  PropertyRNA *layer_mask_prop = RNA_struct_find_property(&ptr, "use_masks");

  const std::optional<std::string> layer_mask_rna_path = RNA_path_from_ID_to_property(
      &ptr, layer_mask_prop);
  if (RNA_path_resolve_property(&id_ptr, layer_mask_rna_path->c_str(), &ptr, &layer_mask_prop)) {
    uiDefAutoButR(block,
                  &ptr,
                  layer_mask_prop,
                  array_index,
                  "",
                  ICON_CLIPUV_HLT,
                  offset,
                  rect->ymin,
                  ICON_WIDTH,
                  channel_height);
  }

  /* Layer opacity. */
  const short width = SLIDER_WIDTH * 0.6;
  offset -= width;
  UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
  PropertyRNA *opacity_prop = RNA_struct_find_property(&ptr, "opacity");
  const std::optional<std::string> opacity_rna_path = RNA_path_from_ID_to_property(&ptr,
                                                                                   opacity_prop);
  if (RNA_path_resolve_property(&id_ptr, opacity_rna_path->c_str(), &ptr, &opacity_prop)) {
    uiDefAutoButR(block,
                  &ptr,
                  opacity_prop,
                  array_index,
                  "",
                  ICON_NONE,
                  offset,
                  rect->ymin,
                  width,
                  channel_height);
  }
}

void ANIM_channel_draw_widgets(const bContext *C,
                               bAnimContext *ac,
                               bAnimListElem *ale,
                               uiBlock *block,
                               const rctf *rect,
                               size_t channel_index)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  float ymid;
  const short channel_height = round_fl_to_int(BLI_rctf_size_y(rect));
  const bool is_being_renamed = achannel_is_being_renamed(ac, acf, channel_index);

  /* sanity checks - don't draw anything */
  if (ELEM(nullptr, acf, ale, block)) {
    return;
  }

  /* get initial offset */
  short offset = rect->xmin;
  if (acf->get_offset) {
    offset += acf->get_offset(ac, ale);
  }

  /* calculate appropriate y-coordinates for icon buttons */
  ymid = BLI_rctf_cent_y(rect) - 0.5f * ICON_WIDTH;

  /* no button backdrop behind icons */
  UI_block_emboss_set(block, blender::ui::EmbossType::None);

  /* step 1) draw expand widget ....................................... */
  if (acf->has_setting(ac, ale, ACHANNEL_SETTING_EXPAND)) {
    draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_EXPAND);
    offset += ICON_WIDTH;
  }

  /* step 2) draw icon ............................................... */
  if (acf->icon) {
    /* icon is not drawn here (not a widget) */
    offset += ICON_WIDTH;
  }

  /* step 3) draw special toggles  .................................
   * - in Graph Editor, check-boxes for visibility in curves area
   * - in NLA Editor, glowing dots for solo/not solo...
   * - in Grease Pencil mode, color swatches for layer color
   */
  if (ac->sl) {
    if (ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH) &&
        (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE) ||
         acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE)) &&
        !ELEM(ale->type,
              ANIMTYPE_GPLAYER,
              ANIMTYPE_GREASE_PENCIL_LAYER,
              ANIMTYPE_GREASE_PENCIL_LAYER_GROUP))
    {
      /* Pin toggle. */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE)) {
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_ALWAYS_VISIBLE);
        offset += ICON_WIDTH;
      }
      /* Visibility toggle. */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE)) {
        /* For F-Curves, add the extra space for the color bands. */
        if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
          offset += GRAPH_ICON_VISIBILITY_OFFSET;
        }
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_VISIBLE);
        offset += ICON_WIDTH;
      }
    }
  }

  /* step 4) draw text - check if renaming widget is in use... */
  if (is_being_renamed) {
    PointerRNA ptr = {};
    PropertyRNA *prop = nullptr;

    /* draw renaming widget if we can get RNA pointer for it
     * NOTE: property may only be available in some cases, even if we have
     *       a callback available (e.g. broken F-Curve rename)
     */
    if (acf->name_prop(ale, &ptr, &prop)) {
      const short margin_x = 3 * round_fl_to_int(UI_SCALE_FAC);
      const short width = ac->region->winx - offset - (margin_x * 2);
      uiBut *but;

      UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);

      but = uiDefButR(block,
                      ButType::Text,
                      1,
                      "",
                      offset + margin_x,
                      rect->ymin,
                      std::max(width, RENAME_TEXT_MIN_WIDTH),
                      channel_height,
                      &ptr,
                      RNA_property_identifier(prop),
                      -1,
                      0,
                      0,
                      std::nullopt);

      /* copy what outliner does here, see outliner_buttons */
      if (UI_but_active_only(C, ac->region, block, but) == false) {
        ac->ads->renameIndex = 0;

        /* send notifiers */
        WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_RENAME, nullptr);
      }

      UI_block_emboss_set(block, blender::ui::EmbossType::None);
    }
    else {
      /* Cannot get property/cannot or rename for some reason, so clear rename index
       * so that this doesn't hang around, and the name can be drawn normally - #47492
       */
      ac->ads->renameIndex = 0;
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
    }
  }

  /* step 5) draw mute+protection toggles + (sliders) ....................... */
  /* reset offset - now goes from RHS of panel */
  offset = int(rect->xmax);

  /* TODO: when drawing sliders, make those draw instead of these toggles if not enough space. */
  if (!is_being_renamed) {
    short draw_sliders = 0;

    /* check if we need to show the sliders */
    if ((ac->sl) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH)) {
      switch (ac->spacetype) {
        case SPACE_ACTION: {
          SpaceAction *saction = reinterpret_cast<SpaceAction *>(ac->sl);
          draw_sliders = (saction->flag & SACTION_SLIDERS);
          break;
        }
        case SPACE_GRAPH: {
          SpaceGraph *sipo = reinterpret_cast<SpaceGraph *>(ac->sl);
          draw_sliders = (sipo->flag & SIPO_SLIDERS);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
    }

    /* check if there's enough space for the toggles if the sliders are drawn too */
    if (!(draw_sliders) || (BLI_rcti_size_x(&v2d->mask) > ANIM_UI_get_channel_button_width() / 2))
    {
      /* NOTE: The comments here match the comments in ANIM_channel_draw(), as that
       * function and this one are strongly coupled. */

      /* Little channel color rectangle. */
      const bool show_group_colors = acf_show_channel_colors();
      if (show_group_colors) {
        const float rect_width = CHANNEL_COLOR_RECT_WIDTH;
        const float rect_margin = CHANNEL_COLOR_RECT_MARGIN;
        uint8_t color[3];
        if (acf->get_channel_color && acf->get_channel_color(ale, color)) {
          immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
          immUniformColor3ubv(color);

          GPUVertFormat format = {0};
          uint pos = GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32);
          immRectf(pos,
                   rect->xmax - rect_width - rect_margin,
                   rect->ymin + rect_margin,
                   rect->xmax - rect_margin,
                   rect->ymax - rect_margin);

          immUnbindProgram();
        }
        offset -= rect_width + 2 * rect_margin;
      }

      /* solo... */
      if ((ac->spacetype == SPACE_NLA) && acf->has_setting(ac, ale, ACHANNEL_SETTING_SOLO)) {
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_SOLO);
        /* A touch of padding because the star icon is so wide. */
        offset -= short(0.2f * ICON_WIDTH);
      }
      /* protect... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PROTECT)) {
        offset -= ICON_WIDTH;
        if (ale->type == ANIMTYPE_FCURVE) {
          FCurve *fcu = static_cast<FCurve *>(ale->data);
          /* Don't draw lock icon when curve is baked.
           * Still using the offset so icons are aligned. */
          if (fcu->bezt) {
            draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_PROTECT);
          }
        }
        else {
          draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_PROTECT);
        }
      }
      /* mute... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MUTE)) {
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_MUTE);
      }
      if (ELEM(ale->type,
               ANIMTYPE_GPLAYER,
               ANIMTYPE_GREASE_PENCIL_LAYER,
               ANIMTYPE_GREASE_PENCIL_LAYER_GROUP))
      {
        /* Not technically "mute"
         * (in terms of anim channels, but this sets layer visibility instead). */
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_VISIBLE);
      }

      /* modifiers disable */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MOD_OFF)) {
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_MOD_OFF);
      }

      /* ----------- */

      /* pinned... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PINNED)) {
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_PINNED);
      }

      /* NLA Action "pushdown" */
      if ((ale->type == ANIMTYPE_NLAACTION) && (ale->adt && ale->adt->action) &&
          !(ale->adt->flag & ADT_NLA_EDIT_ON))
      {
        uiBut *but;
        PointerRNA *opptr_b;

        UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);

        offset -= UI_UNIT_X;
        but = uiDefIconButO(block,
                            ButType::But,
                            "NLA_OT_action_pushdown",
                            blender::wm::OpCallContext::InvokeDefault,
                            ICON_NLA_PUSHDOWN,
                            offset,
                            ymid,
                            UI_UNIT_X,
                            UI_UNIT_X,
                            std::nullopt);

        opptr_b = UI_but_operator_ptr_ensure(but);
        RNA_int_set(opptr_b, "track_index", channel_index);

        UI_block_emboss_set(block, blender::ui::EmbossType::None);
      }

      /* Slot ID type indicator. */
      if (ale->type == ANIMTYPE_ACTION_SLOT) {
        offset -= ICON_WIDTH;
        UI_icon_draw(offset, ymid, acf_action_slot_idtype_icon(ale));
      }
    }

    /* Draw slider:
     * - Even if we can draw sliders for this view, we must also check that the channel-type
     *   supports them (only F-Curves really can support them for now).
     * - To make things easier, we use RNA-auto-buttons for this so that changes are
     *   reflected immediately, wherever they occurred.
     *   BUT, we don't use the layout engine, otherwise we'd get wrong alignment,
     *   and wouldn't be able to auto-keyframe.
     * - Slider should start before the toggles (if they're visible)
     *   to keep a clean line down the side.
     * - Sliders are always drawn in Shape-key mode now. Prior to this
     *   the SACTION_SLIDERS flag would be set when changing into shape-key mode.
     */
    if (((draw_sliders) && ELEM(ale->type,
                                ANIMTYPE_FCURVE,
                                ANIMTYPE_NLACURVE,
                                ANIMTYPE_SHAPEKEY,
                                ANIMTYPE_GPLAYER,
                                ANIMTYPE_GREASE_PENCIL_LAYER,
                                ANIMTYPE_GREASE_PENCIL_LAYER_GROUP)) ||
        ale->type == ANIMTYPE_SHAPEKEY)
    {
      /* adjust offset */
      /* TODO: make slider width dynamic,
       * so that they can be easier to use when the view is wide enough. */
      offset -= SLIDER_WIDTH;

      /* need backdrop behind sliders... */
      UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);

      if (ale->owner) { /* Slider using custom RNA Access ---------- */
        if (ale->type == ANIMTYPE_NLACURVE) {
          NlaStrip *strip = static_cast<NlaStrip *>(ale->owner);
          FCurve *fcu = static_cast<FCurve *>(ale->data);
          PropertyRNA *prop;

          /* create RNA pointers */
          PointerRNA ptr = RNA_pointer_create_discrete(ale->id, &RNA_NlaStrip, strip);
          prop = RNA_struct_find_property(&ptr, fcu->rna_path);

          /* create property slider */
          if (prop) {
            uiBut *but;

            /* Create the slider button,
             * and assign relevant callback to ensure keyframes are inserted. */
            but = uiDefAutoButR(block,
                                &ptr,
                                prop,
                                fcu->array_index,
                                "",
                                ICON_NONE,
                                offset,
                                rect->ymin,
                                SLIDER_WIDTH,
                                channel_height);
            UI_but_func_set(but, achannel_setting_slider_nla_curve_cb, ale->id, ale->data);
          }
        }
      }
      else if (ale->id) { /* Slider using RNA Access --------------- */
        PointerRNA ptr;
        PropertyRNA *prop;
        std::optional<std::string> rna_path;
        int array_index = 0;

        /* get destination info */
        if (ale->type == ANIMTYPE_FCURVE) {
          FCurve *fcu = static_cast<FCurve *>(ale->data);

          rna_path = fcu->rna_path;
          array_index = fcu->array_index;
        }
        else if (ale->type == ANIMTYPE_SHAPEKEY) {
          KeyBlock *kb = static_cast<KeyBlock *>(ale->data);
          Key *key = reinterpret_cast<Key *>(ale->id);

          rna_path = BKE_keyblock_curval_rnapath_get(key, kb);
        }
        /* Special for Grease Pencil Layer. */
        else if (ale->type == ANIMTYPE_GPLAYER) {
          bGPdata *gpd = reinterpret_cast<bGPdata *>(ale->id);
          if ((gpd->flag & GP_DATA_ANNOTATIONS) == 0) {
            /* Reset slider offset, in order to add special gp icons. */
            offset += SLIDER_WIDTH;

            bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

            /* Create the RNA pointers. */
            ptr = RNA_pointer_create_discrete(ale->id, &RNA_AnnotationLayer, ale->data);
            PointerRNA id_ptr = RNA_id_pointer_create(ale->id);
            int icon;

            /* Layer onion skinning switch. */
            offset -= ICON_WIDTH;
            UI_block_emboss_set(block, blender::ui::EmbossType::None);
            prop = RNA_struct_find_property(&ptr, "use_annotation_onion_skinning");
            if (const std::optional<std::string> gp_rna_path = RNA_path_from_ID_to_property(&ptr,
                                                                                            prop))
            {
              if (RNA_path_resolve_property(&id_ptr, gp_rna_path->c_str(), &ptr, &prop)) {
                icon = (gpl->onion_flag & GP_LAYER_ONIONSKIN) ? ICON_ONIONSKIN_ON :
                                                                ICON_ONIONSKIN_OFF;
                uiDefAutoButR(block,
                              &ptr,
                              prop,
                              array_index,
                              "",
                              icon,
                              offset,
                              rect->ymin,
                              ICON_WIDTH,
                              channel_height);
              }
            }

            /* Layer opacity. */
            const short width = SLIDER_WIDTH * 0.6;
            offset -= width;
            UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
            prop = RNA_struct_find_property(&ptr, "annotation_opacity");
            if (const std::optional<std::string> gp_rna_path = RNA_path_from_ID_to_property(&ptr,
                                                                                            prop))
            {
              if (RNA_path_resolve_property(&id_ptr, gp_rna_path->c_str(), &ptr, &prop)) {
                uiDefAutoButR(block,
                              &ptr,
                              prop,
                              array_index,
                              "",
                              ICON_NONE,
                              offset,
                              rect->ymin,
                              width,
                              channel_height);
              }
            }
          }
        }
        else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
          draw_grease_pencil_layer_widgets(ale, block, rect, offset, channel_height, array_index);
        }

        /* Only if RNA-Path found. */
        if (rna_path) {
          /* get RNA pointer, and resolve the path */
          PointerRNA id_ptr = RNA_id_pointer_create(ale->id);

          /* try to resolve the path */
          if (RNA_path_resolve_property(&id_ptr, rna_path->c_str(), &ptr, &prop)) {
            uiBut *but;

            /* Create the slider button,
             * and assign relevant callback to ensure keyframes are inserted. */
            but = uiDefAutoButR(block,
                                &ptr,
                                prop,
                                array_index,
                                RNA_property_type(prop) == PROP_ENUM ? std::nullopt :
                                                                       std::optional(""),
                                ICON_NONE,
                                offset,
                                rect->ymin,
                                SLIDER_WIDTH,
                                channel_height);

            /* assign keyframing function according to slider type */
            if (ale->type == ANIMTYPE_SHAPEKEY) {
              UI_but_func_set(but, achannel_setting_slider_shapekey_cb, ale->id, ale->data);
            }
            else {
              UI_but_func_set(but, achannel_setting_slider_cb, ale->id, ale->data);
            }
          }
        }
      }
      else { /* Special Slider for stuff without RNA Access ---------- */
        /* TODO: only implement this case when we really need it... */
      }
    }
  }
}

/* *********************************************** */
