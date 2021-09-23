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
 */

/** \file
 * \ingroup RNA
 */

#pragma once

#include "BLI_utildefines.h"

#include "BLI_compiler_attrs.h"

#include "rna_internal_types.h"

#include "UI_resources.h"

#define RNA_MAGIC ((int)~0)

struct AssetLibraryReference;
struct FreestyleSettings;
struct ID;
struct IDOverrideLibrary;
struct IDOverrideLibraryPropertyOperation;
struct IDProperty;
struct Main;
struct Object;
struct ReportList;
struct SDNA;
struct ViewLayer;

/* Data structures used during define */

typedef struct ContainerDefRNA {
  void *next, *prev;

  ContainerRNA *cont;
  ListBase properties;
} ContainerDefRNA;

typedef struct FunctionDefRNA {
  ContainerDefRNA cont;

  FunctionRNA *func;
  const char *srna;
  const char *call;
  const char *gencall;
} FunctionDefRNA;

typedef struct PropertyDefRNA {
  struct PropertyDefRNA *next, *prev;

  struct ContainerRNA *cont;
  struct PropertyRNA *prop;

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
} PropertyDefRNA;

typedef struct StructDefRNA {
  ContainerDefRNA cont;

  struct StructRNA *srna;
  const char *filename;

  const char *dnaname;

  /* for derived structs to find data in some property */
  const char *dnafromname;
  const char *dnafromprop;

  ListBase functions;
} StructDefRNA;

typedef struct AllocDefRNA {
  struct AllocDefRNA *next, *prev;
  void *mem;
} AllocDefRNA;

typedef struct BlenderDefRNA {
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
} BlenderDefRNA;

extern BlenderDefRNA DefRNA;

/* Define functions for all types */
#ifndef __RNA_ACCESS_H__
extern BlenderRNA BLENDER_RNA;
#endif

void RNA_def_ID(struct BlenderRNA *brna);
void RNA_def_action(struct BlenderRNA *brna);
void RNA_def_animation(struct BlenderRNA *brna);
void RNA_def_animviz(struct BlenderRNA *brna);
void RNA_def_armature(struct BlenderRNA *brna);
void RNA_def_attribute(struct BlenderRNA *brna);
void RNA_def_asset(struct BlenderRNA *brna);
void RNA_def_boid(struct BlenderRNA *brna);
void RNA_def_brush(struct BlenderRNA *brna);
void RNA_def_cachefile(struct BlenderRNA *brna);
void RNA_def_camera(struct BlenderRNA *brna);
void RNA_def_cloth(struct BlenderRNA *brna);
void RNA_def_collections(struct BlenderRNA *brna);
void RNA_def_color(struct BlenderRNA *brna);
void RNA_def_constraint(struct BlenderRNA *brna);
void RNA_def_context(struct BlenderRNA *brna);
void RNA_def_curve(struct BlenderRNA *brna);
void RNA_def_depsgraph(struct BlenderRNA *brna);
void RNA_def_dynamic_paint(struct BlenderRNA *brna);
void RNA_def_fcurve(struct BlenderRNA *brna);
void RNA_def_gpencil(struct BlenderRNA *brna);
void RNA_def_greasepencil_modifier(struct BlenderRNA *brna);
void RNA_def_shader_fx(struct BlenderRNA *brna);
void RNA_def_hair(struct BlenderRNA *brna);
void RNA_def_image(struct BlenderRNA *brna);
void RNA_def_key(struct BlenderRNA *brna);
void RNA_def_light(struct BlenderRNA *brna);
void RNA_def_lattice(struct BlenderRNA *brna);
void RNA_def_linestyle(struct BlenderRNA *brna);
void RNA_def_main(struct BlenderRNA *brna);
void RNA_def_material(struct BlenderRNA *brna);
void RNA_def_mesh(struct BlenderRNA *brna);
void RNA_def_meta(struct BlenderRNA *brna);
void RNA_def_modifier(struct BlenderRNA *brna);
void RNA_def_nla(struct BlenderRNA *brna);
void RNA_def_nodetree(struct BlenderRNA *brna);
void RNA_def_object(struct BlenderRNA *brna);
void RNA_def_object_force(struct BlenderRNA *brna);
void RNA_def_packedfile(struct BlenderRNA *brna);
void RNA_def_palette(struct BlenderRNA *brna);
void RNA_def_particle(struct BlenderRNA *brna);
void RNA_def_pointcloud(struct BlenderRNA *brna);
void RNA_def_pose(struct BlenderRNA *brna);
void RNA_def_profile(struct BlenderRNA *brna);
void RNA_def_lightprobe(struct BlenderRNA *brna);
void RNA_def_render(struct BlenderRNA *brna);
void RNA_def_rigidbody(struct BlenderRNA *brna);
void RNA_def_rna(struct BlenderRNA *brna);
void RNA_def_scene(struct BlenderRNA *brna);
void RNA_def_simulation(struct BlenderRNA *brna);
void RNA_def_view_layer(struct BlenderRNA *brna);
void RNA_def_screen(struct BlenderRNA *brna);
void RNA_def_sculpt_paint(struct BlenderRNA *brna);
void RNA_def_sequencer(struct BlenderRNA *brna);
void RNA_def_fluid(struct BlenderRNA *brna);
void RNA_def_space(struct BlenderRNA *brna);
void RNA_def_speaker(struct BlenderRNA *brna);
void RNA_def_test(struct BlenderRNA *brna);
void RNA_def_text(struct BlenderRNA *brna);
void RNA_def_texture(struct BlenderRNA *brna);
void RNA_def_timeline_marker(struct BlenderRNA *brna);
void RNA_def_sound(struct BlenderRNA *brna);
void RNA_def_ui(struct BlenderRNA *brna);
void RNA_def_userdef(struct BlenderRNA *brna);
void RNA_def_vfont(struct BlenderRNA *brna);
void RNA_def_volume(struct BlenderRNA *brna);
void RNA_def_wm(struct BlenderRNA *brna);
void RNA_def_wm_gizmo(struct BlenderRNA *brna);
void RNA_def_workspace(struct BlenderRNA *brna);
void RNA_def_world(struct BlenderRNA *brna);
void RNA_def_movieclip(struct BlenderRNA *brna);
void RNA_def_tracking(struct BlenderRNA *brna);
void RNA_def_mask(struct BlenderRNA *brna);
void RNA_def_xr(struct BlenderRNA *brna);

