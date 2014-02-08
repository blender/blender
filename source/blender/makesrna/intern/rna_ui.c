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
 * Contributor(s): Blender Foundation (2009)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_ui.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "DNA_screen_types.h"

#include "BLF_translation.h"

#include "BKE_idprop.h"

#include "RNA_define.h"

#include "rna_internal.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"

#include "WM_types.h"

/* see WM_types.h */
EnumPropertyItem operator_context_items[] = {
	{WM_OP_INVOKE_DEFAULT, "INVOKE_DEFAULT", 0, "Invoke Default", ""},
	{WM_OP_INVOKE_REGION_WIN, "INVOKE_REGION_WIN", 0, "Invoke Region Window", ""},
	{WM_OP_INVOKE_REGION_CHANNELS, "INVOKE_REGION_CHANNELS", 0, "Invoke Region Channels", ""},
	{WM_OP_INVOKE_REGION_PREVIEW, "INVOKE_REGION_PREVIEW", 0, "Invoke Region Preview", ""},
	{WM_OP_INVOKE_AREA, "INVOKE_AREA", 0, "Invoke Area", ""},
	{WM_OP_INVOKE_SCREEN, "INVOKE_SCREEN", 0, "Invoke Screen", ""},
	{WM_OP_EXEC_DEFAULT, "EXEC_DEFAULT", 0, "Exec Default", ""},
	{WM_OP_EXEC_REGION_WIN, "EXEC_REGION_WIN", 0, "Exec Region Window", ""},
	{WM_OP_EXEC_REGION_CHANNELS, "EXEC_REGION_CHANNELS", 0, "Exec Region Channels", ""},
	{WM_OP_EXEC_REGION_PREVIEW, "EXEC_REGION_PREVIEW", 0, "Exec Region Preview", ""},
	{WM_OP_EXEC_AREA, "EXEC_AREA", 0, "Exec Area", ""},
	{WM_OP_EXEC_SCREEN, "EXEC_SCREEN", 0, "Exec Screen", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem uilist_layout_type_items[] = {
	{UILST_LAYOUT_DEFAULT, "DEFAULT", 0, "Default Layout", "Use the default, multi-rows layout"},
	{UILST_LAYOUT_COMPACT, "COMPACT", 0, "Compact Layout", "Use the compact, single-row layout"},
	{UILST_LAYOUT_GRID, "GRID", 0, "Grid Layout", "Use the grid-based layout"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include <assert.h>

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "WM_api.h"

static ARegionType *region_type_find(ReportList *reports, int space_type, int region_type)
{
	SpaceType *st;
	ARegionType *art;

	st = BKE_spacetype_from_id(space_type);

	for (art = (st) ? st->regiontypes.first : NULL; art; art = art->next) {
		if (art->regionid == region_type)
			break;
	}
	
	/* region type not found? abort */
	if (art == NULL) {
		BKE_report(reports, RPT_ERROR, "Region not found in space type");
		return NULL;
	}

	return art;
}

/* Panel */

static int panel_poll(const bContext *C, PanelType *pt)
{
	extern FunctionRNA rna_Panel_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, pt->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_Panel_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	pt->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static void panel_draw(const bContext *C, Panel *pnl)
{
	extern FunctionRNA rna_Panel_draw_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, pnl->type->ext.srna, pnl, &ptr);
	func = &rna_Panel_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	pnl->type->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void panel_draw_header(const bContext *C, Panel *pnl)
{
	extern FunctionRNA rna_Panel_draw_header_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, pnl->type->ext.srna, pnl, &ptr);
	func = &rna_Panel_draw_header_func; /* RNA_struct_find_function(&ptr, "draw_header"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	pnl->type->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Panel_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	ARegionType *art;
	PanelType *pt = RNA_struct_blender_type_get(type);

	if (!pt)
		return;
	if (!(art = region_type_find(NULL, pt->space_type, pt->region_type)))
		return;
	
	RNA_struct_free_extension(type, &pt->ext);

	BLI_freelinkN(&art->paneltypes, pt);
	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
}

static StructRNA *rna_Panel_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                     StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	ARegionType *art;
	PanelType *pt, dummypt = {NULL};
	Panel dummypanel = {NULL};
	PointerRNA dummyptr;
	int have_function[3];

	/* setup dummy panel & panel type to store static properties in */
	dummypanel.type = &dummypt;
	RNA_pointer_create(NULL, &RNA_Panel, &dummypanel, &dummyptr);

	/* We have to set default context! Else we get a void string... */
	strcpy(dummypt.translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;
		
	if (strlen(identifier) >= sizeof(dummypt.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering panel class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummypt.idname));
		return NULL;
	}
	
	if (!(art = region_type_find(reports, dummypt.space_type, dummypt.region_type)))
		return NULL;

	/* check if we have registered this panel type before, and remove it */
	for (pt = art->paneltypes.first; pt; pt = pt->next) {
		if (strcmp(pt->idname, dummypt.idname) == 0) {
			if (pt->ext.srna)
				rna_Panel_unregister(bmain, pt->ext.srna);
			else
				BLI_freelinkN(&art->paneltypes, pt);
			break;
		}
	}
	
	/* create a new panel type */
	pt = MEM_callocN(sizeof(PanelType), "python buttons panel");
	memcpy(pt, &dummypt, sizeof(dummypt));

	pt->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, pt->idname, &RNA_Panel);
	RNA_def_struct_translation_context(pt->ext.srna, pt->translation_context);
	pt->ext.data = data;
	pt->ext.call = call;
	pt->ext.free = free;
	RNA_struct_blender_type_set(pt->ext.srna, pt);
	RNA_def_struct_flag(pt->ext.srna, STRUCT_NO_IDPROPERTIES);

	pt->poll = (have_function[0]) ? panel_poll : NULL;
	pt->draw = (have_function[1]) ? panel_draw : NULL;
	pt->draw_header = (have_function[2]) ? panel_draw_header : NULL;

	/* XXX use "no header" flag for some ordering of panels until we have real panel ordering */
	if (pt->flag & PNL_NO_HEADER) {
		PanelType *pth = art->paneltypes.first;
		while (pth && pth->flag & PNL_NO_HEADER)
			pth = pth->next;

		if (pth)
			BLI_insertlinkbefore(&art->paneltypes, pth, pt);
		else
			BLI_addtail(&art->paneltypes, pt);
	}
	else
		BLI_addtail(&art->paneltypes, pt);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
	
	return pt->ext.srna;
}

static StructRNA *rna_Panel_refine(PointerRNA *ptr)
{
	Panel *hdr = (Panel *)ptr->data;
	return (hdr->type && hdr->type->ext.srna) ? hdr->type->ext.srna : &RNA_Panel;
}

/* UIList */
static unsigned int rna_UIList_filter_const_FILTER_ITEM_get(PointerRNA *UNUSED(ptr))
{
	return UILST_FLT_ITEM;
}

static IDProperty *rna_UIList_idprops(PointerRNA *ptr, bool create)
{
	uiList *ui_list = (uiList *)ptr->data;
	if (create && !ui_list->properties) {
		IDPropertyTemplate val = {0};
		ui_list->properties = IDP_New(IDP_GROUP, &val, "RNA_UIList IDproperties group");
	}

	return ui_list->properties;
}

static void uilist_draw_item(uiList *ui_list, bContext *C, uiLayout *layout, PointerRNA *dataptr, PointerRNA *itemptr,
                             int icon, PointerRNA *active_dataptr, const char *active_propname, int index, int flt_flag)
{
	extern FunctionRNA rna_UIList_draw_item_func;

	PointerRNA ul_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, ui_list->type->ext.srna, ui_list, &ul_ptr);
	func = &rna_UIList_draw_item_func; /* RNA_struct_find_function(&ul_ptr, "draw_item"); */

	RNA_parameter_list_create(&list, &ul_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "layout", &layout);
	RNA_parameter_set_lookup(&list, "data", dataptr);
	RNA_parameter_set_lookup(&list, "item", itemptr);
	RNA_parameter_set_lookup(&list, "icon", &icon);
	RNA_parameter_set_lookup(&list, "active_data", active_dataptr);
	RNA_parameter_set_lookup(&list, "active_property", &active_propname);
	RNA_parameter_set_lookup(&list, "index", &index);
	RNA_parameter_set_lookup(&list, "flt_flag", &flt_flag);
	ui_list->type->ext.call((bContext *)C, &ul_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void uilist_draw_filter(uiList *ui_list, bContext *C, uiLayout *layout)
{
	extern FunctionRNA rna_UIList_draw_filter_func;

	PointerRNA ul_ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, ui_list->type->ext.srna, ui_list, &ul_ptr);
	func = &rna_UIList_draw_filter_func; /* RNA_struct_find_function(&ul_ptr, "draw_filter"); */

	RNA_parameter_list_create(&list, &ul_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "layout", &layout);
	ui_list->type->ext.call((bContext *)C, &ul_ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void uilist_filter_items(uiList *ui_list, bContext *C, PointerRNA *dataptr, const char *propname)
{
	extern FunctionRNA rna_UIList_filter_items_func;

	PointerRNA ul_ptr;
	ParameterList list;
	FunctionRNA *func;
	PropertyRNA *parm;

	uiListDyn *flt_data = ui_list->dyn_data;
	int *filter_flags, *filter_neworder;
	void *ret1, *ret2;
	int ret_len;
	int len = flt_data->items_len = RNA_collection_length(dataptr, propname);

	RNA_pointer_create(&CTX_wm_screen(C)->id, ui_list->type->ext.srna, ui_list, &ul_ptr);
	func = &rna_UIList_filter_items_func; /* RNA_struct_find_function(&ul_ptr, "filter_items"); */

	RNA_parameter_list_create(&list, &ul_ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "data", dataptr);
	RNA_parameter_set_lookup(&list, "property", &propname);

	ui_list->type->ext.call((bContext *)C, &ul_ptr, func, &list);

	parm = RNA_function_find_parameter(NULL, func, "filter_flags");
	ret_len = RNA_parameter_dynamic_length_get(&list, parm);
	if (ret_len != len && ret_len != 0) {
		printf("%s: Error, py func returned %d items in %s, %d or none were expected.\n", __func__,
		       RNA_parameter_dynamic_length_get(&list, parm), "filter_flags", len);
		/* Note: we cannot return here, we would let flt_data in inconsistent state... see T38356. */
		filter_flags = NULL;
	}
	else {
		RNA_parameter_get(&list, parm, &ret1);
		filter_flags = (int *)ret1;
	}

	parm = RNA_function_find_parameter(NULL, func, "filter_neworder");
	ret_len = RNA_parameter_dynamic_length_get(&list, parm);
	if (ret_len != len && ret_len != 0) {
		printf("%s: Error, py func returned %d items in %s, %d or none were expected.\n", __func__,
		       RNA_parameter_dynamic_length_get(&list, parm), "filter_neworder", len);
		/* Note: we cannot return here, we would let flt_data in inconsistent state... see T38356. */
		filter_neworder = NULL;
	}
	else {
		RNA_parameter_get(&list, parm, &ret2);
		filter_neworder = (int *)ret2;
	}

	/* We have to do some final checks and transforms... */
	{
		int i, filter_exclude = ui_list->filter_flag & UILST_FLT_EXCLUDE;
		if (filter_flags) {
			flt_data->items_filter_flags = MEM_mallocN(sizeof(int) * len, __func__);
			memcpy(flt_data->items_filter_flags, filter_flags, sizeof(int) * len);

			if (filter_neworder) {
				/* For sake of simplicity, py filtering is expected to filter all items, but we actually only want
				 * reordering data for shown items!
				 */
				int items_shown, shown_idx;
				int t_idx, t_ni, prev_ni;
				flt_data->items_shown = 0;
				for (i = 0, shown_idx = 0; i < len; i++) {
					if ((filter_flags[i] & UILST_FLT_ITEM) ^ filter_exclude) {
						filter_neworder[shown_idx++] = filter_neworder[i];
					}
				}
				items_shown = flt_data->items_shown = shown_idx;
				flt_data->items_filter_neworder = MEM_mallocN(sizeof(int) * items_shown, __func__);
				/* And now, bring back new indices into the [0, items_shown[ range!
				 * XXX This is O(N²)... :/
				 */
				for (shown_idx = 0, prev_ni = -1; shown_idx < items_shown; shown_idx++) {
					for (i = 0, t_ni = len, t_idx = -1; i < items_shown; i++) {
						int ni = filter_neworder[i];
						if (ni > prev_ni && ni < t_ni) {
							t_idx = i;
							t_ni = ni;
						}
					}
					if (t_idx >= 0) {
						prev_ni = t_ni;
						flt_data->items_filter_neworder[t_idx] = shown_idx;
					}
				}
			}
			else {
				/* we still have to set flt_data->items_shown... */
				flt_data->items_shown = 0;
				for (i = 0; i < len; i++) {
					if ((filter_flags[i] & UILST_FLT_ITEM) ^ filter_exclude) {
						flt_data->items_shown++;
					}
				}
			}
		}
		else {
			flt_data->items_shown = len;

			if (filter_neworder) {
				flt_data->items_filter_neworder = MEM_mallocN(sizeof(int) * len, __func__);
				memcpy(flt_data->items_filter_neworder, filter_neworder, sizeof(int) * len);
			}
		}
	}

	RNA_parameter_list_free(&list);
}

static void rna_UIList_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	uiListType *ult = RNA_struct_blender_type_get(type);

	if (!ult)
		return;

	RNA_struct_free_extension(type, &ult->ext);

	WM_uilisttype_freelink(ult);

	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
}

static StructRNA *rna_UIList_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                      StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	uiListType *ult, dummyult = {NULL};
	uiList dummyuilist = {NULL};
	PointerRNA dummyul_ptr;
	int have_function[3];
	size_t over_alloc = 0; /* warning, if this becomes a bess, we better do another alloc */

	/* setup dummy menu & menu type to store static properties in */
	dummyuilist.type = &dummyult;
	RNA_pointer_create(NULL, &RNA_UIList, &dummyuilist, &dummyul_ptr);

	/* validate the python class */
	if (validate(&dummyul_ptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummyult.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering uilist class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyult.idname));
		return NULL;
	}

	/* check if we have registered this uilist type before, and remove it */
	ult = WM_uilisttype_find(dummyult.idname, TRUE);
	if (ult && ult->ext.srna)
		rna_UIList_unregister(bmain, ult->ext.srna);

	/* create a new menu type */
	ult = MEM_callocN(sizeof(uiListType) + over_alloc, "python uilist");
	memcpy(ult, &dummyult, sizeof(dummyult));

	ult->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ult->idname, &RNA_UIList);
	ult->ext.data = data;
	ult->ext.call = call;
	ult->ext.free = free;
	RNA_struct_blender_type_set(ult->ext.srna, ult);

	ult->draw_item = (have_function[0]) ? uilist_draw_item : NULL;
	ult->draw_filter = (have_function[1]) ? uilist_draw_filter : NULL;
	ult->filter_items = (have_function[2]) ? uilist_filter_items : NULL;

	WM_uilisttype_add(ult);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);

	return ult->ext.srna;
}

