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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

/* Enable special trickery to treat nested owned IDs (such as nodetree of
 * material) to be handled in same way as "real" data-blocks, even tho some
 * internal BKE routines doesn't treat them like that.
 *
 * TODO(sergey): Re-evaluate that after new ID handling is in place. */
#define NESTED_ID_NASTY_WORKAROUND

/* Silence warnings from copying deprecated fields. */
#define DNA_DEPRECATED_ALLOW

#include "intern/eval/deg_eval_copy_on_write.h"

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_simulation_types.h"
#include "DNA_sound_types.h"

#include "DRW_engine.h"

#ifdef NESTED_ID_NASTY_WORKAROUND
#  include "DNA_curve_types.h"
#  include "DNA_key_types.h"
#  include "DNA_lattice_types.h"
#  include "DNA_light_types.h"
#  include "DNA_linestyle_types.h"
#  include "DNA_material_types.h"
#  include "DNA_meta_types.h"
#  include "DNA_node_types.h"
#  include "DNA_texture_types.h"
#  include "DNA_world_types.h"
#endif

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_editmesh.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_nodes.h"
#include "intern/depsgraph.h"
#include "intern/eval/deg_eval_runtime_backup.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_id.h"

namespace DEG {

#define DEBUG_PRINT \
  if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) \
  printf

namespace {

#ifdef NESTED_ID_NASTY_WORKAROUND
union NestedIDHackTempStorage {
  Curve curve;
  FreestyleLineStyle linestyle;
  Light lamp;
  Lattice lattice;
  Material material;
  Mesh mesh;
  Scene scene;
  Tex tex;
  World world;
};

/* Set nested owned ID pointers to nullptr. */
void nested_id_hack_discard_pointers(ID *id_cow)
{
  switch (GS(id_cow->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field) \
    case id_type: { \
      ((dna_type *)id_cow)->field = nullptr; \
      break; \
    }

    SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree)
    SPECIAL_CASE(ID_LA, Light, nodetree)
    SPECIAL_CASE(ID_MA, Material, nodetree)
    SPECIAL_CASE(ID_TE, Tex, nodetree)
    SPECIAL_CASE(ID_WO, World, nodetree)

    SPECIAL_CASE(ID_CU, Curve, key)
    SPECIAL_CASE(ID_LT, Lattice, key)
    SPECIAL_CASE(ID_ME, Mesh, key)

    case ID_SCE: {
      Scene *scene_cow = (Scene *)id_cow;
      /* Node trees always have their own ID node in the graph, and are
       * being copied as part of their copy-on-write process. */
      scene_cow->nodetree = nullptr;
      /* Tool settings pointer is shared with the original scene. */
      scene_cow->toolsettings = nullptr;
      break;
    }

    case ID_OB: {
      /* Clear the ParticleSettings pointer to prevent doubly-freeing it. */
      Object *ob = (Object *)id_cow;
      LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
        psys->part = nullptr;
      }
      break;
    }
#  undef SPECIAL_CASE

    default:
      break;
  }
}

/* Set ID pointer of nested owned IDs (nodetree, key) to nullptr.
 *
 * Return pointer to a new ID to be used. */
const ID *nested_id_hack_get_discarded_pointers(NestedIDHackTempStorage *storage, const ID *id)
{
  switch (GS(id->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field, variable) \
    case id_type: { \
      storage->variable = *(dna_type *)id; \
      storage->variable.field = nullptr; \
      return &storage->variable.id; \
    }

    SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree, linestyle)
    SPECIAL_CASE(ID_LA, Light, nodetree, lamp)
    SPECIAL_CASE(ID_MA, Material, nodetree, material)
    SPECIAL_CASE(ID_TE, Tex, nodetree, tex)
    SPECIAL_CASE(ID_WO, World, nodetree, world)

    SPECIAL_CASE(ID_CU, Curve, key, curve)
    SPECIAL_CASE(ID_LT, Lattice, key, lattice)
    SPECIAL_CASE(ID_ME, Mesh, key, mesh)

    case ID_SCE: {
      storage->scene = *(Scene *)id;
      storage->scene.toolsettings = nullptr;
      storage->scene.nodetree = nullptr;
      return &storage->scene.id;
    }

#  undef SPECIAL_CASE

    default:
      break;
  }
  return id;
}

/* Set ID pointer of nested owned IDs (nodetree, key) to the original value. */
void nested_id_hack_restore_pointers(const ID *old_id, ID *new_id)
{
  if (new_id == nullptr) {
    return;
  }
  switch (GS(old_id->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field) \
    case id_type: { \
      ((dna_type *)(new_id))->field = ((dna_type *)(old_id))->field; \
      break; \
    }

    SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree)
    SPECIAL_CASE(ID_LA, Light, nodetree)
    SPECIAL_CASE(ID_MA, Material, nodetree)
    SPECIAL_CASE(ID_SCE, Scene, nodetree)
    SPECIAL_CASE(ID_TE, Tex, nodetree)
    SPECIAL_CASE(ID_WO, World, nodetree)

    SPECIAL_CASE(ID_CU, Curve, key)
    SPECIAL_CASE(ID_LT, Lattice, key)
    SPECIAL_CASE(ID_ME, Mesh, key)

#  undef SPECIAL_CASE
    default:
      break;
  }
}

