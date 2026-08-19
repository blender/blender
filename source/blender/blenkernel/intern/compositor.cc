/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>
#include <string>

#include <fmt/format.h>

#include "BLI_enum_flags.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.hh"
#include "BLI_math_base.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.hh"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "DNA_layer_types.h"
#include "DNA_node_types.h"
#include "DNA_object_enums.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BLO_read_write.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.hh"
#include "BKE_compositor.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_cryptomatte.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "WM_api.hh"

#include "IMB_imbuf.hh"

#include "NOD_dependencies.hh"

namespace blender::bke::compositor {

/* --------------------------------------------------------------------
 * Cache.
 */

Cache::~Cache()
{
  this->clear_frames();
}

const ImBuf *Cache::get_frame(const int frame_number, const int view_identifier)
{
  std::scoped_lock lock{frames_mutex_};
  return this->frames_.lookup_try(FrameKey(frame_number, view_identifier)).value_or(nullptr);
}

void Cache::add_frame(const int frame_number, const int view_identifier, ImBuf *image_buffer)
{
  std::scoped_lock lock{frames_mutex_};
  /* First evict frames if needed to maintain the memory cache limit. In almost all cases, the
   * while loop will run exactly once, since the images in the cache will almost always have the
   * same size, so one goes out, one goes in. So we needn't worry about performance. */
  const int64_t cache_limit = size_t(U.memcachelimit) * 1024 * 1024;
  const int64_t image_size = IMB_get_size_in_memory(image_buffer);
  while (!this->frames_.is_empty() && this->size() + image_size > cache_limit) {
    this->evict_frame(frame_number);
  }

  this->frames_.add_new(FrameKey(frame_number, view_identifier), image_buffer);
}

void Cache::clear_frames()
{
  std::scoped_lock lock{frames_mutex_};
  for (ImBuf *image_buffer : this->frames_.values()) {
    IMB_freeImBuf(image_buffer);
  }
  this->frames_.clear();
}

Vector<IndexRange> Cache::compute_frame_ranges()
{
  /* Compute a sorted vector of all cached frames. */
  VectorSet<int> frame_numbers_set;
  {
    std::scoped_lock lock{frames_mutex_};
    frame_numbers_set.reserve(this->frames_.size());
    for (const FrameKey &key : this->frames_.keys()) {
      frame_numbers_set.add(key.frame_number);
    }
  }
  Vector<int> frame_numbers = frame_numbers_set.extract_vector();
  std::ranges::sort(frame_numbers);

  Vector<IndexRange> frame_ranges;
  for (const int frame : frame_numbers) {
    /* We start a new range by appending a singleton range of the current frame, either because
     * this is the first range or because the last range will not be contiguous with the current
     * frame. */
    if (frame_ranges.is_empty() || frame - frame_ranges.last().last() > 1) {
      frame_ranges.append(IndexRange(frame, 1));
    }
    else {
      /* Otherwise, the frame is contiguous with the last range, so we just grow its size by 1. */
      frame_ranges.last() = IndexRange(frame_ranges.last().start(),
                                       frame_ranges.last().size() + 1);
    }
  }

  return frame_ranges;
}

void Cache::evict_frame(const int current_frame_number)
{
  if (this->frames_.is_empty()) {
    return;
  }

  /* Find the keys with the maximum and minimum frame numbers. */
  FrameKey minimum_key = FrameKey(std::numeric_limits<int>::max());
  FrameKey maximum_key = FrameKey(std::numeric_limits<int>::lowest());
  for (const FrameKey &key : this->frames_.keys()) {
    if (key.frame_number < minimum_key.frame_number) {
      minimum_key = key;
    }
    if (key.frame_number > maximum_key.frame_number) {
      maximum_key = key;
    }
  }

  /* Prioritize evicting frames that are behind the current frame and are furthest from it. */
  if (minimum_key.frame_number < current_frame_number) {
    IMB_freeImBuf(this->frames_.pop(minimum_key));
    return;
  }

  /* Otherwise, evict the frame that is after the current frame and is furthest from it. */
  IMB_freeImBuf(this->frames_.pop(maximum_key));
}

int64_t Cache::size()
{
  int64_t size = 0;
  for (ImBuf *image_buffer : this->frames_.values()) {
    size += IMB_get_size_in_memory(image_buffer);
  }
  return size;
}

/* --------------------------------------------------------------------
 * Scene Compositor Effects.
 */

bool is_enabled(const Scene &scene, const ExecutionMode mode)
{
  if (mode == ExecutionMode::Render && !(scene.r.scemode & R_DOCOMP)) {
    return false;
  }

  for (SceneCompositorEffect &effect : scene.compositor_effects) {
    if (is_effect_enabled(effect, mode)) {
      return true;
    }
  }

  return false;
}

SceneCompositorEffect *get_effect(const Scene &scene, StringRef name)
{
  return static_cast<SceneCompositorEffect *>(BLI_findstring(
      &(scene.compositor_effects), name.data(), offsetof(SceneCompositorEffect, name)));
}

SceneCompositorEffect *get_active_effect(const Scene &scene)
{
  for (SceneCompositorEffect &effect : scene.compositor_effects) {
    if (flag_is_set(effect.flags, SceneCompositorEffectFlags::IsActive)) {
      return &effect;
    }
  }

  return nullptr;
}

bool is_effect_enabled(const SceneCompositorEffect &effect, const ExecutionMode mode)
{
  if (!effect.node_group) {
    return false;
  }

  switch (mode) {
    case ExecutionMode::Render:
      return flag_is_set(effect.flags, SceneCompositorEffectFlags::EnableForRender);
    case ExecutionMode::Preview:
      return flag_is_set(effect.flags, SceneCompositorEffectFlags::EnableForPreview);
  }

  BLI_assert_unreachable();
  return false;
}

void set_active_effect(const Scene &scene, SceneCompositorEffect &effect)
{
  for (SceneCompositorEffect &other_effect : scene.compositor_effects) {
    other_effect.flags &= ~SceneCompositorEffectFlags::IsActive;
  }

  /* Activate the active state of the effect. */
  effect.flags |= SceneCompositorEffectFlags::IsActive;
}

void rename_effect(Scene &scene,
                   SceneCompositorEffect &effect,
                   StringRef new_name,
                   const bool update_animation_data)
{
  std::string old_name = effect.name;
  new_name.copy_utf8_truncated(effect.name);
  BLI_uniquename(&scene.compositor_effects,
                 &effect,
                 CTX_DATA_(BLT_I18NCONTEXT_ID_SCENE, "Compositor Effect"),
                 '.',
                 offsetof(SceneCompositorEffect, name),
                 sizeof(effect.name));

  if (!update_animation_data) {
    return;
  }

  /* Fix all the animation data which may link to this. */
  BKE_animdata_fix_paths(scene.id,
                         "compositor_effects",
                         RNA_path_name_to_infix(old_name.c_str()),
                         RNA_path_name_to_infix(effect.name),
                         true,
                         *G_MAIN);
}

SceneCompositorEffect &new_effect(Scene &scene, StringRef name)
{
  SceneCompositorEffect &effect = *MEM_new<SceneCompositorEffect>("Scene Compositor Effect");
  rename_effect(scene, effect, name, false);
  BLI_addtail(&scene.compositor_effects, &effect);
  set_active_effect(scene, effect);
  return effect;
}

SceneCompositorEffect &duplicate_effect(Scene &scene, SceneCompositorEffect &source_effect)
{
  SceneCompositorEffect &new_effect = *MEM_dupalloc(&source_effect);
  if (source_effect.node_group) {
    id_us_plus(&new_effect.node_group->id);
  }
  if (source_effect.system_properties) {
    new_effect.system_properties = IDP_CopyProperty_ex(source_effect.system_properties, 0);
  }
  BLI_addtail(&scene.compositor_effects, &new_effect);
  rename_effect(scene, new_effect, source_effect.name, false);
  set_active_effect(scene, new_effect);
  return new_effect;
}

static void free_effect(SceneCompositorEffect &effect)
{
  if (effect.system_properties) {
    IDP_FreeProperty_ex(effect.system_properties, false);
  }
  MEM_delete(&effect);
}

void remove_effect(Scene &scene, SceneCompositorEffect &effect)
{
  if (effect.node_group) {
    id_us_min(&effect.node_group->id);
  }
  BLI_remlink(&scene.compositor_effects, &effect);
  const bool was_active = flag_is_set(effect.flags, SceneCompositorEffectFlags::IsActive);
  if (was_active && !scene.compositor_effects.is_empty()) {
    set_active_effect(scene, *scene.compositor_effects.begin());
  }
  free_effect(effect);
}

void copy_effects(Scene &target_scene, const Scene &source_scene, const int flags)
{
  target_scene.compositor_effects.clear_no_delete();
  for (const SceneCompositorEffect &source_effect : source_scene.compositor_effects) {
    SceneCompositorEffect &new_effect = *MEM_dupalloc(&source_effect);
    BLI_addtail(&target_scene.compositor_effects, &new_effect);
    if (source_effect.system_properties) {
      new_effect.system_properties = IDP_CopyProperty_ex(source_effect.system_properties, flags);
    }
  }
}

void free_effects(Scene &scene)
{
  for (SceneCompositorEffect &effect : scene.compositor_effects.items_mutable()) {
    free_effect(effect);
  }
  scene.compositor_effects.clear_no_delete();
}

void clear_effects(Scene &scene)
{
  for (SceneCompositorEffect &effect : scene.compositor_effects.items_reversed_mutable()) {
    remove_effect(scene, effect);
  }
}

void for_each_id_in_effects(const Scene &scene, LibraryForeachIDData &data)
{
  for (SceneCompositorEffect &effect : scene.compositor_effects) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(&data, effect.node_group, IDWALK_CB_USER);
    if (effect.system_properties) {
      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
          &data,
          IDP_foreach_property(
              effect.system_properties, IDP_TYPE_FILTER_ID, [&](IDProperty *property) {
                BKE_lib_query_idpropertiesForeachIDLink_callback(property, &data);
              }));
    }
  }
}

