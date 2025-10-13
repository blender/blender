/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <variant>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_listBase.h"
#include "DNA_object_enums.h"
#include "RNA_types.hh"

struct ARegion;
struct AssetLibraryReference;
struct AssetWeakReference;
struct Base;
struct bGPDframe;
struct bGPDlayer;
struct bPoseChannel;
struct bScreen;
struct CacheFile;
struct Collection;
struct Depsgraph;
struct EditBone;
struct ID;
struct Image;
struct LayerCollection;
struct ListBase;
struct Main;
struct Mask;
struct MovieClip;
struct Object;
struct PointerRNA;
struct RegionView3D;
struct RenderEngineType;
struct ReportList;
struct Scene;
struct ScrArea;
struct SpaceAction;
struct SpaceClip;
struct SpaceClip;
struct SpaceConsole;
struct SpaceFile;
struct SpaceGraph;
struct SpaceImage;
struct SpaceInfo;
struct SpaceLink;
struct SpaceNla;
struct SpaceNode;
struct SpaceOutliner;
struct SpaceProperties;
struct SpaceSeq;
struct SpaceSpreadsheet;
struct SpaceText;
struct SpaceTopBar;
struct SpaceUserPref;
struct StructRNA;
struct Text;
struct ToolSettings;
struct View3D;
struct ViewLayer;
struct wmGizmoGroup;
struct wmMsgBus;
struct wmWindow;
struct wmWindowManager;
struct WorkSpace;

/* Structs */

struct bContext;

struct bContextDataResult;

/* Result of context lookups.
 * The specific values are important, and used implicitly in ctx_data_get(). Some functions also
 * still accept/return `int` instead, to ensure that the compiler uses the correct storage size
 * when mixing C/C++ code. */
enum eContextResult {
  /* The context member was found, and its data is available. */
  CTX_RESULT_OK = 1,

  /* The context member was not found. */
  CTX_RESULT_MEMBER_NOT_FOUND = 0,

  /* The context member was found, but its data is not available.
   * For example, "active_bone" is a valid context member, but has not data in Object mode. */
  CTX_RESULT_NO_DATA = -1,
};

/* Function mapping a context member name to its value. */
using bContextDataCallback = int /*eContextResult*/ (*)(const bContext *C,
                                                        const char *member,
                                                        bContextDataResult *result);

struct bContextStoreEntry {
  std::string name;
  std::variant<PointerRNA, std::string, int64_t> value;
};

struct bContextStore {
  blender::Vector<bContextStoreEntry> entries;
  bool used = false;
};

namespace blender::asset_system {
class AssetRepresentation;
}

/* for the context's rna mode enum
 * keep aligned with data_mode_strings in context.cc */
enum eContextObjectMode {
  CTX_MODE_EDIT_MESH = 0,
  CTX_MODE_EDIT_CURVE,
  CTX_MODE_EDIT_SURFACE,
  CTX_MODE_EDIT_TEXT,
  CTX_MODE_EDIT_ARMATURE,
  CTX_MODE_EDIT_METABALL,
  CTX_MODE_EDIT_LATTICE,
  CTX_MODE_EDIT_CURVES,
  CTX_MODE_EDIT_GREASE_PENCIL,
  CTX_MODE_EDIT_POINTCLOUD,
  CTX_MODE_POSE,
  CTX_MODE_SCULPT,
  CTX_MODE_PAINT_WEIGHT,
  CTX_MODE_PAINT_VERTEX,
  CTX_MODE_PAINT_TEXTURE,
  CTX_MODE_PARTICLE,
  CTX_MODE_OBJECT,
  CTX_MODE_PAINT_GPENCIL_LEGACY,
  CTX_MODE_EDIT_GPENCIL_LEGACY,
  CTX_MODE_SCULPT_GPENCIL_LEGACY,
  CTX_MODE_WEIGHT_GPENCIL_LEGACY,
  CTX_MODE_VERTEX_GPENCIL_LEGACY,
  CTX_MODE_SCULPT_CURVES,
  CTX_MODE_PAINT_GREASE_PENCIL,
  CTX_MODE_SCULPT_GREASE_PENCIL,
  CTX_MODE_WEIGHT_GREASE_PENCIL,
  CTX_MODE_VERTEX_GREASE_PENCIL,
};
#define CTX_MODE_NUM (CTX_MODE_VERTEX_GREASE_PENCIL + 1)

/* Context */

bContext *CTX_create();
void CTX_free(bContext *C);

bContext *CTX_copy(const bContext *C);

/* Stored Context */

bContextStore *CTX_store_add(blender::Vector<std::unique_ptr<bContextStore>> &contexts,
                             blender::StringRef name,
                             const PointerRNA *ptr);
