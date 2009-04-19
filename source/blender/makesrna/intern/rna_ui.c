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
 * Contributor(s): Blender Foundation (2009)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "DNA_screen_types.h"

#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#define RNA_STRING_FUNCTIONS(fname, member) \
static void fname##_get(PointerRNA *ptr, char *value) \
{ \
	BLI_strncpy(value, member, sizeof(member)); \
} \
\
static int fname##_length(PointerRNA *ptr) \
{ \
	return strlen(member); \
} \
\
static void fname##_set(PointerRNA *ptr, const char *value) \
{ \
	BLI_strncpy(member, value, sizeof(member)); \
} \

RNA_STRING_FUNCTIONS(rna_Panel_idname, ((Panel*)ptr->data)->type->idname)
RNA_STRING_FUNCTIONS(rna_Panel_label, ((Panel*)ptr->data)->type->label)
RNA_STRING_FUNCTIONS(rna_Panel_context, ((Panel*)ptr->data)->type->context)
RNA_STRING_FUNCTIONS(rna_Panel_space_type, ((Panel*)ptr->data)->type->space_type)
RNA_STRING_FUNCTIONS(rna_Panel_region_type, ((Panel*)ptr->data)->type->region_type)

static void panel_draw(const bContext *C, Panel *pnl)
{
	PointerRNA ptr;
	ParameterList *list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, pnl->type->py_srna, pnl, &ptr);
	func= RNA_struct_find_function(&ptr, "draw");

	list= RNA_parameter_list_create(&ptr, func);
	RNA_parameter_set_lookup(list, "context", &C);
	pnl->type->py_call(&ptr, func, list);

	RNA_parameter_list_free(list);
}

static int panel_poll(const bContext *C, PanelType *pt)
{
	PointerRNA ptr;
	ParameterList *list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, pt->py_srna, NULL, &ptr); /* dummy */
	func= RNA_struct_find_function(&ptr, "poll");

	list= RNA_parameter_list_create(&ptr, func);
	RNA_parameter_set_lookup(list, "context", &C);
	pt->py_call(&ptr, func, list);

	RNA_parameter_get_lookup(list, "visible", &ret);
	visible= *(int*)ret;

	RNA_parameter_list_free(list);

	return visible;
}