static StructRNA *rna_UIList_refine(PointerRNA *ptr)
{
	uiList *ui_list = (uiList *)ptr->data;
	return (ui_list->type && ui_list->type->ext.srna) ? ui_list->type->ext.srna : &RNA_UIList;
}

/* Header */

static void header_draw(const bContext *C, Header *hdr)
{
	extern FunctionRNA rna_Header_draw_func;

	PointerRNA htr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, hdr->type->ext.srna, hdr, &htr);
	func = &rna_Header_draw_func; /* RNA_struct_find_function(&htr, "draw"); */

	RNA_parameter_list_create(&list, &htr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	hdr->type->ext.call((bContext *)C, &htr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Header_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	ARegionType *art;
	HeaderType *ht = RNA_struct_blender_type_get(type);

	if (!ht)
		return;
	if (!(art = region_type_find(NULL, ht->space_type, RGN_TYPE_HEADER)))
		return;
	
	RNA_struct_free_extension(type, &ht->ext);

	BLI_freelinkN(&art->headertypes, ht);
	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
}

static StructRNA *rna_Header_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                      StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	ARegionType *art;
	HeaderType *ht, dummyht = {NULL};
	Header dummyheader = {NULL};
	PointerRNA dummyhtr;
	int have_function[1];

	/* setup dummy header & header type to store static properties in */
	dummyheader.type = &dummyht;
	RNA_pointer_create(NULL, &RNA_Header, &dummyheader, &dummyhtr);

	/* validate the python class */
	if (validate(&dummyhtr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummyht.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering header class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyht.idname));
		return NULL;
	}

	if (!(art = region_type_find(reports, dummyht.space_type, RGN_TYPE_HEADER)))
		return NULL;

	/* check if we have registered this header type before, and remove it */
	for (ht = art->headertypes.first; ht; ht = ht->next) {
		if (strcmp(ht->idname, dummyht.idname) == 0) {
			if (ht->ext.srna)
				rna_Header_unregister(bmain, ht->ext.srna);
			break;
		}
	}
	
	/* create a new header type */
	ht = MEM_callocN(sizeof(HeaderType), "python buttons header");
	memcpy(ht, &dummyht, sizeof(dummyht));

	ht->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ht->idname, &RNA_Header);
	ht->ext.data = data;
	ht->ext.call = call;
	ht->ext.free = free;
	RNA_struct_blender_type_set(ht->ext.srna, ht);

	ht->draw = (have_function[0]) ? header_draw : NULL;

	BLI_addtail(&art->headertypes, ht);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
	
	return ht->ext.srna;
}

