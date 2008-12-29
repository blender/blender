/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_ipo_types.h"

#ifdef RNA_RUNTIME

static float rna_BezTriple_handle1_get(PointerRNA *ptr, int index)
{
	BezTriple *bt= (BezTriple*)ptr->data;
	return bt->vec[0][index];
}

static void rna_BezTriple_handle1_set(PointerRNA *ptr, int index, float value)
{
	BezTriple *bt= (BezTriple*)ptr->data;
	bt->vec[0][index]= value;
}

static float rna_BezTriple_handle2_get(PointerRNA *ptr, int index)
{
	BezTriple *bt= (BezTriple*)ptr->data;
	return bt->vec[2][index];
}

static void rna_BezTriple_handle2_set(PointerRNA *ptr, int index, float value)
{
	BezTriple *bt= (BezTriple*)ptr->data;
	bt->vec[2][index]= value;
}

static float rna_BezTriple_ctrlpoint_get(PointerRNA *ptr, int index)
{
	BezTriple *bt= (BezTriple*)ptr->data;
	return bt->vec[1][index];
}

static void rna_BezTriple_ctrlpoint_set(PointerRNA *ptr, int index, float value)
{
	BezTriple *bt= (BezTriple*)ptr->data;
	bt->vec[1][index]= value;
}

#else

void rna_def_bpoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "BPoint", NULL);
	RNA_def_struct_ui_text(srna, "BPoint", "DOC_BROKEN");

	/* Boolean values */
	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Selected", "Selection status");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 0);
	RNA_def_property_ui_text(prop, "Hidden", "Visibility status");

	/* Vector value */
	prop= RNA_def_property(srna, "point", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 4);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_ui_text(prop, "Point", "Point coordinates");

	/* Number values */
	prop= RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alfa");
	/*RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);*/
	RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3d View");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");

	prop= RNA_def_property(srna, "bevel_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	/*RNA_def_property_range(prop, 0.0f, 1.0f);*/
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Bevel Radius", "Radius for bevelling");
}

