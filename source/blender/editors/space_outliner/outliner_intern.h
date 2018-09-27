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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_intern.h
 *  \ingroup spoutliner
 */


#ifndef __OUTLINER_INTERN_H__
#define __OUTLINER_INTERN_H__

#include "RNA_types.h"

/* internal exports only */

struct wmOperatorType;
struct TreeStoreElem;
struct bContext;
struct Scene;
struct ID;
struct Object;
struct bPoseChannel;
struct EditBone;

typedef struct TreeElement {
	struct TreeElement *next, *prev, *parent;
	ListBase subtree;
	int xs, ys;                // do selection
	TreeStoreElem *store_elem; // element in tree store
	short flag;                // flag for non-saved stuff
	short index;               // index for data arrays
	short idcode;              // from TreeStore id
	short xend;                // width of item display, for select
	const char *name;
	void *directdata;          // Armature Bones, Base, Sequence, Strip...
	PointerRNA rnaptr;         // RNA Pointer
}  TreeElement;

#define TREESTORE_ID_TYPE(_id) \
	(ELEM(GS((_id)->name), ID_SCE, ID_LI, ID_OB, ID_ME, ID_CU, ID_MB, ID_NT, ID_MA, ID_TE, ID_IM, ID_LT, ID_LA, ID_CA) || \
	 ELEM(GS((_id)->name), ID_KE, ID_WO, ID_SPK, ID_GR, ID_AR, ID_AC, ID_BR, ID_PA, ID_GD, ID_LS) || \
	 ELEM(GS((_id)->name), ID_SCR, ID_WM, ID_TXT, ID_VF, ID_SO, ID_CF, ID_PAL, ID_MC))  /* Only in 'blendfile' mode ... :/ */

/* TreeElement->flag */
#define TE_ACTIVE       1
#define TE_ICONROW      2
#define TE_LAZY_CLOSED  4
#define TE_FREE_NAME    8

/* button events */
#define OL_NAMEBUTTON       1

typedef enum {
	OL_DRAWSEL_NONE    = 0,  /* inactive (regular black text) */
	OL_DRAWSEL_NORMAL  = 1,  /* active object (draws white text) */
	OL_DRAWSEL_ACTIVE  = 2,  /* active obdata (draws a circle around the icon) */
} eOLDrawState;

typedef enum {
	OL_SETSEL_NONE     = 0,  /* don't change the selection state */
	OL_SETSEL_NORMAL   = 1,  /* select the item */
	OL_SETSEL_EXTEND   = 2,  /* select the item and extend (also toggles selection) */
} eOLSetState;

/* get TreeStoreElem associated with a TreeElement
 * < a: (TreeElement) tree element to find stored element for
 */
#define TREESTORE(a) ((a)->store_elem)

/* size constants */
#define OL_Y_OFFSET 2

#define OL_TOG_RESTRICT_VIEWX   (UI_UNIT_X * 3.0f)
#define OL_TOG_RESTRICT_SELECTX (UI_UNIT_X * 2.0f)
#define OL_TOG_RESTRICT_RENDERX UI_UNIT_X

#define OL_TOGW OL_TOG_RESTRICT_VIEWX

#define OL_RNA_COLX         (UI_UNIT_X * 15)
#define OL_RNA_COL_SIZEX    (UI_UNIT_X * 7.5f)
#define OL_RNA_COL_SPACEX   (UI_UNIT_X * 2.5f)


/* Outliner Searching --
 *
 * Are we looking for something in the outliner?
 * If so finding matches in child items makes it more useful
 *
 * - We want to flag parents to act as being open to filter child matches
 * - and also flag matches so we can highlight them
 * - Flags are stored in TreeStoreElem->flag
 * - Flag options defined in DNA_outliner_types.h
 * - SO_SEARCH_RECURSIVE defined in DNA_space_types.h
 *
 * - NOT in datablocks view - searching all datablocks takes way too long
 *   to be useful
 * - not searching into RNA items helps but isn't the complete solution
 */

#define SEARCHING_OUTLINER(sov)   (sov->search_flags & SO_SEARCH_RECURSIVE)

/* is the current element open? if so we also show children */
#define TSELEM_OPEN(telm, sv)    ( (telm->flag & TSE_CLOSED) == 0 || (SEARCHING_OUTLINER(sv) && (telm->flag & TSE_CHILDSEARCH)) )

/* outliner_tree.c ----------------------------------------------- */

void outliner_free_tree(ListBase *lb);
void outliner_cleanup_tree(struct SpaceOops *soops);

TreeElement *outliner_find_tse(struct SpaceOops *soops, const TreeStoreElem *tse);
TreeElement *outliner_find_tree_element(ListBase *lb, const TreeStoreElem *store_elem);
TreeElement *outliner_find_id(struct SpaceOops *soops, ListBase *lb, const struct ID *id);
TreeElement *outliner_find_posechannel(ListBase *lb, const struct bPoseChannel *pchan);
TreeElement *outliner_find_editbone(ListBase *lb, const struct EditBone *ebone);
struct ID *outliner_search_back(SpaceOops *soops, TreeElement *te, short idcode);

void outliner_build_tree(struct Main *mainvar, struct Scene *scene, struct SpaceOops *soops);

/* outliner_draw.c ---------------------------------------------- */

void draw_outliner(const struct bContext *C);
void restrictbutton_gr_restrict_flag(void *poin, void *poin2, int flag);

/* outliner_select.c -------------------------------------------- */
eOLDrawState tree_element_type_active(
        struct bContext *C, struct Scene *scene, struct SpaceOops *soops,
        TreeElement *te, TreeStoreElem *tselem, const eOLSetState set, bool recursive);