static StructRNA *rna_Header_refine(PointerRNA *htr)
{
	Header *hdr = (Header *)htr->data;
	return (hdr->type && hdr->type->ext.srna) ? hdr->type->ext.srna : &RNA_Header;
}

/* Menu */

static int menu_poll(const bContext *C, MenuType *pt)
{
	extern FunctionRNA rna_Menu_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, pt->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_Menu_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	pt->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static void menu_draw(const bContext *C, Menu *hdr)
{
	extern FunctionRNA rna_Menu_draw_func;

	PointerRNA mtr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(&CTX_wm_screen(C)->id, hdr->type->ext.srna, hdr, &mtr);
	func = &rna_Menu_draw_func; /* RNA_struct_find_function(&mtr, "draw"); */

	RNA_parameter_list_create(&list, &mtr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	hdr->type->ext.call((bContext *)C, &mtr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Menu_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	MenuType *mt = RNA_struct_blender_type_get(type);

	if (!mt)
		return;
	
	RNA_struct_free_extension(type, &mt->ext);

	WM_menutype_freelink(mt);

	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
}

static StructRNA *rna_Menu_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                    StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	MenuType *mt, dummymt = {NULL};
	Menu dummymenu = {NULL};
	PointerRNA dummymtr;
	int have_function[2];
	size_t over_alloc = 0; /* warning, if this becomes a bess, we better do another alloc */
	size_t description_size = 0;
	char _menu_descr[RNA_DYN_DESCR_MAX];

	/* setup dummy menu & menu type to store static properties in */
	dummymenu.type = &dummymt;
	_menu_descr[0] = '\0';
	dummymenu.type->description = _menu_descr;
	RNA_pointer_create(NULL, &RNA_Menu, &dummymenu, &dummymtr);

	/* We have to set default context! Else we get a void string... */
	strcpy(dummymt.translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);

	/* validate the python class */
	if (validate(&dummymtr, data, have_function) != 0)
		return NULL;
	
	if (strlen(identifier) >= sizeof(dummymt.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering menu class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummymt.idname));
		return NULL;
	}

	/* check if we have registered this menu type before, and remove it */
	mt = WM_menutype_find(dummymt.idname, TRUE);
	if (mt && mt->ext.srna)
		rna_Menu_unregister(bmain, mt->ext.srna);
	
	/* create a new menu type */
	if (_menu_descr[0]) {
		description_size = strlen(_menu_descr) + 1;
		over_alloc += description_size;
	}

	mt = MEM_callocN(sizeof(MenuType) + over_alloc, "python buttons menu");
	memcpy(mt, &dummymt, sizeof(dummymt));

	if (_menu_descr[0]) {
		char *buf = (char *)(mt + 1);
		memcpy(buf, _menu_descr, description_size);
		mt->description = buf;
	}
	else
		mt->description = "";

	mt->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, mt->idname, &RNA_Menu);
	RNA_def_struct_translation_context(mt->ext.srna, mt->translation_context);
	mt->ext.data = data;
	mt->ext.call = call;
	mt->ext.free = free;
	RNA_struct_blender_type_set(mt->ext.srna, mt);
	RNA_def_struct_flag(mt->ext.srna, STRUCT_NO_IDPROPERTIES);

	mt->poll = (have_function[0]) ? menu_poll : NULL;
	mt->draw = (have_function[1]) ? menu_draw : NULL;

	WM_menutype_add(mt);

	/* update while blender is running */
	WM_main_add_notifier(NC_WINDOW, NULL);
	
	return mt->ext.srna;
}