bContextStore *CTX_store_add(blender::Vector<std::unique_ptr<bContextStore>> &contexts,
                             blender::StringRef name,
                             blender::StringRef str);
bContextStore *CTX_store_add(blender::Vector<std::unique_ptr<bContextStore>> &contexts,
                             blender::StringRef name,
                             int64_t value);
bContextStore *CTX_store_add_all(blender::Vector<std::unique_ptr<bContextStore>> &contexts,
                                 const bContextStore *context);
const bContextStore *CTX_store_get(const bContext *C);
void CTX_store_set(bContext *C, const bContextStore *store);
const PointerRNA *CTX_store_ptr_lookup(const bContextStore *store,
                                       blender::StringRef name,
                                       const StructRNA *type = nullptr);
std::optional<blender::StringRefNull> CTX_store_string_lookup(const bContextStore *store,
                                                              blender::StringRef name);
std::optional<int64_t> CTX_store_int_lookup(const bContextStore *store, blender::StringRef name);

/** Needed to store if Python is initialized or not. */
bool CTX_py_init_get(const bContext *C);
void CTX_py_init_set(bContext *C, bool value);

void *CTX_py_dict_get(const bContext *C);
void *CTX_py_dict_get_orig(const bContext *C);

struct bContext_PyState {
  void *py_context;
  void *py_context_orig;
};
void CTX_py_state_push(bContext *C, bContext_PyState *pystate, void *value);
void CTX_py_state_pop(bContext *C, bContext_PyState *pystate);

/* Window Manager Context */

wmWindowManager *CTX_wm_manager(const bContext *C);
wmWindow *CTX_wm_window(const bContext *C);
WorkSpace *CTX_wm_workspace(const bContext *C);
bScreen *CTX_wm_screen(const bContext *C);
ScrArea *CTX_wm_area(const bContext *C);
SpaceLink *CTX_wm_space_data(const bContext *C);
ARegion *CTX_wm_region(const bContext *C);
void *CTX_wm_region_data(const bContext *C);
ARegion *CTX_wm_region_popup(const bContext *C);
wmGizmoGroup *CTX_wm_gizmo_group(const bContext *C);
wmMsgBus *CTX_wm_message_bus(const bContext *C);
ReportList *CTX_wm_reports(const bContext *C);

View3D *CTX_wm_view3d(const bContext *C);
RegionView3D *CTX_wm_region_view3d(const bContext *C);
SpaceText *CTX_wm_space_text(const bContext *C);
SpaceImage *CTX_wm_space_image(const bContext *C);
SpaceConsole *CTX_wm_space_console(const bContext *C);
SpaceProperties *CTX_wm_space_properties(const bContext *C);
SpaceFile *CTX_wm_space_file(const bContext *C);
SpaceSeq *CTX_wm_space_seq(const bContext *C);
SpaceOutliner *CTX_wm_space_outliner(const bContext *C);
SpaceNla *CTX_wm_space_nla(const bContext *C);
SpaceNode *CTX_wm_space_node(const bContext *C);
SpaceGraph *CTX_wm_space_graph(const bContext *C);
SpaceAction *CTX_wm_space_action(const bContext *C);
SpaceInfo *CTX_wm_space_info(const bContext *C);
SpaceUserPref *CTX_wm_space_userpref(const bContext *C);
SpaceClip *CTX_wm_space_clip(const bContext *C);
SpaceTopBar *CTX_wm_space_topbar(const bContext *C);
SpaceSpreadsheet *CTX_wm_space_spreadsheet(const bContext *C);

void CTX_wm_manager_set(bContext *C, wmWindowManager *wm);
void CTX_wm_window_set(bContext *C, wmWindow *win);
void CTX_wm_screen_set(bContext *C, bScreen *screen); /* to be removed */
void CTX_wm_area_set(bContext *C, ScrArea *area);
void CTX_wm_region_set(bContext *C, ARegion *region);
void CTX_wm_region_popup_set(bContext *C, ARegion *region_popup);
void CTX_wm_gizmo_group_set(bContext *C, wmGizmoGroup *gzgroup);

/**
 * Values to create the message that describes the reason poll failed.
 *
 * \note This must be called in the same context as the poll function that created it.
 */
struct bContextPollMsgDyn_Params {
  /** The result is allocated. */
  char *(*get_fn)(bContext *C, void *user_data);
  /** Optionally free the user-data. */
  void (*free_fn)(bContext *C, void *user_data);
  void *user_data;
};

const char *CTX_wm_operator_poll_msg_get(bContext *C, bool *r_free);

/**
 * Set a message to be shown when the operator is disabled in the UI.
 *
 * \note even though the function name does not include the word "disabled", the
 * message is only shown when the operator (in the UI) is in fact disabled.
 *
 * \note even though the function name suggests this is limited to situations
 * when the poll function returns false, this is not the case. Even when the
 * operator is disabled because it is added to a disabled uiLayout, this message
 * will show.
 */
