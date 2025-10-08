/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstdio>
#include <cstdlib>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_array_utils.hh"
#include "DNA_curve_types.h"
#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_force_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bitmap.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_anonymous_attribute_id.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_curves.h"
#include "BKE_curves.hh"
#include "BKE_displist.h"
#include "BKE_editmesh.hh"
#include "BKE_effect.h"
#include "BKE_geometry_set.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_key.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_material.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_object_types.hh"
#include "BKE_ocean.h"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_softbody.h"
#include "BKE_volume.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_armature.hh"
#include "ED_grease_pencil.hh"
#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_object_vgroup.hh"
#include "ED_screen.hh"

#include "ANIM_bone_collections.hh"

#include "GEO_merge_layers.hh"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "object_intern.hh"

namespace blender::ed::object {

static CLG_LogRef LOG = {"object"};

static void modifier_skin_customdata_delete(Object *ob);

/* ------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void object_force_modifier_update_for_bind(Depsgraph *depsgraph, Object *ob)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  BKE_object_eval_reset(ob_eval);
  if (ob->type == OB_MESH) {
    Mesh *mesh_eval = blender::bke::mesh_create_eval_final(
        depsgraph, scene_eval, ob_eval, &CD_MASK_DERIVEDMESH);
    BKE_id_free(nullptr, mesh_eval);
  }
  else if (ob->type == OB_LATTICE) {
    BKE_lattice_modifiers_calc(depsgraph, scene_eval, ob_eval);
  }
  else if (ob->type == OB_MBALL) {
    BKE_mball_data_update(depsgraph, scene_eval, ob_eval);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
    BKE_displist_make_curveTypes(depsgraph, scene_eval, ob_eval, false);
  }
  else if (ob->type == OB_CURVES) {
    BKE_curves_data_update(depsgraph, scene_eval, ob);
  }
  else if (ob->type == OB_POINTCLOUD) {
    BKE_pointcloud_data_update(depsgraph, scene_eval, ob);
  }
  else if (ob->type == OB_VOLUME) {
    BKE_volume_data_update(depsgraph, scene_eval, ob);
  }
}

static void object_force_modifier_bind_simple_options(Depsgraph *depsgraph,
                                                      Object *object,
                                                      ModifierData *md)
{
  ModifierData *md_eval = BKE_modifier_get_evaluated(depsgraph, object, md);
  const int mode = md_eval->mode;
  md_eval->mode |= eModifierMode_Realtime;
  object_force_modifier_update_for_bind(depsgraph, object);
  md_eval->mode = mode;
}

ModifierData *modifier_add(
    ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type)
{
  ModifierData *new_md = nullptr;
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)type);

  /* Check compatibility of modifier [#25291, #50373]. */
  if (!BKE_object_support_modifier_type_check(ob, type)) {
    BKE_reportf(reports, RPT_WARNING, "Modifiers cannot be added to object '%s'", ob->id.name + 2);
    return nullptr;
  }

  if (mti->flags & eModifierTypeFlag_Single) {
    if (BKE_modifiers_findby_type(ob, (ModifierType)type)) {
      BKE_report(reports, RPT_WARNING, "Only one modifier of this type is allowed");
      return nullptr;
    }
  }

  if (type == eModifierType_ParticleSystem) {
    /* don't need to worry about the new modifier's name, since that is set to the number
     * of particle systems which shouldn't have too many duplicates
     */
    new_md = object_add_particle_system(bmain, scene, ob, name);
  }
  else {
    /* get new modifier data to add */
    new_md = BKE_modifier_new(type);

    ModifierData *next_md = nullptr;
    LISTBASE_FOREACH_BACKWARD (ModifierData *, md, &ob->modifiers) {
      if (md->flag & eModifierFlag_PinLast) {
        next_md = md;
      }
      else {
        break;
      }
    }
    if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
      next_md = static_cast<ModifierData *>(ob->modifiers.first);

      while (next_md && BKE_modifier_get_info((ModifierType)next_md->type)->type ==
                            ModifierTypeType::OnlyDeform)
      {
        if (next_md->next && (next_md->next->flag & eModifierFlag_PinLast) != 0) {
          break;
        }
        next_md = next_md->next;
      }
    }
    BLI_insertlinkbefore(&ob->modifiers, next_md, new_md);
    BKE_modifiers_persistent_uid_init(*ob, *new_md);

    if (name) {
      STRNCPY_UTF8(new_md->name, name);
    }

    /* make sure modifier data has unique name */

    BKE_modifier_unique_name(&ob->modifiers, new_md);

    /* special cases */
    if (type == eModifierType_Softbody) {
      if (!ob->soft) {
        ob->soft = sbNew();
        ob->softflag |= OB_SB_GOAL | OB_SB_EDGES;
      }
    }
    else if (type == eModifierType_Collision) {
      if (!ob->pd) {
        ob->pd = BKE_partdeflect_new(0);
      }

      ob->pd->deflect = 1;
    }
    else if (type == eModifierType_Surface) {
      /* pass */
    }
    else if (type == eModifierType_Multires) {
      /* set totlvl from existing MDISPS layer if object already had it */
      multiresModifier_set_levels_from_disps((MultiresModifierData *)new_md, ob);

      if (ob->mode & OB_MODE_SCULPT) {
        /* ensure that grid paint mask layer is created */
        BKE_sculpt_mask_layers_ensure(nullptr, nullptr, ob, (MultiresModifierData *)new_md);
      }
    }
    else if (type == eModifierType_Skin) {
      /* ensure skin-node customdata exists */
      BKE_mesh_ensure_skin_customdata(static_cast<Mesh *>(ob->data));
    }
  }

  BKE_object_modifier_set_active(ob, new_md);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return new_md;
}

/* Return true if the object has a modifier of type 'type' other than
 * the modifier pointed to be 'exclude', otherwise returns false. */
static bool object_has_modifier(const Object *ob, const ModifierData *exclude, ModifierType type)
{
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if ((md != exclude) && (md->type == type)) {
      return true;
    }
  }

  return false;
}

bool iter_other(Main *bmain,
                Object *orig_ob,
                const bool include_orig,
                bool (*callback)(Object *ob, void *callback_data),
                void *callback_data)
{
  ID *ob_data_id = static_cast<ID *>(orig_ob->data);
  int users = ob_data_id->us;

  if (ob_data_id->flag & ID_FLAG_FAKEUSER) {
    users--;
  }

  /* First check that the object's data has multiple users */
  if (users > 1) {
    Object *ob;
    int totfound = include_orig ? 0 : 1;

    for (ob = static_cast<Object *>(bmain->objects.first); ob && totfound < users;
         ob = reinterpret_cast<Object *>(ob->id.next))
    {
      if (((ob != orig_ob) || include_orig) && (ob->data == orig_ob->data)) {
        if (callback(ob, callback_data)) {
          return true;
        }

        totfound++;
      }
    }
  }
  else if (include_orig) {
    return callback(orig_ob, callback_data);
  }

  return false;
}

static bool object_has_modifier_cb(Object *ob, void *data)
{
  ModifierType type = *((ModifierType *)data);

  return object_has_modifier(ob, nullptr, type);
}

bool multires_update_totlevels(Object *ob, void *totlevel_v)
{
  int totlevel = *((char *)totlevel_v);

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Multires) {
      multires_set_tot_level(ob, (MultiresModifierData *)md, totlevel);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  return false;
}

/* Return true if no modifier of type 'type' other than 'exclude' */
static bool object_modifier_safe_to_delete(Main *bmain,
                                           Object *ob,
                                           ModifierData *exclude,
                                           ModifierType type)
{
  return (!object_has_modifier(ob, exclude, type) &&
          !iter_other(bmain, ob, false, object_has_modifier_cb, &type));
}

static bool object_modifier_remove(
    Main *bmain, Scene *scene, Object *ob, ModifierData *md, bool *r_sort_depsgraph)
{
  /* It seems on rapid delete it is possible to
   * get called twice on same modifier, so make
   * sure it is in list. */
  if (BLI_findindex(&ob->modifiers, md) == -1) {
    return false;
  }

  /* special cases */
  if (md->type == eModifierType_ParticleSystem) {
    object_remove_particle_system(bmain, scene, ob, ((ParticleSystemModifierData *)md)->psys);
    return true;
  }

  if (md->type == eModifierType_Softbody) {
    if (ob->soft) {
      sbFree(ob);
      ob->softflag = 0; /* TODO(Sybren): this should probably be moved into sbFree() */
    }
  }
  else if (md->type == eModifierType_Collision) {
    if (ob->pd) {
      ob->pd->deflect = 0;
    }

    *r_sort_depsgraph = true;
  }
  else if (md->type == eModifierType_Surface) {
    *r_sort_depsgraph = true;
  }
  else if (md->type == eModifierType_Multires) {
    /* Delete MDisps layer if not used by another multires modifier */
    if (object_modifier_safe_to_delete(bmain, ob, md, eModifierType_Multires)) {
      multires_customdata_delete(static_cast<Mesh *>(ob->data));
    }
  }
  else if (md->type == eModifierType_Skin) {
    /* Delete MVertSkin layer if not used by another skin modifier */
    if (object_modifier_safe_to_delete(bmain, ob, md, eModifierType_Skin)) {
      modifier_skin_customdata_delete(ob);
    }
  }

  if (ELEM(md->type, eModifierType_Softbody, eModifierType_Cloth) &&
      BLI_listbase_is_empty(&ob->particlesystem))
  {
    ob->mode &= ~OB_MODE_PARTICLE_EDIT;
  }

  BKE_animdata_drivers_remove_for_rna_struct(ob->id, RNA_Modifier, md);

  BKE_modifier_remove_from_list(ob, md);
  BKE_modifier_free(md);
  BKE_object_free_derived_caches(ob);

  return true;
}

bool modifier_remove(ReportList *reports, Main *bmain, Scene *scene, Object *ob, ModifierData *md)
{
  bool sort_depsgraph = false;

  bool ok = object_modifier_remove(bmain, scene, ob, md, &sort_depsgraph);

  if (!ok) {
    BKE_reportf(reports, RPT_ERROR, "Modifier '%s' not in object '%s'", md->name, ob->id.name);
    return false;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);

  return true;
}

void modifiers_clear(Main *bmain, Scene *scene, Object *ob)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);
  bool sort_depsgraph = false;

  if (!md) {
    return;
  }

  while (md) {
    ModifierData *next_md = md->next;

    object_modifier_remove(bmain, scene, ob, md, &sort_depsgraph);

    md = next_md;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
}

static bool object_modifier_check_move_before(ReportList *reports,
                                              eReportType error_type,
                                              ModifierData *md,
                                              ModifierData *md_prev)
{
  if (md_prev) {
    if (md->flag & eModifierFlag_PinLast && !(md_prev->flag & eModifierFlag_PinLast)) {
      return false;
    }
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (mti->type != ModifierTypeType::OnlyDeform) {
      const ModifierTypeInfo *nmti = BKE_modifier_get_info((ModifierType)md_prev->type);

      if (nmti->flags & eModifierTypeFlag_RequiresOriginalData) {
        BKE_report(reports, error_type, "Cannot move above a modifier requiring original data");
        return false;
      }
    }
  }
  else {
    BKE_report(reports, error_type, "Cannot move modifier beyond the start of the list");
    return false;
  }

  return true;
}

bool modifier_move_up(ReportList *reports, eReportType error_type, Object *ob, ModifierData *md)
{
  if (object_modifier_check_move_before(reports, error_type, md, md->prev)) {
    BLI_listbase_swaplinks(&ob->modifiers, md, md->prev);
    return true;
  }

  return false;
}

static bool object_modifier_check_move_after(ReportList *reports,
                                             eReportType error_type,
                                             ModifierData *md,
                                             ModifierData *md_next)
{
  if (md_next) {
    if (md_next->flag & eModifierFlag_PinLast && !(md->flag & eModifierFlag_PinLast)) {
      return false;
    }
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
      const ModifierTypeInfo *nmti = BKE_modifier_get_info((ModifierType)md_next->type);

      if (nmti->type != ModifierTypeType::OnlyDeform) {
        BKE_report(reports, error_type, "Cannot move beyond a non-deforming modifier");
        return false;
      }
    }
  }
  else {
    BKE_report(reports, error_type, "Cannot move modifier beyond the end of the list");
    return false;
  }

  return true;
}

bool modifier_move_down(ReportList *reports, eReportType error_type, Object *ob, ModifierData *md)
{
  if (object_modifier_check_move_after(reports, error_type, md, md->next)) {
    BLI_listbase_swaplinks(&ob->modifiers, md, md->next);
    return true;
  }

  return false;
}