/* Remap pointer of nested owned IDs (nodetree. key) to the new ID values. */
void ntree_hack_remap_pointers(const Depsgraph *depsgraph, ID *id_cow)
{
  switch (GS(id_cow->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field, field_type) \
    case id_type: { \
      dna_type *data = (dna_type *)id_cow; \
      if (data->field != nullptr) { \
        ID *ntree_id_cow = depsgraph->get_cow_id(&data->field->id); \
        if (ntree_id_cow != nullptr) { \
          DEG_COW_PRINT("    Remapping datablock for %s: id_orig=%p id_cow=%p\n", \
                        data->field->id.name, \
                        data->field, \
                        ntree_id_cow); \
          data->field = (field_type *)ntree_id_cow; \
        } \
      } \
      break; \
    }

    SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree, bNodeTree)
    SPECIAL_CASE(ID_LA, Light, nodetree, bNodeTree)
    SPECIAL_CASE(ID_MA, Material, nodetree, bNodeTree)
    SPECIAL_CASE(ID_SCE, Scene, nodetree, bNodeTree)
    SPECIAL_CASE(ID_TE, Tex, nodetree, bNodeTree)
    SPECIAL_CASE(ID_WO, World, nodetree, bNodeTree)

    SPECIAL_CASE(ID_CU, Curve, key, Key)
    SPECIAL_CASE(ID_LT, Lattice, key, Key)
    SPECIAL_CASE(ID_ME, Mesh, key, Key)

#  undef SPECIAL_CASE
    default:
      break;
  }
}
#endif /* NODETREE_NASTY_WORKAROUND */

struct ValidateData {
  bool is_valid;
};

/* Similar to generic BKE_id_copy() but does not require main and assumes pointer
 * is already allocated. */
bool id_copy_inplace_no_main(const ID *id, ID *newid)
{
  const ID *id_for_copy = id;

#ifdef NESTED_ID_NASTY_WORKAROUND
  NestedIDHackTempStorage id_hack_storage;
  id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage, id);
#endif

  bool result = BKE_id_copy_ex(
      nullptr, (ID *)id_for_copy, &newid, (LIB_ID_COPY_LOCALIZE | LIB_ID_CREATE_NO_ALLOCATE));

#ifdef NESTED_ID_NASTY_WORKAROUND
  if (result) {
    nested_id_hack_restore_pointers(id, newid);
  }
#endif

  return result;
}

/* Similar to BKE_scene_copy() but does not require main and assumes pointer
 * is already allocated. */
bool scene_copy_inplace_no_main(const Scene *scene, Scene *new_scene)
{
  const ID *id_for_copy = &scene->id;

#ifdef NESTED_ID_NASTY_WORKAROUND
  NestedIDHackTempStorage id_hack_storage;
  id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage, &scene->id);
#endif

  bool result = BKE_id_copy_ex(
      nullptr, id_for_copy, (ID **)&new_scene, LIB_ID_COPY_LOCALIZE | LIB_ID_CREATE_NO_ALLOCATE);

#ifdef NESTED_ID_NASTY_WORKAROUND
  if (result) {
    nested_id_hack_restore_pointers(&scene->id, &new_scene->id);
  }
#endif

  return result;
}

/* For the given scene get view layer which corresponds to an original for the
 * scene's evaluated one. This depends on how the scene is pulled into the
 * dependency  graph. */
ViewLayer *get_original_view_layer(const Depsgraph *depsgraph, const IDNode *id_node)
{
  if (id_node->linked_state == DEG_ID_LINKED_DIRECTLY) {
    return depsgraph->view_layer;
  }
  else if (id_node->linked_state == DEG_ID_LINKED_VIA_SET) {
    Scene *scene_orig = reinterpret_cast<Scene *>(id_node->id_orig);
    return BKE_view_layer_default_render(scene_orig);
  }
  /* Is possible to have scene linked indirectly (i.e. via the driver) which
   * we need to support. Currently there are issues somewhere else, which
   * makes testing hard. This is a reported problem, so will eventually be
   * properly fixed.
   *
   * TODO(sergey): Support indirectly linked scene. */
  return nullptr;
}

