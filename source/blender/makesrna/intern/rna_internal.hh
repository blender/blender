/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

#include "BLI_utildefines.h"

#include "BLI_compiler_attrs.h"

#include "rna_internal_types.hh"

#include "UI_resources.hh"

#define RNA_MAGIC ((int)~0)

struct FreestyleSettings;
struct ID;
struct IDOverrideLibrary;
struct IDProperty;
struct FreestyleLineSet;
struct FreestyleModuleConfig;
struct Main;
struct MTex;
struct Object;
struct ReportList;
struct SDNA;
struct ViewLayer;

/* Data structures used during define */

struct ContainerDefRNA {
  void *next, *prev;

  ContainerRNA *cont;
  ListBase properties;
};

struct FunctionDefRNA {
  ContainerDefRNA cont;

  FunctionRNA *func;
  const char *srna;
  const char *call;
  const char *gencall;
};

struct PropertyDefRNA {
  PropertyDefRNA *next, *prev;

  ContainerRNA *cont;
  PropertyRNA *prop;

  /* struct */
  const char *dnastructname;
  const char *dnastructfromname;
  const char *dnastructfromprop;

  /* property */
  const char *dnaname;
  const char *dnatype;
  int dnaarraylength;
  int dnapointerlevel;
  /**
   * Offset in bytes within `dnastructname`.
   * -1 when unusable (follows pointer for e.g.). */
  int dnaoffset;
  int dnasize;

  /* for finding length of array collections */
  const char *dnalengthstructname;
  const char *dnalengthname;
  int dnalengthfixed;

  int64_t booleanbit;
  bool booleannegative;

  /* not to be confused with PROP_ENUM_FLAG
   * this only allows one of the flags to be set at a time, clearing all others */
  int enumbitflags;
};

struct StructDefRNA {
  ContainerDefRNA cont;

  StructRNA *srna;
  const char *filename;

  const char *dnaname;

  /* for derived structs to find data in some property */
  const char *dnafromname;
  const char *dnafromprop;

  ListBase functions;
};

struct AllocDefRNA {
  AllocDefRNA *next, *prev;
  void *mem;
};

struct BlenderDefRNA {
  struct SDNA *sdna;
  ListBase structs;
  ListBase allocs;
  struct StructRNA *laststruct;
  bool error;
  bool silent;
  bool preprocess;
  bool verify;
  bool animate;
  /** Whether RNA properties defined should be overridable or not by default. */
  bool make_overridable;

  /* Keep last. */
#ifndef RNA_RUNTIME
  struct {
    /** #RNA_def_property_update */
    struct {
      int noteflag;
      const char *updatefunc;
    } property_update;
  } fallback;
#endif
};

extern BlenderDefRNA DefRNA;

/* Define functions for all types */
#ifndef __RNA_ACCESS_H__
extern BlenderRNA BLENDER_RNA;
#endif