static char *enum_as_string(EnumPropertyItem *item)
{
	DynStr *dynstr= BLI_dynstr_new();
	EnumPropertyItem *e;
	char *cstring;

	for (e= item; item->identifier; item++) {
		BLI_dynstr_appendf(dynstr, (e==item)?"'%s'":", '%s'", item->identifier);
	}

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

static int space_region_type_from_panel(PanelType *pt, ReportList *reports, SpaceType **r_st, ARegionType **r_art)
{
	SpaceType *st;
	ARegionType *art;
	int space_value;
	int region_value;

	/* find the space type */
	if (RNA_enum_value_from_id(space_type_items, pt->space_type, &space_value)==0) {
		char *cstring= enum_as_string(space_type_items);
		BKE_reportf(reports, RPT_ERROR, "SpaceType \"%s\" is not one of [%s]", pt->space_type, cstring);
		MEM_freeN(cstring);
		return 0;
	}

	/* find the region type */
	if (RNA_enum_value_from_id(region_type_items, pt->region_type, &region_value)==0) {
		char *cstring= enum_as_string(region_type_items);
		BKE_reportf(reports, RPT_ERROR, "RegionType \"%s\" is not one of [%s]", pt->region_type, cstring);
		MEM_freeN(cstring);
		return 0;
	}

	st= BKE_spacetype_from_id(space_value);

	for(art= st->regiontypes.first; art; art= art->next) {
		if (art->regionid==region_value)
			break;
	}
	
	/* region type not found? abort */
	if (art==NULL) {
		BKE_reportf(reports, RPT_ERROR, "SpaceType \"%s\" does not have a UI region '%s'", pt->space_type, pt->region_type);
		return 0;
	}

	*r_st= st;
	*r_art= art;

	return 1;
}

static StructRNA *rna_Panel_register(const bContext *C, ReportList *reports, void *data, StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	SpaceType *st;
	ARegionType *art;
	PanelType *pt;
	Panel dummypanel;
	PanelType dummypt;
	PointerRNA dummyptr;
	int have_function[2];

	/* setup dummy panel & panel type to store static properties in */
	memset(&dummypanel, 0, sizeof(dummypanel));
	memset(&dummypt, 0, sizeof(dummypt));
	dummypanel.type= &dummypt;
	RNA_pointer_create(NULL, &RNA_Panel, &dummypanel, &dummyptr);

	/* validate the python class */
	if(validate(&dummyptr, data, have_function) != 0)
		return NULL;
	
	if(!space_region_type_from_panel(&dummypt, reports, &st, &art))
		return NULL;

	/* check if we have registered this panel type before */
	for(pt=art->paneltypes.first; pt; pt=pt->next)
		if(strcmp(pt->idname, dummypt.idname) == 0)
			break;

	/* create a new panel type if needed, otherwise we overwrite */
	if(!pt) {
		pt= MEM_callocN(sizeof(PanelType), "python buttons panel");
		BLI_strncpy(pt->idname, dummypt.idname, sizeof(pt->idname));
		pt->py_srna= RNA_def_struct(&BLENDER_RNA, pt->idname, "Panel"); 
		RNA_struct_blender_type_set(pt->py_srna, pt);
		BLI_addtail(&art->paneltypes, pt);
	}

	BLI_strncpy(pt->label, dummypt.label, sizeof(pt->label));
	BLI_strncpy(pt->space_type, dummypt.space_type, sizeof(pt->space_type));
	BLI_strncpy(pt->region_type, dummypt.region_type, sizeof(pt->region_type));
	BLI_strncpy(pt->context, dummypt.context, sizeof(pt->context));

	pt->poll= (have_function[0])? panel_poll: NULL;
	pt->draw= (have_function[1])? panel_draw: NULL;

	pt->py_data= data;
	pt->py_call= call;
	pt->py_free= free;

	/* update while blender is running */
	if(C)
		WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
	
	return pt->py_srna;
}

static void rna_Panel_unregister(const bContext *C, StructRNA *type)
{
	SpaceType *st;
	ARegionType *art;
	PanelType *pt= RNA_struct_blender_type_get(type);

	if(!space_region_type_from_panel(pt, NULL, &st, &art))
		return;
	
	BLI_freelinkN(&art->paneltypes, pt);
	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	if(C)
		WM_event_add_notifier(C, NC_SCREEN|NA_EDITED, NULL);
}

static StructRNA* rna_Panel_refine(struct PointerRNA *ptr)
{
	Panel *pnl= (Panel*)ptr->data;
	return (pnl->type)? pnl->type->py_srna: &RNA_Panel;
}

#else

static void rna_def_ui_layout(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "UILayout", NULL);
	RNA_def_struct_sdna(srna, "uiLayout");
	RNA_def_struct_ui_text(srna, "UI Layout", "User interface layout in a panel or header.");

	RNA_api_ui_layout(srna);
}

static void rna_def_panel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna= RNA_def_struct(brna, "Panel", NULL);
	RNA_def_struct_ui_text(srna, "Panel", "Buttons panel.");
	RNA_def_struct_sdna(srna, "Panel");
	RNA_def_struct_refine_func(srna, "rna_Panel_refine");
	RNA_def_struct_register_funcs(srna, "rna_Panel_register", "rna_Panel_unregister");

	func= RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Test if the panel is visible or not.");
	RNA_def_function_flag(func, FUNC_REGISTER|FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	RNA_def_pointer(func, "context", "Context", "", "");

	func= RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw buttons into the panel UI layout.");
	RNA_def_function_flag(func, FUNC_REGISTER);
	RNA_def_pointer(func, "context", "Context", "", "");

	prop= RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "UILayout");

	prop= RNA_def_property(srna, "idname", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_string_funcs(prop, "rna_Panel_idname_get", "rna_Panel_idname_length", "rna_Panel_idname_set");

	prop= RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_string_funcs(prop, "rna_Panel_label_get", "rna_Panel_label_length", "rna_Panel_label_set");

	prop= RNA_def_property(srna, "space_type", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_string_funcs(prop, "rna_Panel_space_type_get", "rna_Panel_space_type_length", "rna_Panel_space_type_set");

	prop= RNA_def_property(srna, "region_type", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_string_funcs(prop, "rna_Panel_region_type_get", "rna_Panel_region_type_length", "rna_Panel_region_type_set");

	prop= RNA_def_property(srna, "context", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_string_funcs(prop, "rna_Panel_context_get", "rna_Panel_context_length", "rna_Panel_context_set");
}

void RNA_def_ui(BlenderRNA *brna)
{
	rna_def_ui_layout(brna);
	rna_def_panel(brna);
}

#endif