/* Remove all view layers but the one which corresponds to an input one. */
void scene_remove_unused_view_layers(const Depsgraph *depsgraph,
                                     const IDNode *id_node,
                                     Scene *scene_cow)
{
  const ViewLayer *view_layer_input;
  if (depsgraph->is_render_pipeline_depsgraph) {
    /* If the dependency graph is used for post-processing (such as compositor) we do need to
     * have access to its view layer names so can not remove any view layers.
     * On a more positive side we can remove all the bases from all the view layers.
     *
     * NOTE: Need to clear pointers which might be pointing to original on freed (due to being
     * unused) data.
     *
     * NOTE: Need to keep view layers for all scenes, even indirect ones. This is because of
     * render layer node possibly pointing to another scene. */
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene_cow->view_layers) {
      view_layer->basact = nullptr;
    }
    return;
  }
  else if (id_node->linked_state == DEG_ID_LINKED_INDIRECTLY) {
    /* Indirectly linked scenes means it's not an input scene and not a set scene, and is pulled
     * via some driver. Such scenes should not have view layers after copy. */
    view_layer_input = nullptr;
  }
  else {
    view_layer_input = get_original_view_layer(depsgraph, id_node);
  }
  ViewLayer *view_layer_eval = nullptr;
  /* Find evaluated view layer. At the same time we free memory used by
   * all other of the view layers. */
  for (ViewLayer *view_layer_cow = reinterpret_cast<ViewLayer *>(scene_cow->view_layers.first),
                 *view_layer_next;
       view_layer_cow != nullptr;
       view_layer_cow = view_layer_next) {
    view_layer_next = view_layer_cow->next;
    if (view_layer_input != nullptr && STREQ(view_layer_input->name, view_layer_cow->name)) {
      view_layer_eval = view_layer_cow;
    }
    else {
      BKE_view_layer_free_ex(view_layer_cow, false);
    }
  }
  /* Make evaluated view layer the only one in the evaluated scene (if it exists). */
  if (view_layer_eval != nullptr) {
    view_layer_eval->prev = view_layer_eval->next = nullptr;
  }
  scene_cow->view_layers.first = view_layer_eval;
  scene_cow->view_layers.last = view_layer_eval;
}

void scene_remove_all_bases(Scene *scene_cow)
{
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene_cow->view_layers) {
    BLI_freelistN(&view_layer->object_bases);
  }
}

/* Makes it so given view layer only has bases corresponding to enabled
 * objects. */
void view_layer_remove_disabled_bases(const Depsgraph *depsgraph, ViewLayer *view_layer)
{
  if (view_layer == nullptr) {
    return;
  }
  ListBase enabled_bases = {nullptr, nullptr};
  LISTBASE_FOREACH_MUTABLE (Base *, base, &view_layer->object_bases) {
    /* TODO(sergey): Would be cool to optimize this somehow, or make it so
     * builder tags bases.
     *
     * NOTE: The idea of using id's tag and check whether its copied ot not
     * is not reliable, since object might be indirectly linked into the
     * graph.
     *
     * NOTE: We are using original base since the object which evaluated base
     * points to is not yet copied. This is dangerous access from evaluated
     * domain to original one, but this is how the entire copy-on-write works:
     * it does need to access original for an initial copy. */
    const bool is_object_enabled = deg_check_base_in_depsgraph(depsgraph, base);
    if (is_object_enabled) {
      BLI_addtail(&enabled_bases, base);
    }
    else {
      if (base == view_layer->basact) {
        view_layer->basact = nullptr;
      }
      MEM_freeN(base);
    }
  }
  view_layer->object_bases = enabled_bases;
}

void view_layer_update_orig_base_pointers(const ViewLayer *view_layer_orig,
                                          ViewLayer *view_layer_eval)
{
  if (view_layer_orig == nullptr || view_layer_eval == nullptr) {
    /* Happens when scene is only used for parameters or compositor/sequencer. */
    return;
  }
  Base *base_orig = reinterpret_cast<Base *>(view_layer_orig->object_bases.first);
  LISTBASE_FOREACH (Base *, base_eval, &view_layer_eval->object_bases) {
    base_eval->base_orig = base_orig;
    base_orig = base_orig->next;
  }
}

void scene_setup_view_layers_before_remap(const Depsgraph *depsgraph,
                                          const IDNode *id_node,
                                          Scene *scene_cow)
{
  scene_remove_unused_view_layers(depsgraph, id_node, scene_cow);
  /* If dependency graph is used for post-processing we don't need any bases and can free of them.
   * Do it before re-mapping to make that process faster. */
  if (depsgraph->is_render_pipeline_depsgraph) {
    scene_remove_all_bases(scene_cow);
  }
}

void scene_setup_view_layers_after_remap(const Depsgraph *depsgraph,
                                         const IDNode *id_node,
                                         Scene *scene_cow)
{
  const ViewLayer *view_layer_orig = get_original_view_layer(depsgraph, id_node);
  ViewLayer *view_layer_eval = reinterpret_cast<ViewLayer *>(scene_cow->view_layers.first);
  view_layer_update_orig_base_pointers(view_layer_orig, view_layer_eval);
  view_layer_remove_disabled_bases(depsgraph, view_layer_eval);
  /* TODO(sergey): Remove objects from collections as well.
   * Not a HUGE deal for now, nobody is looking into those CURRENTLY.
   * Still not an excuse to have those. */
}

