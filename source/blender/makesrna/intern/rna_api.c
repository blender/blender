#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "UI_interface.h"


void RNA_api_main(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	func= RNA_def_function(srna, "add_mesh", "RNA_api_main_add_mesh");
	RNA_def_function_ui_description(func, "Add a new mesh.");
	prop= RNA_def_string(func, "name", "", 0, "", "New name for the datablock.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "mesh", "Mesh", "", "A new mesh.");
	RNA_def_function_return(func, prop);

	func= RNA_def_function(srna, "remove_mesh", "RNA_api_main_remove_mesh");
	RNA_def_function_ui_description(func, "Remove a mesh if it has only one user.");
	prop= RNA_def_pointer(func, "mesh", "Mesh", "", "A mesh to remove.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
}

void RNA_api_mesh(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	/*
	func= RNA_def_function(srna, "copy", "RNA_api_mesh_copy");
	RNA_def_function_ui_description(func, "Copy mesh data.");
	prop= RNA_def_pointer(func, "src", "Mesh", "", "A mesh to copy data from.");
	RNA_def_property_flag(prop, PROP_REQUIRED);*/

	func= RNA_def_function(srna, "make_rendermesh", "RNA_api_mesh_make_rendermesh");
	RNA_def_function_ui_description(func, "Copy mesh data from object with all modifiers applied.");
	prop= RNA_def_pointer(func, "sce", "Scene", "", "Scene.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "ob", "Object", "", "Object to copy data from.");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	/*
	func= RNA_def_function(srna, "add_geom", "RNA_api_mesh_add_geom");
	RNA_def_function_ui_description(func, "Add geometry data to mesh.");
	prop= RNA_def_collection(func, "verts", "?", "", "Vertices.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_collection(func, "faces", "?", "", "Faces.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	*/
}

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	func= RNA_def_function(srna, "add_fileselect", "RNA_api_wm_add_fileselect");
	RNA_def_function_ui_description(func, "Show up the file selector.");
	prop= RNA_def_pointer(func, "context", "Context", "", "Context.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "op", "Operator", "", "Operator to call.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
}