static StructRNA *rna_Menu_refine(PointerRNA *mtr)
{
	Menu *hdr = (Menu *)mtr->data;
	return (hdr->type && hdr->type->ext.srna) ? hdr->type->ext.srna : &RNA_Menu;
}

static void rna_Menu_bl_description_set(PointerRNA *ptr, const char *value)
{
	Menu *data = (Menu *)(ptr->data);
	char *str = (char *)data->type->description;
	if (!str[0]) BLI_strncpy(str, value, RNA_DYN_DESCR_MAX);     /* utf8 already ensured */
	else assert(!"setting the bl_description on a non-builtin menu");
}

/* UILayout */

static int rna_UILayout_active_get(PointerRNA *ptr)
{
	return uiLayoutGetActive(ptr->data);
}

static void rna_UILayout_active_set(PointerRNA *ptr, int value)
{
	uiLayoutSetActive(ptr->data, value);
}

static int rna_UILayout_alert_get(PointerRNA *ptr)
{
	return uiLayoutGetRedAlert(ptr->data);
}

static void rna_UILayout_alert_set(PointerRNA *ptr, int value)
{
	uiLayoutSetRedAlert(ptr->data, value);
}

static void rna_UILayout_op_context_set(PointerRNA *ptr, int value)
{
	uiLayoutSetOperatorContext(ptr->data, value);
}