void update_sequence_orig_pointers(const ListBase *sequences_orig, ListBase *sequences_cow)
{
  Sequence *sequence_orig = reinterpret_cast<Sequence *>(sequences_orig->first);
  Sequence *sequence_cow = reinterpret_cast<Sequence *>(sequences_cow->first);
  while (sequence_orig != nullptr) {
    update_sequence_orig_pointers(&sequence_orig->seqbase, &sequence_cow->seqbase);
    sequence_cow->orig_sequence = sequence_orig;
    sequence_cow = sequence_cow->next;
    sequence_orig = sequence_orig->next;
  }
}

void update_scene_orig_pointers(const Scene *scene_orig, Scene *scene_cow)
{
  if (scene_orig->ed != nullptr) {
    update_sequence_orig_pointers(&scene_orig->ed->seqbase, &scene_cow->ed->seqbase);
  }
}

/* Check whether given ID is expanded or still a shallow copy. */
BLI_INLINE bool check_datablock_expanded(const ID *id_cow)
{
  return (id_cow->name[0] != '\0');
}

/* Callback for BKE_library_foreach_ID_link which remaps original ID pointer
 * with the one created by CoW system. */

struct RemapCallbackUserData {
  /* Dependency graph for which remapping is happening. */
  const Depsgraph *depsgraph;
  /* Create placeholder for ID nodes for cases when we need to remap original
   * ID to it[s CoW version but we don't have required ID node yet.
   *
   * This happens when expansion happens a ta construction time. */
  DepsgraphNodeBuilder *node_builder;
  bool create_placeholders;
};

int foreach_libblock_remap_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;
  if (*id_p == nullptr) {
    return IDWALK_RET_NOP;
  }

  ID *id_self = cb_data->id_self;
  RemapCallbackUserData *user_data = (RemapCallbackUserData *)cb_data->user_data;
  const Depsgraph *depsgraph = user_data->depsgraph;
  ID *id_orig = *id_p;
  if (deg_copy_on_write_is_needed(id_orig)) {
    ID *id_cow;
    if (user_data->create_placeholders) {
      /* Special workaround to stop creating temp datablocks for
       * objects which are coming from scene's collection and which
       * are never linked to any of layers.
       *
       * TODO(sergey): Ideally we need to tell ID looper to ignore
       * those or at least make it more reliable check where the
       * pointer is coming from. */
      const ID_Type id_type = GS(id_orig->name);
      const ID_Type id_type_self = GS(id_self->name);
      if (id_type == ID_OB && id_type_self == ID_SCE) {
        IDNode *id_node = depsgraph->find_id_node(id_orig);
        if (id_node == nullptr) {
          id_cow = id_orig;
        }
        else {
          id_cow = id_node->id_cow;
        }
      }
      else {
        id_cow = user_data->node_builder->ensure_cow_id(id_orig);
      }
    }
    else {
      id_cow = depsgraph->get_cow_id(id_orig);
    }
    BLI_assert(id_cow != nullptr);
    DEG_COW_PRINT(
        "    Remapping datablock for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
    *id_p = id_cow;
  }
  return IDWALK_RET_NOP;
}

void update_armature_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                        const ID *id_orig,
                                        ID *id_cow)
{
  const bArmature *armature_orig = (const bArmature *)id_orig;
  bArmature *armature_cow = (bArmature *)id_cow;
  armature_cow->edbo = armature_orig->edbo;
  armature_cow->act_edbone = armature_orig->act_edbone;
}

void update_curve_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                     const ID *id_orig,
                                     ID *id_cow)
{
  const Curve *curve_orig = (const Curve *)id_orig;
  Curve *curve_cow = (Curve *)id_cow;
  curve_cow->editnurb = curve_orig->editnurb;
  curve_cow->editfont = curve_orig->editfont;
}

void update_mball_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                     const ID *id_orig,
                                     ID *id_cow)
{
  const MetaBall *mball_orig = (const MetaBall *)id_orig;
  MetaBall *mball_cow = (MetaBall *)id_cow;
  mball_cow->editelems = mball_orig->editelems;
}

void update_lattice_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                       const ID *id_orig,
                                       ID *id_cow)
{
  const Lattice *lt_orig = (const Lattice *)id_orig;
  Lattice *lt_cow = (Lattice *)id_cow;
  lt_cow->editlatt = lt_orig->editlatt;
}

void update_mesh_edit_mode_pointers(const ID *id_orig, ID *id_cow)
{
  /* For meshes we need to update edit_mesh to make it to point
   * to the CoW version of object.
   *
   * This is kind of confusing, because actual bmesh is not owned by
   * the CoW object, so need to be accurate about using link from
   * edit_mesh to object. */
  const Mesh *mesh_orig = (const Mesh *)id_orig;
  Mesh *mesh_cow = (Mesh *)id_cow;
  if (mesh_orig->edit_mesh == nullptr) {
    return;
  }
  mesh_cow->edit_mesh = (BMEditMesh *)MEM_dupallocN(mesh_orig->edit_mesh);
  mesh_cow->edit_mesh->mesh_eval_cage = nullptr;
  mesh_cow->edit_mesh->mesh_eval_final = nullptr;
}

