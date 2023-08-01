/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include <memory>

#include "RNA_types.h"

/* Needed for `tree_element_cast()`. */
#include "tree/tree_element.hh"

/* internal exports only */

struct ARegion;
struct Bone;
struct Collection;
struct EditBone;
struct ID;
struct LayerCollection;
struct ListBase;
struct Main;
struct Object;
struct Scene;
struct ShaderFxData;
struct TreeStoreElem;
struct ViewLayer;
struct bContext;
struct bContextDataResult;
struct bDeformGroup;
struct bPoseChannel;
struct ParticleSystem;
struct View2D;
struct wmKeyConfig;
struct wmOperatorType;

namespace blender::bke::outliner::treehash {
class TreeHash;
}

namespace blender::ed::outliner {

class AbstractTreeDisplay;
class AbstractTreeElement;

namespace treehash = blender::bke::outliner::treehash;

struct TreeElement;

struct SpaceOutliner_Runtime {
  /** Object to create and manage the tree for a specific display type (View Layers, Scenes,
   * Blender File, etc.). */
  std::unique_ptr<AbstractTreeDisplay> tree_display;

  /* Hash table for tree-store elements, using `(id, type, index)` as key. */
  std::unique_ptr<treehash::TreeHash> tree_hash;

  SpaceOutliner_Runtime() = default;
  /** Used for copying runtime data to a duplicated space. */
  SpaceOutliner_Runtime(const SpaceOutliner_Runtime &);
  ~SpaceOutliner_Runtime() = default;
};

enum TreeElementInsertType {
  TE_INSERT_BEFORE,
  TE_INSERT_AFTER,
  TE_INSERT_INTO,
};

enum TreeTraversalAction {
  /** Continue traversal regularly, don't skip children. */
  TRAVERSE_CONTINUE = 0,
  /** Stop traversal. */
  TRAVERSE_BREAK,
  /** Continue traversal, but skip children of traversed element. */
  TRAVERSE_SKIP_CHILDS,
};

typedef TreeTraversalAction (*TreeTraversalFunc)(TreeElement *te, void *customdata);

struct TreeElement {
  TreeElement *next, *prev, *parent;

  /**
   * The new inheritance based representation of the element (a derived type of base
   * #AbstractTreeElement) that should eventually replace #TreeElement. Step by step, data should
   * be moved to it and operations based on the type should become virtual methods of the class
   * hierarchy.
   */
  std::unique_ptr<AbstractTreeElement> abstract_element;

