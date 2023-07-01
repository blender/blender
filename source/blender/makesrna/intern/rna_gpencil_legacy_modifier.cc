/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_math_rotation.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_object_greasepencil_modifier_type_items[] = {
    RNA_ENUM_ITEM_HEADING(N_("Modify"), nullptr),
    {eGpencilModifierType_Texture,
     "GP_TEXTURE",
     ICON_MOD_UVPROJECT,
     "Texture Mapping",
     "Change stroke UV texture values"},
    {eGpencilModifierType_Time, "GP_TIME", ICON_MOD_TIME, "Time Offset", "Offset keyframes"},
    {eGpencilModifierType_WeightAngle,
     "GP_WEIGHT_ANGLE",
     ICON_MOD_VERTEX_WEIGHT,
     "Vertex Weight Angle",
     "Generate Vertex Weights base on stroke angle"},
    {eGpencilModifierType_WeightProximity,
     "GP_WEIGHT_PROXIMITY",
     ICON_MOD_VERTEX_WEIGHT,
     "Vertex Weight Proximity",
     "Generate Vertex Weights base on distance to object"},

    RNA_ENUM_ITEM_HEADING(N_("Generate"), nullptr),
    {eGpencilModifierType_Array,
     "GP_ARRAY",
     ICON_MOD_ARRAY,
     "Array",
     "Create array of duplicate instances"},
    {eGpencilModifierType_Build,
     "GP_BUILD",
     ICON_MOD_BUILD,
     "Build",
     "Create duplication of strokes"},
    {eGpencilModifierType_Dash,
     "GP_DASH",
     ICON_MOD_DASH,
     "Dot Dash",
     "Generate dot-dash styled strokes"},
    {eGpencilModifierType_Envelope,
     "GP_ENVELOPE",
     ICON_MOD_ENVELOPE,
     "Envelope",
     "Create an envelope shape"},
    {eGpencilModifierType_Length,
     "GP_LENGTH",
     ICON_MOD_LENGTH,
     "Length",
     "Extend or shrink strokes"},
    {eGpencilModifierType_Lineart,
     "GP_LINEART",
     ICON_MOD_LINEART,
     "Line Art",
     "Generate line art strokes from selected source"},
    {eGpencilModifierType_Mirror,
     "GP_MIRROR",
     ICON_MOD_MIRROR,
     "Mirror",
     "Duplicate strokes like a mirror"},
    {eGpencilModifierType_Multiply,
     "GP_MULTIPLY",
     ICON_GP_MULTIFRAME_EDITING,
     "Multiple Strokes",
     "Produce multiple strokes along one stroke"},
    {eGpencilModifierType_Outline,
     "GP_OUTLINE",
     ICON_MOD_OUTLINE,
     "Outline",
     "Convert stroke to perimeter"},
    {eGpencilModifierType_Simplify,
     "GP_SIMPLIFY",
     ICON_MOD_SIMPLIFY,
     "Simplify",
     "Simplify stroke reducing number of points"},
    {eGpencilModifierType_Subdiv,
     "GP_SUBDIV",
     ICON_MOD_SUBSURF,
     "Subdivide",
     "Subdivide stroke adding more control points"},
    RNA_ENUM_ITEM_HEADING(N_("Deform"), nullptr),
    {eGpencilModifierType_Armature,
     "GP_ARMATURE",
     ICON_MOD_ARMATURE,
     "Armature",
     "Deform stroke points using armature object"},
    {eGpencilModifierType_Hook,
     "GP_HOOK",
     ICON_HOOK,
     "Hook",
     "Deform stroke points using objects"},
    {eGpencilModifierType_Lattice,
     "GP_LATTICE",
     ICON_MOD_LATTICE,
     "Lattice",
     "Deform strokes using lattice"},
    {eGpencilModifierType_Noise, "GP_NOISE", ICON_MOD_NOISE, "Noise", "Add noise to strokes"},
    {eGpencilModifierType_Offset,
     "GP_OFFSET",
     ICON_MOD_OFFSET,
     "Offset",
     "Change stroke location, rotation or scale"},
    {eGpencilModifierType_Shrinkwrap,
     "SHRINKWRAP",
     ICON_MOD_SHRINKWRAP,
     "Shrinkwrap",
     "Project the shape onto another object"},
    {eGpencilModifierType_Smooth, "GP_SMOOTH", ICON_MOD_SMOOTH, "Smooth", "Smooth stroke"},
    {eGpencilModifierType_Thick,
     "GP_THICK",
     ICON_MOD_THICKNESS,
     "Thickness",
     "Change stroke thickness"},
    RNA_ENUM_ITEM_HEADING(N_("Color"), nullptr),
    {eGpencilModifierType_Color,
     "GP_COLOR",
     ICON_MOD_HUE_SATURATION,
     "Hue/Saturation",
     "Apply changes to stroke colors"},
    {eGpencilModifierType_Opacity,
     "GP_OPACITY",
     ICON_MOD_OPACITY,
     "Opacity",
     "Opacity of the strokes"},
    {eGpencilModifierType_Tint, "GP_TINT", ICON_MOD_TINT, "Tint", "Tint strokes with new color"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem gpencil_build_time_mode_items[] = {
    {GP_BUILD_TIMEMODE_DRAWSPEED,
     "DRAWSPEED",
     0,
     "Natural Drawing Speed",
     "Use recorded speed multiplied by a factor"},
    {GP_BUILD_TIMEMODE_FRAMES,
     "FRAMES",
     0,
     "Number of Frames",
     "Set a fixed number of frames for all build animations"},
    {GP_BUILD_TIMEMODE_PERCENTAGE,
     "PERCENTAGE",
     0,
     "Percentage Factor",
     "Set a manual percentage to build"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem modifier_modify_color_items[] = {
    {GP_MODIFY_COLOR_BOTH, "BOTH", 0, "Stroke & Fill", "Modify fill and stroke colors"},
    {GP_MODIFY_COLOR_STROKE, "STROKE", 0, "Stroke", "Modify stroke color only"},
    {GP_MODIFY_COLOR_FILL, "FILL", 0, "Fill", "Modify fill color only"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem modifier_modify_opacity_items[] = {
    {GP_MODIFY_COLOR_BOTH, "BOTH", 0, "Stroke & Fill", "Modify fill and stroke colors"},
    {GP_MODIFY_COLOR_STROKE, "STROKE", 0, "Stroke", "Modify stroke color only"},
    {GP_MODIFY_COLOR_FILL, "FILL", 0, "Fill", "Modify fill color only"},
    {GP_MODIFY_COLOR_HARDNESS, "HARDNESS", 0, "Hardness", "Modify stroke hardness"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem modifier_gphook_falloff_items[] = {
    {eGPHook_Falloff_None, "NONE", 0, "No Falloff", ""},
    {eGPHook_Falloff_Curve, "CURVE", 0, "Curve", ""},
    {eGPHook_Falloff_Smooth, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", ""},
    {eGPHook_Falloff_Sphere, "SPHERE", ICON_SPHERECURVE, "Sphere", ""},
    {eGPHook_Falloff_Root, "ROOT", ICON_ROOTCURVE, "Root", ""},
    {eGPHook_Falloff_InvSquare, "INVERSE_SQUARE", ICON_ROOTCURVE, "Inverse Square", ""},
    {eGPHook_Falloff_Sharp, "SHARP", ICON_SHARPCURVE, "Sharp", ""},
    {eGPHook_Falloff_Linear, "LINEAR", ICON_LINCURVE, "Linear", ""},
    {eGPHook_Falloff_Const, "CONSTANT", ICON_NOCURVE, "Constant", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_time_mode_items[] = {
    {GP_TIME_MODE_NORMAL, "NORMAL", 0, "Regular", "Apply offset in usual animation direction"},
    {GP_TIME_MODE_REVERSE, "REVERSE", 0, "Reverse", "Apply offset in reverse animation direction"},
    {GP_TIME_MODE_FIX, "FIX", 0, "Fixed Frame", "Keep frame and do not change with time"},
    {GP_TIME_MODE_PINGPONG, "PINGPONG", 0, "Ping Pong", "Loop back and forth starting in reverse"},
    {GP_TIME_MODE_CHAIN, "CHAIN", 0, "Chain", "List of chained animation segments"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_time_seg_mode_items[] = {
    {GP_TIME_SEG_MODE_NORMAL, "NORMAL", 0, "Regular", "Apply offset in usual animation direction"},
    {GP_TIME_SEG_MODE_REVERSE,
     "REVERSE",
     0,
     "Reverse",
     "Apply offset in reverse animation direction"},
    {GP_TIME_SEG_MODE_PINGPONG, "PINGPONG", 0, "Ping Pong", "Loop back and forth"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem gpencil_subdivision_type_items[] = {
    {GP_SUBDIV_CATMULL, "CATMULL_CLARK", 0, "Catmull-Clark", ""},
    {GP_SUBDIV_SIMPLE, "SIMPLE", 0, "Simple", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem gpencil_tint_type_items[] = {
    {GP_TINT_UNIFORM, "UNIFORM", 0, "Uniform", ""},
    {GP_TINT_GRADIENT, "GRADIENT", 0, "Gradient", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem gpencil_length_mode_items[] = {
    {GP_LENGTH_RELATIVE, "RELATIVE", 0, "Relative", "Length in ratio to the stroke's length"},
    {GP_LENGTH_ABSOLUTE, "ABSOLUTE", 0, "Absolute", "Length in geometry space"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem gpencil_envelope_mode_items[] = {
    {GP_ENVELOPE_DEFORM,
     "DEFORM",
     0,
     "Deform",
     "Deform the stroke to best match the envelope shape"},
    {GP_ENVELOPE_SEGMENTS,
     "SEGMENTS",
     0,
     "Segments",
     "Add segments to create the envelope. Keep the original stroke"},
    {GP_ENVELOPE_FILLS,
     "FILLS",
     0,
     "Fills",
     "Add fill segments to create the envelope. Don't keep the original stroke"},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem modifier_noise_random_mode_items[] = {
    {GP_NOISE_RANDOM_STEP, "STEP", 0, "Steps", "Randomize every number of frames"},
    {GP_NOISE_RANDOM_KEYFRAME, "KEYFRAME", 0, "Keyframes", "Randomize on keyframes only"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

#ifdef RNA_RUNTIME

#  include "DNA_curve_types.h"
#  include "DNA_fluid_types.h"
#  include "DNA_material_types.h"
#  include "DNA_particle_types.h"

#  include "BKE_cachefile.h"
#  include "BKE_context.h"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_gpencil_modifier_legacy.h"
#  include "BKE_object.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

static StructRNA *rna_GpencilModifier_refine(PointerRNA *ptr)
{
  GpencilModifierData *md = (GpencilModifierData *)ptr->data;

  switch ((GpencilModifierType)md->type) {
    case eGpencilModifierType_Noise:
      return &RNA_NoiseGpencilModifier;
    case eGpencilModifierType_Subdiv:
      return &RNA_SubdivGpencilModifier;
    case eGpencilModifierType_Simplify:
      return &RNA_SimplifyGpencilModifier;
    case eGpencilModifierType_Thick:
      return &RNA_ThickGpencilModifier;
    case eGpencilModifierType_Tint:
      return &RNA_TintGpencilModifier;
    case eGpencilModifierType_Time:
      return &RNA_TimeGpencilModifier;
    case eGpencilModifierType_WeightProximity:
      return &RNA_WeightProxGpencilModifier;
    case eGpencilModifierType_WeightAngle:
      return &RNA_WeightAngleGpencilModifier;
    case eGpencilModifierType_Color:
      return &RNA_ColorGpencilModifier;
    case eGpencilModifierType_Array:
      return &RNA_ArrayGpencilModifier;
    case eGpencilModifierType_Build:
      return &RNA_BuildGpencilModifier;
    case eGpencilModifierType_Opacity:
      return &RNA_OpacityGpencilModifier;
    case eGpencilModifierType_Outline:
      return &RNA_OutlineGpencilModifier;
    case eGpencilModifierType_Lattice:
      return &RNA_LatticeGpencilModifier;
    case eGpencilModifierType_Length:
      return &RNA_LengthGpencilModifier;
    case eGpencilModifierType_Mirror:
      return &RNA_MirrorGpencilModifier;
    case eGpencilModifierType_Shrinkwrap:
      return &RNA_ShrinkwrapGpencilModifier;
    case eGpencilModifierType_Smooth:
      return &RNA_SmoothGpencilModifier;
    case eGpencilModifierType_Hook:
      return &RNA_HookGpencilModifier;
    case eGpencilModifierType_Offset:
      return &RNA_OffsetGpencilModifier;
    case eGpencilModifierType_Armature:
      return &RNA_ArmatureGpencilModifier;
    case eGpencilModifierType_Multiply:
      return &RNA_MultiplyGpencilModifier;
    case eGpencilModifierType_Texture:
      return &RNA_TextureGpencilModifier;
    case eGpencilModifierType_Lineart:
      return &RNA_LineartGpencilModifier;
    case eGpencilModifierType_Dash:
      return &RNA_DashGpencilModifierData;
    case eGpencilModifierType_Envelope:
      return &RNA_EnvelopeGpencilModifier;
      /* Default */
    case eGpencilModifierType_None:
    case NUM_GREASEPENCIL_MODIFIER_TYPES:
      return &RNA_GpencilModifier;
  }

  return &RNA_GpencilModifier;
}

static void rna_GpencilModifier_name_set(PointerRNA *ptr, const char *value)
{
  GpencilModifierData *gmd = static_cast<GpencilModifierData *>(ptr->data);
  char oldname[sizeof(gmd->name)];

  /* Make a copy of the old name first. */
  STRNCPY(oldname, gmd->name);

  /* Copy the new name into the name slot. */
  STRNCPY_UTF8(gmd->name, value);

  /* Make sure the name is truly unique. */
  if (ptr->owner_id) {
    Object *ob = (Object *)ptr->owner_id;
    BKE_gpencil_modifier_unique_name(&ob->greasepencil_modifiers, gmd);
  }

  /* Fix all the animation data which may link to this. */
  BKE_animdata_fix_paths_rename_all(nullptr, "grease_pencil_modifiers", oldname, gmd->name);
}

static char *rna_GpencilModifier_path(const PointerRNA *ptr)
{
  const GpencilModifierData *gmd = static_cast<GpencilModifierData *>(ptr->data);
  char name_esc[sizeof(gmd->name) * 2];

  BLI_str_escape(name_esc, gmd->name, sizeof(name_esc));
  return BLI_sprintfN("grease_pencil_modifiers[\"%s\"]", name_esc);
}

static void rna_GpencilModifier_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ptr->owner_id);
}

static void rna_GpencilModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_GpencilModifier_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

/* Vertex Groups */

#  define RNA_GP_MOD_VGROUP_NAME_SET(_type, _prop) \
    static void rna_##_type##GpencilModifier_##_prop##_set(PointerRNA *ptr, const char *value) \
    { \
      _type##GpencilModifierData *tmd = (_type##GpencilModifierData *)ptr->data; \
      rna_object_vgroup_name_set(ptr, value, tmd->_prop, sizeof(tmd->_prop)); \
    }

RNA_GP_MOD_VGROUP_NAME_SET(Noise, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Thick, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Opacity, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Lattice, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Smooth, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Hook, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Offset, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Armature, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Texture, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Tint, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(WeightProx, target_vgname);
RNA_GP_MOD_VGROUP_NAME_SET(WeightProx, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(WeightAngle, target_vgname);
RNA_GP_MOD_VGROUP_NAME_SET(WeightAngle, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Lineart, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Shrinkwrap, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Envelope, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Build, target_vgname);

#  undef RNA_GP_MOD_VGROUP_NAME_SET

/* Objects */

static void greasepencil_modifier_object_set(Object *self,
                                             Object **ob_p,
                                             int type,
                                             PointerRNA value)
{
  Object *ob = static_cast<Object *>(value.data);

  if (!self || ob != self) {
    if (!ob || type == OB_EMPTY || ob->type == type) {
      id_lib_extern((ID *)ob);
      *ob_p = ob;
    }
  }
}

#  define RNA_GP_MOD_OBJECT_SET(_type, _prop, _obtype) \
    static void rna_##_type##GpencilModifier_##_prop##_set( \
        PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/) \
    { \
      _type##GpencilModifierData *tmd = (_type##GpencilModifierData *)ptr->data; \
      greasepencil_modifier_object_set((Object *)ptr->owner_id, &tmd->_prop, _obtype, value); \
    }

RNA_GP_MOD_OBJECT_SET(Armature, object, OB_ARMATURE);
RNA_GP_MOD_OBJECT_SET(Lattice, object, OB_LATTICE);
RNA_GP_MOD_OBJECT_SET(Mirror, object, OB_EMPTY);
RNA_GP_MOD_OBJECT_SET(WeightProx, object, OB_EMPTY);
RNA_GP_MOD_OBJECT_SET(Shrinkwrap, target, OB_MESH);
RNA_GP_MOD_OBJECT_SET(Shrinkwrap, aux_target, OB_MESH);
RNA_GP_MOD_OBJECT_SET(Build, object, OB_EMPTY);

#  undef RNA_GP_MOD_OBJECT_SET

static void rna_HookGpencilModifier_object_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               ReportList * /*reports*/)
{
  HookGpencilModifierData *hmd = static_cast<HookGpencilModifierData *>(ptr->data);
  Object *ob = (Object *)value.data;

  hmd->object = ob;
  id_lib_extern((ID *)ob);
  BKE_object_modifier_gpencil_hook_reset(ob, hmd);
}

static void rna_TintGpencilModifier_object_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               ReportList * /*reports*/)
{
  TintGpencilModifierData *hmd = static_cast<TintGpencilModifierData *>(ptr->data);
  Object *ob = (Object *)value.data;

  hmd->object = ob;
  id_lib_extern((ID *)ob);
}

static void rna_TimeModifier_start_frame_set(PointerRNA *ptr, int value)
{
  TimeGpencilModifierData *tmd = static_cast<TimeGpencilModifierData *>(ptr->data);
  CLAMP(value, MINFRAME, MAXFRAME);
  tmd->sfra = value;

  if (tmd->sfra >= tmd->efra) {
    tmd->efra = MIN2(tmd->sfra, MAXFRAME);
  }
}

static void rna_TimeModifier_end_frame_set(PointerRNA *ptr, int value)
{
  TimeGpencilModifierData *tmd = static_cast<TimeGpencilModifierData *>(ptr->data);
  CLAMP(value, MINFRAME, MAXFRAME);
  tmd->efra = value;

  if (tmd->sfra >= tmd->efra) {
    tmd->sfra = MAX2(tmd->efra, MINFRAME);
  }
}

static void rna_GpencilOpacity_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  OpacityGpencilModifierData *md = (OpacityGpencilModifierData *)ptr->data;

  *min = 0.0f;
  *softmin = 0.0f;

  *softmax = (md->flag & GP_OPACITY_NORMALIZE) ? 1.0f : 2.0f;
  *max = *softmax;
}

static void rna_GpencilOpacity_max_set(PointerRNA *ptr, float value)
{
  OpacityGpencilModifierData *md = (OpacityGpencilModifierData *)ptr->data;

  md->factor = value;
  if (md->flag & GP_OPACITY_NORMALIZE) {
    if (md->factor > 1.0f) {
      md->factor = 1.0f;
    }
  }
}

static void rna_GpencilModifier_opacity_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  OpacityGpencilModifierData *md = (OpacityGpencilModifierData *)ptr->data;
  if (md->flag & GP_OPACITY_NORMALIZE) {
    if (md->factor > 1.0f) {
      md->factor = 1.0f;
    }
  }

  rna_GpencilModifier_update(bmain, scene, ptr);
}

bool rna_GpencilModifier_material_poll(PointerRNA *ptr, PointerRNA value)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma = (Material *)value.owner_id;

  return BKE_gpencil_object_material_index_get(ob, ma) != -1;
}

static void rna_GpencilModifier_material_set(PointerRNA *ptr,
                                             PointerRNA value,
                                             Material **ma_target,
                                             ReportList *reports)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma = (Material *)value.owner_id;

  if (ma == nullptr || BKE_gpencil_object_material_index_get(ob, ma) != -1) {
    id_lib_extern((ID *)ob);
    *ma_target = ma;
  }
  else {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Cannot assign material '%s', it has to be used by the grease pencil object already",
        ma->id.name);
  }
}

static void rna_LineartGpencilModifier_material_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList *reports)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)ptr->data;
  Material **ma_target = &lmd->target_material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_NoiseGpencilModifier_material_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList *reports)
{
  NoiseGpencilModifierData *nmd = (NoiseGpencilModifierData *)ptr->data;
  Material **ma_target = &nmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_SmoothGpencilModifier_material_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   ReportList *reports)
{
  SmoothGpencilModifierData *smd = (SmoothGpencilModifierData *)ptr->data;
  Material **ma_target = &smd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_SubdivGpencilModifier_material_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   ReportList *reports)
{
  SubdivGpencilModifierData *smd = (SubdivGpencilModifierData *)ptr->data;
  Material **ma_target = &smd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_SimplifyGpencilModifier_material_set(PointerRNA *ptr,
                                                     PointerRNA value,
                                                     ReportList *reports)
{
  SimplifyGpencilModifierData *smd = (SimplifyGpencilModifierData *)ptr->data;
  Material **ma_target = &smd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_ThickGpencilModifier_material_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList *reports)
{
  ThickGpencilModifierData *tmd = (ThickGpencilModifierData *)ptr->data;
  Material **ma_target = &tmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_WeightProxGpencilModifier_material_set(PointerRNA *ptr,
                                                       PointerRNA value,
                                                       ReportList *reports)
{
  WeightProxGpencilModifierData *tmd = (WeightProxGpencilModifierData *)ptr->data;
  Material **ma_target = &tmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_WeightAngleGpencilModifier_material_set(PointerRNA *ptr,
                                                        PointerRNA value,
                                                        ReportList *reports)
{
  WeightAngleGpencilModifierData *tmd = (WeightAngleGpencilModifierData *)ptr->data;
  Material **ma_target = &tmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_OffsetGpencilModifier_material_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   ReportList *reports)
{
  OffsetGpencilModifierData *omd = (OffsetGpencilModifierData *)ptr->data;
  Material **ma_target = &omd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_TintGpencilModifier_material_set(PointerRNA *ptr,
                                                 PointerRNA value,
                                                 ReportList *reports)
{
  TintGpencilModifierData *tmd = (TintGpencilModifierData *)ptr->data;
  Material **ma_target = &tmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_ColorGpencilModifier_material_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList *reports)
{
  ColorGpencilModifierData *cmd = (ColorGpencilModifierData *)ptr->data;
  Material **ma_target = &cmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_ArrayGpencilModifier_material_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList *reports)
{
  ArrayGpencilModifierData *amd = (ArrayGpencilModifierData *)ptr->data;
  Material **ma_target = &amd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_OpacityGpencilModifier_material_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList *reports)
{
  OpacityGpencilModifierData *omd = (OpacityGpencilModifierData *)ptr->data;
  Material **ma_target = &omd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_OutlineGpencilModifier_object_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList * /*reports*/)
{
  OutlineGpencilModifierData *omd = static_cast<OutlineGpencilModifierData *>(ptr->data);
  Object *ob = (Object *)value.data;

  omd->object = ob;
  id_lib_extern((ID *)ob);
}

static void rna_OutlineGpencilModifier_material_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList *reports)
{
  OutlineGpencilModifierData *omd = (OutlineGpencilModifierData *)ptr->data;
  Material **ma_target = &omd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_OutlineStrokeGpencilModifier_material_set(PointerRNA *ptr,
                                                          PointerRNA value,
                                                          ReportList *reports)
{
  OutlineGpencilModifierData *omd = (OutlineGpencilModifierData *)ptr->data;
  Material **ma_target = &omd->outline_material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_LatticeGpencilModifier_material_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList *reports)
{
  LatticeGpencilModifierData *lmd = (LatticeGpencilModifierData *)ptr->data;
  Material **ma_target = &lmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_MirrorGpencilModifier_material_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   ReportList *reports)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)ptr->data;
  Material **ma_target = &mmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_HookGpencilModifier_material_set(PointerRNA *ptr,
                                                 PointerRNA value,
                                                 ReportList *reports)
{
  HookGpencilModifierData *hmd = (HookGpencilModifierData *)ptr->data;
  Material **ma_target = &hmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_MultiplyGpencilModifier_material_set(PointerRNA *ptr,
                                                     PointerRNA value,
                                                     ReportList *reports)
{
  MultiplyGpencilModifierData *mmd = (MultiplyGpencilModifierData *)ptr->data;
  Material **ma_target = &mmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_TextureGpencilModifier_material_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList *reports)
{
  TextureGpencilModifierData *tmd = (TextureGpencilModifierData *)ptr->data;
  Material **ma_target = &tmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_ShrinkwrapGpencilModifier_material_set(PointerRNA *ptr,
                                                       PointerRNA value,
                                                       ReportList *reports)
{
  ShrinkwrapGpencilModifierData *tmd = (ShrinkwrapGpencilModifierData *)ptr->data;
  Material **ma_target = &tmd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

static void rna_Lineart_start_level_set(PointerRNA *ptr, int value)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)ptr->data;

  CLAMP(value, 0, 128);
  lmd->level_start = value;
  lmd->level_end = MAX2(value, lmd->level_end);
}

static void rna_Lineart_end_level_set(PointerRNA *ptr, int value)
{
  LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)ptr->data;

  CLAMP(value, 0, 128);
  lmd->level_end = value;
  lmd->level_start = MIN2(value, lmd->level_start);
}

static void rna_GpencilDash_segments_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  DashGpencilModifierData *dmd = (DashGpencilModifierData *)ptr->data;
  rna_iterator_array_begin(
      iter, dmd->segments, sizeof(DashGpencilModifierSegment), dmd->segments_len, false, nullptr);
}

static void rna_GpencilTime_segments_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)ptr->data;
  rna_iterator_array_begin(iter,
                           gpmd->segments,
                           sizeof(TimeGpencilModifierSegment),
                           gpmd->segments_len,
                           false,
                           nullptr);
}

static char *rna_TimeGpencilModifierSegment_path(const PointerRNA *ptr)
{
  TimeGpencilModifierSegment *ds = (TimeGpencilModifierSegment *)ptr->data;

  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)ds->gpmd;

  BLI_assert(gpmd != nullptr);

  char name_esc[sizeof(gpmd->modifier.name) * 2];
  BLI_str_escape(name_esc, gpmd->modifier.name, sizeof(name_esc));

  char ds_name_esc[sizeof(ds->name) * 2];
  BLI_str_escape(ds_name_esc, ds->name, sizeof(ds_name_esc));

  return BLI_sprintfN("grease_pencil_modifiers[\"%s\"].segments[\"%s\"]", name_esc, ds_name_esc);
}

static char *rna_DashGpencilModifierSegment_path(const PointerRNA *ptr)

{
  const DashGpencilModifierSegment *ds = (DashGpencilModifierSegment *)ptr->data;

  const DashGpencilModifierData *dmd = (DashGpencilModifierData *)ds->dmd;

  BLI_assert(dmd != nullptr);

  char name_esc[sizeof(dmd->modifier.name) * 2];
  BLI_str_escape(name_esc, dmd->modifier.name, sizeof(name_esc));

  char ds_name_esc[sizeof(ds->name) * 2];
  BLI_str_escape(ds_name_esc, ds->name, sizeof(ds_name_esc));

  return BLI_sprintfN("grease_pencil_modifiers[\"%s\"].segments[\"%s\"]", name_esc, ds_name_esc);
}

static bool dash_segment_name_exists_fn(void *arg, const char *name)
{
  const DashGpencilModifierData *dmd = (const DashGpencilModifierData *)arg;
  for (int i = 0; i < dmd->segments_len; i++) {
    if (STREQ(dmd->segments[i].name, name) && dmd->segments[i].name != name) {
      return true;
    }
  }
  return false;
}

static bool time_segment_name_exists_fn(void *arg, const char *name)
{
  const TimeGpencilModifierData *gpmd = (const TimeGpencilModifierData *)arg;
  for (int i = 0; i < gpmd->segments_len; i++) {
    if (STREQ(gpmd->segments[i].name, name) && gpmd->segments[i].name != name) {
      return true;
    }
  }
  return false;
}

static void rna_DashGpencilModifierSegment_name_set(PointerRNA *ptr, const char *value)
{
  DashGpencilModifierSegment *ds = static_cast<DashGpencilModifierSegment *>(ptr->data);

  char oldname[sizeof(ds->name)];
  STRNCPY(oldname, ds->name);

  STRNCPY_UTF8(ds->name, value);

  BLI_assert(ds->dmd != nullptr);
  BLI_uniquename_cb(
      dash_segment_name_exists_fn, ds->dmd, "Segment", '.', ds->name, sizeof(ds->name));

  char name_esc[sizeof(ds->dmd->modifier.name) * 2];
  BLI_str_escape(name_esc, ds->dmd->modifier.name, sizeof(name_esc));

  char rna_path_prefix[36 + sizeof(name_esc) + 1];
  SNPRINTF(rna_path_prefix, "grease_pencil_modifiers[\"%s\"].segments", name_esc);

  /* Fix all the animation data which may link to this. */
  BKE_animdata_fix_paths_rename_all(nullptr, rna_path_prefix, oldname, ds->name);
}

static void rna_TimeGpencilModifierSegment_name_set(PointerRNA *ptr, const char *value)
{
  TimeGpencilModifierSegment *ds = static_cast<TimeGpencilModifierSegment *>(ptr->data);

  char oldname[sizeof(ds->name)];
  STRNCPY(oldname, ds->name);

  STRNCPY_UTF8(ds->name, value);

  BLI_assert(ds->gpmd != nullptr);
  BLI_uniquename_cb(
      time_segment_name_exists_fn, ds->gpmd, "Segment", '.', ds->name, sizeof(ds->name));

  char name_esc[sizeof(ds->gpmd->modifier.name) * 2];
  BLI_str_escape(name_esc, ds->gpmd->modifier.name, sizeof(name_esc));

  char rna_path_prefix[36 + sizeof(name_esc) + 1];
  SNPRINTF(rna_path_prefix, "grease_pencil_modifiers[\"%s\"].segments", name_esc);

  /* Fix all the animation data which may link to this. */
  BKE_animdata_fix_paths_rename_all(nullptr, rna_path_prefix, oldname, ds->name);
}

static int rna_ShrinkwrapGpencilModifier_face_cull_get(PointerRNA *ptr)
{
  ShrinkwrapGpencilModifierData *swm = (ShrinkwrapGpencilModifierData *)ptr->data;
  return swm->shrink_opts & MOD_SHRINKWRAP_CULL_TARGET_MASK;
}

static void rna_ShrinkwrapGpencilModifier_face_cull_set(PointerRNA *ptr, int value)
{
  ShrinkwrapGpencilModifierData *swm = (ShrinkwrapGpencilModifierData *)ptr->data;
  swm->shrink_opts = (swm->shrink_opts & ~MOD_SHRINKWRAP_CULL_TARGET_MASK) | value;
}

static void rna_EnvelopeGpencilModifier_material_set(PointerRNA *ptr,
                                                     PointerRNA value,
                                                     ReportList *reports)
{
  EnvelopeGpencilModifierData *emd = (EnvelopeGpencilModifierData *)ptr->data;
  Material **ma_target = &emd->material;

  rna_GpencilModifier_material_set(ptr, value, ma_target, reports);
}

const EnumPropertyItem *gpencil_build_time_mode_filter(bContext * /*C*/,
                                                       PointerRNA *ptr,
                                                       PropertyRNA * /*prop*/,
                                                       bool *r_free)
{

  GpencilModifierData *md = static_cast<GpencilModifierData *>(ptr->data);
  BuildGpencilModifierData *mmd = (BuildGpencilModifierData *)md;
  const bool is_concurrent = (mmd->mode == GP_BUILD_MODE_CONCURRENT);

  EnumPropertyItem *item_list = nullptr;
  int totitem = 0;

  for (const EnumPropertyItem *item = gpencil_build_time_mode_items; item->identifier != nullptr;
       item++)
  {
    if (is_concurrent && (item->value == GP_BUILD_TIMEMODE_DRAWSPEED)) {
      continue;
    }
    RNA_enum_item_add(&item_list, &totitem, item);
  }

  RNA_enum_item_end(&item_list, &totitem);
  *r_free = true;

  return item_list;
}

#else

static void rna_def_modifier_gpencilnoise(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NoiseGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Noise Modifier", "Noise effect modifier");
  RNA_def_struct_sdna(srna, "NoiseGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_NOISE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_NoiseGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_NoiseGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "factor");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_ui_text(prop, "Offset Factor", "Amount of noise to apply");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "factor_strength");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_ui_text(prop, "Strength Factor", "Amount of noise to apply to opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor_thickness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "factor_thickness");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_ui_text(prop, "Thickness Factor", "Amount of noise to apply to thickness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor_uvs", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "factor_uvs");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_ui_text(prop, "UV Factor", "Amount of noise to apply to UV rotation");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_USE_RANDOM);
  RNA_def_property_ui_text(prop, "Random", "Use random values over time");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Noise Seed", "Random seed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "noise_scale");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Noise Scale", "Scale the noise frequency");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "noise_offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "noise_offset");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Noise Offset", "Offset the noise along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define noise effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "step");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(prop, "Step", "Number of frames between randomization steps");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_NOISE_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "noise_mode");
  RNA_def_property_enum_items(prop, modifier_noise_random_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Where to perform randomization");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilsmooth(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SmoothGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Smooth Modifier", "Smooth effect modifier");
  RNA_def_struct_sdna(srna, "SmoothGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_SmoothGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_SmoothGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "factor");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Factor", "Amount of smooth to apply");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_position", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_MOD_LOCATION);
  RNA_def_property_ui_text(
      prop, "Affect Position", "The modifier affects the position of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_MOD_STRENGTH);
  RNA_def_property_ui_text(
      prop, "Affect Strength", "The modifier affects the color strength of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_thickness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_MOD_THICKNESS);
  RNA_def_property_ui_text(
      prop, "Affect Thickness", "The modifier affects the thickness of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_MOD_UV);
  RNA_def_property_ui_text(
      prop, "Affect UV", "The modifier affects the UV rotation factor of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "step");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_text(
      prop, "Steps", "Number of times to apply smooth (high numbers can reduce fps)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_keep_shape", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_KEEP_SHAPE);
  RNA_def_property_ui_text(prop, "Keep Shape", "Smooth the details, but keep the overall shape");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SMOOTH_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define smooth effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilsubdiv(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SubdivGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Subdivision Modifier", "Subdivide Stroke modifier");
  RNA_def_struct_sdna(srna, "SubdivGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SUBSURF);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_SubdivGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "level", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "level");
  RNA_def_property_range(prop, 0, 16);
  RNA_def_property_ui_range(prop, 0.0, 5.0, 1, 0);
  RNA_def_property_ui_text(prop, "Level", "Number of subdivisions");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "subdivision_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, gpencil_subdivision_type_items);
  RNA_def_property_ui_text(prop, "Subdivision Type", "Select type of subdivision algorithm");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SUBDIV_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SUBDIV_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SUBDIV_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SUBDIV_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilsimplify(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static EnumPropertyItem prop_gpencil_simplify_mode_items[] = {
      {GP_SIMPLIFY_FIXED,
       "FIXED",
       ICON_IPO_CONSTANT,
       "Fixed",
       "Delete alternating vertices in the stroke, except extremes"},
      {GP_SIMPLIFY_ADAPTIVE,
       "ADAPTIVE",
       ICON_IPO_EASE_IN_OUT,
       "Adaptive",
       "Use a Ramer-Douglas-Peucker algorithm to simplify the stroke preserving main shape"},
      {GP_SIMPLIFY_SAMPLE,
       "SAMPLE",
       ICON_IPO_EASE_IN_OUT,
       "Sample",
       "Re-sample the stroke with segments of the specified length"},
      {GP_SIMPLIFY_MERGE,
       "MERGE",
       ICON_IPO_EASE_IN_OUT,
       "Merge",
       "Simplify the stroke by merging vertices closer than a given distance"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SimplifyGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Simplify Modifier", "Simplify Stroke modifier");
  RNA_def_struct_sdna(srna, "SimplifyGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SIMPLIFY);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_SimplifyGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "factor");
  RNA_def_property_range(prop, 0, 100.0);
  RNA_def_property_ui_range(prop, 0, 5.0f, 1.0f, 3);
  RNA_def_property_ui_text(prop, "Factor", "Factor of Simplify");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SIMPLIFY_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SIMPLIFY_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SIMPLIFY_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SIMPLIFY_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gpencil_simplify_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "How to simplify the stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "step");
  RNA_def_property_range(prop, 1, 50);
  RNA_def_property_ui_text(prop, "Iterations", "Number of times to apply simplify");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Sample */
  prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "length");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.005, 1.0, 0.05, 3);
  RNA_def_property_ui_text(prop, "Length", "Length of each segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "sharp_threshold", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "sharp_threshold");
  RNA_def_property_range(prop, 0, M_PI);
  RNA_def_property_ui_range(prop, 0, M_PI, 1.0, 1);
  RNA_def_property_ui_text(
      prop, "Sharp Threshold", "Preserve corners that have sharper angle than this threshold");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Merge */
  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "distance");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1.0, 0.01, 3);
  RNA_def_property_ui_text(prop, "Distance", "Distance between points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilthick(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThickGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Thick Modifier", "Subdivide and Smooth Stroke modifier");
  RNA_def_struct_sdna(srna, "ThickGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_THICKNESS);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_ThickGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ThickGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "thickness");
  RNA_def_property_range(prop, -100, 500);
  RNA_def_property_ui_text(prop, "Thickness", "Absolute thickness to apply everywhere");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "thickness_fac");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 10.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Thickness Factor", "Factor to multiply the thickness with");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_weight_factor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_WEIGHT_FACTOR);
  RNA_def_property_ui_text(prop, "Weighted", "Use weight to modulate effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define thickness change along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_normalized_thickness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_THICK_NORMALIZE);
  RNA_def_property_ui_text(prop, "Uniform Thickness", "Replace the stroke thickness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_thickness");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpenciloffset(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_define_lib_overridable(true);
  static EnumPropertyItem rna_enum_offset_mode_items[] = {
      {GP_OFFSET_RANDOM, "RANDOM", 0, "Random", "Randomize stroke offset"},
      {GP_OFFSET_LAYER, "LAYER", 0, "Layer", "Offset layers by the same factor"},
      {GP_OFFSET_STROKE,
       "STROKE",
       0,
       "Stroke",
       "Offset strokes by the same factor based on stroke draw order"},
      {GP_OFFSET_MATERIAL, "MATERIAL", 0, "Material", "Offset materials by the same factor"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "OffsetGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Offset Modifier", "Offset Stroke modifier");
  RNA_def_struct_sdna(srna, "OffsetGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_OFFSET);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_offset_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_OffsetGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_OffsetGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OFFSET_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OFFSET_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OFFSET_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OFFSET_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OFFSET_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "loc");
  RNA_def_property_ui_text(prop, "Location", "Values for change location");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "rot");
  RNA_def_property_ui_text(prop, "Rotation", "Values for changes in rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_ui_text(prop, "Scale", "Values for changes in scale");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "rnd_offset");
  RNA_def_property_ui_text(prop, "Random Offset", "Value for changes in location");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "rnd_rot");
  RNA_def_property_ui_text(prop, "Random Rotation", "Value for changes in rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "rnd_scale");
  RNA_def_property_ui_text(prop, "Scale", "Value for changes in scale");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Seed", "Random seed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "stroke_step", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Step", "Number of elements that will be grouped");
  RNA_def_property_range(prop, 1, 500);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "stroke_start_offset", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Start Offset", "Offset starting point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_uniform_random_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OFFSET_UNIFORM_RANDOM_SCALE);
  RNA_def_property_ui_text(
      prop, "Uniform Scale", "Use the same random seed for each scale axis for a uniform scale");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpenciltint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* modes */
  static EnumPropertyItem tint_mode_types_items[] = {
      {GPPAINT_MODE_STROKE, "STROKE", 0, "Stroke", "Vertex Color affects to Stroke only"},
      {GPPAINT_MODE_FILL, "FILL", 0, "Fill", "Vertex Color affects to Fill only"},
      {GPPAINT_MODE_BOTH, "BOTH", 0, "Stroke & Fill", "Vertex Color affects to Stroke and Fill"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "TintGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Tint Modifier", "Tint modifier");
  RNA_def_struct_sdna(srna, "TintGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_COLOR);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Parent object to define the center of the effect");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_TintGpencilModifier_object_set", nullptr, nullptr);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_TintGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_TintGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse Vertex Group", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "factor");
  RNA_def_property_range(prop, 0, 2.0);
  RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 2);
  RNA_def_property_ui_text(prop, "Strength", "Factor for tinting");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_weight_factor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_WEIGHT_FACTOR);
  RNA_def_property_ui_text(prop, "Weighted", "Use weight to modulate effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "radius");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Radius", "Defines the maximum distance of the effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Mode type. */
  prop = RNA_def_property(srna, "vertex_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, tint_mode_types_items);
  RNA_def_property_ui_text(prop, "Mode", "Defines how vertex color affect to the strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Type of Tint. */
  prop = RNA_def_property(srna, "tint_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, gpencil_tint_type_items);
  RNA_def_property_ui_text(prop, "Tint Type", "Select type of tinting algorithm");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Simple Color. */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "rgb");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Color used for tinting");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Color band. */
  prop = RNA_def_property(srna, "colors", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "colorband");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Colors", "Color ramp used to define tinting colors");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TINT_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define vertex color effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpenciltime(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  srna = RNA_def_struct(brna, "TimeGpencilModifierSegment", nullptr);
  RNA_def_struct_ui_text(srna, "Time Modifier Segment", "Configuration for a single dash segment");
  RNA_def_struct_sdna(srna, "TimeGpencilModifierSegment");
  RNA_def_struct_path_func(srna, "rna_TimeGpencilModifierSegment_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of the dash segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_TimeGpencilModifierSegment_name_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, nullptr);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "seg_start", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, INT16_MAX);
  RNA_def_property_ui_text(prop, "Frame Start", "First frame of the segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seg_end", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, INT16_MAX);
  RNA_def_property_ui_text(prop, "End", "Last frame of the segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seg_repeat", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, INT16_MAX);
  RNA_def_property_ui_text(prop, "Repeat", "Number of cycle repeats");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seg_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "seg_mode");
  RNA_def_property_enum_items(prop, rna_enum_time_seg_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  srna = RNA_def_struct(brna, "TimeGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Time Offset Modifier", "Time offset modifier");
  RNA_def_struct_sdna(srna, "TimeGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TIME);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "segments", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "TimeGpencilModifierSegment");
  RNA_def_property_collection_sdna(prop, nullptr, "segments", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    "rna_GpencilTime_segments_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Segments", "");

  prop = RNA_def_property(srna, "segment_active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Dash Segment Index", "Active index in the segment list");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_time_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TIME_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TIME_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "offset");
  RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Frame Offset", "Number of frames to offset original keyframe number or frame to fix");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "frame_scale");
  RNA_def_property_range(prop, 0.001f, 100.0f);
  RNA_def_property_ui_text(prop, "Frame Scale", "Evaluation time in seconds");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "sfra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_TimeModifier_start_frame_set", nullptr);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Start Frame", "First frame of the range");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, nullptr, "efra");
  RNA_def_property_int_funcs(prop, nullptr, "rna_TimeModifier_end_frame_set", nullptr);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "End Frame", "Final frame of the range");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_keep_loop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TIME_KEEP_LOOP);
  RNA_def_property_ui_text(
      prop, "Keep Loop", "Retiming end frames and move to start of animation to keep loop");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TIME_CUSTOM_RANGE);
  RNA_def_property_ui_text(
      prop, "Custom Range", "Define a custom range of frames to use in modifier");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilcolor(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ColorGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Hue/Saturation Modifier", "Change Hue/Saturation modifier");
  RNA_def_struct_sdna(srna, "ColorGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TINT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "modify_color", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_modify_color_items); /* share the enum */
  RNA_def_property_ui_text(prop, "Mode", "Set what colors of the stroke are affected");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_ColorGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "hue", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "hsv[0]");
  RNA_def_property_ui_text(prop, "Hue", "Color Hue");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "hsv[1]");
  RNA_def_property_ui_text(prop, "Saturation", "Color Saturation");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "hsv[2]");
  RNA_def_property_ui_text(prop, "Value", "Color Value");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_COLOR_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_COLOR_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_COLOR_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_COLOR_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_COLOR_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define color effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilopacity(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "OpacityGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Opacity Modifier", "Opacity of Strokes modifier");
  RNA_def_struct_sdna(srna, "OpacityGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_OPACITY);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "modify_color", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_modify_opacity_items);
  RNA_def_property_ui_text(prop, "Mode", "Set what colors of the stroke are affected");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_OpacityGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_OpacityGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "factor");
  RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 2);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_GpencilOpacity_max_set", "rna_GpencilOpacity_range");
  RNA_def_property_ui_text(prop, "Opacity Factor", "Factor of Opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "hardness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "hardeness");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 0.1, 2);
  RNA_def_property_ui_text(prop, "Hardness", "Factor of stroke hardness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_weight_factor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_WEIGHT_FACTOR);
  RNA_def_property_ui_text(prop, "Weighted", "Use weight to modulate effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_normalized_opacity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_NORMALIZE);
  RNA_def_property_ui_text(prop, "Uniform Opacity", "Replace the stroke opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_opacity_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OPACITY_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define opacity effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpenciloutline(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "OutlineGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Outline Modifier", "Outline of Strokes modifier from camera view");
  RNA_def_struct_sdna(srna, "OutlineGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_OUTLINE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_OutlineGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OUTLINE_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OUTLINE_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OUTLINE_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OUTLINE_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "thickness");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of the perimeter stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "sample_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sample_length");
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 0.1f, 2);
  RNA_def_property_ui_text(prop, "Sample Length", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "subdivision", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "subdiv");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(prop, "Subdivisions", "Number of subdivisions");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_keep_shape", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_OUTLINE_KEEP_SHAPE);
  RNA_def_property_ui_text(prop, "Keep Shape", "Try to keep global shape");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "outline_material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_OutlineStrokeGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Outline Material", "Material used for outline strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Target Object", "Target object to define stroke start");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_OutlineGpencilModifier_object_set", nullptr, nullptr);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilarray(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ArrayGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Instance Modifier", "Create grid of duplicate instances");
  RNA_def_struct_sdna(srna, "ArrayGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_ARRAY);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_ArrayGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "count", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 50, 1, -1);
  RNA_def_property_ui_text(prop, "Count", "Number of items");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Offset parameters */
  prop = RNA_def_property(srna, "offset_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "object");
  RNA_def_property_ui_text(
      prop,
      "Offset Object",
      "Use the location and rotation of another object to determine the distance and "
      "rotational change between arrayed items");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "constant_offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_ui_text(prop, "Constant Offset", "Value for the distance between items");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "relative_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "shift");
  RNA_def_property_ui_text(
      prop,
      "Relative Offset",
      "The size of the geometry will determine the distance between arrayed items");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "rnd_offset");
  RNA_def_property_ui_text(prop, "Random Offset", "Value for changes in location");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "rnd_rot");
  RNA_def_property_ui_text(prop, "Random Rotation", "Value for changes in rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "rnd_scale");
  RNA_def_property_ui_text(prop, "Scale", "Value for changes in scale");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Seed", "Random seed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "replace_material", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mat_rpl");
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Material",
      "Index of the material used for generated strokes (0 keep original material)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_constant_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_USE_OFFSET);
  RNA_def_property_ui_text(prop, "Offset", "Enable offset");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_object_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_USE_OB_OFFSET);
  RNA_def_property_ui_text(prop, "Use Object Offset", "Enable object offset");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_relative_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_USE_RELATIVE);
  RNA_def_property_ui_text(prop, "Shift", "Enable shift");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_uniform_random_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ARRAY_UNIFORM_RANDOM_SCALE);
  RNA_def_property_ui_text(
      prop, "Uniform Scale", "Use the same random seed for each scale axis for a uniform scale");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilbuild(BlenderRNA *brna)
{
  static EnumPropertyItem prop_gpencil_build_mode_items[] = {
      {GP_BUILD_MODE_SEQUENTIAL,
       "SEQUENTIAL",
       0,
       "Sequential",
       "Strokes appear/disappear one after the other, but only a single one changes at a time"},
      {GP_BUILD_MODE_CONCURRENT,
       "CONCURRENT",
       0,
       "Concurrent",
       "Multiple strokes appear/disappear at once"},
      {GP_BUILD_MODE_ADDITIVE,
       "ADDITIVE",
       0,
       "Additive",
       "Builds only new strokes (assuming 'additive' drawing)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_gpencil_build_transition_items[] = {
      {GP_BUILD_TRANSITION_GROW,
       "GROW",
       0,
       "Grow",
       "Show points in the order they occur in each stroke "
       "(e.g. for animating lines being drawn)"},
      {GP_BUILD_TRANSITION_SHRINK,
       "SHRINK",
       0,
       "Shrink",
       "Hide points from the end of each stroke to the start "
       "(e.g. for animating lines being erased)"},
      {GP_BUILD_TRANSITION_VANISH,
       "FADE", /* "Fade" is the original id string kept for compatibility purpose. */
       0,
       "Vanish",
       "Hide points in the order they occur in each stroke "
       "(e.g. for animating ink fading or vanishing after getting drawn)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_gpencil_build_time_align_items[] = {
      {GP_BUILD_TIMEALIGN_START,
       "START",
       0,
       "Align Start",
       "All strokes start at same time (i.e. short strokes finish earlier)"},
      {GP_BUILD_TIMEALIGN_END,
       "END",
       0,
       "Align End",
       "All strokes end at same time (i.e. short strokes start later)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BuildGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Build Modifier", "Animate strokes appearing and disappearing");
  RNA_def_struct_sdna(srna, "BuildGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_BUILD);

  RNA_define_lib_overridable(true);

  /* Mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gpencil_build_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "How strokes are being built");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Direction */
  prop = RNA_def_property(srna, "transition", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gpencil_build_transition_items);
  RNA_def_property_ui_text(
      prop, "Transition", "How are strokes animated (i.e. are they appearing or disappearing)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Transition Onset Delay + Length */
  prop = RNA_def_property(srna, "start_delay", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "start_delay");
  RNA_def_property_ui_text(
      prop, "Delay", "Number of frames after each GP keyframe before the modifier has any effect");
  RNA_def_property_range(prop, 0, MAXFRAMEF);
  RNA_def_property_ui_range(prop, 0, 200, 1, -1);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "length");
  RNA_def_property_ui_text(prop,
                           "Length",
                           "Maximum number of frames that the build effect can run for "
                           "(unless another GP keyframe occurs before this time has elapsed)");
  RNA_def_property_range(prop, 1, MAXFRAMEF);
  RNA_def_property_ui_range(prop, 1, 1000, 1, -1);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Concurrent Mode Settings */
  prop = RNA_def_property(srna, "concurrent_time_alignment", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "time_alignment");
  RNA_def_property_enum_items(prop, prop_gpencil_build_time_align_items);
  RNA_def_property_ui_text(prop, "Time Alignment", "How should strokes start to appear/disappear");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Which time mode to use: Current frames, manual percentage, or draw-speed. */
  prop = RNA_def_property(srna, "time_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "time_mode");
  RNA_def_property_enum_items(prop, gpencil_build_time_mode_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "gpencil_build_time_mode_filter");
  RNA_def_property_ui_text(
      prop,
      "Timing",
      "Use drawing speed, a number of frames, or a manual factor to build strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Speed factor for GP_BUILD_TIMEMODE_DRAWSPEED. */
  /* Todo: Does it work? */
  prop = RNA_def_property(srna, "speed_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fac");
  RNA_def_property_ui_text(prop, "Speed Factor", "Multiply recorded drawing speed by a factor");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0, 5, 0.001, -1);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Max gap in seconds between strokes for GP_BUILD_TIMEMODE_DRAWSPEED. */
  prop = RNA_def_property(srna, "speed_maxgap", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_maxgap");
  RNA_def_property_ui_text(prop, "Maximum Gap", "The maximum gap between strokes in seconds");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0, 4, 0.01, -1);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Time Limits */
  prop = RNA_def_property(srna, "use_restrict_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BUILD_RESTRICT_TIME);
  RNA_def_property_ui_text(
      prop, "Restrict Frame Range", "Only modify strokes during the specified frame range");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Use percentage bool (used by sequential & concurrent modes) */
  prop = RNA_def_property(srna, "use_percentage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "time_mode", GP_BUILD_TIMEMODE_PERCENTAGE);
  RNA_def_property_ui_text(
      prop, "Restrict Visible Points", "Use a percentage factor to determine the visible points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Percentage factor. */
  prop = RNA_def_property(srna, "percentage_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "percentage_fac");
  RNA_def_property_ui_text(prop, "Factor", "Defines how much of the stroke is visible");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "start_frame");
  RNA_def_property_ui_text(
      prop, "Start Frame", "Start Frame (when Restrict Frame Range is enabled)");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "end_frame");
  RNA_def_property_ui_text(prop, "End Frame", "End Frame (when Restrict Frame Range is enabled)");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_fading", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BUILD_USE_FADING);
  RNA_def_property_ui_text(prop, "Use Fading", "Fade out strokes instead of directly cutting off");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fade_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fade_fac");
  RNA_def_property_ui_text(prop, "Fade Factor", "Defines how much of the stroke is fading in/out");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "target_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "target_vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Output Vertex group");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_BuildGpencilModifier_target_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fade_opacity_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fade_opacity_strength");
  RNA_def_property_ui_text(
      prop, "Opacity Strength", "How much strength fading applies on top of stroke opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fade_thickness_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fade_thickness_strength");
  RNA_def_property_ui_text(
      prop, "Thickness Strength", "How much strength fading applies on top of stroke thickness");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Object used as build starting position");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_BuildGpencilModifier_object_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  /* Filters - Layer */
  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BUILD_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_BUILD_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencillattice(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LatticeGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Lattice Modifier", "Change stroke using lattice to deform modifier");
  RNA_def_struct_sdna(srna, "LatticeGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_LATTICE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_LatticeGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_LatticeGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LATTICE_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LATTICE_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LATTICE_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LATTICE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LATTICE_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Lattice object to deform with");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_LatticeGpencilModifier_object_set", nullptr, "rna_Lattice_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 10, 2);
  RNA_def_property_ui_text(prop, "Strength", "Strength of modifier effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilmirror(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MirrorGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Mirror Modifier", "Create mirroring strokes");
  RNA_def_struct_sdna(srna, "MirrorGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MIRROR);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_MirrorGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Object used as center");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_MirrorGpencilModifier_object_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_CLIPPING);
  RNA_def_property_ui_text(prop, "Clip", "Clip points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_axis_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_AXIS_X);
  RNA_def_property_ui_text(prop, "X", "Mirror the X axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_axis_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_AXIS_Y);
  RNA_def_property_ui_text(prop, "Y", "Mirror the Y axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_axis_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_AXIS_Z);
  RNA_def_property_ui_text(prop, "Z", "Mirror the Z axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilhook(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "HookGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Hook Modifier", "Hook modifier to modify the location of stroke points");
  RNA_def_struct_sdna(srna, "HookGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_HOOK);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Object", "Parent Object for hook, also recalculates and clears offset");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_HookGpencilModifier_object_set", nullptr, nullptr);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "subtarget");
  RNA_def_property_ui_text(
      prop,
      "Sub-Target",
      "Name of Parent Bone for hook (if applicable), also recalculates and clears offset");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_HookGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_HookGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_HOOK_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_HOOK_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_HOOK_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_HOOK_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse Vertex Group", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_HOOK_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "force");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Strength", "Relative force of the hook");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_gphook_falloff_items); /* share the enum */
  RNA_def_property_ui_text(prop, "Falloff Type", "");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "falloff");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 100, 2);
  RNA_def_property_ui_text(
      prop, "Radius", "If not zero, the distance from the hook where influence ends");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curfalloff");
  RNA_def_property_ui_text(prop, "Falloff Curve", "Custom falloff curve");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "cent");
  RNA_def_property_ui_text(prop, "Hook Center", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "parentinv");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Matrix", "Reverse the transformation between this object and its target");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_falloff_uniform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_HOOK_UNIFORM_SPACE);
  RNA_def_property_ui_text(prop, "Uniform Falloff", "Compensate for non-uniform object scale");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilarmature(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ArmatureGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Armature Modifier", "Change stroke using armature to deform modifier");
  RNA_def_struct_sdna(srna, "ArmatureGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_ARMATURE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Armature object to deform with");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_ArmatureGpencilModifier_object_set",
                                 nullptr,
                                 "rna_Armature_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_bone_envelopes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "deformflag", ARM_DEF_ENVELOPE);
  RNA_def_property_ui_text(prop, "Use Bone Envelopes", "Bind Bone envelopes to armature modifier");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_vertex_groups", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "deformflag", ARM_DEF_VGROUP);
  RNA_def_property_ui_text(prop, "Use Vertex Groups", "Bind vertex groups to armature modifier");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_deform_preserve_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "deformflag", ARM_DEF_QUATERNION);
  RNA_def_property_ui_text(
      prop, "Preserve Volume", "Deform rotation interpolation with quaternions");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ArmatureGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "deformflag", ARM_DEF_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilmultiply(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MultiplyGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Multiply Modifier", "Generate multiple strokes from one stroke");
  RNA_def_struct_sdna(srna, "MultiplyGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_GP_MULTIFRAME_EDITING);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_MultiplyGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MIRROR_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", GP_MULTIPLY_ENABLE_FADING);
  RNA_def_property_ui_text(prop, "Fade", "Fade the stroke thickness for each generated stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "duplicates", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "duplications");
  RNA_def_property_range(prop, 0, 999);
  RNA_def_property_ui_range(prop, 1, 10, 1, 1);
  RNA_def_property_ui_text(prop, "Duplicates", "How many copies of strokes be displayed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.01, 3);
  RNA_def_property_ui_text(prop, "Distance", "Distance of duplications");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_range(prop, -1, 1, 0.01, 3);
  RNA_def_property_ui_text(prop, "Offset", "Offset of duplicates. -1 to 1: inner to outer");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fading_thickness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Thickness", "Fade influence of stroke's thickness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fading_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Opacity", "Fade influence of stroke's opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fading_center", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Center", "Fade center");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpenciltexture(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem fit_type_items[] = {
      {GP_TEX_CONSTANT_LENGTH,
       "CONSTANT_LENGTH",
       0,
       "Constant Length",
       "Keep the texture at a constant length regardless of the length of each stroke"},
      {GP_TEX_FIT_STROKE,
       "FIT_STROKE",
       0,
       "Stroke Length",
       "Scale the texture to fit the length of each stroke"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mode_items[] = {
      {STROKE, "STROKE", 0, "Stroke", "Manipulate only stroke texture coordinates"},
      {FILL, "FILL", 0, "Fill", "Manipulate only fill texture coordinates"},
      {STROKE_AND_FILL,
       "STROKE_AND_FILL",
       0,
       "Stroke & Fill",
       "Manipulate both stroke and fill texture coordinates"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "TextureGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Texture Modifier", "Transform stroke texture coordinates Modifier");
  RNA_def_struct_sdna(srna, "TextureGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_UVPROJECT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TEX_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_TextureGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TEX_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_TextureGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TEX_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TEX_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_TEX_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "uv_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uv_offset");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100.0, 100.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "UV Offset", "Offset value to add to stroke UVs");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "uv_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "uv_scale");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "UV Scale", "Factor to scale the UVs");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Rotation of Dot Texture. */
  prop = RNA_def_property(srna, "alignment_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "alignment_rotation");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -DEG2RADF(90.0f), DEG2RADF(90.0f));
  RNA_def_property_ui_range(prop, -DEG2RADF(90.0f), DEG2RADF(90.0f), 10, 3);
  RNA_def_property_ui_text(
      prop, "Rotation", "Additional rotation applied to dots and square strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fill_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "fill_rotation");
  RNA_def_property_ui_text(prop, "Fill Rotation", "Additional rotation of the fill UV");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fill_offset", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, nullptr, "fill_offset");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Fill Offset", "Additional offset of the fill UV");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fill_scale", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, nullptr, "fill_scale");
  RNA_def_property_range(prop, 0.01f, 100.0f);
  RNA_def_property_ui_text(prop, "Fill Scale", "Additional scale of the fill UV");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fit_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "fit_method");
  RNA_def_property_enum_items(prop, fit_type_items);
  RNA_def_property_ui_text(prop, "Fit Method", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilweight_proximity(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WeightProxGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Weight Modifier Proximity", "Calculate Vertex Weight dynamically");
  RNA_def_struct_sdna(srna, "WeightProxGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "target_vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Output Vertex group");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_WeightProxGpencilModifier_target_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_multiply", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_MULTIPLY_DATA);
  RNA_def_property_ui_text(
      prop,
      "Multiply Weights",
      "Multiply the calculated weights with the existing values in the vertex group");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_invert_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_OUTPUT);
  RNA_def_property_ui_text(prop, "Invert", "Invert output weight values");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_WeightProxGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_WeightProxGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Distance reference object */
  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Target Object", "Object used as distance reference");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_WeightProxGpencilModifier_object_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "distance_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "dist_start");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 1.0, 2);
  RNA_def_property_ui_text(prop, "Lowest", "Distance mapping to 0.0 weight");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "minimum_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "min_weight");
  RNA_def_property_ui_text(prop, "Minimum", "Minimum value for vertex weight");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "distance_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "dist_end");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 1.0, 2);
  RNA_def_property_ui_text(prop, "Highest", "Distance mapping to 1.0 weight");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilweight_angle(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem axis_items[] = {
      {0, "X", 0, "X", ""},
      {1, "Y", 0, "Y", ""},
      {2, "Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem space_items[] = {
      {GP_SPACE_LOCAL, "LOCAL", 0, "Local Space", ""},
      {GP_SPACE_WORLD, "WORLD", 0, "World Space", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "WeightAngleGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Weight Modifier Angle", "Calculate Vertex Weight dynamically");
  RNA_def_struct_sdna(srna, "WeightAngleGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_VERTEX_WEIGHT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "target_vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Output Vertex group");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_WeightAngleGpencilModifier_target_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_multiply", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_MULTIPLY_DATA);
  RNA_def_property_ui_text(
      prop,
      "Multiply Weights",
      "Multiply the calculated weights with the existing values in the vertex group");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_invert_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_OUTPUT);
  RNA_def_property_ui_text(prop, "Invert", "Invert output weight values");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle");
  RNA_def_property_ui_text(prop, "Angle", "Angle");
  RNA_def_property_range(prop, 0.0f, DEG2RAD(180.0f));
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "axis");
  RNA_def_property_enum_items(prop, axis_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "space");
  RNA_def_property_enum_items(prop, space_items);
  RNA_def_property_ui_text(prop, "Space", "Coordinates space");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_WeightAngleGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_WeightAngleGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "minimum_weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "min_weight");
  RNA_def_property_ui_text(prop, "Minimum", "Minimum value for vertex weight");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_WEIGHT_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencillineart(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem modifier_lineart_source_type[] = {
      {LRT_SOURCE_COLLECTION, "COLLECTION", 0, "Collection", ""},
      {LRT_SOURCE_OBJECT, "OBJECT", 0, "Object", ""},
      {LRT_SOURCE_SCENE, "SCENE", 0, "Scene", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem modifier_lineart_shadow_region_filtering[] = {
      {LRT_SHADOW_FILTER_NONE,
       "NONE",
       0,
       "None",
       "Not filtering any lines based on illumination region"},
      {LRT_SHADOW_FILTER_ILLUMINATED,
       "ILLUMINATED",
       0,
       "Illuminated",
       "Only selecting lines from illuminated regions"},
      {LRT_SHADOW_FILTER_SHADED,
       "SHADED",
       0,
       "Shaded",
       "Only selecting lines from shaded regions"},
      {LRT_SHADOW_FILTER_ILLUMINATED_ENCLOSED_SHAPES,
       "ILLUMINATED_ENCLOSED",
       0,
       "Illuminated (Enclosed Shapes)",
       "Selecting lines from lit regions, and make the combination of contour, light contour and "
       "shadow lines into enclosed shapes"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem modifier_lineart_silhouette_filtering[] = {
      {LRT_SILHOUETTE_FILTER_NONE, "NONE", 0, "Contour", ""},
      {LRT_SILHOUETTE_FILTER_GROUP, "GROUP", 0, "Silhouette", ""},
      {LRT_SILHOUETTE_FILTER_INDIVIDUAL, "INDIVIDUAL", 0, "Individual Silhouette", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "LineartGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Line Art Modifier", "Generate line art strokes from selected source");
  RNA_def_struct_sdna(srna, "LineartGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_LINEART);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_custom_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_USE_CUSTOM_CAMERA);
  RNA_def_property_ui_text(
      prop, "Use Custom Camera", "Use custom camera instead of the active camera");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_fuzzy_intersections", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_INTERSECTION_AS_CONTOUR);
  RNA_def_property_ui_text(prop,
                           "Intersection With Contour",
                           "Treat intersection and contour lines as if they were the same type so "
                           "they can be chained together");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_fuzzy_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_EVERYTHING_AS_CONTOUR);
  RNA_def_property_ui_text(
      prop, "All Lines", "Treat all lines as the same line type so they can be chained together");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_object_instances", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_ALLOW_DUPLI_OBJECTS);
  RNA_def_property_ui_text(prop,
                           "Instanced Objects",
                           "Allow particle objects and face/vertex instances to show in line art");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edge_overlap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_ALLOW_OVERLAPPING_EDGES);
  RNA_def_property_ui_text(
      prop,
      "Handle Overlapping Edges",
      "Allow edges in the same location (i.e. from edge split) to show properly. May run slower");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_clip_plane_boundaries", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_ALLOW_CLIPPING_BOUNDARIES);
  RNA_def_property_ui_text(prop,
                           "Clipping Boundaries",
                           "Allow lines generated by the near/far clipping plane to be shown");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "crease_threshold", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0, DEG2RAD(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RAD(180.0f), 0.01f, 1);
  RNA_def_property_ui_text(prop,
                           "Crease Threshold",
                           "Angles smaller than this will be treated as creases. Crease angle "
                           "priority: object line art crease override > mesh auto smooth angle > "
                           "line art default crease");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "split_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle_splitting_threshold");
  RNA_def_property_ui_text(
      prop, "Angle Splitting", "Angle in screen space below which a stroke is split in two");
  /* Don't allow value very close to PI, or we get a lot of small segments. */
  RNA_def_property_ui_range(prop, 0.0f, DEG2RAD(179.5f), 0.01f, 1);
  RNA_def_property_range(prop, 0.0f, DEG2RAD(180.0f));
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "smooth_tolerance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "chain_smooth_tolerance");
  RNA_def_property_ui_text(
      prop, "Smooth Tolerance", "Strength of smoothing applied on jagged chains");
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.05f, 4);
  RNA_def_property_range(prop, 0.0f, 30.0f);
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_loose_as_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_LOOSE_AS_CONTOUR);
  RNA_def_property_ui_text(prop, "Loose As Contour", "Loose edges will have contour type");
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_source_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "calculation_flags", LRT_GPENCIL_INVERT_SOURCE_VGROUP);
  RNA_def_property_ui_text(prop, "Invert Vertex Group", "Invert source vertex group values");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_output_vertex_group_match_by_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "calculation_flags", LRT_GPENCIL_MATCH_OUTPUT_VGROUP);
  RNA_def_property_ui_text(prop, "Match Output", "Match output vertex group based on name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_face_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_FILTER_FACE_MARK);
  RNA_def_property_ui_text(
      prop, "Filter Face Marks", "Filter feature lines using freestyle face marks");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_face_mark_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_FILTER_FACE_MARK_INVERT);
  RNA_def_property_ui_text(prop, "Invert", "Invert face mark filtering");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_face_mark_boundaries", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "calculation_flags", LRT_FILTER_FACE_MARK_BOUNDARIES);
  RNA_def_property_ui_text(
      prop, "Boundaries", "Filter feature lines based on face mark boundaries");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_face_mark_keep_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "calculation_flags", LRT_FILTER_FACE_MARK_KEEP_CONTOUR);
  RNA_def_property_ui_text(prop, "Keep Contour", "Preserve contour lines while filtering");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "chaining_image_threshold", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(
      prop,
      "Image Threshold",
      "Segments with an image distance smaller than this will be chained together");
  RNA_def_property_ui_range(prop, 0.0f, 0.3f, 0.001f, 4);
  RNA_def_property_range(prop, 0.0f, 0.3f);
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_loose_edge_chain", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_CHAIN_LOOSE_EDGES);
  RNA_def_property_ui_text(prop, "Chain Loose Edges", "Allow loose edges to be chained together");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_geometry_space_chain", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_CHAIN_GEOMETRY_SPACE);
  RNA_def_property_ui_text(
      prop, "Use Geometry Space", "Use geometry distance for chaining instead of image space");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_detail_preserve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_CHAIN_PRESERVE_DETAILS);
  RNA_def_property_ui_text(
      prop, "Preserve Details", "Keep the zig-zag \"noise\" in initial chaining");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_overlap_edge_type_support", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_ALLOW_OVERLAP_EDGE_TYPES);
  RNA_def_property_ui_text(prop,
                           "Overlapping Edge Types",
                           "Allow an edge to have multiple overlapping types. This will create a "
                           "separate stroke for each overlapping type");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "stroke_depth_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_text(prop,
                           "Stroke Depth Offset",
                           "Move strokes slightly towards the camera to avoid clipping while "
                           "preserve depth for the viewport");
  RNA_def_property_ui_range(prop, 0.0, 0.5, 0.001, 4);
  RNA_def_property_range(prop, -0.1, FLT_MAX);
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_offset_towards_custom_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_GPENCIL_OFFSET_TOWARDS_CUSTOM_CAMERA);
  RNA_def_property_ui_text(prop,
                           "Offset Towards Custom Camera",
                           "Offset strokes towards selected camera instead of the active camera");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "source_camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Camera Object", "Use specified camera object for generating line art");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "light_contour_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Light Object", "Use this light object to generate light contour");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "source_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_lineart_source_type);
  RNA_def_property_ui_text(prop, "Source Type", "Line art stroke source type");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "source_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_ui_text(prop, "Object", "Generate strokes from this object");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "source_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(
      prop, "Collection", "Generate strokes from the objects in this collection");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  /* types */
  prop = RNA_def_property(srna, "use_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_CONTOUR);
  RNA_def_property_ui_text(prop, "Use Contour", "Generate strokes from contours lines");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_loose", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_LOOSE);
  RNA_def_property_ui_text(prop, "Use Loose", "Generate strokes from loose edges");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_CREASE);
  RNA_def_property_ui_text(prop, "Use Crease", "Generate strokes from creased edges");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_material", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_MATERIAL);
  RNA_def_property_ui_text(
      prop, "Use Material", "Generate strokes from borders between materials");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edge_mark", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_EDGE_MARK);
  RNA_def_property_ui_text(prop, "Use Edge Mark", "Generate strokes from freestyle marked edges");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_intersection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_INTERSECTION);
  RNA_def_property_ui_text(prop, "Use Intersection", "Generate strokes from intersections");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_light_contour", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_LIGHT_CONTOUR);
  RNA_def_property_ui_text(prop,
                           "Use Light Contour",
                           "Generate light/shadow separation lines from a reference light object");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "edge_types", LRT_EDGE_FLAG_PROJECTED_SHADOW);
  RNA_def_property_ui_text(
      prop, "Use Shadow", "Project contour lines using a light source object");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "shadow_region_filtering", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shadow_selection");
  RNA_def_property_enum_items(prop, modifier_lineart_shadow_region_filtering);
  RNA_def_property_ui_text(prop,
                           "Shadow Region Filtering",
                           "Select feature lines that comes from lit or shaded regions. Will not "
                           "affect cast shadow and light contour since they are at the border");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "silhouette_filtering", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "silhouette_selection");
  RNA_def_property_enum_items(prop, modifier_lineart_silhouette_filtering);
  RNA_def_property_ui_text(prop, "Silhouette Filtering", "Select contour or silhouette");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_multiple_levels", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_multiple_levels", 0);
  RNA_def_property_ui_text(
      prop, "Use Occlusion Range", "Generate strokes from a range of occlusion levels");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "level_start", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Level Start", "Minimum number of occlusions for the generated strokes");
  RNA_def_property_range(prop, 0, 128);
  RNA_def_property_int_funcs(prop, nullptr, "rna_Lineart_start_level_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "level_end", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Level End", "Maximum number of occlusions for the generated strokes");
  RNA_def_property_range(prop, 0, 128);
  RNA_def_property_int_funcs(prop, nullptr, "rna_Lineart_end_level_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "target_material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_LineartGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(
      prop, "Material", "Grease Pencil material assigned to the generated strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "target_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Layer", "Grease Pencil layer to which assign the generated strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "source_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Source Vertex Group",
      "Match the beginning of vertex group names from mesh objects, match all when left empty");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_LineartGpencilModifier_vgname_set");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for selected strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "is_baked", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_GPENCIL_IS_BAKED);
  RNA_def_property_ui_text(prop, "Is Baked", "This modifier has baked data");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_GPENCIL_USE_CACHE);
  RNA_def_property_ui_text(prop,
                           "Use Cache",
                           "Use cached scene data from the first line art modifier in the stack. "
                           "Certain settings will be unavailable");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "overscan", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Overscan",
      "A margin to prevent strokes from ending abruptly at the edge of the image");
  RNA_def_property_ui_range(prop, 0.0f, 0.5f, 0.01f, 3);
  RNA_def_property_range(prop, 0.0f, 0.5f);
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Thickness", "The thickness for the generated strokes");
  RNA_def_property_ui_range(prop, 1, 100, 1, 1);
  RNA_def_property_range(prop, 1, 200);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Opacity", "The strength value for the generate strokes");
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01f, 2);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_material_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mask_switches", LRT_GPENCIL_MATERIAL_MASK_ENABLE);
  RNA_def_property_ui_text(
      prop, "Use Material Mask", "Use material masks to filter out occluded strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_material_mask_match", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mask_switches", LRT_GPENCIL_MATERIAL_MASK_MATCH);
  RNA_def_property_ui_text(
      prop, "Match Masks", "Require matching all material masks instead of just one");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_material_mask_bits", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "material_mask_bits", 1);
  RNA_def_property_array(prop, 8);
  RNA_def_property_ui_text(prop, "Masks", "Mask bits to match from Material Line Art settings");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_intersection_match", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mask_switches", LRT_GPENCIL_INTERSECTION_MATCH);
  RNA_def_property_ui_text(
      prop, "Match Intersection", "Require matching all intersection masks instead of just one");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_intersection_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "intersection_mask", 1);
  RNA_def_property_array(prop, 8);
  RNA_def_property_ui_text(prop, "Masks", "Mask bits to match from Collection Line Art settings");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_crease_on_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "calculation_flags", LRT_USE_CREASE_ON_SMOOTH_SURFACES);
  RNA_def_property_ui_text(
      prop, "Crease On Smooth Surfaces", "Allow crease edges to show inside smooth surfaces");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_crease_on_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_USE_CREASE_ON_SHARP_EDGES);
  RNA_def_property_ui_text(prop, "Crease On Sharp Edges", "Allow crease to show on sharp edges");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_image_boundary_trimming", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "calculation_flags", LRT_USE_IMAGE_BOUNDARY_TRIMMING);
  RNA_def_property_ui_text(
      prop,
      "Image Boundary Trimming",
      "Trim all edges right at the boundary of image (including overscan region)");

  prop = RNA_def_property(srna, "use_back_face_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "calculation_flags", LRT_USE_BACK_FACE_CULLING);
  RNA_def_property_ui_text(
      prop,
      "Back Face Culling",
      "Remove all back faces to speed up calculation, this will create edges in "
      "different occlusion levels than when disabled");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "shadow_camera_near", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Shadow Camera Near", "Near clipping distance of shadow camera");
  RNA_def_property_ui_range(prop, 0.0f, 500.0f, 0.1f, 2);
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "shadow_camera_far", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Shadow Camera Far", "Far clipping distance of shadow camera");
  RNA_def_property_ui_range(prop, 0.0f, 500.0f, 0.1f, 2);
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "shadow_camera_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Shadow Camera Size",
      "Represents the \"Orthographic Scale\" of an orthographic camera. "
      "If the camera is positioned at the light's location with this scale, it will "
      "represent the coverage of the shadow \"camera\"");
  RNA_def_property_ui_range(prop, 0.0f, 500.0f, 0.1f, 2);
  RNA_def_property_range(prop, 0.0f, 10000.0f);

  prop = RNA_def_property(srna, "use_invert_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_GPENCIL_INVERT_COLLECTION);
  RNA_def_property_ui_text(prop,
                           "Invert Collection Filtering",
                           "Select everything except lines from specified collection");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_invert_silhouette", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_GPENCIL_INVERT_SILHOUETTE_FILTER);
  RNA_def_property_ui_text(prop, "Invert Silhouette Filtering", "Select anti-silhouette lines");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencillength(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LengthGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Length Modifier", "Stretch or shrink strokes");
  RNA_def_struct_sdna(srna, "LengthGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_LENGTH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "start_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "start_fac");
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 2);
  RNA_def_property_ui_text(
      prop, "Start Factor", "Added length to the start of each stroke relative to its length");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "end_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "end_fac");
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 2);
  RNA_def_property_ui_text(
      prop, "End Factor", "Added length to the end of each stroke relative to its length");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "start_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "start_fac");
  RNA_def_property_ui_range(prop, -100.0f, 100.0f, 0.1f, 3);
  RNA_def_property_ui_text(
      prop, "Start Factor", "Absolute added length to the start of each stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "end_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "end_fac");
  RNA_def_property_ui_range(prop, -100.0f, 100.0f, 0.1f, 3);
  RNA_def_property_ui_text(prop, "End Factor", "Absolute added length to the end of each stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_start_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rand_start_fac");
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 1);
  RNA_def_property_ui_text(
      prop, "Random Start Factor", "Size of random length added to the start of each stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_end_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rand_end_fac");
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 1);
  RNA_def_property_ui_text(
      prop, "Random End Factor", "Size of random length added to the end of each stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rand_offset");
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Random Noise Offset", "Smoothly offset each stroke's random value");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_USE_RANDOM);
  RNA_def_property_ui_text(prop, "Random", "Use random values over time");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Seed", "Random seed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "step");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(prop, "Step", "Number of frames between randomization steps");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "overshoot_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overshoot_fac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Used Length",
      "Defines what portion of the stroke is used for the calculation of the extension");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, gpencil_length_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode to define length");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_curvature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_USE_CURVATURE);
  RNA_def_property_ui_text(prop, "Use Curvature", "Follow the curvature of the stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_curvature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_INVERT_CURVATURE);
  RNA_def_property_ui_text(
      prop, "Invert Curvature", "Invert the curvature of the stroke's extension");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "point_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.1f, 1000.0f);
  RNA_def_property_ui_range(prop, 0.1f, 1000.0f, 1.0f, 1);
  RNA_def_property_ui_scale_type(prop, PROP_SCALE_CUBIC);
  RNA_def_property_ui_text(
      prop, "Point Density", "Multiplied by Start/End for the total added point count");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "segment_influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, -2.0f, 3.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 2);
  RNA_def_property_ui_text(prop,
                           "Segment Influence",
                           "Factor to determine how much the length of the individual segments "
                           "should influence the final computed curvature. Higher factors makes "
                           "small segments influence the overall curvature less");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "max_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_ui_text(prop,
                           "Filter Angle",
                           "Ignore points on the stroke that deviate from their neighbors by more "
                           "than this angle when determining the extrapolation shape");
  RNA_def_property_range(prop, 0.0f, DEG2RAD(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RAD(179.5f), 10.0f, 1);
  RNA_def_property_update(prop, NC_SCENE, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LENGTH_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencildash(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DashGpencilModifierSegment", nullptr);
  RNA_def_struct_ui_text(srna, "Dash Modifier Segment", "Configuration for a single dash segment");
  RNA_def_struct_sdna(srna, "DashGpencilModifierSegment");
  RNA_def_struct_path_func(srna, "rna_DashGpencilModifierSegment_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of the dash segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_DashGpencilModifierSegment_name_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, nullptr);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "dash", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, INT16_MAX);
  RNA_def_property_ui_text(
      prop,
      "Dash",
      "The number of consecutive points from the original stroke to include in this segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "gap", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, INT16_MAX);
  RNA_def_property_ui_text(prop, "Gap", "The number of points skipped after this segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_FACTOR | PROP_UNSIGNED);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
  RNA_def_property_ui_text(
      prop, "Radius", "The factor to apply to the original point's radius for the new points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
  RNA_def_property_ui_text(
      prop, "Opacity", "The factor to apply to the original point's opacity for the new points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mat_nr");
  RNA_def_property_range(prop, -1, INT16_MAX);
  RNA_def_property_ui_text(
      prop,
      "Material Index",
      "Use this index on generated segment. -1 means using the existing material");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DASH_USE_CYCLIC);
  RNA_def_property_ui_text(prop, "Cyclic", "Enable cyclic on individual stroke dashes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  srna = RNA_def_struct(brna, "DashGpencilModifierData", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Dash Modifier", "Create dot-dash effect for strokes");
  RNA_def_struct_sdna(srna, "DashGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_DASH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "segments", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "DashGpencilModifierSegment");
  RNA_def_property_collection_sdna(prop, nullptr, "segments", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    "rna_GpencilDash_segments_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Segments", "");

  prop = RNA_def_property(srna, "segment_active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Active Dash Segment Index", "Active index in the segment list");

  prop = RNA_def_property(srna, "dash_offset", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Offset",
      "Offset into each stroke before the beginning of the dashed segment generation");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Common properties. */

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DASH_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DASH_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DASH_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_DASH_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilshrinkwrap(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ShrinkwrapGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna,
                         "Shrinkwrap Modifier",
                         "Shrink wrapping modifier to shrink wrap and object to a target");
  RNA_def_struct_sdna(srna, "ShrinkwrapGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SHRINKWRAP);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "wrap_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shrink_type");
  RNA_def_property_enum_items(prop, rna_enum_shrinkwrap_type_items);
  RNA_def_property_ui_text(prop, "Wrap Method", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "wrap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shrink_mode");
  RNA_def_property_enum_items(prop, rna_enum_modifier_shrinkwrap_mode_items);
  RNA_def_property_ui_text(
      prop, "Snap Mode", "Select how vertices are constrained to the target surface");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "cull_face", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shrink_opts");
  RNA_def_property_enum_items(prop, rna_enum_shrinkwrap_face_cull_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ShrinkwrapGpencilModifier_face_cull_get",
                              "rna_ShrinkwrapGpencilModifier_face_cull_set",
                              nullptr);
  RNA_def_property_ui_text(
      prop,
      "Face Cull",
      "Stop vertices from projecting to a face on the target when facing towards/away");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Target", "Mesh target to shrink to");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_ShrinkwrapGpencilModifier_target_set", nullptr, "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "auxiliary_target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "aux_target");
  RNA_def_property_ui_text(prop, "Auxiliary Target", "Additional mesh target to shrink to");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_ShrinkwrapGpencilModifier_aux_target_set",
                                 nullptr,
                                 "rna_Mesh_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "keep_dist");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100, 100, 1, 2);
  RNA_def_property_ui_text(prop, "Offset", "Distance to keep from the target");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "project_limit", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "proj_limit");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 1, 2);
  RNA_def_property_ui_text(
      prop, "Project Limit", "Limit the distance used for projection (zero disables)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_project_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proj_axis", MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS);
  RNA_def_property_ui_text(prop, "X", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_project_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proj_axis", MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS);
  RNA_def_property_ui_text(prop, "Y", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_project_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "proj_axis", MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS);
  RNA_def_property_ui_text(prop, "Z", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "subsurf_levels", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "subsurf_levels");
  RNA_def_property_range(prop, 0, 6);
  RNA_def_property_ui_range(prop, 0, 6, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Subdivision Levels",
      "Number of subdivisions that must be performed before extracting vertices' "
      "positions and normals");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_negative_direction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "shrink_opts", MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR);
  RNA_def_property_ui_text(
      prop, "Negative", "Allow vertices to move in the negative direction of axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_positive_direction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "shrink_opts", MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR);
  RNA_def_property_ui_text(
      prop, "Positive", "Allow vertices to move in the positive direction of axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_invert_cull", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "shrink_opts", MOD_SHRINKWRAP_INVERT_CULL_TARGET);
  RNA_def_property_ui_text(
      prop, "Invert Cull", "When projecting in the negative direction invert the face cull mode");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_ShrinkwrapGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(
      prop, nullptr, nullptr, "rna_ShrinkwrapGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SHRINKWRAP_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SHRINKWRAP_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SHRINKWRAP_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MOD_SHRINKWRAP_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SHRINKWRAP_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "smooth_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "smooth_factor");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Smooth Factor", "Amount of smoothing to apply");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "smooth_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "smooth_step");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(
      prop, "Steps", "Number of times to apply smooth (high numbers can reduce FPS)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_modifier_gpencilenvelope(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "EnvelopeGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Envelope Modifier", "Envelope stroke effect modifier");
  RNA_def_struct_sdna(srna, "EnvelopeGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_ENVELOPE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_EnvelopeGpencilModifier_material_set",
                                 nullptr,
                                 "rna_GpencilModifier_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_EnvelopeGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "spread", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "spread");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_text(
      prop, "Spread Length", "The number of points to skip to create straight segments");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, gpencil_envelope_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Algorithm to use for generating the envelope");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "mat_nr", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mat_nr");
  RNA_def_property_range(prop, -1, INT16_MAX);
  RNA_def_property_ui_text(prop, "Material Index", "The material to use for the new strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "thickness");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Thickness", "Multiplier for the thickness of the new strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "strength");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Strength", "Multiplier for the strength of the new strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "skip", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "skip");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(
      prop, "Skip Segments", "The number of generated segments to skip to reduce complexity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ENVELOPE_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ENVELOPE_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ENVELOPE_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ENVELOPE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_ENVELOPE_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  RNA_define_lib_overridable(false);
}

void RNA_def_greasepencil_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* data */
  srna = RNA_def_struct(brna, "GpencilModifier", nullptr);
  RNA_def_struct_ui_text(srna, "GpencilModifier", "Modifier affecting the Grease Pencil object");
  RNA_def_struct_refine_func(srna, "rna_GpencilModifier_refine");
  RNA_def_struct_path_func(srna, "rna_GpencilModifier_path");
  RNA_def_struct_sdna(srna, "GpencilModifierData");

  /* strings */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_GpencilModifier_name_set");
  RNA_def_property_ui_text(prop, "Name", "Modifier name");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, nullptr);
  RNA_def_struct_name_property(srna, prop);

  /* enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, rna_enum_object_greasepencil_modifier_type_items);
  RNA_def_property_ui_text(prop, "Type", "");

  /* flags */
  prop = RNA_def_property(srna, "show_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", eGpencilModifierMode_Realtime);
  RNA_def_property_ui_text(prop, "Realtime", "Display modifier in viewport");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_ON, 1);

  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", eGpencilModifierMode_Render);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Render", "Use modifier during render");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_ON, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "show_in_editmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mode", eGpencilModifierMode_Editmode);
  RNA_def_property_ui_text(prop, "Edit Mode", "Display modifier in Edit mode");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ui_expand_flag", 0);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface");
  RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);

  prop = RNA_def_boolean(srna,
                         "is_override_data",
                         false,
                         "Override Modifier",
                         "In a local override object, whether this modifier comes from the linked "
                         "reference object, or is local to the override");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "flag", eGpencilModifierFlag_OverrideLibrary_Local);

  /* types */
  rna_def_modifier_gpencilnoise(brna);
  rna_def_modifier_gpencilsmooth(brna);
  rna_def_modifier_gpencilsubdiv(brna);
  rna_def_modifier_gpencilsimplify(brna);
  rna_def_modifier_gpencilthick(brna);
  rna_def_modifier_gpenciloffset(brna);
  rna_def_modifier_gpenciltint(brna);
  rna_def_modifier_gpenciltime(brna);
  rna_def_modifier_gpencilcolor(brna);
  rna_def_modifier_gpencilarray(brna);
  rna_def_modifier_gpencilbuild(brna);
  rna_def_modifier_gpencilopacity(brna);
  rna_def_modifier_gpenciloutline(brna);
  rna_def_modifier_gpencillattice(brna);
  rna_def_modifier_gpencilmirror(brna);
  rna_def_modifier_gpencilhook(brna);
  rna_def_modifier_gpencilarmature(brna);
  rna_def_modifier_gpencilmultiply(brna);
  rna_def_modifier_gpenciltexture(brna);
  rna_def_modifier_gpencilweight_angle(brna);
  rna_def_modifier_gpencilweight_proximity(brna);
  rna_def_modifier_gpencillineart(brna);
  rna_def_modifier_gpencillength(brna);
  rna_def_modifier_gpencildash(brna);
  rna_def_modifier_gpencilshrinkwrap(brna);
  rna_def_modifier_gpencilenvelope(brna);
}

#endif
