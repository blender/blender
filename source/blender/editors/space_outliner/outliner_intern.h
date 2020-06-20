/*
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
 */

/** \file
 * \ingroup spoutliner
 */

#ifndef __OUTLINER_INTERN_H__
#define __OUTLINER_INTERN_H__

#include "RNA_types.h"

/* internal exports only */

struct ARegion;
struct EditBone;
struct ID;
struct ListBase;
struct Main;
struct Object;
struct Scene;
struct TreeElement;
struct TreeStoreElem;
struct ViewLayer;
struct bContext;
struct bPoseChannel;
struct wmKeyConfig;
struct wmOperatorType;

typedef enum TreeElementInsertType {
  TE_INSERT_BEFORE,
  TE_INSERT_AFTER,
  TE_INSERT_INTO,
} TreeElementInsertType;

typedef enum TreeTraversalAction {
  /* Continue traversal regularly, don't skip children. */
  TRAVERSE_CONTINUE = 0,
  /* Stop traversal */
  TRAVERSE_BREAK,
  /* Continue traversal, but skip childs of traversed element */
  TRAVERSE_SKIP_CHILDS,
} TreeTraversalAction;

typedef TreeTraversalAction (*TreeTraversalFunc)(struct TreeElement *te, void *customdata);

typedef struct TreeElement {
  struct TreeElement *next, *prev, *parent;
  ListBase subtree;
  int xs, ys;                 // do selection
  TreeStoreElem *store_elem;  // element in tree store
  short flag;                 // flag for non-saved stuff
  short index;                // index for data arrays
  short idcode;               // from TreeStore id
  short xend;                 // width of item display, for select
  const char *name;
  void *directdata;   // Armature Bones, Base, Sequence, Strip...
  PointerRNA rnaptr;  // RNA Pointer
} TreeElement;

typedef struct TreeElementIcon {
  struct ID *drag_id, *drag_parent;
  int icon;
} TreeElementIcon;

#define TREESTORE_ID_TYPE(_id) \
  (ELEM(GS((_id)->name), \
        ID_SCE, \
        ID_LI, \
        ID_OB, \
        ID_ME, \
        ID_CU, \
        ID_MB, \
        ID_NT, \
        ID_MA, \
        ID_TE, \
        ID_IM, \
        ID_LT, \
        ID_LA, \
        ID_CA) || \
   ELEM(GS((_id)->name), \
        ID_KE, \
        ID_WO, \
        ID_SPK, \
        ID_GR, \
        ID_AR, \
        ID_AC, \
        ID_BR, \
        ID_PA, \
        ID_GD, \
        ID_LS, \
        ID_LP, \
        ID_HA, \
        ID_PT, \
        ID_VO, \
        ID_SIM) || /* Only in 'blendfile' mode ... :/ */ \
   ELEM(GS((_id)->name), \
        ID_SCR, \
        ID_WM, \
        ID_TXT, \
        ID_VF, \
        ID_SO, \
        ID_CF, \
        ID_PAL, \
        ID_MC, \
        ID_WS, \
        ID_MSK, \
        ID_PC))

/* TreeElement->flag */
enum {
  TE_ACTIVE = (1 << 0),
  /* Closed items display their children as icon within the row. TE_ICONROW is for
   * these child-items that are visible but only within the row of the closed parent. */
  TE_ICONROW = (1 << 1),
  TE_LAZY_CLOSED = (1 << 2),
  TE_FREE_NAME = (1 << 3),
  TE_DISABLED = (1 << 4),
  TE_DRAGGING = (1 << 5),
  TE_CHILD_NOT_IN_COLLECTION = (1 << 6),
  /* Child elements of the same type in the icon-row are drawn merged as one icon.
   * This flag is set for an element that is part of these merged child icons. */
  TE_ICONROW_MERGED = (1 << 7),
};

/* button events */
#define OL_NAMEBUTTON 1

typedef enum {
  OL_DRAWSEL_NONE = 0,   /* inactive (regular black text) */
  OL_DRAWSEL_NORMAL = 1, /* active object (draws white text) */
  OL_DRAWSEL_ACTIVE = 2, /* active obdata (draws a circle around the icon) */
} eOLDrawState;