void RNA_def_ID(BlenderRNA *brna);
void RNA_def_action(BlenderRNA *brna);
void RNA_def_animation(BlenderRNA *brna);
void RNA_def_animviz(BlenderRNA *brna);
void RNA_def_armature(BlenderRNA *brna);
void RNA_def_attribute(BlenderRNA *brna);
void RNA_def_asset(BlenderRNA *brna);
void RNA_def_boid(BlenderRNA *brna);
void RNA_def_brush(BlenderRNA *brna);
void RNA_def_cachefile(BlenderRNA *brna);
void RNA_def_camera(BlenderRNA *brna);
void RNA_def_cloth(BlenderRNA *brna);
void RNA_def_collections(BlenderRNA *brna);
void RNA_def_color(BlenderRNA *brna);
void RNA_def_constraint(BlenderRNA *brna);
void RNA_def_context(BlenderRNA *brna);
void RNA_def_curve(BlenderRNA *brna);
void RNA_def_depsgraph(BlenderRNA *brna);
void RNA_def_dynamic_paint(BlenderRNA *brna);
void RNA_def_fcurve(BlenderRNA *brna);
void RNA_def_gpencil(BlenderRNA *brna);
#ifdef WITH_GREASE_PENCIL_V3
void RNA_def_grease_pencil(BlenderRNA *brna);
#endif
void RNA_def_greasepencil_modifier(BlenderRNA *brna);
void RNA_def_shader_fx(BlenderRNA *brna);
void RNA_def_curves(BlenderRNA *brna);
void RNA_def_image(BlenderRNA *brna);
void RNA_def_key(BlenderRNA *brna);
void RNA_def_light(BlenderRNA *brna);
void RNA_def_lattice(BlenderRNA *brna);
void RNA_def_linestyle(BlenderRNA *brna);
void RNA_def_main(BlenderRNA *brna);
void RNA_def_material(BlenderRNA *brna);
void RNA_def_mesh(BlenderRNA *brna);
void RNA_def_meta(BlenderRNA *brna);
void RNA_def_modifier(BlenderRNA *brna);
void RNA_def_nla(BlenderRNA *brna);
void RNA_def_nodetree(BlenderRNA *brna);
void RNA_def_node_socket_subtypes(BlenderRNA *brna);
void RNA_def_node_tree_interface(BlenderRNA *brna);
void RNA_def_object(BlenderRNA *brna);
void RNA_def_object_force(BlenderRNA *brna);
void RNA_def_packedfile(BlenderRNA *brna);
void RNA_def_palette(BlenderRNA *brna);
void RNA_def_particle(BlenderRNA *brna);
void RNA_def_pointcloud(BlenderRNA *brna);
void RNA_def_pose(BlenderRNA *brna);
void RNA_def_profile(BlenderRNA *brna);
void RNA_def_lightprobe(BlenderRNA *brna);
void RNA_def_render(BlenderRNA *brna);
void RNA_def_rigidbody(BlenderRNA *brna);
void RNA_def_rna(BlenderRNA *brna);
void RNA_def_scene(BlenderRNA *brna);
void RNA_def_simulation(BlenderRNA *brna);
void RNA_def_view_layer(BlenderRNA *brna);
void RNA_def_screen(BlenderRNA *brna);
void RNA_def_sculpt_paint(BlenderRNA *brna);
void RNA_def_sequencer(BlenderRNA *brna);
void RNA_def_fluid(BlenderRNA *brna);
void RNA_def_space(BlenderRNA *brna);
void RNA_def_speaker(BlenderRNA *brna);
void RNA_def_test(BlenderRNA *brna);
void RNA_def_text(BlenderRNA *brna);
void RNA_def_texture(BlenderRNA *brna);
void RNA_def_timeline_marker(BlenderRNA *brna);
void RNA_def_sound(BlenderRNA *brna);
void RNA_def_ui(BlenderRNA *brna);
void RNA_def_usd(BlenderRNA *brna);
void RNA_def_userdef(BlenderRNA *brna);
void RNA_def_vfont(BlenderRNA *brna);
void RNA_def_volume(BlenderRNA *brna);
void RNA_def_wm(BlenderRNA *brna);
void RNA_def_wm_gizmo(BlenderRNA *brna);
void RNA_def_workspace(BlenderRNA *brna);
void RNA_def_world(BlenderRNA *brna);
void RNA_def_movieclip(BlenderRNA *brna);
void RNA_def_tracking(BlenderRNA *brna);
void RNA_def_mask(BlenderRNA *brna);
void RNA_def_xr(BlenderRNA *brna);

/* Common Define functions */

void rna_def_attributes_common(StructRNA *srna);

void rna_AttributeGroup_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr);
void rna_AttributeGroup_iterator_next(CollectionPropertyIterator *iter);
PointerRNA rna_AttributeGroup_iterator_get(CollectionPropertyIterator *iter);
int rna_AttributeGroup_length(PointerRNA *ptr);

void rna_AttributeGroup_color_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr);
void rna_AttributeGroup_color_iterator_next(CollectionPropertyIterator *iter);
PointerRNA rna_AttributeGroup_color_iterator_get(CollectionPropertyIterator *iter);
int rna_AttributeGroup_color_length(PointerRNA *ptr);

void rna_def_animdata_common(StructRNA *srna);

bool rna_AnimaData_override_apply(Main *bmain, RNAPropertyOverrideApplyContext &rnaapply_ctx);

void rna_def_animviz_common(StructRNA *srna);
void rna_def_motionpath_common(StructRNA *srna);

void api_ui_item_common_translation(FunctionRNA *func);

/**
 * Settings for curved bbone settings.
 */
void rna_def_bone_curved_common(StructRNA *srna, bool is_posebone, bool is_editbone);

void rna_def_texmat_common(StructRNA *srna, const char *texspace_editable);
void rna_def_mtex_common(BlenderRNA *brna,
                         StructRNA *srna,
                         const char *begin,
                         const char *activeget,
                         const char *activeset,
                         const char *activeeditable,
                         const char *name,
                         const char *name_slots,
                         const char *update,
                         const char *update_index);
