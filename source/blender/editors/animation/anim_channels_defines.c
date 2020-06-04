/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup edanimation
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
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
#include "DNA_simulation_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "RNA_access.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_gpencil.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_nla.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"

#include "WM_api.h"
#include "WM_types.h"

/* *********************************************** */
// XXX constant defines to be moved elsewhere?

/* extra padding for lengths (to go under scrollers) */
#define EXTRA_SCROLL_PAD 100.0f

/* size of indent steps */
#define INDENT_STEP_SIZE (0.35f * U.widget_unit)

/* size of string buffers used for animation channel displayed names */
#define ANIM_CHAN_NAME_SIZE 256

/* get the pointer used for some flag */
#define GET_ACF_FLAG_PTR(ptr, type) ((*(type) = sizeof((ptr))), &(ptr))

/* *********************************************** */
/* Generic Functions (Type independent) */

/* Draw Backdrop ---------------------------------- */

/* get backdrop color for top-level widgets (Scene and Object only) */
static void acf_generic_root_color(bAnimContext *UNUSED(ac),
                                   bAnimListElem *UNUSED(ale),
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
  UI_draw_roundbox_3fv_alpha(
      true, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc, 8, color, 1.0f);
}

/* get backdrop color for data expanders under top-level Scene/Object */
static void acf_generic_dataexpand_color(bAnimContext *UNUSED(ac),
                                         bAnimListElem *UNUSED(ale),
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

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3fv(color);

  /* no rounded corner - just rectangular box */
  immRectf(pos, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc);

  immUnbindProgram();
}

/* helper method to test if group colors should be drawn */
static bool acf_show_channel_colors(bAnimContext *ac)
{
  bool showGroupColors = false;

  if (ac->sl) {
    switch (ac->spacetype) {
      case SPACE_ACTION: {
        SpaceAction *saction = (SpaceAction *)ac->sl;
        showGroupColors = !(saction->flag & SACTION_NODRAWGCOLORS);

        break;
      }
      case SPACE_GRAPH: {
        SpaceGraph *sipo = (SpaceGraph *)ac->sl;
        showGroupColors = !(sipo->flag & SIPO_NODRAWGCOLORS);

        break;
      }
    }
  }

  return showGroupColors;
}

/* get backdrop color for generic channels */
static void acf_generic_channel_color(bAnimContext *ac, bAnimListElem *ale, float r_color[3])
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  bActionGroup *grp = NULL;
  short indent = (acf->get_indent_level) ? acf->get_indent_level(ac, ale) : 0;
  bool showGroupColors = acf_show_channel_colors(ac);

  if (ale->type == ANIMTYPE_FCURVE) {
    FCurve *fcu = (FCurve *)ale->data;
    grp = fcu->grp;
  }

  /* set color for normal channels
   * - use 3 shades of color group/standard color for 3 indention level
   * - only use group colors if allowed to, and if actually feasible
   */
  if (showGroupColors && (grp) && (grp->customCol)) {
    uchar cp[3];

    if (indent == 2) {
      copy_v3_v3_uchar(cp, grp->cs.solid);
    }
    else if (indent == 1) {
      copy_v3_v3_uchar(cp, grp->cs.select);
    }
    else {
      copy_v3_v3_uchar(cp, grp->cs.active);
    }

    /* copy the colors over, transforming from bytes to floats */
    rgb_uchar_to_float(r_color, cp);
  }
  else {
    /* FIXME: what happens when the indention is 1 greater than what it should be
     * (due to grouping)? */
    int colOfs = 10 - 10 * indent;
    UI_GetThemeColorShade3fv(TH_SHADE2, colOfs, r_color);
  }
}

/* get backdrop color for grease pencil channels */
static void acf_gpencil_channel_color(bAnimContext *ac, bAnimListElem *ale, float r_color[3])
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  short indent = (acf->get_indent_level) ? acf->get_indent_level(ac, ale) : 0;
  bool showGroupColors = acf_show_channel_colors(ac);

  if ((showGroupColors) && (ale->type == ANIMTYPE_GPLAYER)) {
    bGPDlayer *gpl = (bGPDlayer *)ale->data;
    copy_v3_v3(r_color, gpl->color);
  }
  else {
    int colOfs = 10 - 10 * indent;
    UI_GetThemeColorShade3fv(TH_SHADE2, colOfs, r_color);
  }
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

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* set backdrop drawing color */
  acf->get_backdrop_color(ac, ale, color);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3fv(color);

  /* no rounded corners - just rectangular box */
  immRectf(pos, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc);

  immUnbindProgram();
}

/* Indention + Offset ------------------------------------------- */

/* indention level is always the value in the name */
static short acf_generic_indention_0(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale))
{
  return 0;
}
static short acf_generic_indention_1(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale))
{
  return 1;
}
#if 0  // XXX not used
static short acf_generic_indention_2(bAnimContext *ac, bAnimListElem *ale)
{
  return 2;
}
#endif

/* indention which varies with the grouping status */
static short acf_generic_indention_flexible(bAnimContext *UNUSED(ac), bAnimListElem *ale)
{
  short indent = 0;

  /* grouped F-Curves need extra level of indention */
  if (ale->type == ANIMTYPE_FCURVE) {
    FCurve *fcu = (FCurve *)ale->data;

    // TODO: we need some way of specifying that the indention color should be one less...
    if (fcu->grp) {
      indent++;
    }
  }

  /* no indention */
  return indent;
}

/* basic offset for channels derived from indention */
static short acf_generic_basic_offset(bAnimContext *ac, bAnimListElem *ale)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

  if (acf && acf->get_indent_level) {
    return acf->get_indent_level(ac, ale) * INDENT_STEP_SIZE;
  }
  else {
    return 0;
  }
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
    /* texture animdata */
    if (GS(ale->id->name) == ID_TE) {
      offset += U.widget_unit;
    }
    /* materials and particles animdata */
    else if (ELEM(GS(ale->id->name), ID_MA, ID_PA)) {
      offset += (short)(0.7f * U.widget_unit);

      /* If not in Action Editor mode, action-groups (and their children)
       * must carry some offset too. */
    }
    else if (ac->datatype != ANIMCONT_ACTION) {
      offset += (short)(0.7f * U.widget_unit);
    }

    /* nodetree animdata */
    if (GS(ale->id->name) == ID_NT) {
      offset += acf_nodetree_rootType_offset((bNodeTree *)ale->id);
    }
  }

  /* offset is just the normal type - i.e. based on indention */
  return offset;
}

/* Name ------------------------------------------- */