typedef enum {
  OL_SETSEL_NONE = 0,   /* don't change the selection state */
  OL_SETSEL_NORMAL = 1, /* select the item */
  OL_SETSEL_EXTEND = 2, /* select the item and extend (also toggles selection) */
} eOLSetState;

/* get TreeStoreElem associated with a TreeElement
 * < a: (TreeElement) tree element to find stored element for
 */
#define TREESTORE(a) ((a)->store_elem)

/* size constants */
#define OL_Y_OFFSET 2

#define OL_TOG_USER_BUTS_USERS (UI_UNIT_X * 2.0f + V2D_SCROLL_WIDTH)
#define OL_TOG_USER_BUTS_STATUS (UI_UNIT_X * 3.0f + V2D_SCROLL_WIDTH)
#define OL_TOG_USER_BUTS_FAKEUSER (UI_UNIT_X + V2D_SCROLL_WIDTH)

#define OL_RNA_COLX (UI_UNIT_X * 15)
#define OL_RNA_COL_SIZEX (UI_UNIT_X * 7.5f)
#define OL_RNA_COL_SPACEX (UI_UNIT_X * 2.5f)

/* The outliner display modes that support the filter system.
 * Note: keep it synced with space_outliner.py */
#define SUPPORT_FILTER_OUTLINER(soops_) (ELEM((soops_)->outlinevis, SO_VIEW_LAYER))

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
 * - NOT in data-blocks view - searching all data-blocks takes way too long
 *   to be useful
 * - not searching into RNA items helps but isn't the complete solution
 */

#define SEARCHING_OUTLINER(sov) (sov->search_flags & SO_SEARCH_RECURSIVE)

/* is the current element open? if so we also show children */
#define TSELEM_OPEN(telm, sv) \
  (((telm)->flag & TSE_CLOSED) == 0 || \
   (SEARCHING_OUTLINER(sv) && ((telm)->flag & TSE_CHILDSEARCH)))

/**
 * Container to avoid passing around these variables to many functions.
 * Also so we can have one place to assign these variables.
 */
typedef struct TreeViewContext {
  /* Scene level. */
  struct Scene *scene;
  struct ViewLayer *view_layer;

  /* Object level. */
  /** Avoid OBACT macro everywhere. */
  Object *obact;
  Object *ob_edit;
  /**
   * The pose object may not be the active object (when in weight paint mode).
   * Checking this in draw loops isn't efficient, so set only once. */
  Object *ob_pose;
} TreeViewContext;

typedef enum TreeItemSelectAction {
  OL_ITEM_DESELECT = 0,           /* Deselect the item */
  OL_ITEM_SELECT = (1 << 0),      /* Select the item */
  OL_ITEM_SELECT_DATA = (1 << 1), /* Select object data */
  OL_ITEM_ACTIVATE = (1 << 2),    /* Activate the item */
  OL_ITEM_EXTEND = (1 << 3),      /* Extend the current selection */
  OL_ITEM_RECURSIVE = (1 << 4),   /* Select recursively */
  OL_ITEM_TOGGLE_MODE = (1 << 5)  /* Temporary */
} TreeItemSelectAction;

/* outliner_tree.c ----------------------------------------------- */

void outliner_free_tree(ListBase *tree);
void outliner_cleanup_tree(struct SpaceOutliner *soops);
void outliner_free_tree_element(TreeElement *element, ListBase *parent_subtree);

void outliner_build_tree(struct Main *mainvar,
                         struct Scene *scene,
                         struct ViewLayer *view_layer,
                         struct SpaceOutliner *soops,
                         struct ARegion *region);

bool outliner_element_needs_rebuild_on_open_change(const TreeStoreElem *tselem);

typedef struct IDsSelectedData {
  struct ListBase selected_array;
} IDsSelectedData;

TreeTraversalAction outliner_find_selected_collections(struct TreeElement *te, void *customdata);
TreeTraversalAction outliner_find_selected_objects(struct TreeElement *te, void *customdata);

/* outliner_draw.c ---------------------------------------------- */

void draw_outliner(const struct bContext *C);

TreeElementIcon tree_element_get_icon(TreeStoreElem *tselem, TreeElement *te);