void CTX_wm_operator_poll_msg_set(bContext *C, const char *msg);
void CTX_wm_operator_poll_msg_set_dynamic(bContext *C, const bContextPollMsgDyn_Params *params);
void CTX_wm_operator_poll_msg_clear(bContext *C);

/* Data Context
 *
 * - The dir #ListBase consists of #LinkData items.
 */

/** Data type, needed so we can tell between a NULL pointer and an empty list. */
enum class ContextDataType : uint8_t {
  Pointer = 0,
  Collection,
  Property,
  String,
  Int64,
};

PointerRNA CTX_data_pointer_get(const bContext *C, const char *member);
PointerRNA CTX_data_pointer_get_type(const bContext *C, const char *member, StructRNA *type);
PointerRNA CTX_data_pointer_get_type_silent(const bContext *C,
                                            const char *member,
                                            StructRNA *type);
blender::Vector<PointerRNA> CTX_data_collection_get(const bContext *C, const char *member);

/**
 * For each pointer in collection_pointers, remap it to point to `ptr->propname`.
 *
 * Example:
 *
 *   lb = CTX_data_collection_get(C, "selected_pose_bones"); // lb contains pose bones.
 *   CTX_data_collection_remap_property(lb, "color");        // lb now contains bone colors.
 */
void CTX_data_collection_remap_property(blender::MutableSpan<PointerRNA> collection_pointers,
                                        const char *propname);

std::optional<blender::StringRefNull> CTX_data_string_get(const bContext *C, const char *member);
std::optional<int64_t> CTX_data_int_get(const bContext *C, const char *member);

/**
 * \param C: Context.
 * \param use_store: Use 'C->wm.store'.
 * \param use_rna: Use Include the properties from #RNA_Context.
 * \param use_all: Don't skip values (currently only "scene").
 */
ListBase CTX_data_dir_get_ex(const bContext *C, bool use_store, bool use_rna, bool use_all);
ListBase CTX_data_dir_get(const bContext *C);
int /*eContextResult*/ CTX_data_get(const bContext *C,
                                    const char *member,
                                    PointerRNA *r_ptr,
                                    blender::Vector<PointerRNA> *r_lb,
                                    PropertyRNA **r_prop,
                                    int *r_index,
                                    blender::StringRef *r_str,
                                    std::optional<int64_t> *r_int_value,
                                    ContextDataType *r_type);

void CTX_data_id_pointer_set(bContextDataResult *result, ID *id);
void CTX_data_pointer_set_ptr(bContextDataResult *result, const PointerRNA *ptr);
void CTX_data_pointer_set(bContextDataResult *result, ID *id, StructRNA *type, void *data);

void CTX_data_id_list_add(bContextDataResult *result, ID *id);
void CTX_data_list_add_ptr(bContextDataResult *result, const PointerRNA *ptr);
void CTX_data_list_add(bContextDataResult *result, ID *id, StructRNA *type, void *data);

/**
 * Stores a property in a result. Make sure to also call
 * `CTX_data_type_set(result, ContextDataType::Property)`.
 * \param result: The result to store the property in.
 * \param prop: The property to store.
 * \param index: The particular index in the property to store.
 */
void CTX_data_prop_set(bContextDataResult *result, PropertyRNA *prop, int index);

void CTX_data_dir_set(bContextDataResult *result, const char **dir);

void CTX_data_type_set(bContextDataResult *result, ContextDataType type);
ContextDataType CTX_data_type_get(bContextDataResult *result);

bool CTX_data_equals(const char *member, const char *str);
bool CTX_data_dir(const char *member);

#define CTX_DATA_BEGIN(C, Type, instance, member) \
  { \
    blender::Vector<PointerRNA> ctx_data_list; \
    CTX_data_##member(C, &ctx_data_list); \
    for (PointerRNA &ctx_link : ctx_data_list) { \
      Type instance = (Type)ctx_link.data;

#define CTX_DATA_END \
  } \
  } \
  (void)0

#define CTX_DATA_BEGIN_WITH_ID(C, Type, instance, member, Type_id, instance_id) \
  CTX_DATA_BEGIN (C, Type, instance, member) \
    Type_id instance_id = (Type_id)ctx_link.owner_id;

int ctx_data_list_count(const bContext *C,
                        bool (*func)(const bContext *, blender::Vector<PointerRNA> *));

#define CTX_DATA_COUNT(C, member) ctx_data_list_count(C, CTX_data_##member)

/* Data Context Members */

