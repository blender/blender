/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_gpencil_modifier.c
 *  \ingroup RNA
 */


#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_mesh_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_data_transfer.h"
#include "BKE_DerivedMesh.h"
#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_multires.h"
#include "BKE_smoke.h" /* For smokeModifier_free & smokeModifier_createType */

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_object_greasepencil_modifier_type_items[] = {
	{0, "", 0, N_("Generate"), "" },
	{eGpencilModifierType_Instance, "GP_INSTANCE", ICON_MOD_ARRAY, "Instance", "Create grid of duplicate instances"},
	{eGpencilModifierType_Build, "GP_BUILD", ICON_MOD_BUILD, "Build", "Create duplication of strokes"},
	{eGpencilModifierType_Simplify, "GP_SIMPLIFY", ICON_MOD_DECIM, "Simplify", "Simplify stroke reducing number of points"},
	{eGpencilModifierType_Subdiv, "GP_SUBDIV", ICON_MOD_SUBSURF, "Subdivide", "Subdivide stroke adding more control points"},
	{0, "", 0, N_("Deform"), "" },
	{eGpencilModifierType_Hook, "GP_HOOK", ICON_HOOK, "Hook", "Deform stroke points using objects"},
	{eGpencilModifierType_Lattice, "GP_LATTICE", ICON_MOD_LATTICE, "Lattice", "Deform strokes using lattice"},
	{eGpencilModifierType_Mirror, "GP_MIRROR", ICON_MOD_MIRROR, "Mirror", "Duplicate strokes like a mirror"},
	{eGpencilModifierType_Noise, "GP_NOISE", ICON_RNDCURVE, "Noise", "Add noise to strokes"},
	{eGpencilModifierType_Offset, "GP_OFFSET", ICON_MOD_DISPLACE, "Offset", "Change stroke location, rotation or scale"},
	{eGpencilModifierType_Smooth, "GP_SMOOTH", ICON_MOD_SMOOTH, "Smooth", "Smooth stroke"},
	{eGpencilModifierType_Thick, "GP_THICK", ICON_MAN_ROT, "Thickness", "Change stroke thickness"},
	{0, "", 0, N_("Color"), "" },
	{eGpencilModifierType_Color, "GP_COLOR", ICON_GROUP_VCOL, "Hue/Saturation", "Apply changes to stroke colors"},
	{eGpencilModifierType_Opacity, "GP_OPACITY", ICON_MOD_MASK, "Opacity", "Opacity of the strokes"},
	{eGpencilModifierType_Tint, "GP_TINT", ICON_COLOR, "Tint", "Tint strokes with new color"},
	{0, NULL, 0, NULL, NULL}
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem modifier_gphook_falloff_items[] = {
	{ eGPHook_Falloff_None,    "NONE", 0, "No Falloff", "" },
	{ eGPHook_Falloff_Curve,   "CURVE", 0, "Curve", "" },
	{ eGPHook_Falloff_Smooth,  "SMOOTH", ICON_SMOOTHCURVE, "Smooth", "" },
	{ eGPHook_Falloff_Sphere,  "SPHERE", ICON_SPHERECURVE, "Sphere", "" },
	{ eGPHook_Falloff_Root,    "ROOT", ICON_ROOTCURVE, "Root", "" },
	{ eGPHook_Falloff_InvSquare, "INVERSE_SQUARE", ICON_ROOTCURVE, "Inverse Square", "" },
	{ eGPHook_Falloff_Sharp,   "SHARP", ICON_SHARPCURVE, "Sharp", "" },
	{ eGPHook_Falloff_Linear,  "LINEAR", ICON_LINCURVE, "Linear", "" },
	{ eGPHook_Falloff_Const,   "CONSTANT", ICON_NOCURVE, "Constant", "" },
	{ 0, NULL, 0, NULL, NULL }
};

static const EnumPropertyItem rna_enum_gpencil_lockshift_items[] = {
	{ GP_LOCKAXIS_X, "GP_LOCKAXIS_X", 0, "X", "Use X axis" },
	{ GP_LOCKAXIS_Y, "GP_LOCKAXIS_Y", 0, "Y", "Use Y axis" },
	{ GP_LOCKAXIS_Z, "GP_LOCKAXIS_Z", 0, "Z", "Use Z axis" },
	{ 0, NULL, 0, NULL, NULL }
};

#endif

#ifdef RNA_RUNTIME

#include "DNA_particle_types.h"
#include "DNA_curve_types.h"
#include "DNA_smoke_types.h"

#include "BKE_cachefile.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_object.h"
#include "BKE_gpencil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

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
		case eGpencilModifierType_Color:
			return &RNA_ColorGpencilModifier;
		case eGpencilModifierType_Instance:
			return &RNA_InstanceGpencilModifier;
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
	if (ptr->id.data) {
		Object *ob = ptr->id.data;
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
	DEG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ptr->id.data);
}