/* name for ID block entries */
static void acf_generic_idblock_name(bAnimListElem *ale, char *name)
{
  ID *id = (ID *)ale->data; /* data pointed to should be an ID block */

  /* just copy the name... */
  if (id && name) {
    BLI_strncpy(name, id->name + 2, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for ID block entries */
static bool acf_generic_idblock_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  RNA_id_pointer_create(ale->data, ptr);
  *prop = RNA_struct_name_property(ptr->type);

  return (*prop != NULL);
}

/* name property for ID block entries which are just subheading "fillers" */
static bool acf_generic_idfill_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  /* actual ID we're representing is stored in ale->data not ale->id, as id gives the owner */
  RNA_id_pointer_create(ale->data, ptr);
  *prop = RNA_struct_name_property(ptr->type);

  return (*prop != NULL);
}

/* Settings ------------------------------------------- */

#if 0
/* channel type has no settings */
static bool acf_generic_none_setting_valid(bAnimContext *ac,
                                           bAnimListElem *ale,
                                           eAnimChannel_Settings setting)
{
  return false;
}
#endif

/* check if some setting exists for this object-based data-expander (datablock only) */
static bool acf_generic_dataexpand_setting_valid(bAnimContext *ac,
                                                 bAnimListElem *UNUSED(ale),
                                                 eAnimChannel_Settings setting)
{
  switch (setting) {
    /* expand is always supported */
    case ACHANNEL_SETTING_EXPAND:
      return true;

    /* mute is only supported for NLA */
    case ACHANNEL_SETTING_MUTE:
      return ((ac) && (ac->spacetype == SPACE_NLA));

    /* select is ok for most "ds*" channels (e.g. dsmat) */
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
static void acf_summary_color(bAnimContext *UNUSED(ac),
                              bAnimListElem *UNUSED(ale),
                              float r_color[3])
{
  /* reddish color - same as the 'action' line in NLA */
  UI_GetThemeColor3fv(TH_ANIM_ACTIVE, r_color);
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
  UI_draw_roundbox_3fv_alpha(
      true, 0, yminc - 2, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc, 8, color, 1.0f);
}

/* name for summary entries */
static void acf_summary_name(bAnimListElem *UNUSED(ale), char *name)
{
  if (name) {
    BLI_strncpy(name, IFACE_("Summary"), ANIM_CHAN_NAME_SIZE);
  }
}

/* check if some setting exists for this channel */
static bool acf_summary_setting_valid(bAnimContext *UNUSED(ac),
                                      bAnimListElem *UNUSED(ale),
                                      eAnimChannel_Settings setting)
{
  /* only expanded is supported, as it is used for hiding all stuff which the summary covers */
  return (setting == ACHANNEL_SETTING_EXPAND);
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_summary_setting_flag(bAnimContext *UNUSED(ac),
                                    eAnimChannel_Settings setting,
                                    bool *neg)
{
  if (setting == ACHANNEL_SETTING_EXPAND) {
    /* expanded */
    *neg = true;
    return ADS_FLAG_SUMMARY_COLLAPSED;
  }
  else {
    /* unsupported */
    *neg = false;
    return 0;
  }
}

/* get pointer to the setting */
static void *acf_summary_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *type)
{
  bAnimContext *ac = (bAnimContext *)ale->data;

  /* if data is valid, return pointer to active dopesheet's relevant flag
   * - this is restricted to DopeSheet/Action Editor only
   */
  if ((ac->sl) && (ac->spacetype == SPACE_ACTION) && (setting == ACHANNEL_SETTING_EXPAND)) {
    SpaceAction *saction = (SpaceAction *)ac->sl;
    bDopeSheet *ads = &saction->ads;

    /* return pointer to DopeSheet's flag */
    return GET_ACF_FLAG_PTR(ads->flag, type);
  }
  else {
    /* can't return anything useful - unsupported */
    *type = 0;
    return NULL;
  }
}

/* all animation summary (DopeSheet only) type define */
static bAnimChannelType ACF_SUMMARY = {
    "Summary",              /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_summary_color,       /* backdrop color */
    acf_summary_backdrop,    /* backdrop */
    acf_generic_indention_0, /* indent level */
    NULL,                    /* offset */

    acf_summary_name, /* name */
    NULL,             /* name prop */
    NULL,             /* icon */

    acf_summary_setting_valid, /* has setting */
    acf_summary_setting_flag,  /* flag for setting */
    acf_summary_setting_ptr,   /* pointer for setting */
};

/* Scene ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_scene_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_SCENE_DATA;
}

/* check if some setting exists for this channel */
static bool acf_scene_setting_valid(bAnimContext *ac,
                                    bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_scene_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return SCE_DS_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *neg = true;
      return SCE_DS_COLLAPSED;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_scene_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Scene *scene = (Scene *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return GET_ACF_FLAG_PTR(scene->flag, type);

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(scene->flag, type);

    case ACHANNEL_SETTING_MUTE:    /* mute (only in NLA) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (scene->adt) {
        return GET_ACF_FLAG_PTR(scene->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* scene type define */
static bAnimChannelType ACF_SCENE = {
    "Scene",                /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_root_color,    /* backdrop color */
    acf_generic_root_backdrop, /* backdrop */
    acf_generic_indention_0,   /* indent level */
    NULL,                      /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_scene_icon,                /* icon */

    acf_scene_setting_valid, /* has setting */
    acf_scene_setting_flag,  /* flag for setting */
    acf_scene_setting_ptr,   /* pointer for setting */
};

/* Object ------------------------------------------- */

static int acf_object_icon(bAnimListElem *ale)
{
  Base *base = (Base *)ale->data;
  Object *ob = base->object;

  /* icon depends on object-type */
  switch (ob->type) {
    case OB_LAMP:
      return ICON_OUTLINER_OB_LIGHT;
    case OB_MESH:
      return ICON_OUTLINER_OB_MESH;
    case OB_CAMERA:
      return ICON_OUTLINER_OB_CAMERA;
    case OB_CURVE:
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
    case OB_HAIR:
      return ICON_OUTLINER_OB_HAIR;
    case OB_POINTCLOUD:
      return ICON_OUTLINER_OB_POINTCLOUD;
    case OB_VOLUME:
      return ICON_OUTLINER_OB_VOLUME;
    case OB_EMPTY:
      return ICON_OUTLINER_OB_EMPTY;
    case OB_GPENCIL:
      return ICON_OUTLINER_OB_GREASEPENCIL;
    default:
      return ICON_OBJECT_DATA;
  }
}

/* name for object */
static void acf_object_name(bAnimListElem *ale, char *name)
{
  Base *base = (Base *)ale->data;
  Object *ob = base->object;

  /* just copy the name... */
  if (ob && name) {
    BLI_strncpy(name, ob->id.name + 2, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for object */
static bool acf_object_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  RNA_id_pointer_create(ale->id, ptr);
  *prop = RNA_struct_name_property(ptr->type);

  return (*prop != NULL);
}

/* check if some setting exists for this channel */
static bool acf_object_setting_valid(bAnimContext *ac,
                                     bAnimListElem *ale,
                                     eAnimChannel_Settings setting)
{
  Base *base = (Base *)ale->data;
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_object_setting_flag(bAnimContext *UNUSED(ac),
                                   eAnimChannel_Settings setting,
                                   bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return BASE_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *neg = 1;
      return OB_ADS_COLLAPSED;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_object_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Base *base = (Base *)ale->data;
  Object *ob = base->object;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return GET_ACF_FLAG_PTR(base->flag, type);

    case ACHANNEL_SETTING_EXPAND:                  /* expanded */
      return GET_ACF_FLAG_PTR(ob->nlaflag, type);  // xxx

    case ACHANNEL_SETTING_MUTE:    /* mute (only in NLA) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      if (ob->adt) {
        return GET_ACF_FLAG_PTR(ob->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* object type define */
static bAnimChannelType ACF_OBJECT = {
    "Object",               /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_root_color,    /* backdrop color */
    acf_generic_root_backdrop, /* backdrop */
    acf_generic_indention_0,   /* indent level */
    NULL,                      /* offset */

    acf_object_name,      /* name */
    acf_object_name_prop, /* name prop */
    acf_object_icon,      /* icon */

    acf_object_setting_valid, /* has setting */
    acf_object_setting_flag,  /* flag for setting */
    acf_object_setting_ptr,   /* pointer for setting */
};

/* Group ------------------------------------------- */

/* get backdrop color for group widget */
static void acf_group_color(bAnimContext *ac, bAnimListElem *ale, float r_color[3])
{
  bActionGroup *agrp = (bActionGroup *)ale->data;
  bool showGroupColors = acf_show_channel_colors(ac);

  if (showGroupColors && agrp->customCol) {
    uchar cp[3];

    /* highlight only for active */
    if (ale->flag & AGRP_ACTIVE) {
      copy_v3_v3_uchar(cp, agrp->cs.select);
    }
    else {
      copy_v3_v3_uchar(cp, agrp->cs.solid);
    }

    /* copy the colors over, transforming from bytes to floats */
    rgb_uchar_to_float(r_color, cp);
  }
  else {
    /* highlight only for active */
    if (ale->flag & AGRP_ACTIVE) {
      UI_GetThemeColor3fv(TH_GROUP_ACTIVE, r_color);
    }
    else {
      UI_GetThemeColor3fv(TH_GROUP, r_color);
    }
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
  UI_draw_roundbox_3fv_alpha(
      true, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc, 8, color, 1.0f);
}

/* name for group entries */
static void acf_group_name(bAnimListElem *ale, char *name)
{
  bActionGroup *agrp = (bActionGroup *)ale->data;

  /* just copy the name... */
  if (agrp && name) {
    BLI_strncpy(name, agrp->name, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for group entries */
static bool acf_group_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  RNA_pointer_create(ale->id, &RNA_ActionGroup, ale->data, ptr);
  *prop = RNA_struct_name_property(ptr->type);

  return (*prop != NULL);
}

/* check if some setting exists for this channel */
static bool acf_group_setting_valid(bAnimContext *ac,
                                    bAnimListElem *UNUSED(ale),
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

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return (ac->spacetype == SPACE_GRAPH);

    default: /* always supported */
      return true;
  }
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_group_setting_flag(bAnimContext *ac, eAnimChannel_Settings setting, bool *neg)
{
  /* clear extra return data first */
  *neg = false;

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
      *neg = 1;
      return AGRP_MODIFIERS_OFF;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return AGRP_PROTECTED;

    case ACHANNEL_SETTING_VISIBLE: /* visibility - graph editor */
      *neg = 1;
      return AGRP_NOTVISIBLE;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return ADT_CURVES_ALWAYS_VISIBLE;

    default:
      /* this shouldn't happen */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_group_setting_ptr(bAnimListElem *ale,
                                   eAnimChannel_Settings UNUSED(setting),
                                   short *type)
{
  bActionGroup *agrp = (bActionGroup *)ale->data;

  /* all flags are just in agrp->flag for now... */
  return GET_ACF_FLAG_PTR(agrp->flag, type);
}

/* group type define */
static bAnimChannelType ACF_GROUP = {
    "Group",               /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_group_color,          /* backdrop color */
    acf_group_backdrop,       /* backdrop */
    acf_generic_indention_0,  /* indent level */
    acf_generic_group_offset, /* offset */

    acf_group_name,      /* name */
    acf_group_name_prop, /* name prop */
    NULL,                /* icon */

    acf_group_setting_valid, /* has setting */
    acf_group_setting_flag,  /* flag for setting */
    acf_group_setting_ptr,   /* pointer for setting */
};

/* F-Curve ------------------------------------------- */

/* name for fcurve entries */
static void acf_fcurve_name(bAnimListElem *ale, char *name)
{
  getname_anim_fcurve(name, ale->id, ale->data);
}

/* "name" property for fcurve entries */
static bool acf_fcurve_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  FCurve *fcu = (FCurve *)ale->data;

  /* Ctrl-Click Usability Convenience Hack:
   * For disabled F-Curves, allow access to the RNA Path
   * as our "name" so that user can perform quick fixes
   */
  if (fcu->flag & FCURVE_DISABLED) {
    RNA_pointer_create(ale->id, &RNA_FCurve, ale->data, ptr);
    *prop = RNA_struct_find_property(ptr, "data_path");
  }
  else {
    /* for "normal" F-Curves - no editable name, but *prop may not be set properly yet... */
    *prop = NULL;
  }

  return (*prop != NULL);
}

/* check if some setting exists for this channel */
static bool acf_fcurve_setting_valid(bAnimContext *ac,
                                     bAnimListElem *ale,
                                     eAnimChannel_Settings setting)
{
  FCurve *fcu = (FCurve *)ale->data;

  switch (setting) {
    /* unsupported */
    case ACHANNEL_SETTING_SOLO:   /* Solo Flag is only for NLA */
    case ACHANNEL_SETTING_EXPAND: /* F-Curves are not containers */
    case ACHANNEL_SETTING_PINNED: /* This is only for NLA Actions */
      return false;

    /* conditionally available */
    case ACHANNEL_SETTING_PROTECT: /* Protection is only valid when there's keyframes */
      if (fcu->bezt) {
        return true;
      }
      else {
        return false;  // NOTE: in this special case, we need to draw ICON_ZOOMOUT
      }

    case ACHANNEL_SETTING_VISIBLE: /* Only available in Graph Editor */
      return (ac->spacetype == SPACE_GRAPH);

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return false;

    /* always available */
    default:
      return true;
  }
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fcurve_setting_flag(bAnimContext *UNUSED(ac),
                                   eAnimChannel_Settings setting,
                                   bool *neg)
{
  /* clear extra return data first */
  *neg = false;

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
      *neg = 1;
      return FCURVE_MOD_OFF;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_fcurve_setting_ptr(bAnimListElem *ale,
                                    eAnimChannel_Settings UNUSED(setting),
                                    short *type)
{
  FCurve *fcu = (FCurve *)ale->data;

  /* all flags are just in agrp->flag for now... */
  return GET_ACF_FLAG_PTR(fcu->flag, type);
}

/* fcurve type define */
static bAnimChannelType ACF_FCURVE = {
    "F-Curve",             /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_generic_channel_color,    /* backdrop color */
    acf_generic_channel_backdrop, /* backdrop */
    acf_generic_indention_flexible,
    /* indent level */        // xxx rename this to f-curves only?
    acf_generic_group_offset, /* offset */

    acf_fcurve_name,      /* name */
    acf_fcurve_name_prop, /* name prop */
    NULL,                 /* icon */

    acf_fcurve_setting_valid, /* has setting */
    acf_fcurve_setting_flag,  /* flag for setting */
    acf_fcurve_setting_ptr,   /* pointer for setting */
};

/* NLA Control FCurves Expander ----------------------- */

/* get backdrop color for nla controls widget */
static void acf_nla_controls_color(bAnimContext *UNUSED(ac),
                                   bAnimListElem *UNUSED(ale),
                                   float r_color[3])
{
  // TODO: give this its own theme setting?
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
  UI_draw_roundbox_3fv_alpha(
      true, offset, yminc, v2d->cur.xmax + EXTRA_SCROLL_PAD, ymaxc, 5, color, 1.0f);
}

/* name for nla controls expander entries */
static void acf_nla_controls_name(bAnimListElem *UNUSED(ale), char *name)
{
  BLI_strncpy(name, IFACE_("NLA Strip Controls"), ANIM_CHAN_NAME_SIZE);
}

/* check if some setting exists for this channel */
static bool acf_nla_controls_setting_valid(bAnimContext *UNUSED(ac),
                                           bAnimListElem *UNUSED(ale),
                                           eAnimChannel_Settings setting)
{
  /* for now, all settings are supported, though some are only conditionally */
  switch (setting) {
    /* supported */
    case ACHANNEL_SETTING_EXPAND:
      return true;

      // TODO: selected?

    default: /* unsupported */
      return false;
  }
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_nla_controls_setting_flag(bAnimContext *UNUSED(ac),
                                         eAnimChannel_Settings setting,
                                         bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *neg = true;
      return ADT_NLA_SKEYS_COLLAPSED;

    default:
      /* this shouldn't happen */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_nla_controls_setting_ptr(bAnimListElem *ale,
                                          eAnimChannel_Settings UNUSED(setting),
                                          short *type)
{
  AnimData *adt = (AnimData *)ale->data;

  /* all flags are just in adt->flag for now... */
  return GET_ACF_FLAG_PTR(adt->flag, type);
}

static int acf_nla_controls_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_NLA;
}

/* NLA Control FCurves Expander type define */
static bAnimChannelType ACF_NLACONTROLS = {
    "NLA Controls Expander", /* type name */
    ACHANNEL_ROLE_EXPANDER,  /* role */

    acf_nla_controls_color,    /* backdrop color */
    acf_nla_controls_backdrop, /* backdrop */
    acf_generic_indention_0,   /* indent level */
    acf_generic_group_offset,  /* offset */

    acf_nla_controls_name, /* name */
    NULL,                  /* name prop */
    acf_nla_controls_icon, /* icon */

    acf_nla_controls_setting_valid, /* has setting */
    acf_nla_controls_setting_flag,  /* flag for setting */
    acf_nla_controls_setting_ptr,   /* pointer for setting */
};

/* NLA Control F-Curve -------------------------------- */

/* name for nla control fcurve entries */
static void acf_nla_curve_name(bAnimListElem *ale, char *name)
{
  NlaStrip *strip = ale->owner;
  FCurve *fcu = ale->data;
  PropertyRNA *prop;

  /* try to get RNA property that this shortened path (relative to the strip) refers to */
  prop = RNA_struct_type_find_property(&RNA_NlaStrip, fcu->rna_path);
  if (prop) {
    /* "name" of this strip displays the UI identifier + the name of the NlaStrip */
    BLI_snprintf(name, 256, "%s (%s)", RNA_property_ui_name(prop), strip->name);
  }
  else {
    /* unknown property... */
    BLI_snprintf(name, 256, "%s[%d]", fcu->rna_path, fcu->array_index);
  }
}

/* NLA Control F-Curve type define */
static bAnimChannelType ACF_NLACURVE = {
    "NLA Control F-Curve", /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_generic_channel_color,    /* backdrop color */
    acf_generic_channel_backdrop, /* backdrop */
    acf_generic_indention_1,      /* indent level */
    acf_generic_group_offset,     /* offset */

    acf_nla_curve_name,   /* name */
    acf_fcurve_name_prop, /* name prop */
    NULL,                 /* icon */

    acf_fcurve_setting_valid, /* has setting */
    acf_fcurve_setting_flag,  /* flag for setting */
    acf_fcurve_setting_ptr,   /* pointer for setting */
};

/* Object Action Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_fillactd_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_ACTION;
}

/* check if some setting exists for this channel */
static bool acf_fillactd_setting_valid(bAnimContext *UNUSED(ac),
                                       bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_fillactd_setting_flag(bAnimContext *UNUSED(ac),
                                     eAnimChannel_Settings setting,
                                     bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *neg = true;
      return ACT_COLLAPSED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_fillactd_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *type)
{
  bAction *act = (bAction *)ale->data;
  AnimData *adt = ale->adt;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      if (adt) {
        return GET_ACF_FLAG_PTR(adt->flag, type);
      }
      return NULL;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(act->flag, type);

    default: /* unsupported */
      return NULL;
  }
}

/* object action expander type define */
static bAnimChannelType ACF_FILLACTD = {
    "Ob-Action Filler",     /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_fillactd_icon,            /* icon */

    acf_fillactd_setting_valid, /* has setting */
    acf_fillactd_setting_flag,  /* flag for setting */
    acf_fillactd_setting_ptr,   /* pointer for setting */
};

/* Drivers Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_filldrivers_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_DRIVER;
}

static void acf_filldrivers_name(bAnimListElem *UNUSED(ale), char *name)
{
  BLI_strncpy(name, IFACE_("Drivers"), ANIM_CHAN_NAME_SIZE);
}

/* check if some setting exists for this channel */
// TODO: this could be made more generic
static bool acf_filldrivers_setting_valid(bAnimContext *UNUSED(ac),
                                          bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_filldrivers_setting_flag(bAnimContext *UNUSED(ac),
                                        eAnimChannel_Settings setting,
                                        bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      *neg = true;
      return ADT_DRIVERS_COLLAPSED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_filldrivers_setting_ptr(bAnimListElem *ale,
                                         eAnimChannel_Settings setting,
                                         short *type)
{
  AnimData *adt = (AnimData *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(adt->flag, type);

    default: /* unsupported */
      return NULL;
  }
}

/* drivers expander type define */
static bAnimChannelType ACF_FILLDRIVERS = {
    "Drivers Filler",       /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_filldrivers_name, /* name */
    NULL,                 /* name prop */
    acf_filldrivers_icon, /* icon */

    acf_filldrivers_setting_valid, /* has setting */
    acf_filldrivers_setting_flag,  /* flag for setting */
    acf_filldrivers_setting_ptr,   /* pointer for setting */
};

/* Material Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmat_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_MATERIAL_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmat_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MA_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmat_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Material *ma = (Material *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(ma->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (ma->adt) {
        return GET_ACF_FLAG_PTR(ma->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* material expander type define */
static bAnimChannelType ACF_DSMAT = {
    "Material Data Expander", /* type name */
    ACHANNEL_ROLE_EXPANDER,   /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsmat_icon,                /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsmat_setting_flag,               /* flag for setting */
    acf_dsmat_setting_ptr,                /* pointer for setting */
};

/* Light Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dslight_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_LIGHT_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dslight_setting_flag(bAnimContext *UNUSED(ac),
                                    eAnimChannel_Settings setting,
                                    bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LA_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dslight_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *type)
{
  Light *la = (Light *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(la->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (la->adt) {
        return GET_ACF_FLAG_PTR(la->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* light expander type define */
static bAnimChannelType ACF_DSLIGHT = {
    "Light Expander",       /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dslight_icon,              /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dslight_setting_flag,             /* flag for setting */
    acf_dslight_setting_ptr,              /* pointer for setting */
};

/* Texture Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dstex_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_TEXTURE_DATA;
}

/* offset for texture expanders */
// FIXME: soon to be obsolete?
static short acf_dstex_offset(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale))
{
  return 14;  // XXX: simply include this in indention instead?
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dstex_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return TEX_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dstex_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Tex *tex = (Tex *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(tex->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (tex->adt) {
        return GET_ACF_FLAG_PTR(tex->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* texture expander type define */
static bAnimChannelType ACF_DSTEX = {
    "Texture Data Expander", /* type name */
    ACHANNEL_ROLE_EXPANDER,  /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_dstex_offset,                /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_dstex_icon,               /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dstex_setting_flag,               /* flag for setting */
    acf_dstex_setting_ptr,                /* pointer for setting */
};

/* Camera Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscachefile_icon(bAnimListElem *ale)
{
  UNUSED_VARS(ale);
  return ICON_FILE;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscachefile_setting_flag(bAnimContext *ac, eAnimChannel_Settings setting, bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return CACHEFILE_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
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
                                         short *type)
{
  CacheFile *cache_file = (CacheFile *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(cache_file->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (cache_file->adt) {
        return GET_ACF_FLAG_PTR(cache_file->adt->flag, type);
      }

      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* CacheFile expander type define. */
static bAnimChannelType ACF_DSCACHEFILE = {
    "Cache File Expander",  /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_dscachefile_icon,         /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dscachefile_setting_flag,         /* flag for setting */
    acf_dscachefile_setting_ptr,          /* pointer for setting */
};

/* Camera Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscam_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_CAMERA_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscam_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return CAM_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      return ADT_CURVES_ALWAYS_VISIBLE;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dscam_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Camera *ca = (Camera *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(ca->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      if (ca->adt) {
        return GET_ACF_FLAG_PTR(ca->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* camera expander type define */
static bAnimChannelType ACF_DSCAM = {
    "Camera Expander",      /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_dscam_icon,               /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dscam_setting_flag,               /* flag for setting */
    acf_dscam_setting_ptr,                /* pointer for setting */
};

/* Curve Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dscur_icon(bAnimListElem *ale)
{
  Curve *cu = (Curve *)ale->data;
  short obtype = BKE_curve_type_get(cu);

  switch (obtype) {
    case OB_FONT:
      return ICON_FONT_DATA;
    case OB_SURF:
      return ICON_SURFACE_DATA;
    default:
      return ICON_CURVE_DATA;
  }
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dscur_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return CU_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dscur_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Curve *cu = (Curve *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(cu->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (cu->adt) {
        return GET_ACF_FLAG_PTR(cu->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* curve expander type define */
static bAnimChannelType ACF_DSCUR = {
    "Curve Expander",       /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dscur_icon,                /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dscur_setting_flag,               /* flag for setting */
    acf_dscur_setting_ptr,                /* pointer for setting */
};

/* Shape Key Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsskey_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_SHAPEKEY_DATA;
}

/* check if some setting exists for this channel */
static bool acf_dsskey_setting_valid(bAnimContext *ac,
                                     bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsskey_setting_flag(bAnimContext *UNUSED(ac),
                                   eAnimChannel_Settings setting,
                                   bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return KEY_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsskey_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Key *key = (Key *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(key->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (key->adt) {
        return GET_ACF_FLAG_PTR(key->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* shapekey expander type define */
static bAnimChannelType ACF_DSSKEY = {
    "Shape Key Expander",   /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsskey_icon,               /* icon */

    acf_dsskey_setting_valid, /* has setting */
    acf_dsskey_setting_flag,  /* flag for setting */
    acf_dsskey_setting_ptr,   /* pointer for setting */
};

/* World Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dswor_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_WORLD_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dswor_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return WO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dswor_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  World *wo = (World *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(wo->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (wo->adt) {
        return GET_ACF_FLAG_PTR(wo->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* world expander type define */
static bAnimChannelType ACF_DSWOR = {
    "World Expander",       /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_dswor_icon,               /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dswor_setting_flag,               /* flag for setting */
    acf_dswor_setting_ptr,                /* pointer for setting */
};

/* Particle Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dspart_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_PARTICLE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dspart_setting_flag(bAnimContext *UNUSED(ac),
                                   eAnimChannel_Settings setting,
                                   bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return PART_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dspart_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  ParticleSettings *part = (ParticleSettings *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(part->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (part->adt) {
        return GET_ACF_FLAG_PTR(part->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* particle expander type define */
static bAnimChannelType ACF_DSPART = {
    "Particle Data Expander", /* type name */
    ACHANNEL_ROLE_EXPANDER,   /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dspart_icon,               /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dspart_setting_flag,              /* flag for setting */
    acf_dspart_setting_ptr,               /* pointer for setting */
};

/* MetaBall Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmball_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_META_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmball_setting_flag(bAnimContext *UNUSED(ac),
                                    eAnimChannel_Settings setting,
                                    bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MB_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmball_setting_ptr(bAnimListElem *ale,
                                     eAnimChannel_Settings setting,
                                     short *type)
{
  MetaBall *mb = (MetaBall *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(mb->flag2, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (mb->adt) {
        return GET_ACF_FLAG_PTR(mb->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* metaball expander type define */
static bAnimChannelType ACF_DSMBALL = {
    "Metaball Expander",    /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsmball_icon,              /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsmball_setting_flag,             /* flag for setting */
    acf_dsmball_setting_ptr,              /* pointer for setting */
};

/* Armature Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsarm_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_ARMATURE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsarm_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return ARM_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsarm_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  bArmature *arm = (bArmature *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(arm->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (arm->adt) {
        return GET_ACF_FLAG_PTR(arm->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* metaball expander type define */
static bAnimChannelType ACF_DSARM = {
    "Armature Expander",    /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsarm_icon,                /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsarm_setting_flag,               /* flag for setting */
    acf_dsarm_setting_ptr,                /* pointer for setting */
};

/* NodeTree Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsntree_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_NODETREE;
}

/* offset for nodetree expanders */
static short acf_dsntree_offset(bAnimContext *ac, bAnimListElem *ale)
{
  bNodeTree *ntree = (bNodeTree *)ale->data;
  short offset = acf_generic_basic_offset(ac, ale);

  offset += acf_nodetree_rootType_offset(ntree);

  return offset;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsntree_setting_flag(bAnimContext *UNUSED(ac),
                                    eAnimChannel_Settings setting,
                                    bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return NTREE_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
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
                                     short *type)
{
  bNodeTree *ntree = (bNodeTree *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(ntree->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (ntree->adt) {
        return GET_ACF_FLAG_PTR(ntree->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* node tree expander type define */
static bAnimChannelType ACF_DSNTREE = {
    "Node Tree Expander",   /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_dsntree_offset,              /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsntree_icon,              /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsntree_setting_flag,             /* flag for setting */
    acf_dsntree_setting_ptr,              /* pointer for setting */
};

/* LineStyle Expander  ------------------------------------------- */

/* TODO: just get this from RNA? */
static int acf_dslinestyle_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_LINE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dslinestyle_setting_flag(bAnimContext *UNUSED(ac),
                                        eAnimChannel_Settings setting,
                                        bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LS_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
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
                                         short *type)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(linestyle->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (linestyle->adt) {
        return GET_ACF_FLAG_PTR(linestyle->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* node tree expander type define */
static bAnimChannelType ACF_DSLINESTYLE = {
    "Line Style Expander",  /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dslinestyle_icon,          /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dslinestyle_setting_flag,         /* flag for setting */
    acf_dslinestyle_setting_ptr,          /* pointer for setting */
};

/* Mesh Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmesh_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_MESH_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmesh_setting_flag(bAnimContext *UNUSED(ac),
                                   eAnimChannel_Settings setting,
                                   bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return ME_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsmesh_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Mesh *me = (Mesh *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(me->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (me->adt) {
        return GET_ACF_FLAG_PTR(me->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* node tree expander type define */
static bAnimChannelType ACF_DSMESH = {
    "Mesh Expander",        /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,
    /* indent level */        // XXX this only works for compositing
    acf_generic_basic_offset, /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsmesh_icon,               /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsmesh_setting_flag,              /* flag for setting */
    acf_dsmesh_setting_ptr,               /* pointer for setting */
};

/* Lattice Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dslat_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_LATTICE_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dslat_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return LT_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dslat_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Lattice *lt = (Lattice *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(lt->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (lt->adt) {
        return GET_ACF_FLAG_PTR(lt->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* node tree expander type define */
static bAnimChannelType ACF_DSLAT = {
    "Lattice Expander",     /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,
    /* indent level */        // XXX this only works for compositing
    acf_generic_basic_offset, /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dslat_icon,                /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dslat_setting_flag,               /* flag for setting */
    acf_dslat_setting_ptr,                /* pointer for setting */
};

/* Speaker Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsspk_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_SPEAKER;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsspk_setting_flag(bAnimContext *UNUSED(ac),
                                  eAnimChannel_Settings setting,
                                  bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return SPK_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsspk_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Speaker *spk = (Speaker *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(spk->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (spk->adt) {
        return GET_ACF_FLAG_PTR(spk->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* speaker expander type define */
static bAnimChannelType ACF_DSSPK = {
    "Speaker Expander",     /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsspk_icon,                /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsspk_setting_flag,               /* flag for setting */
    acf_dsspk_setting_ptr,                /* pointer for setting */
};

/* Hair Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dshair_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_HAIR_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dshair_setting_flag(bAnimContext *UNUSED(ac),
                                   eAnimChannel_Settings setting,
                                   bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return VO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dshair_setting_ptr(bAnimListElem *ale, eAnimChannel_Settings setting, short *type)
{
  Hair *hair = (Hair *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(hair->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (hair->adt)
        return GET_ACF_FLAG_PTR(hair->adt->flag, type);
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* hair expander type define */
static bAnimChannelType ACF_DSHAIR = {
    "Hair Expander",        /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dshair_icon,               /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dshair_setting_flag,              /* flag for setting */
    acf_dshair_setting_ptr                /* pointer for setting */
};

/* PointCloud Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dspointcloud_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_POINTCLOUD_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dspointcloud_setting_flag(bAnimContext *UNUSED(ac),
                                         eAnimChannel_Settings setting,
                                         bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return VO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dspointcloud_setting_ptr(bAnimListElem *ale,
                                          eAnimChannel_Settings setting,
                                          short *type)
{
  PointCloud *pointcloud = (PointCloud *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(pointcloud->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (pointcloud->adt)
        return GET_ACF_FLAG_PTR(pointcloud->adt->flag, type);
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* pointcloud expander type define */
static bAnimChannelType ACF_DSPOINTCLOUD = {
    "PointCloud Expander",  /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dspointcloud_icon,         /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dspointcloud_setting_flag,        /* flag for setting */
    acf_dspointcloud_setting_ptr          /* pointer for setting */
};

/* Volume Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsvolume_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_VOLUME_DATA;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsvolume_setting_flag(bAnimContext *UNUSED(ac),
                                     eAnimChannel_Settings setting,
                                     bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return VO_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_dsvolume_setting_ptr(bAnimListElem *ale,
                                      eAnimChannel_Settings setting,
                                      short *type)
{
  Volume *volume = (Volume *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(volume->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (volume->adt)
        return GET_ACF_FLAG_PTR(volume->adt->flag, type);
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* volume expander type define */
static bAnimChannelType ACF_DSVOLUME = {
    "Volume Expander",      /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsvolume_icon,             /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsvolume_setting_flag,            /* flag for setting */
    acf_dsvolume_setting_ptr              /* pointer for setting */
};

/* Simulation Expander ----------------------------------------- */

static int acf_dssimulation_icon(bAnimListElem *UNUSED(ale))
{
  /* TODO: Use correct icon. */
  return ICON_PHYSICS;
}

static int acf_dssimulation_setting_flag(bAnimContext *UNUSED(ac),
                                         eAnimChannel_Settings setting,
                                         bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return SIM_DS_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
      return ADT_CURVES_NOT_VISIBLE;

    case ACHANNEL_SETTING_SELECT: /* selected */
      return ADT_UI_SELECTED;

    default: /* unsupported */
      return 0;
  }
}

static void *acf_dssimulation_setting_ptr(bAnimListElem *ale,
                                          eAnimChannel_Settings setting,
                                          short *type)
{
  Simulation *simulation = (Simulation *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(simulation->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (simulation->adt)
        return GET_ACF_FLAG_PTR(simulation->adt->flag, type);
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

static bAnimChannelType ACF_DSSIMULATION = {
    "Simulation Expander",  /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dssimulation_icon,         /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dssimulation_setting_flag,        /* flag for setting */
    acf_dssimulation_setting_ptr          /* pointer for setting */
};

/* GPencil Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsgpencil_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_GREASEPENCIL;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsgpencil_setting_flag(bAnimContext *UNUSED(ac),
                                      eAnimChannel_Settings setting,
                                      bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GP_DATA_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
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
                                       short *type)
{
  bGPdata *gpd = (bGPdata *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(gpd->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (gpd->adt) {
        return GET_ACF_FLAG_PTR(gpd->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* grease pencil expander type define */
static bAnimChannelType ACF_DSGPENCIL = {
    "GPencil DS Expander",  /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,      /* name */
    acf_generic_idblock_name_prop, /* name prop */
    acf_dsgpencil_icon,            /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsgpencil_setting_flag,           /* flag for setting */
    acf_dsgpencil_setting_ptr,            /* pointer for setting */
};

/* World Expander  ------------------------------------------- */

// TODO: just get this from RNA?
static int acf_dsmclip_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_SEQUENCE;
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_dsmclip_setting_flag(bAnimContext *UNUSED(ac),
                                    eAnimChannel_Settings setting,
                                    bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return MCLIP_DATA_EXPAND;

    case ACHANNEL_SETTING_MUTE: /* mute (only in NLA) */
      return ADT_NLA_EVAL_OFF;

    case ACHANNEL_SETTING_VISIBLE: /* visible (only in Graph Editor) */
      *neg = true;
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
                                     short *type)
{
  MovieClip *clip = (MovieClip *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GET_ACF_FLAG_PTR(clip->flag, type);

    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted (for NLA only) */
    case ACHANNEL_SETTING_VISIBLE: /* visible (for Graph Editor only) */
      if (clip->adt != NULL) {
        return GET_ACF_FLAG_PTR(clip->adt->flag, type);
      }
      return NULL;

    default: /* unsupported */
      return NULL;
  }
}

/* world expander type define */
static bAnimChannelType ACF_DSMCLIP = {
    "Movieclip Expander",   /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_generic_dataexpand_color,    /* backdrop color */
    acf_generic_dataexpand_backdrop, /* backdrop */
    acf_generic_indention_1,         /* indent level */
    acf_generic_basic_offset,        /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_dsmclip_icon,             /* icon */

    acf_generic_dataexpand_setting_valid, /* has setting */
    acf_dsmclip_setting_flag,             /* flag for setting */
    acf_dsmclip_setting_ptr,              /* pointer for setting */
};

/* ShapeKey Entry  ------------------------------------------- */

/* name for ShapeKey */
static void acf_shapekey_name(bAnimListElem *ale, char *name)
{
  KeyBlock *kb = (KeyBlock *)ale->data;

  /* just copy the name... */
  if (kb && name) {
    /* if the KeyBlock had a name, use it, otherwise use the index */
    if (kb->name[0]) {
      BLI_strncpy(name, kb->name, ANIM_CHAN_NAME_SIZE);
    }
    else {
      BLI_snprintf(name, ANIM_CHAN_NAME_SIZE, IFACE_("Key %d"), ale->index);
    }
  }
}

/* name property for ShapeKey entries */
static bool acf_shapekey_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  KeyBlock *kb = (KeyBlock *)ale->data;

  /* if the KeyBlock had a name, use it, otherwise use the index */
  if (kb && kb->name[0]) {
    RNA_pointer_create(ale->id, &RNA_ShapeKey, kb, ptr);
    *prop = RNA_struct_name_property(ptr->type);

    return (*prop != NULL);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_shapekey_setting_valid(bAnimContext *UNUSED(ac),
                                       bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_shapekey_setting_flag(bAnimContext *UNUSED(ac),
                                     eAnimChannel_Settings setting,
                                     bool *neg)
{
  /* clear extra return data first */
  *neg = false;

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
                                      short *type)
{
  KeyBlock *kb = (KeyBlock *)ale->data;

  /* clear extra return data first */
  *type = 0;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT:  /* selected */
    case ACHANNEL_SETTING_MUTE:    /* muted */
    case ACHANNEL_SETTING_PROTECT: /* protected */
      return GET_ACF_FLAG_PTR(kb->flag, type);

    default: /* unsupported */
      return NULL;
  }
}

/* shapekey expander type define */
static bAnimChannelType ACF_SHAPEKEY = {
    "Shape Key",           /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_generic_channel_color,    /* backdrop color */
    acf_generic_channel_backdrop, /* backdrop */
    acf_generic_indention_0,      /* indent level */
    acf_generic_basic_offset,     /* offset */

    acf_shapekey_name,      /* name */
    acf_shapekey_name_prop, /* name prop */
    NULL,                   /* icon */

    acf_shapekey_setting_valid, /* has setting */
    acf_shapekey_setting_flag,  /* flag for setting */
    acf_shapekey_setting_ptr,   /* pointer for setting */
};

/* GPencil Datablock ------------------------------------------- */

/* get backdrop color for gpencil datablock widget */
static void acf_gpd_color(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), float r_color[3])
{
  /* these are ID-blocks, but not exactly standalone... */
  UI_GetThemeColorShade3fv(TH_DOPESHEET_CHANNELSUBOB, 20, r_color);
}

// TODO: just get this from RNA?
static int acf_gpd_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_GREASEPENCIL;
}

/* check if some setting exists for this channel */
static bool acf_gpd_setting_valid(bAnimContext *UNUSED(ac),
                                  bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_gpd_setting_flag(bAnimContext *UNUSED(ac), eAnimChannel_Settings setting, bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return AGRP_SELECTED;

    case ACHANNEL_SETTING_EXPAND: /* expanded */
      return GP_DATA_EXPAND;

    default:
      /* these shouldn't happen */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_gpd_setting_ptr(bAnimListElem *ale,
                                 eAnimChannel_Settings UNUSED(setting),
                                 short *type)
{
  bGPdata *gpd = (bGPdata *)ale->data;

  /* all flags are just in gpd->flag for now... */
  return GET_ACF_FLAG_PTR(gpd->flag, type);
}

/* gpencil datablock type define */
static bAnimChannelType ACF_GPD = {
    "GPencil Datablock",    /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_gpd_color,            /* backdrop color */
    acf_group_backdrop,       /* backdrop */
    acf_generic_indention_0,  /* indent level */
    acf_generic_group_offset, /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_gpd_icon,                 /* icon */

    acf_gpd_setting_valid, /* has setting */
    acf_gpd_setting_flag,  /* flag for setting */
    acf_gpd_setting_ptr,   /* pointer for setting */
};

/* GPencil Layer ------------------------------------------- */

/* name for grease pencil layer entries */
static void acf_gpl_name(bAnimListElem *ale, char *name)
{
  bGPDlayer *gpl = (bGPDlayer *)ale->data;

  if (gpl && name) {
    BLI_strncpy(name, gpl->info, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for grease pencil layer entries */
static bool acf_gpl_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  if (ale->data) {
    RNA_pointer_create(ale->id, &RNA_GPencilLayer, ale->data, ptr);
    *prop = RNA_struct_name_property(ptr->type);

    return (*prop != NULL);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_gpl_setting_valid(bAnimContext *UNUSED(ac),
                                  bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_gpl_setting_flag(bAnimContext *UNUSED(ac), eAnimChannel_Settings setting, bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_SELECT: /* selected */
      return GP_LAYER_SELECT;

    case ACHANNEL_SETTING_MUTE: /* animation muting - similar to frame lock... */
      return GP_LAYER_FRAMELOCK;

    case ACHANNEL_SETTING_VISIBLE: /* visibility of the layers (NOT muting) */
      *neg = true;
      return GP_LAYER_HIDE;

    case ACHANNEL_SETTING_PROTECT: /* protected */
      return GP_LAYER_LOCKED;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_gpl_setting_ptr(bAnimListElem *ale,
                                 eAnimChannel_Settings UNUSED(setting),
                                 short *type)
{
  bGPDlayer *gpl = (bGPDlayer *)ale->data;

  /* all flags are just in gpl->flag for now... */
  return GET_ACF_FLAG_PTR(gpl->flag, type);
}

/* grease pencil layer type define */
static bAnimChannelType ACF_GPL = {
    "GPencil Layer",       /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_gpencil_channel_color,      /* backdrop color */
    acf_generic_channel_backdrop,   /* backdrop */
    acf_generic_indention_flexible, /* indent level */
    acf_generic_group_offset,       /* offset */

    acf_gpl_name,      /* name */
    acf_gpl_name_prop, /* name prop */
    NULL,              /* icon */

    acf_gpl_setting_valid, /* has setting */
    acf_gpl_setting_flag,  /* flag for setting */
    acf_gpl_setting_ptr,   /* pointer for setting */
};

/* Mask Datablock ------------------------------------------- */

/* get backdrop color for mask datablock widget */
static void acf_mask_color(bAnimContext *UNUSED(ac), bAnimListElem *UNUSED(ale), float r_color[3])
{
  /* these are ID-blocks, but not exactly standalone... */
  UI_GetThemeColorShade3fv(TH_DOPESHEET_CHANNELSUBOB, 20, r_color);
}

// TODO: just get this from RNA?
static int acf_mask_icon(bAnimListElem *UNUSED(ale))
{
  return ICON_MOD_MASK;
}

/* check if some setting exists for this channel */
static bool acf_mask_setting_valid(bAnimContext *UNUSED(ac),
                                   bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_mask_setting_flag(bAnimContext *UNUSED(ac),
                                 eAnimChannel_Settings setting,
                                 bool *neg)
{
  /* clear extra return data first */
  *neg = false;

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
                                  eAnimChannel_Settings UNUSED(setting),
                                  short *type)
{
  Mask *mask = (Mask *)ale->data;

  /* all flags are just in mask->flag for now... */
  return GET_ACF_FLAG_PTR(mask->flag, type);
}

/* mask datablock type define */
static bAnimChannelType ACF_MASKDATA = {
    "Mask Datablock",       /* type name */
    ACHANNEL_ROLE_EXPANDER, /* role */

    acf_mask_color,           /* backdrop color */
    acf_group_backdrop,       /* backdrop */
    acf_generic_indention_0,  /* indent level */
    acf_generic_group_offset, /* offset */

    acf_generic_idblock_name,     /* name */
    acf_generic_idfill_name_prop, /* name prop */
    acf_mask_icon,                /* icon */

    acf_mask_setting_valid, /* has setting */
    acf_mask_setting_flag,  /* flag for setting */
    acf_mask_setting_ptr,   /* pointer for setting */
};

/* Mask Layer ------------------------------------------- */

/* name for grease pencil layer entries */
static void acf_masklay_name(bAnimListElem *ale, char *name)
{
  MaskLayer *masklay = (MaskLayer *)ale->data;

  if (masklay && name) {
    BLI_strncpy(name, masklay->name, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for grease pencil layer entries */
static bool acf_masklay_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  if (ale->data) {
    RNA_pointer_create(ale->id, &RNA_MaskLayer, ale->data, ptr);
    *prop = RNA_struct_name_property(ptr->type);

    return (*prop != NULL);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_masklay_setting_valid(bAnimContext *UNUSED(ac),
                                      bAnimListElem *UNUSED(ale),
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

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_masklay_setting_flag(bAnimContext *UNUSED(ac),
                                    eAnimChannel_Settings setting,
                                    bool *neg)
{
  /* clear extra return data first */
  *neg = false;

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
                                     eAnimChannel_Settings UNUSED(setting),
                                     short *type)
{
  MaskLayer *masklay = (MaskLayer *)ale->data;

  /* all flags are just in masklay->flag for now... */
  return GET_ACF_FLAG_PTR(masklay->flag, type);
}

/* grease pencil layer type define */
static bAnimChannelType ACF_MASKLAYER = {
    "Mask Layer",          /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_generic_channel_color,      /* backdrop color */
    acf_generic_channel_backdrop,   /* backdrop */
    acf_generic_indention_flexible, /* indent level */
    acf_generic_group_offset,       /* offset */

    acf_masklay_name,      /* name */
    acf_masklay_name_prop, /* name prop */
    NULL,                  /* icon */

    acf_masklay_setting_valid, /* has setting */
    acf_masklay_setting_flag,  /* flag for setting */
    acf_masklay_setting_ptr,   /* pointer for setting */
};

/* NLA Track ----------------------------------------------- */

/* get backdrop color for nla track channels */
static void acf_nlatrack_color(bAnimContext *UNUSED(ac), bAnimListElem *ale, float r_color[3])
{
  NlaTrack *nlt = (NlaTrack *)ale->data;
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
  NlaTrack *nlt = (NlaTrack *)ale->data;

  if (nlt && name) {
    BLI_strncpy(name, nlt->name, ANIM_CHAN_NAME_SIZE);
  }
}

/* name property for nla track entries */
static bool acf_nlatrack_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  if (ale->data) {
    RNA_pointer_create(ale->id, &RNA_NlaTrack, ale->data, ptr);
    *prop = RNA_struct_name_property(ptr->type);

    return (*prop != NULL);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_nlatrack_setting_valid(bAnimContext *UNUSED(ac),
                                       bAnimListElem *ale,
                                       eAnimChannel_Settings setting)
{
  NlaTrack *nlt = (NlaTrack *)ale->data;
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
          else {
            /* not ok - we've got a solo track, but this isn't it, so make it more obvious */
            return false;
          }
        }

        /* ok - no tracks are solo'd, and this isn't being tweaked */
        return true;
      }
      else {
        /* unsupported - this track is being tweaked */
        return false;
      }

    /* unsupported */
    default:
      return false;
  }
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_nlatrack_setting_flag(bAnimContext *UNUSED(ac),
                                     eAnimChannel_Settings setting,
                                     bool *neg)
{
  /* clear extra return data first */
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
                                      eAnimChannel_Settings UNUSED(setting),
                                      short *type)
{
  NlaTrack *nlt = (NlaTrack *)ale->data;
  return GET_ACF_FLAG_PTR(nlt->flag, type);
}

/* nla track type define */
static bAnimChannelType ACF_NLATRACK = {
    "NLA Track",           /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_nlatrack_color,             /* backdrop color */
    acf_generic_channel_backdrop,   /* backdrop */
    acf_generic_indention_flexible, /* indent level */
    acf_generic_group_offset,
    /* offset */  // XXX?

    acf_nlatrack_name,      /* name */
    acf_nlatrack_name_prop, /* name prop */
    NULL,                   /* icon */

    acf_nlatrack_setting_valid, /* has setting */
    acf_nlatrack_setting_flag,  /* flag for setting */
    acf_nlatrack_setting_ptr,   /* pointer for setting */
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
  else {
    return ICON_ACTION;
  }
}

/* Backdrop color for nla action channel
 * Although this can't be used directly for NLA Action drawing,
 * it is still needed for use behind the RHS toggles
 */
static void acf_nlaaction_color(bAnimContext *UNUSED(ac), bAnimListElem *ale, float r_color[3])
{
  float color[4];

  /* Action Line
   *   The alpha values action_get_color returns are only useful for drawing
   *   strips backgrounds but here we're doing channel list backgrounds instead
   *   so we ignore that and use our own when needed
   */
  nla_action_get_color(ale->adt, (bAction *)ale->data, color);

  /* NOTE: since the return types only allow rgb, we cannot do the alpha-blending we'd
   * like for the solo-drawing case. Hence, this method isn't actually used for drawing
   * most of the channel...
   */
  copy_v3_v3(r_color, color);
}

/* backdrop for nla action channel */
static void acf_nlaaction_backdrop(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  AnimData *adt = ale->adt;
  short offset = (acf->get_offset) ? acf->get_offset(ac, ale) : 0;
  float color[4];

  /* Action Line
   *   The alpha values action_get_color returns are only useful for drawing
   *   strips backgrounds but here we're doing channel list backgrounds instead
   *   so we ignore that and use our own when needed
   */
  nla_action_get_color(adt, (bAction *)ale->data, color);

  if (adt && (adt->flag & ADT_NLA_EDIT_ON)) {
    color[3] = 1.0f;
  }
  else {
    color[3] = (adt && (adt->flag & ADT_NLA_SOLO_TRACK)) ? 0.3f : 1.0f;
  }

  /* only on top left corner, to show that this channel sits on top of the preceding ones
   * while still linking into the action line strip to the right
   */
  UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT);

  /* draw slightly shifted up vertically to look like it has more separation from other channels,
   * but we then need to slightly shorten it so that it doesn't look like it overlaps
   */
  UI_draw_roundbox_4fv(true,
                       offset,
                       yminc + NLACHANNEL_SKIP,
                       (float)v2d->cur.xmax,
                       ymaxc + NLACHANNEL_SKIP - 1,
                       8,
                       color);
}

/* name for nla action entries */
static void acf_nlaaction_name(bAnimListElem *ale, char *name)
{
  bAction *act = (bAction *)ale->data;

  if (name) {
    if (act) {
      // TODO: add special decoration when doing this in tweaking mode?
      BLI_strncpy(name, act->id.name + 2, ANIM_CHAN_NAME_SIZE);
    }
    else {
      BLI_strncpy(name, "<No Action>", ANIM_CHAN_NAME_SIZE);
    }
  }
}

/* name property for nla action entries */
static bool acf_nlaaction_name_prop(bAnimListElem *ale, PointerRNA *ptr, PropertyRNA **prop)
{
  if (ale->data) {
    RNA_pointer_create(ale->id, &RNA_Action, ale->data, ptr);
    *prop = RNA_struct_name_property(ptr->type);

    return (*prop != NULL);
  }

  return false;
}

/* check if some setting exists for this channel */
static bool acf_nlaaction_setting_valid(bAnimContext *UNUSED(ac),
                                        bAnimListElem *ale,
                                        eAnimChannel_Settings setting)
{
  AnimData *adt = ale->adt;

  /* visibility of settings depends on various states... */
  switch (setting) {
    /* conditionally supported */
    case ACHANNEL_SETTING_PINNED: /* pinned - map/unmap */
      if ((adt) && (adt->flag & ADT_NLA_EDIT_ON)) {
        /* this should only appear in tweakmode */
        return true;
      }
      else {
        return false;
      }

    /* unsupported */
    default:
      return false;
  }
}

/* get the appropriate flag(s) for the setting when it is valid  */
static int acf_nlaaction_setting_flag(bAnimContext *UNUSED(ac),
                                      eAnimChannel_Settings setting,
                                      bool *neg)
{
  /* clear extra return data first */
  *neg = false;

  switch (setting) {
    case ACHANNEL_SETTING_PINNED: /* pinned - map/unmap */
      *neg = true;                // XXX
      return ADT_NLA_EDIT_NOMAP;

    default: /* unsupported */
      return 0;
  }
}

/* get pointer to the setting */
static void *acf_nlaaction_setting_ptr(bAnimListElem *ale,
                                       eAnimChannel_Settings UNUSED(setting),
                                       short *type)
{
  AnimData *adt = ale->adt;
  return GET_ACF_FLAG_PTR(adt->flag, type);
}

/* nla action type define */
static bAnimChannelType ACF_NLAACTION = {
    "NLA Active Action",   /* type name */
    ACHANNEL_ROLE_CHANNEL, /* role */

    acf_nlaaction_color,            /* backdrop color (NOTE: the backdrop handles this too,
                                     * since it needs special hacks). */
    acf_nlaaction_backdrop,         /* backdrop */
    acf_generic_indention_flexible, /* indent level */
    acf_generic_group_offset,
    /* offset */  // XXX?

    acf_nlaaction_name,      /* name */
    acf_nlaaction_name_prop, /* name prop */
    acf_nlaaction_icon,      /* icon */

    acf_nlaaction_setting_valid, /* has setting */
    acf_nlaaction_setting_flag,  /* flag for setting */
    acf_nlaaction_setting_ptr,   /* pointer for setting */
};

/* *********************************************** */
/* Type Registration and General Access */

/* These globals only ever get directly accessed in this file */
static bAnimChannelType *animchannelTypeInfo[ANIMTYPE_NUM_TYPES];
static short ACF_INIT = 1; /* when non-zero, the list needs to be updated */

/* Initialize type info definitions */
static void ANIM_init_channel_typeinfo_data(void)
{
  int type = 0;

  /* start initializing if necessary... */
  if (ACF_INIT) {
    ACF_INIT = 0;

    /* NOTE: need to keep the order of these synchronized with the definition of
     * channel types (eAnim_ChannelType) in ED_anim_api.h
     */
    animchannelTypeInfo[type++] = NULL; /* None */
    animchannelTypeInfo[type++] = NULL; /* AnimData */
    animchannelTypeInfo[type++] = NULL; /* Special */

    animchannelTypeInfo[type++] = &ACF_SUMMARY; /* Motion Summary */

    animchannelTypeInfo[type++] = &ACF_SCENE;  /* Scene */
    animchannelTypeInfo[type++] = &ACF_OBJECT; /* Object */
    animchannelTypeInfo[type++] = &ACF_GROUP;  /* Group */
    animchannelTypeInfo[type++] = &ACF_FCURVE; /* F-Curve */

    animchannelTypeInfo[type++] = &ACF_NLACONTROLS; /* NLA Control FCurve Expander */
    animchannelTypeInfo[type++] = &ACF_NLACURVE;    /* NLA Control FCurve Channel */

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
    animchannelTypeInfo[type++] = &ACF_DSHAIR;       /* Hair Channel */
    animchannelTypeInfo[type++] = &ACF_DSPOINTCLOUD; /* PointCloud Channel */
    animchannelTypeInfo[type++] = &ACF_DSVOLUME;     /* Volume Channel */
    animchannelTypeInfo[type++] = &ACF_DSSIMULATION; /* Simulation Channel */

    animchannelTypeInfo[type++] = &ACF_SHAPEKEY; /* ShapeKey */

    animchannelTypeInfo[type++] = &ACF_GPD; /* Grease Pencil Datablock */
    animchannelTypeInfo[type++] = &ACF_GPL; /* Grease Pencil Layer */

    animchannelTypeInfo[type++] = &ACF_MASKDATA;  /* Mask Datablock */
    animchannelTypeInfo[type++] = &ACF_MASKLAYER; /* Mask Layer */

    animchannelTypeInfo[type++] = &ACF_NLATRACK;  /* NLA Track */
    animchannelTypeInfo[type++] = &ACF_NLAACTION; /* NLA Action */
  }
}

/* Get type info from given channel type */
const bAnimChannelType *ANIM_channel_get_typeinfo(bAnimListElem *ale)
{
  /* santiy checks */
  if (ale == NULL) {
    return NULL;
  }

  /* init the typeinfo if not available yet... */
  ANIM_init_channel_typeinfo_data();

  /* check if type is in bounds... */
  if ((ale->type >= 0) && (ale->type < ANIMTYPE_NUM_TYPES)) {
    return animchannelTypeInfo[ale->type];
  }
  else {
    return NULL;
  }
}

/* --------------------------- */

/* Print debug info string for the given channel */
void ANIM_channel_debug_print_info(bAnimListElem *ale, short indent_level)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

  /* print indents */
  for (; indent_level > 0; indent_level--) {
    printf("  ");
  }

  /* print info */
  if (acf) {
    char name[ANIM_CHAN_NAME_SIZE]; /* hopefully this will be enough! */

    /* get UI name */
    if (acf->name) {
      acf->name(ale, name);
    }
    else {
      BLI_strncpy(name, "<No name>", sizeof(name));
    }

    /* print type name + ui name */
    printf("ChanType: <%s> Name: \"%s\"\n", acf->channel_type_name, name);
  }
  else if (ale) {
    printf("ChanType: <Unknown - %d>\n", ale->type);
  }
  else {
    printf("<Invalid channel - NULL>\n");
  }
}

/* --------------------------- */

/* Check if some setting for a channel is enabled
 * Returns: 1 = On, 0 = Off, -1 = Invalid
 */
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
          else {
            return ((*val) & flag) != 0;
          }
        }
        case sizeof(short): /* short pointer for setting */
        {
          const short *val = (short *)ptr;

          if (negflag) {
            return ((*val) & flag) == 0;
          }
          else {
            return ((*val) & flag) != 0;
          }
        }
        case sizeof(char): /* char pointer for setting */
        {
          const char *val = (char *)ptr;

          if (negflag) {
            return ((*val) & flag) == 0;
          }
          else {
            return ((*val) & flag) != 0;
          }
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
      if (smode == ACHANNEL_SETFLAG_INVERT) \
        (sval) ^= (sflag); \
      else if (smode == ACHANNEL_SETFLAG_ADD) \
        (sval) &= ~(sflag); \
      else \
        (sval) |= (sflag); \
    } \
    else { \
      if (smode == ACHANNEL_SETFLAG_INVERT) \
        (sval) ^= (sflag); \
      else if (smode == ACHANNEL_SETFLAG_ADD) \
        (sval) |= (sflag); \
      else \
        (sval) &= ~(sflag); \
    } \
  } \
  (void)0

/* Change value of some setting for a channel
 * - setting: eAnimChannel_Settings
 * - mode: eAnimChannels_SetFlag
 */
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
    }
  }
}

/* --------------------------- */

// size of icons
#define ICON_WIDTH (0.85f * U.widget_unit)
// width of sliders
#define SLIDER_WIDTH (4 * U.widget_unit)
// min-width of rename textboxes
#define RENAME_TEXT_MIN_WIDTH (U.widget_unit)
// width of graph editor color bands
#define GRAPH_COLOR_BAND_WIDTH (0.3f * U.widget_unit)
// extra offset for the visibility icons in the graph editor
#define GRAPH_ICON_VISIBILITY_OFFSET (GRAPH_COLOR_BAND_WIDTH * 1.5f)

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

/* Draw the given channel */
void ANIM_channel_draw(
    bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc, size_t channel_index)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  short selected, offset;
  float y, ymid, ytext;

  /* sanity checks - don't draw anything */
  if (ELEM(NULL, acf, ale)) {
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
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  /* step 1) draw backdrop ...........................................  */
  if (acf->draw_backdrop) {
    acf->draw_backdrop(ac, ale, yminc, ymaxc);
  }

  /* step 2) draw expand widget ....................................... */
  if (acf->has_setting(ac, ale, ACHANNEL_SETTING_EXPAND)) {
    /* just skip - drawn as widget now */
    offset += ICON_WIDTH;
  }

  /* step 3) draw icon ............................................... */
  if (acf->icon) {
    UI_icon_draw(offset, ymid, acf->icon(ale));
    offset += ICON_WIDTH;
  }

  /* step 4) draw special toggles  .................................
   * - in Graph Editor, checkboxes for visibility in curves area
   * - in NLA Editor, glowing dots for solo/not solo...
   * - in Grease Pencil mode, color swatches for layer color
   */
  if (ac->sl) {
    if ((ac->spacetype == SPACE_GRAPH) &&
        (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE) ||
         acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE))) {
      /* for F-Curves, draw color-preview of curve left to the visibility icon */
      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        FCurve *fcu = (FCurve *)ale->data;
        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

        immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

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
      GPU_blend(false);

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
    else if ((ac->spacetype == SPACE_NLA) && acf->has_setting(ac, ale, ACHANNEL_SETTING_SOLO)) {
      /* just skip - drawn as widget now */
      offset += ICON_WIDTH;
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

    /* get name */
    acf->name(ale, name);

    offset += 3;
    UI_fontstyle_draw_simple(fstyle, offset, ytext, name, col);

    /* draw red underline if channel is disabled */
    if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE) && (ale->flag & FCURVE_DISABLED)) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      /* FIXME: replace hardcoded color here, and check on extents! */
      immUniformColor3f(1.0f, 0.0f, 0.0f);

      GPU_line_width(2.0f);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, (float)offset, yminc);
      immVertex2f(pos, (float)v2d->cur.xmax, yminc);
      immEnd();

      immUnbindProgram();
    }
  }

  /* step 6) draw backdrops behind mute+protection toggles + (sliders) ....................... */
  /*  - Reset offset - now goes from RHS of panel.
   *  - Exception for graph editor, which needs extra space for the scroll bar.
   */
  if (ac->spacetype == SPACE_GRAPH &&
      ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE, ANIMTYPE_GROUP)) {
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
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    /* get and set backdrop color */
    acf->get_backdrop_color(ac, ale, color);
    immUniformColor3fv(color);

    /* check if we need to show the sliders */
    if ((ac->sl) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH)) {
      switch (ac->spacetype) {
        case SPACE_ACTION: {
          SpaceAction *saction = (SpaceAction *)ac->sl;
          draw_sliders = (saction->flag & SACTION_SLIDERS);
          break;
        }
        case SPACE_GRAPH: {
          SpaceGraph *sipo = (SpaceGraph *)ac->sl;
          draw_sliders = (sipo->flag & SIPO_SLIDERS);
          break;
        }
      }
    }

    /* check if there's enough space for the toggles if the sliders are drawn too */
    if (!(draw_sliders) || (BLI_rcti_size_x(&v2d->mask) > ACHANNEL_BUTTON_WIDTH / 2)) {
      /* protect... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PROTECT)) {
        offset += ICON_WIDTH;
      }

      /* mute... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MUTE)) {
        offset += ICON_WIDTH;
      }

      /* grease pencil visibility... */
      if (ale->type == ANIMTYPE_GPLAYER) {
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

      /* NLA action channels have slightly different spacing requirements... */
      if (ale->type == ANIMTYPE_NLAACTION) {
        ymin_ofs = NLACHANNEL_SKIP;
      }
    }

    /* Draw slider:
     * - Even if we can draw sliders for this view,
     *   we must also check that the channel-type supports them
     *   (only only F-Curves really can support them for now).
     * - Slider should start before the toggles (if they're visible)
     *   to keep a clean line down the side.
     */
    if ((draw_sliders) &&
        ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE, ANIMTYPE_SHAPEKEY, ANIMTYPE_GPLAYER)) {
      /* adjust offset */
      offset += SLIDER_WIDTH;
    }

    /* Finally draw a backdrop rect behind these:
     * - Starts from the point where the first toggle/slider starts.
     * - Ends past the space that might be reserved for a scroller.
     */
    immRectf(pos,
             v2d->cur.xmax - (float)offset,
             yminc + ymin_ofs,
             v2d->cur.xmax + EXTRA_SCROLL_PAD,
             ymaxc);

    immUnbindProgram();
  }
}

/* ------------------ */

/* callback for (normal) widget settings - send notifiers */
static void achannel_setting_widget_cb(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
}

/* callback for widget settings that need flushing */
static void achannel_setting_flush_widget_cb(bContext *C, void *ale_npoin, void *setting_wrap)
{
  bAnimListElem *ale_setting = (bAnimListElem *)ale_npoin;
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  int filter;
  int setting = POINTER_AS_INT(setting_wrap);
  short on = 0;

  /* send notifiers before doing anything else... */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  /* verify that we have a channel to operate on. */
  if (!ale_setting) {
    return;
  }

  if (ale_setting->type == ANIMTYPE_GPLAYER) {
    /* draw cache updates for settings that affect the visible strokes */
    if (setting == ACHANNEL_SETTING_VISIBLE) {
      bGPdata *gpd = (bGPdata *)ale_setting->id;
      DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }

    /* UI updates */
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, NULL);
  }

  /* Tag for full animation update, so that the settings will have an effect. */
  if (ale_setting->id) {
    DEG_id_tag_update(ale_setting->id, ID_RECALC_ANIMATION);
  }
  if (ale_setting->adt && ale_setting->adt->action) {
    /* Action is it's own datablock, so has to be tagged specifically. */
    DEG_id_tag_update(&ale_setting->adt->action->id, ID_RECALC_ANIMATION);
  }

  /* verify animation context */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  /* check if the setting is on... */
  on = ANIM_channel_setting_get(&ac, ale_setting, setting);

  /* on == -1 means setting not found... */
  if (on == -1) {
    return;
  }

  /* get all channels that can possibly be chosen - but ignore hierarchy */
  filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS;
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* call API method to flush the setting */
  ANIM_flush_setting_anim_channels(&ac, &anim_data, ale_setting, setting, on);

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);
}

/* callback for wrapping NLA Track "solo" toggle logic */
static void achannel_nlatrack_solo_widget_cb(bContext *C, void *ale_poin, void *UNUSED(arg2))
{
  bAnimListElem *ale = ale_poin;
  AnimData *adt = ale->adt;
  NlaTrack *nlt = ale->data;

  /* Toggle 'solo' mode. There are several complications here which need explaining:
   * - The method call is needed to perform a few additional validation operations
   *   to ensure that the mode is applied properly
   * - BUT, since the button already toggles the value, we need to un-toggle it
   *   before the API call gets to it, otherwise it will end up clearing the result
   *   again!
   */
  nlt->flag ^= NLATRACK_SOLO;
  BKE_nlatrack_solo_toggle(adt, nlt);

  /* send notifiers */
  DEG_id_tag_update(ale->id, ID_RECALC_ANIMATION);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
}

/* callback for widget sliders - insert keyframes */
static void achannel_setting_slider_cb(bContext *C, void *id_poin, void *fcu_poin)
{
  ID *id = (ID *)id_poin;
  AnimData *adt = BKE_animdata_from_id(id);
  FCurve *fcu = (FCurve *)fcu_poin;

  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ListBase nla_cache = {NULL, NULL};
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  eInsertKeyFlags flag = 0;
  bool done = false;
  float cfra;

  /* Get RNA pointer */
  RNA_id_pointer_create(id, &id_ptr);

  /* Get NLA context for value remapping */
  NlaKeyframingContext *nla_context = BKE_animsys_get_nla_keyframing_context(
      &nla_cache, &id_ptr, adt, (float)CFRA, false);

  /* get current frame and apply NLA-mapping to it (if applicable) */
  cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);

  /* Get flags for keyframing. */
  flag = ANIM_get_keyframing_flags(scene, true);

  /* try to resolve the path stored in the F-Curve */
  if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
    /* set the special 'replace' flag if on a keyframe */
    if (fcurve_frame_has_keyframe(fcu, cfra, 0)) {
      flag |= INSERTKEY_REPLACE;
    }

    /* insert a keyframe for this F-Curve */
    done = insert_keyframe_direct(
        reports, ptr, prop, fcu, cfra, ts->keyframe_type, nla_context, flag);

    if (done) {
      if (adt->action != NULL) {
        DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
      }
      DEG_id_tag_update(id, ID_RECALC_ANIMATION_NO_FLUSH);
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
    }
  }

  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
}

/* callback for shapekey widget sliders - insert keyframes */
static void achannel_setting_slider_shapekey_cb(bContext *C, void *key_poin, void *kb_poin)
{
  Main *bmain = CTX_data_main(C);
  Key *key = (Key *)key_poin;
  KeyBlock *kb = (KeyBlock *)kb_poin;
  char *rna_path = BKE_keyblock_curval_rnapath_get(key, kb);

  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ListBase nla_cache = {NULL, NULL};
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  eInsertKeyFlags flag = 0;
  bool done = false;
  float cfra;

  /* Get RNA pointer */
  RNA_id_pointer_create((ID *)key, &id_ptr);

  /* Get NLA context for value remapping */
  NlaKeyframingContext *nla_context = BKE_animsys_get_nla_keyframing_context(
      &nla_cache, &id_ptr, key->adt, (float)CFRA, false);

  /* get current frame and apply NLA-mapping to it (if applicable) */
  cfra = BKE_nla_tweakedit_remap(key->adt, (float)CFRA, NLATIME_CONVERT_UNMAP);

  /* get flags for keyframing */
  flag = ANIM_get_keyframing_flags(scene, true);

  /* try to resolve the path stored in the F-Curve */
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop)) {
    /* find or create new F-Curve */
    // XXX is the group name for this ok?
    bAction *act = ED_id_action_ensure(bmain, (ID *)key);
    FCurve *fcu = ED_action_fcurve_ensure(bmain, act, NULL, &ptr, rna_path, 0);

    /* set the special 'replace' flag if on a keyframe */
    if (fcurve_frame_has_keyframe(fcu, cfra, 0)) {
      flag |= INSERTKEY_REPLACE;
    }

    /* insert a keyframe for this F-Curve */
    done = insert_keyframe_direct(
        reports, ptr, prop, fcu, cfra, ts->keyframe_type, nla_context, flag);

    if (done) {
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
    }
  }

  /* free the path */
  if (rna_path) {
    MEM_freeN(rna_path);
  }

  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
}

/* callback for NLA Control Curve widget sliders - insert keyframes */
static void achannel_setting_slider_nla_curve_cb(bContext *C,
                                                 void *UNUSED(id_poin),
                                                 void *fcu_poin)
{
  /* ID *id = (ID *)id_poin; */
  FCurve *fcu = (FCurve *)fcu_poin;

  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  eInsertKeyFlags flag = 0;
  bool done = false;
  float cfra;

  /* get current frame - *no* NLA mapping should be done */
  cfra = (float)CFRA;

  /* get flags for keyframing */
  flag = ANIM_get_keyframing_flags(scene, true);

  /* Get pointer and property from the slider -
   * this should all match up with the NlaStrip required. */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (fcu && prop) {
    /* set the special 'replace' flag if on a keyframe */
    if (fcurve_frame_has_keyframe(fcu, cfra, 0)) {
      flag |= INSERTKEY_REPLACE;
    }

    /* insert a keyframe for this F-Curve */
    done = insert_keyframe_direct(reports, ptr, prop, fcu, cfra, ts->keyframe_type, NULL, flag);

    if (done) {
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
    }
  }
}

/* Draw a widget for some setting */
static void draw_setting_widget(bAnimContext *ac,
                                bAnimListElem *ale,
                                const bAnimChannelType *acf,
                                uiBlock *block,
                                int xpos,
                                int ypos,
                                int setting)
{
  short ptrsize, butType;
  bool negflag;
  bool usetoggle = true;
  int flag, icon;
  void *ptr;
  const char *tooltip;
  uiBut *but = NULL;
  bool enabled;

  /* get the flag and the pointer to that flag */
  flag = acf->setting_flag(ac, setting, &negflag);
  ptr = acf->setting_ptr(ale, setting, &ptrsize);
  enabled = ANIM_channel_setting_get(ac, ale, setting);

  /* get the base icon for the setting */
  switch (setting) {
    case ACHANNEL_SETTING_VISIBLE: /* visibility eyes */
      // icon = ((enabled) ? ICON_HIDE_OFF : ICON_HIDE_ON);
      icon = ICON_HIDE_ON;

      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        tooltip = TIP_("F-Curve visibility in Graph Editor");
      }
      else if (ale->type == ANIMTYPE_GPLAYER) {
        tooltip = TIP_("Grease Pencil layer is visible in the viewport");
      }
      else {
        tooltip = TIP_("Channels are visible in Graph Editor for editing");
      }
      break;

    case ACHANNEL_SETTING_ALWAYS_VISIBLE:
      icon = ICON_UNPINNED;
      tooltip = TIP_("Channels are visible in Graph Editor for editing");
      break;

    case ACHANNEL_SETTING_MOD_OFF: /* modifiers disabled */
      icon = ICON_MODIFIER_OFF;
      tooltip = TIP_("Enable F-Curve modifiers");
      break;

    case ACHANNEL_SETTING_EXPAND: /* expanded triangle */
      // icon = ((enabled) ? ICON_TRIA_DOWN : ICON_TRIA_RIGHT);
      icon = ICON_TRIA_RIGHT;
      tooltip = TIP_("Make channels grouped under this channel visible");
      break;

    case ACHANNEL_SETTING_SOLO: /* NLA Tracks only */
      // icon = ((enabled) ? ICON_SOLO_OFF : ICON_SOLO_ON);
      icon = ICON_SOLO_OFF;
      tooltip = TIP_(
          "NLA Track is the only one evaluated in this animation data-block, with all others "
          "muted");
      break;

      /* --- */

    case ACHANNEL_SETTING_PROTECT: /* protected lock */
      // TODO: what about when there's no protect needed?
      // icon = ((enabled) ? ICON_LOCKED : ICON_UNLOCKED);
      icon = ICON_UNLOCKED;

      if (ale->datatype != ALE_NLASTRIP) {
        tooltip = TIP_("Editability of keyframes for this channel");
      }
      else {
        tooltip = TIP_("Editability of NLA Strips in this track");
      }
      break;

    case ACHANNEL_SETTING_MUTE: /* muted speaker */
      icon = ((enabled) ? ICON_CHECKBOX_DEHLT : ICON_CHECKBOX_HLT);
      usetoggle = false;

      if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
        tooltip = TIP_("Does F-Curve contribute to result");
      }
      else if ((ac) && (ac->spacetype == SPACE_NLA) && (ale->type != ANIMTYPE_NLATRACK)) {
        tooltip = TIP_(
            "Temporarily disable NLA stack evaluation (i.e. only the active action is evaluated)");
      }
      else if (ale->type == ANIMTYPE_GPLAYER) {
        tooltip = TIP_(
            "All keyframes contribute to the result (uncheck to use the current frame only)");
      }
      else {
        tooltip = TIP_("Do channels contribute to result (toggle channel muting)");
      }
      break;

    case ACHANNEL_SETTING_PINNED: /* pin icon */
      // icon = ((enabled) ? ICON_PINNED : ICON_UNPINNED);
      icon = ICON_UNPINNED;

      if (ale->type == ANIMTYPE_NLAACTION) {
        tooltip = TIP_("Display action without any time remapping (when unpinned)");
      }
      else {
        /* TODO: there are no other tools which require the 'pinning' concept yet */
        tooltip = NULL;
      }
      break;

    default:
      tooltip = NULL;
      icon = 0;
      break;
  }

  /* type of button */
  if (usetoggle) {
    if (negflag) {
      butType = UI_BTYPE_ICON_TOGGLE_N;
    }
    else {
      butType = UI_BTYPE_ICON_TOGGLE;
    }
  }
  else {
    if (negflag) {
      butType = UI_BTYPE_TOGGLE_N;
    }
    else {
      butType = UI_BTYPE_TOGGLE;
    }
  }
  /* draw button for setting */
  if (ptr && flag) {
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
                               ptr,
                               0,
                               0,
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
                               ptr,
                               0,
                               0,
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
                               ptr,
                               0,
                               0,
                               0,
                               0,
                               tooltip);
        break;
    }

    /* set call to send relevant notifiers and/or perform type-specific updates */
    if (but) {
      switch (setting) {
        /* settings needing flushing up/down hierarchy  */
        case ACHANNEL_SETTING_VISIBLE: /* Graph Editor - 'visibility' toggles */
        case ACHANNEL_SETTING_PROTECT: /* General - protection flags */
        case ACHANNEL_SETTING_MUTE:    /* General - muting flags */
        case ACHANNEL_SETTING_PINNED:  /* NLA Actions - 'map/nomap' */
        case ACHANNEL_SETTING_MOD_OFF:
        case ACHANNEL_SETTING_ALWAYS_VISIBLE:
          UI_but_funcN_set(but,
                           achannel_setting_flush_widget_cb,
                           MEM_dupallocN(ale),
                           POINTER_FROM_INT(setting));
          break;

        /* settings needing special attention */
        case ACHANNEL_SETTING_SOLO: /* NLA Tracks - Solo toggle */
          UI_but_funcN_set(but, achannel_nlatrack_solo_widget_cb, MEM_dupallocN(ale), NULL);
          break;

        /* no flushing */
        case ACHANNEL_SETTING_EXPAND: /* expanding - cannot flush,
                                       * otherwise all would open/close at once */
        default:
          UI_but_func_set(but, achannel_setting_widget_cb, NULL, NULL);
          break;
      }

      if ((ale->fcurve_owner_id != NULL && ID_IS_LINKED(ale->fcurve_owner_id)) ||
          (ale->id != NULL && ID_IS_LINKED(ale->id))) {
        if (setting != ACHANNEL_SETTING_EXPAND) {
          UI_but_flag_enable(but, UI_BUT_DISABLED);
        }
      }
    }
  }
}

/* Draw UI widgets the given channel */
void ANIM_channel_draw_widgets(const bContext *C,
                               bAnimContext *ac,
                               bAnimListElem *ale,
                               uiBlock *block,
                               rctf *rect,
                               size_t channel_index)
{
  const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
  View2D *v2d = &ac->region->v2d;
  float ymid;
  const short channel_height = round_fl_to_int(BLI_rctf_size_y(rect));
  const bool is_being_renamed = achannel_is_being_renamed(ac, acf, channel_index);

  /* sanity checks - don't draw anything */
  if (ELEM(NULL, acf, ale, block)) {
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
  UI_block_emboss_set(block, UI_EMBOSS_NONE);

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
   * - in Graph Editor, checkboxes for visibility in curves area
   * - in NLA Editor, glowing dots for solo/not solo...
   * - in Grease Pencil mode, color swatches for layer color
   */
  if (ac->sl) {
    if ((ac->spacetype == SPACE_GRAPH) &&
        (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE) ||
         acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE))) {
      /* pin toggle  */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_ALWAYS_VISIBLE)) {
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_ALWAYS_VISIBLE);
        offset += ICON_WIDTH;
      }
      /* visibility toggle  */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_VISIBLE)) {
        /* For F-curves, add the extra space for the color bands. */
        if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
          offset += GRAPH_ICON_VISIBILITY_OFFSET;
        }
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_VISIBLE);
        offset += ICON_WIDTH;
      }
    }
    else if ((ac->spacetype == SPACE_NLA) && acf->has_setting(ac, ale, ACHANNEL_SETTING_SOLO)) {
      /* 'solo' setting for NLA Tracks */
      draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_SOLO);
      offset += ICON_WIDTH;
    }
  }

  /* step 4) draw text - check if renaming widget is in use... */
  if (is_being_renamed) {
    PointerRNA ptr = {NULL};
    PropertyRNA *prop = NULL;

    /* draw renaming widget if we can get RNA pointer for it
     * NOTE: property may only be available in some cases, even if we have
     *       a callback available (e.g. broken F-Curve rename)
     */
    if (acf->name_prop(ale, &ptr, &prop)) {
      const short margin_x = 3 * round_fl_to_int(UI_DPI_FAC);
      const short width = ac->region->winx - offset - (margin_x * 2);
      uiBut *but;

      UI_block_emboss_set(block, UI_EMBOSS);

      but = uiDefButR(block,
                      UI_BTYPE_TEXT,
                      1,
                      "",
                      offset + margin_x,
                      rect->ymin,
                      MAX2(width, RENAME_TEXT_MIN_WIDTH),
                      channel_height,
                      &ptr,
                      RNA_property_identifier(prop),
                      -1,
                      0,
                      0,
                      -1,
                      -1,
                      NULL);

      /* copy what outliner does here, see outliner_buttons */
      if (UI_but_active_only(C, ac->region, block, but) == false) {
        ac->ads->renameIndex = 0;

        /* send notifiers */
        WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_RENAME, NULL);
      }

      UI_block_emboss_set(block, UI_EMBOSS_NONE);
    }
    else {
      /* Cannot get property/cannot or rename for some reason, so clear rename index
       * so that this doesn't hang around, and the name can be drawn normally - T47492
       */
      ac->ads->renameIndex = 0;
      WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, NULL);
    }
  }

  /* step 5) draw mute+protection toggles + (sliders) ....................... */
  /* reset offset - now goes from RHS of panel */
  offset = (int)rect->xmax;

  // TODO: when drawing sliders, make those draw instead of these toggles if not enough space
  if (v2d && !is_being_renamed) {
    short draw_sliders = 0;

    /* check if we need to show the sliders */
    if ((ac->sl) && ELEM(ac->spacetype, SPACE_ACTION, SPACE_GRAPH)) {
      switch (ac->spacetype) {
        case SPACE_ACTION: {
          SpaceAction *saction = (SpaceAction *)ac->sl;
          draw_sliders = (saction->flag & SACTION_SLIDERS);
          break;
        }
        case SPACE_GRAPH: {
          SpaceGraph *sipo = (SpaceGraph *)ac->sl;
          draw_sliders = (sipo->flag & SIPO_SLIDERS);
          break;
        }
      }
    }

    /* check if there's enough space for the toggles if the sliders are drawn too */
    if (!(draw_sliders) || (BLI_rcti_size_x(&v2d->mask) > ACHANNEL_BUTTON_WIDTH / 2)) {
      /* protect... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_PROTECT)) {
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_PROTECT);
      }
      /* mute... */
      if (acf->has_setting(ac, ale, ACHANNEL_SETTING_MUTE)) {
        offset -= ICON_WIDTH;
        draw_setting_widget(ac, ale, acf, block, offset, ymid, ACHANNEL_SETTING_MUTE);
      }
      if (ale->type == ANIMTYPE_GPLAYER) {
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
          !(ale->adt->flag & ADT_NLA_EDIT_ON)) {
        uiBut *but;
        PointerRNA *opptr_b;

        UI_block_emboss_set(block, UI_EMBOSS);

        offset -= UI_UNIT_X;
        but = uiDefIconButO(block,
                            UI_BTYPE_BUT,
                            "NLA_OT_action_pushdown",
                            WM_OP_INVOKE_DEFAULT,
                            ICON_NLA_PUSHDOWN,
                            offset,
                            ymid,
                            UI_UNIT_X,
                            UI_UNIT_X,
                            NULL);

        opptr_b = UI_but_operator_ptr_get(but);
        RNA_int_set(opptr_b, "channel_index", channel_index);

        UI_block_emboss_set(block, UI_EMBOSS_NONE);
      }
    }

    /* Draw slider:
     * - Even if we can draw sliders for this view, we must also check that the channel-type
     *   supports them (only only F-Curves really can support them for now).
     * - To make things easier, we use RNA-autobuts for this so that changes are
     *   reflected immediately, wherever they occurred.
     *   BUT, we don't use the layout engine, otherwise we'd get wrong alignment,
     *   and wouldn't be able to auto-keyframe.
     * - Slider should start before the toggles (if they're visible)
     *   to keep a clean line down the side.
     */
    if ((draw_sliders) &&
        ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE, ANIMTYPE_SHAPEKEY, ANIMTYPE_GPLAYER)) {
      /* adjust offset */
      /* TODO: make slider width dynamic,
       * so that they can be easier to use when the view is wide enough. */
      offset -= SLIDER_WIDTH;

      /* need backdrop behind sliders... */
      UI_block_emboss_set(block, UI_EMBOSS);

      if (ale->owner) { /* Slider using custom RNA Access ---------- */
        if (ale->type == ANIMTYPE_NLACURVE) {
          NlaStrip *strip = (NlaStrip *)ale->owner;
          FCurve *fcu = (FCurve *)ale->data;
          PointerRNA ptr;
          PropertyRNA *prop;

          /* create RNA pointers */
          RNA_pointer_create(ale->id, &RNA_NlaStrip, strip, &ptr);
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
                                ymid,
                                SLIDER_WIDTH,
                                channel_height);
            UI_but_func_set(but, achannel_setting_slider_nla_curve_cb, ale->id, ale->data);
          }
        }
      }
      else if (ale->id) { /* Slider using RNA Access --------------- */
        PointerRNA id_ptr, ptr;
        PropertyRNA *prop;
        char *rna_path = NULL;
        int array_index = 0;
        short free_path = 0;

        /* get destination info */
        if (ale->type == ANIMTYPE_FCURVE) {
          FCurve *fcu = (FCurve *)ale->data;

          rna_path = fcu->rna_path;
          array_index = fcu->array_index;
        }
        else if (ale->type == ANIMTYPE_SHAPEKEY) {
          KeyBlock *kb = (KeyBlock *)ale->data;
          Key *key = (Key *)ale->id;

          rna_path = BKE_keyblock_curval_rnapath_get(key, kb);
          free_path = 1;
        }
        /* Special for Grease Pencil Layer. */
        else if (ale->type == ANIMTYPE_GPLAYER) {
          bGPdata *gpd = (bGPdata *)ale->id;
          if ((gpd != NULL) && ((gpd->flag & GP_DATA_ANNOTATIONS) == 0)) {
            /* Reset slider offset, in order to add special gp icons. */
            offset += SLIDER_WIDTH;

            char *gp_rna_path = NULL;
            bGPDlayer *gpl = (bGPDlayer *)ale->data;

            /* Create the RNA pointers. */
            RNA_pointer_create(ale->id, &RNA_GPencilLayer, ale->data, &ptr);
            RNA_id_pointer_create(ale->id, &id_ptr);
            int icon;

            /* Layer onion skinning switch. */
            offset -= ICON_WIDTH;
            UI_block_emboss_set(block, UI_EMBOSS_NONE);
            prop = RNA_struct_find_property(&ptr, "use_onion_skinning");
            gp_rna_path = RNA_path_from_ID_to_property(&ptr, prop);
            if (RNA_path_resolve_property(&id_ptr, gp_rna_path, &ptr, &prop)) {
              icon = (gpl->onion_flag & GP_LAYER_ONIONSKIN) ? ICON_ONIONSKIN_ON :
                                                              ICON_ONIONSKIN_OFF;
              uiDefAutoButR(block,
                            &ptr,
                            prop,
                            array_index,
                            "",
                            icon,
                            offset,
                            ymid,
                            ICON_WIDTH,
                            channel_height);
            }
            MEM_freeN(gp_rna_path);

            /* Mask Layer. */
            offset -= ICON_WIDTH;
            UI_block_emboss_set(block, UI_EMBOSS_NONE);
            prop = RNA_struct_find_property(&ptr, "use_mask_layer");
            gp_rna_path = RNA_path_from_ID_to_property(&ptr, prop);
            if (RNA_path_resolve_property(&id_ptr, gp_rna_path, &ptr, &prop)) {
              icon = ICON_LAYER_ACTIVE;
              if (gpl->flag & GP_LAYER_USE_MASK) {
                icon = ICON_MOD_MASK;
              }
              else {
                icon = ICON_LAYER_ACTIVE;
              }
              uiDefAutoButR(block,
                            &ptr,
                            prop,
                            array_index,
                            "",
                            icon,
                            offset,
                            ymid,
                            ICON_WIDTH,
                            channel_height);
            }
            MEM_freeN(gp_rna_path);

            /* Layer opacity. */
            const short width = SLIDER_WIDTH * 0.6;
            offset -= width;
            UI_block_emboss_set(block, UI_EMBOSS);
            prop = RNA_struct_find_property(&ptr, "opacity");
            gp_rna_path = RNA_path_from_ID_to_property(&ptr, prop);
            if (RNA_path_resolve_property(&id_ptr, gp_rna_path, &ptr, &prop)) {
              uiDefAutoButR(block,
                            &ptr,
                            prop,
                            array_index,
                            "",
                            ICON_NONE,
                            offset,
                            ymid,
                            width,
                            channel_height);
            }
            MEM_freeN(gp_rna_path);
          }
        }

        /* Only if RNA-Path found. */
        if (rna_path) {
          /* get RNA pointer, and resolve the path */
          RNA_id_pointer_create(ale->id, &id_ptr);

          /* try to resolve the path */
          if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop)) {
            uiBut *but;

            /* Create the slider button,
             * and assign relevant callback to ensure keyframes are inserted. */
            but = uiDefAutoButR(block,
                                &ptr,
                                prop,
                                array_index,
                                "",
                                ICON_NONE,
                                offset,
                                ymid,
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

          /* free the path if necessary */
          if (free_path) {
            MEM_freeN(rna_path);
          }
        }
      }
      else { /* Special Slider for stuff without RNA Access ---------- */
        // TODO: only implement this case when we really need it...
      }
    }
  }
}

/* *********************************************** */