bool modifier_move_to_index(ReportList *reports,
                            eReportType error_type,
                            Object *ob,
                            ModifierData *md,
                            const int index,
                            bool allow_partial)
{
  BLI_assert(md != nullptr);

  if (index < 0 || index >= BLI_listbase_count(&ob->modifiers)) {
    BKE_report(reports, error_type, "Cannot move modifier beyond the end of the stack");
    return false;
  }

  int md_index = BLI_findindex(&ob->modifiers, md);
  BLI_assert(md_index != -1);

  if (md_index < index) {
    /* Move modifier down in list. */
    ModifierData *md_target = md;

    for (; md_index < index; md_index++, md_target = md_target->next) {
      if (!object_modifier_check_move_after(reports, error_type, md, md_target->next)) {
        if (!allow_partial || md == md_target) {
          return false;
        }

        break;
      }
    }

    BLI_assert(md != md_target && md_target);

    BLI_remlink(&ob->modifiers, md);
    BLI_insertlinkafter(&ob->modifiers, md_target, md);
  }
  else if (md_index > index) {
    /* Move modifier up in list. */
    ModifierData *md_target = md;

    for (; md_index > index; md_index--, md_target = md_target->prev) {
      if (!object_modifier_check_move_before(reports, error_type, md, md_target->prev)) {
        if (!allow_partial || md == md_target) {
          return false;
        }

        break;
      }
    }

    BLI_assert(md != md_target && md_target);

    BLI_remlink(&ob->modifiers, md);
    BLI_insertlinkbefore(&ob->modifiers, md_target, md);
  }
  else {
    return true;
  }

  /* NOTE: Dependency graph only uses modifier nodes for visibility updates, and exact order of
   * modifier nodes in the graph does not matter. */

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);

  return true;
}

void modifier_link(bContext *C, Object *ob_dst, Object *ob_src)
{
  BKE_object_link_modifiers(ob_dst, ob_src);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob_dst);
  DEG_id_tag_update(&ob_dst->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
}

bool modifier_copy_to_object(Main *bmain,
                             const Scene *scene,
                             const Object *ob_src,
                             const ModifierData *md,
                             Object *ob_dst,
                             ReportList *reports)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

  BLI_assert(ob_src != ob_dst);

  /* Checked in #BKE_object_copy_modifier, but check here too so we can give a better message. */
  if (!BKE_object_support_modifier_type_check(ob_dst, md->type)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Object '%s' does not support %s modifiers",
                ob_dst->id.name + 2,
                RPT_(mti->name));
    return false;
  }

  if (mti->flags & eModifierTypeFlag_Single) {
    if (BKE_modifiers_findby_type(ob_dst, (ModifierType)md->type)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Modifier can only be added once to object '%s'",
                  ob_dst->id.name + 2);
      return false;
    }
  }

  if (!BKE_object_copy_modifier(bmain, scene, ob_dst, ob_src, md)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Copying modifier '%s' to object '%s' failed",
                md->name,
                ob_dst->id.name + 2);
    return false;
  }

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_ADDED, ob_dst);
  DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  DEG_relations_tag_update(bmain);
  return true;
}

bool convert_psys_to_mesh(ReportList * /*reports*/,
                          Main *bmain,
                          Depsgraph *depsgraph,
                          Scene *scene,
                          ViewLayer *view_layer,
                          Object *ob,
                          ModifierData *md)
{
  int cvert = 0;

  if (md->type != eModifierType_ParticleSystem) {
    return false;
  }
  if (ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
    return false;
  }

  ParticleSystem *psys_orig = ((ParticleSystemModifierData *)md)->psys;
  ParticleSettings *part = psys_orig->part;

  if (part->ren_as != PART_DRAW_PATH) {
    return false;
  }
  ParticleSystem *psys_eval = psys_eval_get(depsgraph, ob, psys_orig);
  if (psys_eval->pathcache == nullptr) {
    return false;
  }

  int part_num = psys_eval->totcached;
  int child_num = psys_eval->totchildcache;

  if (child_num && (part->draw & PART_DRAW_PARENT) == 0) {
    part_num = 0;
  }

  /* count */
  int verts_num = 0, edges_num = 0;
  ParticleCacheKey **cache = psys_eval->pathcache;
  for (int a = 0; a < part_num; a++) {
    ParticleCacheKey *key = cache[a];

    if (key->segments > 0) {
      verts_num += key->segments + 1;
      edges_num += key->segments;
    }
  }

  cache = psys_eval->childcache;
  for (int a = 0; a < child_num; a++) {
    ParticleCacheKey *key = cache[a];

    if (key->segments > 0) {
      verts_num += key->segments + 1;
      edges_num += key->segments;
    }
  }

  if (verts_num == 0) {
    return false;
  }

  Mesh *mesh = BKE_mesh_new_nomain(verts_num, edges_num, 0, 0);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int2> edges = mesh->edges_for_write();

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);

  int edge_index = 0;

  /* copy coordinates */
  int vert_index = 0;
  cache = psys_eval->pathcache;
  for (int a = 0; a < part_num; a++) {
    ParticleCacheKey *key = cache[a];
    int kmax = key->segments;
    for (int k = 0; k <= kmax; k++, key++, cvert++, vert_index++) {
      positions[vert_index] = key->co;
      if (k) {
        edges[edge_index] = int2(cvert - 1, cvert);
        edge_index++;
      }
      else {
        /* cheap trick to select the roots */
        select_vert.span[vert_index] = true;
      }
    }
  }

  cache = psys_eval->childcache;
  for (int a = 0; a < child_num; a++) {
    ParticleCacheKey *key = cache[a];
    int kmax = key->segments;
    for (int k = 0; k <= kmax; k++, key++, cvert++, vert_index++) {
      copy_v3_v3(positions[vert_index], key->co);
      if (k) {
        edges[edge_index] = int2(cvert - 1, cvert);
        edge_index++;
      }
      else {
        /* cheap trick to select the roots */
        select_vert.span[vert_index] = true;
      }
    }
  }

  select_vert.finish();

  Object *obn = BKE_object_add(bmain, scene, view_layer, OB_MESH, nullptr);
  BKE_mesh_nomain_to_mesh(mesh, static_cast<Mesh *>(obn->data), obn);

  DEG_relations_tag_update(bmain);

  return true;
}

static void add_shapekey_layers(Mesh &mesh_dest, const Mesh &mesh_src)
{
  if (!mesh_src.key) {
    return;
  }
  int i;
  LISTBASE_FOREACH_INDEX (const KeyBlock *, kb, &mesh_src.key->block, i) {
    void *array;
    if (mesh_src.verts_num != kb->totelem) {
      CLOG_ERROR(&LOG,
                 "vertex size mismatch (Mesh '%s':%d != KeyBlock '%s':%d)",
                 mesh_src.id.name + 2,
                 mesh_src.verts_num,
                 kb->name,
                 kb->totelem);
      array = MEM_calloc_arrayN<float[3]>(mesh_src.verts_num, __func__);
    }
    else {
      array = MEM_malloc_arrayN<float[3]>(size_t(mesh_src.verts_num), __func__);
      memcpy(array, kb->data, sizeof(float[3]) * size_t(mesh_src.verts_num));
    }

    CustomData_add_layer_with_data(
        &mesh_dest.vert_data, CD_SHAPEKEY, array, mesh_dest.verts_num, nullptr);
    const int ci = CustomData_get_layer_index_n(&mesh_dest.vert_data, CD_SHAPEKEY, i);

    mesh_dest.vert_data.layers[ci].uid = kb->uid;
  }
}

/**
 * \param use_virtual_modifiers: When enabled, calculate virtual-modifiers before applying
 * `md_eval`. This is supported because virtual-modifiers are not modifiers from a user
 * perspective, allowing shape keys to be included with the modifier being applied, see: #91923.
 */
static Mesh *create_applied_mesh_for_modifier(Depsgraph *depsgraph,
                                              Scene *scene,
                                              Object *ob_eval,
                                              ModifierData *md_eval,
                                              const bool use_virtual_modifiers,
                                              const bool build_shapekey_layers,
                                              ReportList *reports)
{
  Mesh *mesh = ob_eval->runtime->data_orig ?
                   reinterpret_cast<Mesh *>(ob_eval->runtime->data_orig) :
                   reinterpret_cast<Mesh *>(ob_eval->data);
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md_eval->type));
  const ModifierEvalContext mectx = {depsgraph, ob_eval, MOD_APPLY_TO_ORIGINAL};

  if (!(md_eval->mode & eModifierMode_Realtime)) {
    return nullptr;
  }

  if (mti->is_disabled && mti->is_disabled(scene, md_eval, false)) {
    return nullptr;
  }

  if (build_shapekey_layers && mesh->key) {
    if (KeyBlock *kb = static_cast<KeyBlock *>(
            BLI_findlink(&mesh->key->block, ob_eval->shapenr - 1)))
    {
      BKE_keyblock_convert_to_mesh(kb, mesh->vert_positions_for_write());
    }
  }

  Mesh *mesh_temp = reinterpret_cast<Mesh *>(
      BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE));
  MutableSpan<float3> deformedVerts = mesh_temp->vert_positions_for_write();

  if (use_virtual_modifiers) {
    VirtualModifierData virtual_modifier_data;
    for (ModifierData *md_eval_virt =
             BKE_modifiers_get_virtual_modifierlist(ob_eval, &virtual_modifier_data);
         md_eval_virt && (md_eval_virt != ob_eval->modifiers.first);
         md_eval_virt = md_eval_virt->next)
    {
      if (!BKE_modifier_is_enabled(scene, md_eval_virt, eModifierMode_Realtime)) {
        continue;
      }
      /* All virtual modifiers are deform modifiers. */
      const ModifierTypeInfo *mti_virt = BKE_modifier_get_info(ModifierType(md_eval_virt->type));
      BLI_assert(mti_virt->type == ModifierTypeType::OnlyDeform);
      if (mti_virt->type != ModifierTypeType::OnlyDeform) {
        continue;
      }

      mti_virt->deform_verts(md_eval_virt, &mectx, mesh_temp, deformedVerts);
    }
  }

  Mesh *result = nullptr;
  if (mti->type == ModifierTypeType::OnlyDeform) {
    result = mesh_temp;
    mti->deform_verts(md_eval, &mectx, result, deformedVerts);
    result->tag_positions_changed();

    if (build_shapekey_layers) {
      add_shapekey_layers(*result, *mesh);
    }
  }
  else {
    if (build_shapekey_layers) {
      add_shapekey_layers(*mesh_temp, *mesh);
    }

    if (mti->modify_geometry_set) {
      bke::GeometrySet geometry_set = bke::GeometrySet::from_mesh(
          mesh_temp, bke::GeometryOwnershipType::Owned);
      mti->modify_geometry_set(md_eval, &mectx, &geometry_set);
      if (!geometry_set.has_mesh()) {
        BKE_report(reports, RPT_ERROR, "Evaluated geometry from modifier does not contain a mesh");
        return nullptr;
      }
      result = geometry_set.get_component_for_write<bke::MeshComponent>().release();
    }
    else {
      result = mti->modify_mesh(md_eval, &mectx, mesh_temp);
      if (mesh_temp != result) {
        BKE_id_free(nullptr, mesh_temp);
      }
    }
  }

  return result;
}

static bool modifier_apply_shape(Main *bmain,
                                 ReportList *reports,
                                 Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob,
                                 ModifierData *md_eval)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md_eval->type);

  if (mti->is_disabled && mti->is_disabled(scene, md_eval, false)) {
    BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
    return false;
  }

  /* We could investigate using the #CD_ORIGINDEX layer
   * to support other kinds of modifiers besides deforming modifiers.
   * as this is done in many other places, see: #BKE_mesh_foreach_mapped_vert_coords_get.
   *
   * This isn't high priority in practice since most modifiers users
   * want to apply as a shape are deforming modifiers.
   *
   * If a compelling use-case comes up where we want to support other kinds of modifiers
   * we can look into supporting them. */

  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    Key *key = mesh->key;

    if (!BKE_modifier_is_same_topology(md_eval) || mti->type == ModifierTypeType::NonGeometrical) {
      BKE_report(reports, RPT_ERROR, "Only deforming modifiers can be applied to shapes");
      return false;
    }

    Mesh *mesh_applied = create_applied_mesh_for_modifier(depsgraph,
                                                          DEG_get_evaluated_scene(depsgraph),
                                                          DEG_get_evaluated(depsgraph, ob),
                                                          md_eval,
                                                          true,
                                                          false,
                                                          reports);
    if (!mesh_applied) {
      BKE_report(reports, RPT_ERROR, "Modifier is disabled or returned error, skipping apply");
      return false;
    }

    if (key == nullptr) {
      key = mesh->key = BKE_key_add(bmain, (ID *)mesh);
      key->type = KEY_RELATIVE;
      /* if that was the first key block added, then it was the basis.
       * Initialize it with the mesh, and add another for the modifier */
      KeyBlock *kb = BKE_keyblock_add(key, nullptr);
      BKE_keyblock_convert_from_mesh(mesh, key, kb);
    }

    KeyBlock *kb = BKE_keyblock_add(key, md_eval->name);
    BKE_mesh_nomain_to_meshkey(mesh_applied, mesh, kb);

    BKE_id_free(nullptr, mesh_applied);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
    return false;
  }
  return true;
}