static void rna_GpencilModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_GpencilModifier_update(bmain, scene, ptr);
	DEG_relations_tag_update(bmain);
}

/* Vertex Groups */

#define RNA_GP_MOD_VGROUP_NAME_SET(_type, _prop)                                               \
static void rna_##_type##GpencilModifier_##_prop##_set(PointerRNA *ptr, const char *value)         \
{                                                                                           \
	_type##GpencilModifierData *tmd = (_type##GpencilModifierData *)ptr->data;                            \
	rna_object_vgroup_name_set(ptr, value, tmd->_prop, sizeof(tmd->_prop));                 \
}

RNA_GP_MOD_VGROUP_NAME_SET(Noise, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Thick, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Opacity, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Lattice, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Smooth, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Hook, vgname);
RNA_GP_MOD_VGROUP_NAME_SET(Offset, vgname);

#undef RNA_GP_MOD_VGROUP_NAME_SET

/* Objects */

static void greasepencil_modifier_object_set(Object *self, Object **ob_p, int type, PointerRNA value)
{
	Object *ob = value.data;

	if (!self || ob != self) {
		if (!ob || type == OB_EMPTY || ob->type == type) {
			id_lib_extern((ID *)ob);
			*ob_p = ob;
		}
	}
}

#define RNA_GP_MOD_OBJECT_SET(_type, _prop, _obtype)                                           \
static void rna_##_type##GpencilModifier_##_prop##_set(PointerRNA *ptr, PointerRNA value)          \
{                                                                                           \
	_type##GpencilModifierData *tmd = (_type##GpencilModifierData *)ptr->data;                            \
	greasepencil_modifier_object_set(ptr->id.data, &tmd->_prop, _obtype, value);                         \
}

RNA_GP_MOD_OBJECT_SET(Lattice, object, OB_LATTICE);
RNA_GP_MOD_OBJECT_SET(Mirror, object, OB_EMPTY);

#undef RNA_GP_MOD_OBJECT_SET

static void rna_HookGpencilModifier_object_set(PointerRNA *ptr, PointerRNA value)
{
	HookGpencilModifierData *hmd = ptr->data;
	Object *ob = (Object *)value.data;

	hmd->object = ob;
	id_lib_extern((ID *)ob);
	BKE_object_modifier_gpencil_hook_reset(ob, hmd);
}

#else