/* Common Define functions */

void rna_def_attributes_common(struct StructRNA *srna);

void rna_AttributeGroup_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr);
void rna_AttributeGroup_iterator_next(CollectionPropertyIterator *iter);
PointerRNA rna_AttributeGroup_iterator_get(CollectionPropertyIterator *iter);
int rna_AttributeGroup_length(PointerRNA *ptr);

void rna_def_animdata_common(struct StructRNA *srna);

bool rna_AnimaData_override_apply(struct Main *bmain,
                                  struct PointerRNA *ptr_local,
                                  struct PointerRNA *ptr_reference,
                                  struct PointerRNA *ptr_storage,
                                  struct PropertyRNA *prop_local,
                                  struct PropertyRNA *prop_reference,
                                  struct PropertyRNA *prop_storage,
                                  const int len_local,
                                  const int len_reference,
                                  const int len_storage,
                                  struct PointerRNA *ptr_item_local,
                                  struct PointerRNA *ptr_item_reference,
                                  struct PointerRNA *ptr_item_storage,
                                  struct IDOverrideLibraryPropertyOperation *opop);

void rna_def_animviz_common(struct StructRNA *srna);
void rna_def_motionpath_common(struct StructRNA *srna);

void rna_def_bone_curved_common(struct StructRNA *srna, bool is_posebone, bool is_editbone);

void rna_def_texmat_common(struct StructRNA *srna, const char *texspace_editable);
void rna_def_mtex_common(struct BlenderRNA *brna,
                         struct StructRNA *srna,
                         const char *begin,
                         const char *activeget,
                         const char *activeset,
                         const char *activeeditable,
                         const char *structname,
                         const char *structname_slots,
                         const char *update,
                         const char *update_index);
void rna_def_texpaint_slots(struct BlenderRNA *brna, struct StructRNA *srna);
void rna_def_view_layer_common(struct BlenderRNA *brna, struct StructRNA *srna, const bool scene);

int rna_AssetMetaData_editable(struct PointerRNA *ptr, const char **r_info);
PropertyRNA *rna_def_asset_library_reference_common(struct StructRNA *srna,
                                                    const char *get,
                                                    const char *set);
