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
 */

/** \file
 * \ingroup RNA
 */

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_data_transfer.h"
#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_fluid.h" /* For BKE_fluid_modifier_free & BKE_fluid_modifier_create_type_data */
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_multires.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_object_greasepencil_modifier_type_items[] = {
    {0, "", 0, N_("Generate"), ""},
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
    {0, "", 0, N_("Deform"), ""},
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
    {eGpencilModifierType_Smooth, "GP_SMOOTH", ICON_MOD_SMOOTH, "Smooth", "Smooth stroke"},
    {eGpencilModifierType_Thick,
     "GP_THICK",
     ICON_MOD_THICKNESS,
     "Thickness",
     "Change stroke thickness"},
    {eGpencilModifierType_Time, "GP_TIME", ICON_MOD_TIME, "Time Offset", "Offset keyframes"},
    {0, "", 0, N_("Color"), ""},
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
    {eGpencilModifierType_Texture,
     "GP_TEXTURE",
     ICON_TEXTURE,
     "Texture Mapping",
     "Change stroke uv texture values"},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem modifier_modify_color_items[] = {
    {GP_MODIFY_COLOR_BOTH, "BOTH", 0, "Stroke and Fill", "Modify fill and stroke colors"},
    {GP_MODIFY_COLOR_STROKE, "STROKE", 0, "Stroke", "Modify stroke color only"},
    {GP_MODIFY_COLOR_FILL, "FILL", 0, "Fill", "Modify fill color only"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem modifier_modify_opacity_items[] = {
    {GP_MODIFY_COLOR_BOTH, "BOTH", 0, "Stroke and Fill", "Modify fill and stroke colors"},
    {GP_MODIFY_COLOR_STROKE, "STROKE", 0, "Stroke", "Modify stroke color only"},
    {GP_MODIFY_COLOR_FILL, "FILL", 0, "Fill", "Modify fill color only"},
    {GP_MODIFY_COLOR_HARDNESS, "HARDNESS", 0, "Hardness", "Modify stroke hardness"},
    {0, NULL, 0, NULL, NULL},
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
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_time_mode_items[] = {
    {GP_TIME_MODE_NORMAL, "NORMAL", 0, "Regular", "Apply offset in usual animation direction"},
    {GP_TIME_MODE_REVERSE, "REVERSE", 0, "Reverse", "Apply offset in reverse animation direction"},
    {GP_TIME_MODE_FIX, "FIX", 0, "Fixed Frame", "Keep frame and do not change with time"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem gpencil_subdivision_type_items[] = {
    {GP_SUBDIV_CATMULL, "CATMULL_CLARK", 0, "Catmull-Clark", ""},
    {GP_SUBDIV_SIMPLE, "SIMPLE", 0, "Simple", ""},
    {0, NULL, 0, NULL, NULL},
};
static const EnumPropertyItem gpencil_tint_type_items[] = {
    {GP_TINT_UNIFORM, "UNIFORM", 0, "Uniform", ""},
    {GP_TINT_GRADIENT, "GRADIENT", 0, "Gradient", ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

#ifdef RNA_RUNTIME

#  include "DNA_curve_types.h"
#  include "DNA_fluid_types.h"
#  include "DNA_particle_types.h"

#  include "BKE_cachefile.h"
#  include "BKE_context.h"
#  include "BKE_gpencil.h"
#  include "BKE_gpencil_modifier.h"
#  include "BKE_object.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

static StructRNA *rna_GpencilModifier_refine(struct PointerRNA *ptr)
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
    case eGpencilModifierType_Color:
      return &RNA_ColorGpencilModifier;
    case eGpencilModifierType_Array:
      return &RNA_ArrayGpencilModifier;
    case eGpencilModifierType_Build:
      return &RNA_BuildGpencilModifier;
    case eGpencilModifierType_Opacity:
      return &RNA_OpacityGpencilModifier;
    case eGpencilModifierType_Lattice:
      return &RNA_LatticeGpencilModifier;
    case eGpencilModifierType_Mirror:
      return &RNA_MirrorGpencilModifier;
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
      /* Default */
    case eGpencilModifierType_None:
    case NUM_GREASEPENCIL_MODIFIER_TYPES:
      return &RNA_GpencilModifier;
  }

  return &RNA_GpencilModifier;
}

static void rna_GpencilModifier_name_set(PointerRNA *ptr, const char *value)
{
  GpencilModifierData *gmd = ptr->data;
  char oldname[sizeof(gmd->name)];

  /* make a copy of the old name first */
  BLI_strncpy(oldname, gmd->name, sizeof(gmd->name));

  /* copy the new name into the name slot */
  BLI_strncpy_utf8(gmd->name, value, sizeof(gmd->name));

  /* make sure the name is truly unique */
  if (ptr->owner_id) {
    Object *ob = (Object *)ptr->owner_id;
    BKE_gpencil_modifier_unique_name(&ob->greasepencil_modifiers, gmd);
  }

  /* fix all the animation data which may link to this */
  BKE_animdata_fix_paths_rename_all(NULL, "grease_pencil_modifiers", oldname, gmd->name);
}

static char *rna_GpencilModifier_path(PointerRNA *ptr)
{
  GpencilModifierData *gmd = ptr->data;
  char name_esc[sizeof(gmd->name) * 2];

  BLI_strescape(name_esc, gmd->name, sizeof(name_esc));
  return BLI_sprintfN("grease_pencil_modifiers[\"%s\"]", name_esc);
}

static void rna_GpencilModifier_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
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

#  undef RNA_GP_MOD_VGROUP_NAME_SET

/* Objects */

static void greasepencil_modifier_object_set(Object *self,
                                             Object **ob_p,
                                             int type,
                                             PointerRNA value)
{
  Object *ob = value.data;

  if (!self || ob != self) {
    if (!ob || type == OB_EMPTY || ob->type == type) {
      id_lib_extern((ID *)ob);
      *ob_p = ob;
    }
  }
}

#  define RNA_GP_MOD_OBJECT_SET(_type, _prop, _obtype) \
    static void rna_##_type##GpencilModifier_##_prop##_set( \
        PointerRNA *ptr, PointerRNA value, struct ReportList *UNUSED(reports)) \
    { \
      _type##GpencilModifierData *tmd = (_type##GpencilModifierData *)ptr->data; \
      greasepencil_modifier_object_set((Object *)ptr->owner_id, &tmd->_prop, _obtype, value); \
    }

RNA_GP_MOD_OBJECT_SET(Armature, object, OB_ARMATURE);
RNA_GP_MOD_OBJECT_SET(Lattice, object, OB_LATTICE);
RNA_GP_MOD_OBJECT_SET(Mirror, object, OB_EMPTY);

#  undef RNA_GP_MOD_OBJECT_SET

static void rna_HookGpencilModifier_object_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               struct ReportList *UNUSED(reports))
{
  HookGpencilModifierData *hmd = ptr->data;
  Object *ob = (Object *)value.data;

  hmd->object = ob;
  id_lib_extern((ID *)ob);
  BKE_object_modifier_gpencil_hook_reset(ob, hmd);
}

static void rna_TintGpencilModifier_object_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               struct ReportList *UNUSED(reports))
{
  TintGpencilModifierData *hmd = ptr->data;
  Object *ob = (Object *)value.data;

  hmd->object = ob;
  id_lib_extern((ID *)ob);
}

static void rna_TimeModifier_start_frame_set(PointerRNA *ptr, int value)
{
  TimeGpencilModifierData *tmd = ptr->data;
  CLAMP(value, MINFRAME, MAXFRAME);
  tmd->sfra = value;

  if (tmd->sfra >= tmd->efra) {
    tmd->efra = MIN2(tmd->sfra, MAXFRAME);
  }
}

static void rna_TimeModifier_end_frame_set(PointerRNA *ptr, int value)
{
  TimeGpencilModifierData *tmd = ptr->data;
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

#else

static void rna_def_modifier_gpencilnoise(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NoiseGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Noise Modifier", "Noise effect modifier");
  RNA_def_struct_sdna(srna, "NoiseGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_NOISE);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NoiseGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(prop, "Offset Factor", "Amount of noise to apply");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor_strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "factor_strength");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(prop, "Strength Factor", "Amount of noise to apply to opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor_thickness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "factor_thickness");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(prop, "Thickness Factor", "Amount of noise to apply to thickness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor_uvs", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "factor_uvs");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(prop, "UV Factor", "Amount of noise to apply uv rotation");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_USE_RANDOM);
  RNA_def_property_ui_text(prop, "Random", "Use random values over time");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Seed", "Random seed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "noise_scale", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "noise_scale");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Noise Scale", "Scale the noise frequency");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define noise effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "step");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(
      prop, "Step", "Number of frames before recalculate random values again");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilsmooth(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SmoothGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Smooth Modifier", "Smooth effect modifier");
  RNA_def_struct_sdna(srna, "SmoothGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SMOOTH);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SmoothGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Factor", "Amount of smooth to apply");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_position", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_LOCATION);
  RNA_def_property_ui_text(
      prop, "Affect Position", "The modifier affects the position of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_STRENGTH);
  RNA_def_property_ui_text(
      prop, "Affect Strength", "The modifier affects the color strength of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_thickness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_THICKNESS);
  RNA_def_property_ui_text(
      prop, "Affect Thickness", "The modifier affects the thickness of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_edit_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_UV);
  RNA_def_property_ui_text(
      prop, "Affect UV", "The modifier affects the UV rotation factor of the point");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "step");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(
      prop, "Step", "Number of times to apply smooth (high numbers can reduce fps)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define smooth effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilsubdiv(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SubdivGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Subdivision Modifier", "Subdivide Stroke modifier");
  RNA_def_struct_sdna(srna, "SubdivGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SUBSURF);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "level", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "level");
  RNA_def_property_range(prop, 0, 5);
  RNA_def_property_ui_text(prop, "Level", "Number of subdivisions");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "subdivision_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, gpencil_subdivision_type_items);
  RNA_def_property_ui_text(prop, "Subdivision Type", "Select type of subdivision algorithm");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SUBDIV_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SUBDIV_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SUBDIV_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SUBDIV_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SimplifyGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Simplify Modifier", "Simplify Stroke modifier");
  RNA_def_struct_sdna(srna, "SimplifyGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_SIMPLIFY);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_range(prop, 0, 100.0);
  RNA_def_property_ui_range(prop, 0, 100.0, 1.0f, 3);
  RNA_def_property_ui_text(prop, "Factor", "Factor of Simplify");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SIMPLIFY_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SIMPLIFY_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SIMPLIFY_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SIMPLIFY_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gpencil_simplify_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "How to simplify the stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "step");
  RNA_def_property_range(prop, 1, 50);
  RNA_def_property_ui_text(prop, "Iterations", "Number of times to apply simplify");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Sample */
  prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "length");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1.0, 0.01, 3);
  RNA_def_property_ui_text(prop, "Length", "Length of each segment");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Merge */
  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "distance");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1.0, 0.01, 3);
  RNA_def_property_ui_text(prop, "Distance", "Distance between points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilthick(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThickGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Thick Modifier", "Subdivide and Smooth Stroke modifier");
  RNA_def_struct_sdna(srna, "ThickGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_THICKNESS);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ThickGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "thickness");
  RNA_def_property_range(prop, -100, 500);
  RNA_def_property_ui_text(prop, "Thickness", "Absolute thickness to apply everywhere");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "thickness_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "thickness_fac");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 10.0, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Thickness Factor", "Factor to multiply the thickness with");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define thickness change along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "normalize_thickness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_NORMALIZE);
  RNA_def_property_ui_text(prop, "Uniform Thickness", "Replace the stroke thickness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_thickness");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpenciloffset(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "OffsetGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Offset Modifier", "Offset Stroke modifier");
  RNA_def_struct_sdna(srna, "OffsetGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_OFFSET);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_OffsetGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "loc");
  RNA_def_property_ui_text(prop, "Location", "Values for change location");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, NULL, "rot");
  RNA_def_property_ui_text(prop, "Rotation", "Values for changes in rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "scale");
  RNA_def_property_ui_text(prop, "Scale", "Values for changes in scale");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpenciltint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* modes */
  static EnumPropertyItem tint_mode_types_items[] = {
      {GPPAINT_MODE_STROKE, "STROKE", 0, "Stroke", "Vertex Color affects to Stroke only"},
      {GPPAINT_MODE_FILL, "FILL", 0, "Fill", "Vertex Color affects to Fill only"},
      {GPPAINT_MODE_BOTH, "BOTH", 0, "Stroke and Fill", "Vertex Color affects to Stroke and Fill"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "TintGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Tint Modifier", "Tint modifier");
  RNA_def_struct_sdna(srna, "TintGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_COLOR);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Parent object to define the center of the effect");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_TintGpencilModifier_object_set", NULL, NULL);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_HookGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse Vertex Group", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_range(prop, 0, 2.0);
  RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 2);
  RNA_def_property_ui_text(prop, "Strength", "Factor for tinting");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "radius");
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Radius", "Defines the maximum distance of the effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Mode type. */
  prop = RNA_def_property(srna, "vertex_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, tint_mode_types_items);
  RNA_def_property_ui_text(prop, "Mode", "Defines how vertex color affect to the strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Type of Tint. */
  prop = RNA_def_property(srna, "tint_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, gpencil_tint_type_items);
  RNA_def_property_ui_text(prop, "Tint Type", "Select type of tinting algorithm");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Simple Color. */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, NULL, "rgb");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Color used for tinting");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Color band. */
  prop = RNA_def_property(srna, "colors", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "colorband");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Colors", "Color ramp used to define tinting colors");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define vertex color effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpenciltime(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TimeGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Time Offset Modifier", "Time offset modifier");
  RNA_def_struct_sdna(srna, "TimeGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TIME);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_time_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TIME_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TIME_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "offset");
  RNA_def_property_range(prop, SHRT_MIN, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Frame Offset", "Number of frames to offset original keyframe number or frame to fix");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "frame_scale");
  RNA_def_property_range(prop, 0.001f, 100.0f);
  RNA_def_property_ui_text(prop, "Frame Scale", "Evaluation time in seconds");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "sfra");
  RNA_def_property_int_funcs(prop, NULL, "rna_TimeModifier_start_frame_set", NULL);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_text(prop, "Start Frame", "First frame of the range");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "efra");
  RNA_def_property_int_funcs(prop, NULL, "rna_TimeModifier_end_frame_set", NULL);
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_int_default(prop, 250);
  RNA_def_property_ui_text(prop, "End Frame", "Final frame of the range");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_keep_loop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TIME_KEEP_LOOP);
  RNA_def_property_ui_text(
      prop, "Keep Loop", "Retiming end frames and move to start of animation to keep loop");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TIME_CUSTOM_RANGE);
  RNA_def_property_ui_text(
      prop, "Custom Range", "Define a custom range of frames to use in modifier");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilcolor(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ColorGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Hue/Saturation Modifier", "Change Hue/Saturation modifier");
  RNA_def_struct_sdna(srna, "ColorGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TINT);

  prop = RNA_def_property(srna, "modify_color", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_modify_color_items); /* share the enum */
  RNA_def_property_ui_text(prop, "Mode", "Set what colors of the stroke are affected");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "hue", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 3);
  RNA_def_property_float_default(prop, 0.5);
  RNA_def_property_float_sdna(prop, NULL, "hsv[0]");
  RNA_def_property_ui_text(prop, "Hue", "Color Hue");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0);
  RNA_def_property_float_sdna(prop, NULL, "hsv[1]");
  RNA_def_property_ui_text(prop, "Saturation", "Color Saturation");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0);
  RNA_def_property_float_sdna(prop, NULL, "hsv[2]");
  RNA_def_property_ui_text(prop, "Value", "Color Value");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define color effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilopacity(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "OpacityGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Opacity Modifier", "Opacity of Strokes modifier");
  RNA_def_struct_sdna(srna, "OpacityGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_OPACITY);

  prop = RNA_def_property(srna, "modify_color", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_modify_opacity_items);
  RNA_def_property_ui_text(prop, "Mode", "Set what colors of the stroke are affected");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_OpacityGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "factor");
  RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 2);
  RNA_def_property_float_funcs(
      prop, NULL, "rna_GpencilOpacity_max_set", "rna_GpencilOpacity_range");
  RNA_def_property_ui_text(prop, "Opacity Factor", "Factor of Opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "hardness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "hardeness");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 0.1, 2);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Hardness", "Factor of stroke hardness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "normalize_opacity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_NORMALIZE);
  RNA_def_property_ui_text(prop, "Uniform Opacity", "Replace the stroke opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_opacity_update");

  prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_CUSTOM_CURVE);
  RNA_def_property_ui_text(
      prop, "Custom Curve", "Use a custom curve to define opacity effect along the strokes");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve_intensity");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to apply effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilarray(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ArrayGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Instance Modifier", "Create grid of duplicate instances");
  RNA_def_struct_sdna(srna, "ArrayGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_ARRAY);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
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
  RNA_def_property_pointer_sdna(prop, NULL, "object");
  RNA_def_property_ui_text(
      prop,
      "Object Offset",
      "Use the location and rotation of another object to determine the distance and "
      "rotational change between arrayed items");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "constant_offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_ui_text(prop, "Constant Offset", "Value for the distance between items");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "relative_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "shift");
  RNA_def_property_ui_text(
      prop,
      "Relative Offset",
      "The size of the geometry will determine the distance between arrayed items");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "rnd_offset");
  RNA_def_property_ui_text(prop, "Random Offset", "Value for changes in location");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, NULL, "rnd_rot");
  RNA_def_property_ui_text(prop, "Random Rotation", "Value for changes in rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "random_scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "rnd_scale");
  RNA_def_property_ui_text(prop, "Scale", "Value for changes in scale");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Seed", "Random seed");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "replace_material", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "mat_rpl");
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Material",
      "Index of the material used for generated strokes (0 keep original material)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_constant_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_USE_OFFSET);
  RNA_def_property_ui_text(prop, "Offset", "Enable offset");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_object_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_USE_OB_OFFSET);
  RNA_def_property_ui_text(prop, "Object Offset", "Enable object offset");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_relative_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_ARRAY_USE_RELATIVE);
  RNA_def_property_ui_text(prop, "Shift", "Enable shift");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilbuild(BlenderRNA *brna)
{
  static EnumPropertyItem prop_gpencil_build_mode_items[] = {
      {GP_BUILD_MODE_SEQUENTIAL,
       "SEQUENTIAL",
       ICON_PARTICLE_POINT,
       "Sequential",
       "Strokes appear/disappear one after the other, but only a single one changes at a time"},
      {GP_BUILD_MODE_CONCURRENT,
       "CONCURRENT",
       ICON_PARTICLE_TIP,
       "Concurrent",
       "Multiple strokes appear/disappear at once"},
      {0, NULL, 0, NULL, NULL},
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
      {GP_BUILD_TRANSITION_FADE,
       "FADE",
       0,
       "Fade",
       "Hide points in the order they occur in each stroke "
       "(e.g. for animating ink fading or vanishing after getting drawn)"},
      {0, NULL, 0, NULL, NULL},
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
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BuildGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Build Modifier", "Animate strokes appearing and disappearing");
  RNA_def_struct_sdna(srna, "BuildGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_BUILD);

  /* Mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gpencil_build_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "How many strokes are being animated at a time");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Direction */
  prop = RNA_def_property(srna, "transition", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_gpencil_build_transition_items);
  RNA_def_property_ui_text(
      prop, "Transition", "How are strokes animated (i.e. are they appearing or disappearing)");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Transition Onset Delay + Length */
  prop = RNA_def_property(srna, "start_delay", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "start_delay");
  RNA_def_property_ui_text(
      prop,
      "Start Delay",
      "Number of frames after each GP keyframe before the modifier has any effect");
  RNA_def_property_range(prop, 0, MAXFRAMEF);
  RNA_def_property_ui_range(prop, 0, 200, 1, -1);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "length");
  RNA_def_property_ui_text(prop,
                           "Length",
                           "Maximum number of frames that the build effect can run for "
                           "(unless another GP keyframe occurs before this time has elapsed)");
  RNA_def_property_range(prop, 1, MAXFRAMEF);
  RNA_def_property_ui_range(prop, 1, 1000, 1, -1);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Concurrent Mode Settings */
  prop = RNA_def_property(srna, "concurrent_time_alignment", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "time_alignment");
  RNA_def_property_enum_items(prop, prop_gpencil_build_time_align_items);
  RNA_def_property_ui_text(
      prop, "Time Alignment", "When should strokes start to appear/disappear");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Time Limits */
  prop = RNA_def_property(srna, "use_restrict_frame_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_BUILD_RESTRICT_TIME);
  RNA_def_property_ui_text(
      prop, "Restrict Frame Range", "Only modify strokes during the specified frame range");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Use percentage */
  prop = RNA_def_property(srna, "use_percentage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_BUILD_PERCENTAGE);
  RNA_def_property_ui_text(
      prop, "Restrict Visible Points", "Use a percentage factor to determine the visible points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Percentage factor. */
  prop = RNA_def_property(srna, "percentage_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "percentage_fac");
  RNA_def_property_ui_text(prop, "Factor", "Defines how much of the stroke is visible");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "start_frame");
  RNA_def_property_ui_text(
      prop, "Start Frame", "Start Frame (when Restrict Frame Range is enabled)");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "end_frame");
  RNA_def_property_ui_text(prop, "End Frame", "End Frame (when Restrict Frame Range is enabled)");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  /* Filters - Layer */
  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_BUILD_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_BUILD_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
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

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_LatticeGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Lattice object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_LatticeGpencilModifier_object_set", NULL, "rna_Lattice_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 1, 10, 2);
  RNA_def_property_ui_text(prop, "Strength", "Strength of modifier effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilmirror(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MirrorGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Mirror Modifier", "Change stroke using lattice to deform modifier");
  RNA_def_struct_sdna(srna, "MirrorGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MIRROR);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Object used as center");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_MirrorGpencilModifier_object_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_CLIPPING);
  RNA_def_property_ui_text(prop, "Clip", "Clip points");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "x_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_AXIS_X);
  RNA_def_property_ui_text(prop, "X", "Mirror this axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "y_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_AXIS_Y);
  RNA_def_property_ui_text(prop, "Y", "Mirror this axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "z_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_AXIS_Z);
  RNA_def_property_ui_text(prop, "Z", "Mirror this axis");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
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

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Object", "Parent Object for hook, also recalculates and clears offset");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_HookGpencilModifier_object_set", NULL, NULL);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "subtarget");
  RNA_def_property_ui_text(
      prop,
      "Sub-Target",
      "Name of Parent Bone for hook (if applicable), also recalculates and clears offset");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_HookGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse Vertex Group", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "force");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Strength", "Relative force of the hook");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, modifier_gphook_falloff_items); /* share the enum */
  RNA_def_property_ui_text(prop, "Falloff Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "falloff");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 100, 100, 2);
  RNA_def_property_ui_text(
      prop, "Radius", "If not zero, the distance from the hook where influence ends");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
  RNA_def_property_ui_text(prop, "Falloff Curve", "Custom light falloff curve");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "cent");
  RNA_def_property_ui_text(prop, "Hook Center", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "parentinv");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Matrix", "Reverse the transformation between this object and its target");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_falloff_uniform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_UNIFORM_SPACE);
  RNA_def_property_ui_text(prop, "Uniform Falloff", "Compensate for non-uniform object scale");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
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

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Object", "Armature object to deform with");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_ArmatureGpencilModifier_object_set", NULL, "rna_Armature_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_bone_envelopes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_ENVELOPE);
  RNA_def_property_ui_text(prop, "Use Bone Envelopes", "Bind Bone envelopes to armature modifier");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_vertex_groups", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_VGROUP);
  RNA_def_property_ui_text(prop, "Use Vertex Groups", "Bind vertex groups to armature modifier");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "use_deform_preserve_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_QUATERNION);
  RNA_def_property_ui_text(
      prop, "Preserve Volume", "Deform rotation interpolation with quaternions");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(
      prop,
      "Vertex Group",
      "Name of Vertex Group which determines influence of modifier per point");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ArmatureGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "invert_vertex_group", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "deformflag", ARM_DEF_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Invert", "Invert vertex group influence");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");
}