eOLDrawState tree_element_active(struct bContext *C, struct Scene *scene, SpaceOops *soops,
                                 TreeElement *te, const eOLSetState set, const bool handle_all_types);

void outliner_item_do_activate_from_tree_element(
        struct bContext *C, TreeElement *te, TreeStoreElem *tselem,
        bool extend, bool recursive);
int outliner_item_do_activate_from_cursor(
        struct bContext *C, const int mval[2],
        bool extend, bool recursive);

/* outliner_edit.c ---------------------------------------------- */
typedef void (*outliner_operation_cb)(
        struct bContext *C, struct ReportList *, struct Scene *scene,
        struct TreeElement *, struct TreeStoreElem *, TreeStoreElem *, void *);

void outliner_do_object_operation_ex(
        struct bContext *C, ReportList *reports, struct Scene *scene, struct SpaceOops *soops, struct ListBase *lb,
        outliner_operation_cb operation_cb, bool recurse_selected);
void outliner_do_object_operation(
        struct bContext *C, ReportList *reports, struct Scene *scene, struct SpaceOops *soops, struct ListBase *lb,
        outliner_operation_cb operation_cb);

int common_restrict_check(struct bContext *C, struct Object *ob);

int outliner_flag_is_any_test(ListBase *lb, short flag, const int curlevel);
void outliner_flag_set(ListBase *lb, short flag, short set);

void object_toggle_visibility_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void object_toggle_selectability_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void object_toggle_renderability_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);


void group_toggle_visibility_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void group_toggle_selectability_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void group_toggle_renderability_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);

void item_rename_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene,
        TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void lib_relocate_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene, struct TreeElement *te,
        struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void lib_reload_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene, struct TreeElement *te,
        struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);

void id_delete_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene, struct TreeElement *te,
        struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);
void id_remap_cb(
        struct bContext *C, struct ReportList *reports, struct Scene *scene, struct TreeElement *te,
        struct TreeStoreElem *tsep, struct TreeStoreElem *tselem, void *user_data);

TreeElement *outliner_dropzone_find(const struct SpaceOops *soops, const float fmval[2], const bool children);
/* ...................................................... */

void OUTLINER_OT_item_activate(struct wmOperatorType *ot);
void OUTLINER_OT_item_openclose(struct wmOperatorType *ot);
void OUTLINER_OT_item_rename(struct wmOperatorType *ot);
void OUTLINER_OT_lib_relocate(struct wmOperatorType *ot);
void OUTLINER_OT_lib_reload(struct wmOperatorType *ot);

void OUTLINER_OT_id_delete(struct wmOperatorType *ot);

void OUTLINER_OT_show_one_level(struct wmOperatorType *ot);
void OUTLINER_OT_show_active(struct wmOperatorType *ot);
void OUTLINER_OT_show_hierarchy(struct wmOperatorType *ot);

void OUTLINER_OT_select_border(struct wmOperatorType *ot);

void OUTLINER_OT_selected_toggle(struct wmOperatorType *ot);
void OUTLINER_OT_expanded_toggle(struct wmOperatorType *ot);

void OUTLINER_OT_scroll_page(struct wmOperatorType *ot);

void OUTLINER_OT_renderability_toggle(struct wmOperatorType *ot);
void OUTLINER_OT_selectability_toggle(struct wmOperatorType *ot);
void OUTLINER_OT_visibility_toggle(struct wmOperatorType *ot);

void OUTLINER_OT_keyingset_add_selected(struct wmOperatorType *ot);
void OUTLINER_OT_keyingset_remove_selected(struct wmOperatorType *ot);

void OUTLINER_OT_drivers_add_selected(struct wmOperatorType *ot);
void OUTLINER_OT_drivers_delete_selected(struct wmOperatorType *ot);

void OUTLINER_OT_orphans_purge(struct wmOperatorType *ot);

void OUTLINER_OT_parent_drop(struct wmOperatorType *ot);
void OUTLINER_OT_parent_clear(struct wmOperatorType *ot);
void OUTLINER_OT_scene_drop(struct wmOperatorType *ot);
void OUTLINER_OT_material_drop(struct wmOperatorType *ot);
void OUTLINER_OT_group_link(struct wmOperatorType *ot);

/* outliner_tools.c ---------------------------------------------- */

void OUTLINER_OT_operation(struct wmOperatorType *ot);
void OUTLINER_OT_scene_operation(struct wmOperatorType *ot);
void OUTLINER_OT_object_operation(struct wmOperatorType *ot);
void OUTLINER_OT_group_operation(struct wmOperatorType *ot);
void OUTLINER_OT_lib_operation(struct wmOperatorType *ot);
void OUTLINER_OT_id_operation(struct wmOperatorType *ot);
void OUTLINER_OT_id_remap(struct wmOperatorType *ot);
void OUTLINER_OT_data_operation(struct wmOperatorType *ot);
void OUTLINER_OT_animdata_operation(struct wmOperatorType *ot);
void OUTLINER_OT_action_set(struct wmOperatorType *ot);
void OUTLINER_OT_constraint_operation(struct wmOperatorType *ot);
void OUTLINER_OT_modifier_operation(struct wmOperatorType *ot);
/* ---------------------------------------------------------------- */

/* outliner_ops.c */
void outliner_operatortypes(void);
void outliner_keymap(struct wmKeyConfig *keyconf);

#endif /* __OUTLINER_INTERN_H__ */
