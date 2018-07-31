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

/** \file blender/makesrna/intern/rna_shader_fx.c
 *  \ingroup RNA
 */


#include <float.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_shader_fx_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_shader_fx.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_object_shaderfx_type_items[] = {
	{eShaderFxType_Blur, "FX_BLUR", ICON_SOLO_ON, "Blur", "Apply Gaussian Blur to object" },
	{eShaderFxType_Colorize, "FX_COLORIZE", ICON_SOLO_ON, "Colorize", "Apply different tint effects" },
	{eShaderFxType_Flip, "FX_FLIP", ICON_SOLO_ON, "Flip", "Flip image" },
	{eShaderFxType_Light, "FX_LIGHT", ICON_SOLO_ON, "Light", "Simulate ilumination" },
	{eShaderFxType_Pixel, "FX_PIXEL", ICON_SOLO_ON, "Pixelate", "Pixelate image"},
	{eShaderFxType_Rim, "FX_RIM", ICON_SOLO_ON, "Rim", "Add a rim to the image" },
	{eShaderFxType_Swirl, "FX_SWIRL", ICON_SOLO_ON, "Swirl", "Create a rotation distortion"},
	{eShaderFxType_Wave, "FX_WAVE", ICON_SOLO_ON, "Wave Distortion", "Apply sinusoidal deformation"},
	{0, NULL, 0, NULL, NULL}
};

const EnumPropertyItem rna_enum_shaderfx_rim_modes_items[] = {
	{eShaderFxRimMode_Normal, "NORMAL", 0, "Normal", "" },
	{eShaderFxRimMode_Overlay, "OVERLAY", 0, "Overlay", "" },
	{eShaderFxRimMode_Add, "ADD", 0, "Add", "" },
	{eShaderFxRimMode_Subtract, "SUBTRACT", 0, "Subtract", "" },
	{eShaderFxRimMode_Multiply, "MULTIPLY", 0, "Multiply", "" },
	{eShaderFxRimMode_Divide, "DIVIDE", 0, "Divide", "" },
	{0, NULL, 0, NULL, NULL }
};

const EnumPropertyItem rna_enum_shaderfx_colorize_modes_items[] = {
	{eShaderFxColorizeMode_GrayScale, "GRAYSCALE", 0, "Gray Scale", "" },
	{eShaderFxColorizeMode_Sepia, "SEPIA", 0, "Sepia", "" },
	{eShaderFxColorizeMode_BiTone, "BITONE", 0, "Bi-Tone", "" },
	{eShaderFxColorizeMode_Transparent, "TRANSPARENT", 0, "Transparent", "" },
	{eShaderFxColorizeMode_Custom, "CUSTOM", 0, "Custom", "" },
	{0, NULL, 0, NULL, NULL }
};

#ifdef RNA_RUNTIME

#include "BKE_shader_fx.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

static StructRNA *rna_ShaderFx_refine(struct PointerRNA *ptr)
{
	ShaderFxData *md = (ShaderFxData *)ptr->data;

	switch ((ShaderFxType)md->type) {
		case eShaderFxType_Blur:
			return &RNA_ShaderFxBlur;
		case eShaderFxType_Colorize:
			return &RNA_ShaderFxColorize;
		case eShaderFxType_Wave:
			return &RNA_ShaderFxWave;
		case eShaderFxType_Pixel:
			return &RNA_ShaderFxPixel;
		case eShaderFxType_Rim:
			return &RNA_ShaderFxRim;
		case eShaderFxType_Swirl:
			return &RNA_ShaderFxSwirl;
		case eShaderFxType_Flip:
			return &RNA_ShaderFxFlip;
		case eShaderFxType_Light:
			return &RNA_ShaderFxLight;
			/* Default */
		case eShaderFxType_None:
		case NUM_SHADER_FX_TYPES:
			return &RNA_ShaderFx;
	}

	return &RNA_ShaderFx;
}

static void rna_ShaderFx_name_set(PointerRNA *ptr, const char *value)
{
	ShaderFxData *gmd = ptr->data;
	char oldname[sizeof(gmd->name)];

	/* make a copy of the old name first */
	BLI_strncpy(oldname, gmd->name, sizeof(gmd->name));

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(gmd->name, value, sizeof(gmd->name));

	/* make sure the name is truly unique */
	if (ptr->id.data) {
		Object *ob = ptr->id.data;
		BKE_shaderfx_unique_name(&ob->shader_fx, gmd);
	}

	/* fix all the animation data which may link to this */
	BKE_animdata_fix_paths_rename_all(NULL, "shader_effects", oldname, gmd->name);
}