static void rna_def_modifier_gpencilmultiply(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MultiplyGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(srna, "Multiply Modifier", "Generate multiple strokes from one stroke");
  RNA_def_struct_sdna(srna, "MultiplyGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_GP_MULTIFRAME_EDITING);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "enable_angle_splitting", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", GP_MULTIPLY_ENABLE_ANGLE_SPLITTING);
  RNA_def_property_ui_text(prop, "Angle Splitting", "Enable angle splitting");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "use_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", GP_MULTIPLY_ENABLE_FADING);
  RNA_def_property_ui_text(prop, "Fade", "Fade the stroke thickness for each generated stroke");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "split_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0, M_PI);
  RNA_def_property_ui_range(prop, 0, M_PI, 10, 2);
  RNA_def_property_ui_text(prop, "Angle", "Split angle for segments");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "duplicates", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "duplications");
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
  RNA_def_property_float_default(prop, 0.5);
  RNA_def_property_ui_text(prop, "Thickness", "Fade influence of stroke's thickness");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fading_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_float_default(prop, 0.5);
  RNA_def_property_ui_text(prop, "Opacity", "Fade influence of stroke's opacity");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fading_center", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_float_default(prop, 0.5);
  RNA_def_property_ui_text(prop, "Center", "Fade center");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
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
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem mode_items[] = {
      {STROKE, "STROKE", 0, "Stroke", "Manipulate only stroke texture coordinates"},
      {FILL, "FILL", 0, "Fill", "Manipulate only fill texture coordinates"},
      {STROKE_AND_FILL,
       "STROKE_AND_FILL",
       0,
       "Stroke and Fill",
       "Manipulate both stroke and fill texture coordinates"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "TextureGpencilModifier", "GpencilModifier");
  RNA_def_struct_ui_text(
      srna, "Texture Modifier", "Transform stroke texture coordinates Modifier");
  RNA_def_struct_sdna(srna, "TextureGpencilModifierData");
  RNA_def_struct_ui_icon(srna, ICON_TEXTURE);

  prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "layername");
  RNA_def_property_ui_text(prop, "Layer", "Layer name");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TEX_INVERT_LAYER);
  RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Material", "Material used for filtering effect");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TEX_INVERT_MATERIAL);
  RNA_def_property_ui_text(prop, "Inverse Materials", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "vgname");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_TextureGpencilModifier_vgname_set");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TEX_INVERT_VGROUP);
  RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "pass_index");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_material_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TEX_INVERT_PASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "layer_pass", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "layer_pass");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Pass", "Layer pass index");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "invert_layer_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TEX_INVERT_LAYERPASS);
  RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "uv_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "uv_offset");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -100.0, 100.0, 0.1, 3);
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_ui_text(prop, "UV Offset", "Offset value to add to stroke UVs");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "uv_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "uv_scale");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "UV Scale", "Factor to scale the UVs");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fill_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "fill_rotation");
  RNA_def_property_ui_text(prop, "Fill Rotation", "Additional rotation of the fill UV");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fill_offset", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "fill_offset");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Fill Offset", "Additional offset of the fill UV");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fill_scale", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "fill_scale");
  RNA_def_property_range(prop, 0.01f, 100.0f);
  RNA_def_property_ui_text(prop, "Fill Scale", "Additional scale of the fill UV");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

  prop = RNA_def_property(srna, "fit_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "fit_method");
  RNA_def_property_enum_items(prop, fit_type_items);
  RNA_def_property_ui_text(prop, "Fit Method", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");
}

void RNA_def_greasepencil_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* data */
  srna = RNA_def_struct(brna, "GpencilModifier", NULL);
  RNA_def_struct_ui_text(srna, "GpencilModifier", "Modifier affecting the grease pencil object");
  RNA_def_struct_refine_func(srna, "rna_GpencilModifier_refine");
  RNA_def_struct_path_func(srna, "rna_GpencilModifier_path");
  RNA_def_struct_sdna(srna, "GpencilModifierData");

  /* strings */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GpencilModifier_name_set");
  RNA_def_property_ui_text(prop, "Name", "Modifier name");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, NULL);
  RNA_def_struct_name_property(srna, prop);

  /* enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_object_greasepencil_modifier_type_items);
  RNA_def_property_ui_text(prop, "Type", "");

  /* flags */
  prop = RNA_def_property(srna, "show_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eGpencilModifierMode_Realtime);
  RNA_def_property_ui_text(prop, "Realtime", "Display modifier in viewport");
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_ON, 1);

  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eGpencilModifierMode_Render);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Render", "Use modifier during render");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_ON, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

  prop = RNA_def_property(srna, "show_in_editmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mode", eGpencilModifierMode_Editmode);
  RNA_def_property_ui_text(prop, "Edit Mode", "Display modifier in Edit mode");
  RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
  RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, NULL, "ui_expand_flag", 0);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface");
  RNA_def_property_ui_icon(prop, ICON_DISCLOSURE_TRI_RIGHT, 1);

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
  rna_def_modifier_gpencillattice(brna);
  rna_def_modifier_gpencilmirror(brna);
  rna_def_modifier_gpencilhook(brna);
  rna_def_modifier_gpencilarmature(brna);
  rna_def_modifier_gpencilmultiply(brna);
  rna_def_modifier_gpenciltexture(brna);
}

#endif
