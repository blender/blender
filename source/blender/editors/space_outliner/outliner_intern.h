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

struct wmWindowManager;
struct wmOperatorType;
struct TreeStoreElem;
struct bContext;
struct Scene;
struct ARegion;
struct ID;
struct Object;

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

/* TreeElement->flag */
#define TE_ACTIVE       1
#define TE_ICONROW      2
#define TE_LAZY_CLOSED  4
#define TE_FREE_NAME    8

/* TreeStoreElem types */
#define TSE_NLA             1
#define TSE_NLA_ACTION      2
#define TSE_DEFGROUP_BASE   3
#define TSE_DEFGROUP        4
#define TSE_BONE            5
#define TSE_EBONE           6
#define TSE_CONSTRAINT_BASE 7
#define TSE_CONSTRAINT      8
#define TSE_MODIFIER_BASE   9
#define TSE_MODIFIER        10
#define TSE_LINKED_OB       11
// #define TSE_SCRIPT_BASE     12  // UNUSED
#define TSE_POSE_BASE       13
#define TSE_POSE_CHANNEL    14
#define TSE_ANIM_DATA       15
#define TSE_DRIVER_BASE     16
#define TSE_DRIVER          17

#define TSE_PROXY           18
#define TSE_R_LAYER_BASE    19
#define TSE_R_LAYER         20
#define TSE_R_PASS          21
#define TSE_LINKED_MAT      22
/* NOTE, is used for light group */
#define TSE_LINKED_LAMP     23
#define TSE_POSEGRP_BASE    24
#define TSE_POSEGRP         25
#define TSE_SEQUENCE        26
#define TSE_SEQ_STRIP       27
#define TSE_SEQUENCE_DUP    28
#define TSE_LINKED_PSYS     29
#define TSE_RNA_STRUCT      30
#define TSE_RNA_PROPERTY    31
#define TSE_RNA_ARRAY_ELEM  32
#define TSE_NLA_TRACK       33
#define TSE_KEYMAP          34
#define TSE_KEYMAP_ITEM     35
#define TSE_ID_BASE			36

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

/* is the currrent element open? if so we also show children */
#define TSELEM_OPEN(telm, sv)    ( (telm->flag & TSE_CLOSED) == 0 || (SEARCHING_OUTLINER(sv) && (telm->flag & TSE_CHILDSEARCH)) )

/* outliner_tree.c ----------------------------------------------- */

void outliner_free_tree(ListBase *lb);
void outliner_cleanup_tree(struct SpaceOops *soops);

TreeElement *outliner_find_tse(struct SpaceOops *soops, TreeStoreElem *tse);
TreeElement *outliner_find_id(struct SpaceOops *soops, ListBase *lb, struct ID *id);
struct ID *outliner_search_back(SpaceOops *soops, TreeElement *te, short idcode);

void outliner_build_tree(struct Main *mainvar, struct Scene *scene, struct SpaceOops *soops);

/* outliner_draw.c ---------------------------------------------- */

void draw_outliner(const struct bContext *C);
void restrictbutton_gr_restrict_flag(void *poin, void *poin2, int flag);

/* outliner_select.c -------------------------------------------- */
eOLDrawState tree_element_type_active(
        struct bContext *C, struct Scene *scene, struct SpaceOops *soops,
        TreeElement *te, TreeStoreElem *tselem, const eOLSetState set, bool recursive);
eOLDrawState tree_element_active(
        struct bContext *C, struct Scene *scene, SpaceOops *soops,
        TreeElement *te, const eOLSetState set);
int outliner_item_do_activate(struct bContext *C, int x, int y, bool extend, bool recursive);

/* outliner_edit.c ---------------------------------------------- */

void outliner_do_object_operation(struct bContext *C, struct Scene *scene, struct SpaceOops *soops, struct ListBase *lb, 
                                  void (*operation_cb)(struct bContext *C, struct Scene *scene, struct TreeElement *, struct TreeStoreElem *, TreeStoreElem *));

int common_restrict_check(struct bContext *C, struct Object *ob);

int outliner_has_one_flag(struct SpaceOops *soops, ListBase *lb, short flag, const int curlevel);
void outliner_set_flag(struct SpaceOops *soops, ListBase *lb, short flag, short set);

void object_toggle_visibility_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);
void object_toggle_selectability_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);
void object_toggle_renderability_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);


void group_toggle_visibility_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);
void group_toggle_selectability_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);
void group_toggle_renderability_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);

void item_rename_cb(struct bContext *C, struct Scene *scene, TreeElement *te, struct TreeStoreElem *tsep, struct TreeStoreElem *tselem);

TreeElement *outliner_dropzone_find(const struct SpaceOops *soops, const float fmval[2], const int children);
/* ...................................................... */

void OUTLINER_OT_item_activate(struct wmOperatorType *ot);
void OUTLINER_OT_item_openclose(struct wmOperatorType *ot);
void OUTLINER_OT_item_rename(struct wmOperatorType *ot);

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

void OUTLINER_OT_parent_drop(struct wmOperatorType *ot);
void OUTLINER_OT_parent_clear(struct wmOperatorType *ot);
void OUTLINER_OT_scene_drop(struct wmOperatorType *ot);
void OUTLINER_OT_material_drop(struct wmOperatorType *ot);

/* outliner_tools.c ---------------------------------------------- */

void OUTLINER_OT_operation(struct wmOperatorType *ot);
void OUTLINER_OT_object_operation(struct wmOperatorType *ot);
void OUTLINER_OT_group_operation(struct wmOperatorType *ot);
void OUTLINER_OT_id_operation(struct wmOperatorType *ot);
void OUTLINER_OT_data_operation(struct wmOperatorType *ot);
void OUTLINER_OT_animdata_operation(struct wmOperatorType *ot);
void OUTLINER_OT_action_set(struct wmOperatorType *ot);

/* ---------------------------------------------------------------- */

/* outliner_ops.c */
void outliner_operatortypes(void);
void outliner_keymap(struct wmKeyConfig *keyconf);

#endif /* __OUTLINER_INTERN_H__ */