void outliner_collection_isolate_flag(struct Scene *scene,
                                      struct ViewLayer *view_layer,
                                      struct LayerCollection *layer_collection,
                                      struct Collection *collection,
                                      struct PropertyRNA *layer_or_collection_prop,
                                      const char *propname,
                                      const bool value);

int tree_element_id_type_to_index(TreeElement *te);

/* outliner_select.c -------------------------------------------- */
eOLDrawState tree_element_type_active(struct bContext *C,
                                      const TreeViewContext *tvc,
                                      struct SpaceOutliner *soops,
                                      TreeElement *te,
                                      TreeStoreElem *tselem,
                                      const eOLSetState set,
                                      bool recursive);
eOLDrawState tree_element_active(struct bContext *C,
                                 const TreeViewContext *tvc,
                                 SpaceOutliner *soops,
                                 TreeElement *te,
                                 const eOLSetState set,
                                 const bool handle_all_types);

void outliner_item_select(struct bContext *C,
                          struct SpaceOutliner *soops,
                          struct TreeElement *te,
                          const short select_flag);

void outliner_object_mode_toggle(struct bContext *C,
                                 Scene *scene,
                                 ViewLayer *view_layer,
                                 Base *base);

bool outliner_item_is_co_over_name_icons(const TreeElement *te, float view_co_x);
bool outliner_item_is_co_within_close_toggle(const TreeElement *te, float view_co_x);

/* outliner_edit.c ---------------------------------------------- */
typedef void (*outliner_operation_cb)(struct bContext *C,
                                      struct ReportList *,
                                      struct Scene *scene,
                                      struct TreeElement *,
                                      struct TreeStoreElem *,
                                      TreeStoreElem *,
                                      void *);

void outliner_do_object_operation_ex(struct bContext *C,
                                     struct ReportList *reports,
                                     struct Scene *scene,
                                     struct SpaceOutliner *soops,
                                     struct ListBase *lb,
                                     outliner_operation_cb operation_cb,
                                     void *user_data,
                                     bool recurse_selected);
void outliner_do_object_operation(struct bContext *C,
                                  struct ReportList *reports,
                                  struct Scene *scene,
                                  struct SpaceOutliner *soops,
                                  struct ListBase *lb,
                                  outliner_operation_cb operation_cb);

int common_restrict_check(struct bContext *C, struct Object *ob);

int outliner_flag_is_any_test(ListBase *lb, short flag, const int curlevel);
bool outliner_flag_set(ListBase *lb, short flag, short set);
bool outliner_flag_flip(ListBase *lb, short flag);

void item_rename_cb(struct bContext *C,
                    struct ReportList *reports,
                    struct Scene *scene,
                    TreeElement *te,
                    struct TreeStoreElem *tsep,
                    struct TreeStoreElem *tselem,
                    void *user_data);
void lib_relocate_cb(struct bContext *C,
                     struct ReportList *reports,
                     struct Scene *scene,
                     struct TreeElement *te,
                     struct TreeStoreElem *tsep,
                     struct TreeStoreElem *tselem,
                     void *user_data);
void lib_reload_cb(struct bContext *C,
                   struct ReportList *reports,
                   struct Scene *scene,
                   struct TreeElement *te,
                   struct TreeStoreElem *tsep,
                   struct TreeStoreElem *tselem,
                   void *user_data);

void id_delete_cb(struct bContext *C,
                  struct ReportList *reports,
                  struct Scene *scene,
                  struct TreeElement *te,
                  struct TreeStoreElem *tsep,
                  struct TreeStoreElem *tselem,
                  void *user_data);
void id_remap_cb(struct bContext *C,
                 struct ReportList *reports,
                 struct Scene *scene,
                 struct TreeElement *te,
                 struct TreeStoreElem *tsep,
                 struct TreeStoreElem *tselem,
                 void *user_data);

void item_object_mode_enter_cb(struct bContext *C,
                               struct ReportList *reports,
                               struct Scene *scene,
                               TreeElement *te,
                               struct TreeStoreElem *tsep,
                               struct TreeStoreElem *tselem,
                               void *user_data);