void write_effects(const Scene &scene, BlendWriter &writer)
{
  writer.write_struct_list(&scene.compositor_effects);
  for (const SceneCompositorEffect &effect : scene.compositor_effects) {
    if (effect.system_properties) {
      IDP_BlendWrite(&writer, effect.system_properties);
    }
  }
}

void read_effects(Scene &scene, BlendDataReader &reader)
{
  BLO_read_struct_list(&reader, SceneCompositorEffect, &scene.compositor_effects);
  for (SceneCompositorEffect &effect : scene.compositor_effects) {
    BLO_read_struct(&reader, IDProperty, &effect.system_properties);
    IDP_BlendDataRead(&reader, &effect.system_properties);
  }
}

const SceneCompositorEffect *get_effect_from_property(const PointerRNA &property_ptr)
{
  const std::optional<AncestorPointerRNA> effect_ptr = RNA_struct_search_closest_ancestor_by_type(
      &property_ptr, RNA_SceneCompositorEffect);
  if (effect_ptr.has_value()) {
    return static_cast<const SceneCompositorEffect *>(effect_ptr->data);
  }

  const Scene *scene = id_cast<const Scene *>(property_ptr.owner_id);
  for (SceneCompositorEffect &effect : scene->compositor_effects) {
    bool found = false;
    IDP_foreach_property(effect.system_properties, 0, [&](IDProperty *id_property) {
      if (id_property == property_ptr.data) {
        found = true;
      }
    });
    if (found) {
      return &effect;
    }
  }
  return nullptr;
}

