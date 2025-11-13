/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "BLI_utildefines.h"

#include "BKE_curve.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "DRW_engine.hh"

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

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_editmesh.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_pointcache.h"

#include "SEQ_relations.hh"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_nodes.h"
#include "intern/depsgraph.hh"
#include "intern/eval/deg_eval_runtime_backup.h"
#include "intern/node/deg_node.hh"
#include "intern/node/deg_node_id.hh"

namespace blender::deg {

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

    SPECIAL_CASE(ID_CU_LEGACY, Curve, key)
    SPECIAL_CASE(ID_LT, Lattice, key)
    SPECIAL_CASE(ID_ME, Mesh, key)

    case ID_SCE: {
      Scene *scene_cow = (Scene *)id_cow;
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
      storage->variable = dna::shallow_copy(*(dna_type *)id); \
      storage->variable.field = nullptr; \
      return &storage->variable.id; \
    }

    SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree, linestyle)
    SPECIAL_CASE(ID_LA, Light, nodetree, lamp)
    SPECIAL_CASE(ID_MA, Material, nodetree, material)
    SPECIAL_CASE(ID_TE, Tex, nodetree, tex)
    SPECIAL_CASE(ID_WO, World, nodetree, world)

    SPECIAL_CASE(ID_CU_LEGACY, Curve, key, curve)
    SPECIAL_CASE(ID_LT, Lattice, key, lattice)
    SPECIAL_CASE(ID_ME, Mesh, key, mesh)

    case ID_SCE: {
      storage->scene = *(Scene *)id;
      storage->scene.toolsettings = nullptr;
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
    SPECIAL_CASE(ID_TE, Tex, nodetree)
    SPECIAL_CASE(ID_WO, World, nodetree)

    SPECIAL_CASE(ID_CU_LEGACY, Curve, key)
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
    SPECIAL_CASE(ID_TE, Tex, nodetree, bNodeTree)
    SPECIAL_CASE(ID_WO, World, nodetree, bNodeTree)

    SPECIAL_CASE(ID_CU_LEGACY, Curve, key, Key)
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

  if (G.debug & G_DEBUG_DEPSGRAPH_UID) {
    const ID_Type id_type = GS(id_for_copy->name);
    if (id_type == ID_OB) {
      const Object *object = reinterpret_cast<const Object *>(id_for_copy);
      BKE_object_check_uids_unique_and_report(object);
    }
  }

#ifdef NESTED_ID_NASTY_WORKAROUND
  NestedIDHackTempStorage id_hack_storage;
  id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage, id);
#endif

  bool result = (BKE_id_copy_ex(nullptr,
                                (ID *)id_for_copy,
                                &newid,
                                (LIB_ID_COPY_LOCALIZE | LIB_ID_CREATE_NO_ALLOCATE |
                                 LIB_ID_COPY_SET_COPIED_ON_WRITE)) != nullptr);

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

  if (G.debug & G_DEBUG_DEPSGRAPH_UID) {
    seq::relations_check_uids_unique_and_report(scene);
  }

#ifdef NESTED_ID_NASTY_WORKAROUND
  NestedIDHackTempStorage id_hack_storage;
  const ID *id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage, &scene->id);
#else
  const ID *id_for_copy = &scene->id;
#endif
  bool result = (BKE_id_copy_ex(nullptr,
                                id_for_copy,
                                (ID **)&new_scene,
                                (LIB_ID_COPY_LOCALIZE | LIB_ID_CREATE_NO_ALLOCATE |
                                 LIB_ID_COPY_SET_COPIED_ON_WRITE)) != nullptr);

#ifdef NESTED_ID_NASTY_WORKAROUND
  if (result) {
    nested_id_hack_restore_pointers(&scene->id, &new_scene->id);
  }
#endif

  return result;
}

/* For the given scene get view layer which corresponds to an original for the
 * scene's evaluated one. This depends on how the scene is pulled into the
 * dependency graph. */