const EnumPropertyItem *rna_asset_library_reference_itemf(struct bContext *C,
                                                          struct PointerRNA *ptr,
                                                          struct PropertyRNA *prop,
                                                          bool *r_free);

void rna_def_actionbone_group_common(struct StructRNA *srna,
                                     int update_flag,
                                     const char *update_cb);
void rna_ActionGroup_colorset_set(struct PointerRNA *ptr, int value);
bool rna_ActionGroup_is_custom_colorset_get(struct PointerRNA *ptr);

void rna_ID_name_get(struct PointerRNA *ptr, char *value);
int rna_ID_name_length(struct PointerRNA *ptr);
void rna_ID_name_set(struct PointerRNA *ptr, const char *value);
struct StructRNA *rna_ID_refine(struct PointerRNA *ptr);
struct IDProperty **rna_ID_idprops(struct PointerRNA *ptr);
void rna_ID_fake_user_set(struct PointerRNA *ptr, bool value);
void **rna_ID_instance(PointerRNA *ptr);
struct IDProperty **rna_PropertyGroup_idprops(struct PointerRNA *ptr);
void rna_PropertyGroup_unregister(struct Main *bmain, struct StructRNA *type);
struct StructRNA *rna_PropertyGroup_register(struct Main *bmain,
                                             struct ReportList *reports,
                                             void *data,
                                             const char *identifier,
                                             StructValidateFunc validate,
                                             StructCallbackFunc call,
                                             StructFreeFunc free);
struct StructRNA *rna_PropertyGroup_refine(struct PointerRNA *ptr);

void rna_object_vgroup_name_index_get(struct PointerRNA *ptr, char *value, int index);
int rna_object_vgroup_name_index_length(struct PointerRNA *ptr, int index);
void rna_object_vgroup_name_index_set(struct PointerRNA *ptr, const char *value, short *index);
void rna_object_vgroup_name_set(struct PointerRNA *ptr,
                                const char *value,
                                char *result,
                                int maxlen);
void rna_object_uvlayer_name_set(struct PointerRNA *ptr,
                                 const char *value,
                                 char *result,
                                 int maxlen);
void rna_object_vcollayer_name_set(struct PointerRNA *ptr,
                                   const char *value,
                                   char *result,
                                   int maxlen);
PointerRNA rna_object_shapekey_index_get(struct ID *id, int value);
int rna_object_shapekey_index_set(struct ID *id, PointerRNA value, int current);

/* ViewLayer related functions defined in rna_scene.c but required in rna_layer.c */
void rna_def_freestyle_settings(struct BlenderRNA *brna);
struct PointerRNA rna_FreestyleLineSet_linestyle_get(struct PointerRNA *ptr);
void rna_FreestyleLineSet_linestyle_set(struct PointerRNA *ptr,
                                        struct PointerRNA value,
                                        struct ReportList *reports);
struct FreestyleLineSet *rna_FreestyleSettings_lineset_add(struct ID *id,
                                                           struct FreestyleSettings *config,
                                                           struct Main *bmain,
                                                           const char *name);
void rna_FreestyleSettings_lineset_remove(struct ID *id,
                                          struct FreestyleSettings *config,
                                          struct ReportList *reports,
                                          struct PointerRNA *lineset_ptr);