void item_object_mode_exit_cb(struct bContext *C,
                              struct ReportList *reports,
                              struct Scene *scene,
                              TreeElement *te,
                              struct TreeStoreElem *tsep,
                              struct TreeStoreElem *tselem,
                              void *user_data);

void outliner_set_coordinates(struct ARegion *region, struct SpaceOutliner *soops);

void outliner_item_openclose(TreeElement *te, bool open, bool toggle_all);

/* outliner_dragdrop.c */
void outliner_dropboxes(void);

void OUTLINER_OT_item_drag_drop(struct wmOperatorType *ot);
void OUTLINER_OT_parent_drop(struct wmOperatorType *ot);
void OUTLINER_OT_parent_clear(struct wmOperatorType *ot);
void OUTLINER_OT_scene_drop(struct wmOperatorType *ot);
void OUTLINER_OT_material_drop(struct wmOperatorType *ot);
void OUTLINER_OT_collection_drop(struct wmOperatorType *ot);

/* ...................................................... */

void OUTLINER_OT_highlight_update(struct wmOperatorType *ot);

void OUTLINER_OT_item_activate(struct wmOperatorType *ot);
void OUTLINER_OT_item_openclose(struct wmOperatorType *ot);
void OUTLINER_OT_item_rename(struct wmOperatorType *ot);
void OUTLINER_OT_lib_relocate(struct wmOperatorType *ot);
void OUTLINER_OT_lib_reload(struct wmOperatorType *ot);

void OUTLINER_OT_id_delete(struct wmOperatorType *ot);

void OUTLINER_OT_show_one_level(struct wmOperatorType *ot);
void OUTLINER_OT_show_active(struct wmOperatorType *ot);
void OUTLINER_OT_show_hierarchy(struct wmOperatorType *ot);

void OUTLINER_OT_select_box(struct wmOperatorType *ot);
void OUTLINER_OT_select_walk(struct wmOperatorType *ot);

void OUTLINER_OT_select_all(struct wmOperatorType *ot);
void OUTLINER_OT_expanded_toggle(struct wmOperatorType *ot);

void OUTLINER_OT_scroll_page(struct wmOperatorType *ot);

void OUTLINER_OT_keyingset_add_selected(struct wmOperatorType *ot);
void OUTLINER_OT_keyingset_remove_selected(struct wmOperatorType *ot);

void OUTLINER_OT_drivers_add_selected(struct wmOperatorType *ot);
void OUTLINER_OT_drivers_delete_selected(struct wmOperatorType *ot);

void OUTLINER_OT_orphans_purge(struct wmOperatorType *ot);

/* outliner_tools.c ---------------------------------------------- */

void merged_element_search_menu_invoke(struct bContext *C,
                                       TreeElement *parent_te,
                                       TreeElement *activate_te);

void OUTLINER_OT_operation(struct wmOperatorType *ot);
void OUTLINER_OT_scene_operation(struct wmOperatorType *ot);
void OUTLINER_OT_object_operation(struct wmOperatorType *ot);
void OUTLINER_OT_lib_operation(struct wmOperatorType *ot);
void OUTLINER_OT_id_operation(struct wmOperatorType *ot);
void OUTLINER_OT_id_remap(struct wmOperatorType *ot);
void OUTLINER_OT_id_copy(struct wmOperatorType *ot);
void OUTLINER_OT_id_paste(struct wmOperatorType *ot);
void OUTLINER_OT_data_operation(struct wmOperatorType *ot);
void OUTLINER_OT_animdata_operation(struct wmOperatorType *ot);
void OUTLINER_OT_action_set(struct wmOperatorType *ot);
void OUTLINER_OT_constraint_operation(struct wmOperatorType *ot);
void OUTLINER_OT_modifier_operation(struct wmOperatorType *ot);
void OUTLINER_OT_delete(struct wmOperatorType *ot);

/* ---------------------------------------------------------------- */

/* outliner_ops.c */
void outliner_operatortypes(void);
void outliner_keymap(struct wmKeyConfig *keyconf);

/* outliner_collections.c */

bool outliner_is_collection_tree_element(const TreeElement *te);
struct Collection *outliner_collection_from_tree_element(const TreeElement *te);
void outliner_collection_delete(struct bContext *C,
                                struct Main *bmain,
                                struct Scene *scene,
                                struct ReportList *reports,
                                bool hierarchy);