void rna_def_texpaint_slots(BlenderRNA *brna, StructRNA *srna);
void rna_def_view_layer_common(BlenderRNA *brna, StructRNA *srna, bool scene);

int rna_AssetMetaData_editable(const PointerRNA *ptr, const char **r_info);
/**
 * \note the UI text and updating has to be set by the caller.
 */
PropertyRNA *rna_def_asset_library_reference_common(StructRNA *srna,
                                                    const char *get,
                                                    const char *set);
const EnumPropertyItem *rna_asset_library_reference_itemf(bContext *C,
                                                          PointerRNA *ptr,
                                                          PropertyRNA *prop,
                                                          bool *r_free);

/**
 * Common properties for Action/Bone Groups - related to color.
 */
void rna_def_actionbone_group_common(StructRNA *srna, int update_flag, const char *update_cb);
void rna_ActionGroup_colorset_set(PointerRNA *ptr, int value);
bool rna_ActionGroup_is_custom_colorset_get(PointerRNA *ptr);

void rna_ID_name_get(PointerRNA *ptr, char *value);
int rna_ID_name_length(PointerRNA *ptr);
void rna_ID_name_set(PointerRNA *ptr, const char *value);
StructRNA *rna_ID_refine(PointerRNA *ptr);
IDProperty **rna_ID_idprops(PointerRNA *ptr);
void rna_ID_fake_user_set(PointerRNA *ptr, bool value);
void **rna_ID_instance(PointerRNA *ptr);
IDProperty **rna_PropertyGroup_idprops(PointerRNA *ptr);
bool rna_PropertyGroup_unregister(Main *bmain, StructRNA *type);
StructRNA *rna_PropertyGroup_register(Main *bmain,
                                      ReportList *reports,
                                      void *data,
                                      const char *identifier,
                                      StructValidateFunc validate,
                                      StructCallbackFunc call,
                                      StructFreeFunc free);
StructRNA *rna_PropertyGroup_refine(PointerRNA *ptr);

void rna_object_vgroup_name_index_get(PointerRNA *ptr, char *value, int index);
int rna_object_vgroup_name_index_length(PointerRNA *ptr, int index);
void rna_object_vgroup_name_index_set(PointerRNA *ptr, const char *value, short *index);
void rna_object_vgroup_name_set(PointerRNA *ptr,
                                const char *value,
                                char *result,
                                int result_maxncpy);
void rna_object_uvlayer_name_set(PointerRNA *ptr,
                                 const char *value,
                                 char *result,
                                 int result_maxncpy);
void rna_object_vcollayer_name_set(PointerRNA *ptr,
                                   const char *value,
                                   char *result,
                                   int result_maxncpy);
PointerRNA rna_object_shapekey_index_get(ID *id, int value);
int rna_object_shapekey_index_set(ID *id, PointerRNA value, int current);

void rna_def_object_type_visibility_flags_common(StructRNA *srna,
                                                 int noteflag,
                                                 const char *update_func);
int rna_object_type_visibility_icon_get_common(int object_type_exclude_viewport,
                                               const int *object_type_exclude_select);

/* ViewLayer related functions defined in rna_scene.cc but required in rna_layer.cc */
void rna_def_freestyle_settings(BlenderRNA *brna);
PointerRNA rna_FreestyleLineSet_linestyle_get(PointerRNA *ptr);
void rna_FreestyleLineSet_linestyle_set(PointerRNA *ptr, PointerRNA value, ReportList *reports);
FreestyleLineSet *rna_FreestyleSettings_lineset_add(ID *id,
                                                    FreestyleSettings *config,
                                                    Main *bmain,
                                                    const char *name);
void rna_FreestyleSettings_lineset_remove(ID *id,
                                          FreestyleSettings *config,
                                          ReportList *reports,
                                          PointerRNA *lineset_ptr);
PointerRNA rna_FreestyleSettings_active_lineset_get(PointerRNA *ptr);
void rna_FreestyleSettings_active_lineset_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
int rna_FreestyleSettings_active_lineset_index_get(PointerRNA *ptr);
void rna_FreestyleSettings_active_lineset_index_set(PointerRNA *ptr, int value);
FreestyleModuleConfig *rna_FreestyleSettings_module_add(ID *id, FreestyleSettings *config);
void rna_FreestyleSettings_module_remove(ID *id,
                                         FreestyleSettings *config,
                                         ReportList *reports,
                                         PointerRNA *module_ptr);