static int rna_UILayout_op_context_get(PointerRNA *ptr)
{
	return uiLayoutGetOperatorContext(ptr->data);
}

static int rna_UILayout_enabled_get(PointerRNA *ptr)
{
	return uiLayoutGetEnabled(ptr->data);
}

static void rna_UILayout_enabled_set(PointerRNA *ptr, int value)
{
	uiLayoutSetEnabled(ptr->data, value);
}

#if 0
static int rna_UILayout_red_alert_get(PointerRNA *ptr)
{
	return uiLayoutGetRedAlert(ptr->data);
}

static void rna_UILayout_red_alert_set(PointerRNA *ptr, int value)
{
	uiLayoutSetRedAlert(ptr->data, value);
}

static int rna_UILayout_keep_aspect_get(PointerRNA *ptr)
{
	return uiLayoutGetKeepAspect(ptr->data);
}

static void rna_UILayout_keep_aspect_set(PointerRNA *ptr, int value)
{
	uiLayoutSetKeepAspect(ptr->data, value);
}
#endif

static int rna_UILayout_alignment_get(PointerRNA *ptr)
{
	return uiLayoutGetAlignment(ptr->data);
}

static void rna_UILayout_alignment_set(PointerRNA *ptr, int value)
{
	uiLayoutSetAlignment(ptr->data, value);
}

