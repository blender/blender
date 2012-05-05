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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_ID.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_access.h"
#include "RNA_define.h"

#include "DNA_ID.h"
#include "DNA_vfont_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "WM_types.h"

#include "rna_internal.h"

/* enum of ID-block types
 * NOTE: need to keep this in line with the other defines for these
 */
EnumPropertyItem id_type_items[] = {
	{ID_AC, "ACTION", ICON_ACTION, "Action", ""},
	{ID_AR, "ARMATURE", ICON_ARMATURE_DATA, "Armature", ""},
	{ID_BR, "BRUSH", ICON_BRUSH_DATA, "Brush", ""},
	{ID_CA, "CAMERA", ICON_CAMERA_DATA, "Camera", ""},
	{ID_CU, "CURVE", ICON_CURVE_DATA, "Curve", ""},
	{ID_VF, "FONT", ICON_FONT_DATA, "Font", ""},
	{ID_GD, "GREASEPENCIL", ICON_GREASEPENCIL, "Grease Pencil", ""},
	{ID_GR, "GROUP", ICON_GROUP, "Group", ""},
	{ID_IM, "IMAGE", ICON_IMAGE_DATA, "Image", ""},
	{ID_KE, "KEY", ICON_SHAPEKEY_DATA, "Key", ""},
	{ID_LA, "LAMP", ICON_LAMP_DATA, "Lamp", ""},
	{ID_LI, "LIBRARY", ICON_LIBRARY_DATA_DIRECT, "Library", ""},
	{ID_LT, "LATTICE", ICON_LATTICE_DATA, "Lattice", ""},
	{ID_MA, "MATERIAL", ICON_MATERIAL_DATA, "Material", ""},
	{ID_MB, "META", ICON_META_DATA, "MetaBall", ""},
	{ID_ME, "MESH", ICON_MESH_DATA, "Mesh", ""},
	{ID_NT, "NODETREE", ICON_NODETREE, "NodeTree", ""},
	{ID_OB, "OBJECT", ICON_OBJECT_DATA, "Object", ""},
	{ID_PA, "PARTICLE", ICON_PARTICLE_DATA, "Particle", ""},
	{ID_SCE, "SCENE", ICON_SCENE_DATA, "Scene", ""},
	{ID_SCR, "SCREEN", ICON_SPLITSCREEN, "Screen", ""},
	{ID_SPK, "SPEAKER", ICON_SPEAKER, "Speaker", ""},
	{ID_SO, "SOUND", ICON_PLAY_AUDIO, "Sound", ""},
	{ID_TXT, "TEXT", ICON_TEXT, "Text", ""},
	{ID_TE, "TEXTURE", ICON_TEXTURE_DATA, "Texture", ""},
	{ID_WO, "WORLD", ICON_WORLD_DATA, "World", ""},
	{ID_WM, "WINDOWMANAGER", ICON_FULLSCREEN, "Window Manager", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_animsys.h"
#include "BKE_material.h"
#include "BKE_depsgraph.h"

/* name functions that ignore the first two ID characters */
void rna_ID_name_get(PointerRNA *ptr, char *value)
{
	ID *id = (ID*)ptr->data;
	BLI_strncpy(value, id->name+2, sizeof(id->name)-2);
}

int rna_ID_name_length(PointerRNA *ptr)
{
	ID *id = (ID*)ptr->data;
	return strlen(id->name+2);
}

void rna_ID_name_set(PointerRNA *ptr, const char *value)
{
	ID *id = (ID*)ptr->data;
	BLI_strncpy_utf8(id->name+2, value, sizeof(id->name)-2);
	test_idbutton(id->name+2);
}

static int rna_ID_name_editable(PointerRNA *ptr)
{
	ID *id = (ID*)ptr->data;
	
	if (GS(id->name) == ID_VF) {
		VFont *vf = (VFont *)id;
		if (strcmp(vf->name, FO_BUILTIN_NAME) == 0)
			return 0;
	}
	
	return 1;
}

short RNA_type_to_ID_code(StructRNA *type)
{
	if (RNA_struct_is_a(type, &RNA_Action)) return ID_AC;
	if (RNA_struct_is_a(type, &RNA_Armature)) return ID_AR;
	if (RNA_struct_is_a(type, &RNA_Brush)) return ID_BR;
	if (RNA_struct_is_a(type, &RNA_Camera)) return ID_CA;
	if (RNA_struct_is_a(type, &RNA_Curve)) return ID_CU;
	if (RNA_struct_is_a(type, &RNA_GreasePencil)) return ID_GD;
	if (RNA_struct_is_a(type, &RNA_Group)) return ID_GR;
	if (RNA_struct_is_a(type, &RNA_Image)) return ID_IM;
	if (RNA_struct_is_a(type, &RNA_Key)) return ID_KE;
	if (RNA_struct_is_a(type, &RNA_Lamp)) return ID_LA;
	if (RNA_struct_is_a(type, &RNA_Library)) return ID_LI;
	if (RNA_struct_is_a(type, &RNA_Lattice)) return ID_LT;
	if (RNA_struct_is_a(type, &RNA_Material)) return ID_MA;
	if (RNA_struct_is_a(type, &RNA_MetaBall)) return ID_MB;
	if (RNA_struct_is_a(type, &RNA_NodeTree)) return ID_NT;
	if (RNA_struct_is_a(type, &RNA_Mesh)) return ID_ME;
	if (RNA_struct_is_a(type, &RNA_Object)) return ID_OB;
	if (RNA_struct_is_a(type, &RNA_ParticleSettings)) return ID_PA;
	if (RNA_struct_is_a(type, &RNA_Scene)) return ID_SCE;
	if (RNA_struct_is_a(type, &RNA_Screen)) return ID_SCR;
	if (RNA_struct_is_a(type, &RNA_Speaker)) return ID_SPK;
	if (RNA_struct_is_a(type, &RNA_Sound)) return ID_SO;
	if (RNA_struct_is_a(type, &RNA_Text)) return ID_TXT;
	if (RNA_struct_is_a(type, &RNA_Texture)) return ID_TE;
	if (RNA_struct_is_a(type, &RNA_VectorFont)) return ID_VF;
	if (RNA_struct_is_a(type, &RNA_World)) return ID_WO;
	if (RNA_struct_is_a(type, &RNA_WindowManager)) return ID_WM;
	if (RNA_struct_is_a(type, &RNA_MovieClip)) return ID_MC;

	return 0;
}

StructRNA *ID_code_to_RNA_type(short idcode)
{
	switch (idcode) {
		case ID_AC: return &RNA_Action;
		case ID_AR: return &RNA_Armature;
		case ID_BR: return &RNA_Brush;
		case ID_CA: return &RNA_Camera;
		case ID_CU: return &RNA_Curve;
		case ID_GD: return &RNA_GreasePencil;
		case ID_GR: return &RNA_Group;
		case ID_IM: return &RNA_Image;
		case ID_KE: return &RNA_Key;
		case ID_LA: return &RNA_Lamp;
		case ID_LI: return &RNA_Library;
		case ID_LT: return &RNA_Lattice;
		case ID_MA: return &RNA_Material;
		case ID_MB: return &RNA_MetaBall;
		case ID_NT: return &RNA_NodeTree;
		case ID_ME: return &RNA_Mesh;
		case ID_OB: return &RNA_Object;
		case ID_PA: return &RNA_ParticleSettings;
		case ID_SCE: return &RNA_Scene;
		case ID_SCR: return &RNA_Screen;
		case ID_SPK: return &RNA_Speaker;
		case ID_SO: return &RNA_Sound;
		case ID_TXT: return &RNA_Text;
		case ID_TE: return &RNA_Texture;
		case ID_VF: return &RNA_VectorFont;
		case ID_WO: return &RNA_World;
		case ID_WM: return &RNA_WindowManager;
		case ID_MC: return &RNA_MovieClip;
		default: return &RNA_ID;
	}
}

StructRNA *rna_ID_refine(PointerRNA *ptr)
{
	ID *id = (ID*)ptr->data;

	return ID_code_to_RNA_type(GS(id->name));
}

IDProperty *rna_ID_idprops(PointerRNA *ptr, int create)
{
	return IDP_GetProperties(ptr->data, create);
}

void rna_ID_fake_user_set(PointerRNA *ptr, int value)
{
	ID *id = (ID*)ptr->data;

	if (value && !(id->flag & LIB_FAKEUSER)) {
		id->flag |= LIB_FAKEUSER;
		id_us_plus(id);
	}
	else if (!value && (id->flag & LIB_FAKEUSER)) {
		id->flag &= ~LIB_FAKEUSER;
		id_us_min(id);
	}
}

IDProperty *rna_PropertyGroup_idprops(PointerRNA *ptr, int UNUSED(create))
{
	return ptr->data;
}

void rna_PropertyGroup_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	RNA_struct_free(&BLENDER_RNA, type);
}

StructRNA *rna_PropertyGroup_register(Main *UNUSED(bmain), ReportList *reports, void *data, const char *identifier,
                                      StructValidateFunc validate, StructCallbackFunc UNUSED(call),
                                      StructFreeFunc UNUSED(free))
{
	PointerRNA dummyptr;

	/* create dummy pointer */
	RNA_pointer_create(NULL, &RNA_PropertyGroup, NULL, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, NULL) != 0)
		return NULL;

	/* note: it looks like there is no length limit on the srna id since its
	 * just a char pointer, but take care here, also be careful that python
	 * owns the string pointer which it could potentially free while blender
	 * is running. */
	if (BLI_strnlen(identifier, MAX_IDPROP_NAME) == MAX_IDPROP_NAME) {
		BKE_reportf(reports, RPT_ERROR, "registering id property class: '%s' is too long, maximum length is "
		                                STRINGIFY(MAX_IDPROP_NAME), identifier);
		return NULL;
	}

	return RNA_def_struct(&BLENDER_RNA, identifier, "PropertyGroup");  /* XXX */
}