void rna_Scene_use_view_map_cache_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Scene_render_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Scene_freestyle_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_ViewLayer_name_set(PointerRNA *ptr, const char *value);
void rna_ViewLayer_material_override_update(Main *bmain, Scene *activescene, PointerRNA *ptr);
void rna_ViewLayer_pass_update(Main *bmain, Scene *activescene, PointerRNA *ptr);
void rna_ViewLayer_active_aov_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
int rna_ViewLayer_active_aov_index_get(PointerRNA *ptr);
void rna_ViewLayer_active_aov_index_set(PointerRNA *ptr, int value);
void rna_ViewLayer_active_lightgroup_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
int rna_ViewLayer_active_lightgroup_index_get(PointerRNA *ptr);
void rna_ViewLayer_active_lightgroup_index_set(PointerRNA *ptr, int value);
/**
 * Set `r_rna_path` with the base view-layer path.
 * `rna_path_buffer_size` should be at least `sizeof(ViewLayer.name) * 3`.
 * \return actual length of the generated RNA path.
 */
size_t rna_ViewLayer_path_buffer_get(const ViewLayer *view_layer,
                                     char *r_rna_path,
                                     const size_t rna_path_buffer_size);

/* named internal so as not to conflict with obj.update() rna func */
void rna_Object_internal_update_data(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Mesh_update_draw(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_TextureSlot_update(bContext *C, PointerRNA *ptr);

/* basic poll functions for object types */
bool rna_Armature_object_poll(PointerRNA *ptr, PointerRNA value);
bool rna_Camera_object_poll(PointerRNA *ptr, PointerRNA value);
bool rna_Curve_object_poll(PointerRNA *ptr, PointerRNA value);
bool rna_GPencil_object_poll(PointerRNA *ptr, PointerRNA value);
bool rna_Light_object_poll(PointerRNA *ptr, PointerRNA value);
bool rna_Lattice_object_poll(PointerRNA *ptr, PointerRNA value);
bool rna_Mesh_object_poll(PointerRNA *ptr, PointerRNA value);

/* basic poll functions for actions (to prevent actions getting set in wrong places) */
bool rna_Action_id_poll(PointerRNA *ptr, PointerRNA value);
bool rna_Action_actedit_assign_poll(PointerRNA *ptr, PointerRNA value);

/* Grease Pencil datablock polling functions - for filtering GP Object vs Annotation datablocks */
bool rna_GPencil_datablocks_annotations_poll(PointerRNA *ptr, const PointerRNA value);
bool rna_GPencil_datablocks_obdata_poll(PointerRNA *ptr, const PointerRNA value);

std::optional<std::string> rna_TextureSlot_path(const PointerRNA *ptr);
std::optional<std::string> rna_Node_ImageUser_path(const PointerRNA *ptr);
std::optional<std::string> rna_CameraBackgroundImage_image_or_movieclip_user_path(
    const PointerRNA *ptr);

/* Node socket subtypes for group interface. */
void rna_def_node_socket_interface_subtypes(BlenderRNA *brna);

/* Set U.is_dirty and redraw. */

/**
 * Use single function so we can more easily break-point it.
 */
void rna_userdef_is_dirty_update_impl();
/**
 * Use as a fallback update handler to ensure #U.runtime.is_dirty is set.
 * So the preferences are saved when modified.
 */
void rna_userdef_is_dirty_update(Main *bmain, Scene *scene, PointerRNA *ptr);

/* API functions */

void RNA_api_action(StructRNA *srna);
void RNA_api_animdata(StructRNA *srna);
void RNA_api_armature_edit_bone(StructRNA *srna);
void RNA_api_bone(StructRNA *srna);
void RNA_api_bonecollection(StructRNA *srna);
void RNA_api_camera(StructRNA *srna);
void RNA_api_curve(StructRNA *srna);
void RNA_api_curve_nurb(StructRNA *srna);
void RNA_api_fcurves(StructRNA *srna);
void RNA_api_drivers(StructRNA *srna);
void RNA_api_image_packed_file(StructRNA *srna);
void RNA_api_image(StructRNA *srna);
void RNA_api_lattice(StructRNA *srna);
void RNA_api_operator(StructRNA *srna);
void RNA_api_macro(StructRNA *srna);
void RNA_api_gizmo(StructRNA *srna);
void RNA_api_gizmogroup(StructRNA *srna);
void RNA_api_keyconfig(StructRNA *srna);
void RNA_api_keyconfigs(StructRNA *srna);
void RNA_api_keyingset(StructRNA *srna);
void RNA_api_keymap(StructRNA *srna);
void RNA_api_keymaps(StructRNA *srna);
void RNA_api_keymapitem(StructRNA *srna);
void RNA_api_keymapitems(StructRNA *srna);
void RNA_api_main(StructRNA *srna);
void RNA_api_material(StructRNA *srna);
void RNA_api_mesh(StructRNA *srna);
void RNA_api_meta(StructRNA *srna);
void RNA_api_object(StructRNA *srna);
void RNA_api_pose(StructRNA *srna);
void RNA_api_pose_channel(StructRNA *srna);
void RNA_api_scene(StructRNA *srna);
void RNA_api_scene_render(StructRNA *srna);
void RNA_api_sequence_strip(StructRNA *srna);
void RNA_api_text(StructRNA *srna);
void RNA_api_ui_layout(StructRNA *srna);
void RNA_api_window(StructRNA *srna);
void RNA_api_wm(StructRNA *srna);
void RNA_api_space_node(StructRNA *srna);
void RNA_api_space_text(StructRNA *srna);
void RNA_api_space_filebrowser(StructRNA *srna);
void RNA_api_region_view3d(StructRNA *srna);
void RNA_api_texture(StructRNA *srna);
void RNA_api_sequences(BlenderRNA *brna, PropertyRNA *cprop, bool metastrip);
void RNA_api_sequence_elements(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_api_sequence_retiming_keys(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_api_sound(StructRNA *srna);
void RNA_api_vfont(StructRNA *srna);
void RNA_api_workspace(StructRNA *srna);
void RNA_api_workspace_tool(StructRNA *srna);

/* main collection functions */
void RNA_def_main_cameras(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_scenes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_objects(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_materials(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_node_groups(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_meshes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_lights(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_libraries(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_screens(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_window_managers(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_images(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_lattices(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_curves(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_metaballs(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_fonts(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_textures(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_brushes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_worlds(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_collections(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_texts(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_speakers(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_sounds(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_armatures(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_actions(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_particles(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_palettes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_gpencil_legacy(BlenderRNA *brna, PropertyRNA *cprop);
#ifdef WITH_GREASE_PENCIL_V3
void RNA_def_main_grease_pencil(BlenderRNA *brna, PropertyRNA *cprop);
#endif
void RNA_def_main_movieclips(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_masks(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_linestyles(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_cachefiles(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_paintcurves(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_workspaces(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_lightprobes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_hair_curves(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_pointclouds(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_volumes(BlenderRNA *brna, PropertyRNA *cprop);

/* ID Properties */

#ifndef __RNA_ACCESS_H__
extern StructRNA RNA_PropertyGroupItem;
extern StructRNA RNA_PropertyGroup;
#endif

/**
 * This function only returns an #IDProperty,
 * or NULL (in case IDProp could not be found, or prop is a real RNA property).
 */
IDProperty *rna_idproperty_check(PropertyRNA **prop, PointerRNA *ptr) ATTR_WARN_UNUSED_RESULT;
/**
 * This function always return the valid, real data pointer, be it a regular RNA property one,
 * or an #IDProperty one.
 */
PropertyRNA *rna_ensure_property_realdata(PropertyRNA **prop,
                                          PointerRNA *ptr) ATTR_WARN_UNUSED_RESULT;
PropertyRNA *rna_ensure_property(PropertyRNA *prop) ATTR_WARN_UNUSED_RESULT;

/* Override default callbacks. */
/* Default override callbacks for all types. */
/* TODO: Maybe at some point we'll want to write that in direct RNA-generated code instead
 *       (like we do for default get/set/etc.)?
 *       Not obvious though, those are fairly more complicated than basic SDNA access.
 */
void rna_property_override_diff_default(Main *bmain, RNAPropertyOverrideDiffContext &rnadiff_ctx);

bool rna_property_override_store_default(Main *bmain,
                                         PointerRNA *ptr_local,
                                         PointerRNA *ptr_reference,
                                         PointerRNA *ptr_storage,
                                         PropertyRNA *prop_local,
                                         PropertyRNA *prop_reference,
                                         PropertyRNA *prop_storage,
                                         int len_local,
                                         int len_reference,
                                         int len_storage,
                                         IDOverrideLibraryPropertyOperation *opop);

bool rna_property_override_apply_default(Main *bmain,
                                         RNAPropertyOverrideApplyContext &rnaapply_ctx);

/* Builtin Property Callbacks */

void rna_builtin_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr);
void rna_builtin_properties_next(CollectionPropertyIterator *iter);
PointerRNA rna_builtin_properties_get(CollectionPropertyIterator *iter);
PointerRNA rna_builtin_type_get(PointerRNA *ptr);
int rna_builtin_properties_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr);

/* Iterators */

void rna_iterator_listbase_begin(CollectionPropertyIterator *iter,
                                 ListBase *lb,
                                 IteratorSkipFunc skip);
void rna_iterator_listbase_next(CollectionPropertyIterator *iter);
void *rna_iterator_listbase_get(CollectionPropertyIterator *iter);
void rna_iterator_listbase_end(CollectionPropertyIterator *iter);
PointerRNA rna_listbase_lookup_int(PointerRNA *ptr, StructRNA *type, ListBase *lb, int index);

void rna_iterator_array_begin(CollectionPropertyIterator *iter,
                              void *ptr,
                              int itemsize,
                              int length,
                              bool free_ptr,
                              IteratorSkipFunc skip);
void rna_iterator_array_next(CollectionPropertyIterator *iter);
void *rna_iterator_array_get(CollectionPropertyIterator *iter);
void *rna_iterator_array_dereference_get(CollectionPropertyIterator *iter);
void rna_iterator_array_end(CollectionPropertyIterator *iter);
PointerRNA rna_array_lookup_int(
    PointerRNA *ptr, StructRNA *type, void *data, int itemsize, int length, int index);

/* Duplicated code since we can't link in blenlib */

#ifndef RNA_RUNTIME
void *rna_alloc_from_buffer(const char *buffer, int buffer_len);
void *rna_calloc(int buffer_len);
#endif

void rna_addtail(ListBase *listbase, void *vlink);
void rna_freelinkN(ListBase *listbase, void *vlink);
void rna_freelistN(ListBase *listbase);
PropertyDefRNA *rna_findlink(ListBase *listbase, const char *identifier);

StructDefRNA *rna_find_struct_def(StructRNA *srna);
FunctionDefRNA *rna_find_function_def(FunctionRNA *func);
PropertyDefRNA *rna_find_parameter_def(PropertyRNA *parm);
PropertyDefRNA *rna_find_struct_property_def(StructRNA *srna, PropertyRNA *prop);

/* Pointer Handling */

PointerRNA rna_pointer_inherit_refine(const PointerRNA *ptr, StructRNA *type, void *data);

/* Functions */

int rna_parameter_size(PropertyRNA *parm);
int rna_parameter_size_pad(const int size);

/* XXX, these should not need to be defined here~! */
MTex *rna_mtex_texture_slots_add(ID *self, bContext *C, ReportList *reports);
MTex *rna_mtex_texture_slots_create(ID *self, bContext *C, ReportList *reports, int index);
void rna_mtex_texture_slots_clear(ID *self, bContext *C, ReportList *reports, int index);

int rna_IDMaterials_assign_int(PointerRNA *ptr, int key, const PointerRNA *assign_ptr);

const char *rna_translate_ui_text(
    const char *text, const char *text_ctxt, StructRNA *type, PropertyRNA *prop, bool translate);

/* Internal functions that cycles uses so we need to declare (not ideal!). */
void rna_RenderPass_rect_set(PointerRNA *ptr, const float *values);

#ifdef RNA_RUNTIME
#  ifdef __GNUC__
#    pragma GCC diagnostic ignored "-Wredundant-decls"
#  endif
#endif

/* C11 for compile time range checks */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define USE_RNA_RANGE_CHECK
#  define TYPEOF_MAX(x) \
    _Generic((x), \
        bool: 1, \
        char: CHAR_MAX, \
        signed char: SCHAR_MAX, \
        unsigned char: UCHAR_MAX, \
        signed short: SHRT_MAX, \
        unsigned short: USHRT_MAX, \
        signed int: INT_MAX, \
        unsigned int: UINT_MAX, \
        float: FLT_MAX, \
        double: DBL_MAX)

#  define TYPEOF_MIN(x) \
    _Generic((x), \
        bool: 0, \
        char: CHAR_MIN, \
        signed char: SCHAR_MIN, \
        unsigned char: 0, \
        signed short: SHRT_MIN, \
        unsigned short: 0, \
        signed int: INT_MIN, \
        unsigned int: 0, \
        float: -FLT_MAX, \
        double: -DBL_MAX)
#endif