void update_effect_node_group_interface(Main &main, Scene &scene, SceneCompositorEffect &effect)
{
  if (!effect.system_properties) {
    effect.system_properties =
        bke::idprop::create_group("SceneCompositorEffectProperties").release();
  }

  /* In case the node group is missing, do not update the properties to avoid the values reverting
   * to their default value if the node group later becomes available. */
  if (!effect.node_group || ID_MISSING(effect.node_group)) {
    return;
  }

  PointerRNA properties_ptr = RNA_pointer_create_discrete(
      &scene.id, RNA_SceneCompositorEffectProperties, &effect);
  RNA_ensure_and_sync_system_properties(main, properties_ptr, *effect.system_properties);

  DEG_id_tag_update(&scene.id, ID_RECALC_COMPOSITOR);
  WM_main_add_notifier(NC_SCENE | ND_COMPO_RESULT, &scene);
}

/* --------------------------------------------------------------------
 * Query.
 */

/* Adds the pass names of the passes used by the given Render Layer node to the given used passes.
 * This essentially adds the pass names of the outputs that are logically linked. */
static void add_passes_used_by_render_layer_node(const bNode *node, Set<std::string> &used_passes)
{
  for (const bNodeSocket *output : node->output_sockets()) {
    if (output->is_logically_linked()) {
      /* The combined pass is aliased as Image and Alpha is generated by the node based on the
       * combined pass. */
      if (output->identifier == StringRef("Image") || output->identifier == StringRef("Alpha")) {
        used_passes.add(RE_PASSNAME_COMBINED);
      }
      else {
        used_passes.add(output->identifier);
      }
    }
  }
}