StructRNA* rna_PropertyGroup_refine(PointerRNA *ptr)
{
	return ptr->type;
}

ID *rna_ID_copy(ID *id)
{
	ID *newid;

	if (id_copy(id, &newid, 0)) {
		if (newid) id_us_min(newid);
		return newid;
	}
	
	return NULL;
}

static void rna_ID_update_tag(ID *id, ReportList *reports, int flag)
{
	/* XXX, new function for this! */
#if 0
	if (ob->type == OB_FONT) {
		Curve *cu = ob->data;
		freedisplist(&cu->disp);
		BKE_vfont_to_curve(sce, ob, CU_LEFT);
	}
#endif

	if (flag == 0) {
		/* pass */
	}
	else {
		/* ensure flag us correct for the type */
		switch (GS(id->name)) {
		case ID_OB:
			if (flag & ~(OB_RECALC_ALL)) {
				BKE_report(reports, RPT_ERROR, "'refresh' incompatible with Object ID type");
				return;
			}
			break;
		/* Could add particle updates later */
#if 0
		case ID_PA:
			if (flag & ~(OB_RECALC_ALL|PSYS_RECALC)) {
				BKE_report(reports, RPT_ERROR, "'refresh' incompatible with ParticleSettings ID type");
				return;
			}
			break;
#endif
		default:
			BKE_report(reports, RPT_ERROR, "This ID type is not compatible with any 'refresh' options");
			return;
		}
	}

	DAG_id_tag_update(id, flag);
}