static void rna_def_modifier_gpencilnoise(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NoiseGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Noise Modifier", "Noise effect modifier");
	RNA_def_struct_sdna(srna, "NoiseGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_RNDCURVE);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgname");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NoiseGpencilModifier_vgname_set");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_range(prop, 0, 30.0);
	RNA_def_property_ui_text(prop, "Factor", "Amount of noise to apply");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "random", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_USE_RANDOM);
	RNA_def_property_ui_text(prop, "Random", "Use random values");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_position", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_MOD_LOCATION);
	RNA_def_property_ui_text(prop, "Affect Position", "The modifier affects the position of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_strength", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_MOD_STRENGTH);
	RNA_def_property_ui_text(prop, "Affect Strength", "The modifier affects the color strength of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_thickness", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_MOD_THICKNESS);
	RNA_def_property_ui_text(prop, "Affect Thickness", "The modifier affects the thickness of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_MOD_UV);
	RNA_def_property_ui_text(prop, "Affect UV", "The modifier affects the UV rotation factor of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "full_stroke", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_FULL_STROKE);
	RNA_def_property_ui_text(prop, "Full Stroke", "The noise moves the stroke as a whole, not point by point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "move_extreme", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_MOVE_EXTREME);
	RNA_def_property_ui_text(prop, "Move Extremes", "The noise moves the stroke extreme points");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pass_index");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Pass", "Pass index");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "step");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Step", "Number of frames before recalculate random values again");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_LAYER);
	RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_NOISE_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
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

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgname");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SmoothGpencilModifier_vgname_set");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Factor", "Amount of smooth to apply");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_position", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_LOCATION);
	RNA_def_property_ui_text(prop, "Affect Position", "The modifier affects the position of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_strength", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_STRENGTH);
	RNA_def_property_ui_text(prop, "Affect Strength", "The modifier affects the color strength of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_thickness", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_THICKNESS);
	RNA_def_property_ui_text(prop, "Affect Thickness", "The modifier affects the thickness of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "affect_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_MOD_UV);
	RNA_def_property_ui_text(prop, "Affect UV", "The modifier affects the UV rotation factor of the point");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pass_index");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Pass", "Pass index");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "step");
	RNA_def_property_range(prop, 1, 10);
	RNA_def_property_ui_text(prop, "Step", "Number of times to apply smooth (high numbers can reduce fps)");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_LAYER);
	RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SMOOTH_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
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

	prop = RNA_def_property(srna, "level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "level");
	RNA_def_property_range(prop, 0, 5);
	RNA_def_property_ui_text(prop, "Level", "Number of subdivisions");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "simple", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SUBDIV_SIMPLE);
	RNA_def_property_ui_text(prop, "Simple", "The modifier only add control points");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SUBDIV_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilsimplify(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_gpencil_simplify_mode_items[] = {
		{ GP_SIMPLIFY_FIXED, "FIXED", ICON_IPO_CONSTANT, "Fixed",
		"Delete alternative vertices in the stroke, except extrems" },
		{ GP_SIMPLIFY_ADAPTATIVE, "ADAPTATIVE", ICON_IPO_EASE_IN_OUT, "Adaptative",
		"Use a RDP algorithm to simplify" },
		{ 0, NULL, 0, NULL, NULL }
	};

	srna = RNA_def_struct(brna, "SimplifyGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Simplify Modifier", "Simplify Stroke modifier");
	RNA_def_struct_sdna(srna, "SimplifyGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DECIM);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SIMPLIFY_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	/* Mode */
	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_gpencil_simplify_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "How simplify the stroke");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "step", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "step");
	RNA_def_property_range(prop, 1, 50);
	RNA_def_property_ui_text(prop, "Iterations", "Number of times to apply simplify");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilthick(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ThickGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Thick Modifier", "Subdivide and Smooth Stroke modifier");
	RNA_def_struct_sdna(srna, "ThickGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MAN_ROT);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgname");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ThickGpencilModifier_vgname_set");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "thickness");
	RNA_def_property_range(prop, -100, 500);
	RNA_def_property_ui_text(prop, "Thickness", "Factor of thickness change");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "use_custom_curve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_CUSTOM_CURVE);
	RNA_def_property_ui_text(prop, "Custom Curve", "Use a custom curve to define thickness changes");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "normalize_thickness", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_THICK_NORMALIZE);
	RNA_def_property_ui_text(prop, "Normalize", "Normalize the full stroke to modifier thickness");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curve_thickness");
	RNA_def_property_ui_text(prop, "Curve", "Custom Thickness Curve");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpenciloffset(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OffsetGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Offset Modifier", "Offset Stroke modifier");
	RNA_def_struct_sdna(srna, "OffsetGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_DISPLACE);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OFFSET_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Location", "Values for change location");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "Values for chages in rotation");
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

	srna = RNA_def_struct(brna, "TintGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Tint Modifier", "Tint Stroke Color modifier");
	RNA_def_struct_sdna(srna, "TintGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_COLOR);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rgb");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Color used for tinting");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 3);
	RNA_def_property_ui_text(prop, "Factor", "Factor for mixing color");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "create_materials", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_CREATE_COLORS);
	RNA_def_property_ui_text(prop, "Create Materials", "When apply modifier, create new material");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_TINT_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilcolor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ColorGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Hue/Saturation Modifier", "Change Hue/Saturation modifier");
	RNA_def_struct_sdna(srna, "ColorGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_GROUP_VCOL);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "hue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
	RNA_def_property_float_sdna(prop, NULL, "hsv[0]");
	RNA_def_property_ui_text(prop, "Hue", "Color Hue");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
	RNA_def_property_float_sdna(prop, NULL, "hsv[1]");
	RNA_def_property_ui_text(prop, "Saturation", "Color Saturation");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_range(prop, 0.0, 2.0, 0.1, 3);
	RNA_def_property_float_sdna(prop, NULL, "hsv[2]");
	RNA_def_property_ui_text(prop, "Value", "Color Value");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "create_materials", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_CREATE_COLORS);
	RNA_def_property_ui_text(prop, "Create Materials", "When apply modifier, create new material");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_COLOR_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilopacity(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "OpacityGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Opacity Modifier", "Opacity of Strokes modifier");
	RNA_def_struct_sdna(srna, "OpacityGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgname");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex group name for modulating the deform");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_OpacityGpencilModifier_vgname_set");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_ui_range(prop, 0, 2.0, 0.1, 3);
	RNA_def_property_ui_text(prop, "Factor", "Factor of Opacity");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_OPACITY_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilinstance(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "InstanceGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Instance Modifier", "Create grid of duplicate instances");
	RNA_def_struct_sdna(srna, "InstanceGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_ARRAY);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pass_index");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Pass", "Pass index");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "count", PROP_INT, PROP_XYZ);
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_range(prop, 1, 1000, 1, -1);
	RNA_def_property_ui_text(prop, "Count", "Number of items");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	/* Offset parameters */
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_ui_text(prop, "Offset", "Value for the distance between items");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "shift", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "shift");
	RNA_def_property_ui_text(prop, "Shift", "Shiftness value");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "lock_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "lock_axis");
	RNA_def_property_enum_items(prop, rna_enum_gpencil_lockshift_items);
	//RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Axis", "");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "Value for chages in rotation");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_ui_text(prop, "Scale", "Value for changes in scale");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "random_rot", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_INSTANCE_RANDOM_ROT);
	RNA_def_property_ui_text(prop, "Random Rotation", "Use random factors for rotation");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "rot_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rnd_rot");
	RNA_def_property_ui_text(prop, "Rotation Factor", "Random factor for rotation");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "random_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_INSTANCE_RANDOM_SIZE);
	RNA_def_property_ui_text(prop, "Random Scale", "Use random factors for scale");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "scale_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rnd_size");
	RNA_def_property_ui_text(prop, "Scale Factor", "Random factor for scale");
	RNA_def_property_range(prop, -10.0, 10.0);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_INSTANCE_INVERT_LAYER);
	RNA_def_property_ui_text(prop, "Inverse Layers", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_INSTANCE_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "use_make_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_INSTANCE_MAKE_OBJECTS);
	RNA_def_property_ui_text(prop, "Make Objects",
		"When applying this modifier, instances get created as separate objects");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
}