/* Edit data is stored and owned by original datablocks, copied ones
 * are simply referencing to them. */
void update_edit_mode_pointers(const Depsgraph *depsgraph, const ID *id_orig, ID *id_cow)
{
  const ID_Type type = GS(id_orig->name);
  switch (type) {
    case ID_AR:
      update_armature_edit_mode_pointers(depsgraph, id_orig, id_cow);
      break;
    case ID_ME:
      update_mesh_edit_mode_pointers(id_orig, id_cow);
      break;
    case ID_CU:
      update_curve_edit_mode_pointers(depsgraph, id_orig, id_cow);
      break;
    case ID_MB:
      update_mball_edit_mode_pointers(depsgraph, id_orig, id_cow);
      break;
    case ID_LT:
      update_lattice_edit_mode_pointers(depsgraph, id_orig, id_cow);
      break;
    default:
      break;
  }
}

template<typename T>
void update_list_orig_pointers(const ListBase *listbase_orig,
                               ListBase *listbase,
                               T *T::*orig_field)
{
  T *element_orig = reinterpret_cast<T *>(listbase_orig->first);
  T *element_cow = reinterpret_cast<T *>(listbase->first);
  while (element_orig != nullptr) {
    element_cow->*orig_field = element_orig;
    element_cow = element_cow->next;
    element_orig = element_orig->next;
  }
}

void update_particle_system_orig_pointers(const Object *object_orig, Object *object_cow)
{
  update_list_orig_pointers(
      &object_orig->particlesystem, &object_cow->particlesystem, &ParticleSystem::orig_psys);
}

void set_particle_system_modifiers_loaded(Object *object_cow)
{
  LISTBASE_FOREACH (ModifierData *, md, &object_cow->modifiers) {
    if (md->type != eModifierType_ParticleSystem) {
      continue;
    }
    ParticleSystemModifierData *psmd = reinterpret_cast<ParticleSystemModifierData *>(md);
    psmd->flag |= eParticleSystemFlag_file_loaded;
  }
}

void reset_particle_system_edit_eval(const Depsgraph *depsgraph, Object *object_cow)
{
  /* Inactive (and render) dependency graphs are living in own little bubble, should not care about
   * edit mode at all. */
  if (!DEG_is_active(reinterpret_cast<const ::Depsgraph *>(depsgraph))) {
    return;
  }
  LISTBASE_FOREACH (ParticleSystem *, psys, &object_cow->particlesystem) {
    ParticleSystem *orig_psys = psys->orig_psys;
    if (orig_psys->edit != nullptr) {
      orig_psys->edit->psys_eval = nullptr;
      orig_psys->edit->psmd_eval = nullptr;
    }
  }
}

void update_particles_after_copy(const Depsgraph *depsgraph,
                                 const Object *object_orig,
                                 Object *object_cow)
{
  update_particle_system_orig_pointers(object_orig, object_cow);
  set_particle_system_modifiers_loaded(object_cow);
  reset_particle_system_edit_eval(depsgraph, object_cow);
}

void update_pose_orig_pointers(const bPose *pose_orig, bPose *pose_cow)
{
  update_list_orig_pointers(&pose_orig->chanbase, &pose_cow->chanbase, &bPoseChannel::orig_pchan);
}

void update_modifiers_orig_pointers(const Object *object_orig, Object *object_cow)
{
  update_list_orig_pointers(
      &object_orig->modifiers, &object_cow->modifiers, &ModifierData::orig_modifier_data);
}

void update_simulation_states_orig_pointers(const Simulation *simulation_orig,
                                            Simulation *simulation_cow)
{
  update_list_orig_pointers(
      &simulation_orig->states, &simulation_cow->states, &SimulationState::orig_state);
}

void update_nla_strips_orig_pointers(const ListBase *strips_orig, ListBase *strips_cow)
{
  NlaStrip *strip_orig = reinterpret_cast<NlaStrip *>(strips_orig->first);
  NlaStrip *strip_cow = reinterpret_cast<NlaStrip *>(strips_cow->first);
  while (strip_orig != nullptr) {
    strip_cow->orig_strip = strip_orig;
    update_nla_strips_orig_pointers(&strip_orig->strips, &strip_cow->strips);
    strip_cow = strip_cow->next;
    strip_orig = strip_orig->next;
  }
}

void update_nla_tracks_orig_pointers(const ListBase *tracks_orig, ListBase *tracks_cow)
{
  NlaTrack *track_orig = reinterpret_cast<NlaTrack *>(tracks_orig->first);
  NlaTrack *track_cow = reinterpret_cast<NlaTrack *>(tracks_cow->first);
  while (track_orig != nullptr) {
    update_nla_strips_orig_pointers(&track_orig->strips, &track_cow->strips);
    track_cow = track_cow->next;
    track_orig = track_orig->next;
  }
}