void rna_def_beztriple(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_handle_type_items[] = {
		{HD_FREE, "FREE", "Free", ""},
		{HD_AUTO, "AUTO", "Auto", ""},
		{HD_VECT, "VECTOR", "Vector", ""},
		{HD_ALIGN, "ALIGNED", "Aligned", ""},
		{HD_AUTO_ANIM, "AUTO_CLAMPED", "Auto Clamped", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_mode_interpolation_items[] = {
		{IPO_CONST, "CONSTANT", "Constant", ""},
		{IPO_LIN, "LINEAR", "Linear", ""},
		{IPO_BEZ, "BEZIER", "Bezier", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "BezTriple", NULL);
	RNA_def_struct_ui_text(srna, "Bezier Triple", "DOC_BROKEN");

	/* Boolean values */
	prop= RNA_def_property(srna, "selected_handle1", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Handle 1 selected", "Handle 1 selection status");

	prop= RNA_def_property(srna, "selected_handle2", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f3", 0);
	RNA_def_property_ui_text(prop, "Handle 2 selected", "Handle 2 selection status");

	prop= RNA_def_property(srna, "selected_control_point", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f2", 0);
	RNA_def_property_ui_text(prop, "Control Point selected", "Control point selection status");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 0);
	RNA_def_property_ui_text(prop, "Hidden", "Visibility status");

	/* Enums */
	prop= RNA_def_property(srna, "handle1_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h1");
	RNA_def_property_enum_items(prop, prop_handle_type_items);
	RNA_def_property_ui_text(prop, "Handle 1 Type", "Handle types");

	prop= RNA_def_property(srna, "handle2_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h2");
	RNA_def_property_enum_items(prop, prop_handle_type_items);
	RNA_def_property_ui_text(prop, "Handle 2 Type", "Handle types");

	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ipo");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, prop_mode_interpolation_items);
	RNA_def_property_ui_text(prop, "Interpolation", "");

	/* Vector values */
	prop= RNA_def_property(srna, "handle1", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_handle1_get", "rna_BezTriple_handle1_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 1", "Coordinates of the first handle");

	prop= RNA_def_property(srna, "control_point", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_ctrlpoint_get", "rna_BezTriple_ctrlpoint_set", NULL);
	RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");

	prop= RNA_def_property(srna, "handle2", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_handle2_get", "rna_BezTriple_handle2_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 2", "Coordinates of the second handle");

	/* Number values */
	prop= RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alfa");
	/*RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);*/
	RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3d View");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");

	prop= RNA_def_property(srna, "bevel_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	/*RNA_def_property_range(prop, 0.0f, 1.0f);*/
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Bevel Radius", "Radius for bevelling");
}

void rna_def_ipodriver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{IPO_DRIVER_TYPE_NORMAL, "NORMAL", "Normal", ""},
		{IPO_DRIVER_TYPE_PYTHON, "SCRIPTED", "Scripted", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "IpoDriver", NULL);
	RNA_def_struct_ui_text(srna, "Ipo Driver", "DOC_BROKEN");

	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Ipo Driver types.");

	/* String values */
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Bone name or scripting expression.");

	/* Pointers */
	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_ui_text(prop, "Driver Object", "Object that controls this Ipo Driver.");
}

void rna_def_ipocurve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_mode_interpolation_items[] = {
		{IPO_CONST, "CONSTANT", "Constant", ""},
		{IPO_LIN, "LINEAR", "Linear", ""},
		{IPO_BEZ, "BEZIER", "Bezier", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_mode_extend_items[] = {
		{IPO_HORIZ, "CONSTANT", "Constant", ""},
		{IPO_DIR, "EXTRAP", "Extrapolation", ""},
		{IPO_CYCL, "CYCLIC", "Cyclic", ""},
		{IPO_CYCLX, "CYCLICX", "Cyclic Extrapolation", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "IpoCurve", NULL);
	RNA_def_struct_ui_text(srna, "Ipo Curve", "DOC_BROKEN");

	/* Enums */
	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ipo");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, prop_mode_interpolation_items);
	RNA_def_property_ui_text(prop, "Interpolation", "");

	prop= RNA_def_property(srna, "extrapolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "extrap");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, prop_mode_extend_items);
	RNA_def_property_ui_text(prop, "Extrapolation", "");

	/* Pointers */
	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "driver");
	RNA_def_property_ui_text(prop, "Ipo Driver", "");

	/* Collections */
	prop= RNA_def_property(srna, "bpoints", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bp", "totvert");
	RNA_def_property_struct_type(prop, "BPoint");
	RNA_def_property_ui_text(prop, "BPoints", "");

	prop= RNA_def_property(srna, "bezier_triples", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bezt", "totvert");
	RNA_def_property_struct_type(prop, "BezTriple");
	RNA_def_property_ui_text(prop, "Bezier Triples", "");
}

void rna_def_ipo(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_blocktype_items[] = {
		{ID_OB, "OBJECT", "Object", ""},
		{ID_MA, "MATERIAL", "Material", ""},
		{ID_TE, "TEXTURE", "Texture", ""},
		{ID_SEQ, "SEQUENCE", "Sequence", ""},
		{ID_CU, "CURVE", "Curve", ""},
		{ID_KE, "KEY", "Key", ""},
		{ID_WO, "WORLD", "World", ""},
		{ID_LA, "LAMP", "Lamp", ""},
		{ID_CA, "CAMERA", "Camera", ""},
		{ID_SO, "SOUND", "Sound", ""},
		{ID_PO, "POSECHANNEL", "PoseChannel", ""},
		{ID_CO, "CONSTRAINT", "Constraint", ""},
		{ID_FLUIDSIM, "FLUIDSIM", "FluidSim", ""},
		{ID_PA, "PARTICLES", "Particles", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Ipo", "ID");
	RNA_def_struct_ui_text(srna, "Ipo", "DOC_BROKEN");

	/* Enums */
	prop= RNA_def_property(srna, "block_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blocktype");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, prop_blocktype_items);
	RNA_def_property_ui_text(prop, "Block Type", "");

	/* Boolean values */
	prop= RNA_def_property(srna, "show_keys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "showkey", 0);
	RNA_def_property_ui_text(prop, "Show Keys", "Show Ipo Keys.");

	prop= RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "muteipo", 0);
	RNA_def_property_ui_text(prop, "Mute", "Mute this Ipo block.");

	/* Collection */
	prop= RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curve", NULL);
	RNA_def_property_struct_type(prop, "IpoCurve");
	RNA_def_property_ui_text(prop, "Curves", "");
}

void RNA_def_ipo(BlenderRNA *brna)
{
	rna_def_ipo(brna);
	rna_def_ipocurve(brna);
	rna_def_bpoint(brna);
	rna_def_beztriple(brna);
	rna_def_ipodriver(brna);
}

#endif