/* Adds the pass names of the passes used by the given Group Input node to the given used passes.
 * The Group Input node only uses the combined pass for the first input, while the rest are
 * ignored. */
static void add_passes_used_by_group_input_node(const bNode *node, Set<std::string> &used_passes)
{
  /* Only the virtual socket exists, so no pass is used. */
  if (node->output_sockets().size() == 1) {
    return;
  }

  if (!node->output_sockets()[0]->is_logically_linked()) {
    return;
  }

  used_passes.add(RE_PASSNAME_COMBINED);
}

/* Adds the pass names of all Cryptomatte layers needed by the given node to the given used passes.
 * Only passes in the given viewer layers are added. */
static void add_passes_used_by_cryptomatte_node(const bNode *node,
                                                const ViewLayer *view_layer,
                                                Set<std::string> &used_passes)
{
  if (node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    return;
  }

  Scene *scene = reinterpret_cast<Scene *>(node->id);
  if (!scene) {
    return;
  }

  cryptomatte::CryptomatteSessionPtr session = cryptomatte::CryptomatteSessionPtr(
      BKE_cryptomatte_init_from_scene(scene, false));

  const Vector<std::string> &layer_names = cryptomatte::BKE_cryptomatte_layer_names_get(*session);
  if (layer_names.is_empty()) {
    return;
  }

  /* If the stored layer name doesn't corresponds to an existing Cryptomatte layer, fall back to
   * the name of the first layer. */
  const NodeCryptomatte *data = static_cast<NodeCryptomatte *>(node->storage);
  const std::string layer_name = layer_names.contains(data->layer_name) ? data->layer_name :
                                                                          layer_names[0];

  /* Does not use passes from the given view layer, so no need to add anything. */
  if (!StringRef(layer_name).startswith(view_layer->name)) {
    return;
  }

  /* Find out which type of Cryptomatte layers the node needs. Also ensure the type is enabled in
   * the view layer, because the node can use one of the types as a placeholder. */
  const char *cryptomatte_type_name = nullptr;
  if (StringRef(layer_name).endswith(RE_PASSNAME_CRYPTOMATTE_OBJECT)) {
    if (view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_OBJECT) {
      cryptomatte_type_name = RE_PASSNAME_CRYPTOMATTE_OBJECT;
    }
  }
  else if (StringRef(layer_name).endswith(RE_PASSNAME_CRYPTOMATTE_ASSET)) {
    if (view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_ASSET) {
      cryptomatte_type_name = RE_PASSNAME_CRYPTOMATTE_ASSET;
    }
  }
  else if (StringRef(layer_name).endswith(RE_PASSNAME_CRYPTOMATTE_MATERIAL)) {
    if (view_layer->cryptomatte_flag & VIEW_LAYER_CRYPTOMATTE_MATERIAL) {
      cryptomatte_type_name = RE_PASSNAME_CRYPTOMATTE_MATERIAL;
    }
  }

  if (!cryptomatte_type_name) {
    return;
  }

  /* Each layer stores two ranks/levels, so do ceiling division by two. */
  const int cryptomatte_layers_count = int(math::ceil(view_layer->cryptomatte_levels / 2.0f));
  for (const int i : IndexRange(cryptomatte_layers_count)) {
    used_passes.add(fmt::format("{}{:02}", cryptomatte_type_name, i));
  }
}

/* Adds the pass names of the passes used by the given compositor node tree to the given used
 * passes. This is called recursively for node groups. */