static bool apply_grease_pencil_for_modifier(Depsgraph *depsgraph,
                                             Object *ob,
                                             GreasePencil &grease_pencil_orig,
                                             ModifierData *md_eval)
{
  using namespace bke;
  using namespace bke::greasepencil;
  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md_eval->type));
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  GreasePencil *grease_pencil_for_eval = ob_eval->runtime->data_orig ?
                                             reinterpret_cast<GreasePencil *>(
                                                 ob_eval->runtime->data_orig) :
                                             &grease_pencil_orig;
  const int eval_frame = int(DEG_get_ctime(depsgraph));
  GreasePencil *grease_pencil_temp = reinterpret_cast<GreasePencil *>(
      BKE_id_copy_ex(nullptr, &grease_pencil_for_eval->id, nullptr, LIB_ID_COPY_LOCALIZE));
  grease_pencil_temp->runtime->eval_frame = eval_frame;
  GeometrySet eval_geometry_set = GeometrySet::from_grease_pencil(grease_pencil_temp,
                                                                  GeometryOwnershipType::Owned);

  ModifierEvalContext mectx = {depsgraph, ob_eval, MOD_APPLY_TO_ORIGINAL};
  mti->modify_geometry_set(md_eval, &mectx, &eval_geometry_set);
  if (!eval_geometry_set.has_grease_pencil()) {

    return false;
  }
  GreasePencil &grease_pencil_result =
      *eval_geometry_set.get_component_for_write<GreasePencilComponent>().get_for_write();

  ed::greasepencil::apply_eval_grease_pencil_data(grease_pencil_result,
                                                  eval_frame,
                                                  grease_pencil_orig.layers().index_range(),
                                                  grease_pencil_orig);

  Main *bmain = DEG_get_bmain(depsgraph);
  /* There might be layers with empty names after evaluation. Make sure to rename them. */
  bke::greasepencil::ensure_non_empty_layer_names(*bmain, grease_pencil_result);
  BKE_object_material_from_eval_data(bmain, ob, &grease_pencil_result.id);
  return true;
}

static bool apply_grease_pencil_for_modifier_all_keyframes(Depsgraph *depsgraph,
                                                           Scene *scene,
                                                           Object *ob,
                                                           GreasePencil &grease_pencil_orig,
                                                           ModifierData *md)
{
  using namespace bke;
  using namespace bke::greasepencil;
  Main *bmain = DEG_get_bmain(depsgraph);

  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));

  WM_cursor_wait(true);

  Map<int, Vector<int>> layer_indices_to_apply_per_frame;
  {
    for (const int layer_i : grease_pencil_orig.layers().index_range()) {
      const Layer &layer = grease_pencil_orig.layer(layer_i);
      for (const auto &[key, value] : layer.frames().items()) {
        if (value.is_end()) {
          continue;
        }
        layer_indices_to_apply_per_frame.lookup_or_add(key, {}).append(layer_i);
      }
    }
  }

  Array<int> sorted_frame_times(layer_indices_to_apply_per_frame.size());
  int i = 0;
  for (const int key : layer_indices_to_apply_per_frame.keys()) {
    sorted_frame_times[i++] = key;
  }
  std::sort(sorted_frame_times.begin(), sorted_frame_times.end());

  const int prev_frame = int(DEG_get_ctime(depsgraph));
  bool changed = false;
  for (const int eval_frame : sorted_frame_times) {
    const Span<int> layer_indices = layer_indices_to_apply_per_frame.lookup(eval_frame).as_span();
    scene->r.cfra = eval_frame;
    BKE_scene_graph_update_for_newframe(depsgraph);

    Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
    GreasePencil *grease_pencil_for_eval = ob_eval->runtime->data_orig ?
                                               reinterpret_cast<GreasePencil *>(
                                                   ob_eval->runtime->data_orig) :
                                               &grease_pencil_orig;

    GreasePencil *grease_pencil_temp = reinterpret_cast<GreasePencil *>(
        BKE_id_copy_ex(nullptr, &grease_pencil_for_eval->id, nullptr, LIB_ID_COPY_LOCALIZE));
    grease_pencil_temp->runtime->eval_frame = eval_frame;
    GeometrySet eval_geometry_set = GeometrySet::from_grease_pencil(grease_pencil_temp,
                                                                    GeometryOwnershipType::Owned);

    ModifierData *md_eval = BKE_modifier_get_evaluated(depsgraph, ob, md);
    ModifierEvalContext mectx = {depsgraph, ob_eval, MOD_APPLY_TO_ORIGINAL};
    mti->modify_geometry_set(md_eval, &mectx, &eval_geometry_set);
    if (!eval_geometry_set.has_grease_pencil()) {
      continue;
    }
    GreasePencil &grease_pencil_result =
        *eval_geometry_set.get_component_for_write<GreasePencilComponent>().get_for_write();

    IndexMaskMemory memory;
    const IndexMask orig_layers_to_apply = IndexMask::from_indices(layer_indices, memory);
    ed::greasepencil::apply_eval_grease_pencil_data(
        grease_pencil_result, eval_frame, orig_layers_to_apply, grease_pencil_orig);

    BKE_object_material_from_eval_data(bmain, ob, &grease_pencil_result.id);
    changed = true;
  }

  scene->r.cfra = prev_frame;
  BKE_scene_graph_update_for_newframe(depsgraph);

  /* There might be layers with empty names after evaluation. Make sure to rename them. */
  bke::greasepencil::ensure_non_empty_layer_names(*bmain, grease_pencil_orig);

  WM_cursor_wait(false);
  return changed;
}

static bool modifier_apply_obdata(ReportList *reports,
                                  Depsgraph *depsgraph,
                                  Scene *scene,
                                  Object *ob,
                                  ModifierData *md_eval,
                                  const bool do_all_keyframes)
{
  const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md_eval->type);

  if (mti->is_disabled && mti->is_disabled(scene, md_eval, false)) {
    BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
    return false;
  }

  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    MultiresModifierData *mmd = find_multires_modifier_before(scene, md_eval);

    if (mesh->key && mti->type != ModifierTypeType::NonGeometrical) {
      BKE_report(reports, RPT_ERROR, "Modifier cannot be applied to a mesh with shape keys");
      return false;
    }

    /* Multires: ensure that recent sculpting is applied */
    if (md_eval->type == eModifierType_Multires) {
      multires_force_sculpt_rebuild(ob);
    }

    if (mmd && mmd->totlvl &&
        (mti->type == ModifierTypeType::OnlyDeform || md_eval->type == eModifierType_Nodes))
    {
      if (!multiresModifier_reshapeFromDeformModifier(depsgraph, ob, mmd, md_eval)) {
        BKE_report(reports, RPT_ERROR, "Multires modifier returned error, skipping apply");
        return false;
      }
    }
    else {
      Mesh *mesh_applied = create_applied_mesh_for_modifier(
          depsgraph,
          DEG_get_evaluated_scene(depsgraph),
          DEG_get_evaluated(depsgraph, ob),
          md_eval,
          /* It's important not to apply virtual modifiers (e.g. shape-keys) because they're kept,
           * causing them to be applied twice, see: #97758. */
          false,
          true,
          reports);
      if (!mesh_applied) {
        return false;
      }

      Main *bmain = DEG_get_bmain(depsgraph);
      BKE_object_material_from_eval_data(bmain, ob, &mesh_applied->id);
      BKE_mesh_nomain_to_mesh(mesh_applied, mesh, ob);

      /* Anonymous attributes shouldn't be available on the applied geometry. */
      mesh->attributes_for_write().remove_anonymous();

      /* Remove strings referring to attributes if they no longer exist. */
      bke::mesh_remove_invalid_attribute_strings(*mesh);

      if (md_eval->type == eModifierType_Multires) {
        multires_customdata_delete(mesh);
      }
    }
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    Object *object_eval = DEG_get_evaluated(depsgraph, ob);
    Curve *curve = static_cast<Curve *>(ob->data);
    Curve *curve_eval = static_cast<Curve *>(object_eval->data);
    ModifierEvalContext mectx = {depsgraph, object_eval, MOD_APPLY_TO_ORIGINAL};

    if (ELEM(mti->type, ModifierTypeType::Constructive, ModifierTypeType::Nonconstructive)) {
      BKE_report(
          reports,
          RPT_ERROR,
          "Cannot apply constructive modifiers on curve. Convert curve to mesh in order to apply");
      return false;
    }

    BKE_report(reports,
               RPT_INFO,
               "Applied modifier only changed CV points, not tessellated/bevel vertices");

    Array<float3> vertexCos = BKE_curve_nurbs_vert_coords_alloc(&curve_eval->nurb);
    mti->deform_verts(md_eval, &mectx, nullptr, vertexCos);
    BKE_curve_nurbs_vert_coords_apply(&curve->nurb, vertexCos, false);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else if (ob->type == OB_LATTICE) {
    Object *object_eval = DEG_get_evaluated(depsgraph, ob);
    Lattice *lattice = static_cast<Lattice *>(ob->data);
    ModifierEvalContext mectx = {depsgraph, object_eval, MOD_APPLY_TO_ORIGINAL};

    if (ELEM(mti->type, ModifierTypeType::Constructive, ModifierTypeType::Nonconstructive)) {
      BKE_report(reports, RPT_ERROR, "Constructive modifiers cannot be applied");
      return false;
    }

    Array<float3> positions = BKE_lattice_vert_coords_alloc(lattice);
    mti->deform_verts(md_eval, &mectx, nullptr, positions);
    BKE_lattice_vert_coords_apply(lattice, positions);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else if (ob->type == OB_CURVES) {
    Curves &curves = *static_cast<Curves *>(ob->data);
    if (mti->modify_geometry_set == nullptr) {
      BLI_assert_unreachable();
      return false;
    }

    bke::GeometrySet geometry_set = bke::GeometrySet::from_curves(
        &curves, bke::GeometryOwnershipType::ReadOnly);

    ModifierEvalContext mectx = {depsgraph, ob, MOD_APPLY_TO_ORIGINAL};
    mti->modify_geometry_set(md_eval, &mectx, &geometry_set);
    if (!geometry_set.has_curves()) {
      BKE_report(reports, RPT_ERROR, "Evaluated geometry from modifier does not contain curves");
      return false;
    }
    Curves &curves_eval = *geometry_set.get_curves_for_write();

    /* Anonymous attributes shouldn't be available on original geometry. */
    curves_eval.geometry.wrap().attributes_for_write().remove_anonymous();

    curves.geometry.wrap() = std::move(curves_eval.geometry.wrap());
    Main *bmain = DEG_get_bmain(depsgraph);
    BKE_object_material_from_eval_data(bmain, ob, &curves_eval.id);
  }
  else if (ob->type == OB_POINTCLOUD) {
    PointCloud &points = *static_cast<PointCloud *>(ob->data);
    if (mti->modify_geometry_set == nullptr) {
      BLI_assert_unreachable();
      return false;
    }

    bke::GeometrySet geometry_set = bke::GeometrySet::from_pointcloud(
        BKE_pointcloud_copy_for_eval(&points));

    ModifierEvalContext mectx = {depsgraph, ob, MOD_APPLY_TO_ORIGINAL};
    mti->modify_geometry_set(md_eval, &mectx, &geometry_set);
    if (!geometry_set.has_pointcloud()) {
      BKE_report(
          reports, RPT_ERROR, "Evaluated geometry from modifier does not contain a point cloud");
      return false;
    }
    PointCloud *pointcloud_eval =
        geometry_set.get_component_for_write<bke::PointCloudComponent>().release();

    /* Anonymous attributes shouldn't be available on original geometry. */
    pointcloud_eval->attributes_for_write().remove_anonymous();

    Main *bmain = DEG_get_bmain(depsgraph);
    BKE_object_material_from_eval_data(bmain, ob, &pointcloud_eval->id);
    BKE_pointcloud_nomain_to_pointcloud(pointcloud_eval, &points);
  }
  else if (ob->type == OB_GREASE_PENCIL) {
    if (mti->modify_geometry_set == nullptr) {
      BKE_report(reports, RPT_ERROR, "Cannot apply this modifier to Grease Pencil geometry");
      return false;
    }
    GreasePencil &grease_pencil_orig = *static_cast<GreasePencil *>(ob->data);
    bool success = false;
    if (do_all_keyframes) {
      /* The function #apply_grease_pencil_for_modifier_all_keyframes will retrieve
       * the evaluated modifier for each keyframe. The original modifier is passed
       * to ensure the evaluated modifier is not used, as it will be invalid when
       * the scene graph is updated for the next keyframe. */
      ModifierData *md = BKE_modifier_get_original(ob, md_eval);
      success = apply_grease_pencil_for_modifier_all_keyframes(
          depsgraph, scene, ob, grease_pencil_orig, md);
    }
    else {
      success = apply_grease_pencil_for_modifier(depsgraph, ob, grease_pencil_orig, md_eval);
    }
    if (!success) {
      BKE_report(reports,
                 RPT_ERROR,
                 "Evaluated geometry from modifier does not contain Grease Pencil geometry");
      return false;
    }
  }
  else {
    /* TODO: implement for volumes. */
    BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
    return false;
  }

  /* lattice modifier can be applied to particle system too */
  if (ob->particlesystem.first) {
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      if (psys->part->type != PART_HAIR) {
        continue;
      }

      psys_apply_hair_lattice(depsgraph, scene, ob, psys);
    }
  }

  return true;
}