Main *CTX_data_main(const bContext *C);
Scene *CTX_data_scene(const bContext *C);
Scene *CTX_data_sequencer_scene(const bContext *C);
/**
 * This is tricky. Sometimes the user overrides the render_layer
 * but not the scene_collection. In this case what to do?
 *
 * If the scene_collection is linked to the #ViewLayer we use it.
 * Otherwise we fall back to the active one of the #ViewLayer.
 */
LayerCollection *CTX_data_layer_collection(const bContext *C);
Collection *CTX_data_collection(const bContext *C);
ViewLayer *CTX_data_view_layer(const bContext *C);
RenderEngineType *CTX_data_engine_type(const bContext *C);
ToolSettings *CTX_data_tool_settings(const bContext *C);

const char *CTX_data_mode_string(const bContext *C);
enum eContextObjectMode CTX_data_mode_enum_ex(const Object *obedit,
                                              const Object *ob,
                                              eObjectMode object_mode);
enum eContextObjectMode CTX_data_mode_enum(const bContext *C);

void CTX_data_main_set(bContext *C, Main *bmain);
void CTX_data_scene_set(bContext *C, Scene *scene);

/* Only Outliner currently! */
bool CTX_data_selected_ids(const bContext *C, blender::Vector<PointerRNA> *list);

bool CTX_data_selected_editable_objects(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_selected_editable_bases(const bContext *C, blender::Vector<PointerRNA> *list);

bool CTX_data_editable_objects(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_editable_bases(const bContext *C, blender::Vector<PointerRNA> *list);

bool CTX_data_selected_objects(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_selected_bases(const bContext *C, blender::Vector<PointerRNA> *list);

bool CTX_data_visible_objects(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_visible_bases(const bContext *C, blender::Vector<PointerRNA> *list);

bool CTX_data_selectable_objects(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_selectable_bases(const bContext *C, blender::Vector<PointerRNA> *list);

Object *CTX_data_active_object(const bContext *C);
Base *CTX_data_active_base(const bContext *C);
Object *CTX_data_edit_object(const bContext *C);

Image *CTX_data_edit_image(const bContext *C);

Text *CTX_data_edit_text(const bContext *C);
MovieClip *CTX_data_edit_movieclip(const bContext *C);
Mask *CTX_data_edit_mask(const bContext *C);

CacheFile *CTX_data_edit_cachefile(const bContext *C);

bool CTX_data_selected_nodes(const bContext *C, blender::Vector<PointerRNA> *list);

EditBone *CTX_data_active_bone(const bContext *C);
bool CTX_data_selected_bones(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_selected_editable_bones(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_visible_bones(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_editable_bones(const bContext *C, blender::Vector<PointerRNA> *list);

bPoseChannel *CTX_data_active_pose_bone(const bContext *C);
bool CTX_data_selected_pose_bones(const bContext *C, blender::Vector<PointerRNA> *list);
bool CTX_data_selected_pose_bones_from_active_object(const bContext *C,
                                                     blender::Vector<PointerRNA> *list);
bool CTX_data_visible_pose_bones(const bContext *C, blender::Vector<PointerRNA> *list);

const AssetLibraryReference *CTX_wm_asset_library_ref(const bContext *C);
class blender::asset_system::AssetRepresentation *CTX_wm_asset(const bContext *C);

bool CTX_wm_interface_locked(const bContext *C);

/**
 * Gets pointer to the dependency graph.
 * If it doesn't exist yet, it will be allocated.
 *
 * The result dependency graph is NOT guaranteed to be up-to-date neither from relation nor from
 * evaluated data points of view.
 *
 * \note Can not be used if access to a fully evaluated data-block is needed.
 */
Depsgraph *CTX_data_depsgraph_pointer(const bContext *C);

/**
 * Get dependency graph which is expected to be fully evaluated.
 *
 * In the release builds it is the same as CTX_data_depsgraph_pointer(). In the debug builds extra
 * sanity checks are done. Additionally, this provides more semantic meaning to what is exactly
 * expected to happen.
 */
Depsgraph *CTX_data_expect_evaluated_depsgraph(const bContext *C);

/**
 * Gets fully updated and evaluated dependency graph.
 *
 * All the relations and evaluated objects are guaranteed to be up to date.
 *
 * \note Will be expensive if there are relations or objects tagged for update.
 * \note If there are pending updates depsgraph hooks will be invoked.
 * \warning In many cases, runtime data on associated objects will be destroyed & recreated.
 */
Depsgraph *CTX_data_ensure_evaluated_depsgraph(const bContext *C);

/* Will Return NULL if depsgraph is not allocated yet.
 * Only used by handful of operators which are run on file load.
 */
Depsgraph *CTX_data_depsgraph_on_load(const bContext *C);

/**
 * Enable or disable logging of context members.
 */
void CTX_member_logging_set(bContext *C, bool enable);

/**
 * Check if logging is enabled of context members.
 */
bool CTX_member_logging_get(const bContext *C);