static float rna_UILayout_scale_x_get(PointerRNA *ptr)
{
	return uiLayoutGetScaleX(ptr->data);
}

static void rna_UILayout_scale_x_set(PointerRNA *ptr, float value)
{
	uiLayoutSetScaleX(ptr->data, value);
}

static float rna_UILayout_scale_y_get(PointerRNA *ptr)
{
	return uiLayoutGetScaleY(ptr->data);
}

static void rna_UILayout_scale_y_set(PointerRNA *ptr, float value)
{
	uiLayoutSetScaleY(ptr->data, value);
}

#else /* RNA_RUNTIME */

static void rna_def_ui_layout(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem alignment_items[] = {
		{UI_LAYOUT_ALIGN_EXPAND, "EXPAND", 0, "Expand", ""},
		{UI_LAYOUT_ALIGN_LEFT, "LEFT", 0, "Left", ""},
		{UI_LAYOUT_ALIGN_CENTER, "CENTER", 0, "Center", ""},
		{UI_LAYOUT_ALIGN_RIGHT, "RIGHT", 0, "Right", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* layout */

	srna = RNA_def_struct(brna, "UILayout", NULL);
	RNA_def_struct_sdna(srna, "uiLayout");
	RNA_def_struct_ui_text(srna, "UI Layout", "User interface layout in a panel or header");

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_UILayout_active_get", "rna_UILayout_active_set");
	
	prop = RNA_def_property(srna, "operator_context", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, operator_context_items);
	RNA_def_property_enum_funcs(prop, "rna_UILayout_op_context_get", "rna_UILayout_op_context_set", NULL);
	
	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_UILayout_enabled_get", "rna_UILayout_enabled_set");
	RNA_def_property_ui_text(prop, "Enabled", "When false, this (sub)layout is grayed out");
	
	prop = RNA_def_property(srna, "alert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_UILayout_alert_get", "rna_UILayout_alert_set");

	prop = RNA_def_property(srna, "alignment", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, alignment_items);
	RNA_def_property_enum_funcs(prop, "rna_UILayout_alignment_get", "rna_UILayout_alignment_set", NULL);

#if 0
	prop = RNA_def_property(srna, "keep_aspect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_UILayout_keep_aspect_get", "rna_UILayout_keep_aspect_set");
#endif

	prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_funcs(prop, "rna_UILayout_scale_x_get", "rna_UILayout_scale_x_set", NULL);
	RNA_def_property_ui_text(prop, "Scale X", "Scale factor along the X for items in this (sub)layout");
	
	prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_funcs(prop, "rna_UILayout_scale_y_get", "rna_UILayout_scale_y_set", NULL);
	RNA_def_property_ui_text(prop, "Scale Y", "Scale factor along the Y for items in this (sub)layout");
	RNA_api_ui_layout(srna);
}

static void rna_def_panel(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;
	
	static EnumPropertyItem panel_flag_items[] = {
		{PNL_DEFAULT_CLOSED, "DEFAULT_CLOSED", 0, "Default Closed",
		                     "Defines if the panel has to be open or collapsed at the time of its creation"},
		{PNL_NO_HEADER, "HIDE_HEADER", 0, "Hide Header",
		                "If set to False, the panel shows a header, which contains a clickable "
		                "arrow to collapse the panel and the label (see bl_label)"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "Panel", NULL);
	RNA_def_struct_ui_text(srna, "Panel", "Panel containing UI elements");
	RNA_def_struct_sdna(srna, "Panel");
	RNA_def_struct_refine_func(srna, "rna_Panel_refine");
	RNA_def_struct_register_funcs(srna, "rna_Panel_register", "rna_Panel_unregister", NULL);
	RNA_def_struct_translation_context(srna, BLF_I18NCONTEXT_DEFAULT_BPYRNA);

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "If this method returns a non-null output, then the panel can be drawn");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw UI elements into the panel UI layout");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	func = RNA_def_function(srna, "draw_header", NULL);
	RNA_def_function_ui_description(func, "Draw UI elements into the panel's header UI layout");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "UILayout");
	RNA_def_property_ui_text(prop, "Layout", "Defines the structure of the panel in the UI");
	
	prop = RNA_def_property(srna, "text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "drawname");
	RNA_def_property_ui_text(prop, "Text", "XXX todo");
	
	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name",
	                         "If this is set, the panel gets a custom ID, otherwise it takes the "
	                         "name of the class used to define the panel. For example, if the "
	                         "class name is \"OBJECT_PT_hello\", and bl_idname is not set by the "
	                         "script, then bl_idname = \"OBJECT_PT_hello\"");

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->label");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Label",
	                         "The panel label, shows up in the panel header at the right of the "
	                         "triangle used to collapse the panel");

	prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->translation_context");
	RNA_def_property_string_default(prop, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_define_verify_sdna(TRUE);

	prop = RNA_def_property(srna, "bl_category", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->category");
	RNA_def_property_string_default(prop, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->space_type");
	RNA_def_property_enum_items(prop, space_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Space type", "The space where the panel is going to be used in");
	
	prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->region_type");
	RNA_def_property_enum_items(prop, region_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Region Type", "The region where the panel is going to be used in");

	prop = RNA_def_property(srna, "bl_context", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->context");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL); /* Only used in Properties Editor and 3D View - Thomas */
	RNA_def_property_ui_text(prop, "Context",
	                         "The context in which the panel belongs to. (TODO: explain the "
	                         "possible combinations bl_context/bl_region_type/bl_space_type)");
	
	prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->flag");
	RNA_def_property_enum_items(prop, panel_flag_items);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Options",  "Options for this panel type");

	prop = RNA_def_property(srna, "use_pin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PNL_PIN);
	RNA_def_property_ui_text(prop, "Pin",  "");
	/* XXX, should only tag region for redraw */
	RNA_def_property_update(prop, NC_WINDOW, NULL);
}

static void rna_def_uilist(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;

	srna = RNA_def_struct(brna, "UIList", NULL);
	RNA_def_struct_ui_text(srna, "UIList", "UI list containing the elements of a collection");
	RNA_def_struct_sdna(srna, "uiList");
	RNA_def_struct_refine_func(srna, "rna_UIList_refine");
	RNA_def_struct_register_funcs(srna, "rna_UIList_register", "rna_UIList_unregister", NULL);
	RNA_def_struct_idprops_func(srna, "rna_UIList_idprops");

	/* Registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name",
	                         "If this is set, the uilist gets a custom ID, otherwise it takes the "
	                         "name of the class used to define the uilist (for example, if the "
	                         "class name is \"OBJECT_UL_vgroups\", and bl_idname is not set by the "
	                         "script, then bl_idname = \"OBJECT_UL_vgroups\")");

	/* Data */
	prop = RNA_def_property(srna, "layout_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, uilist_layout_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* Filter options */
	prop = RNA_def_property(srna, "use_filter_show", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter_flag", UILST_FLT_SHOW);
	RNA_def_property_ui_text(prop, "Show Filter", "Show filtering options");

	prop = RNA_def_property(srna, "filter_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "filter_byname");
	RNA_def_property_ui_text(prop, "Filter by Name", "Only show items matching this name (use '*' as wildcard)");

	prop = RNA_def_property(srna, "use_filter_invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter_flag", UILST_FLT_EXCLUDE);
	RNA_def_property_ui_text(prop, "Invert", "Invert filtering (show hidden items, and vice-versa)");

	prop = RNA_def_property(srna, "use_filter_sort_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter_sort_flag", UILST_FLT_SORT_ALPHA);
	RNA_def_property_ui_icon(prop, ICON_SORTALPHA, 0);
	RNA_def_property_ui_text(prop, "Sort by Name", "Sort items by their name");

	prop = RNA_def_property(srna, "use_filter_sort_reverse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter_sort_flag", UILST_FLT_SORT_REVERSE);
	RNA_def_property_ui_text(prop, "Invert", "Invert the order of shown items");

	/* draw_item */
	func = RNA_def_function(srna, "draw_item", NULL);
	RNA_def_function_ui_description(func, "Draw an item in the list (NOTE: when you define your own draw_item "
	                                      "function, you may want to check given 'item' is of the right type...)");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Layout to draw the item");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take Collection property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	parm = RNA_def_pointer(func, "item", "AnyType", "", "Item of the collection property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	parm = RNA_def_int(func, "icon", 0, 0, INT_MAX, "", "Icon of the item in the collection", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "active_data", "AnyType", "",
	                       "Data from which to take property for the active element");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_string(func, "active_property", NULL, 0, "",
	                      "Identifier of property in active_data, for the active element");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_int(func, "index", 0, 0, INT_MAX, "", "Index of the item in the collection", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_PYFUNC_OPTIONAL);
	prop = RNA_def_property(func, "flt_flag", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "", "The filter-flag result for this item");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_PYFUNC_OPTIONAL);

	/* draw_filter */
	func = RNA_def_function(srna, "draw_filter", NULL);
	RNA_def_function_ui_description(func, "Draw filtering options");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Layout to draw the item");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* filter */
	func = RNA_def_function(srna, "filter_items", NULL);
	RNA_def_function_ui_description(func, "Filter and/or re-order items of the collection (output filter results in "
	                                      "filter_flags, and reorder results in filter_neworder arrays)");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take Collection property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in data, for the collection");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	prop = RNA_def_property(func, "filter_flags", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_flag(prop, PROP_REQUIRED | PROP_DYNAMIC);
	RNA_def_property_array(prop, 1);  /* XXX Dummy value, default 0 does not work */
	RNA_def_property_ui_text(prop, "", "An array of filter flags, one for each item in the collection (NOTE: "
	                                   "FILTER_ITEM bit is reserved, it defines whether the item is shown or not)");
	RNA_def_function_output(func, prop);
	prop = RNA_def_property(func, "filter_neworder", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_flag(prop, PROP_REQUIRED | PROP_DYNAMIC);
	RNA_def_property_array(prop, 1);  /* XXX Dummy value, default 0 does not work */
	RNA_def_property_ui_text(prop, "", "An array of indices, one for each item in the collection, mapping the org "
	                                   "index to the new one");
	RNA_def_function_output(func, prop);

	/* "Constants"! */
	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "bitflag_filter_item", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "FILTER_ITEM",
	                               "The value of the reserved bitflag 'FILTER_ITEM' (in filter_flags values)");
	RNA_def_property_int_funcs(prop, "rna_UIList_filter_const_FILTER_ITEM_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_header(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "Header", NULL);
	RNA_def_struct_ui_text(srna, "Header", "Editor header containing UI elements");
	RNA_def_struct_sdna(srna, "Header");
	RNA_def_struct_refine_func(srna, "rna_Header_refine");
	RNA_def_struct_register_funcs(srna, "rna_Header_register", "rna_Header_unregister", NULL);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw UI elements into the header UI layout");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	RNA_define_verify_sdna(0); /* not in sdna */

	prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "layout");
	RNA_def_property_struct_type(prop, "UILayout");
	RNA_def_property_ui_text(prop, "Layout", "Structure of the header in the UI");

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name",
	                         "If this is set, the header gets a custom ID, otherwise it takes the "
	                         "name of the class used to define the panel; for example, if the "
	                         "class name is \"OBJECT_HT_hello\", and bl_idname is not set by the "
	                         "script, then bl_idname = \"OBJECT_HT_hello\"");

	prop = RNA_def_property(srna, "bl_space_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type->space_type");
	RNA_def_property_enum_items(prop, space_type_items);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Space type", "The space where the header is going to be used in");

	RNA_define_verify_sdna(1);
}

static void rna_def_menu(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "Menu", NULL);
	RNA_def_struct_ui_text(srna, "Menu", "Editor menu containing buttons");
	RNA_def_struct_sdna(srna, "Menu");
	RNA_def_struct_refine_func(srna, "rna_Menu_refine");
	RNA_def_struct_register_funcs(srna, "rna_Menu_register", "rna_Menu_unregister", NULL);
	RNA_def_struct_translation_context(srna, BLF_I18NCONTEXT_DEFAULT_BPYRNA);

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "If this method returns a non-null output, then the menu can be drawn");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", 1, "", ""));
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* draw */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw UI elements into the menu UI layout");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	RNA_define_verify_sdna(FALSE); /* not in sdna */

	prop = RNA_def_property(srna, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "layout");
	RNA_def_property_struct_type(prop, "UILayout");
	RNA_def_property_ui_text(prop, "Layout", "Defines the structure of the menu in the UI");

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name",
	                         "If this is set, the menu gets a custom ID, otherwise it takes the "
	                         "name of the class used to define the menu (for example, if the "
	                         "class name is \"OBJECT_MT_hello\", and bl_idname is not set by the "
	                         "script, then bl_idname = \"OBJECT_MT_hello\")");

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->label");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Label", "The menu label");

	prop = RNA_def_property(srna, "bl_translation_context", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->translation_context");
	RNA_def_property_string_default(prop, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->description");
	RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Menu_bl_description_set");
	/* RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_clear_flag(prop, PROP_NEVER_NULL); /* check for NULL */

	RNA_define_verify_sdna(1);
}

void RNA_def_ui(BlenderRNA *brna)
{
	rna_def_ui_layout(brna);
	rna_def_panel(brna);
	rna_def_uilist(brna);
	rna_def_header(brna);
	rna_def_menu(brna);
}

#endif /* RNA_RUNTIME */