bool modifier_apply(Main *bmain,
                    ReportList *reports,
                    Depsgraph *depsgraph,
                    Scene *scene,
                    Object *ob,
                    ModifierData *md,
                    int mode,
                    bool keep_modifier,
                    const bool do_all_keyframes)
{
  if (BKE_object_is_in_editmode(ob)) {
    BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in edit mode");
    return false;
  }
  if (mode != MODIFIER_APPLY_SHAPE && ID_REAL_USERS(ob->data) > 1) {
    BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
    return false;
  }
  if ((ob->mode & OB_MODE_SCULPT) && find_multires_modifier_before(scene, md) &&
      (BKE_modifier_is_same_topology(md) == false))
  {
    BKE_report(reports,
               RPT_ERROR,
               "Constructive modifier cannot be applied to multi-res data in sculpt mode");
    return false;
  }

  if (md != ob->modifiers.first) {
    BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected");
  }

  /* Get evaluated modifier, so object links pointer to evaluated data,
   * but still use original object it is applied to the original mesh. */
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  ModifierData *md_eval = (ob_eval) ? BKE_modifiers_findby_name(ob_eval, md->name) : md;

  Depsgraph *apply_depsgraph = depsgraph;
  Depsgraph *local_depsgraph = nullptr;

  /* If the object is hidden or the modifier is not enabled for the viewport is disabled a special
   * handling is required. This is because the viewport dependency graph optimizes out evaluation
   * of objects which are used by hidden objects and disabled modifiers.
   *
   * The idea is to create a dependency graph which does not perform those optimizations. */
  if ((ob_eval->base_flag & BASE_ENABLED_VIEWPORT) == 0 ||
      (md_eval->mode & eModifierMode_Realtime) == 0)
  {
    ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

    local_depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);
    DEG_disable_visibility_optimization(local_depsgraph);

    DEG_graph_build_from_ids(local_depsgraph, {&ob->id});
    DEG_evaluate_on_refresh(local_depsgraph);

    apply_depsgraph = local_depsgraph;

    /* The evaluated object and modifier are now from the different dependency graph. */
    ob_eval = DEG_get_evaluated(local_depsgraph, ob);
    md_eval = BKE_modifiers_findby_name(ob_eval, md->name);

    /* Force mode on the evaluated modifier, enforcing the modifier evaluation in the apply()
     * functions. */
    md_eval->mode |= eModifierMode_Realtime;
  }

  bool did_apply = false;
  if (mode == MODIFIER_APPLY_SHAPE) {
    did_apply = modifier_apply_shape(bmain, reports, apply_depsgraph, scene, ob, md_eval);
  }
  else {
    did_apply = modifier_apply_obdata(
        reports, apply_depsgraph, scene, ob, md_eval, do_all_keyframes);
  }

  if (did_apply) {
    if (!keep_modifier) {
      BKE_modifier_remove_from_list(ob, md);
      BKE_modifier_free(md);
    }
    BKE_object_free_derived_caches(ob);
  }

  if (local_depsgraph != nullptr) {
    DEG_graph_free(local_depsgraph);
  }

  return true;
}

bool modifier_copy(
    ReportList * /*reports*/, Main *bmain, Scene *scene, Object *ob, ModifierData *md)
{
  if (md->type == eModifierType_ParticleSystem) {
    ModifierData *nmd = object_copy_particle_system(
        bmain, scene, ob, ((ParticleSystemModifierData *)md)->psys);
    BLI_remlink(&ob->modifiers, nmd);
    BLI_insertlinkafter(&ob->modifiers, md, nmd);
    BKE_object_modifier_set_active(ob, nmd);
    return true;
  }

  ModifierData *nmd = BKE_modifier_new(md->type);
  BKE_modifier_copydata(md, nmd);
  BLI_insertlinkafter(&ob->modifiers, md, nmd);
  STRNCPY_UTF8(nmd->name, md->name);
  BKE_modifier_unique_name(&ob->modifiers, nmd);
  BKE_modifiers_persistent_uid_init(*ob, *nmd);
  BKE_object_modifier_set_active(ob, nmd);

  nmd->flag |= eModifierFlag_OverrideLibrary_Local;

  return true;
}

/** \} */

Vector<PointerRNA> modifier_get_edit_objects(const bContext &C, const wmOperator &op)
{
  Vector<PointerRNA> objects;
  if (RNA_boolean_get(op.ptr, "use_selected_objects")) {
    CTX_data_selected_editable_objects(&C, &objects);
  }
  else {
    if (Object *object = context_active_object(&C)) {
      objects.append(RNA_id_pointer_create(&object->id));
    }
  }
  return objects;
}

void modifier_register_use_selected_objects_prop(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "use_selected_objects",
      false,
      "Selected Objects",
      "Affect all selected objects instead of just the active object");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/* ------------------------------------------------------------------- */
/** \name Add Modifier Operator
 * \{ */

static wmOperatorStatus modifier_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int type = RNA_enum_get(op->ptr, "type");

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    if (!modifier_add(op->reports, bmain, scene, ob, nullptr, type)) {
      continue;
    }
    changed = true;
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, ob);
  }
  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_add_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->modifier & KM_ALT || CTX_wm_view3d(C)) {
    RNA_boolean_set(op->ptr, "use_selected_objects", true);
  }
  if (!RNA_struct_property_is_set(op->ptr, "type")) {
    return WM_menu_invoke(C, op, event);
  }
  return modifier_add_exec(C, op);
}

static const EnumPropertyItem *modifier_add_itemf(bContext *C,
                                                  PointerRNA * /*ptr*/,
                                                  PropertyRNA * /*prop*/,
                                                  bool *r_free)
{
  Object *ob = context_active_object(C);

  if (!ob) {
    return rna_enum_object_modifier_type_items;
  }

  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  const EnumPropertyItem *group_item = nullptr;
  for (int a = 0; rna_enum_object_modifier_type_items[a].identifier; a++) {
    const EnumPropertyItem *md_item = &rna_enum_object_modifier_type_items[a];

    if (md_item->identifier[0]) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md_item->value);

      if (mti->flags & eModifierTypeFlag_NoUserAdd) {
        continue;
      }

      if (!BKE_object_support_modifier_type_check(ob, md_item->value)) {
        continue;
      }
    }
    else {
      group_item = md_item;
      continue;
    }

    if (group_item) {
      RNA_enum_item_add(&items, &totitem, group_item);
      group_item = nullptr;
    }

    RNA_enum_item_add(&items, &totitem, md_item);
  }

  RNA_enum_item_end(&items, &totitem);
  *r_free = true;

  return items;
}

void OBJECT_OT_modifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Modifier";
  ot->description = "Add a procedural operation/effect to the active object";
  ot->idname = "OBJECT_OT_modifier_add";

  /* API callbacks. */
  ot->invoke = modifier_add_invoke;
  ot->exec = modifier_add_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(
      ot->srna, "type", rna_enum_object_modifier_type_items, eModifierType_Subsurf, "Type", "");
  RNA_def_enum_funcs(prop, modifier_add_itemf);
  ot->prop = prop;
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Generic Poll Function and Properties
 *
 * Using modifier names and data context.
 * \{ */

bool edit_modifier_poll_generic(bContext *C,
                                StructRNA *rna_type,
                                int obtype_flag,
                                const bool is_editmode_allowed,
                                const bool is_liboverride_allowed)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", rna_type);
  Object *ob = (ptr.owner_id) ? (Object *)ptr.owner_id : context_active_object(C);
  ModifierData *mod = static_cast<ModifierData *>(ptr.data); /* May be nullptr. */

  if (mod == nullptr && ob != nullptr) {
    mod = BKE_object_active_modifier(ob);
  }

  if (!ob || !BKE_id_is_editable(bmain, &ob->id)) {
    return false;
  }
  if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) {
    return false;
  }
  if (ptr.owner_id && !BKE_id_is_editable(bmain, ptr.owner_id)) {
    return false;
  }

  if (!is_liboverride_allowed && BKE_modifier_is_nonlocal_in_liboverride(ob, mod)) {
    CTX_wm_operator_poll_msg_set(
        C, "Cannot edit modifiers coming from linked data in a library override");
    return false;
  }

  if (!is_editmode_allowed && CTX_data_edit_object(C) != nullptr) {
    CTX_wm_operator_poll_msg_set(C, "This modifier operation is not allowed from Edit mode");
    return false;
  }

  return true;
}

static bool edit_modifier_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_Modifier, 0, true, false);
}

/* Used by operators performing actions allowed also on modifiers from the overridden linked object
 * (not only from added 'local' ones). */
static bool edit_modifier_liboverride_allowed_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_Modifier, 0, true, true);
}