  ListBase subtree;
  int xs, ys;                /* Do selection. */
  TreeStoreElem *store_elem; /* Element in tree store. */
  short flag;                /* Flag for non-saved stuff. */
  short index;               /* Index for data arrays. */
  short idcode;              /* From TreeStore id. */
  short xend;                /* Width of item display, for select. */
  const char *name;
  void *directdata; /* Armature Bones, Base, ... */
};

struct TreeElementIcon {
  ID *drag_id, *drag_parent;
  int icon;
};

#define TREESTORE_ID_TYPE(_id) \
  (ELEM(GS((_id)->name), \
        ID_SCE, \
        ID_LI, \
        ID_OB, \
        ID_ME, \
        ID_CU_LEGACY, \
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
        ID_GD_LEGACY, \
        ID_LS, \
        ID_LP, \
        ID_CV, \
        ID_PT, \
        ID_VO, \
        ID_GP) || /* Only in 'blendfile' mode ... :/ */ \
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
  /** Treat the element as if it had children, e.g. draw an icon to un-collapse it, even if it
   * doesn't. Used where children are lazy-built only if the parent isn't collapsed (see
   * #AbstractTreeDisplay::is_lazy_built()). */
  TE_PRETEND_HAS_CHILDREN = (1 << 2),
  TE_FREE_NAME = (1 << 3),
  TE_DRAGGING = (1 << 4),
  TE_CHILD_NOT_IN_COLLECTION = (1 << 6),
  /* Child elements of the same type in the icon-row are drawn merged as one icon.
   * This flag is set for an element that is part of these merged child icons. */
  TE_ICONROW_MERGED = (1 << 7),
};

/* button events */
#define OL_NAMEBUTTON 1

enum eOLDrawState {
  OL_DRAWSEL_NONE = 0,   /* inactive (regular black text) */
  OL_DRAWSEL_NORMAL = 1, /* active object (draws white text) */
  OL_DRAWSEL_ACTIVE = 2, /* active obdata (draws a circle around the icon) */
};

enum eOLSetState {
  OL_SETSEL_NONE = 0,   /* don't change the selection state */
  OL_SETSEL_NORMAL = 1, /* select the item */
  OL_SETSEL_EXTEND = 2, /* select the item and extend (also toggles selection) */
};

/* get TreeStoreElem associated with a TreeElement
 * < a: (TreeElement) tree element to find stored element for
 */
#define TREESTORE(a) ((a)->store_elem)

/* size constants */
#define OL_Y_OFFSET 2

#define OL_TOG_USER_BUTS_USERS (UI_UNIT_X * 2.0f + V2D_SCROLL_WIDTH)
#define OL_TOG_USER_BUTS_STATUS (UI_UNIT_X + V2D_SCROLL_WIDTH)

#define OL_RNA_COLX (UI_UNIT_X * 15)
#define OL_RNA_COL_SIZEX (UI_UNIT_X * 7.5f)
#define OL_RNA_COL_SPACEX (UI_UNIT_X * 2.5f)

/* The outliner display modes that support the filter system.
 * NOTE: keep it synced with `space_outliner.py`. */
#define SUPPORT_FILTER_OUTLINER(space_outliner_) \
  (ELEM((space_outliner_)->outlinevis, SO_VIEW_LAYER, SO_OVERRIDES_LIBRARY))

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

#define SEARCHING_OUTLINER(sov) ((sov)->search_flags & SO_SEARCH_RECURSIVE)

/* is the current element open? if so we also show children */
#define TSELEM_OPEN(telm, sv) \
  (CHECK_TYPE_INLINE(telm, TreeStoreElem *), \
   (((telm)->flag & TSE_CLOSED) == 0 || \
    (SEARCHING_OUTLINER(sv) && ((telm)->flag & TSE_CHILDSEARCH))))

/**
 * Container to avoid passing around these variables to many functions.
 * Also so we can have one place to assign these variables.
 */
struct TreeViewContext {
  /* Scene level. */
  Scene *scene;
  ViewLayer *view_layer;