static char *rna_ShaderFx_path(PointerRNA *ptr)
{
	ShaderFxData *gmd = ptr->data;
	char name_esc[sizeof(gmd->name) * 2];

	BLI_strescape(name_esc, gmd->name, sizeof(name_esc));
	return BLI_sprintfN("shader_effects[\"%s\"]", name_esc);
}

static void rna_ShaderFx_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	DEG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NC_GPENCIL, ptr->id.data);
}

static void rna_ShaderFx_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_ShaderFx_update(bmain, scene, ptr);
	DEG_relations_tag_update(bmain);
}

/* Objects */

static void shaderfx_object_set(Object *self, Object **ob_p, int type, PointerRNA value)
{
	Object *ob = value.data;

	if (!self || ob != self) {
		if (!ob || type == OB_EMPTY || ob->type == type) {
			id_lib_extern((ID *)ob);
			*ob_p = ob;
		}
	}
}

#define RNA_FX_OBJECT_SET(_type, _prop, _obtype)                                           \
static void rna_##_type##ShaderFx_##_prop##_set(PointerRNA *ptr, PointerRNA value)          \
{                                                                                           \
	_type##ShaderFxData *tmd = (_type##ShaderFxData *)ptr->data;                            \
	shaderfx_object_set(ptr->id.data, &tmd->_prop, _obtype, value);                         \
}

RNA_FX_OBJECT_SET(Light, object, OB_EMPTY);
RNA_FX_OBJECT_SET(Swirl, object, OB_EMPTY);

#undef RNA_FX_OBJECT_SET

#else

static void rna_def_shader_fx_blur(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderFxBlur", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Gaussian Blur Effect", "Gaussian Blur effect");
	RNA_def_struct_sdna(srna, "BlurShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "factor", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "radius");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Factor", "Factor of Blur");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_range(prop, 0, 32);
	RNA_def_property_ui_range(prop, 0, 32, 2, -1);
	RNA_def_property_int_default(prop, 4);
	RNA_def_property_ui_text(prop, "Samples", "Number of Blur Samples (zero, disable blur)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "coc", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "coc");
	RNA_def_property_range(prop, 0.001f, 1.0f);
	RNA_def_property_float_default(prop, 0.025f);
	RNA_def_property_ui_text(prop, "Precision", "Define circle of confusion for depth of field");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "use_dof_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FX_BLUR_DOF_MODE);
	RNA_def_property_ui_text(prop, "Lock Focal Plane", "Blur using focal plane distance as factor to simulate depth of field effect (only in camera view)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
}

static void rna_def_shader_fx_colorize(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderFxColorize", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Colorize Effect", "Colorize effect");
	RNA_def_struct_sdna(srna, "ColorizeShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "factor");
	RNA_def_property_range(prop, 0, 1.0);
	RNA_def_property_ui_text(prop, "Factor", "Mix factor");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "low_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "low_color");
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Low color", "First color used for effect");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "high_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "high_color");
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Hight color", "Second color used for effect");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, rna_enum_shaderfx_colorize_modes_items);
	RNA_def_property_ui_text(prop, "Mode", "Effect mode");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
}

static void rna_def_shader_fx_wave(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_shaderfx_wave_type_items[] = {
		{ 0, "HORIZONTAL", 0, "Horizontal", "" },
		{ 1, "VERTICAL", 0, "Vertical", "" },
		{ 0, NULL, 0, NULL, NULL }
	};

	srna = RNA_def_struct(brna, "ShaderFxWave", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Wave Deformation Effect", "Wave Deformation effect");
	RNA_def_struct_sdna(srna, "WaveShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "orientation");
	RNA_def_property_enum_items(prop, prop_shaderfx_wave_type_items);
	RNA_def_property_ui_text(prop, "Orientation", "Direction of the wave");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "amplitude");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of Wave");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "period", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "period");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Period", "Period of Wave");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phase");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Phase", "Phase Shift of Wave");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
}

static void rna_def_shader_fx_pixel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderFxPixel", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Pixelate Effect", "Pixelate effect");
	RNA_def_struct_sdna(srna, "PixelShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "size");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Size", "Pixel size");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rgba");
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "Color", "Color used for lines");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "use_lines", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FX_PIXEL_USE_LINES);
	RNA_def_property_ui_text(prop, "Lines", "Display lines between pixels");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
}