void rna_ID_user_clear(ID *id)
{
	id->us = 0; /* don't save */
	id->flag &= ~LIB_FAKEUSER;
}

static void rna_IDPArray_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	IDProperty *prop = (IDProperty *)ptr->data;
	rna_iterator_array_begin(iter, IDP_IDPArray(prop), sizeof(IDProperty), prop->len, 0, NULL);
}

static int rna_IDPArray_length(PointerRNA *ptr)
{
	IDProperty *prop = (IDProperty *)ptr->data;
	return prop->len;
}

int rna_IDMaterials_assign_int(PointerRNA *ptr, int key, const PointerRNA *assign_ptr)
{
	ID *id =           ptr->id.data;
	short *totcol = give_totcolp_id(id);
	Material *mat_id = assign_ptr->id.data;
	if (totcol && (key >= 0 && key < *totcol)) {
		assign_material_id(id, mat_id, key + 1);
		return 1;
	}
	else {
		return 0;
	}
}

void rna_Library_filepath_set(PointerRNA *ptr, const char *value)
{
	Library *lib = (Library*)ptr->data;
	BKE_library_filepath_set(lib, value);
}

#else

static void rna_def_ID_properties(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* this is struct is used for holding the virtual
	 * PropertyRNA's for ID properties */
	srna = RNA_def_struct(brna, "PropertyGroupItem", NULL);
	RNA_def_struct_sdna(srna, "IDProperty");
	RNA_def_struct_ui_text(srna, "ID Property", "Property that stores arbitrary, user defined properties");
	
	/* IDP_STRING */
	prop = RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	/* IDP_INT */
	prop = RNA_def_property(srna, "int", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	prop = RNA_def_property(srna, "int_array", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_array(prop, 1);

	/* IDP_FLOAT */
	prop = RNA_def_property(srna, "float", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	prop = RNA_def_property(srna, "float_array", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_array(prop, 1);

	/* IDP_DOUBLE */
	prop = RNA_def_property(srna, "double", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	prop = RNA_def_property(srna, "double_array", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_array(prop, 1);

	/* IDP_GROUP */
	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "PropertyGroup");

	prop = RNA_def_property(srna, "collection", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_struct_type(prop, "PropertyGroup");

	prop = RNA_def_property(srna, "idp_array", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "PropertyGroup");
	RNA_def_property_collection_funcs(prop, "rna_IDPArray_begin", "rna_iterator_array_next", "rna_iterator_array_end",
	                                  "rna_iterator_array_get", "rna_IDPArray_length", NULL, NULL, NULL);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	/* never tested, maybe its useful to have this? */
#if 0
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Unique name used in the code and scripting");
	RNA_def_struct_name_property(srna, prop);
#endif

	/* IDP_ID -- not implemented yet in id properties */

	/* ID property groups > level 0, since level 0 group is merged
	 * with native RNA properties. the builtin_properties will take
	 * care of the properties here */
	srna = RNA_def_struct(brna, "PropertyGroup", NULL);
	RNA_def_struct_sdna(srna, "IDPropertyGroup");
	RNA_def_struct_ui_text(srna, "ID Property Group", "Group of ID properties");
	RNA_def_struct_idprops_func(srna, "rna_PropertyGroup_idprops");
	RNA_def_struct_register_funcs(srna, "rna_PropertyGroup_register", "rna_PropertyGroup_unregister", NULL);
	RNA_def_struct_refine_func(srna, "rna_PropertyGroup_refine");

	/* important so python types can have their name used in list views
	 * however this isn't prefect because it overrides how python would set the name
	 * when we only really want this so RNA_def_struct_name_property() is set to something useful */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	/*RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_ui_text(prop, "Name", "Unique name used in the code and scripting");
	RNA_def_struct_name_property(srna, prop);
}


static void rna_def_ID_materials(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	/* for mesh/mball/curve materials */
	srna = RNA_def_struct(brna, "IDMaterials", NULL);
	RNA_def_struct_sdna(srna, "ID");
	RNA_def_struct_ui_text(srna, "ID Materials", "Collection of materials");

	func = RNA_def_function(srna, "append", "material_append_id");
	RNA_def_function_ui_description(func, "Add a new material to the data block");
	parm = RNA_def_pointer(func, "material", "Material", "", "Material to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func = RNA_def_function(srna, "pop", "material_pop_id");
	RNA_def_function_ui_description(func, "Remove a material from the data block");
	parm = RNA_def_int(func, "index", 0, 0, MAXMAT, "", "Index of material to remove", 0, MAXMAT);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "update_data", 0, "", "Update data by re-adjusting the material slots assigned");
	parm = RNA_def_pointer(func, "material", "Material", "", "Material to remove");
	RNA_def_function_return(func, parm);
}

static void rna_def_ID(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop, *parm;

	static EnumPropertyItem update_flag_items[] = {
		{OB_RECALC_OB, "OBJECT", 0, "Object", ""},
		{OB_RECALC_DATA, "DATA", 0, "Data", ""},
		{OB_RECALC_TIME, "TIME", 0, "Time", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ID", NULL);
	RNA_def_struct_ui_text(srna, "ID",
	                       "Base type for datablocks, defining a unique name, linking from other libraries "
	                       "and garbage collection");
	RNA_def_struct_flag(srna, STRUCT_ID|STRUCT_ID_REFCOUNT);
	RNA_def_struct_refine_func(srna, "rna_ID_refine");
	RNA_def_struct_idprops_func(srna, "rna_ID_idprops");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Unique datablock ID name");
	RNA_def_property_string_funcs(prop, "rna_ID_name_get", "rna_ID_name_length", "rna_ID_name_set");
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME-2);
	RNA_def_property_editable_func(prop, "rna_ID_name_editable");
	RNA_def_property_update(prop, NC_ID|NA_RENAME, NULL);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "users", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "us");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Users", "Number of times this datablock is referenced");

	prop = RNA_def_property(srna, "use_fake_user", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIB_FAKEUSER);
	RNA_def_property_ui_text(prop, "Fake User", "Save this datablock even if it has no users");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_ID_fake_user_set");

	prop = RNA_def_property(srna, "tag", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIB_DOIT);
	RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
	RNA_def_property_ui_text(prop, "Tag", "Tools can use this to tag data (initial state is undefined)");

	prop = RNA_def_property(srna, "is_updated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIB_ID_RECALC);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Updated", "Datablock is tagged for recalculation");

	prop = RNA_def_property(srna, "is_updated_data", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LIB_ID_RECALC_DATA);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Updated Data", "Datablock data is tagged for recalculation");

	prop = RNA_def_property(srna, "library", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "lib");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Library", "Library file the datablock is linked from");

	/* functions */
	func = RNA_def_function(srna, "copy", "rna_ID_copy");
	RNA_def_function_ui_description(func, "Create a copy of this datablock (not supported for all datablocks)");
	parm = RNA_def_pointer(func, "id", "ID", "", "New copy of the ID");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "user_clear", "rna_ID_user_clear");
	RNA_def_function_ui_description(func, "Clear the user count of a datablock so its not saved, "
	                                      "on reload the data will be removed");

	func = RNA_def_function(srna, "animation_data_create", "BKE_id_add_animdata");
	RNA_def_function_ui_description(func, "Create animation data to this ID, note that not all ID types support this");
	parm = RNA_def_pointer(func, "anim_data", "AnimData", "", "New animation data or NULL");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "animation_data_clear", "BKE_free_animdata");
	RNA_def_function_ui_description(func, "Clear animation on this this ID");

	func = RNA_def_function(srna, "update_tag", "rna_ID_update_tag");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Tag the ID to update its display data");
	RNA_def_enum_flag(func, "refresh", update_flag_items, 0, "", "Type of updates to perform");
}

static void rna_def_library(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Library", "ID");
	RNA_def_struct_ui_text(srna, "Library", "External .blend file from which data is linked");
	RNA_def_struct_ui_icon(srna, ICON_LIBRARY_DATA_DIRECT);

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "File Path", "Path to the library .blend file");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Library_filepath_set");
	
	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Library");
	RNA_def_property_ui_text(prop, "Parent", "");
}
void RNA_def_ID(BlenderRNA *brna)
{
	StructRNA *srna;

	/* built-in unknown type */
	srna = RNA_def_struct(brna, "UnknownType", NULL);
	RNA_def_struct_ui_text(srna, "Unknown Type", "Stub RNA type used for pointers to unknown or internal data");

	/* built-in any type */
	srna = RNA_def_struct(brna, "AnyType", NULL);
	RNA_def_struct_ui_text(srna, "Any Type", "RNA type used for pointers to any possible data");

	rna_def_ID(brna);
	rna_def_ID_properties(brna);
	rna_def_ID_materials(brna);
	rna_def_library(brna);
}

#endif