static void add_used_passes_recursive(const bNodeTree *node_tree,
                                      const ViewLayer *view_layer,
                                      const bool is_root_tree_of_first_effect,
                                      Set<const bNodeTree *> &node_trees_already_searched,
                                      Set<std::string> &used_passes)
{
  if (node_tree == nullptr) {
    return;
  }

  node_tree->ensure_topology_cache();
  for (const bNode *node : node_tree->all_nodes()) {
    if (node->is_muted()) {
      continue;
    }

    switch (node->type_legacy) {
      case NODE_GROUP:
      case NODE_CUSTOM_GROUP: {
        const bNodeTree *node_group_tree = reinterpret_cast<const bNodeTree *>(node->id);
        if (node_trees_already_searched.add(node_group_tree)) {
          add_used_passes_recursive(
              node_group_tree, view_layer, false, node_trees_already_searched, used_passes);
        }
        break;
      }
      case CMP_NODE_R_LAYERS:
        add_passes_used_by_render_layer_node(node, used_passes);
        break;
      case NODE_GROUP_INPUT:
        if (is_root_tree_of_first_effect) {
          add_passes_used_by_group_input_node(node, used_passes);
        }
        break;
      case CMP_NODE_CRYPTOMATTE:
        add_passes_used_by_cryptomatte_node(node, view_layer, used_passes);
        break;
      default:
        break;
    }
  }
}

Set<std::string> get_used_passes(const Scene &scene,
                                 const ViewLayer *view_layer,
                                 const ExecutionMode mode)
{
  Set<std::string> used_passes;
  bool is_first_effect = true;
  Set<const bNodeTree *> node_trees_already_searched;
  for (const SceneCompositorEffect &effect : scene.compositor_effects) {
    if (!is_effect_enabled(effect, mode)) {
      continue;
    }
    add_used_passes_recursive(
        effect.node_group, view_layer, is_first_effect, node_trees_already_searched, used_passes);
    is_first_effect = false;
  }
  return used_passes;
}

bool is_viewport_compositor_enabled(const View3D &view_3d, const RegionView3D &region_view_3d)
{
  if (view_3d.shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
    return false;
  }

  if (!ELEM(view_3d.shading.type, OB_MATERIAL, OB_TEXTURE, OB_RENDER)) {
    return false;
  }

  if (view_3d.shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_CAMERA &&
      region_view_3d.persp != RV3D_CAMOB)
  {
    return false;
  }

  return true;
}