ViewLayer *get_original_view_layer(const Depsgraph *depsgraph, const IDNode *id_node)
{
  if (id_node->linked_state == DEG_ID_LINKED_DIRECTLY) {
    return depsgraph->view_layer;
  }
  if (id_node->linked_state == DEG_ID_LINKED_VIA_SET) {
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

/* Remove all bases from all view layers except the input one. */
void scene_minimize_unused_view_layers(const Depsgraph *depsgraph,
                                       const IDNode *id_node,
                                       Scene *scene_cow)
{
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
      BKE_view_layer_free_object_content(view_layer);
    }
    return;
  }

  const ViewLayer *view_layer_input = get_original_view_layer(depsgraph, id_node);
  ViewLayer *view_layer_eval = nullptr;
  /* Find evaluated view layer. At the same time we free memory used by
   * all other of the view layers. */
  for (ViewLayer *view_layer_cow = reinterpret_cast<ViewLayer *>(scene_cow->view_layers.first),
                 *view_layer_next;
       view_layer_cow != nullptr;
       view_layer_cow = view_layer_next)
  {
    view_layer_next = view_layer_cow->next;
    if (view_layer_input != nullptr && STREQ(view_layer_input->name, view_layer_cow->name)) {
      view_layer_eval = view_layer_cow;
    }
    else {
      BKE_view_layer_free_object_content(view_layer_cow);
    }
  }

  /* Make evaluated view layer the first one in the evaluated scene (if it exists). This is for
   * legacy sake, as this used to remove all other view layers, automatically making the evaluated
   * one the first. Some other code may still assume it is. */
  if (view_layer_eval != nullptr) {
    BLI_listbase_swaplinks(&scene_cow->view_layers, scene_cow->view_layers.first, view_layer_eval);
  }
}

void scene_remove_all_bases(Scene *scene_cow)
{
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene_cow->view_layers) {
    BLI_freelistN(&view_layer->object_bases);
  }
}

/* Makes it so given view layer only has bases corresponding to enabled
 * objects. */