void update_animation_data_after_copy(const ID *id_orig, ID *id_cow)
{
  const AnimData *anim_data_orig = BKE_animdata_from_id(const_cast<ID *>(id_orig));
  if (anim_data_orig == nullptr) {
    return;
  }
  AnimData *anim_data_cow = BKE_animdata_from_id(id_cow);
  BLI_assert(anim_data_cow != nullptr);
  update_nla_tracks_orig_pointers(&anim_data_orig->nla_tracks, &anim_data_cow->nla_tracks);
}

/* Some builders (like motion path one) will ignore proxies from being built. This code makes it so
 * proxy and proxy_group pointers never point to an original objects, preventing evaluation code
 * from assign evaluated pointer to an original proxy->proxy_from. */
void update_proxy_pointers_after_copy(const Depsgraph *depsgraph,
                                      const Object *object_orig,
                                      Object *object_cow)
{
  if (object_cow->proxy != nullptr) {
    if (!deg_check_id_in_depsgraph(depsgraph, &object_orig->proxy->id)) {
      object_cow->proxy = nullptr;
    }
  }
  if (object_cow->proxy_group != nullptr) {
    if (!deg_check_id_in_depsgraph(depsgraph, &object_orig->proxy_group->id)) {
      object_cow->proxy_group = nullptr;
    }
  }
}

/* Do some special treatment of data transfer from original ID to it's
 * CoW complementary part.
 *
 * Only use for the newly created CoW data-blocks. */
void update_id_after_copy(const Depsgraph *depsgraph,
                          const IDNode *id_node,
                          const ID *id_orig,
                          ID *id_cow)
{
  const ID_Type type = GS(id_orig->name);
  update_animation_data_after_copy(id_orig, id_cow);
  switch (type) {
    case ID_OB: {
      /* Ensure we don't drag someone's else derived mesh to the
       * new copy of the object. */
      Object *object_cow = (Object *)id_cow;
      const Object *object_orig = (const Object *)id_orig;
      object_cow->mode = object_orig->mode;
      object_cow->sculpt = object_orig->sculpt;
      object_cow->runtime.data_orig = (ID *)object_cow->data;
      if (object_cow->type == OB_ARMATURE) {
        const bArmature *armature_orig = (bArmature *)object_orig->data;
        bArmature *armature_cow = (bArmature *)object_cow->data;
        BKE_pose_remap_bone_pointers(armature_cow, object_cow->pose);
        if (armature_orig->edbo == nullptr) {
          update_pose_orig_pointers(object_orig->pose, object_cow->pose);
        }
        BKE_pose_pchan_index_rebuild(object_cow->pose);
      }
      if (object_cow->type == OB_GPENCIL) {
        BKE_gpencil_update_orig_pointers(object_orig, object_cow);
      }
      update_particles_after_copy(depsgraph, object_orig, object_cow);
      update_modifiers_orig_pointers(object_orig, object_cow);
      update_proxy_pointers_after_copy(depsgraph, object_orig, object_cow);
      break;
    }
    case ID_SCE: {
      Scene *scene_cow = (Scene *)id_cow;
      const Scene *scene_orig = (const Scene *)id_orig;
      scene_cow->toolsettings = scene_orig->toolsettings;
      scene_cow->eevee.light_cache_data = scene_orig->eevee.light_cache_data;
      scene_setup_view_layers_after_remap(depsgraph, id_node, reinterpret_cast<Scene *>(id_cow));
      update_scene_orig_pointers(scene_orig, scene_cow);
      break;
    }
    case ID_SIM: {
      Simulation *simulation_cow = (Simulation *)id_cow;
      const Simulation *simulation_orig = (const Simulation *)id_orig;
      update_simulation_states_orig_pointers(simulation_orig, simulation_cow);
      break;
    }
    default:
      break;
  }
  update_edit_mode_pointers(depsgraph, id_orig, id_cow);
  BKE_animsys_update_driver_array(id_cow);
}

/* This callback is used to validate that all nested ID data-blocks are
 * properly expanded. */
int foreach_libblock_validate_callback(LibraryIDLinkCallbackData *cb_data)
{
  ValidateData *data = (ValidateData *)cb_data->user_data;
  ID **id_p = cb_data->id_pointer;

  if (*id_p != nullptr) {
    if (!check_datablock_expanded(*id_p)) {
      data->is_valid = false;
      /* TODO(sergey): Store which is not valid? */
    }
  }
  return IDWALK_RET_NOP;
}

}  // namespace

/* Actual implementation of logic which "expands" all the data which was not
 * yet copied-on-write.
 *
 * NOTE: Expects that CoW datablock is empty. */