static void rna_def_modifier_gpencilbuild(BlenderRNA *brna)
{
	static EnumPropertyItem prop_gpencil_build_mode_items[] = {
		{GP_BUILD_MODE_SEQUENTIAL, "SEQUENTIAL", ICON_PARTICLE_POINT, "Sequential",
		 "Strokes appear/disappear one after the other, but only a single one changes at a time"},
		{GP_BUILD_MODE_CONCURRENT, "CONCURRENT", ICON_PARTICLE_TIP, "Concurrent",
		 "Multiple strokes appear/disappear at once"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_gpencil_build_transition_items[] = {
		{GP_BUILD_TRANSITION_GROW, "GROW", 0, "Grow",
		 "Show points in the order they occur in each stroke "
		 "(e.g. for animating lines being drawn)"},
		{GP_BUILD_TRANSITION_SHRINK, "SHRINK", 0, "Shrink",
		 "Hide points from the end of each stroke to the start "
		 "(e.g. for animating lines being erased)"},
		{GP_BUILD_TRANSITION_FADE, "FADE", 0, "Fade",
		 "Hide points in the order they occur in each stroke "
		 "(e.g. for animating ink fading or vanishing after getting drawn)"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_gpencil_build_time_align_items[] = {
		{GP_BUILD_TIMEALIGN_START, "START", 0, "Align Start",
		 "All strokes start at same time (i.e. short strokes finish earlier)"},
		{GP_BUILD_TIMEALIGN_END, "END", 0, "Align End",
		 "All strokes end at same time (i.e. short strokes start later)"},
		{0, NULL, 0, NULL, NULL}
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
	RNA_def_property_ui_text(prop, "Transition", "How are strokes animated (i.e. are they appearing or disappearing)");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");


	/* Transition Onset Delay + Length */
	prop = RNA_def_property(srna, "start_delay", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "start_delay");
	RNA_def_property_ui_text(prop, "Start Delay", "Number of frames after each GP keyframe before the modifier has any effect");
	RNA_def_property_range(prop, 0, MAXFRAMEF);
	RNA_def_property_ui_range(prop, 0, 200, 1, -1);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "length");
	RNA_def_property_ui_text(prop, "Length",
	                         "Maximum number of frames that the build effect can run for "
	                         "(unless another GP keyframe occurs before this time has elapsed)");
	RNA_def_property_range(prop, 1, MAXFRAMEF);
	RNA_def_property_ui_range(prop, 1, 1000, 1, -1);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");


	/* Concurrent Mode Settings */
	prop = RNA_def_property(srna, "concurrent_time_alignment", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "time_alignment");
	RNA_def_property_enum_items(prop, prop_gpencil_build_time_align_items);
	RNA_def_property_ui_text(prop, "Time Alignment", "When should strokes start to appear/disappear");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");



	/* Time Limits */
	prop = RNA_def_property(srna, "use_restrict_frame_range", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_BUILD_RESTRICT_TIME);
	RNA_def_property_ui_text(prop, "Restrict Frame Range", "Only modify strokes during the specified frame range");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "start_frame");
	RNA_def_property_ui_text(prop, "Start Frame", "Start Frame (when Restrict Frame Range is enabled)");
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

	/* Filters - Pass Index */
#if 0
	prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pass_index");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Pass", "Pass index");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_BUILD_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
#endif
}

static void rna_def_modifier_gpencillattice(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "LatticeGpencilModifier", "GpencilModifier");
	RNA_def_struct_ui_text(srna, "Lattice Modifier", "Change stroke using lattice to deform modifier");
	RNA_def_struct_sdna(srna, "LatticeGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_LATTICE);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LATTICE_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Lattice object to deform with");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_LatticeGpencilModifier_object_set", NULL, "rna_Lattice_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
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
	RNA_def_struct_ui_text(srna, "Mirror Modifier", "Change stroke using lattice to deform modifier");
	RNA_def_struct_sdna(srna, "MirrorGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MIRROR);

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_MIRROR_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Object used as center");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_MirrorGpencilModifier_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

	prop = RNA_def_property(srna, "clip", PROP_BOOLEAN, PROP_NONE);
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
	RNA_def_struct_ui_text(srna, "Hook Modifier", "Hook modifier to modify the location of stroke points");
	RNA_def_struct_sdna(srna, "HookGpencilModifierData");
	RNA_def_struct_ui_icon(srna, ICON_HOOK);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Parent Object for hook, also recalculates and clears offset");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_HookGpencilModifier_object_set", NULL, NULL);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

	prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "subtarget");
	RNA_def_property_ui_text(prop, "Sub-Target",
		"Name of Parent Bone for hook (if applicable), also recalculates and clears offset");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_dependency_update");

	prop = RNA_def_property(srna, "layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layername");
	RNA_def_property_ui_text(prop, "Layer", "Layer name");
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

	prop = RNA_def_property(srna, "invert_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_PASS);
	RNA_def_property_ui_text(prop, "Inverse Pass", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "invert_vertex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_INVERT_VGROUP);
	RNA_def_property_ui_text(prop, "Inverse VertexGroup", "Inverse filter");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "force");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Strength", "Relative force of the hook");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, modifier_gphook_falloff_items);  /* share the enum */
	RNA_def_property_ui_text(prop, "Falloff Type", "");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "falloff_radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "falloff");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 100, 100, 2);
	RNA_def_property_ui_text(prop, "Radius", "If not zero, the distance from the hook where influence ends");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
	RNA_def_property_ui_text(prop, "Falloff Curve", "Custom Lamp Falloff Curve");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "center", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cent");
	RNA_def_property_ui_text(prop, "Hook Center", "");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "parentinv");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(prop, "Matrix", "Reverse the transformation between this object and its target");
	RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_GpencilModifier_update");

	prop = RNA_def_property(srna, "use_falloff_uniform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_HOOK_UNIFORM_SPACE);
	RNA_def_property_ui_text(prop, "Uniform Falloff", "Compensate for non-uniform object scale");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
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
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 0);

	prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eGpencilModifierMode_Render);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_ui_text(prop, "Render", "Use modifier during render");
	RNA_def_property_ui_icon(prop, ICON_SCENE, 0);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "show_in_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eGpencilModifierMode_Editmode);
	RNA_def_property_ui_text(prop, "Edit Mode", "Display modifier in Edit mode");
	RNA_def_property_update(prop, 0, "rna_GpencilModifier_update");
	RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eGpencilModifierMode_Expanded);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_ui_text(prop, "Expanded", "Set modifier expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	/* types */
	rna_def_modifier_gpencilnoise(brna);
	rna_def_modifier_gpencilsmooth(brna);
	rna_def_modifier_gpencilsubdiv(brna);
	rna_def_modifier_gpencilsimplify(brna);
	rna_def_modifier_gpencilthick(brna);
	rna_def_modifier_gpenciloffset(brna);
	rna_def_modifier_gpenciltint(brna);
	rna_def_modifier_gpencilcolor(brna);
	rna_def_modifier_gpencilinstance(brna);
	rna_def_modifier_gpencilbuild(brna);
	rna_def_modifier_gpencilopacity(brna);
	rna_def_modifier_gpencillattice(brna);
	rna_def_modifier_gpencilmirror(brna);
	rna_def_modifier_gpencilhook(brna);
}

#endif