void edit_modifier_properties(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "modifier", nullptr, MAX_NAME, "Modifier", "Name of the modifier to edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

static void edit_modifier_report_property(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "report", false, "Report", "Create a notification after the operation");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Generic Invoke Functions
 *
 * Using modifier names and data context.
 * \{ */

bool edit_modifier_invoke_properties(bContext *C, wmOperator *op)
{
  if (RNA_struct_property_is_set(op->ptr, "modifier")) {
    return true;
  }

  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
  if (ctx_ptr.data != nullptr) {
    ModifierData *md = static_cast<ModifierData *>(ctx_ptr.data);
    RNA_string_set(op->ptr, "modifier", md->name);
    return true;
  }

  return false;
}

/**
 * If the "modifier" property is not set, fill the modifier property with the name of the modifier
 * with a UI panel below the mouse cursor, unless a specific modifier is set with a context
 * pointer. Used in order to apply modifier operators on hover over their panels.
 */
static bool edit_modifier_invoke_properties_with_hover(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event,
                                                       wmOperatorStatus *r_retval)
{
  if (RNA_struct_find_property(op->ptr, "use_selected_objects")) {
    if (event->modifier & KM_ALT) {
      RNA_boolean_set(op->ptr, "use_selected_objects", true);
    }
  }

  if (RNA_struct_property_is_set(op->ptr, "modifier")) {
    return true;
  }

  /* Note that the context pointer is *not* the active modifier, it is set in UI layouts. */
  PointerRNA ctx_ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
  if (ctx_ptr.data != nullptr) {
    ModifierData *md = static_cast<ModifierData *>(ctx_ptr.data);
    RNA_string_set(op->ptr, "modifier", md->name);
    return true;
  }

  PointerRNA *panel_ptr = UI_region_panel_custom_data_under_cursor(C, event);
  if (panel_ptr == nullptr || RNA_pointer_is_null(panel_ptr)) {
    *r_retval = OPERATOR_CANCELLED;
    return false;
  }

  if (!RNA_struct_is_a(panel_ptr->type, &RNA_Modifier)) {
    /* Work around multiple operators using the same shortcut. The operators for the other
     * stacks in the property editor use the same key, and will not run after these return
     * OPERATOR_CANCELLED. */
    *r_retval = (OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED);
    return false;
  }

  const ModifierData *md = static_cast<const ModifierData *>(panel_ptr->data);
  RNA_string_set(op->ptr, "modifier", md->name);
  return true;
}

ModifierData *edit_modifier_property_get(wmOperator *op, Object *ob, int type)
{
  char modifier_name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", modifier_name);

  ModifierData *md = BKE_modifiers_findby_name(ob, modifier_name);

  if (md && type != 0 && md->type != type) {
    md = nullptr;
  }

  return md;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Remove Modifier Operator
 * \{ */

static wmOperatorStatus modifier_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (md == nullptr) {
      continue;
    }

    int mode_orig = ob->mode;
    if (!modifier_remove(op->reports, bmain, scene, ob, md)) {
      continue;
    }

    changed = true;

    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_REMOVED, ob);

    /* if cloth/softbody was removed, particle mode could be cleared */
    if (mode_orig & OB_MODE_PARTICLE_EDIT) {
      if ((ob->mode & OB_MODE_PARTICLE_EDIT) == 0) {
        BKE_view_layer_synced_ensure(scene, view_layer);
        if (ob == BKE_view_layer_active_object_get(view_layer)) {
          WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
        }
      }
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "report")) {
    BKE_reportf(op->reports, RPT_INFO, "Removed modifier: %s", name);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_remove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_remove_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_remove(wmOperatorType *ot)
{
  ot->name = "Remove Modifier";
  ot->description = "Remove a modifier from the active object";
  ot->idname = "OBJECT_OT_modifier_remove";

  ot->invoke = modifier_remove_invoke;
  ot->exec = modifier_remove_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  edit_modifier_report_property(ot);
  modifier_register_use_selected_objects_prop(ot);
}

static wmOperatorStatus modifiers_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  CTX_DATA_BEGIN (C, Object *, object, selected_editable_objects) {
    modifiers_clear(bmain, scene, object);
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

static bool modifiers_clear_poll(bContext *C)
{
  if (!ED_operator_object_active_local_editable(C)) {
    return false;
  }
  const Object *object = context_active_object(C);
  if (!BKE_object_supports_modifiers(object)) {
    return false;
  }
  return true;
}

void OBJECT_OT_modifiers_clear(wmOperatorType *ot)
{
  ot->name = "Clear Object Modifiers";
  ot->description = "Clear all modifiers from the selected objects";
  ot->idname = "OBJECT_OT_modifiers_clear";

  ot->exec = modifiers_clear_exec;
  ot->poll = modifiers_clear_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Up Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_up_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  if (!md || !modifier_move_up(op->reports, RPT_WARNING, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_move_up_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_move_up(wmOperatorType *ot)
{
  ot->name = "Move Up Modifier";
  ot->description = "Move modifier up in the stack";
  ot->idname = "OBJECT_OT_modifier_move_up";

  ot->invoke = modifier_move_up_invoke;
  ot->exec = modifier_move_up_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move Down Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_down_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  if (!md || !modifier_move_down(op->reports, RPT_WARNING, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_down_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_move_down_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_move_down(wmOperatorType *ot)
{
  ot->name = "Move Down Modifier";
  ot->description = "Move modifier down in the stack";
  ot->idname = "OBJECT_OT_modifier_move_down";

  ot->invoke = modifier_move_down_invoke;
  ot->exec = modifier_move_down_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Move to Index Modifier Operator
 * \{ */

static wmOperatorStatus modifier_move_to_index_exec(bContext *C, wmOperator *op)
{
  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  const int index = RNA_int_get(op->ptr, "index");

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (!md) {
      continue;
    }

    if (!modifier_move_to_index(op->reports, RPT_WARNING, ob, md, index, true)) {
      continue;
    }
    changed = true;
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_move_to_index_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_move_to_index_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_move_to_index(wmOperatorType *ot)
{
  ot->name = "Move Active Modifier to Index";
  ot->description =
      "Change the modifier's index in the stack so it evaluates after the set number of others";
  ot->idname = "OBJECT_OT_modifier_move_to_index";

  ot->invoke = modifier_move_to_index_invoke;
  ot->exec = modifier_move_to_index_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "The index to move the modifier to", 0, INT_MAX);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Apply Modifier Operator
 * \{ */

static bool modifier_apply_poll(bContext *C)
{
  if (!edit_modifier_poll_generic(C, &RNA_Modifier, 0, false, false)) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
  Object *ob = (ptr.owner_id != nullptr) ? (Object *)ptr.owner_id : context_active_object(C);
  ModifierData *md = static_cast<ModifierData *>(ptr.data); /* May be nullptr. */

  if (ID_IS_OVERRIDE_LIBRARY(ob) || ((ob->data != nullptr) && ID_IS_OVERRIDE_LIBRARY(ob->data))) {
    CTX_wm_operator_poll_msg_set(C, "Modifiers cannot be applied on override data");
    return false;
  }
  if (md != nullptr) {
    if ((ob->mode & OB_MODE_SCULPT) && find_multires_modifier_before(scene, md) &&
        (BKE_modifier_is_same_topology(md) == false))
    {
      CTX_wm_operator_poll_msg_set(
          C, "Constructive modifier cannot be applied to multi-res data in sculpt mode");
      return false;
    }
  }
  return true;
}

static wmOperatorStatus modifier_apply_exec_ex(bContext *C,
                                               wmOperator *op,
                                               int apply_as,
                                               bool keep_modifier)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Vector<PointerRNA> objects = modifier_get_edit_objects(*C, *op);

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  const bool do_report = RNA_boolean_get(op->ptr, "report");
  const int reports_len = do_report ? BLI_listbase_count(&op->reports->list) : 0;

  const bool do_single_user = (apply_as == MODIFIER_APPLY_DATA) ?
                                  RNA_boolean_get(op->ptr, "single_user") :
                                  false;
  const bool do_merge_customdata = (apply_as == MODIFIER_APPLY_DATA) ?
                                       RNA_boolean_get(op->ptr, "merge_customdata") :
                                       false;
  const bool do_all_keyframes = (apply_as == MODIFIER_APPLY_DATA) ?
                                    RNA_boolean_get(op->ptr, "all_keyframes") :
                                    false;

  bool changed = false;
  for (const PointerRNA &ptr : objects) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (md == nullptr) {
      continue;
    }

    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)md->type);

    if (do_single_user && ID_REAL_USERS(ob->data) > 1) {
      single_obdata_user_make(bmain, scene, ob);
      BKE_main_id_newptr_and_tag_clear(bmain);
      WM_event_add_notifier(C, NC_WINDOW, nullptr);
      DEG_relations_tag_update(bmain);
    }

    if (!modifier_apply(bmain,
                        op->reports,
                        depsgraph,
                        scene,
                        ob,
                        md,
                        apply_as,
                        keep_modifier,
                        do_all_keyframes))
    {
      continue;
    }
    changed = true;

    if (ob->type == OB_MESH && do_merge_customdata &&
        ELEM(mti->type, ModifierTypeType::Constructive, ModifierTypeType::Nonconstructive))
    {
      BKE_mesh_merge_customdata_for_apply_modifier((Mesh *)ob->data);
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (do_report) {
    /* Only add this report if the operator didn't cause another one. The purpose here is
     * to alert that something happened, and the previous report will do that anyway. */
    if (BLI_listbase_count(&op->reports->list) == reports_len) {
      BKE_reportf(op->reports, RPT_INFO, "Applied modifier: %s", name);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_apply_exec(bContext *C, wmOperator *op)
{
  return modifier_apply_exec_ex(C, op, MODIFIER_APPLY_DATA, false);
}

static wmOperatorStatus modifier_apply_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
    Object *ob = (ptr.owner_id != nullptr) ? (Object *)ptr.owner_id : context_active_object(C);

    if ((ob->data != nullptr) && ID_REAL_USERS(ob->data) > 1) {
      PropertyRNA *prop = RNA_struct_find_property(op->ptr, "single_user");
      if (!RNA_property_is_set(op->ptr, prop)) {
        RNA_property_boolean_set(op->ptr, prop, true);
      }
      if (RNA_property_boolean_get(op->ptr, prop)) {
        return WM_operator_confirm_ex(
            C,
            op,
            IFACE_("Apply Modifier"),
            IFACE_("Make data single-user, apply modifier, and remove it from the list."),
            IFACE_("Apply"),
            ALERT_ICON_WARNING,
            false);
      }
    }
    return modifier_apply_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_apply(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Apply Modifier";
  ot->description = "Apply modifier and remove from the stack";
  ot->idname = "OBJECT_OT_modifier_apply";

  ot->invoke = modifier_apply_invoke;
  ot->exec = modifier_apply_exec;
  ot->poll = modifier_apply_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  edit_modifier_properties(ot);
  edit_modifier_report_property(ot);

  RNA_def_boolean(ot->srna,
                  "merge_customdata",
                  true,
                  "Merge UVs",
                  "For mesh objects, merge UV coordinates that share a vertex to account for "
                  "imprecision in some modifiers");
  prop = RNA_def_boolean(ot->srna,
                         "single_user",
                         false,
                         "Make Data Single User",
                         "Make the object's data single user if needed");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "all_keyframes",
                         false,
                         "Apply to all keyframes",
                         "For Grease Pencil objects, apply the modifier to all the keyframes");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Apply Modifier As Shape-Key Operator
 * \{ */

static bool modifier_apply_as_shapekey_poll(bContext *C)
{
  return modifier_apply_poll(C);
}

static wmOperatorStatus modifier_apply_as_shapekey_exec(bContext *C, wmOperator *op)
{
  bool keep = RNA_boolean_get(op->ptr, "keep_modifier");

  return modifier_apply_exec_ex(C, op, MODIFIER_APPLY_SHAPE, keep);
}

static wmOperatorStatus modifier_apply_as_shapekey_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_apply_as_shapekey_exec(C, op);
  }
  return retval;
}

static std::string modifier_apply_as_shapekey_get_description(bContext * /*C*/,
                                                              wmOperatorType * /*ot*/,
                                                              PointerRNA *ptr)
{
  bool keep = RNA_boolean_get(ptr, "keep_modifier");
  if (keep) {
    return TIP_("Apply modifier as a new shapekey and keep it in the stack");
  }

  return "";
}

void OBJECT_OT_modifier_apply_as_shapekey(wmOperatorType *ot)
{
  ot->name = "Apply Modifier as Shape Key";
  ot->description = "Apply modifier as a new shape key and remove from the stack";
  ot->idname = "OBJECT_OT_modifier_apply_as_shapekey";

  ot->invoke = modifier_apply_as_shapekey_invoke;
  ot->exec = modifier_apply_as_shapekey_exec;
  ot->poll = modifier_apply_as_shapekey_poll;
  ot->get_description = modifier_apply_as_shapekey_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_boolean(
      ot->srna, "keep_modifier", false, "Keep Modifier", "Do not remove the modifier from stack");
  edit_modifier_properties(ot);
  edit_modifier_report_property(ot);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Convert Particle System Modifier to Mesh Operator
 * \{ */

static wmOperatorStatus modifier_convert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  if (!md || !convert_psys_to_mesh(op->reports, bmain, depsgraph, scene, view_layer, ob, md)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_convert_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return modifier_convert_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_convert(wmOperatorType *ot)
{
  ot->name = "Convert Particles to Mesh";
  ot->description = "Convert particles to a mesh object";
  ot->idname = "OBJECT_OT_modifier_convert";

  ot->invoke = modifier_convert_invoke;
  ot->exec = modifier_convert_exec;
  ot->poll = edit_modifier_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Modifier Operator
 * \{ */

static wmOperatorStatus modifier_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  char name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier", name);

  bool changed = false;
  for (const PointerRNA &ptr : modifier_get_edit_objects(*C, *op)) {
    Object *ob = static_cast<Object *>(ptr.data);
    ModifierData *md = BKE_modifiers_findby_name(ob, name);
    if (!md) {
      continue;
    }

    if (!modifier_copy(op->reports, bmain, scene, ob, md)) {
      continue;
    }
    changed = true;
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, ob);
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_copy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_copy_exec(C, op);
  }
  return retval;
}

void OBJECT_OT_modifier_copy(wmOperatorType *ot)
{
  ot->name = "Copy Modifier";
  ot->description = "Duplicate modifier at the same position in the stack";
  ot->idname = "OBJECT_OT_modifier_copy";

  ot->invoke = modifier_copy_invoke;
  ot->exec = modifier_copy_exec;
  ot->poll = edit_modifier_liboverride_allowed_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
  modifier_register_use_selected_objects_prop(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Set Active Modifier Operator
 * \{ */

static wmOperatorStatus modifier_set_active_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, ob, 0);

  /* If there is no modifier set for this operator, clear the active modifier field. */
  BKE_object_modifier_set_active(ob, md);

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_set_active_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_set_active_exec(C, op);
  }
  return retval;
}

static bool modifier_set_active_poll(bContext *C)
{
  /* Only make this operator work in the Modifiers tab of the Properties editor.
   * Otherwise it may eat up too many mouse click events. */
  SpaceProperties *space_properties = CTX_wm_space_properties(C);
  if (!(space_properties && space_properties->mainb == BCONTEXT_MODIFIER)) {
    return false;
  }

  return ED_operator_object_active_only(C);
}

void OBJECT_OT_modifier_set_active(wmOperatorType *ot)
{
  ot->name = "Set Active Modifier";
  ot->description = "Activate the modifier to use as the context";
  ot->idname = "OBJECT_OT_modifier_set_active";

  ot->invoke = modifier_set_active_invoke;
  ot->exec = modifier_set_active_exec;
  ot->poll = modifier_set_active_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy Modifier To Selected Operator
 * \{ */

static wmOperatorStatus modifier_copy_to_selected_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  Object *obact = context_active_object(C);
  ModifierData *md = edit_modifier_property_get(op, obact, 0);
  if (!md) {
    return OPERATOR_CANCELLED;
  }

  int num_copied = 0;

  Vector<PointerRNA> selected_objects;
  CTX_data_selected_objects(C, &selected_objects);
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }
    if (!ID_IS_EDITABLE(ob)) {
      continue;
    }
    if (modifier_copy_to_object(bmain, scene, obact, md, ob, op->reports)) {
      WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, ob);
      num_copied++;
    }
  }
  CTX_DATA_END;

  if (num_copied > 0) {
    DEG_relations_tag_update(bmain);
  }
  else {
    BKE_reportf(op->reports, RPT_ERROR, "Modifier '%s' was not copied to any objects", md->name);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus modifier_copy_to_selected_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  wmOperatorStatus retval;
  if (edit_modifier_invoke_properties_with_hover(C, op, event, &retval)) {
    return modifier_copy_to_selected_exec(C, op);
  }
  return retval;
}

static bool modifier_copy_to_selected_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
  Object *obact = (ptr.owner_id) ? (Object *)ptr.owner_id : context_active_object(C);
  ModifierData *md = static_cast<ModifierData *>(ptr.data);

  /* This just mirrors the check in #BKE_object_copy_modifier,
   * but there is no reasoning for it there. */
  if (md && ELEM(md->type, eModifierType_Hook, eModifierType_Collision)) {
    CTX_wm_operator_poll_msg_set(C, R"(Not supported for "Collision" or "Hook" modifiers)");
    return false;
  }

  if (!obact) {
    CTX_wm_operator_poll_msg_set(C, "No selected object is active");
    return false;
  }

  if (!BKE_object_supports_modifiers(obact)) {
    CTX_wm_operator_poll_msg_set(C, "Object type of source object is not supported");
    return false;
  }

  /* This could have a performance impact in the worst case, where there are many objects selected
   * and none of them pass either of the checks. But that should be uncommon, and this operator is
   * only exposed in a drop-down menu anyway. */
  bool found_supported_objects = false;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob == obact) {
      continue;
    }

    if (!md) {
      /* Skip type check if modifier could not be found ("modifier" context variable not set). */
      if (BKE_object_supports_modifiers(ob)) {
        found_supported_objects = true;
        break;
      }
    }
    else if (BKE_object_support_modifier_type_check(ob, md->type)) {
      found_supported_objects = true;
      break;
    }
  }
  CTX_DATA_END;

  if (!found_supported_objects) {
    CTX_wm_operator_poll_msg_set(C, "No supported objects were selected");
    return false;
  }
  return true;
}

void OBJECT_OT_modifier_copy_to_selected(wmOperatorType *ot)
{
  ot->name = "Copy Modifier to Selected";
  ot->description = "Copy the modifier from the active object to all selected objects";
  ot->idname = "OBJECT_OT_modifier_copy_to_selected";

  ot->invoke = modifier_copy_to_selected_invoke;
  ot->exec = modifier_copy_to_selected_exec;
  ot->poll = modifier_copy_to_selected_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

static wmOperatorStatus object_modifiers_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  Object *active_object = context_active_object(C);

  Vector<PointerRNA> selected_objects;
  CTX_data_selected_objects(C, &selected_objects);
  CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
    if (object == active_object) {
      continue;
    }
    LISTBASE_FOREACH (const ModifierData *, md, &active_object->modifiers) {
      if (modifier_copy_to_object(bmain, scene, active_object, md, object, op->reports)) {
        WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, object);
      }
    }
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

static bool modifiers_copy_to_selected_poll(bContext *C)
{
  if (!ED_operator_object_active_editable(C)) {
    return false;
  }
  const Object *active_object = context_active_object(C);
  if (!BKE_object_supports_modifiers(active_object)) {
    return false;
  }
  if (BLI_listbase_is_empty(&active_object->modifiers)) {
    CTX_wm_operator_poll_msg_set(C, "Active object has no modifiers");
    return false;
  }
  return true;
}

void OBJECT_OT_modifiers_copy_to_selected(wmOperatorType *ot)
{
  ot->name = "Copy Modifiers to Selected Objects";
  ot->idname = "OBJECT_OT_modifiers_copy_to_selected";
  ot->description = "Copy modifiers to other selected objects";

  ot->exec = object_modifiers_copy_exec;
  ot->poll = modifiers_copy_to_selected_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Skin Modifier
 * \{ */

static void modifier_skin_customdata_delete(Object *ob)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    BM_data_layer_free(em->bm, &em->bm->vdata, CD_MVERT_SKIN);
  }
  else {
    CustomData_free_layer_active(&mesh->vert_data, CD_MVERT_SKIN);
  }
}

static bool skin_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_SkinModifier, (1 << OB_MESH), false, false);
}

static bool skin_edit_poll(bContext *C)
{
  Object *ob = CTX_data_edit_object(C);
  return (ob != nullptr &&
          edit_modifier_poll_generic(C, &RNA_SkinModifier, (1 << OB_MESH), true, false) &&
          !ID_IS_OVERRIDE_LIBRARY(ob) && !ID_IS_OVERRIDE_LIBRARY(ob->data));
}

static void skin_root_clear(BMVert *bm_vert, GSet *visited, const int cd_vert_skin_offset)
{
  BMEdge *bm_edge;
  BMIter bm_iter;

  BM_ITER_ELEM (bm_edge, &bm_iter, bm_vert, BM_EDGES_OF_VERT) {
    BMVert *v2 = BM_edge_other_vert(bm_edge, bm_vert);

    if (BLI_gset_add(visited, v2)) {
      MVertSkin *vs = static_cast<MVertSkin *>(BM_ELEM_CD_GET_VOID_P(v2, cd_vert_skin_offset));

      /* clear vertex root flag and add to visited set */
      vs->flag &= ~MVERT_SKIN_ROOT;

      skin_root_clear(v2, visited, cd_vert_skin_offset);
    }
  }
}

static wmOperatorStatus skin_root_mark_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BMesh *bm = em->bm;

  GSet *visited = BLI_gset_ptr_new(__func__);

  BKE_mesh_ensure_skin_customdata(static_cast<Mesh *>(ob->data));

  const int cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);

  BMVert *bm_vert;
  BMIter bm_iter;
  BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT) && BLI_gset_add(visited, bm_vert)) {
      MVertSkin *vs = static_cast<MVertSkin *>(
          BM_ELEM_CD_GET_VOID_P(bm_vert, cd_vert_skin_offset));

      /* mark vertex as root and add to visited set */
      vs->flag |= MVERT_SKIN_ROOT;

      /* clear root flag from all connected vertices (recursively) */
      skin_root_clear(bm_vert, visited, cd_vert_skin_offset);
    }
  }

  BLI_gset_free(visited, nullptr);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_root_mark(wmOperatorType *ot)
{
  ot->name = "Skin Root Mark";
  ot->description = "Mark selected vertices as roots";
  ot->idname = "OBJECT_OT_skin_root_mark";

  ot->poll = skin_edit_poll;
  ot->exec = skin_root_mark_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

enum SkinLooseAction {
  SKIN_LOOSE_MARK,
  SKIN_LOOSE_CLEAR,
};

static wmOperatorStatus skin_loose_mark_clear_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BMesh *bm = em->bm;
  SkinLooseAction action = static_cast<SkinLooseAction>(RNA_enum_get(op->ptr, "action"));

  if (!CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
    return OPERATOR_CANCELLED;
  }

  BMVert *bm_vert;
  BMIter bm_iter;
  BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT)) {
      MVertSkin *vs = static_cast<MVertSkin *>(
          CustomData_bmesh_get(&bm->vdata, bm_vert->head.data, CD_MVERT_SKIN));

      switch (action) {
        case SKIN_LOOSE_MARK:
          vs->flag |= MVERT_SKIN_LOOSE;
          break;
        case SKIN_LOOSE_CLEAR:
          vs->flag &= ~MVERT_SKIN_LOOSE;
          break;
      }
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_loose_mark_clear(wmOperatorType *ot)
{
  static const EnumPropertyItem action_items[] = {
      {SKIN_LOOSE_MARK, "MARK", 0, "Mark", "Mark selected vertices as loose"},
      {SKIN_LOOSE_CLEAR, "CLEAR", 0, "Clear", "Set selected vertices as not loose"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Skin Mark/Clear Loose";
  ot->description = "Mark/clear selected vertices as loose";
  ot->idname = "OBJECT_OT_skin_loose_mark_clear";

  ot->poll = skin_edit_poll;
  ot->exec = skin_loose_mark_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "action", action_items, SKIN_LOOSE_MARK, "Action", nullptr);
}

static wmOperatorStatus skin_radii_equalize_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BMesh *bm = em->bm;

  if (!CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
    return OPERATOR_CANCELLED;
  }

  BMVert *bm_vert;
  BMIter bm_iter;
  BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT)) {
      MVertSkin *vs = static_cast<MVertSkin *>(
          CustomData_bmesh_get(&bm->vdata, bm_vert->head.data, CD_MVERT_SKIN));
      float avg = (vs->radius[0] + vs->radius[1]) * 0.5f;

      vs->radius[0] = vs->radius[1] = avg;
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_radii_equalize(wmOperatorType *ot)
{
  ot->name = "Skin Radii Equalize";
  ot->description = "Make skin radii of selected vertices equal on each axis";
  ot->idname = "OBJECT_OT_skin_radii_equalize";

  ot->poll = skin_edit_poll;
  ot->exec = skin_radii_equalize_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void skin_armature_bone_create(Object *skin_ob,
                                      const Span<float3> positions,
                                      const int2 *edges,
                                      bArmature *arm,
                                      BLI_bitmap *edges_visited,
                                      const GroupedSpan<int> emap,
                                      EditBone *parent_bone,
                                      int parent_v)
{
  for (int i = 0; i < emap[parent_v].size(); i++) {
    int endx = emap[parent_v][i];
    const int2 &edge = edges[endx];

    /* ignore edge if already visited */
    if (BLI_BITMAP_TEST(edges_visited, endx)) {
      continue;
    }
    BLI_BITMAP_ENABLE(edges_visited, endx);

    int v = bke::mesh::edge_other_vert(edge, parent_v);

    EditBone *bone = ED_armature_ebone_add(arm, "Bone");

    bone->parent = parent_bone;
    if (parent_bone != nullptr) {
      bone->flag |= BONE_CONNECTED;
    }

    copy_v3_v3(bone->head, positions[parent_v]);
    copy_v3_v3(bone->tail, positions[v]);
    bone->rad_head = bone->rad_tail = 0.25;
    SNPRINTF_UTF8(bone->name, "Bone.%.2d", endx);

    /* add bDeformGroup */
    bDeformGroup *dg = BKE_object_defgroup_add_name(skin_ob, bone->name);
    if (dg != nullptr) {
      blender::ed::object::vgroup_vert_add(skin_ob, dg, parent_v, 1, WEIGHT_REPLACE);
      blender::ed::object::vgroup_vert_add(skin_ob, dg, v, 1, WEIGHT_REPLACE);
    }

    skin_armature_bone_create(skin_ob, positions, edges, arm, edges_visited, emap, bone, v);
  }
}

static Object *modifier_skin_armature_create(Depsgraph *depsgraph, Main *bmain, Object *skin_ob)
{
  Mesh *mesh = static_cast<Mesh *>(skin_ob->data);
  const Span<float3> me_positions = mesh->vert_positions();
  const Span<int2> me_edges = mesh->edges();

  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated(depsgraph, skin_ob);

  const Mesh *me_eval_deform = blender::bke::mesh_get_eval_deform(
      depsgraph, scene_eval, ob_eval, &CD_MASK_BAREMESH);
  const Span<float3> positions_eval = me_eval_deform->vert_positions();

  /* add vertex weights to original mesh */
  mesh->deform_verts_for_write();

  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  Object *arm_ob = BKE_object_add(bmain, scene, view_layer, OB_ARMATURE, nullptr);
  BKE_object_transform_copy(arm_ob, skin_ob);
  bArmature *arm = static_cast<bArmature *>(arm_ob->data);
  ANIM_armature_bonecoll_show_all(arm);
  arm_ob->dtx |= OB_DRAW_IN_FRONT;
  arm->drawtype = ARM_DRAW_TYPE_STICK;
  arm->edbo = MEM_callocN<ListBase>("edbo armature");

  MVertSkin *mvert_skin = static_cast<MVertSkin *>(
      CustomData_get_layer_for_write(&mesh->vert_data, CD_MVERT_SKIN, mesh->verts_num));

  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  const GroupedSpan<int> emap = bke::mesh::build_vert_to_edge_map(
      me_edges, mesh->verts_num, vert_to_edge_offsets, vert_to_edge_indices);

  BLI_bitmap *edges_visited = BLI_BITMAP_NEW(mesh->edges_num, "edge_visited");

  /* NOTE: we use EditBones here, easier to set them up and use
   * edit-armature functions to convert back to regular bones */
  for (int v = 0; v < mesh->verts_num; v++) {
    if (mvert_skin[v].flag & MVERT_SKIN_ROOT) {
      EditBone *bone = nullptr;

      /* Unless the skin root has just one adjacent edge, create
       * a fake root bone (have it going off in the Y direction
       * (arbitrary) */
      if (emap[v].size() > 1) {
        bone = ED_armature_ebone_add(arm, "Bone");

        copy_v3_v3(bone->head, me_positions[v]);
        copy_v3_v3(bone->tail, me_positions[v]);

        bone->head[1] = 1.0f;
        bone->rad_head = bone->rad_tail = 0.25;
      }

      if (emap[v].size() >= 1) {
        skin_armature_bone_create(
            skin_ob, positions_eval, me_edges.data(), arm, edges_visited, emap, bone, v);
      }
    }
  }

  MEM_freeN(edges_visited);

  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);

  return arm_ob;
}

static wmOperatorStatus skin_armature_create_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  ModifierData *skin_md;

  if (!CustomData_has_layer(&mesh->vert_data, CD_MVERT_SKIN)) {
    BKE_reportf(op->reports, RPT_WARNING, "Mesh '%s' has no skin vertex data", mesh->id.name + 2);
    return OPERATOR_CANCELLED;
  }

  /* create new armature */
  Object *arm_ob = modifier_skin_armature_create(depsgraph, bmain, ob);

  /* add a modifier to connect the new armature to the mesh */
  ArmatureModifierData *arm_md = (ArmatureModifierData *)BKE_modifier_new(eModifierType_Armature);
  if (arm_md) {
    skin_md = edit_modifier_property_get(op, ob, eModifierType_Skin);
    BLI_insertlinkafter(&ob->modifiers, skin_md, arm_md);
    BKE_modifiers_persistent_uid_init(*arm_ob, arm_md->modifier);

    arm_md->object = arm_ob;
    arm_md->deformflag = ARM_DEF_VGROUP | ARM_DEF_QUATERNION;
    DEG_relations_tag_update(bmain);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus skin_armature_create_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return skin_armature_create_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_skin_armature_create(wmOperatorType *ot)
{
  ot->name = "Skin Armature Create";
  ot->description = "Create an armature that parallels the skin layout";
  ot->idname = "OBJECT_OT_skin_armature_create";

  ot->poll = skin_poll;
  ot->invoke = skin_armature_create_invoke;
  ot->exec = skin_armature_create_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Delta Mesh Bind Operator
 * \{ */

static bool correctivesmooth_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_CorrectiveSmoothModifier, 0, true, false);
}

static wmOperatorStatus correctivesmooth_bind_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = context_active_object(C);
  CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_CorrectiveSmooth);

  if (!csmd) {
    return OPERATOR_CANCELLED;
  }

  if (!BKE_modifier_is_enabled(scene, &csmd->modifier, eModifierMode_Realtime)) {
    BKE_report(op->reports, RPT_ERROR, "Modifier is disabled");
    return OPERATOR_CANCELLED;
  }

  const bool is_bind = (csmd->bind_coords != nullptr);

  implicit_sharing::free_shared_data(&csmd->bind_coords, &csmd->bind_coords_sharing_info);
  MEM_SAFE_FREE(csmd->delta_cache.deltas);

  if (is_bind) {
    /* toggle off */
    csmd->bind_coords_num = 0;
  }
  else {
    /* Signal to modifier to recalculate. */
    CorrectiveSmoothModifierData *csmd_eval = (CorrectiveSmoothModifierData *)
        BKE_modifier_get_evaluated(depsgraph, ob, &csmd->modifier);
    csmd_eval->bind_coords_num = uint(-1);

    /* Force modifier to run, it will call binding routine
     * (this has to happen outside of depsgraph evaluation). */
    object_force_modifier_bind_simple_options(depsgraph, ob, &csmd->modifier);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus correctivesmooth_bind_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return correctivesmooth_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_correctivesmooth_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Corrective Smooth Bind";
  ot->description = "Bind base pose in Corrective Smooth modifier";
  ot->idname = "OBJECT_OT_correctivesmooth_bind";

  /* API callbacks. */
  ot->poll = correctivesmooth_poll;
  ot->invoke = correctivesmooth_bind_invoke;
  ot->exec = correctivesmooth_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Mesh Deform Bind Operator
 * \{ */

static bool meshdeform_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_MeshDeformModifier, 0, true, false);
}

static wmOperatorStatus meshdeform_bind_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = context_active_object(C);
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_MeshDeform);

  if (mmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (mmd->bindcagecos != nullptr) {
    implicit_sharing::free_shared_data(&mmd->bindcagecos, &mmd->bindcagecos_sharing_info);
    implicit_sharing::free_shared_data(&mmd->dyngrid, &mmd->dyngrid_sharing_info);
    implicit_sharing::free_shared_data(&mmd->dyninfluences, &mmd->dyninfluences_sharing_info);
    implicit_sharing::free_shared_data(&mmd->bindinfluences, &mmd->bindinfluences_sharing_info);
    implicit_sharing::free_shared_data(&mmd->bindoffsets, &mmd->bindoffsets_sharing_info);
    implicit_sharing::free_shared_data(&mmd->dynverts, &mmd->dynverts_sharing_info);
    MEM_SAFE_FREE(mmd->bindweights); /* Deprecated */
    MEM_SAFE_FREE(mmd->bindcos);     /* Deprecated */
    mmd->verts_num = 0;
    mmd->cage_verts_num = 0;
    mmd->influences_num = 0;
  }
  else {
    /* Force modifier to run, it will call binding routine
     * (this has to happen outside of depsgraph evaluation). */
    MeshDeformModifierData *mmd_eval = (MeshDeformModifierData *)BKE_modifier_get_evaluated(
        depsgraph, ob, &mmd->modifier);
    mmd_eval->bindfunc = ED_mesh_deform_bind_callback;
    object_force_modifier_bind_simple_options(depsgraph, ob, &mmd->modifier);
    mmd_eval->bindfunc = nullptr;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus meshdeform_bind_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return meshdeform_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_meshdeform_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mesh Deform Bind";
  ot->description = "Bind mesh to cage in mesh deform modifier";
  ot->idname = "OBJECT_OT_meshdeform_bind";

  /* API callbacks. */
  ot->poll = meshdeform_poll;
  ot->invoke = meshdeform_bind_invoke;
  ot->exec = meshdeform_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Explode Refresh Operator
 * \{ */

static bool explode_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_ExplodeModifier, 0, true, false);
}

static wmOperatorStatus explode_refresh_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  ExplodeModifierData *emd = (ExplodeModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_Explode);

  if (!emd) {
    return OPERATOR_CANCELLED;
  }

  emd->flag |= eExplodeFlag_CalcFaces;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus explode_refresh_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return explode_refresh_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_explode_refresh(wmOperatorType *ot)
{
  ot->name = "Explode Refresh";
  ot->description = "Refresh data in the Explode modifier";
  ot->idname = "OBJECT_OT_explode_refresh";

  ot->poll = explode_poll;
  ot->invoke = explode_refresh_invoke;
  ot->exec = explode_refresh_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Ocean Bake Operator
 * \{ */

static bool ocean_bake_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_OceanModifier, 0, true, false);
}

struct OceanBakeJob {
  /* from wmJob */
  Object *owner;
  bool *stop, *do_update;
  float *progress;
  int current_frame;
  OceanCache *och;
  Ocean *ocean;
  OceanModifierData *omd;
};

static void oceanbake_free(void *customdata)
{
  OceanBakeJob *oj = static_cast<OceanBakeJob *>(customdata);
  MEM_delete(oj);
}

/* called by oceanbake, only to check job 'stop' value */
static int oceanbake_breakjob(void * /*customdata*/)
{
  // OceanBakeJob *ob = (OceanBakeJob *)customdata;
  // return *(ob->stop);

  /* this is not nice yet, need to make the jobs list template better
   * for identifying/acting upon various different jobs */
  /* but for now we'll reuse the render break... */
  return (G.is_break);
}

/* called by oceanbake, wmJob sends notifier */
static void oceanbake_update(void *customdata, float progress, int *cancel)
{
  OceanBakeJob *oj = static_cast<OceanBakeJob *>(customdata);

  if (oceanbake_breakjob(oj)) {
    *cancel = 1;
  }

  *(oj->do_update) = true;
  *(oj->progress) = progress;
}

static void oceanbake_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  OceanBakeJob *oj = static_cast<OceanBakeJob *>(customdata);

  oj->stop = &worker_status->stop;
  oj->do_update = &worker_status->do_update;
  oj->progress = &worker_status->progress;

  G.is_break = false; /* XXX shared with render - replace with job 'stop' switch */

  BKE_ocean_bake(oj->ocean, oj->och, oceanbake_update, (void *)oj);

  worker_status->do_update = true;
  worker_status->stop = false;
}

static void oceanbake_endjob(void *customdata)
{
  OceanBakeJob *oj = static_cast<OceanBakeJob *>(customdata);

  if (oj->ocean) {
    BKE_ocean_free(oj->ocean);
    oj->ocean = nullptr;
  }

  oj->omd->oceancache = oj->och;
  oj->omd->cached = true;

  Object *ob = oj->owner;
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
}

static wmOperatorStatus ocean_bake_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  OceanModifierData *omd = (OceanModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_Ocean);
  Scene *scene = CTX_data_scene(C);
  const bool free = RNA_boolean_get(op->ptr, "free");

  if (!omd) {
    return OPERATOR_CANCELLED;
  }

  if (free) {
    BKE_ocean_free_modifier_cache(omd);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
    return OPERATOR_FINISHED;
  }

  OceanCache *och = BKE_ocean_init_cache(omd->cachepath,
                                         BKE_modifier_path_relbase(bmain, ob),
                                         omd->bakestart,
                                         omd->bakeend,
                                         omd->wave_scale,
                                         omd->chop_amount,
                                         omd->foam_coverage,
                                         omd->foam_fade,
                                         omd->resolution);

  och->time = MEM_malloc_arrayN<float>(och->duration, "foam bake time");

  int cfra = scene->r.cfra;

  /* precalculate time variable before baking */
  int i = 0;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  for (int f = omd->bakestart; f <= omd->bakeend; f++) {
    /* For now only simple animation of time value is supported, nothing else.
     * No drivers or other modifier parameters. */
    /* TODO(sergey): This operates on an original data, so no flush is needed. However, baking
     * usually should happen on an evaluated objects, so this seems to be deeper issue here. */

    const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                      f);
    BKE_animsys_evaluate_animdata((ID *)ob, ob->adt, &anim_eval_context, ADT_RECALC_ANIM, false);

    och->time[i] = omd->time;
    i++;
  }

  /* Make a copy of ocean to use for baking - thread-safety. */
  Ocean *ocean = BKE_ocean_add();
  BKE_ocean_init_from_modifier(ocean, omd, omd->resolution);

#if 0
  BKE_ocean_bake(ocean, och);

  omd->oceancache = och;
  omd->cached = true;

  scene->r.cfra = cfra;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
#endif

  /* job stuff */

  scene->r.cfra = cfra;

  /* setup job */
  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Simulating ocean...",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_OBJECT_SIM_OCEAN);
  OceanBakeJob *oj = MEM_callocN<OceanBakeJob>("ocean bake job");
  oj->owner = ob;
  oj->ocean = ocean;
  oj->och = och;
  oj->omd = omd;

  WM_jobs_customdata_set(wm_job, oj, oceanbake_free);
  WM_jobs_timer(wm_job, 0.1, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
  WM_jobs_callbacks(wm_job, oceanbake_startjob, nullptr, nullptr, oceanbake_endjob);

  WM_jobs_start(CTX_wm_manager(C), wm_job);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus ocean_bake_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return ocean_bake_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_ocean_bake(wmOperatorType *ot)
{
  ot->name = "Bake Ocean";
  ot->description = "Bake an image sequence of ocean data";
  ot->idname = "OBJECT_OT_ocean_bake";

  ot->poll = ocean_bake_poll;
  ot->invoke = ocean_bake_invoke;
  ot->exec = ocean_bake_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  RNA_def_boolean(ot->srna, "free", false, "Free", "Free the bake, rather than generating it");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Laplacian-Deform Bind Operator
 * \{ */

static bool laplaciandeform_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_LaplacianDeformModifier, 0, false, false);
}

static wmOperatorStatus laplaciandeform_bind_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_LaplacianDeform);

  if (lmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (lmd->flag & MOD_LAPLACIANDEFORM_BIND) {
    lmd->flag &= ~MOD_LAPLACIANDEFORM_BIND;
  }
  else {
    lmd->flag |= MOD_LAPLACIANDEFORM_BIND;
  }

  LaplacianDeformModifierData *lmd_eval = (LaplacianDeformModifierData *)
      BKE_modifier_get_evaluated(depsgraph, ob, &lmd->modifier);
  lmd_eval->flag = lmd->flag;

  /* Force modifier to run, it will call binding routine
   * (this has to happen outside of depsgraph evaluation). */
  object_force_modifier_bind_simple_options(depsgraph, ob, &lmd->modifier);

  /* This is hard to know from the modifier itself whether the evaluation is
   * happening for binding or not. So we copy all the required data here. */
  lmd->verts_num = lmd_eval->verts_num;
  if (lmd_eval->vertexco == nullptr) {
    implicit_sharing::free_shared_data(&lmd->vertexco, &lmd->vertexco_sharing_info);
  }
  else {
    implicit_sharing::copy_shared_pointer(lmd_eval->vertexco,
                                          lmd_eval->vertexco_sharing_info,
                                          &lmd->vertexco,
                                          &lmd->vertexco_sharing_info);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus laplaciandeform_bind_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return laplaciandeform_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_laplaciandeform_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Laplacian Deform Bind";
  ot->description = "Bind mesh to system in laplacian deform modifier";
  ot->idname = "OBJECT_OT_laplaciandeform_bind";

  /* API callbacks. */
  ot->poll = laplaciandeform_poll;
  ot->invoke = laplaciandeform_bind_invoke;
  ot->exec = laplaciandeform_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Surface Deform Bind Operator
 * \{ */

static bool surfacedeform_bind_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_SurfaceDeformModifier, 0, true, false);
}

static wmOperatorStatus surfacedeform_bind_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)edit_modifier_property_get(
      op, ob, eModifierType_SurfaceDeform);

  if (smd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (smd->flags & MOD_SDEF_BIND) {
    smd->flags &= ~MOD_SDEF_BIND;
  }
  else if (smd->target) {
    smd->flags |= MOD_SDEF_BIND;
  }

  SurfaceDeformModifierData *smd_eval = (SurfaceDeformModifierData *)BKE_modifier_get_evaluated(
      depsgraph, ob, &smd->modifier);
  smd_eval->flags = smd->flags;

  /* Force modifier to run, it will call binding routine
   * (this has to happen outside of depsgraph evaluation). */
  object_force_modifier_bind_simple_options(depsgraph, ob, &smd->modifier);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus surfacedeform_bind_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return surfacedeform_bind_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_surfacedeform_bind(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Surface Deform Bind";
  ot->description = "Bind mesh to target in surface deform modifier";
  ot->idname = "OBJECT_OT_surfacedeform_bind";

  /* API callbacks. */
  ot->poll = surfacedeform_bind_poll;
  ot->invoke = surfacedeform_bind_invoke;
  ot->exec = surfacedeform_bind_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Toggle Value or Attribute Operator
 *
 * \note This operator basically only exists to provide a better tooltip for the toggle button,
 * since it is stored as an IDProperty. It also stops the button from being highlighted when
 * "use_attribute" is on, which isn't expected.
 * \{ */

static wmOperatorStatus geometry_nodes_input_attribute_toggle_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);

  char modifier_name[MAX_NAME];
  RNA_string_get(op->ptr, "modifier_name", modifier_name);
  NodesModifierData *nmd = (NodesModifierData *)BKE_modifiers_findby_name(ob, modifier_name);
  if (nmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char input_name[MAX_NAME];
  RNA_string_get(op->ptr, "input_name", input_name);

  IDProperty *use_attribute = IDP_GetPropertyFromGroup(
      nmd->settings.properties, std::string(input_name + std::string("_use_attribute")).c_str());
  if (!use_attribute) {
    return OPERATOR_CANCELLED;
  }

  if (use_attribute->type == IDP_INT) {
    IDP_int_set(use_attribute, !IDP_int_get(use_attribute));
  }
  else if (use_attribute->type == IDP_BOOLEAN) {
    IDP_bool_set(use_attribute, !IDP_bool_get(use_attribute));
  }
  else {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_geometry_nodes_input_attribute_toggle(wmOperatorType *ot)
{
  ot->name = "Input Attribute Toggle";
  ot->description =
      "Switch between an attribute and a single value to define the data for every element";
  ot->idname = "OBJECT_OT_geometry_nodes_input_attribute_toggle";

  ot->exec = geometry_nodes_input_attribute_toggle_exec;
  ot->poll = ED_operator_object_active_editable;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(ot->srna, "input_name", nullptr, 0, "Input Name", "");
  RNA_def_string(ot->srna, "modifier_name", nullptr, MAX_NAME, "Modifier Name", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Copy and Assign Geometry Node Group operator
 * \{ */

static wmOperatorStatus geometry_node_tree_copy_assign_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = context_active_object(C);
  ModifierData *md = BKE_object_active_modifier(ob);
  if (!(md && md->type == eModifierType_Nodes)) {
    return OPERATOR_CANCELLED;
  }

  NodesModifierData *nmd = (NodesModifierData *)md;
  bNodeTree *tree = nmd->node_group;
  if (tree == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree *new_tree = (bNodeTree *)BKE_id_copy_ex(
      bmain, &tree->id, nullptr, LIB_ID_COPY_ACTIONS | LIB_ID_COPY_DEFAULT);

  nmd->flag &= ~NODES_MODIFIER_HIDE_DATABLOCK_SELECTOR;

  if (new_tree == nullptr) {
    return OPERATOR_CANCELLED;
  }

  nmd->node_group = new_tree;
  id_us_min(&tree->id);

  BKE_main_ensure_invariants(*bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_geometry_node_tree_copy_assign(wmOperatorType *ot)
{
  ot->name = "New Geometry Node Group";
  ot->description =
      "Duplicate the active geometry node group and assign it to the active modifier";
  ot->idname = "OBJECT_OT_geometry_node_tree_copy_assign";

  ot->exec = geometry_node_tree_copy_assign_exec;
  ot->poll = ED_operator_object_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Dash Modifier
 * \{ */

static bool dash_modifier_segment_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_GreasePencilDashModifierData, 0, false, false);
}

static wmOperatorStatus dash_modifier_segment_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_GreasePencilDash));

  if (dmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  GreasePencilDashModifierSegment *new_segments =
      MEM_malloc_arrayN<GreasePencilDashModifierSegment>(dmd->segments_num + 1, __func__);

  const int new_active_index = std::clamp(dmd->segment_active_index + 1, 0, dmd->segments_num);
  if (dmd->segments_num != 0) {
    /* Copy the segments before the new segment. */
    memcpy(new_segments,
           dmd->segments_array,
           sizeof(GreasePencilDashModifierSegment) * new_active_index);
    /* Copy the segments after the new segment. */
    memcpy(new_segments + new_active_index + 1,
           dmd->segments_array + new_active_index,
           sizeof(GreasePencilDashModifierSegment) * (dmd->segments_num - new_active_index));
  }

  /* Create the new segment. */
  GreasePencilDashModifierSegment *ds = &new_segments[new_active_index];
  memcpy(ds,
         DNA_struct_default_get(GreasePencilDashModifierSegment),
         sizeof(GreasePencilDashModifierSegment));
  BLI_uniquename_cb(
      [&](const StringRef name) {
        for (const GreasePencilDashModifierSegment &ds : dmd->segments()) {
          if (STREQ(ds.name, name.data())) {
            return true;
          }
        }
        return false;
      },
      '.',
      ds->name);

  MEM_SAFE_FREE(dmd->segments_array);
  dmd->segments_array = new_segments;
  dmd->segments_num++;
  dmd->segment_active_index = new_active_index;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus dash_modifier_segment_add_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return dash_modifier_segment_add_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_grease_pencil_dash_modifier_segment_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Segment";
  ot->description = "Add a segment to the dash modifier";
  ot->idname = "OBJECT_OT_grease_pencil_dash_modifier_segment_add";

  /* API callbacks. */
  ot->poll = dash_modifier_segment_poll;
  ot->invoke = dash_modifier_segment_add_invoke;
  ot->exec = dash_modifier_segment_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

static void dash_modifier_segment_free(GreasePencilDashModifierSegment * /*ds*/) {}

static wmOperatorStatus dash_modifier_segment_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_GreasePencilDash));

  if (dmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (!dmd->segments().index_range().contains(dmd->segment_active_index)) {
    return OPERATOR_CANCELLED;
  }

  dna::array::remove_index(&dmd->segments_array,
                           &dmd->segments_num,
                           &dmd->segment_active_index,
                           dmd->segment_active_index,
                           dash_modifier_segment_free);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus dash_modifier_segment_remove_invoke(bContext *C,
                                                            wmOperator *op,
                                                            const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return dash_modifier_segment_remove_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_grease_pencil_dash_modifier_segment_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Dash Segment";
  ot->description = "Remove the active segment from the dash modifier";
  ot->idname = "OBJECT_OT_grease_pencil_dash_modifier_segment_remove";

  /* API callbacks. */
  ot->poll = dash_modifier_segment_poll;
  ot->invoke = dash_modifier_segment_remove_invoke;
  ot->exec = dash_modifier_segment_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "Index of the segment to remove", 0, INT_MAX);
}

enum class DashSegmentMoveDirection {
  Up = -1,
  Down = 1,
};

static wmOperatorStatus dash_modifier_segment_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  auto *dmd = reinterpret_cast<GreasePencilDashModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_GreasePencilDash));

  if (dmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (dmd->segments_num < 2) {
    return OPERATOR_CANCELLED;
  }

  const DashSegmentMoveDirection direction = DashSegmentMoveDirection(
      RNA_enum_get(op->ptr, "type"));
  switch (direction) {
    case DashSegmentMoveDirection::Up:
      if (dmd->segment_active_index == 0) {
        return OPERATOR_CANCELLED;
      }

      std::swap(dmd->segments_array[dmd->segment_active_index],
                dmd->segments_array[dmd->segment_active_index - 1]);

      dmd->segment_active_index--;
      break;
    case DashSegmentMoveDirection::Down:
      if (dmd->segment_active_index == dmd->segments_num - 1) {
        return OPERATOR_CANCELLED;
      }

      std::swap(dmd->segments_array[dmd->segment_active_index],
                dmd->segments_array[dmd->segment_active_index + 1]);

      dmd->segment_active_index++;
      break;
    default:
      return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus dash_modifier_segment_move_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return dash_modifier_segment_move_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_grease_pencil_dash_modifier_segment_move(wmOperatorType *ot)
{
  static const EnumPropertyItem segment_move[] = {
      {int(DashSegmentMoveDirection::Up), "UP", 0, "Up", ""},
      {int(DashSegmentMoveDirection::Down), "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Dash Segment";
  ot->description = "Move the active dash segment up or down";
  ot->idname = "OBJECT_OT_grease_pencil_dash_modifier_segment_move";

  /* API callbacks. */
  ot->poll = dash_modifier_segment_poll;
  ot->invoke = dash_modifier_segment_move_invoke;
  ot->exec = dash_modifier_segment_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  ot->prop = RNA_def_enum(ot->srna, "type", segment_move, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Time Modifier
 * \{ */

static bool time_modifier_segment_poll(bContext *C)
{
  return edit_modifier_poll_generic(C, &RNA_GreasePencilTimeModifier, 0, false, false);
}

static wmOperatorStatus time_modifier_segment_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_GreasePencilTime));

  if (tmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  GreasePencilTimeModifierSegment *new_segments =
      MEM_malloc_arrayN<GreasePencilTimeModifierSegment>(tmd->segments_num + 1, __func__);

  const int new_active_index = std::clamp(tmd->segment_active_index + 1, 0, tmd->segments_num);
  if (tmd->segments_num != 0) {
    /* Copy the segments before the new segment. */
    memcpy(new_segments,
           tmd->segments_array,
           sizeof(GreasePencilTimeModifierSegment) * new_active_index);
    /* Copy the segments after the new segment. */
    memcpy(new_segments + new_active_index + 1,
           tmd->segments_array + new_active_index,
           sizeof(GreasePencilTimeModifierSegment) * (tmd->segments_num - new_active_index));
  }

  /* Create the new segment. */
  GreasePencilTimeModifierSegment *segment = &new_segments[new_active_index];
  memcpy(segment,
         DNA_struct_default_get(GreasePencilTimeModifierSegment),
         sizeof(GreasePencilTimeModifierSegment));
  BLI_uniquename_cb(
      [&](const StringRef name) {
        for (const GreasePencilTimeModifierSegment &segment : tmd->segments()) {
          if (STREQ(segment.name, name.data())) {
            return true;
          }
        }
        return false;
      },
      '.',
      segment->name);

  MEM_SAFE_FREE(tmd->segments_array);
  tmd->segments_array = new_segments;
  tmd->segments_num++;
  tmd->segment_active_index++;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus time_modifier_segment_add_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return time_modifier_segment_add_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_grease_pencil_time_modifier_segment_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Segment";
  ot->description = "Add a segment to the time modifier";
  ot->idname = "OBJECT_OT_grease_pencil_time_modifier_segment_add";

  /* API callbacks. */
  ot->poll = time_modifier_segment_poll;
  ot->invoke = time_modifier_segment_add_invoke;
  ot->exec = time_modifier_segment_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);
}

static void time_modifier_segment_free(GreasePencilTimeModifierSegment * /*ds*/) {}

static wmOperatorStatus time_modifier_segment_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_GreasePencilTime));

  if (tmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (!tmd->segments().index_range().contains(tmd->segment_active_index)) {
    return OPERATOR_CANCELLED;
  }

  dna::array::remove_index(&tmd->segments_array,
                           &tmd->segments_num,
                           &tmd->segment_active_index,
                           tmd->segment_active_index,
                           time_modifier_segment_free);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus time_modifier_segment_remove_invoke(bContext *C,
                                                            wmOperator *op,
                                                            const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return time_modifier_segment_remove_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_grease_pencil_time_modifier_segment_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Segment";
  ot->description = "Remove the active segment from the time modifier";
  ot->idname = "OBJECT_OT_grease_pencil_time_modifier_segment_remove";

  /* API callbacks. */
  ot->poll = time_modifier_segment_poll;
  ot->invoke = time_modifier_segment_remove_invoke;
  ot->exec = time_modifier_segment_remove_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "Index of the segment to remove", 0, INT_MAX);
}

enum class TimeSegmentMoveDirection {
  Up = -1,
  Down = 1,
};

static wmOperatorStatus time_modifier_segment_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(
      edit_modifier_property_get(op, ob, eModifierType_GreasePencilTime));

  if (tmd == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (tmd->segments_num < 2) {
    return OPERATOR_CANCELLED;
  }

  const TimeSegmentMoveDirection direction = TimeSegmentMoveDirection(
      RNA_enum_get(op->ptr, "type"));
  switch (direction) {
    case TimeSegmentMoveDirection::Up:
      if (tmd->segment_active_index == 0) {
        return OPERATOR_CANCELLED;
      }

      std::swap(tmd->segments_array[tmd->segment_active_index],
                tmd->segments_array[tmd->segment_active_index - 1]);

      tmd->segment_active_index--;
      break;
    case TimeSegmentMoveDirection::Down:
      if (tmd->segment_active_index == tmd->segments_num - 1) {
        return OPERATOR_CANCELLED;
      }

      std::swap(tmd->segments_array[tmd->segment_active_index],
                tmd->segments_array[tmd->segment_active_index + 1]);

      tmd->segment_active_index++;
      break;
    default:
      return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus time_modifier_segment_move_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent * /*event*/)
{
  if (edit_modifier_invoke_properties(C, op)) {
    return time_modifier_segment_move_exec(C, op);
  }
  return OPERATOR_CANCELLED;
}

void OBJECT_OT_grease_pencil_time_modifier_segment_move(wmOperatorType *ot)
{
  static const EnumPropertyItem segment_move[] = {
      {int(TimeSegmentMoveDirection::Up), "UP", 0, "Up", ""},
      {int(TimeSegmentMoveDirection::Down), "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Segment";
  ot->description = "Move the active time segment up or down";
  ot->idname = "OBJECT_OT_grease_pencil_time_modifier_segment_move";

  /* API callbacks. */
  ot->poll = time_modifier_segment_poll;
  ot->invoke = time_modifier_segment_move_invoke;
  ot->exec = time_modifier_segment_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
  edit_modifier_properties(ot);

  ot->prop = RNA_def_enum(ot->srna, "type", segment_move, 0, "Type", "");
}

/** \} */

}  // namespace blender::ed::object