ID *deg_expand_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       const IDNode *id_node,
                                       DepsgraphNodeBuilder *node_builder,
                                       bool create_placeholders)
{
  const ID *id_orig = id_node->id_orig;
  ID *id_cow = id_node->id_cow;
  const int id_cow_recalc = id_cow->recalc;
  /* No need to expand such datablocks, their copied ID is same as original
   * one already. */
  if (!deg_copy_on_write_is_needed(id_orig)) {
    return id_cow;
  }
  DEG_COW_PRINT(
      "Expanding datablock for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
  /* Sanity checks. */
  /* NOTE: Disabled for now, conflicts when re-using evaluated datablock when
   * rebuilding dependencies. */
  if (check_datablock_expanded(id_cow) && create_placeholders) {
    deg_free_copy_on_write_datablock(id_cow);
  }
  // BLI_assert(check_datablock_expanded(id_cow) == false);
  /* Copy data from original ID to a copied version. */
  /* TODO(sergey): Avoid doing full ID copy somehow, make Mesh to reference
   * original geometry arrays for until those are modified. */
  /* TODO(sergey): We do some trickery with temp bmain and extra ID pointer
   * just to be able to use existing API. Ideally we need to replace this with
   * in-place copy from existing datablock to a prepared memory.
   *
   * NOTE: We don't use BKE_main_{new,free} because:
   * - We don't want heap-allocations here.
   * - We don't want bmain's content to be freed when main is freed. */
  bool done = false;
  /* First we handle special cases which are not covered by BKE_id_copy() yet.
   * or cases where we want to do something smarter than simple datablock
   * copy. */
  const ID_Type id_type = GS(id_orig->name);
  switch (id_type) {
    case ID_SCE: {
      done = scene_copy_inplace_no_main((Scene *)id_orig, (Scene *)id_cow);
      if (done) {
        /* NOTE: This is important to do before remap, because this
         * function will make it so less IDs are to be remapped. */
        scene_setup_view_layers_before_remap(depsgraph, id_node, (Scene *)id_cow);
      }
      break;
    }
    case ID_ME: {
      /* TODO(sergey): Ideally we want to handle meshes in a special
       * manner here to avoid initial copy of all the geometry arrays. */
      break;
    }
    default:
      break;
  }
  if (!done) {
    done = id_copy_inplace_no_main(id_orig, id_cow);
  }
  if (!done) {
    BLI_assert(!"No idea how to perform CoW on datablock");
  }
  /* Update pointers to nested ID datablocks. */
  DEG_COW_PRINT(
      "  Remapping ID links for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);

#ifdef NESTED_ID_NASTY_WORKAROUND
  ntree_hack_remap_pointers(depsgraph, id_cow);
#endif
  /* Do it now, so remapping will understand that possibly remapped self ID
   * is not to be remapped again. */
  deg_tag_copy_on_write_id(id_cow, id_orig);
  /* Perform remapping of the nodes. */
  RemapCallbackUserData user_data = {nullptr};
  user_data.depsgraph = depsgraph;
  user_data.node_builder = node_builder;
  user_data.create_placeholders = create_placeholders;
  BKE_library_foreach_ID_link(nullptr,
                              id_cow,
                              foreach_libblock_remap_callback,
                              (void *)&user_data,
                              IDWALK_IGNORE_EMBEDDED_ID);
  /* Correct or tweak some pointers which are not taken care by foreach
   * from above. */
  update_id_after_copy(depsgraph, id_node, id_orig, id_cow);
  id_cow->recalc = id_cow_recalc;
  return id_cow;
}

/* NOTE: Depsgraph is supposed to have ID node already. */
ID *deg_expand_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       ID *id_orig,
                                       DepsgraphNodeBuilder *node_builder,
                                       bool create_placeholders)
{
  DEG::IDNode *id_node = depsgraph->find_id_node(id_orig);
  BLI_assert(id_node != nullptr);
  return deg_expand_copy_on_write_datablock(depsgraph, id_node, node_builder, create_placeholders);
}

ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph, const IDNode *id_node)
{
  const ID *id_orig = id_node->id_orig;
  ID *id_cow = id_node->id_cow;
  /* Similar to expansion, no need to do anything here. */
  if (!deg_copy_on_write_is_needed(id_orig)) {
    return id_cow;
  }
  RuntimeBackup backup(depsgraph);
  backup.init_from_id(id_cow);
  deg_free_copy_on_write_datablock(id_cow);
  deg_expand_copy_on_write_datablock(depsgraph, id_node);
  backup.restore_to_id(id_cow);
  return id_cow;
}

/* NOTE: Depsgraph is supposed to have ID node already. */
ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph, ID *id_orig)
{
  DEG::IDNode *id_node = depsgraph->find_id_node(id_orig);
  BLI_assert(id_node != nullptr);
  return deg_update_copy_on_write_datablock(depsgraph, id_node);
}