static void rna_def_shader_fx_rim(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderFxRim", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Rim Effect", "Rim effect");
	RNA_def_struct_sdna(srna, "RimShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "offset", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, -INT_MAX, INT_MAX);
	RNA_def_property_ui_text(prop, "Offset", "Offset of the rim");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "rim_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rim_rgb");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Rim Color", "Color used for Rim");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "mask_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "mask_rgb");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Mask Color", "Color that must be keept");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, rna_enum_shaderfx_rim_modes_items);
	RNA_def_property_ui_text(prop, "Mode", "Blend mode");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "blur", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "blur");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Blur", "Number of pixels for bluring rim (set to 0 to disable)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_range(prop, 0, 32);
	RNA_def_property_ui_range(prop, 0, 32, 2, -1);
	RNA_def_property_int_default(prop, 4);
	RNA_def_property_ui_text(prop, "Samples", "Number of Blur Samples (zero, disable blur)");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
}

static void rna_def_shader_fx_swirl(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderFxSwirl", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Swirl Effect", "Swirl effect");
	RNA_def_struct_sdna(srna, "SwirlShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "radius", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "radius");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Radius", "Radius to apply");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, DEG2RAD(-5 * 360), DEG2RAD(5 * 360));
	RNA_def_property_ui_range(prop, DEG2RAD(-5 * 360), DEG2RAD(5 * 360), 5, 2);
	RNA_def_property_ui_text(prop, "Angle", "Angle of rotation");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "transparent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FX_SWIRL_MAKE_TRANSPARENT);
	RNA_def_property_ui_text(prop, "Transparent", "Make image transparent outside of radius");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Object to determine center location");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SwirlShaderFx_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_ShaderFx_dependency_update");
}

static void rna_def_shader_fx_flip(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderFxFlip", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Flip Effect", "Flip effect");
	RNA_def_struct_sdna(srna, "FlipShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "flip_horizontal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FX_FLIP_HORIZONTAL);
	RNA_def_property_ui_text(prop, "Horizontal", "Flip image horizontally");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "flip_vertical", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FX_FLIP_VERTICAL);
	RNA_def_property_ui_text(prop, "Vertical", "Flip image vertically");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
}

static void rna_def_shader_fx_light(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ShaderFxLight", "ShaderFx");
	RNA_def_struct_ui_text(srna, "Light Effect", "Light effect");
	RNA_def_struct_sdna(srna, "LightShaderFxData");
	RNA_def_struct_ui_icon(srna, ICON_SOLO_ON);

	prop = RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "energy");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 1, FLT_MAX, 1, 2);
	RNA_def_property_ui_text(prop, "Energy", "Strength of light source");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "ambient", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ambient");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, FLT_MAX, 1, 2);
	RNA_def_property_ui_text(prop, "Ambient", "Strength of ambient light source");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Object", "Object to determine light source location");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_LightShaderFx_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_update(prop, 0, "rna_ShaderFx_dependency_update");
}

void RNA_def_shader_fx(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* data */
	srna = RNA_def_struct(brna, "ShaderFx", NULL);
	RNA_def_struct_ui_text(srna, "ShaderFx", "Effect affecting the grease pencil object");
	RNA_def_struct_refine_func(srna, "rna_ShaderFx_refine");
	RNA_def_struct_path_func(srna, "rna_ShaderFx_path");
	RNA_def_struct_sdna(srna, "ShaderFxData");

	/* strings */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ShaderFx_name_set");
	RNA_def_property_ui_text(prop, "Name", "Effect name");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, NULL);
	RNA_def_struct_name_property(srna, prop);

	/* enums */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_object_shaderfx_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	/* flags */
	prop = RNA_def_property(srna, "show_viewport", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eShaderFxMode_Realtime);
	RNA_def_property_ui_text(prop, "Realtime", "Display effect in viewport");
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 0);

	prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eShaderFxMode_Render);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_ui_text(prop, "Render", "Use effect during render");
	RNA_def_property_ui_icon(prop, ICON_SCENE, 0);
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "show_in_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eShaderFxMode_Editmode);
	RNA_def_property_ui_text(prop, "Edit Mode", "Display effect in Edit mode");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_ShaderFx_update");
	RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);
	
	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", eShaderFxMode_Expanded);
	RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
	RNA_def_property_ui_text(prop, "Expanded", "Set effect expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	/* types */
	rna_def_shader_fx_blur(brna);
	rna_def_shader_fx_colorize(brna);
	rna_def_shader_fx_wave(brna);
	rna_def_shader_fx_pixel(brna);
	rna_def_shader_fx_rim(brna);
	rna_def_shader_fx_swirl(brna);
	rna_def_shader_fx_flip(brna);
	rna_def_shader_fx_light(brna);
}

#endif