struct PointerRNA rna_FreestyleSettings_active_lineset_get(struct PointerRNA *ptr);
void rna_FreestyleSettings_active_lineset_index_range(
    struct PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
int rna_FreestyleSettings_active_lineset_index_get(struct PointerRNA *ptr);
void rna_FreestyleSettings_active_lineset_index_set(struct PointerRNA *ptr, int value);
struct FreestyleModuleConfig *rna_FreestyleSettings_module_add(struct ID *id,
                                                               struct FreestyleSettings *config);
void rna_FreestyleSettings_module_remove(struct ID *id,
                                         struct FreestyleSettings *config,
                                         struct ReportList *reports,
                                         struct PointerRNA *module_ptr);

void rna_Scene_use_view_map_cache_update(struct Main *bmain,
                                         struct Scene *scene,
                                         struct PointerRNA *ptr);
void rna_Scene_glsl_update(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);
void rna_Scene_freestyle_update(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);
void rna_ViewLayer_name_set(struct PointerRNA *ptr, const char *value);
void rna_ViewLayer_material_override_update(struct Main *bmain,
                                            struct Scene *activescene,
                                            struct PointerRNA *ptr);
void rna_ViewLayer_pass_update(struct Main *bmain,
                               struct Scene *activescene,
                               struct PointerRNA *ptr);
void rna_ViewLayer_active_aov_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
int rna_ViewLayer_active_aov_index_get(PointerRNA *ptr);
void rna_ViewLayer_active_aov_index_set(PointerRNA *ptr, int value);

/* named internal so as not to conflict with obj.update() rna func */
void rna_Object_internal_update_data(struct Main *bmain,
                                     struct Scene *scene,
                                     struct PointerRNA *ptr);
void rna_Mesh_update_draw(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);
void rna_TextureSlot_update(struct bContext *C, struct PointerRNA *ptr);

/* basic poll functions for object types */
bool rna_Armature_object_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_Camera_object_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_Curve_object_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_GPencil_object_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_Light_object_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_Lattice_object_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_Mesh_object_poll(struct PointerRNA *ptr, struct PointerRNA value);

/* basic poll functions for actions (to prevent actions getting set in wrong places) */
bool rna_Action_id_poll(struct PointerRNA *ptr, struct PointerRNA value);
bool rna_Action_actedit_assign_poll(struct PointerRNA *ptr, struct PointerRNA value);

/* Grease Pencil datablock polling functions - for filtering GP Object vs Annotation datablocks */
bool rna_GPencil_datablocks_annotations_poll(struct PointerRNA *ptr,
                                             const struct PointerRNA value);
bool rna_GPencil_datablocks_obdata_poll(struct PointerRNA *ptr, const struct PointerRNA value);

char *rna_TextureSlot_path(struct PointerRNA *ptr);
char *rna_Node_ImageUser_path(struct PointerRNA *ptr);

/* Set U.is_dirty and redraw. */
void rna_userdef_is_dirty_update_impl(void);
void rna_userdef_is_dirty_update(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);

/* API functions */

void RNA_api_action(StructRNA *srna);
void RNA_api_animdata(struct StructRNA *srna);
void RNA_api_armature_edit_bone(StructRNA *srna);
void RNA_api_bone(StructRNA *srna);
void RNA_api_camera(StructRNA *srna);
void RNA_api_curve(StructRNA *srna);
void RNA_api_curve_nurb(StructRNA *srna);
void RNA_api_fcurves(StructRNA *srna);
void RNA_api_drivers(StructRNA *srna);
void RNA_api_image_packed_file(struct StructRNA *srna);
void RNA_api_image(struct StructRNA *srna);
void RNA_api_lattice(struct StructRNA *srna);
void RNA_api_operator(struct StructRNA *srna);
void RNA_api_macro(struct StructRNA *srna);
void RNA_api_gizmo(struct StructRNA *srna);
void RNA_api_gizmogroup(struct StructRNA *srna);
void RNA_api_keyconfig(struct StructRNA *srna);
void RNA_api_keyconfigs(struct StructRNA *srna);
void RNA_api_keyingset(struct StructRNA *srna);
void RNA_api_keymap(struct StructRNA *srna);
void RNA_api_keymaps(struct StructRNA *srna);
void RNA_api_keymapitem(struct StructRNA *srna);
void RNA_api_keymapitems(struct StructRNA *srna);
void RNA_api_main(struct StructRNA *srna);
void RNA_api_material(StructRNA *srna);
void RNA_api_mesh(struct StructRNA *srna);
void RNA_api_meta(struct StructRNA *srna);
void RNA_api_object(struct StructRNA *srna);
void RNA_api_pose(struct StructRNA *srna);
void RNA_api_pose_channel(struct StructRNA *srna);
void RNA_api_scene(struct StructRNA *srna);
void RNA_api_scene_render(struct StructRNA *srna);
void RNA_api_sequence_strip(StructRNA *srna);
void RNA_api_text(struct StructRNA *srna);
void RNA_api_ui_layout(struct StructRNA *srna);
void RNA_api_window(struct StructRNA *srna);
void RNA_api_wm(struct StructRNA *srna);
void RNA_api_space_node(struct StructRNA *srna);
void RNA_api_space_text(struct StructRNA *srna);
void RNA_api_space_filebrowser(struct StructRNA *srna);
void RNA_api_region_view3d(struct StructRNA *srna);
void RNA_api_texture(struct StructRNA *srna);
void RNA_api_sequences(BlenderRNA *brna, PropertyRNA *cprop, const bool metastrip);
void RNA_api_sequence_elements(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_api_sound(struct StructRNA *srna);
void RNA_api_vfont(struct StructRNA *srna);
void RNA_api_workspace(struct StructRNA *srna);
void RNA_api_workspace_tool(struct StructRNA *srna);

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
void RNA_def_main_gpencil(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_movieclips(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_masks(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_linestyles(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_cachefiles(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_paintcurves(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_workspaces(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_lightprobes(BlenderRNA *brna, PropertyRNA *cprop);
#ifdef WITH_HAIR_NODES
void RNA_def_main_hairs(BlenderRNA *brna, PropertyRNA *cprop);
#endif
#ifdef WITH_POINT_CLOUD
void RNA_def_main_pointclouds(BlenderRNA *brna, PropertyRNA *cprop);
#endif
void RNA_def_main_volumes(BlenderRNA *brna, PropertyRNA *cprop);
#ifdef WITH_SIMULATION_DATABLOCK
void RNA_def_main_simulations(BlenderRNA *brna, PropertyRNA *cprop);
#endif

/* ID Properties */

extern StringPropertyRNA rna_PropertyGroupItem_string;
extern IntPropertyRNA rna_PropertyGroupItem_int;
extern IntPropertyRNA rna_PropertyGroupItem_int_array;
extern FloatPropertyRNA rna_PropertyGroupItem_float;
extern FloatPropertyRNA rna_PropertyGroupItem_float_array;
extern PointerPropertyRNA rna_PropertyGroupItem_group;
extern PointerPropertyRNA rna_PropertyGroupItem_id;
extern CollectionPropertyRNA rna_PropertyGroupItem_collection;
extern CollectionPropertyRNA rna_PropertyGroupItem_idp_array;
extern FloatPropertyRNA rna_PropertyGroupItem_double;
extern FloatPropertyRNA rna_PropertyGroupItem_double_array;

#ifndef __RNA_ACCESS_H__
extern StructRNA RNA_PropertyGroupItem;
extern StructRNA RNA_PropertyGroup;
#endif

struct IDProperty *rna_idproperty_check(struct PropertyRNA **prop,
                                        struct PointerRNA *ptr) ATTR_WARN_UNUSED_RESULT;
struct PropertyRNA *rna_ensure_property_realdata(struct PropertyRNA **prop,
                                                 struct PointerRNA *ptr) ATTR_WARN_UNUSED_RESULT;
struct PropertyRNA *rna_ensure_property(struct PropertyRNA *prop) ATTR_WARN_UNUSED_RESULT;

/* Override default callbacks. */
/* Default override callbacks for all types. */
/* TODO: Maybe at some point we'll want to write that in direct RNA-generated code instead
 *       (like we do for default get/set/etc.)?
 *       Not obvious though, those are fairly more complicated than basic SDNA access.
 */
int rna_property_override_diff_default(struct Main *bmain,
                                       struct PropertyRNAOrID *prop_a,
                                       struct PropertyRNAOrID *prop_b,
                                       const int mode,
                                       struct IDOverrideLibrary *override,
                                       const char *rna_path,
                                       const size_t rna_path_len,
                                       const int flags,
                                       bool *r_override_changed);

bool rna_property_override_store_default(struct Main *bmain,
                                         struct PointerRNA *ptr_local,
                                         struct PointerRNA *ptr_reference,
                                         struct PointerRNA *ptr_storage,
                                         struct PropertyRNA *prop_local,
                                         struct PropertyRNA *prop_reference,
                                         struct PropertyRNA *prop_storage,
                                         const int len_local,
                                         const int len_reference,
                                         const int len_storage,
                                         struct IDOverrideLibraryPropertyOperation *opop);

bool rna_property_override_apply_default(struct Main *bmain,
                                         struct PointerRNA *ptr_dst,
                                         struct PointerRNA *ptr_src,
                                         struct PointerRNA *ptr_storage,
                                         struct PropertyRNA *prop_dst,
                                         struct PropertyRNA *prop_src,
                                         struct PropertyRNA *prop_storage,
                                         const int len_dst,
                                         const int len_src,
                                         const int len_storage,
                                         struct PointerRNA *ptr_item_dst,
                                         struct PointerRNA *ptr_item_src,
                                         struct PointerRNA *ptr_item_storage,
                                         struct IDOverrideLibraryPropertyOperation *opop);

/* Builtin Property Callbacks */

void rna_builtin_properties_begin(struct CollectionPropertyIterator *iter, struct PointerRNA *ptr);
void rna_builtin_properties_next(struct CollectionPropertyIterator *iter);
PointerRNA rna_builtin_properties_get(struct CollectionPropertyIterator *iter);
PointerRNA rna_builtin_type_get(struct PointerRNA *ptr);
int rna_builtin_properties_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr);

/* Iterators */

void rna_iterator_listbase_begin(struct CollectionPropertyIterator *iter,
                                 struct ListBase *lb,
                                 IteratorSkipFunc skip);
void rna_iterator_listbase_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_listbase_get(struct CollectionPropertyIterator *iter);
void rna_iterator_listbase_end(struct CollectionPropertyIterator *iter);
PointerRNA rna_listbase_lookup_int(PointerRNA *ptr,
                                   StructRNA *type,
                                   struct ListBase *lb,
                                   int index);

void rna_iterator_array_begin(struct CollectionPropertyIterator *iter,
                              void *ptr,
                              int itemsize,
                              int length,
                              bool free_ptr,
                              IteratorSkipFunc skip);
void rna_iterator_array_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_array_get(struct CollectionPropertyIterator *iter);
void *rna_iterator_array_dereference_get(struct CollectionPropertyIterator *iter);
void rna_iterator_array_end(struct CollectionPropertyIterator *iter);
PointerRNA rna_array_lookup_int(
    PointerRNA *ptr, StructRNA *type, void *data, int itemsize, int length, int index);

/* Duplicated code since we can't link in blenlib */

#ifndef RNA_RUNTIME
void *rna_alloc_from_buffer(const char *buffer, int buffer_len);
void *rna_calloc(int buffer_len);
#endif

void rna_addtail(struct ListBase *listbase, void *vlink);
void rna_freelinkN(struct ListBase *listbase, void *vlink);
void rna_freelistN(struct ListBase *listbase);
PropertyDefRNA *rna_findlink(ListBase *listbase, const char *identifier);

StructDefRNA *rna_find_struct_def(StructRNA *srna);
FunctionDefRNA *rna_find_function_def(FunctionRNA *func);
PropertyDefRNA *rna_find_parameter_def(PropertyRNA *parm);
PropertyDefRNA *rna_find_struct_property_def(StructRNA *srna, PropertyRNA *prop);

/* Pointer Handling */

PointerRNA rna_pointer_inherit_refine(struct PointerRNA *ptr, struct StructRNA *type, void *data);

/* Functions */

int rna_parameter_size(struct PropertyRNA *parm);

/* XXX, these should not need to be defined here~! */
struct MTex *rna_mtex_texture_slots_add(struct ID *self,
                                        struct bContext *C,
                                        struct ReportList *reports);
struct MTex *rna_mtex_texture_slots_create(struct ID *self,
                                           struct bContext *C,
                                           struct ReportList *reports,
                                           int index);
void rna_mtex_texture_slots_clear(struct ID *self,
                                  struct bContext *C,
                                  struct ReportList *reports,
                                  int index);

int rna_IDMaterials_assign_int(struct PointerRNA *ptr,
                               int key,
                               const struct PointerRNA *assign_ptr);

const char *rna_translate_ui_text(const char *text,
                                  const char *text_ctxt,
                                  struct StructRNA *type,
                                  struct PropertyRNA *prop,
                                  bool translate);

/* Internal functions that cycles uses so we need to declare (tsk tsk) */
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
    _Generic((x), bool : 1, char \
             : CHAR_MAX, signed char \
             : SCHAR_MAX, unsigned char \
             : UCHAR_MAX, signed short \
             : SHRT_MAX, unsigned short \
             : USHRT_MAX, signed int \
             : INT_MAX, unsigned int \
             : UINT_MAX, float \
             : FLT_MAX, double \
             : DBL_MAX)

#  define TYPEOF_MIN(x) \
    _Generic((x), bool : 0, char \
             : CHAR_MIN, signed char \
             : SCHAR_MIN, unsigned char : 0, signed short \
             : SHRT_MIN, unsigned short : 0, signed int \
             : INT_MIN, unsigned int : 0, float \
             : -FLT_MAX, double \
             : -DBL_MAX)
#endif