namespace {

void discard_armature_edit_mode_pointers(ID *id_cow)
{
  bArmature *armature_cow = (bArmature *)id_cow;
  armature_cow->edbo = nullptr;
}

void discard_curve_edit_mode_pointers(ID *id_cow)
{
  Curve *curve_cow = (Curve *)id_cow;
  curve_cow->editnurb = nullptr;
  curve_cow->editfont = nullptr;
}

void discard_mball_edit_mode_pointers(ID *id_cow)
{
  MetaBall *mball_cow = (MetaBall *)id_cow;
  mball_cow->editelems = nullptr;
}

void discard_lattice_edit_mode_pointers(ID *id_cow)
{
  Lattice *lt_cow = (Lattice *)id_cow;
  lt_cow->editlatt = nullptr;
}

void discard_mesh_edit_mode_pointers(ID *id_cow)
{
  Mesh *mesh_cow = (Mesh *)id_cow;
  if (mesh_cow->edit_mesh == nullptr) {
    return;
  }
  BKE_editmesh_free_derivedmesh(mesh_cow->edit_mesh);
  MEM_freeN(mesh_cow->edit_mesh);
  mesh_cow->edit_mesh = nullptr;
}

void discard_scene_pointers(ID *id_cow)
{
  Scene *scene_cow = (Scene *)id_cow;
  scene_cow->toolsettings = nullptr;
  scene_cow->eevee.light_cache_data = nullptr;
}

/* nullptr-ify all edit mode pointers which points to data from
 * original object. */
void discard_edit_mode_pointers(ID *id_cow)
{
  const ID_Type type = GS(id_cow->name);
  switch (type) {
    case ID_AR:
      discard_armature_edit_mode_pointers(id_cow);
      break;
    case ID_ME:
      discard_mesh_edit_mode_pointers(id_cow);
      break;
    case ID_CU:
      discard_curve_edit_mode_pointers(id_cow);
      break;
    case ID_MB:
      discard_mball_edit_mode_pointers(id_cow);
      break;
    case ID_LT:
      discard_lattice_edit_mode_pointers(id_cow);
      break;
    case ID_SCE:
      /* Not really edit mode but still needs to run before
       * BKE_libblock_free_datablock() */
      discard_scene_pointers(id_cow);
      break;
    default:
      break;
  }
}

}  // namespace

/* Free content of the CoW data-block
 * Notes:
 * - Does not recurs into nested ID data-blocks.
 * - Does not free data-block itself. */
void deg_free_copy_on_write_datablock(ID *id_cow)
{
  if (!check_datablock_expanded(id_cow)) {
    /* Actual content was never copied on top of CoW block, we have
     * nothing to free. */
    return;
  }
  const ID_Type type = GS(id_cow->name);
#ifdef NESTED_ID_NASTY_WORKAROUND
  nested_id_hack_discard_pointers(id_cow);
#endif
  switch (type) {
    case ID_OB: {
      /* TODO(sergey): This workaround is only to prevent free derived
       * caches from modifying object->data. This is currently happening
       * due to mesh/curve datablock boundbox tagging dirty. */
      Object *ob_cow = (Object *)id_cow;
      ob_cow->data = nullptr;
      ob_cow->sculpt = nullptr;
      break;
    }
    default:
      break;
  }
  discard_edit_mode_pointers(id_cow);
  BKE_libblock_free_datablock(id_cow, 0);
  BKE_libblock_free_data(id_cow, false);
  /* Signal datablock as not being expanded. */
  id_cow->name[0] = '\0';
}

void deg_evaluate_copy_on_write(struct ::Depsgraph *graph, const IDNode *id_node)
{
  const DEG::Depsgraph *depsgraph = reinterpret_cast<const DEG::Depsgraph *>(graph);
  DEG_debug_print_eval(graph, __func__, id_node->id_orig->name, id_node->id_cow);
  if (id_node->id_orig == &depsgraph->scene->id) {
    /* NOTE: This is handled by eval_ctx setup routines, which
     * ensures scene and view layer pointers are valid. */
    return;
  }
  deg_update_copy_on_write_datablock(depsgraph, id_node);
}

bool deg_validate_copy_on_write_datablock(ID *id_cow)
{
  if (id_cow == nullptr) {
    return false;
  }
  ValidateData data;
  data.is_valid = true;
  BKE_library_foreach_ID_link(
      nullptr, id_cow, foreach_libblock_validate_callback, &data, IDWALK_NOP);
  return data.is_valid;
}

void deg_tag_copy_on_write_id(ID *id_cow, const ID *id_orig)
{
  BLI_assert(id_cow != id_orig);
  BLI_assert((id_orig->tag & LIB_TAG_COPIED_ON_WRITE) == 0);
  id_cow->tag |= LIB_TAG_COPIED_ON_WRITE;
  /* This ID is no longer localized, is a self-sustaining copy now. */
  id_cow->tag &= ~LIB_TAG_LOCALIZED;
  id_cow->orig_id = (ID *)id_orig;
}

bool deg_copy_on_write_is_expanded(const ID *id_cow)
{
  return check_datablock_expanded(id_cow);
}

bool deg_copy_on_write_is_needed(const ID *id_orig)
{
  const ID_Type id_type = GS(id_orig->name);
  return deg_copy_on_write_is_needed(id_type);
}

bool deg_copy_on_write_is_needed(const ID_Type id_type)
{
  return ID_TYPE_IS_COW(id_type);
}

}  // namespace DEG