void OUTLINER_OT_collection_new(struct wmOperatorType *ot);
void OUTLINER_OT_collection_duplicate_linked(struct wmOperatorType *ot);
void OUTLINER_OT_collection_duplicate(struct wmOperatorType *ot);
void OUTLINER_OT_collection_hierarchy_delete(struct wmOperatorType *ot);
void OUTLINER_OT_collection_objects_select(struct wmOperatorType *ot);
void OUTLINER_OT_collection_objects_deselect(struct wmOperatorType *ot);
void OUTLINER_OT_collection_link(struct wmOperatorType *ot);
void OUTLINER_OT_collection_instance(struct wmOperatorType *ot);
void OUTLINER_OT_collection_exclude_set(struct wmOperatorType *ot);
void OUTLINER_OT_collection_exclude_clear(struct wmOperatorType *ot);
void OUTLINER_OT_collection_holdout_set(struct wmOperatorType *ot);
void OUTLINER_OT_collection_holdout_clear(struct wmOperatorType *ot);
void OUTLINER_OT_collection_indirect_only_set(struct wmOperatorType *ot);
void OUTLINER_OT_collection_indirect_only_clear(struct wmOperatorType *ot);

void OUTLINER_OT_collection_isolate(struct wmOperatorType *ot);
void OUTLINER_OT_collection_show(struct wmOperatorType *ot);
void OUTLINER_OT_collection_hide(struct wmOperatorType *ot);
void OUTLINER_OT_collection_show_inside(struct wmOperatorType *ot);
void OUTLINER_OT_collection_hide_inside(struct wmOperatorType *ot);
void OUTLINER_OT_collection_enable(struct wmOperatorType *ot);
void OUTLINER_OT_collection_disable(struct wmOperatorType *ot);
void OUTLINER_OT_collection_enable_render(struct wmOperatorType *ot);
void OUTLINER_OT_collection_disable_render(struct wmOperatorType *ot);
void OUTLINER_OT_hide(struct wmOperatorType *ot);
void OUTLINER_OT_unhide_all(struct wmOperatorType *ot);

/* outliner_utils.c ---------------------------------------------- */

void outliner_viewcontext_init(const struct bContext *C, TreeViewContext *tvc);

TreeElement *outliner_find_item_at_y(const SpaceOutliner *soops,
                                     const ListBase *tree,
                                     float view_co_y);
TreeElement *outliner_find_item_at_x_in_row(const SpaceOutliner *soops,
                                            const TreeElement *parent_te,
                                            float view_co_x,
                                            bool *multiple_objects);
TreeElement *outliner_find_tse(struct SpaceOutliner *soops, const TreeStoreElem *tse);
TreeElement *outliner_find_tree_element(ListBase *lb, const TreeStoreElem *store_elem);
TreeElement *outliner_find_parent_element(ListBase *lb,
                                          TreeElement *parent_te,
                                          const TreeElement *child_te);
TreeElement *outliner_find_id(struct SpaceOutliner *soops, ListBase *lb, const struct ID *id);
TreeElement *outliner_find_posechannel(ListBase *lb, const struct bPoseChannel *pchan);
TreeElement *outliner_find_editbone(ListBase *lb, const struct EditBone *ebone);
TreeElement *outliner_search_back_te(TreeElement *te, short idcode);
struct ID *outliner_search_back(TreeElement *te, short idcode);
bool outliner_tree_traverse(const SpaceOutliner *soops,
                            ListBase *tree,
                            int filter_te_flag,
                            int filter_tselem_flag,
                            TreeTraversalFunc func,
                            void *customdata);
float outliner_restrict_columns_width(const struct SpaceOutliner *soops);
TreeElement *outliner_find_element_with_flag(const ListBase *lb, short flag);
bool outliner_is_element_visible(const TreeElement *te);
void outliner_scroll_view(struct ARegion *region, int delta_y);

/* outliner_sync.c ---------------------------------------------- */

void outliner_sync_selection(const struct bContext *C, struct SpaceOutliner *soops);

#endif /* __OUTLINER_INTERN_H__ */