bool is_viewport_compositor_used(const bContext &context)
{
  const Scene *scene = CTX_data_scene(&context);
  if (!is_enabled(*scene, ExecutionMode::Preview)) {
    return false;
  }

  wmWindowManager *window_manager = CTX_wm_manager(&context);
  for (const wmWindow &window : window_manager->windows) {
    const bScreen *screen = WM_window_get_active_screen(&window);
    for (const ScrArea &area : screen->areabase) {
      const SpaceLink &space = *static_cast<const SpaceLink *>(area.spacedata.first);
      if (space.spacetype == SPACE_VIEW3D) {
        const View3D &view_3d = reinterpret_cast<const View3D &>(space);
        for (ARegion &region : area.regionbase) {
          if (region.regiontype == RGN_TYPE_WINDOW) {
            const RegionView3D &region_view_3d = *static_cast<RegionView3D *>(region.regiondata);
            if (is_viewport_compositor_enabled(view_3d, region_view_3d)) {
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

/* --------------------------------------------------------------------
 * Depsgraph.
 */

void add_depsgraph_relations(Scene &scene,
                             const SceneCompositorEffect &effect,
                             DepsNodeHandle *compositor_output_depsgraph_node)
{
  nodes::EvalDependencies evaluation_dependencies = nodes::gather_eval_dependencies_recursive(
      *effect.node_group);

  IDP_foreach_property(effect.system_properties, IDP_TYPE_FILTER_ID, [&](IDProperty *property) {
    if (ID *id = IDP_ID_get(property)) {
      evaluation_dependencies.add_generic_id_full(id);
    }
  });

  for (ID *id : evaluation_dependencies.ids.values()) {
    switch (ID_Type(GS(id->name))) {
      case ID_OB: {
        Object *object = reinterpret_cast<Object *>(id);
        const nodes::EvalDependencies::ObjectDependencyInfo &info =
            evaluation_dependencies.objects_info.lookup_default(object->id.session_uid, {});
        if (info.transform) {
          DEG_add_object_relation(compositor_output_depsgraph_node,
                                  object,
                                  DEG_OB_COMP_TRANSFORM,
                                  "Object Transform -> Compositor");
        }
        if (object->type == OB_CAMERA && info.camera_parameters) {
          DEG_add_object_relation(compositor_output_depsgraph_node,
                                  object,
                                  DEG_OB_COMP_PARAMETERS,
                                  "Camera Parameters -> Compositor");
        }
        break;
      }
      case ID_IM:
        DEG_add_generic_id_relation(compositor_output_depsgraph_node, id, "Image -> Compositor");
        break;
      case ID_TE:
        DEG_add_generic_id_relation(compositor_output_depsgraph_node, id, "Texture -> Compositor");
        break;
      case ID_VF:
        DEG_add_vfont_relation(
            compositor_output_depsgraph_node, reinterpret_cast<VFont *>(id), "Font -> Compositor");
        break;
      default:
        break;
    }
  }

  if (evaluation_dependencies.needs_active_camera) {
    DEG_add_scene_camera_relation(compositor_output_depsgraph_node,
                                  &scene,
                                  DEG_OB_COMP_TRANSFORM,
                                  "Active Camera Transforms -> Compositor");
  }

  /* Active camera is a scene parameter that can change, so we need a relation for that, too. */
  if (evaluation_dependencies.needs_active_camera ||
      evaluation_dependencies.needs_scene_render_params)
  {
    DEG_add_scene_relation(compositor_output_depsgraph_node,
                           &scene,
                           DEG_SCENE_COMP_PARAMETERS,
                           "Active Camera Parameters -> Compositor");
  }

  if (evaluation_dependencies.time_dependent) {
    DEG_add_time_source_relation(compositor_output_depsgraph_node, "Time Source -> Compositor");
  }
}

/* --------------------------------------------------------------------
 * Compute Contexts.
 */

/* Recursively search node groups to find the node group whose instance key matches the given
 * active node group instance key, and returns it compute context hash. */
static std::optional<ComputeContextHash> compute_active_compute_context_hash_recursive(
    const bNodeTree &node_group,
    const ComputeContext &compute_context,
    const bNodeInstanceKey instance_key,
    const bNodeInstanceKey active_node_group_instance_key)
{
  /* If this is the active node group, returns it hash.  */
  if (active_node_group_instance_key == instance_key) {
    return compute_context.hash();
  }

  /* Otherwise, we have to check node groups recursively. */
  node_group.ensure_topology_cache();
  for (const bNode *group_node : node_group.group_nodes()) {
    if (!group_node->id || ID_MISSING(group_node->id)) {
      continue;
    }

    const bNodeTree &child_node_group = *id_cast<const bNodeTree *>(group_node->id);
    const bNodeInstanceKey child_instance_key = bke::node_instance_key(
        instance_key, &node_group, group_node);
    const bke::GroupNodeComputeContext child_compute_context(
        &compute_context, group_node->identifier, &group_node->owner_tree());
    std::optional<ComputeContextHash> hash = compute_active_compute_context_hash_recursive(
        child_node_group,
        child_compute_context,
        child_instance_key,
        active_node_group_instance_key);
    if (hash.has_value()) {
      return hash;
    }
  }

  return std::nullopt;
}

ComputeContextHash compute_active_compute_context_hash(const Scene &scene)
{
  const bke::DataBlockComputeContext scene_compute_context(nullptr, scene.id);
  const SceneCompositorEffect *active_effect = get_active_effect(scene);
  if (!active_effect) {
    return scene_compute_context.hash();
  }

  const bke::SceneCompositorEffectComputeContext effect_compute_context(&scene_compute_context,
                                                                        *active_effect);

  if (!active_effect->node_group || ID_MISSING(active_effect->node_group)) {
    return effect_compute_context.hash();
  }

  return compute_active_compute_context_hash_recursive(
             *active_effect->node_group,
             effect_compute_context,
             bke::NODE_INSTANCE_KEY_BASE,
             active_effect->node_group->active_viewer_key)
      .value_or(effect_compute_context.hash());
}

ComputeContextHash compute_active_compute_context_hash(const Scene &scene,
                                                       const bNodeTree &root_node_group)
{
  const bke::DataBlockComputeContext root_compute_context(nullptr, scene.id);
  return compute_active_compute_context_hash_recursive(root_node_group,
                                                       root_compute_context,
                                                       bke::NODE_INSTANCE_KEY_BASE,
                                                       root_node_group.active_viewer_key)
      .value_or(root_compute_context.hash());
}

}  // namespace blender::bke::compositor