  /* Object level. */
  /** Avoid `BKE_view_layer_active_object_get` everywhere. */
  Object *obact;
  Object *ob_edit;
  /**
   * The pose object may not be the active object (when in weight paint mode).
   * Checking this in draw loops isn't efficient, so set only once. */
  Object *ob_pose;
};

enum TreeItemSelectAction {
  OL_ITEM_DESELECT = 0,           /* Deselect the item */
  OL_ITEM_SELECT = (1 << 0),      /* Select the item */
  OL_ITEM_SELECT_DATA = (1 << 1), /* Select object data */
  OL_ITEM_ACTIVATE = (1 << 2),    /* Activate the item */
  OL_ITEM_EXTEND = (1 << 3),      /* Extend the current selection */
  OL_ITEM_RECURSIVE = (1 << 4),   /* Select recursively */
};

/* `outliner_tree.cc` */

void outliner_free_tree(ListBase *tree);
void outliner_cleanup_tree(SpaceOutliner *space_outliner);
/**
 * Free \a element and its sub-tree and remove its link in \a parent_subtree.
 *
 * \note Does not remove the #TreeStoreElem of \a element!
 * \param parent_subtree: Sub-tree of the parent element, so the list containing \a element.
 */
void outliner_free_tree_element(TreeElement *element, ListBase *parent_subtree);

/**
 * Main entry point for building the tree data-structure that the outliner represents.
 */
void outliner_build_tree(Main *mainvar,
                         Scene *scene,
                         ViewLayer *view_layer,
                         SpaceOutliner *space_outliner,
                         ARegion *region);

TreeElement *outliner_add_collection_recursive(SpaceOutliner *space_outliner,
                                               Collection *collection,
                                               TreeElement *ten);

bool outliner_requires_rebuild_on_select_or_active_change(const SpaceOutliner *space_outliner);

struct IDsSelectedData {
  ListBase selected_array;
};

struct BoneElementCreateData {
  ID *armature_id;
  Bone *bone;
};

struct EditBoneElementCreateData {
  ID *armature_id;
  EditBone *ebone;
};

struct DeformGroupElementCreateData {
  Object *object;
  bDeformGroup *defgroup;
};

struct GPencilEffectElementCreateData {
  Object *object;
  ShaderFxData *fx;
};

struct ParticleSystemElementCreateData {
  Object *object;
  ParticleSystem *psys;
};

TreeTraversalAction outliner_collect_selected_collections(TreeElement *te, void *customdata);
TreeTraversalAction outliner_collect_selected_objects(TreeElement *te, void *customdata);

/* `outliner_draw.cc` */

void draw_outliner(const bContext *C);

void outliner_tree_dimensions(SpaceOutliner *space_outliner, int *r_width, int *r_height);

TreeElementIcon tree_element_get_icon(TreeStoreElem *tselem, TreeElement *te);

void outliner_collection_isolate_flag(Scene *scene,
                                      ViewLayer *view_layer,
                                      LayerCollection *layer_collection,
                                      Collection *collection,
                                      PropertyRNA *layer_or_collection_prop,
                                      const char *propname,
                                      bool value);

/**
 * Return the index to use based on the TreeElement ID and object type
 *
 * We use a continuum of indices until we get to the object data-blocks
 * and we then make room for the object types.
 */
int tree_element_id_type_to_index(TreeElement *te);

/* `outliner_select.cc` */
/**
 * Generic call for non-id data to make active in UI
 */
void tree_element_type_active_set(bContext *C,
                                  const TreeViewContext *tvc,
                                  TreeElement *te,
                                  TreeStoreElem *tselem,
                                  eOLSetState set,
                                  bool recursive);
/**
 * Generic call for non-id data to check the active state in UI.
 */
eOLDrawState tree_element_type_active_state_get(const bContext *C,
                                                const TreeViewContext *tvc,
                                                const TreeElement *te,
                                                const TreeStoreElem *tselem);
/**
 * Generic call for ID data check or make/check active in UI.
 */
void tree_element_activate(bContext *C,
                           const TreeViewContext *tvc,
                           TreeElement *te,
                           eOLSetState set,
                           bool handle_all_types);
eOLDrawState tree_element_active_state_get(const TreeViewContext *tvc,
                                           const TreeElement *te,
                                           const TreeStoreElem *tselem);

bPoseChannel *outliner_find_parent_bone(TreeElement *te, TreeElement **r_bone_te);

/**
 * Select the item using the set flags.
 */
void outliner_item_select(bContext *C,
                          SpaceOutliner *space_outliner,
                          TreeElement *te,
                          short select_flag);

/**
 * Find if x coordinate is over an icon or name.
 */
bool outliner_item_is_co_over_name_icons(const TreeElement *te, float view_co_x);
bool outliner_item_is_co_over_icon(const TreeElement *te, float view_co_x);
/**
 * Find if x coordinate is over element name.
 */
bool outliner_item_is_co_over_name(const TreeElement *te, float view_co_x);
/**
 * Find if x coordinate is over element disclosure toggle.
 */
bool outliner_item_is_co_within_close_toggle(const TreeElement *te, float view_co_x);
bool outliner_is_co_within_mode_column(SpaceOutliner *space_outliner, const float view_mval[2]);

/**
 * Toggle the item's interaction mode if supported.
 */
void outliner_item_mode_toggle(bContext *C, TreeViewContext *tvc, TreeElement *te, bool do_extend);

/* `outliner_edit.cc` */
typedef void (*outliner_operation_fn)(bContext *C,
                                      ReportList *,
                                      Scene *scene,
                                      TreeElement *,
                                      TreeStoreElem *,
                                      TreeStoreElem *,
                                      void *);

/**
 * \param recurse_selected: Set to false for operations which are already
 * recursively operating on their children.
 */
void outliner_do_object_operation_ex(bContext *C,
                                     ReportList *reports,
                                     Scene *scene,
                                     SpaceOutliner *space_outliner,
                                     ListBase *lb,
                                     outliner_operation_fn operation_fn,
                                     void *user_data,
                                     bool recurse_selected);
void outliner_do_object_operation(bContext *C,
                                  ReportList *reports,
                                  Scene *scene,
                                  SpaceOutliner *space_outliner,
                                  ListBase *lb,
                                  outliner_operation_fn operation_fn);

int outliner_flag_is_any_test(ListBase *lb, short flag, int curlevel);
/**
 * Set or unset \a flag for all outliner elements in \a lb and sub-trees.
 * \return if any flag was modified.
 */
bool outliner_flag_set(const SpaceOutliner &space_outliner, short flag, short set);
bool outliner_flag_set(const ListBase &lb, short flag, short set);
bool outliner_flag_flip(const SpaceOutliner &space_outliner, short flag);
bool outliner_flag_flip(const ListBase &lb, short flag);

void item_rename_fn(bContext *C,
                    ReportList *reports,
                    Scene *scene,
                    TreeElement *te,
                    TreeStoreElem *tsep,
                    TreeStoreElem *tselem,
                    void *user_data);
void lib_relocate_fn(bContext *C,
                     ReportList *reports,
                     Scene *scene,
                     TreeElement *te,
                     TreeStoreElem *tsep,
                     TreeStoreElem *tselem,
                     void *user_data);
void lib_reload_fn(bContext *C,
                   ReportList *reports,
                   Scene *scene,
                   TreeElement *te,
                   TreeStoreElem *tsep,
                   TreeStoreElem *tselem,
                   void *user_data);

void id_delete_tag_fn(bContext *C,
                      ReportList *reports,
                      Scene *scene,
                      TreeElement *te,
                      TreeStoreElem *tsep,
                      TreeStoreElem *tselem,
                      void *user_data);
void id_remap_fn(bContext *C,
                 ReportList *reports,
                 Scene *scene,
                 TreeElement *te,
                 TreeStoreElem *tsep,
                 TreeStoreElem *tselem,
                 void *user_data);

/**
 * To retrieve coordinates with redrawing the entire tree.
 */
void outliner_set_coordinates(const ARegion *region, const SpaceOutliner *space_outliner);

/**
 * Open or close a tree element, optionally toggling all children recursively.
 */
void outliner_item_openclose(TreeElement *te, bool open, bool toggle_all);

/* `outliner_dragdrop.cc` */

/**
 * Region drop-box definition.
 */
void outliner_dropboxes(void);

void OUTLINER_OT_item_drag_drop(wmOperatorType *ot);
void OUTLINER_OT_parent_drop(wmOperatorType *ot);
void OUTLINER_OT_parent_clear(wmOperatorType *ot);
void OUTLINER_OT_scene_drop(wmOperatorType *ot);
void OUTLINER_OT_material_drop(wmOperatorType *ot);
void OUTLINER_OT_datastack_drop(wmOperatorType *ot);
void OUTLINER_OT_collection_drop(wmOperatorType *ot);

/* ...................................................... */

void OUTLINER_OT_highlight_update(wmOperatorType *ot);

void OUTLINER_OT_item_activate(wmOperatorType *ot);
void OUTLINER_OT_item_openclose(wmOperatorType *ot);
void OUTLINER_OT_item_rename(wmOperatorType *ot);
void OUTLINER_OT_lib_relocate(wmOperatorType *ot);
void OUTLINER_OT_lib_reload(wmOperatorType *ot);

void OUTLINER_OT_id_delete(wmOperatorType *ot);

void OUTLINER_OT_show_one_level(wmOperatorType *ot);
void OUTLINER_OT_show_active(wmOperatorType *ot);
void OUTLINER_OT_show_hierarchy(wmOperatorType *ot);

void OUTLINER_OT_select_box(wmOperatorType *ot);
void OUTLINER_OT_select_walk(wmOperatorType *ot);

void OUTLINER_OT_select_all(wmOperatorType *ot);
void OUTLINER_OT_expanded_toggle(wmOperatorType *ot);

void OUTLINER_OT_scroll_page(wmOperatorType *ot);

void OUTLINER_OT_keyingset_add_selected(wmOperatorType *ot);
void OUTLINER_OT_keyingset_remove_selected(wmOperatorType *ot);

void OUTLINER_OT_drivers_add_selected(wmOperatorType *ot);
void OUTLINER_OT_drivers_delete_selected(wmOperatorType *ot);

void OUTLINER_OT_orphans_purge(wmOperatorType *ot);

/* `outliner_query.cc` */

bool outliner_shows_mode_column(const SpaceOutliner &space_outliner);
bool outliner_has_element_warnings(const SpaceOutliner &space_outliner);

/* `outliner_tools.cc` */

void merged_element_search_menu_invoke(bContext *C,
                                       TreeElement *parent_te,
                                       TreeElement *activate_te);

/* Menu only! Calls other operators */

void OUTLINER_OT_operation(wmOperatorType *ot);
void OUTLINER_OT_scene_operation(wmOperatorType *ot);
void OUTLINER_OT_object_operation(wmOperatorType *ot);
void OUTLINER_OT_lib_operation(wmOperatorType *ot);
void OUTLINER_OT_liboverride_operation(wmOperatorType *ot);
void OUTLINER_OT_liboverride_troubleshoot_operation(wmOperatorType *ot);
void OUTLINER_OT_id_operation(wmOperatorType *ot);
void OUTLINER_OT_id_remap(wmOperatorType *ot);
void OUTLINER_OT_id_copy(wmOperatorType *ot);
void OUTLINER_OT_id_paste(wmOperatorType *ot);
void OUTLINER_OT_data_operation(wmOperatorType *ot);
void OUTLINER_OT_animdata_operation(wmOperatorType *ot);
void OUTLINER_OT_action_set(wmOperatorType *ot);
void OUTLINER_OT_constraint_operation(wmOperatorType *ot);
void OUTLINER_OT_modifier_operation(wmOperatorType *ot);
void OUTLINER_OT_delete(wmOperatorType *ot);

/* `outliner_ops.cc` */

void outliner_operatortypes(void);
void outliner_keymap(wmKeyConfig *keyconf);

/* `outliner_collections.cc` */

bool outliner_is_collection_tree_element(const TreeElement *te);
Collection *outliner_collection_from_tree_element(const TreeElement *te);
void outliner_collection_delete(
    bContext *C, Main *bmain, Scene *scene, ReportList *reports, bool do_hierarchy);

void OUTLINER_OT_collection_new(wmOperatorType *ot);
void OUTLINER_OT_collection_duplicate_linked(wmOperatorType *ot);
void OUTLINER_OT_collection_duplicate(wmOperatorType *ot);
void OUTLINER_OT_collection_hierarchy_delete(wmOperatorType *ot);
void OUTLINER_OT_collection_objects_select(wmOperatorType *ot);
void OUTLINER_OT_collection_objects_deselect(wmOperatorType *ot);
void OUTLINER_OT_collection_link(wmOperatorType *ot);
void OUTLINER_OT_collection_instance(wmOperatorType *ot);
void OUTLINER_OT_collection_exclude_set(wmOperatorType *ot);
void OUTLINER_OT_collection_exclude_clear(wmOperatorType *ot);
void OUTLINER_OT_collection_holdout_set(wmOperatorType *ot);
void OUTLINER_OT_collection_holdout_clear(wmOperatorType *ot);
void OUTLINER_OT_collection_indirect_only_set(wmOperatorType *ot);
void OUTLINER_OT_collection_indirect_only_clear(wmOperatorType *ot);

void OUTLINER_OT_collection_isolate(wmOperatorType *ot);
void OUTLINER_OT_collection_show(wmOperatorType *ot);
void OUTLINER_OT_collection_hide(wmOperatorType *ot);
void OUTLINER_OT_collection_show_inside(wmOperatorType *ot);
void OUTLINER_OT_collection_hide_inside(wmOperatorType *ot);
void OUTLINER_OT_collection_enable(wmOperatorType *ot);
void OUTLINER_OT_collection_disable(wmOperatorType *ot);
void OUTLINER_OT_collection_enable_render(wmOperatorType *ot);
void OUTLINER_OT_collection_disable_render(wmOperatorType *ot);
void OUTLINER_OT_hide(wmOperatorType *ot);
void OUTLINER_OT_unhide_all(wmOperatorType *ot);

void OUTLINER_OT_collection_color_tag_set(wmOperatorType *ot);

/* `outliner_utils.cc` */

void outliner_viewcontext_init(const bContext *C, TreeViewContext *tvc);

/**
 * Try to find an item under y-coordinate \a view_co_y (view-space).
 * \note Recursive
 */
TreeElement *outliner_find_item_at_y(const SpaceOutliner *space_outliner,
                                     const ListBase *tree,
                                     float view_co_y);
/**
 * Collapsed items can show their children as click-able icons. This function tries to find
 * such an icon that represents the child item at x-coordinate \a view_co_x (view-space).
 *
 * \return a hovered child item or \a parent_te (if no hovered child found).
 */
TreeElement *outliner_find_item_at_x_in_row(const SpaceOutliner *space_outliner,
                                            TreeElement *parent_te,
                                            float view_co_x,
                                            bool *r_is_merged_icon,
                                            bool *r_is_over_icon);
/**
 * Find specific item from the trees-tore.
 */
TreeElement *outliner_find_tree_element(ListBase *lb, const TreeStoreElem *store_elem);
/**
 * Find parent element of te.
 */
TreeElement *outliner_find_parent_element(ListBase *lb,
                                          TreeElement *parent_te,
                                          const TreeElement *child_te);
/**
 * Find tree-store that refers to given ID.
 */
TreeElement *outliner_find_id(SpaceOutliner *space_outliner, ListBase *lb, const ID *id);
TreeElement *outliner_find_posechannel(ListBase *lb, const bPoseChannel *pchan);
TreeElement *outliner_find_editbone(ListBase *lb, const EditBone *ebone);
TreeElement *outliner_search_back_te(TreeElement *te, short idcode);
ID *outliner_search_back(TreeElement *te, short idcode);
/**
 * Iterate over all tree elements (pre-order traversal), executing \a func callback for
 * each tree element matching the optional filters.
 *
 * \param filter_te_flag: If not 0, only TreeElements with this flag will be visited.
 * \param filter_tselem_flag: Same as \a filter_te_flag, but for the TreeStoreElem.
 * \param func: Custom callback to execute for each visited item.
 */
bool outliner_tree_traverse(const SpaceOutliner *space_outliner,
                            ListBase *tree,
                            int filter_te_flag,
                            int filter_tselem_flag,
                            TreeTraversalFunc func,
                            void *customdata);
float outliner_right_columns_width(const SpaceOutliner *space_outliner);
/**
 * Find first tree element in tree with matching tree-store flag.
 */
TreeElement *outliner_find_element_with_flag(const ListBase *lb, short flag);
/**
 * Find if element is visible in the outliner tree, i.e. if all of its parents are expanded.
 * Doesn't check if the item is in view-bounds, for that use #outliner_is_element_in_view().
 */
bool outliner_is_element_visible(const TreeElement *te);
/**
 * Check if the element is displayed within the view bounds. Doesn't check if all parents are
 * open/uncollapsed.
 */
bool outliner_is_element_in_view(const TreeElement *te, const View2D *v2d);
/**
 * Scroll view vertically while keeping within total bounds.
 */
void outliner_scroll_view(SpaceOutliner *space_outliner, ARegion *region, int delta_y);
/**
 * The outliner should generally use #ED_region_tag_redraw_no_rebuild() to avoid unnecessary tree
 * rebuilds. If elements are open or closed, we may still have to rebuild.
 * Upon changing the open/closed state, call this to avoid rebuilds if possible.
 */
void outliner_tag_redraw_avoid_rebuild_on_open_change(const SpaceOutliner *space_outliner,
                                                      ARegion *region);

/* `outliner_sync.cc` */

/**
 * If outliner is dirty sync selection from view layer and sequencer.
 */
void outliner_sync_selection(const bContext *C, SpaceOutliner *space_outliner);

/* `outliner_context.cc` */

int outliner_context(const bContext *C, const char *member, bContextDataResult *result);

/**
 * Helper to safely "cast" a #TreeElement to its new C++ #AbstractTreeElement, if possible.
 * \return nullptr if the tree-element doesn't match the requested type \a TreeElementT or the
 *         element doesn't hold a C++ #AbstractTreeElement pendant yet.
 */
template<typename TreeElementT> TreeElementT *tree_element_cast(const TreeElement *te)
{
  static_assert(std::is_base_of_v<AbstractTreeElement, TreeElementT>,
                "Requested tree-element type must be an AbstractTreeElement");
  return dynamic_cast<TreeElementT *>(te->abstract_element.get());
}

}  // namespace blender::ed::outliner