void view_layer_remove_disabled_bases(const Depsgraph *depsgraph,
                                      const Scene *scene,
                                      ViewLayer *view_layer)
{
  if (view_layer == nullptr) {
    return;
  }
  ListBase enabled_bases = {nullptr, nullptr};
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH_MUTABLE (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    /* TODO(sergey): Would be cool to optimize this somehow, or make it so
     * builder tags bases.
     *
     * NOTE: The idea of using id's tag and check whether its copied ot not
     * is not reliable, since object might be indirectly linked into the
     * graph.
     *
     * NOTE: We are using original base since the object which evaluated base
     * points to is not yet copied. This is dangerous access from evaluated
     * domain to original one, but this is how the entire copy-on-evaluation works:
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
  scene_minimize_unused_view_layers(depsgraph, id_node, scene_cow);
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
  view_layer_remove_disabled_bases(depsgraph, scene_cow, view_layer_eval);
  /* TODO(sergey): Remove objects from collections as well.
   * Not a HUGE deal for now, nobody is looking into those CURRENTLY.
   * Still not an excuse to have those. */
}

/* Check whether given ID is expanded or still a shallow copy. */
inline bool check_datablock_expanded(const ID *id_cow)
{
  return (id_cow->name[0] != '\0');
}

/* Callback for BKE_library_foreach_ID_link which remaps original ID pointer
 * with the one created by copy-on-evaluation system. */

struct RemapCallbackUserData {
  /* Dependency graph for which remapping is happening. */
  const Depsgraph *depsgraph;
};

int foreach_libblock_remap_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;
  if (*id_p == nullptr) {
    return IDWALK_RET_NOP;
  }

  RemapCallbackUserData *user_data = (RemapCallbackUserData *)cb_data->user_data;
  const Depsgraph *depsgraph = user_data->depsgraph;
  ID *id_orig = *id_p;
  if (deg_eval_copy_is_needed(id_orig)) {
    ID *id_cow = depsgraph->get_cow_id(id_orig);
    BLI_assert(id_cow != nullptr);
    DEG_COW_PRINT(
        "    Remapping data-block for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
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
  const Mesh *mesh_orig = (const Mesh *)id_orig;
  Mesh *mesh_cow = (Mesh *)id_cow;
  if (mesh_orig->runtime->edit_mesh == nullptr) {
    return;
  }
  mesh_cow->runtime->edit_mesh = mesh_orig->runtime->edit_mesh;
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
    case ID_CU_LEGACY:
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

  /* Both lists should have the same number of elements, so the check on
   * `element_cow` is just to prevent a crash if this is not the case. */
  while (element_orig != nullptr && element_cow != nullptr) {
    element_cow->*orig_field = element_orig;
    element_cow = element_cow->next;
    element_orig = element_orig->next;
  }

  BLI_assert_msg(element_orig == nullptr && element_cow == nullptr,
                 "list of pointers of different sizes, unable to reliably set orig pointer");
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
  /* Inactive (and render) dependency graphs are living in their own little bubble, should not care
   * about edit mode at all. */
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

/* Do some special treatment of data transfer from original ID to its
 * evaluated complementary part.
 *
 * Only use for the newly created evaluated data-blocks. */
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
      object_cow->runtime->data_orig = (ID *)object_cow->data;
      if (object_cow->type == OB_ARMATURE) {
        const bArmature *armature_orig = (bArmature *)object_orig->data;
        bArmature *armature_cow = (bArmature *)object_cow->data;
        BKE_pose_remap_bone_pointers(armature_cow, object_cow->pose);
        if (armature_orig->edbo == nullptr) {
          update_pose_orig_pointers(object_orig->pose, object_cow->pose);
        }
        BKE_pose_pchan_index_rebuild(object_cow->pose);
      }
      update_particles_after_copy(depsgraph, object_orig, object_cow);
      break;
    }
    case ID_SCE: {
      Scene *scene_cow = (Scene *)id_cow;
      const Scene *scene_orig = (const Scene *)id_orig;
      scene_cow->toolsettings = scene_orig->toolsettings;
      scene_setup_view_layers_after_remap(depsgraph, id_node, reinterpret_cast<Scene *>(id_cow));
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

/* Actual implementation of logic which "expands" all the data which was not
 * yet copied-on-eval.
 *
 * NOTE: Expects that evaluated datablock is empty. */
ID *deg_expand_eval_copy_datablock(const Depsgraph *depsgraph, const IDNode *id_node)
{
  const ID *id_orig = id_node->id_orig;
  ID *id_cow = id_node->id_cow;
  const int id_cow_recalc = id_cow->recalc;

  /* No need to expand such datablocks, their copied ID is same as original
   * one already. */
  if (!deg_eval_copy_is_needed(id_orig)) {
    return id_cow;
  }

  DEG_COW_PRINT(
      "Expanding datablock for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);

  /* Sanity checks.
   *
   * At this point, `id_cow` is essentially considered as a (partially dirty) allocated buffer (it
   * has been freed, but not fully cleared, as a result of calling #deg_free_eval_copy_datablock on
   * it). It is not expected to have any valid sub-data, not even a valid `ID::runtime` pointer.
   */
  BLI_assert(check_datablock_expanded(id_cow) == false);
  BLI_assert(id_cow->py_instance == nullptr);
  BLI_assert(id_cow->runtime == nullptr);

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
    BLI_assert_msg(0, "No idea how to perform evaluated copy on datablock");
  }
  /* Update pointers to nested ID datablocks. */
  DEG_COW_PRINT(
      "  Remapping ID links for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);

#ifdef NESTED_ID_NASTY_WORKAROUND
  ntree_hack_remap_pointers(depsgraph, id_cow);
#endif
  /* Do it now, so remapping will understand that possibly remapped self ID
   * is not to be remapped again. */
  deg_tag_eval_copy_id(const_cast<Depsgraph &>(*depsgraph), id_cow, id_orig);
  /* Perform remapping of the nodes. */
  RemapCallbackUserData user_data = {nullptr};
  user_data.depsgraph = depsgraph;
  /* About IDWALK flags:
   *  - #IDWALK_IGNORE_EMBEDDED_ID: In depsgraph embedded IDs are handled (mostly) as regular IDs,
   *    and processed on their own, not as part of their owner ID (the owner ID's pointer to its
   *    embedded data is set to null before actual copying, in #id_copy_inplace_no_main).
   *  - #IDWALK_IGNORE_MISSING_OWNER_ID is necessary for the same reason: when directly processing
   *    an embedded ID here, its owner is unknown, and its internal owner ID pointer is not yet
   *    remapped, so it is currently 'invalid'. */
  BKE_library_foreach_ID_link(nullptr,
                              id_cow,
                              foreach_libblock_remap_callback,
                              (void *)&user_data,
                              IDWALK_IGNORE_EMBEDDED_ID | IDWALK_IGNORE_MISSING_OWNER_ID);
  /* Correct or tweak some pointers which are not taken care by foreach
   * from above. */
  update_id_after_copy(depsgraph, id_node, id_orig, id_cow);
  id_cow->recalc = id_cow_recalc;
  return id_cow;
}

}  // namespace

ID *deg_update_eval_copy_datablock(const Depsgraph *depsgraph, const IDNode *id_node)
{
  const ID *id_orig = id_node->id_orig;
  ID *id_cow = id_node->id_cow;
  /* Similar to expansion, no need to do anything here. */
  if (!deg_eval_copy_is_needed(id_orig)) {
    return id_cow;
  }

  /* When updating object data in edit-mode, don't request copy-on-eval update since this will
   * duplicate all object data which is unnecessary when the edit-mode data is used for calculating
   * modifiers.
   *
   * TODO: Investigate modes besides edit-mode. */
  if (check_datablock_expanded(id_cow) && !id_node->is_cow_explicitly_tagged) {
    const ID_Type id_type = GS(id_orig->name);
    /* Pass nullptr as the object is only needed for Curves which do not have edit mode pointers.
     */
    if (OB_DATA_SUPPORT_EDITMODE(id_type) && BKE_object_data_is_in_editmode(nullptr, id_orig)) {
      /* Make sure pointers in the edit mode data are updated in the copy.
       * This allows depsgraph to pick up changes made in another context after it has been
       * evaluated. Consider the following scenario:
       *
       *  - ObjectA in SceneA is using Mesh.
       *  - ObjectB in SceneB is using Mesh (same exact datablock).
       *  - Depsgraph of SceneA is evaluated.
       *  - Depsgraph of SceneB is evaluated.
       *  - User enters edit mode of ObjectA in SceneA. */
      update_edit_mode_pointers(depsgraph, id_orig, id_cow);
      return id_cow;
    }
  }

  RuntimeBackup backup(depsgraph);
  backup.init_from_id(id_cow);
  deg_free_eval_copy_datablock(id_cow);
  deg_expand_eval_copy_datablock(depsgraph, id_node);
  backup.restore_to_id(id_cow);
  return id_cow;
}

ID *deg_update_eval_copy_datablock(const Depsgraph *depsgraph, ID *id_orig)
{
  /* NOTE: Depsgraph is supposed to have ID node already. */

  IDNode *id_node = depsgraph->find_id_node(id_orig);
  BLI_assert(id_node != nullptr);
  return deg_update_eval_copy_datablock(depsgraph, id_node);
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
  mesh_cow->runtime->edit_mesh = nullptr;
}

void discard_scene_pointers(ID *id_cow)
{
  Scene *scene_cow = (Scene *)id_cow;
  scene_cow->toolsettings = nullptr;
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
    case ID_CU_LEGACY:
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

void deg_free_eval_copy_datablock(ID *id_cow)
{
  /* Free content of the evaluated data-block.
   * Notes:
   * - Does not recurse into nested ID data-blocks.
   * - Does not free data-block itself.
   */

  if (!check_datablock_expanded(id_cow)) {
    /* Actual content was never copied on top of evaluated data-block, we have
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
       * due to mesh/curve data-block bound-box tagging dirty. */
      Object *ob_cow = (Object *)id_cow;
      ob_cow->data = nullptr;
      ob_cow->sculpt = nullptr;
      break;
    }
    default:
      break;
  }
  discard_edit_mode_pointers(id_cow);
  BKE_libblock_free_data_py(id_cow);
  BKE_libblock_free_datablock(id_cow, 0);
  BKE_libblock_free_data(id_cow, false);
  /* Signal datablock as not being expanded. */
  id_cow->name[0] = '\0';
}

void deg_create_eval_copy(::Depsgraph *graph, const IDNode *id_node)
{
  const Depsgraph *depsgraph = reinterpret_cast<const Depsgraph *>(graph);
  DEG_debug_print_eval(graph, __func__, id_node->id_orig->name, id_node->id_cow);
  if (id_node->id_orig == &depsgraph->scene->id) {
    /* NOTE: This is handled by eval_ctx setup routines, which
     * ensures scene and view layer pointers are valid. */
    return;
  }
  deg_update_eval_copy_datablock(depsgraph, id_node);
}

bool deg_validate_eval_copy_datablock(ID *id_cow)
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

void deg_tag_eval_copy_id(deg::Depsgraph &depsgraph, ID *id_cow, const ID *id_orig)
{
  BLI_assert(id_cow != id_orig);
  BLI_assert((id_orig->tag & ID_TAG_COPIED_ON_EVAL) == 0);
  id_cow->tag |= ID_TAG_COPIED_ON_EVAL;
  /* This ID is no longer localized, is a self-sustaining copy now. */
  id_cow->tag &= ~ID_TAG_LOCALIZED;
  id_cow->orig_id = (ID *)id_orig;
  id_cow->runtime->depsgraph = &reinterpret_cast<::Depsgraph &>(depsgraph);
}

bool deg_eval_copy_is_expanded(const ID *id_cow)
{
  return check_datablock_expanded(id_cow);
}

bool deg_eval_copy_is_needed(const ID *id_orig)
{
  const ID_Type id_type = GS(id_orig->name);
  return deg_eval_copy_is_needed(id_type);
}

bool deg_eval_copy_is_needed(const ID_Type id_type)
{
  return ID_TYPE_USE_COPY_ON_EVAL(id_type);
}

}  // namespace blender::deg
