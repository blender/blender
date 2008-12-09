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

#else

void rna_def_ipodriver(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{IPO_DRIVER_TYPE_NORMAL, "NORMAL", "Normal", ""},
		{IPO_DRIVER_TYPE_PYTHON, "SCRIPTED", "Scripted", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "IpoDriver", NULL, "Ipo Driver");

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

	srna= RNA_def_struct(brna, "IpoCurve", NULL, "Ipo Curve");

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

	srna= RNA_def_struct(brna, "Ipo", "ID", "Ipo");

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
	rna_def_ipodriver(brna);
}

#endif

