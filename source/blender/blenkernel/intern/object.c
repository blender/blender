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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_defaults.h"
#include "DNA_effect_types.h"
#include "DNA_fluid_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_kdtree.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_DerivedMesh.h"
#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_anim_path.h"
#include "BKE_anim_visualization.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_hair.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_light.h"
#include "BKE_lightprobe.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_object_facemap.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_shader_fx.h"
#include "BKE_softbody.h"
#include "BKE_speaker.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"
#include "BKE_volume.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"

#include "BLO_read_write.h"

#include "SEQ_sequencer.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "CCGSubSurf.h"
#include "atomic_ops.h"

static CLG_LogRef LOG = {"bke.object"};

/**
 * Vertex parent modifies original BMesh which is not safe for threading.
 * Ideally such a modification should be handled as a separate DAG update
 * callback for mesh datablock, but for until it is actually supported use
 * simpler solution with a mutex lock.
 *                                               - sergey -
 */
#define VPARENT_THREADING_HACK

#ifdef VPARENT_THREADING_HACK
static ThreadMutex vparent_lock = BLI_MUTEX_INITIALIZER;
#endif

static void copy_object_pose(Object *obn, const Object *ob, const int flag);

static void object_init_data(ID *id)
{
  Object *ob = (Object *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(ob, id));

  MEMCPY_STRUCT_AFTER(ob, DNA_struct_default_get(Object), id);

  ob->type = OB_EMPTY;

  ob->trackflag = OB_POSY;
  ob->upflag = OB_POSZ;

  /* Animation Visualization defaults */
  animviz_settings_init(&ob->avs);
}

static void object_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Object *ob_dst = (Object *)id_dst;
  const Object *ob_src = (const Object *)id_src;

  /* Do not copy runtime data. */
  BKE_object_runtime_reset_on_copy(ob_dst, flag);

  /* We never handle usercount here for own data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  if (ob_src->totcol) {
    ob_dst->mat = MEM_dupallocN(ob_src->mat);
    ob_dst->matbits = MEM_dupallocN(ob_src->matbits);
    ob_dst->totcol = ob_src->totcol;
  }
  else if (ob_dst->mat != NULL || ob_dst->matbits != NULL) {
    /* This shall not be needed, but better be safe than sorry. */
    BLI_assert(!"Object copy: non-NULL material pointers with zero counter, should not happen.");
    ob_dst->mat = NULL;
    ob_dst->matbits = NULL;
  }

  if (ob_src->iuser) {
    ob_dst->iuser = MEM_dupallocN(ob_src->iuser);
  }

  if (ob_src->runtime.bb) {
    ob_dst->runtime.bb = MEM_dupallocN(ob_src->runtime.bb);
  }

  BLI_listbase_clear(&ob_dst->modifiers);

  LISTBASE_FOREACH (ModifierData *, md, &ob_src->modifiers) {
    ModifierData *nmd = BKE_modifier_new(md->type);
    BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
    BKE_modifier_copydata_ex(md, nmd, flag_subdata);
    BLI_addtail(&ob_dst->modifiers, nmd);
  }

  BLI_listbase_clear(&ob_dst->greasepencil_modifiers);

  LISTBASE_FOREACH (GpencilModifierData *, gmd, &ob_src->greasepencil_modifiers) {
    GpencilModifierData *nmd = BKE_gpencil_modifier_new(gmd->type);
    BLI_strncpy(nmd->name, gmd->name, sizeof(nmd->name));
    BKE_gpencil_modifier_copydata_ex(gmd, nmd, flag_subdata);
    BLI_addtail(&ob_dst->greasepencil_modifiers, nmd);
  }

  BLI_listbase_clear(&ob_dst->shader_fx);

  LISTBASE_FOREACH (ShaderFxData *, fx, &ob_src->shader_fx) {
    ShaderFxData *nfx = BKE_shaderfx_new(fx->type);
    BLI_strncpy(nfx->name, fx->name, sizeof(nfx->name));
    BKE_shaderfx_copydata_ex(fx, nfx, flag_subdata);
    BLI_addtail(&ob_dst->shader_fx, nfx);
  }

  if (ob_src->pose) {
    copy_object_pose(ob_dst, ob_src, flag_subdata);
    /* backwards compat... non-armatures can get poses in older files? */
    if (ob_src->type == OB_ARMATURE) {
      const bool do_pose_id_user = (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0;
      BKE_pose_rebuild(bmain, ob_dst, ob_dst->data, do_pose_id_user);
    }
  }
  BKE_defgroup_copy_list(&ob_dst->defbase, &ob_src->defbase);
  BKE_object_facemap_copy_list(&ob_dst->fmaps, &ob_src->fmaps);
  BKE_constraints_copy_ex(&ob_dst->constraints, &ob_src->constraints, flag_subdata, true);

  ob_dst->mode = ob_dst->type != OB_GPENCIL ? OB_MODE_OBJECT : ob_dst->mode;
  ob_dst->sculpt = NULL;

  if (ob_src->pd) {
    ob_dst->pd = MEM_dupallocN(ob_src->pd);
    if (ob_dst->pd->rng) {
      ob_dst->pd->rng = MEM_dupallocN(ob_src->pd->rng);
    }
  }
  BKE_object_copy_softbody(ob_dst, ob_src, flag_subdata);
  BKE_rigidbody_object_copy(bmain, ob_dst, ob_src, flag_subdata);

  BKE_object_copy_particlesystems(ob_dst, ob_src, flag_subdata);

  BLI_listbase_clear((ListBase *)&ob_dst->drawdata);
  BLI_listbase_clear(&ob_dst->pc_ids);

  ob_dst->avs = ob_src->avs;
  ob_dst->mpath = animviz_copy_motionpath(ob_src->mpath);

  /* Do not copy object's preview
   * (mostly due to the fact renderers create temp copy of objects). */
  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0 && false) { /* XXX TODO temp hack */
    BKE_previewimg_id_copy(&ob_dst->id, &ob_src->id);
  }
  else {
    ob_dst->preview = NULL;
  }
}

static void object_free_data(ID *id)
{
  Object *ob = (Object *)id;

  DRW_drawdata_free((ID *)ob);

  /* BKE_<id>_free shall never touch to ID->us. Never ever. */
  BKE_object_free_modifiers(ob, LIB_ID_CREATE_NO_USER_REFCOUNT);
  BKE_object_free_shaderfx(ob, LIB_ID_CREATE_NO_USER_REFCOUNT);

  MEM_SAFE_FREE(ob->mat);
  MEM_SAFE_FREE(ob->matbits);
  MEM_SAFE_FREE(ob->iuser);
  MEM_SAFE_FREE(ob->runtime.bb);

  BLI_freelistN(&ob->defbase);
  BLI_freelistN(&ob->fmaps);
  if (ob->pose) {
    BKE_pose_free_ex(ob->pose, false);
    ob->pose = NULL;
  }
  if (ob->mpath) {
    animviz_free_motionpath(ob->mpath);
    ob->mpath = NULL;
  }

  BKE_constraints_free_ex(&ob->constraints, false);

  BKE_partdeflect_free(ob->pd);
  BKE_rigidbody_free_object(ob, NULL);
  BKE_rigidbody_free_constraint(ob);

  sbFree(ob);

  BKE_sculptsession_free(ob);

  BLI_freelistN(&ob->pc_ids);

  /* Free runtime curves data. */
  if (ob->runtime.curve_cache) {
    BKE_curve_bevelList_free(&ob->runtime.curve_cache->bev);
    if (ob->runtime.curve_cache->path) {
      free_path(ob->runtime.curve_cache->path);
    }
    MEM_freeN(ob->runtime.curve_cache);
    ob->runtime.curve_cache = NULL;
  }

  BKE_previewimg_free(&ob->preview);
}

static void object_make_local(Main *bmain, ID *id, const int flags)
{
  Object *ob = (Object *)id;
  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;
  const bool clear_proxy = (flags & LIB_ID_MAKELOCAL_OBJECT_NO_PROXY_CLEARING) == 0;
  bool is_local = false, is_lib = false;

  /* - only lib users: do nothing (unless force_local is set)
   * - only local users: set flag
   * - mixed: make copy
   * In case we make a whole lib's content local,
   * we always want to localize, and we skip remapping (done later).
   */

  if (!ID_IS_LINKED(ob)) {
    return;
  }

  BKE_library_ID_test_usages(bmain, ob, &is_local, &is_lib);

  if (lib_local || is_local) {
    if (!is_lib) {
      BKE_lib_id_clear_library_data(bmain, &ob->id);
      BKE_lib_id_expand_local(bmain, &ob->id);
      if (clear_proxy) {
        if (ob->proxy_from != NULL) {
          ob->proxy_from->proxy = NULL;
          ob->proxy_from->proxy_group = NULL;
        }
        ob->proxy = ob->proxy_from = ob->proxy_group = NULL;
      }
    }
    else {
      Object *ob_new = (Object *)BKE_id_copy(bmain, &ob->id);
      id_us_min(&ob_new->id);

      ob_new->proxy = ob_new->proxy_from = ob_new->proxy_group = NULL;

      /* setting newid is mandatory for complex make_lib_local logic... */
      ID_NEW_SET(ob, ob_new);

      if (!lib_local) {
        BKE_libblock_remap(bmain, ob, ob_new, ID_REMAP_SKIP_INDIRECT_USAGE);
      }
    }
  }
}

static void library_foreach_modifiersForeachIDLink(void *user_data,
                                                   Object *UNUSED(object),
                                                   ID **id_pointer,
                                                   int cb_flag)
{
  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  BKE_lib_query_foreachid_process(data, id_pointer, cb_flag);
}

static void library_foreach_gpencil_modifiersForeachIDLink(void *user_data,
                                                           Object *UNUSED(object),
                                                           ID **id_pointer,
                                                           int cb_flag)
{
  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  BKE_lib_query_foreachid_process(data, id_pointer, cb_flag);
}

static void library_foreach_shaderfxForeachIDLink(void *user_data,
                                                  Object *UNUSED(object),
                                                  ID **id_pointer,
                                                  int cb_flag)
{
  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  BKE_lib_query_foreachid_process(data, id_pointer, cb_flag);
}

static void library_foreach_constraintObjectLooper(bConstraint *UNUSED(con),
                                                   ID **id_pointer,
                                                   bool is_reference,
                                                   void *user_data)
{
  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  const int cb_flag = is_reference ? IDWALK_CB_USER : IDWALK_CB_NOP;
  BKE_lib_query_foreachid_process(data, id_pointer, cb_flag);
}

static void library_foreach_particlesystemsObjectLooper(ParticleSystem *UNUSED(psys),
                                                        ID **id_pointer,
                                                        void *user_data,
                                                        int cb_flag)
{
  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  BKE_lib_query_foreachid_process(data, id_pointer, cb_flag);
}

static void object_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Object *object = (Object *)id;

  /* Object is special, proxies make things hard... */
  const int proxy_cb_flag = ((BKE_lib_query_foreachid_process_flags_get(data) &
                              IDWALK_NO_INDIRECT_PROXY_DATA_USAGE) == 0 &&
                             (object->proxy || object->proxy_group)) ?
                                IDWALK_CB_INDIRECT_USAGE :
                                0;

  /* object data special case */
  if (object->type == OB_EMPTY) {
    /* empty can have NULL or Image */
    BKE_LIB_FOREACHID_PROCESS_ID(data, object->data, proxy_cb_flag | IDWALK_CB_USER);
  }
  else {
    /* when set, this can't be NULL */
    if (object->data) {
      BKE_LIB_FOREACHID_PROCESS_ID(
          data, object->data, proxy_cb_flag | IDWALK_CB_USER | IDWALK_CB_NEVER_NULL);
    }
  }

  BKE_LIB_FOREACHID_PROCESS(data, object->parent, IDWALK_CB_NEVER_SELF);
  BKE_LIB_FOREACHID_PROCESS(data, object->track, IDWALK_CB_NEVER_SELF);
  /* object->proxy is refcounted, but not object->proxy_group... *sigh* */
  BKE_LIB_FOREACHID_PROCESS(data, object->proxy, IDWALK_CB_USER | IDWALK_CB_NEVER_SELF);
  BKE_LIB_FOREACHID_PROCESS(data, object->proxy_group, IDWALK_CB_NOP);

  /* Special case!
   * Since this field is set/owned by 'user' of this ID (and not ID itself),
   * it is only indirect usage if proxy object is linked... Twisted. */
  {
    const int cb_flag_orig = BKE_lib_query_foreachid_process_callback_flag_override(
        data,
        (object->proxy_from != NULL && ID_IS_LINKED(object->proxy_from)) ?
            IDWALK_CB_INDIRECT_USAGE :
            0,
        true);
    BKE_LIB_FOREACHID_PROCESS(data, object->proxy_from, IDWALK_CB_LOOPBACK | IDWALK_CB_NEVER_SELF);
    BKE_lib_query_foreachid_process_callback_flag_override(data, cb_flag_orig, true);
  }

  BKE_LIB_FOREACHID_PROCESS(data, object->poselib, IDWALK_CB_USER);

  for (int i = 0; i < object->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS(data, object->mat[i], proxy_cb_flag | IDWALK_CB_USER);
  }

  /* Note that ob->gpd is deprecated, so no need to handle it here. */
  BKE_LIB_FOREACHID_PROCESS(data, object->instance_collection, IDWALK_CB_USER);

  if (object->pd) {
    BKE_LIB_FOREACHID_PROCESS(data, object->pd->tex, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS(data, object->pd->f_source, IDWALK_CB_NOP);
  }
  /* Note that ob->effect is deprecated, so no need to handle it here. */

  if (object->pose) {
    const int cb_flag_orig = BKE_lib_query_foreachid_process_callback_flag_override(
        data, proxy_cb_flag, false);
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      IDP_foreach_property(
          pchan->prop, IDP_TYPE_FILTER_ID, BKE_lib_query_idpropertiesForeachIDLink_callback, data);
      BKE_LIB_FOREACHID_PROCESS(data, pchan->custom, IDWALK_CB_USER);
      BKE_constraints_id_loop(&pchan->constraints, library_foreach_constraintObjectLooper, data);
    }
    BKE_lib_query_foreachid_process_callback_flag_override(data, cb_flag_orig, true);
  }

  if (object->rigidbody_constraint) {
    BKE_LIB_FOREACHID_PROCESS(data, object->rigidbody_constraint->ob1, IDWALK_CB_NEVER_SELF);
    BKE_LIB_FOREACHID_PROCESS(data, object->rigidbody_constraint->ob2, IDWALK_CB_NEVER_SELF);
  }

  BKE_modifiers_foreach_ID_link(object, library_foreach_modifiersForeachIDLink, data);
  BKE_gpencil_modifiers_foreach_ID_link(
      object, library_foreach_gpencil_modifiersForeachIDLink, data);
  BKE_constraints_id_loop(&object->constraints, library_foreach_constraintObjectLooper, data);
  BKE_shaderfx_foreach_ID_link(object, library_foreach_shaderfxForeachIDLink, data);

  LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
    BKE_particlesystem_id_loop(psys, library_foreach_particlesystemsObjectLooper, data);
  }

  if (object->soft) {
    BKE_LIB_FOREACHID_PROCESS(data, object->soft->collision_group, IDWALK_CB_NOP);

    if (object->soft->effector_weights) {
      BKE_LIB_FOREACHID_PROCESS(data, object->soft->effector_weights->group, IDWALK_CB_NOP);
    }
  }
}

static void write_defgroups(BlendWriter *writer, ListBase *defbase)
{
  LISTBASE_FOREACH (bDeformGroup *, defgroup, defbase) {
    BLO_write_struct(writer, bDeformGroup, defgroup);
  }
}

static void write_fmaps(BlendWriter *writer, ListBase *fbase)
{
  LISTBASE_FOREACH (bFaceMap *, fmap, fbase) {
    BLO_write_struct(writer, bFaceMap, fmap);
  }
}

static void object_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Object *ob = (Object *)id;

  const bool is_undo = BLO_write_is_undo(writer);
  if (ob->id.us > 0 || is_undo) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    BKE_object_runtime_reset(ob);

    if (is_undo) {
      /* For undo we stay in object mode during undo presses, so keep edit-mode disabled on save as
       * well, can help reducing false detection of changed data-blocks. */
      ob->mode &= ~OB_MODE_EDIT;
    }

    /* write LibData */
    BLO_write_id_struct(writer, Object, id_address, &ob->id);
    BKE_id_blend_write(writer, &ob->id);

    if (ob->adt) {
      BKE_animdata_blend_write(writer, ob->adt);
    }

    /* direct data */
    BLO_write_pointer_array(writer, ob->totcol, ob->mat);
    BLO_write_raw(writer, sizeof(char) * ob->totcol, ob->matbits);

    bArmature *arm = NULL;
    if (ob->type == OB_ARMATURE) {
      arm = ob->data;
      if (arm && ob->pose && arm->act_bone) {
        BLI_strncpy(
            ob->pose->proxy_act_bone, arm->act_bone->name, sizeof(ob->pose->proxy_act_bone));
      }
    }

    BKE_pose_blend_write(writer, ob->pose, arm);
    write_defgroups(writer, &ob->defbase);
    write_fmaps(writer, &ob->fmaps);
    BKE_constraint_blend_write(writer, &ob->constraints);
    animviz_motionpath_blend_write(writer, ob->mpath);

    BLO_write_struct(writer, PartDeflect, ob->pd);
    if (ob->soft) {
      /* Set deprecated pointers to prevent crashes of older Blenders */
      ob->soft->pointcache = ob->soft->shared->pointcache;
      ob->soft->ptcaches = ob->soft->shared->ptcaches;
      BLO_write_struct(writer, SoftBody, ob->soft);
      BLO_write_struct(writer, SoftBody_Shared, ob->soft->shared);
      BKE_ptcache_blend_write(writer, &(ob->soft->shared->ptcaches));
      BLO_write_struct(writer, EffectorWeights, ob->soft->effector_weights);
    }

    if (ob->rigidbody_object) {
      /* TODO: if any extra data is added to handle duplis, will need separate function then */
      BLO_write_struct(writer, RigidBodyOb, ob->rigidbody_object);
    }
    if (ob->rigidbody_constraint) {
      BLO_write_struct(writer, RigidBodyCon, ob->rigidbody_constraint);
    }

    if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
      BLO_write_struct(writer, ImageUser, ob->iuser);
    }

    BKE_particle_system_blend_write(writer, &ob->particlesystem);
    BKE_modifier_blend_write(writer, &ob->modifiers);
    BKE_gpencil_modifier_blend_write(writer, &ob->greasepencil_modifiers);
    BKE_shaderfx_blend_write(writer, &ob->shader_fx);

    BLO_write_struct_list(writer, LinkData, &ob->pc_ids);

    BKE_previewimg_blend_write(writer, ob->preview);
  }
}

/* XXX deprecated - old animation system */
static void direct_link_nlastrips(BlendDataReader *reader, ListBase *strips)
{
  BLO_read_list(reader, strips);

  LISTBASE_FOREACH (bActionStrip *, strip, strips) {
    BLO_read_list(reader, &strip->modifiers);
  }
}

static void object_blend_read_data(BlendDataReader *reader, ID *id)
{
  Object *ob = (Object *)id;

  PartEff *paf;

  /* XXX This should not be needed - but seems like it can happen in some cases,
   * so for now play safe. */
  ob->proxy_from = NULL;

  const bool is_undo = BLO_read_data_is_undo(reader);
  if (ob->id.tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT)) {
    /* Do not allow any non-object mode for linked data.
     * See T34776, T42780, T81027 for more information. */
    ob->mode &= ~OB_MODE_ALL_MODE_DATA;
  }
  else if (is_undo) {
    /* For undo we want to stay in object mode during undo presses, so keep some edit modes
     * disabled.
     * TODO: Check if we should not disable more edit modes here? */
    ob->mode &= ~(OB_MODE_EDIT | OB_MODE_PARTICLE_EDIT);
  }

  BLO_read_data_address(reader, &ob->adt);
  BKE_animdata_blend_read_data(reader, ob->adt);

  BLO_read_data_address(reader, &ob->pose);
  BKE_pose_blend_read_data(reader, ob->pose);

  BLO_read_data_address(reader, &ob->mpath);
  if (ob->mpath) {
    animviz_motionpath_blend_read_data(reader, ob->mpath);
  }

  BLO_read_list(reader, &ob->defbase);
  BLO_read_list(reader, &ob->fmaps);
  /* XXX deprecated - old animation system <<< */
  direct_link_nlastrips(reader, &ob->nlastrips);
  BLO_read_list(reader, &ob->constraintChannels);
  /* >>> XXX deprecated - old animation system */

  BLO_read_pointer_array(reader, (void **)&ob->mat);
  BLO_read_data_address(reader, &ob->matbits);

  /* do it here, below old data gets converted */
  BKE_modifier_blend_read_data(reader, &ob->modifiers, ob);
  BKE_gpencil_modifier_blend_read_data(reader, &ob->greasepencil_modifiers);
  BKE_shaderfx_blend_read_data(reader, &ob->shader_fx);

  BLO_read_list(reader, &ob->effect);
  paf = ob->effect.first;
  while (paf) {
    if (paf->type == EFF_PARTICLE) {
      paf->keys = NULL;
    }
    if (paf->type == EFF_WAVE) {
      WaveEff *wav = (WaveEff *)paf;
      PartEff *next = paf->next;
      WaveModifierData *wmd = (WaveModifierData *)BKE_modifier_new(eModifierType_Wave);

      wmd->damp = wav->damp;
      wmd->flag = wav->flag;
      wmd->height = wav->height;
      wmd->lifetime = wav->lifetime;
      wmd->narrow = wav->narrow;
      wmd->speed = wav->speed;
      wmd->startx = wav->startx;
      wmd->starty = wav->startx;
      wmd->timeoffs = wav->timeoffs;
      wmd->width = wav->width;

      BLI_addtail(&ob->modifiers, wmd);

      BLI_remlink(&ob->effect, paf);
      MEM_freeN(paf);

      paf = next;
      continue;
    }
    if (paf->type == EFF_BUILD) {
      BuildEff *baf = (BuildEff *)paf;
      PartEff *next = paf->next;
      BuildModifierData *bmd = (BuildModifierData *)BKE_modifier_new(eModifierType_Build);

      bmd->start = baf->sfra;
      bmd->length = baf->len;
      bmd->randomize = 0;
      bmd->seed = 1;

      BLI_addtail(&ob->modifiers, bmd);

      BLI_remlink(&ob->effect, paf);
      MEM_freeN(paf);

      paf = next;
      continue;
    }
    paf = paf->next;
  }

  BLO_read_data_address(reader, &ob->pd);
  BKE_particle_partdeflect_blend_read_data(reader, ob->pd);
  BLO_read_data_address(reader, &ob->soft);
  if (ob->soft) {
    SoftBody *sb = ob->soft;

    sb->bpoint = NULL; /* init pointers so it gets rebuilt nicely */
    sb->bspring = NULL;
    sb->scratch = NULL;
    /* although not used anymore */
    /* still have to be loaded to be compatible with old files */
    BLO_read_pointer_array(reader, (void **)&sb->keys);
    if (sb->keys) {
      for (int a = 0; a < sb->totkey; a++) {
        BLO_read_data_address(reader, &sb->keys[a]);
      }
    }

    BLO_read_data_address(reader, &sb->effector_weights);
    if (!sb->effector_weights) {
      sb->effector_weights = BKE_effector_add_weights(NULL);
    }

    BLO_read_data_address(reader, &sb->shared);
    if (sb->shared == NULL) {
      /* Link deprecated caches if they exist, so we can use them for versioning.
       * We should only do this when sb->shared == NULL, because those pointers
       * are always set (for compatibility with older Blenders). We mustn't link
       * the same pointcache twice. */
      BKE_ptcache_blend_read_data(reader, &sb->ptcaches, &sb->pointcache, false);
    }
    else {
      /* link caches */
      BKE_ptcache_blend_read_data(reader, &sb->shared->ptcaches, &sb->shared->pointcache, false);
    }
  }
  BLO_read_data_address(reader, &ob->fluidsimSettings); /* NT */

  BLO_read_data_address(reader, &ob->rigidbody_object);
  if (ob->rigidbody_object) {
    RigidBodyOb *rbo = ob->rigidbody_object;
    /* Allocate runtime-only struct */
    rbo->shared = MEM_callocN(sizeof(*rbo->shared), "RigidBodyObShared");
  }
  BLO_read_data_address(reader, &ob->rigidbody_constraint);
  if (ob->rigidbody_constraint) {
    ob->rigidbody_constraint->physics_constraint = NULL;
  }

  BLO_read_list(reader, &ob->particlesystem);
  BKE_particle_system_blend_read_data(reader, &ob->particlesystem);

  BKE_constraint_blend_read_data(reader, &ob->constraints);

  BLO_read_list(reader, &ob->hooks);
  while (ob->hooks.first) {
    ObHook *hook = ob->hooks.first;
    HookModifierData *hmd = (HookModifierData *)BKE_modifier_new(eModifierType_Hook);

    BLO_read_int32_array(reader, hook->totindex, &hook->indexar);

    /* Do conversion here because if we have loaded
     * a hook we need to make sure it gets converted
     * and freed, regardless of version.
     */
    copy_v3_v3(hmd->cent, hook->cent);
    hmd->falloff = hook->falloff;
    hmd->force = hook->force;
    hmd->indexar = hook->indexar;
    hmd->object = hook->parent;
    memcpy(hmd->parentinv, hook->parentinv, sizeof(hmd->parentinv));
    hmd->totindex = hook->totindex;

    BLI_addhead(&ob->modifiers, hmd);
    BLI_remlink(&ob->hooks, hook);

    BKE_modifier_unique_name(&ob->modifiers, (ModifierData *)hmd);

    MEM_freeN(hook);
  }

  BLO_read_data_address(reader, &ob->iuser);
  if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE && !ob->iuser) {
    BKE_object_empty_draw_type_set(ob, ob->empty_drawtype);
  }

  BKE_object_runtime_reset(ob);
  BLO_read_list(reader, &ob->pc_ids);

  /* in case this value changes in future, clamp else we get undefined behavior */
  CLAMP(ob->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

  if (ob->sculpt) {
    ob->sculpt = NULL;
    /* Only create data on undo, otherwise rely on editor mode switching. */
    if (BLO_read_data_is_undo(reader) && (ob->mode & OB_MODE_ALL_SCULPT)) {
      BKE_object_sculpt_data_create(ob);
    }
  }

  BLO_read_data_address(reader, &ob->preview);
  BKE_previewimg_blend_read(reader, ob->preview);
}

/* XXX deprecated - old animation system */
static void lib_link_nlastrips(BlendLibReader *reader, ID *id, ListBase *striplist)
{
  LISTBASE_FOREACH (bActionStrip *, strip, striplist) {
    BLO_read_id_address(reader, id->lib, &strip->object);
    BLO_read_id_address(reader, id->lib, &strip->act);
    BLO_read_id_address(reader, id->lib, &strip->ipo);
    LISTBASE_FOREACH (bActionModifier *, amod, &strip->modifiers) {
      BLO_read_id_address(reader, id->lib, &amod->ob);
    }
  }
}

/* XXX deprecated - old animation system */
static void lib_link_constraint_channels(BlendLibReader *reader, ID *id, ListBase *chanbase)
{
  LISTBASE_FOREACH (bConstraintChannel *, chan, chanbase) {
    BLO_read_id_address(reader, id->lib, &chan->ipo);
  }
}

static void object_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Object *ob = (Object *)id;

  bool warn = false;

  /* XXX deprecated - old animation system <<< */
  BLO_read_id_address(reader, ob->id.lib, &ob->ipo);
  BLO_read_id_address(reader, ob->id.lib, &ob->action);
  /* >>> XXX deprecated - old animation system */

  BLO_read_id_address(reader, ob->id.lib, &ob->parent);
  BLO_read_id_address(reader, ob->id.lib, &ob->track);
  BLO_read_id_address(reader, ob->id.lib, &ob->poselib);

  /* 2.8x drops support for non-empty dupli instances. */
  if (ob->type == OB_EMPTY) {
    BLO_read_id_address(reader, ob->id.lib, &ob->instance_collection);
  }
  else {
    if (ob->instance_collection != NULL) {
      ID *new_id = BLO_read_get_new_id_address(reader, ob->id.lib, &ob->instance_collection->id);
      BLO_reportf_wrap(BLO_read_lib_reports(reader),
                       RPT_WARNING,
                       TIP_("Non-Empty object '%s' cannot duplicate collection '%s' "
                            "anymore in Blender 2.80, removed instancing"),
                       ob->id.name + 2,
                       new_id->name + 2);
    }
    ob->instance_collection = NULL;
    ob->transflag &= ~OB_DUPLICOLLECTION;
  }

  BLO_read_id_address(reader, ob->id.lib, &ob->proxy);
  if (ob->proxy) {
    /* paranoia check, actually a proxy_from pointer should never be written... */
    if (ob->proxy->id.lib == NULL) {
      ob->proxy->proxy_from = NULL;
      ob->proxy = NULL;

      if (ob->id.lib) {
        printf("Proxy lost from  object %s lib %s\n", ob->id.name + 2, ob->id.lib->filepath);
      }
      else {
        printf("Proxy lost from  object %s lib <NONE>\n", ob->id.name + 2);
      }
    }
    else {
      /* this triggers object_update to always use a copy */
      ob->proxy->proxy_from = ob;
    }
  }
  BLO_read_id_address(reader, ob->id.lib, &ob->proxy_group);

  void *poin = ob->data;
  BLO_read_id_address(reader, ob->id.lib, &ob->data);

  if (ob->data == NULL && poin != NULL) {
    if (ob->id.lib) {
      printf("Can't find obdata of %s lib %s\n", ob->id.name + 2, ob->id.lib->filepath);
    }
    else {
      printf("Object %s lost data.\n", ob->id.name + 2);
    }

    ob->type = OB_EMPTY;
    warn = true;

    if (ob->pose) {
      /* we can't call #BKE_pose_free() here because of library linking
       * freeing will recurse down into every pose constraints ID pointers
       * which are not always valid, so for now free directly and suffer
       * some leaked memory rather than crashing immediately
       * while bad this _is_ an exceptional case - campbell */
#if 0
      BKE_pose_free(ob->pose);
#else
      MEM_freeN(ob->pose);
#endif
      ob->pose = NULL;
      ob->mode &= ~OB_MODE_POSE;
    }
  }
  for (int a = 0; a < ob->totcol; a++) {
    BLO_read_id_address(reader, ob->id.lib, &ob->mat[a]);
  }

  /* When the object is local and the data is library its possible
   * the material list size gets out of sync. T22663. */
  if (ob->data && ob->id.lib != ((ID *)ob->data)->lib) {
    const short *totcol_data = BKE_object_material_len_p(ob);
    /* Only expand so as not to lose any object materials that might be set. */
    if (totcol_data && (*totcol_data > ob->totcol)) {
      /* printf("'%s' %d -> %d\n", ob->id.name, ob->totcol, *totcol_data); */
      BKE_object_material_resize(BLO_read_lib_get_main(reader), ob, *totcol_data, false);
    }
  }

  BLO_read_id_address(reader, ob->id.lib, &ob->gpd);

  /* if id.us==0 a new base will be created later on */

  /* WARNING! Also check expand_object(), should reflect the stuff below. */
  BKE_pose_blend_read_lib(reader, ob, ob->pose);
  BKE_constraint_blend_read_lib(reader, &ob->id, &ob->constraints);

  /* XXX deprecated - old animation system <<< */
  lib_link_constraint_channels(reader, &ob->id, &ob->constraintChannels);
  lib_link_nlastrips(reader, &ob->id, &ob->nlastrips);
  /* >>> XXX deprecated - old animation system */

  LISTBASE_FOREACH (PartEff *, paf, &ob->effect) {
    if (paf->type == EFF_PARTICLE) {
      BLO_read_id_address(reader, ob->id.lib, &paf->group);
    }
  }

  {
    FluidsimModifierData *fluidmd = (FluidsimModifierData *)BKE_modifiers_findby_type(
        ob, eModifierType_Fluidsim);

    if (fluidmd && fluidmd->fss) {
      BLO_read_id_address(
          reader, ob->id.lib, &fluidmd->fss->ipo); /* XXX deprecated - old animation system */
    }
  }

  {
    FluidModifierData *fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob,
                                                                            eModifierType_Fluid);

    if (fmd && (fmd->type == MOD_FLUID_TYPE_DOMAIN) && fmd->domain) {
      /* Flag for refreshing the simulation after loading */
      fmd->domain->flags |= FLUID_DOMAIN_FILE_LOAD;
    }
    else if (fmd && (fmd->type == MOD_FLUID_TYPE_FLOW) && fmd->flow) {
      fmd->flow->flags &= ~FLUID_FLOW_NEEDS_UPDATE;
    }
    else if (fmd && (fmd->type == MOD_FLUID_TYPE_EFFEC) && fmd->effector) {
      fmd->effector->flags &= ~FLUID_EFFECTOR_NEEDS_UPDATE;
    }
  }

  /* texture field */
  if (ob->pd) {
    BKE_particle_partdeflect_blend_read_lib(reader, &ob->id, ob->pd);
  }

  if (ob->soft) {
    BLO_read_id_address(reader, ob->id.lib, &ob->soft->collision_group);

    BLO_read_id_address(reader, ob->id.lib, &ob->soft->effector_weights->group);
  }

  BKE_particle_system_blend_read_lib(reader, ob, &ob->id, &ob->particlesystem);
  BKE_modifier_blend_read_lib(reader, ob);
  BKE_gpencil_modifier_blend_read_lib(reader, ob);
  BKE_shaderfx_blend_read_lib(reader, ob);

  if (ob->rigidbody_constraint) {
    BLO_read_id_address(reader, ob->id.lib, &ob->rigidbody_constraint->ob1);
    BLO_read_id_address(reader, ob->id.lib, &ob->rigidbody_constraint->ob2);
  }

  if (warn) {
    BLO_reportf_wrap(BLO_read_lib_reports(reader), RPT_WARNING, "Warning in console");
  }
}

/* XXX deprecated - old animation system */
static void expand_constraint_channels(BlendExpander *expander, ListBase *chanbase)
{
  LISTBASE_FOREACH (bConstraintChannel *, chan, chanbase) {
    BLO_expand(expander, chan->ipo);
  }
}

static void expand_object_expandModifiers(void *userData,
                                          Object *UNUSED(ob),
                                          ID **idpoin,
                                          int UNUSED(cb_flag))
{
  BlendExpander *expander = userData;
  BLO_expand(expander, *idpoin);
}

PartEff *BKE_object_do_version_give_parteff_245(Object *ob)
{
  PartEff *paf;

  paf = ob->effect.first;
  while (paf) {
    if (paf->type == EFF_PARTICLE) {
      return paf;
    }
    paf = paf->next;
  }
  return NULL;
}

static void object_blend_read_expand(BlendExpander *expander, ID *id)
{
  Object *ob = (Object *)id;

  BLO_expand(expander, ob->data);

  /* expand_object_expandModifier() */
  if (ob->modifiers.first) {
    BKE_modifiers_foreach_ID_link(ob, expand_object_expandModifiers, expander);
  }

  /* expand_object_expandModifier() */
  if (ob->greasepencil_modifiers.first) {
    BKE_gpencil_modifiers_foreach_ID_link(ob, expand_object_expandModifiers, expander);
  }

  /* expand_object_expandShaderFx() */
  if (ob->shader_fx.first) {
    BKE_shaderfx_foreach_ID_link(ob, expand_object_expandModifiers, expander);
  }

  BKE_pose_blend_read_expand(expander, ob->pose);
  BLO_expand(expander, ob->poselib);
  BKE_constraint_blend_read_expand(expander, &ob->constraints);

  BLO_expand(expander, ob->gpd);

  /* XXX deprecated - old animation system (for version patching only) */
  BLO_expand(expander, ob->ipo);
  BLO_expand(expander, ob->action);

  expand_constraint_channels(expander, &ob->constraintChannels);

  LISTBASE_FOREACH (bActionStrip *, strip, &ob->nlastrips) {
    BLO_expand(expander, strip->object);
    BLO_expand(expander, strip->act);
    BLO_expand(expander, strip->ipo);
  }
  /* XXX deprecated - old animation system (for version patching only) */

  for (int a = 0; a < ob->totcol; a++) {
    BLO_expand(expander, ob->mat[a]);
  }

  PartEff *paf = BKE_object_do_version_give_parteff_245(ob);
  if (paf && paf->group) {
    BLO_expand(expander, paf->group);
  }

  if (ob->instance_collection) {
    BLO_expand(expander, ob->instance_collection);
  }

  if (ob->proxy) {
    BLO_expand(expander, ob->proxy);
  }
  if (ob->proxy_group) {
    BLO_expand(expander, ob->proxy_group);
  }

  LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
    BLO_expand(expander, psys->part);
  }

  if (ob->pd) {
    BLO_expand(expander, ob->pd->tex);
    BLO_expand(expander, ob->pd->f_source);
  }

  if (ob->soft) {
    BLO_expand(expander, ob->soft->collision_group);

    if (ob->soft->effector_weights) {
      BLO_expand(expander, ob->soft->effector_weights->group);
    }
  }

  if (ob->rigidbody_constraint) {
    BLO_expand(expander, ob->rigidbody_constraint->ob1);
    BLO_expand(expander, ob->rigidbody_constraint->ob2);
  }
}

IDTypeInfo IDType_ID_OB = {
    .id_code = ID_OB,
    .id_filter = FILTER_ID_OB,
    .main_listbase_index = INDEX_ID_OB,
    .struct_size = sizeof(Object),
    .name = "Object",
    .name_plural = "objects",
    .translation_context = BLT_I18NCONTEXT_ID_OBJECT,
    .flags = 0,

    .init_data = object_init_data,
    .copy_data = object_copy_data,
    .free_data = object_free_data,
    .make_local = object_make_local,
    .foreach_id = object_foreach_id,
    .foreach_cache = NULL,

    .blend_write = object_blend_write,
    .blend_read_data = object_blend_read_data,
    .blend_read_lib = object_blend_read_lib,
    .blend_read_expand = object_blend_read_expand,

    .blend_read_undo_preserve = NULL,
};

void BKE_object_workob_clear(Object *workob)
{
  memset(workob, 0, sizeof(Object));

  workob->scale[0] = workob->scale[1] = workob->scale[2] = 1.0f;
  workob->dscale[0] = workob->dscale[1] = workob->dscale[2] = 1.0f;
  workob->rotmode = ROT_MODE_EUL;
}

void BKE_object_free_particlesystems(Object *ob)
{
  ParticleSystem *psys;

  while ((psys = BLI_pophead(&ob->particlesystem))) {
    psys_free(ob, psys);
  }
}

void BKE_object_free_softbody(Object *ob)
{
  sbFree(ob);
}

void BKE_object_free_curve_cache(Object *ob)
{
  if (ob->runtime.curve_cache) {
    BKE_displist_free(&ob->runtime.curve_cache->disp);
    BKE_curve_bevelList_free(&ob->runtime.curve_cache->bev);
    if (ob->runtime.curve_cache->path) {
      free_path(ob->runtime.curve_cache->path);
    }
    BKE_nurbList_free(&ob->runtime.curve_cache->deformed_nurbs);
    MEM_freeN(ob->runtime.curve_cache);
    ob->runtime.curve_cache = NULL;
  }
}

void BKE_object_free_modifiers(Object *ob, const int flag)
{
  ModifierData *md;
  GpencilModifierData *gp_md;

  while ((md = BLI_pophead(&ob->modifiers))) {
    BKE_modifier_free_ex(md, flag);
  }

  while ((gp_md = BLI_pophead(&ob->greasepencil_modifiers))) {
    BKE_gpencil_modifier_free_ex(gp_md, flag);
  }
  /* particle modifiers were freed, so free the particlesystems as well */
  BKE_object_free_particlesystems(ob);

  /* same for softbody */
  BKE_object_free_softbody(ob);

  /* modifiers may have stored data in the DM cache */
  BKE_object_free_derived_caches(ob);
}

void BKE_object_free_shaderfx(Object *ob, const int flag)
{
  ShaderFxData *fx;

  while ((fx = BLI_pophead(&ob->shader_fx))) {
    BKE_shaderfx_free_ex(fx, flag);
  }
}

void BKE_object_modifier_hook_reset(Object *ob, HookModifierData *hmd)
{
  /* reset functionality */
  if (hmd->object) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(hmd->object->pose, hmd->subtarget);

    if (hmd->subtarget[0] && pchan) {
      float imat[4][4], mat[4][4];

      /* Calculate the world-space matrix for the pose-channel target first,
       * then carry on as usual. */
      mul_m4_m4m4(mat, hmd->object->obmat, pchan->pose_mat);

      invert_m4_m4(imat, mat);
      mul_m4_m4m4(hmd->parentinv, imat, ob->obmat);
    }
    else {
      invert_m4_m4(hmd->object->imat, hmd->object->obmat);
      mul_m4_m4m4(hmd->parentinv, hmd->object->imat, ob->obmat);
    }
  }
}

void BKE_object_modifier_gpencil_hook_reset(Object *ob, HookGpencilModifierData *hmd)
{
  if (hmd->object == NULL) {
    return;
  }
  /* reset functionality */
  bPoseChannel *pchan = BKE_pose_channel_find_name(hmd->object->pose, hmd->subtarget);

  if (hmd->subtarget[0] && pchan) {
    float imat[4][4], mat[4][4];

    /* Calculate the world-space matrix for the pose-channel target first,
     * then carry on as usual. */
    mul_m4_m4m4(mat, hmd->object->obmat, pchan->pose_mat);

    invert_m4_m4(imat, mat);
    mul_m4_m4m4(hmd->parentinv, imat, ob->obmat);
  }
  else {
    invert_m4_m4(hmd->object->imat, hmd->object->obmat);
    mul_m4_m4m4(hmd->parentinv, hmd->object->imat, ob->obmat);
  }
}

bool BKE_object_support_modifier_type_check(const Object *ob, int modifier_type)
{
  const ModifierTypeInfo *mti;

  mti = BKE_modifier_get_info(modifier_type);

  /* Only geometry objects should be able to get modifiers T25291. */
  if (ob->type == OB_HAIR) {
    return (mti->modifyHair != NULL) || (mti->flags & eModifierTypeFlag_AcceptsVertexCosOnly);
  }
  if (ob->type == OB_POINTCLOUD) {
    return (mti->modifyPointCloud != NULL) ||
           (mti->flags & eModifierTypeFlag_AcceptsVertexCosOnly);
  }
  if (ob->type == OB_VOLUME) {
    return (mti->modifyVolume != NULL);
  }
  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
    if (ob->type == OB_LATTICE && (mti->flags & eModifierTypeFlag_AcceptsVertexCosOnly) == 0) {
      return false;
    }

    if (!((mti->flags & eModifierTypeFlag_AcceptsCVs) ||
          (ob->type == OB_MESH && (mti->flags & eModifierTypeFlag_AcceptsMesh)))) {
      return false;
    }

    return true;
  }

  return false;
}

bool BKE_object_copy_modifier(struct Object *ob_dst, const struct Object *ob_src, ModifierData *md)
{
  ModifierData *nmd = NULL;

  if (ELEM(md->type, eModifierType_Hook, eModifierType_Collision)) {
    return false;
  }

  if (!BKE_object_support_modifier_type_check(ob_dst, md->type)) {
    return false;
  }

  switch (md->type) {
    case eModifierType_Softbody:
      BKE_object_copy_softbody(ob_dst, ob_src, 0);
      break;
    case eModifierType_Skin:
      /* ensure skin-node customdata exists */
      BKE_mesh_ensure_skin_customdata(ob_dst->data);
      break;
  }

  nmd = BKE_modifier_new(md->type);
  BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));

  if (md->type == eModifierType_Multires) {
    /* Has to be done after mod creation, but *before* we actually copy its settings! */
    multiresModifier_sync_levels_ex(
        ob_dst, (MultiresModifierData *)md, (MultiresModifierData *)nmd);
  }

  BKE_modifier_copydata(md, nmd);
  BLI_addtail(&ob_dst->modifiers, nmd);
  BKE_modifier_unique_name(&ob_dst->modifiers, nmd);

  return true;
}

bool BKE_object_copy_gpencil_modifier(struct Object *ob_dst, GpencilModifierData *md)
{
  GpencilModifierData *nmd = NULL;

  nmd = BKE_gpencil_modifier_new(md->type);
  BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));

  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);
  mti->copyData(md, nmd);

  BLI_addtail(&ob_dst->greasepencil_modifiers, nmd);
  BKE_gpencil_modifier_unique_name(&ob_dst->greasepencil_modifiers, nmd);

  return true;
}

void BKE_object_link_modifiers(struct Object *ob_dst, const struct Object *ob_src)
{
  BKE_object_free_modifiers(ob_dst, 0);

  if (!ELEM(ob_dst->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE, OB_GPENCIL)) {
    /* only objects listed above can have modifiers and linking them to objects
     * which doesn't have modifiers stack is quite silly */
    return;
  }

  /* No grease pencil modifiers. */
  if ((ob_src->type != OB_GPENCIL) && (ob_dst->type != OB_GPENCIL)) {
    LISTBASE_FOREACH (ModifierData *, md, &ob_src->modifiers) {
      BKE_object_copy_modifier(ob_dst, ob_src, md);
    }
  }

  /* Copy grease pencil modifiers. */
  if ((ob_src->type == OB_GPENCIL) && (ob_dst->type == OB_GPENCIL)) {
    LISTBASE_FOREACH (GpencilModifierData *, md, &ob_src->greasepencil_modifiers) {
      BKE_object_copy_gpencil_modifier(ob_dst, md);
    }
  }

  BKE_object_copy_particlesystems(ob_dst, ob_src, 0);

  /* TODO: smoke?, cloth? */
}

/**
 * Copy CCG related data. Used to sync copy of mesh with reshaped original mesh.
 */
static void copy_ccg_data(Mesh *mesh_destination, Mesh *mesh_source, int layer_type)
{
  BLI_assert(mesh_destination->totloop == mesh_source->totloop);
  CustomData *data_destination = &mesh_destination->ldata;
  CustomData *data_source = &mesh_source->ldata;
  const int num_elements = mesh_source->totloop;
  if (!CustomData_has_layer(data_source, layer_type)) {
    return;
  }
  const int layer_index = CustomData_get_layer_index(data_destination, layer_type);
  CustomData_free_layer(data_destination, layer_type, num_elements, layer_index);
  BLI_assert(!CustomData_has_layer(data_destination, layer_type));
  CustomData_add_layer(data_destination, layer_type, CD_CALLOC, NULL, num_elements);
  BLI_assert(CustomData_has_layer(data_destination, layer_type));
  CustomData_copy_layer_type_data(data_source, data_destination, layer_type, 0, 0, num_elements);
}

static void object_update_from_subsurf_ccg(Object *object)
{
  /* Currently CCG is only created for Mesh objects. */
  if (object->type != OB_MESH) {
    return;
  }
  /* If object does not own evaluated mesh we can not access it since it might be freed already
   * (happens on dependency graph free where order of CoW-ed IDs free is undefined).
   *
   * Good news is: such mesh does not have modifiers applied, so no need to worry about CCG. */
  if (!object->runtime.is_data_eval_owned) {
    return;
  }
  /* Object was never evaluated, so can not have CCG subdivision surface. */
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(object);
  if (mesh_eval == NULL) {
    return;
  }
  SubdivCCG *subdiv_ccg = mesh_eval->runtime.subdiv_ccg;
  if (subdiv_ccg == NULL) {
    return;
  }
  /* Check whether there is anything to be reshaped. */
  if (!subdiv_ccg->dirty.coords && !subdiv_ccg->dirty.hidden) {
    return;
  }
  const int tot_level = mesh_eval->runtime.subdiv_ccg_tot_level;
  Object *object_orig = DEG_get_original_object(object);
  Mesh *mesh_orig = (Mesh *)object_orig->data;
  multiresModifier_reshapeFromCCG(tot_level, mesh_orig, subdiv_ccg);
  /* NOTE: we need to reshape into an original mesh from main database,
   * allowing:
   *
   *  - Update copies of that mesh at any moment.
   *  - Save the file without doing extra reshape.
   *  - All the users of the mesh have updated displacement.
   *
   * However, the tricky part here is that we only know about sculpted
   * state of a mesh on an object level, and object is being updated after
   * mesh datablock is updated. This forces us to:
   *
   *  - Update mesh datablock from object evaluation, which is technically
   *    forbidden, but there is no other place for this yet.
   *  - Reshape to the original mesh from main database, and then copy updated
   *    layer to copy of that mesh (since copy of the mesh has decoupled
   *    custom data layers).
   *
   * All this is defeating all the designs we need to follow to allow safe
   * threaded evaluation, but this is as good as we can make it within the
   * current sculpt//evaluated mesh design. This is also how we've survived
   * with old DerivedMesh based solutions. So, while this is all wrong and
   * needs reconsideration, doesn't seem to be a big stopper for real
   * production artists.
   */
  /* TODO(sergey): Solve this somehow, to be fully stable for threaded
   * evaluation environment.
   */
  /* NOTE: runtime.data_orig is what was before assigning mesh_eval,
   * it is orig as in what was in object_eval->data before evaluating
   * modifier stack.
   *
   * mesh_cow is a copy-on-written version od object_orig->data.
   */
  Mesh *mesh_cow = (Mesh *)object->runtime.data_orig;
  copy_ccg_data(mesh_cow, mesh_orig, CD_MDISPS);
  copy_ccg_data(mesh_cow, mesh_orig, CD_GRID_PAINT_MASK);
  /* Everything is now up-to-date. */
  subdiv_ccg->dirty.coords = false;
  subdiv_ccg->dirty.hidden = false;
}

/**
 * Assign #Object.data after modifier stack evaluation.
 */
void BKE_object_eval_assign_data(Object *object_eval, ID *data_eval, bool is_owned)
{
  BLI_assert(object_eval->id.tag & LIB_TAG_COPIED_ON_WRITE);
  BLI_assert(object_eval->runtime.data_eval == NULL);
  BLI_assert(data_eval->tag & LIB_TAG_NO_MAIN);

  if (is_owned) {
    /* Set flag for debugging. */
    data_eval->tag |= LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT;
  }

  /* Assigned evaluated data. */
  object_eval->runtime.data_eval = data_eval;
  object_eval->runtime.is_data_eval_owned = is_owned;

  /* Overwrite data of evaluated object, if the datablock types match. */
  ID *data = object_eval->data;
  if (GS(data->name) == GS(data_eval->name)) {
    /* NOTE: we are not supposed to invoke evaluation for original objects,
     * but some areas are still being ported, so we play safe here. */
    if (object_eval->id.tag & LIB_TAG_COPIED_ON_WRITE) {
      object_eval->data = data_eval;
    }
  }
}

/**
 * Free data derived from mesh, called when mesh changes or is freed.
 */
void BKE_object_free_derived_caches(Object *ob)
{
  MEM_SAFE_FREE(ob->runtime.bb);

  object_update_from_subsurf_ccg(ob);

  if (ob->runtime.data_eval != NULL) {
    if (ob->runtime.is_data_eval_owned) {
      ID *data_eval = ob->runtime.data_eval;
      if (GS(data_eval->name) == ID_ME) {
        BKE_mesh_eval_delete((Mesh *)data_eval);
      }
      else {
        BKE_libblock_free_datablock(data_eval, 0);
        MEM_freeN(data_eval);
      }
    }
    ob->runtime.data_eval = NULL;
  }
  if (ob->runtime.mesh_deform_eval != NULL) {
    Mesh *mesh_deform_eval = ob->runtime.mesh_deform_eval;
    BKE_mesh_eval_delete(mesh_deform_eval);
    ob->runtime.mesh_deform_eval = NULL;
  }

  /* Restore initial pointer for copy-on-write datablocks, object->data
   * might be pointing to an evaluated datablock data was just freed above. */
  if (ob->runtime.data_orig != NULL) {
    ob->data = ob->runtime.data_orig;
  }

  BKE_object_to_mesh_clear(ob);
  BKE_object_free_curve_cache(ob);

  /* Clear grease pencil data. */
  if (ob->runtime.gpd_eval != NULL) {
    BKE_gpencil_eval_delete(ob->runtime.gpd_eval);
    ob->runtime.gpd_eval = NULL;
  }
}

void BKE_object_free_caches(Object *object)
{
  ModifierData *md;
  short update_flag = 0;

  /* Free particle system caches holding paths. */
  if (object->particlesystem.first) {
    ParticleSystem *psys;
    for (psys = object->particlesystem.first; psys != NULL; psys = psys->next) {
      psys_free_path_cache(psys, psys->edit);
      update_flag |= ID_RECALC_PSYS_REDO;
    }
  }

  /* Free memory used by cached derived meshes in the particle system modifiers. */
  for (md = object->modifiers.first; md != NULL; md = md->next) {
    if (md->type == eModifierType_ParticleSystem) {
      ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
      if (psmd->mesh_final) {
        BKE_id_free(NULL, psmd->mesh_final);
        psmd->mesh_final = NULL;
        if (psmd->mesh_original) {
          BKE_id_free(NULL, psmd->mesh_original);
          psmd->mesh_original = NULL;
        }
        psmd->flag |= eParticleSystemFlag_file_loaded;
        update_flag |= ID_RECALC_GEOMETRY;
      }
    }
  }

  /* NOTE: If object is coming from a duplicator, it might be a temporary
   * object created by dependency graph, which shares pointers with original
   * object. In this case we can not free anything.
   */
  if ((object->base_flag & BASE_FROM_DUPLI) == 0) {
    BKE_object_free_derived_caches(object);
    update_flag |= ID_RECALC_GEOMETRY;
  }

  /* Tag object for update, so once memory critical operation is over and
   * scene update routines are back to its business the object will be
   * guaranteed to be in a known state.
   */
  if (update_flag != 0) {
    DEG_id_tag_update(&object->id, update_flag);
  }
}

/**
 * Actual check for internal data, not context or flags.
 */
bool BKE_object_is_in_editmode(const Object *ob)
{
  if (ob->data == NULL) {
    return false;
  }

  switch (ob->type) {
    case OB_MESH:
      return ((Mesh *)ob->data)->edit_mesh != NULL;
    case OB_ARMATURE:
      return ((bArmature *)ob->data)->edbo != NULL;
    case OB_FONT:
      return ((Curve *)ob->data)->editfont != NULL;
    case OB_MBALL:
      return ((MetaBall *)ob->data)->editelems != NULL;
    case OB_LATTICE:
      return ((Lattice *)ob->data)->editlatt != NULL;
    case OB_SURF:
    case OB_CURVE:
      return ((Curve *)ob->data)->editnurb != NULL;
    case OB_GPENCIL:
      /* Grease Pencil object has no edit mode data. */
      return GPENCIL_EDIT_MODE((bGPdata *)ob->data);
    default:
      return false;
  }
}

bool BKE_object_is_in_editmode_vgroup(const Object *ob)
{
  return (OB_TYPE_SUPPORT_VGROUP(ob->type) && BKE_object_is_in_editmode(ob));
}

bool BKE_object_data_is_in_editmode(const ID *id)
{
  const short type = GS(id->name);
  BLI_assert(OB_DATA_SUPPORT_EDITMODE(type));
  switch (type) {
    case ID_ME:
      return ((const Mesh *)id)->edit_mesh != NULL;
    case ID_CU:
      return ((((const Curve *)id)->editnurb != NULL) || (((const Curve *)id)->editfont != NULL));
    case ID_MB:
      return ((const MetaBall *)id)->editelems != NULL;
    case ID_LT:
      return ((const Lattice *)id)->editlatt != NULL;
    case ID_AR:
      return ((const bArmature *)id)->edbo != NULL;
    default:
      BLI_assert(0);
      return false;
  }
}

char *BKE_object_data_editmode_flush_ptr_get(struct ID *id)
{
  const short type = GS(id->name);
  switch (type) {
    case ID_ME: {
      BMEditMesh *em = ((Mesh *)id)->edit_mesh;
      if (em != NULL) {
        return &em->needs_flush_to_id;
      }
      break;
    }
    case ID_CU: {
      if (((Curve *)id)->vfont != NULL) {
        EditFont *ef = ((Curve *)id)->editfont;
        if (ef != NULL) {
          return &ef->needs_flush_to_id;
        }
      }
      else {
        EditNurb *editnurb = ((Curve *)id)->editnurb;
        if (editnurb) {
          return &editnurb->needs_flush_to_id;
        }
      }
      break;
    }
    case ID_MB: {
      MetaBall *mb = (MetaBall *)id;
      return &mb->needs_flush_to_id;
    }
    case ID_LT: {
      EditLatt *editlatt = ((Lattice *)id)->editlatt;
      if (editlatt) {
        return &editlatt->needs_flush_to_id;
      }
      break;
    }
    case ID_AR: {
      bArmature *arm = (bArmature *)id;
      return &arm->needs_flush_to_id;
    }
    default:
      BLI_assert(0);
      return NULL;
  }
  return NULL;
}

bool BKE_object_is_in_wpaint_select_vert(const Object *ob)
{
  if (ob->type == OB_MESH) {
    Mesh *me = ob->data;
    return ((ob->mode & OB_MODE_WEIGHT_PAINT) && (me->edit_mesh == NULL) &&
            (ME_EDIT_PAINT_SEL_MODE(me) == SCE_SELECT_VERTEX));
  }

  return false;
}

bool BKE_object_has_mode_data(const struct Object *ob, eObjectMode object_mode)
{
  if (object_mode & OB_MODE_EDIT) {
    if (BKE_object_is_in_editmode(ob)) {
      return true;
    }
  }
  else if (object_mode & OB_MODE_VERTEX_PAINT) {
    if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_VERTEX_PAINT)) {
      return true;
    }
  }
  else if (object_mode & OB_MODE_WEIGHT_PAINT) {
    if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_WEIGHT_PAINT)) {
      return true;
    }
  }
  else if (object_mode & OB_MODE_SCULPT) {
    if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_SCULPT)) {
      return true;
    }
  }
  else if (object_mode & OB_MODE_POSE) {
    if (ob->pose != NULL) {
      return true;
    }
  }
  return false;
}

bool BKE_object_is_mode_compat(const struct Object *ob, eObjectMode object_mode)
{
  return ((ob->mode == object_mode) || (ob->mode & object_mode) != 0);
}

/**
 * Return which parts of the object are visible, as evaluated by depsgraph
 */
int BKE_object_visibility(const Object *ob, const int dag_eval_mode)
{
  if ((ob->base_flag & BASE_VISIBLE_DEPSGRAPH) == 0) {
    return 0;
  }

  /* Test which components the object has. */
  int visibility = OB_VISIBLE_SELF;
  if (ob->particlesystem.first) {
    visibility |= OB_VISIBLE_INSTANCES | OB_VISIBLE_PARTICLES;
  }
  else if (ob->transflag & OB_DUPLI) {
    visibility |= OB_VISIBLE_INSTANCES;
  }

  /* Optional hiding of self if there are particles or instancers. */
  if (visibility & (OB_VISIBLE_PARTICLES | OB_VISIBLE_INSTANCES)) {
    switch ((eEvaluationMode)dag_eval_mode) {
      case DAG_EVAL_VIEWPORT:
        if (!(ob->duplicator_visibility_flag & OB_DUPLI_FLAG_VIEWPORT)) {
          visibility &= ~OB_VISIBLE_SELF;
        }
        break;
      case DAG_EVAL_RENDER:
        if (!(ob->duplicator_visibility_flag & OB_DUPLI_FLAG_RENDER)) {
          visibility &= ~OB_VISIBLE_SELF;
        }
        break;
    }
  }

  return visibility;
}

bool BKE_object_exists_check(Main *bmain, const Object *obtest)
{
  Object *ob;

  if (obtest == NULL) {
    return false;
  }

  ob = bmain->objects.first;
  while (ob) {
    if (ob == obtest) {
      return true;
    }
    ob = ob->id.next;
  }
  return false;
}

/* *************************************************** */

static const char *get_obdata_defname(int type)
{
  switch (type) {
    case OB_MESH:
      return DATA_("Mesh");
    case OB_CURVE:
      return DATA_("Curve");
    case OB_SURF:
      return DATA_("Surf");
    case OB_FONT:
      return DATA_("Text");
    case OB_MBALL:
      return DATA_("Mball");
    case OB_CAMERA:
      return DATA_("Camera");
    case OB_LAMP:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Light");
    case OB_LATTICE:
      return DATA_("Lattice");
    case OB_ARMATURE:
      return DATA_("Armature");
    case OB_SPEAKER:
      return DATA_("Speaker");
    case OB_HAIR:
      return DATA_("Hair");
    case OB_POINTCLOUD:
      return DATA_("PointCloud");
    case OB_VOLUME:
      return DATA_("Volume");
    case OB_EMPTY:
      return DATA_("Empty");
    case OB_GPENCIL:
      return DATA_("GPencil");
    case OB_LIGHTPROBE:
      return DATA_("LightProbe");
    default:
      CLOG_ERROR(&LOG, "Internal error, bad type: %d", type);
      return DATA_("Empty");
  }
}

static void object_init(Object *ob, const short ob_type)
{
  object_init_data(&ob->id);

  ob->type = ob_type;

  if (ob->type != OB_EMPTY) {
    zero_v2(ob->ima_ofs);
  }

  if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
    ob->trackflag = OB_NEGZ;
    ob->upflag = OB_POSY;
  }

  if (ob->type == OB_GPENCIL) {
    ob->dtx |= OB_USE_GPENCIL_LIGHTS;
  }
}

void *BKE_object_obdata_add_from_type(Main *bmain, int type, const char *name)
{
  if (name == NULL) {
    name = get_obdata_defname(type);
  }

  switch (type) {
    case OB_MESH:
      return BKE_mesh_add(bmain, name);
    case OB_CURVE:
      return BKE_curve_add(bmain, name, OB_CURVE);
    case OB_SURF:
      return BKE_curve_add(bmain, name, OB_SURF);
    case OB_FONT:
      return BKE_curve_add(bmain, name, OB_FONT);
    case OB_MBALL:
      return BKE_mball_add(bmain, name);
    case OB_CAMERA:
      return BKE_camera_add(bmain, name);
    case OB_LAMP:
      return BKE_light_add(bmain, name);
    case OB_LATTICE:
      return BKE_lattice_add(bmain, name);
    case OB_ARMATURE:
      return BKE_armature_add(bmain, name);
    case OB_SPEAKER:
      return BKE_speaker_add(bmain, name);
    case OB_LIGHTPROBE:
      return BKE_lightprobe_add(bmain, name);
    case OB_GPENCIL:
      return BKE_gpencil_data_addnew(bmain, name);
    case OB_HAIR:
      return BKE_hair_add(bmain, name);
    case OB_POINTCLOUD:
      return BKE_pointcloud_add_default(bmain, name);
    case OB_VOLUME:
      return BKE_volume_add(bmain, name);
    case OB_EMPTY:
      return NULL;
    default:
      CLOG_ERROR(&LOG, "Internal error, bad type: %d", type);
      return NULL;
  }
}

/**
 * Return -1 on failure.
 */
int BKE_object_obdata_to_type(const ID *id)
{
  /* Keep in sync with #OB_DATA_SUPPORT_ID macro. */
  switch (GS(id->name)) {
    case ID_ME:
      return OB_MESH;
    case ID_CU:
      return BKE_curve_type_get((const Curve *)id);
    case ID_MB:
      return OB_MBALL;
    case ID_LA:
      return OB_LAMP;
    case ID_SPK:
      return OB_SPEAKER;
    case ID_CA:
      return OB_CAMERA;
    case ID_LT:
      return OB_LATTICE;
    case ID_GD:
      return OB_GPENCIL;
    case ID_AR:
      return OB_ARMATURE;
    case ID_LP:
      return OB_LIGHTPROBE;
    case ID_HA:
      return OB_HAIR;
    case ID_PT:
      return OB_POINTCLOUD;
    case ID_VO:
      return OB_VOLUME;
    default:
      return -1;
  }
}

/**
 * More general add: creates minimum required data, but without vertices etc.
 */
Object *BKE_object_add_only_object(Main *bmain, int type, const char *name)
{
  Object *ob;

  if (!name) {
    name = get_obdata_defname(type);
  }

  /* We cannot use #BKE_id_new here as we need some custom initialization code. */
  ob = BKE_libblock_alloc(bmain, ID_OB, name, 0);

  /* We increase object user count when linking to Collections. */
  id_us_min(&ob->id);

  /* default object vars */
  object_init(ob, type);

  return ob;
}

static Object *object_add_common(Main *bmain, ViewLayer *view_layer, int type, const char *name)
{
  Object *ob;

  ob = BKE_object_add_only_object(bmain, type, name);
  ob->data = BKE_object_obdata_add_from_type(bmain, type, name);
  BKE_view_layer_base_deselect_all(view_layer);

  DEG_id_tag_update_ex(
      bmain, &ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  return ob;
}

/**
 * General add: to scene, with layer from area and default name
 *
 * Object is added to the active #Collection.
 * If there is no linked collection to the active #ViewLayer we create a new one.
 *
 * \note Creates minimum required data, but without vertices etc.
 */
Object *BKE_object_add(Main *bmain, ViewLayer *view_layer, int type, const char *name)
{
  Object *ob;
  Base *base;
  LayerCollection *layer_collection;

  ob = object_add_common(bmain, view_layer, type, name);

  layer_collection = BKE_layer_collection_get_active(view_layer);
  BKE_collection_object_add(bmain, layer_collection->collection, ob);

  base = BKE_view_layer_base_find(view_layer, ob);
  BKE_view_layer_base_select_and_set_active(view_layer, base);

  return ob;
}

/**
 * Add a new object, using another one as a reference
 *
 * \param ob_src: object to use to determine the collections of the new object.
 */
Object *BKE_object_add_from(
    Main *bmain, Scene *scene, ViewLayer *view_layer, int type, const char *name, Object *ob_src)
{
  Object *ob;
  Base *base;

  ob = object_add_common(bmain, view_layer, type, name);
  BKE_collection_object_add_from(bmain, scene, ob_src, ob);

  base = BKE_view_layer_base_find(view_layer, ob);
  BKE_view_layer_base_select_and_set_active(view_layer, base);

  return ob;
}

/**
 * Add a new object, but assign the given datablock as the ob->data
 * for the newly created object.
 *
 * \param data: The datablock to assign as ob->data for the new object.
 *             This is assumed to be of the correct type.
 * \param do_id_user: If true, id_us_plus() will be called on data when
 *                 assigning it to the object.
 */
Object *BKE_object_add_for_data(
    Main *bmain, ViewLayer *view_layer, int type, const char *name, ID *data, bool do_id_user)
{
  Object *ob;
  Base *base;
  LayerCollection *layer_collection;

  /* same as object_add_common, except we don't create new ob->data */
  ob = BKE_object_add_only_object(bmain, type, name);
  ob->data = data;
  if (do_id_user) {
    id_us_plus(data);
  }

  BKE_view_layer_base_deselect_all(view_layer);
  DEG_id_tag_update_ex(
      bmain, &ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

  layer_collection = BKE_layer_collection_get_active(view_layer);
  BKE_collection_object_add(bmain, layer_collection->collection, ob);

  base = BKE_view_layer_base_find(view_layer, ob);
  BKE_view_layer_base_select_and_set_active(view_layer, base);

  return ob;
}

void BKE_object_copy_softbody(struct Object *ob_dst, const struct Object *ob_src, const int flag)
{
  SoftBody *sb = ob_src->soft;
  SoftBody *sbn;
  bool tagged_no_main = ob_dst->id.tag & LIB_TAG_NO_MAIN;

  ob_dst->softflag = ob_src->softflag;
  if (sb == NULL) {
    ob_dst->soft = NULL;
    return;
  }

  sbn = MEM_dupallocN(sb);

  if ((flag & LIB_ID_COPY_CACHES) == 0) {
    sbn->totspring = sbn->totpoint = 0;
    sbn->bpoint = NULL;
    sbn->bspring = NULL;
  }
  else {
    sbn->totspring = sb->totspring;
    sbn->totpoint = sb->totpoint;

    if (sbn->bpoint) {
      int i;

      sbn->bpoint = MEM_dupallocN(sbn->bpoint);

      for (i = 0; i < sbn->totpoint; i++) {
        if (sbn->bpoint[i].springs) {
          sbn->bpoint[i].springs = MEM_dupallocN(sbn->bpoint[i].springs);
        }
      }
    }

    if (sb->bspring) {
      sbn->bspring = MEM_dupallocN(sb->bspring);
    }
  }

  sbn->keys = NULL;
  sbn->totkey = sbn->totpointkey = 0;

  sbn->scratch = NULL;

  if (tagged_no_main == 0) {
    sbn->shared = MEM_dupallocN(sb->shared);
    sbn->shared->pointcache = BKE_ptcache_copy_list(
        &sbn->shared->ptcaches, &sb->shared->ptcaches, flag);
  }

  if (sb->effector_weights) {
    sbn->effector_weights = MEM_dupallocN(sb->effector_weights);
  }

  ob_dst->soft = sbn;
}

ParticleSystem *BKE_object_copy_particlesystem(ParticleSystem *psys, const int flag)
{
  ParticleSystem *psysn = MEM_dupallocN(psys);

  psys_copy_particles(psysn, psys);

  if (psys->clmd) {
    psysn->clmd = (ClothModifierData *)BKE_modifier_new(eModifierType_Cloth);
    BKE_modifier_copydata_ex((ModifierData *)psys->clmd, (ModifierData *)psysn->clmd, flag);
    psys->hair_in_mesh = psys->hair_out_mesh = NULL;
  }

  BLI_duplicatelist(&psysn->targets, &psys->targets);

  psysn->pathcache = NULL;
  psysn->childcache = NULL;
  psysn->edit = NULL;
  psysn->pdd = NULL;
  psysn->effectors = NULL;
  psysn->tree = NULL;
  psysn->bvhtree = NULL;
  psysn->batch_cache = NULL;

  BLI_listbase_clear(&psysn->pathcachebufs);
  BLI_listbase_clear(&psysn->childcachebufs);

  if (flag & LIB_ID_CREATE_NO_MAIN) {
    BLI_assert((psys->flag & PSYS_SHARED_CACHES) == 0);
    psysn->flag |= PSYS_SHARED_CACHES;
    BLI_assert(psysn->pointcache != NULL);
  }
  else {
    psysn->pointcache = BKE_ptcache_copy_list(&psysn->ptcaches, &psys->ptcaches, flag);
  }

  /* XXX - from reading existing code this seems correct but intended usage of
   * pointcache should /w cloth should be added in 'ParticleSystem' - campbell */
  if (psysn->clmd) {
    psysn->clmd->point_cache = psysn->pointcache;
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus((ID *)psysn->part);
  }

  return psysn;
}

void BKE_object_copy_particlesystems(Object *ob_dst, const Object *ob_src, const int flag)
{
  ParticleSystem *psys, *npsys;
  ModifierData *md;

  if (ob_dst->type != OB_MESH) {
    /* currently only mesh objects can have soft body */
    return;
  }

  BLI_listbase_clear(&ob_dst->particlesystem);
  for (psys = ob_src->particlesystem.first; psys; psys = psys->next) {
    npsys = BKE_object_copy_particlesystem(psys, flag);

    BLI_addtail(&ob_dst->particlesystem, npsys);

    /* need to update particle modifiers too */
    for (md = ob_dst->modifiers.first; md; md = md->next) {
      if (md->type == eModifierType_ParticleSystem) {
        ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
        if (psmd->psys == psys) {
          psmd->psys = npsys;
        }
      }
      else if (md->type == eModifierType_DynamicPaint) {
        DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
        if (pmd->brush) {
          if (pmd->brush->psys == psys) {
            pmd->brush->psys = npsys;
          }
        }
      }
      else if (md->type == eModifierType_Fluid) {
        FluidModifierData *fmd = (FluidModifierData *)md;

        if (fmd->type == MOD_FLUID_TYPE_FLOW) {
          if (fmd->flow) {
            if (fmd->flow->psys == psys) {
              fmd->flow->psys = npsys;
            }
          }
        }
      }
    }
  }
}

static void copy_object_pose(Object *obn, const Object *ob, const int flag)
{
  bPoseChannel *chan;

  /* note: need to clear obn->pose pointer first,
   * so that BKE_pose_copy_data works (otherwise there's a crash) */
  obn->pose = NULL;
  BKE_pose_copy_data_ex(&obn->pose, ob->pose, flag, true); /* true = copy constraints */

  for (chan = obn->pose->chanbase.first; chan; chan = chan->next) {
    bConstraint *con;

    chan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SIZE);

    /* XXX Remapping object pointing onto itself should be handled by generic
     *     BKE_library_remap stuff, but...
     *     the flush_constraint_targets callback am not sure about, so will delay that for now. */
    for (con = chan->constraints.first; con; con = con->next) {
      const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
      ListBase targets = {NULL, NULL};
      bConstraintTarget *ct;

      if (cti && cti->get_constraint_targets) {
        cti->get_constraint_targets(con, &targets);

        for (ct = targets.first; ct; ct = ct->next) {
          if (ct->tar == ob) {
            ct->tar = obn;
          }
        }

        if (cti->flush_constraint_targets) {
          cti->flush_constraint_targets(con, &targets, 0);
        }
      }
    }
  }
}

bool BKE_object_pose_context_check(const Object *ob)
{
  if ((ob) && (ob->type == OB_ARMATURE) && (ob->pose) && (ob->mode & OB_MODE_POSE)) {
    return true;
  }

  return false;
}

Object *BKE_object_pose_armature_get(Object *ob)
{
  if (ob == NULL) {
    return NULL;
  }

  if (BKE_object_pose_context_check(ob)) {
    return ob;
  }

  ob = BKE_modifiers_is_deformed_by_armature(ob);

  /* Only use selected check when non-active. */
  if (BKE_object_pose_context_check(ob)) {
    return ob;
  }

  return NULL;
}

Object *BKE_object_pose_armature_get_visible(Object *ob, ViewLayer *view_layer, View3D *v3d)
{
  Object *ob_armature = BKE_object_pose_armature_get(ob);
  if (ob_armature) {
    Base *base = BKE_view_layer_base_find(view_layer, ob_armature);
    if (base) {
      if (BASE_VISIBLE(v3d, base)) {
        return ob_armature;
      }
    }
  }
  return NULL;
}

/**
 * Access pose array with special check to get pose object when in weight paint mode.
 */
Object **BKE_object_pose_array_get_ex(ViewLayer *view_layer,
                                      View3D *v3d,
                                      uint *r_objects_len,
                                      bool unique)
{
  Object *ob_active = OBACT(view_layer);
  Object *ob_pose = BKE_object_pose_armature_get(ob_active);
  Object **objects = NULL;
  if (ob_pose == ob_active) {
    objects = BKE_view_layer_array_from_objects_in_mode(view_layer,
                                                        v3d,
                                                        r_objects_len,
                                                        {
                                                            .object_mode = OB_MODE_POSE,
                                                            .no_dup_data = unique,
                                                        });
  }
  else if (ob_pose != NULL) {
    *r_objects_len = 1;
    objects = MEM_mallocN(sizeof(*objects), __func__);
    objects[0] = ob_pose;
  }
  else {
    *r_objects_len = 0;
    objects = MEM_mallocN(0, __func__);
  }
  return objects;
}
Object **BKE_object_pose_array_get_unique(ViewLayer *view_layer, View3D *v3d, uint *r_objects_len)
{
  return BKE_object_pose_array_get_ex(view_layer, v3d, r_objects_len, true);
}
Object **BKE_object_pose_array_get(ViewLayer *view_layer, View3D *v3d, uint *r_objects_len)
{
  return BKE_object_pose_array_get_ex(view_layer, v3d, r_objects_len, false);
}

Base **BKE_object_pose_base_array_get_ex(ViewLayer *view_layer,
                                         View3D *v3d,
                                         uint *r_bases_len,
                                         bool unique)
{
  Base *base_active = BASACT(view_layer);
  Object *ob_pose = base_active ? BKE_object_pose_armature_get(base_active->object) : NULL;
  Base *base_pose = NULL;
  Base **bases = NULL;

  if (base_active) {
    if (ob_pose == base_active->object) {
      base_pose = base_active;
    }
    else {
      base_pose = BKE_view_layer_base_find(view_layer, ob_pose);
    }
  }

  if (base_active && (base_pose == base_active)) {
    bases = BKE_view_layer_array_from_bases_in_mode(view_layer,
                                                    v3d,
                                                    r_bases_len,
                                                    {
                                                        .object_mode = OB_MODE_POSE,
                                                        .no_dup_data = unique,
                                                    });
  }
  else if (base_pose != NULL) {
    *r_bases_len = 1;
    bases = MEM_mallocN(sizeof(*bases), __func__);
    bases[0] = base_pose;
  }
  else {
    *r_bases_len = 0;
    bases = MEM_mallocN(0, __func__);
  }
  return bases;
}
Base **BKE_object_pose_base_array_get_unique(ViewLayer *view_layer, View3D *v3d, uint *r_bases_len)
{
  return BKE_object_pose_base_array_get_ex(view_layer, v3d, r_bases_len, true);
}
Base **BKE_object_pose_base_array_get(ViewLayer *view_layer, View3D *v3d, uint *r_bases_len)
{
  return BKE_object_pose_base_array_get_ex(view_layer, v3d, r_bases_len, false);
}

void BKE_object_transform_copy(Object *ob_tar, const Object *ob_src)
{
  copy_v3_v3(ob_tar->loc, ob_src->loc);
  copy_v3_v3(ob_tar->rot, ob_src->rot);
  copy_v4_v4(ob_tar->quat, ob_src->quat);
  copy_v3_v3(ob_tar->rotAxis, ob_src->rotAxis);
  ob_tar->rotAngle = ob_src->rotAngle;
  ob_tar->rotmode = ob_src->rotmode;
  copy_v3_v3(ob_tar->scale, ob_src->scale);
}

/**
 * Perform deep-copy of object and its 'children' data-blocks (obdata, materials, actions, etc.).
 *
 * \param dupflag: Controls which sub-data are also duplicated
 * (see #eDupli_ID_Flags in DNA_userdef_types.h).
 *
 * \note This function does not do any remapping to new IDs, caller must do it
 * (\a #BKE_libblock_relink_to_newid()).
 * \note Caller MUST free \a newid pointers itself (#BKE_main_id_clear_newpoins()) and call updates
 * of DEG too (#DAG_relations_tag_update()).
 */
Object *BKE_object_duplicate(Main *bmain,
                             Object *ob,
                             eDupli_ID_Flags dupflag,
                             const eLibIDDuplicateFlags duplicate_options)
{
  const bool is_subprocess = (duplicate_options & LIB_ID_DUPLICATE_IS_SUBPROCESS) != 0;

  if (!is_subprocess) {
    BKE_main_id_tag_all(bmain, LIB_TAG_NEW, false);
    BKE_main_id_clear_newpoins(bmain);
    /* In case root duplicated ID is linked, assume we want to get a local copy of it and duplicate
     * all expected linked data. */
    if (ID_IS_LINKED(ob)) {
      dupflag |= USER_DUP_LINKED_ID;
    }
  }

  Material ***matarar;

  Object *obn = (Object *)BKE_id_copy(bmain, &ob->id);
  id_us_min(&obn->id);
  if (is_subprocess) {
    ID_NEW_SET(ob, obn);
  }

  /* 0 == full linked. */
  if (dupflag == 0) {
    return obn;
  }

  BKE_animdata_duplicate_id_action(bmain, &obn->id, dupflag);

  if (dupflag & USER_DUP_MAT) {
    for (int i = 0; i < obn->totcol; i++) {
      BKE_id_copy_for_duplicate(bmain, (ID *)obn->mat[i], dupflag);
    }
  }
  if (dupflag & USER_DUP_PSYS) {
    ParticleSystem *psys;
    for (psys = obn->particlesystem.first; psys; psys = psys->next) {
      BKE_id_copy_for_duplicate(bmain, (ID *)psys->part, dupflag);
    }
  }

  ID *id_old = obn->data;
  ID *id_new = NULL;
  const bool need_to_duplicate_obdata = (id_old != NULL) && (id_old->newid == NULL);

  switch (obn->type) {
    case OB_MESH:
      if (dupflag & USER_DUP_MESH) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_CURVE:
      if (dupflag & USER_DUP_CURVE) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_SURF:
      if (dupflag & USER_DUP_SURF) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_FONT:
      if (dupflag & USER_DUP_FONT) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_MBALL:
      if (dupflag & USER_DUP_MBALL) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_LAMP:
      if (dupflag & USER_DUP_LAMP) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_ARMATURE:
      if (dupflag & USER_DUP_ARM) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_LATTICE:
      if (dupflag != 0) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_CAMERA:
      if (dupflag != 0) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_LIGHTPROBE:
      if (dupflag & USER_DUP_LIGHTPROBE) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_SPEAKER:
      if (dupflag != 0) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_GPENCIL:
      if (dupflag & USER_DUP_GPENCIL) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_HAIR:
      if (dupflag & USER_DUP_HAIR) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_POINTCLOUD:
      if (dupflag & USER_DUP_POINTCLOUD) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
    case OB_VOLUME:
      if (dupflag & USER_DUP_VOLUME) {
        id_new = BKE_id_copy_for_duplicate(bmain, id_old, dupflag);
      }
      break;
  }

  /* If obdata has been copied, we may also have to duplicate the materials assigned to it. */
  if (need_to_duplicate_obdata && !ELEM(id_new, NULL, id_old)) {
    if (dupflag & USER_DUP_MAT) {
      matarar = BKE_object_material_array_p(obn);
      if (matarar) {
        for (int i = 0; i < obn->totcol; i++) {
          BKE_id_copy_for_duplicate(bmain, (ID *)(*matarar)[i], dupflag);
        }
      }
    }
  }

  if (!is_subprocess) {
    /* This code will follow into all ID links using an ID tagged with LIB_TAG_NEW.*/
    BKE_libblock_relink_to_newid(&obn->id);

#ifndef NDEBUG
    /* Call to `BKE_libblock_relink_to_newid` above is supposed to have cleared all those flags. */
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      BLI_assert((id_iter->tag & LIB_TAG_NEW) == 0);
    }
    FOREACH_MAIN_ID_END;
#endif

    /* Cleanup. */
    BKE_main_id_tag_all(bmain, LIB_TAG_NEW, false);
    BKE_main_id_clear_newpoins(bmain);
  }

  if (obn->type == OB_ARMATURE) {
    DEG_id_tag_update(&obn->id, ID_RECALC_GEOMETRY);
    if (obn->pose) {
      BKE_pose_tag_recalc(bmain, obn->pose);
    }
    //    BKE_pose_rebuild(bmain, obn, obn->data, true);
  }

  if (obn->data != NULL) {
    DEG_id_tag_update_ex(bmain, (ID *)obn->data, ID_RECALC_EDITORS);
  }

  return obn;
}

/**
 * Returns true if the Object is from an external blend file (libdata).
 */
bool BKE_object_is_libdata(const Object *ob)
{
  return (ob && ID_IS_LINKED(ob));
}

/**
 * Returns true if the Object data is from an external blend file (libdata).
 */
bool BKE_object_obdata_is_libdata(const Object *ob)
{
  /* Linked objects with local obdata are forbidden! */
  BLI_assert(!ob || !ob->data || (ID_IS_LINKED(ob) ? ID_IS_LINKED(ob->data) : true));
  return (ob && ob->data && ID_IS_LINKED(ob->data));
}

/* -------------------------------------------------------------------- */
/** \name Object Proxy API
 * \{ */

/* when you make proxy, ensure the exposed layers are extern */
static void armature_set_id_extern(Object *ob)
{
  bArmature *arm = ob->data;
  bPoseChannel *pchan;
  unsigned int lay = arm->layer_protected;

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (!(pchan->bone->layer & lay)) {
      id_lib_extern((ID *)pchan->custom);
    }
  }
}

void BKE_object_copy_proxy_drivers(Object *ob, Object *target)
{
  if ((target->adt) && (target->adt->drivers.first)) {
    FCurve *fcu;

    /* add new animdata block */
    if (!ob->adt) {
      ob->adt = BKE_animdata_add_id(&ob->id);
    }

    /* make a copy of all the drivers (for now), then correct any links that need fixing */
    BKE_fcurves_free(&ob->adt->drivers);
    BKE_fcurves_copy(&ob->adt->drivers, &target->adt->drivers);

    for (fcu = ob->adt->drivers.first; fcu; fcu = fcu->next) {
      ChannelDriver *driver = fcu->driver;
      DriverVar *dvar;

      for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
        /* all drivers */
        DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
          if (dtar->id) {
            if ((Object *)dtar->id == target) {
              dtar->id = (ID *)ob;
            }
            else {
              /* only on local objects because this causes indirect links
               * 'a -> b -> c', blend to point directly to a.blend
               * when a.blend has a proxy that's linked into c.blend  */
              if (!ID_IS_LINKED(ob)) {
                id_lib_extern((ID *)dtar->id);
              }
            }
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }
  }
}

/**
 * Proxy rule:
 * - lib_object->proxy_from == the one we borrow from, set temporally while object_update.
 * - local_object->proxy == pointer to library object, saved in files and read.
 * - local_object->proxy_group == pointer to collection dupli-object, saved in files and read.
 */
void BKE_object_make_proxy(Main *bmain, Object *ob, Object *target, Object *cob)
{
  /* paranoia checks */
  if (ID_IS_LINKED(ob) || !ID_IS_LINKED(target)) {
    CLOG_ERROR(&LOG, "cannot make proxy");
    return;
  }

  ob->proxy = target;
  id_us_plus(&target->id);
  ob->proxy_group = cob;

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  DEG_id_tag_update(&target->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

  /* copy transform
   * - cob means this proxy comes from a collection, just apply the matrix
   *   so the object wont move from its dupli-transform.
   *
   * - no cob means this is being made from a linked object,
   *   this is closer to making a copy of the object - in-place. */
  if (cob) {
    ob->rotmode = target->rotmode;
    mul_m4_m4m4(ob->obmat, cob->obmat, target->obmat);
    if (cob->instance_collection) { /* should always be true */
      float tvec[3];
      mul_v3_mat3_m4v3(tvec, ob->obmat, cob->instance_collection->instance_offset);
      sub_v3_v3(ob->obmat[3], tvec);
    }
    BKE_object_apply_mat4(ob, ob->obmat, false, true);
  }
  else {
    BKE_object_transform_copy(ob, target);
    ob->parent = target->parent; /* libdata */
    copy_m4_m4(ob->parentinv, target->parentinv);
  }

  /* copy animdata stuff - drivers only for now... */
  BKE_object_copy_proxy_drivers(ob, target);

  /* skip constraints? */
  /* FIXME: this is considered by many as a bug */

  /* set object type and link to data */
  ob->type = target->type;
  ob->data = target->data;
  id_us_plus((ID *)ob->data); /* ensures lib data becomes LIB_TAG_EXTERN */

  /* copy vertex groups */
  BKE_defgroup_copy_list(&ob->defbase, &target->defbase);

  /* copy material and index information */
  ob->actcol = ob->totcol = 0;
  if (ob->mat) {
    MEM_freeN(ob->mat);
  }
  if (ob->matbits) {
    MEM_freeN(ob->matbits);
  }
  ob->mat = NULL;
  ob->matbits = NULL;
  if ((target->totcol) && (target->mat) && OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
    int i;

    ob->actcol = target->actcol;
    ob->totcol = target->totcol;

    ob->mat = MEM_dupallocN(target->mat);
    ob->matbits = MEM_dupallocN(target->matbits);
    for (i = 0; i < target->totcol; i++) {
      /* don't need to run BKE_object_materials_test
       * since we know this object is new and not used elsewhere */
      id_us_plus((ID *)ob->mat[i]);
    }
  }

  /* type conversions */
  if (target->type == OB_ARMATURE) {
    copy_object_pose(ob, target, 0);             /* data copy, object pointers in constraints */
    BKE_pose_rest(ob->pose, false);              /* clear all transforms in channels */
    BKE_pose_rebuild(bmain, ob, ob->data, true); /* set all internal links */

    armature_set_id_extern(ob);
  }
  else if (target->type == OB_EMPTY) {
    ob->empty_drawtype = target->empty_drawtype;
    ob->empty_drawsize = target->empty_drawsize;
  }

  /* copy IDProperties */
  if (ob->id.properties) {
    IDP_FreeProperty(ob->id.properties);
    ob->id.properties = NULL;
  }
  if (target->id.properties) {
    ob->id.properties = IDP_CopyProperty(target->id.properties);
  }

  /* copy drawtype info */
  ob->dt = target->dt;
}

/**
 * Use with newly created objects to set their size
 * (used to apply scene-scale).
 */
void BKE_object_obdata_size_init(struct Object *ob, const float size)
{
  /* apply radius as a scale to types that support it */
  switch (ob->type) {
    case OB_EMPTY: {
      ob->empty_drawsize *= size;
      break;
    }
    case OB_FONT: {
      Curve *cu = ob->data;
      cu->fsize *= size;
      break;
    }
    case OB_CAMERA: {
      Camera *cam = ob->data;
      cam->drawsize *= size;
      break;
    }
    case OB_LAMP: {
      Light *lamp = ob->data;
      lamp->dist *= size;
      lamp->area_size *= size;
      lamp->area_sizey *= size;
      lamp->area_sizez *= size;
      break;
    }
    /* Only lattice (not mesh, curve, mball...),
     * because its got data when newly added */
    case OB_LATTICE: {
      struct Lattice *lt = ob->data;
      float mat[4][4];

      unit_m4(mat);
      scale_m4_fl(mat, size);

      BKE_lattice_transform(lt, (float(*)[4])mat, false);
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Matrix Get/Set API
 * \{ */

void BKE_object_scale_to_mat3(Object *ob, float mat[3][3])
{
  float vec[3];
  mul_v3_v3v3(vec, ob->scale, ob->dscale);
  size_to_mat3(mat, vec);
}

void BKE_object_rot_to_mat3(const Object *ob, float mat[3][3], bool use_drot)
{
  float rmat[3][3], dmat[3][3];

  /* 'dmat' is the delta-rotation matrix, which will get (pre)multiplied
   * with the rotation matrix to yield the appropriate rotation
   */

  /* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
  if (ob->rotmode > 0) {
    /* Euler rotations
     * (will cause gimble lock, but this can be alleviated a bit with rotation orders). */
    eulO_to_mat3(rmat, ob->rot, ob->rotmode);
    eulO_to_mat3(dmat, ob->drot, ob->rotmode);
  }
  else if (ob->rotmode == ROT_MODE_AXISANGLE) {
    /* axis-angle - not really that great for 3D-changing orientations */
    axis_angle_to_mat3(rmat, ob->rotAxis, ob->rotAngle);
    axis_angle_to_mat3(dmat, ob->drotAxis, ob->drotAngle);
  }
  else {
    /* quats are normalized before use to eliminate scaling issues */
    float tquat[4];

    normalize_qt_qt(tquat, ob->quat);
    quat_to_mat3(rmat, tquat);

    normalize_qt_qt(tquat, ob->dquat);
    quat_to_mat3(dmat, tquat);
  }

  /* combine these rotations */
  if (use_drot) {
    mul_m3_m3m3(mat, dmat, rmat);
  }
  else {
    copy_m3_m3(mat, rmat);
  }
}

void BKE_object_mat3_to_rot(Object *ob, float mat[3][3], bool use_compat)
{
  BLI_ASSERT_UNIT_M3(mat);

  switch (ob->rotmode) {
    case ROT_MODE_QUAT: {
      float dquat[4];
      mat3_normalized_to_quat(ob->quat, mat);
      normalize_qt_qt(dquat, ob->dquat);
      invert_qt_normalized(dquat);
      mul_qt_qtqt(ob->quat, dquat, ob->quat);
      break;
    }
    case ROT_MODE_AXISANGLE: {
      float quat[4];
      float dquat[4];

      /* without drot we could apply 'mat' directly */
      mat3_normalized_to_quat(quat, mat);
      axis_angle_to_quat(dquat, ob->drotAxis, ob->drotAngle);
      invert_qt_normalized(dquat);
      mul_qt_qtqt(quat, dquat, quat);
      quat_to_axis_angle(ob->rotAxis, &ob->rotAngle, quat);
      break;
    }
    default: /* euler */
    {
      float quat[4];
      float dquat[4];

      /* without drot we could apply 'mat' directly */
      mat3_normalized_to_quat(quat, mat);
      eulO_to_quat(dquat, ob->drot, ob->rotmode);
      invert_qt_normalized(dquat);
      mul_qt_qtqt(quat, dquat, quat);
      /* end drot correction */

      if (use_compat) {
        quat_to_compatible_eulO(ob->rot, ob->rot, ob->rotmode, quat);
      }
      else {
        quat_to_eulO(ob->rot, ob->rotmode, quat);
      }
      break;
    }
  }
}

void BKE_object_tfm_protected_backup(const Object *ob, ObjectTfmProtectedChannels *obtfm)
{

#define TFMCPY(_v) (obtfm->_v = ob->_v)
#define TFMCPY3D(_v) copy_v3_v3(obtfm->_v, ob->_v)
#define TFMCPY4D(_v) copy_v4_v4(obtfm->_v, ob->_v)

  TFMCPY3D(loc);
  TFMCPY3D(dloc);
  TFMCPY3D(scale);
  TFMCPY3D(dscale);
  TFMCPY3D(rot);
  TFMCPY3D(drot);
  TFMCPY4D(quat);
  TFMCPY4D(dquat);
  TFMCPY3D(rotAxis);
  TFMCPY3D(drotAxis);
  TFMCPY(rotAngle);
  TFMCPY(drotAngle);

#undef TFMCPY
#undef TFMCPY3D
#undef TFMCPY4D
}

void BKE_object_tfm_protected_restore(Object *ob,
                                      const ObjectTfmProtectedChannels *obtfm,
                                      const short protectflag)
{
  unsigned int i;

  for (i = 0; i < 3; i++) {
    if (protectflag & (OB_LOCK_LOCX << i)) {
      ob->loc[i] = obtfm->loc[i];
      ob->dloc[i] = obtfm->dloc[i];
    }

    if (protectflag & (OB_LOCK_SCALEX << i)) {
      ob->scale[i] = obtfm->scale[i];
      ob->dscale[i] = obtfm->dscale[i];
    }

    if (protectflag & (OB_LOCK_ROTX << i)) {
      ob->rot[i] = obtfm->rot[i];
      ob->drot[i] = obtfm->drot[i];

      ob->quat[i + 1] = obtfm->quat[i + 1];
      ob->dquat[i + 1] = obtfm->dquat[i + 1];

      ob->rotAxis[i] = obtfm->rotAxis[i];
      ob->drotAxis[i] = obtfm->drotAxis[i];
    }
  }

  if ((protectflag & OB_LOCK_ROT4D) && (protectflag & OB_LOCK_ROTW)) {
    ob->quat[0] = obtfm->quat[0];
    ob->dquat[0] = obtfm->dquat[0];

    ob->rotAngle = obtfm->rotAngle;
    ob->drotAngle = obtfm->drotAngle;
  }
}

void BKE_object_tfm_copy(Object *object_dst, const Object *object_src)
{
#define TFMCPY(_v) (object_dst->_v = object_src->_v)
#define TFMCPY3D(_v) copy_v3_v3(object_dst->_v, object_src->_v)
#define TFMCPY4D(_v) copy_v4_v4(object_dst->_v, object_src->_v)

  TFMCPY3D(loc);
  TFMCPY3D(dloc);
  TFMCPY3D(scale);
  TFMCPY3D(dscale);
  TFMCPY3D(rot);
  TFMCPY3D(drot);
  TFMCPY4D(quat);
  TFMCPY4D(dquat);
  TFMCPY3D(rotAxis);
  TFMCPY3D(drotAxis);
  TFMCPY(rotAngle);
  TFMCPY(drotAngle);

#undef TFMCPY
#undef TFMCPY3D
#undef TFMCPY4D
}

void BKE_object_to_mat3(Object *ob, float r_mat[3][3]) /* no parent */
{
  float smat[3][3];
  float rmat[3][3];
  /*float q1[4];*/

  /* scale */
  BKE_object_scale_to_mat3(ob, smat);

  /* rot */
  BKE_object_rot_to_mat3(ob, rmat, true);
  mul_m3_m3m3(r_mat, rmat, smat);
}

void BKE_object_to_mat4(Object *ob, float r_mat[4][4])
{
  float tmat[3][3];

  BKE_object_to_mat3(ob, tmat);

  copy_m4_m3(r_mat, tmat);

  add_v3_v3v3(r_mat[3], ob->loc, ob->dloc);
}

void BKE_object_matrix_local_get(struct Object *ob, float r_mat[4][4])
{
  if (ob->parent) {
    float par_imat[4][4];

    BKE_object_get_parent_matrix(ob, ob->parent, par_imat);
    invert_m4(par_imat);
    mul_m4_m4m4(r_mat, par_imat, ob->obmat);
  }
  else {
    copy_m4_m4(r_mat, ob->obmat);
  }
}

/**
 * \return success if \a mat is set.
 */
static bool ob_parcurve(Object *ob, Object *par, float r_mat[4][4])
{
  Curve *cu = par->data;
  float vec[4], dir[3], quat[4], radius, ctime;

  /* NOTE: Curve cache is supposed to be evaluated here already, however there
   * are cases where we can not guarantee that. This includes, for example,
   * dependency cycles. We can't correct anything from here, since that would
   * cause a threading conflicts.
   *
   * TODO(sergey): Some of the legit looking cases like T56619 need to be
   * looked into, and maybe curve cache (and other dependencies) are to be
   * evaluated prior to conversion. */
  if (par->runtime.curve_cache == NULL) {
    return false;
  }
  if (par->runtime.curve_cache->path == NULL) {
    return false;
  }

  /* ctime is now a proper var setting of Curve which gets set by Animato like any other var
   * that's animated, but this will only work if it actually is animated.
   *
   * We divide the curve-time calculated in the previous step by the length of the path,
   * to get a time factor, which then gets clamped to lie within 0.0 - 1.0 range.
   */
  if (cu->pathlen) {
    ctime = cu->ctime / cu->pathlen;
  }
  else {
    ctime = cu->ctime;
  }
  CLAMP(ctime, 0.0f, 1.0f);

  unit_m4(r_mat);

  /* vec: 4 items! */
  if (where_on_path(par, ctime, vec, dir, (cu->flag & CU_FOLLOW) ? quat : NULL, &radius, NULL)) {
    if (cu->flag & CU_FOLLOW) {
      quat_apply_track(quat, ob->trackflag, ob->upflag);
      normalize_qt(quat);
      quat_to_mat4(r_mat, quat);
    }
    if (cu->flag & CU_PATH_RADIUS) {
      float tmat[4][4], rmat[4][4];
      scale_m4_fl(tmat, radius);
      mul_m4_m4m4(rmat, tmat, r_mat);
      copy_m4_m4(r_mat, rmat);
    }
    copy_v3_v3(r_mat[3], vec);
  }

  return true;
}

static void ob_parbone(Object *ob, Object *par, float r_mat[4][4])
{
  bPoseChannel *pchan;
  float vec[3];

  if (par->type != OB_ARMATURE) {
    unit_m4(r_mat);
    return;
  }

  /* Make sure the bone is still valid */
  pchan = BKE_pose_channel_find_name(par->pose, ob->parsubstr);
  if (!pchan || !pchan->bone) {
    CLOG_ERROR(
        &LOG, "Object %s with Bone parent: bone %s doesn't exist", ob->id.name + 2, ob->parsubstr);
    unit_m4(r_mat);
    return;
  }

  /* get bone transform */
  if (pchan->bone->flag & BONE_RELATIVE_PARENTING) {
    /* the new option uses the root - expected behavior, but differs from old... */
    /* XXX check on version patching? */
    copy_m4_m4(r_mat, pchan->chan_mat);
  }
  else {
    copy_m4_m4(r_mat, pchan->pose_mat);

    /* but for backwards compatibility, the child has to move to the tail */
    copy_v3_v3(vec, r_mat[1]);
    mul_v3_fl(vec, pchan->bone->length);
    add_v3_v3(r_mat[3], vec);
  }
}

static void give_parvert(Object *par, int nr, float vec[3])
{
  zero_v3(vec);

  if (par->type == OB_MESH) {
    Mesh *me = par->data;
    BMEditMesh *em = me->edit_mesh;
    Mesh *me_eval = (em) ? em->mesh_eval_final : BKE_object_get_evaluated_mesh(par);

    if (me_eval) {
      int count = 0;
      int numVerts = me_eval->totvert;

      if (em && me_eval->runtime.wrapper_type == ME_WRAPPER_TYPE_BMESH) {
        numVerts = em->bm->totvert;
        if (em->bm->elem_table_dirty & BM_VERT) {
#ifdef VPARENT_THREADING_HACK
          BLI_mutex_lock(&vparent_lock);
          if (em->bm->elem_table_dirty & BM_VERT) {
            BM_mesh_elem_table_ensure(em->bm, BM_VERT);
          }
          BLI_mutex_unlock(&vparent_lock);
#else
          BLI_assert(!"Not safe for threading");
          BM_mesh_elem_table_ensure(em->bm, BM_VERT);
#endif
        }
        if (nr < numVerts) {
          if (me_eval && me_eval->runtime.edit_data && me_eval->runtime.edit_data->vertexCos) {
            add_v3_v3(vec, me_eval->runtime.edit_data->vertexCos[nr]);
          }
          else {
            const BMVert *v = BM_vert_at_index(em->bm, nr);
            add_v3_v3(vec, v->co);
          }
          count++;
        }
      }
      else if (CustomData_has_layer(&me_eval->vdata, CD_ORIGINDEX)) {
        const int *index = CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX);
        /* Get the average of all verts with (original index == nr). */
        for (int i = 0; i < numVerts; i++) {
          if (index[i] == nr) {
            add_v3_v3(vec, me_eval->mvert[i].co);
            count++;
          }
        }
      }
      else {
        if (nr < numVerts) {
          add_v3_v3(vec, me_eval->mvert[nr].co);
          count++;
        }
      }

      if (count == 0) {
        /* keep as 0, 0, 0 */
      }
      else if (count > 0) {
        mul_v3_fl(vec, 1.0f / count);
      }
      else {
        /* use first index if its out of range */
        if (me_eval->totvert) {
          copy_v3_v3(vec, me_eval->mvert[0].co);
        }
      }
    }
    else {
      CLOG_ERROR(&LOG,
                 "Evaluated mesh is needed to solve parenting, "
                 "object position can be wrong now");
    }
  }
  else if (ELEM(par->type, OB_CURVE, OB_SURF)) {
    ListBase *nurb;

    /* Unless there's some weird depsgraph failure the cache should exist. */
    BLI_assert(par->runtime.curve_cache != NULL);

    if (par->runtime.curve_cache->deformed_nurbs.first != NULL) {
      nurb = &par->runtime.curve_cache->deformed_nurbs;
    }
    else {
      Curve *cu = par->data;
      nurb = BKE_curve_nurbs_get(cu);
    }

    BKE_nurbList_index_get_co(nurb, nr, vec);
  }
  else if (par->type == OB_LATTICE) {
    Lattice *latt = par->data;
    DispList *dl = par->runtime.curve_cache ?
                       BKE_displist_find(&par->runtime.curve_cache->disp, DL_VERTS) :
                       NULL;
    float(*co)[3] = dl ? (float(*)[3])dl->verts : NULL;
    int tot;

    if (latt->editlatt) {
      latt = latt->editlatt->latt;
    }

    tot = latt->pntsu * latt->pntsv * latt->pntsw;

    /* ensure dl is correct size */
    BLI_assert(dl == NULL || dl->nr == tot);

    if (nr < tot) {
      if (co) {
        copy_v3_v3(vec, co[nr]);
      }
      else {
        copy_v3_v3(vec, latt->def[nr].vec);
      }
    }
  }
}

static void ob_parvert3(Object *ob, Object *par, float r_mat[4][4])
{

  /* in local ob space */
  if (OB_TYPE_SUPPORT_PARVERT(par->type)) {
    float cmat[3][3], v1[3], v2[3], v3[3], q[4];

    give_parvert(par, ob->par1, v1);
    give_parvert(par, ob->par2, v2);
    give_parvert(par, ob->par3, v3);

    tri_to_quat(q, v1, v2, v3);
    quat_to_mat3(cmat, q);
    copy_m4_m3(r_mat, cmat);

    mid_v3_v3v3v3(r_mat[3], v1, v2, v3);
  }
  else {
    unit_m4(r_mat);
  }
}

void BKE_object_get_parent_matrix(Object *ob, Object *par, float r_parentmat[4][4])
{
  float tmat[4][4];
  float vec[3];
  bool ok;

  switch (ob->partype & PARTYPE) {
    case PAROBJECT:
      ok = 0;
      if (par->type == OB_CURVE) {
        if ((((Curve *)par->data)->flag & CU_PATH) && (ob_parcurve(ob, par, tmat))) {
          ok = 1;
        }
      }

      if (ok) {
        mul_m4_m4m4(r_parentmat, par->obmat, tmat);
      }
      else {
        copy_m4_m4(r_parentmat, par->obmat);
      }

      break;
    case PARBONE:
      ob_parbone(ob, par, tmat);
      mul_m4_m4m4(r_parentmat, par->obmat, tmat);
      break;

    case PARVERT1:
      unit_m4(r_parentmat);
      give_parvert(par, ob->par1, vec);
      mul_v3_m4v3(r_parentmat[3], par->obmat, vec);
      break;
    case PARVERT3:
      ob_parvert3(ob, par, tmat);

      mul_m4_m4m4(r_parentmat, par->obmat, tmat);
      break;

    case PARSKEL:
      copy_m4_m4(r_parentmat, par->obmat);
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Matrix Evaluation API
 * \{ */

/**
 * \param r_originmat: Optional matrix that stores the space the object is in
 * (without its own matrix applied)
 */
static void solve_parenting(
    Object *ob, Object *par, const bool set_origin, float r_obmat[4][4], float r_originmat[3][3])
{
  float totmat[4][4];
  float tmat[4][4];
  float locmat[4][4];

  BKE_object_to_mat4(ob, locmat);

  BKE_object_get_parent_matrix(ob, par, totmat);

  /* total */
  mul_m4_m4m4(tmat, totmat, ob->parentinv);
  mul_m4_m4m4(r_obmat, tmat, locmat);

  if (r_originmat) {
    /* usable originmat */
    copy_m3_m4(r_originmat, tmat);
  }

  /* origin, for help line */
  if (set_origin) {
    if ((ob->partype & PARTYPE) == PARSKEL) {
      copy_v3_v3(ob->runtime.parent_display_origin, par->obmat[3]);
    }
    else {
      copy_v3_v3(ob->runtime.parent_display_origin, totmat[3]);
    }
  }
}

/**
 * \note scene is the active scene while actual_scene is the scene the object resides in.
 */
static void object_where_is_calc_ex(Depsgraph *depsgraph,
                                    Scene *scene,
                                    Object *ob,
                                    float ctime,
                                    RigidBodyWorld *rbw,
                                    float r_originmat[3][3])
{
  if (ob->parent) {
    Object *par = ob->parent;

    /* calculate parent matrix */
    solve_parenting(ob, par, true, ob->obmat, r_originmat);
  }
  else {
    BKE_object_to_mat4(ob, ob->obmat);
  }

  /* try to fall back to the scene rigid body world if none given */
  rbw = rbw ? rbw : scene->rigidbody_world;
  /* read values pushed into RBO from sim/cache... */
  BKE_rigidbody_sync_transforms(rbw, ob, ctime);

  /* solve constraints */
  if (ob->constraints.first && !(ob->transflag & OB_NO_CONSTRAINTS)) {
    bConstraintOb *cob;
    cob = BKE_constraints_make_evalob(depsgraph, scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
    BKE_constraints_solve(depsgraph, &ob->constraints, cob, ctime);
    BKE_constraints_clear_evalob(cob);
  }

  /* set negative scale flag in object */
  if (is_negative_m4(ob->obmat)) {
    ob->transflag |= OB_NEG_SCALE;
  }
  else {
    ob->transflag &= ~OB_NEG_SCALE;
  }
}

void BKE_object_where_is_calc_time(Depsgraph *depsgraph, Scene *scene, Object *ob, float ctime)
{
  /* Execute drivers and animation. */
  const bool flush_to_original = DEG_is_active(depsgraph);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    ctime);
  BKE_animsys_evaluate_animdata(
      &ob->id, ob->adt, &anim_eval_context, ADT_RECALC_ALL, flush_to_original);
  object_where_is_calc_ex(depsgraph, scene, ob, ctime, NULL, NULL);
}

/**
 * Calculate object transformation matrix without recalculating dependencies and
 * constraints -- assume dependencies are already solved by depsgraph.
 * No changes to object and its parent would be done.
 * Used for bundles orientation in 3d space relative to parented blender camera.
 */
void BKE_object_where_is_calc_mat4(Object *ob, float r_obmat[4][4])
{
  if (ob->parent) {
    Object *par = ob->parent;
    solve_parenting(ob, par, false, r_obmat, NULL);
  }
  else {
    BKE_object_to_mat4(ob, r_obmat);
  }
}

void BKE_object_where_is_calc_ex(
    Depsgraph *depsgraph, Scene *scene, RigidBodyWorld *rbw, Object *ob, float r_originmat[3][3])
{
  float ctime = DEG_get_ctime(depsgraph);
  object_where_is_calc_ex(depsgraph, scene, ob, ctime, rbw, r_originmat);
}
void BKE_object_where_is_calc(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  float ctime = DEG_get_ctime(depsgraph);
  object_where_is_calc_ex(depsgraph, scene, ob, ctime, NULL, NULL);
}

/**
 * For calculation of the inverse parent transform, only used for editor.
 *
 * It assumes the object parent is already in the depsgraph.
 * Otherwise, after changing ob->parent you need to call:
 * - #DEG_relations_tag_update(bmain);
 * - #BKE_scene_graph_update_tagged(depsgraph, bmain);
 */
void BKE_object_workob_calc_parent(Depsgraph *depsgraph, Scene *scene, Object *ob, Object *workob)
{
  BKE_object_workob_clear(workob);

  unit_m4(workob->obmat);
  unit_m4(workob->parentinv);
  unit_m4(workob->constinv);

  /* Since this is used while calculating parenting,
   * at this moment ob_eval->parent is still NULL. */
  workob->parent = DEG_get_evaluated_object(depsgraph, ob->parent);

  workob->trackflag = ob->trackflag;
  workob->upflag = ob->upflag;

  workob->partype = ob->partype;
  workob->par1 = ob->par1;
  workob->par2 = ob->par2;
  workob->par3 = ob->par3;

  workob->constraints = ob->constraints;

  BLI_strncpy(workob->parsubstr, ob->parsubstr, sizeof(workob->parsubstr));

  BKE_object_where_is_calc(depsgraph, scene, workob);
}

/**
 * Applies the global transformation \a mat to the \a ob using a relative parent space if
 * supplied.
 *
 * \param mat: the global transformation mat that the object should be set object to.
 * \param parent: the parent space in which this object will be set relative to
 * (should probably always be parent_eval).
 * \param use_compat: true to ensure that rotations are set using the
 * min difference between the old and new orientation.
 */
void BKE_object_apply_mat4_ex(Object *ob,
                              const float mat[4][4],
                              Object *parent,
                              const float parentinv[4][4],
                              const bool use_compat)
{
  /* see BKE_pchan_apply_mat4() for the equivalent 'pchan' function */

  float rot[3][3];

  if (parent != NULL) {
    float rmat[4][4], diff_mat[4][4], imat[4][4], parent_mat[4][4];

    BKE_object_get_parent_matrix(ob, parent, parent_mat);

    mul_m4_m4m4(diff_mat, parent_mat, parentinv);
    invert_m4_m4(imat, diff_mat);
    mul_m4_m4m4(rmat, imat, mat); /* get the parent relative matrix */

    /* same as below, use rmat rather than mat */
    mat4_to_loc_rot_size(ob->loc, rot, ob->scale, rmat);
  }
  else {
    mat4_to_loc_rot_size(ob->loc, rot, ob->scale, mat);
  }

  BKE_object_mat3_to_rot(ob, rot, use_compat);

  sub_v3_v3(ob->loc, ob->dloc);

  if (ob->dscale[0] != 0.0f) {
    ob->scale[0] /= ob->dscale[0];
  }
  if (ob->dscale[1] != 0.0f) {
    ob->scale[1] /= ob->dscale[1];
  }
  if (ob->dscale[2] != 0.0f) {
    ob->scale[2] /= ob->dscale[2];
  }

  /* BKE_object_mat3_to_rot handles delta rotations */
}

/**
 * XXX: should be removed after COW operators port to use BKE_object_apply_mat4_ex directly.
 */
void BKE_object_apply_mat4(Object *ob,
                           const float mat[4][4],
                           const bool use_compat,
                           const bool use_parent)
{
  BKE_object_apply_mat4_ex(ob, mat, use_parent ? ob->parent : NULL, ob->parentinv, use_compat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Bounding Box API
 * \{ */

BoundBox *BKE_boundbox_alloc_unit(void)
{
  BoundBox *bb;
  const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};

  bb = MEM_callocN(sizeof(BoundBox), "OB-BoundBox");
  BKE_boundbox_init_from_minmax(bb, min, max);

  return bb;
}

void BKE_boundbox_init_from_minmax(BoundBox *bb, const float min[3], const float max[3])
{
  bb->vec[0][0] = bb->vec[1][0] = bb->vec[2][0] = bb->vec[3][0] = min[0];
  bb->vec[4][0] = bb->vec[5][0] = bb->vec[6][0] = bb->vec[7][0] = max[0];

  bb->vec[0][1] = bb->vec[1][1] = bb->vec[4][1] = bb->vec[5][1] = min[1];
  bb->vec[2][1] = bb->vec[3][1] = bb->vec[6][1] = bb->vec[7][1] = max[1];

  bb->vec[0][2] = bb->vec[3][2] = bb->vec[4][2] = bb->vec[7][2] = min[2];
  bb->vec[1][2] = bb->vec[2][2] = bb->vec[5][2] = bb->vec[6][2] = max[2];
}

void BKE_boundbox_calc_center_aabb(const BoundBox *bb, float r_cent[3])
{
  r_cent[0] = 0.5f * (bb->vec[0][0] + bb->vec[4][0]);
  r_cent[1] = 0.5f * (bb->vec[0][1] + bb->vec[2][1]);
  r_cent[2] = 0.5f * (bb->vec[0][2] + bb->vec[1][2]);
}

void BKE_boundbox_calc_size_aabb(const BoundBox *bb, float r_size[3])
{
  r_size[0] = 0.5f * fabsf(bb->vec[0][0] - bb->vec[4][0]);
  r_size[1] = 0.5f * fabsf(bb->vec[0][1] - bb->vec[2][1]);
  r_size[2] = 0.5f * fabsf(bb->vec[0][2] - bb->vec[1][2]);
}

void BKE_boundbox_minmax(const BoundBox *bb,
                         const float obmat[4][4],
                         float r_min[3],
                         float r_max[3])
{
  int i;
  for (i = 0; i < 8; i++) {
    float vec[3];
    mul_v3_m4v3(vec, obmat, bb->vec[i]);
    minmax_v3v3_v3(r_min, r_max, vec);
  }
}

BoundBox *BKE_object_boundbox_get(Object *ob)
{
  BoundBox *bb = NULL;

  switch (ob->type) {
    case OB_MESH:
      bb = BKE_mesh_boundbox_get(ob);
      break;
    case OB_CURVE:
    case OB_SURF:
    case OB_FONT:
      bb = BKE_curve_boundbox_get(ob);
      break;
    case OB_MBALL:
      bb = BKE_mball_boundbox_get(ob);
      break;
    case OB_LATTICE:
      bb = BKE_lattice_boundbox_get(ob);
      break;
    case OB_ARMATURE:
      bb = BKE_armature_boundbox_get(ob);
      break;
    case OB_GPENCIL:
      bb = BKE_gpencil_boundbox_get(ob);
      break;
    case OB_HAIR:
      bb = BKE_hair_boundbox_get(ob);
      break;
    case OB_POINTCLOUD:
      bb = BKE_pointcloud_boundbox_get(ob);
      break;
    case OB_VOLUME:
      bb = BKE_volume_boundbox_get(ob);
      break;
    default:
      break;
  }
  return bb;
}

/**
 * Use this to temporally disable/enable bound-box.
 */
void BKE_object_boundbox_flag(Object *ob, int flag, const bool set)
{
  BoundBox *bb = BKE_object_boundbox_get(ob);
  if (bb) {
    if (set) {
      bb->flag |= flag;
    }
    else {
      bb->flag &= ~flag;
    }
  }
}

void BKE_object_boundbox_calc_from_mesh(struct Object *ob, struct Mesh *me_eval)
{
  float min[3], max[3];

  INIT_MINMAX(min, max);

  if (!BKE_mesh_wrapper_minmax(me_eval, min, max)) {
    zero_v3(min);
    zero_v3(max);
  }

  if (ob->runtime.bb == NULL) {
    ob->runtime.bb = MEM_callocN(sizeof(BoundBox), "DM-BoundBox");
  }

  BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);

  ob->runtime.bb->flag &= ~BOUNDBOX_DIRTY;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Dimension Get/Set
 *
 * \warning Setting dimensions is prone to feedback loops in evaluation.
 * \{ */

void BKE_object_dimensions_get(Object *ob, float r_vec[3])
{
  BoundBox *bb = NULL;

  bb = BKE_object_boundbox_get(ob);
  if (bb) {
    float scale[3];

    mat4_to_size(scale, ob->obmat);

    r_vec[0] = fabsf(scale[0]) * (bb->vec[4][0] - bb->vec[0][0]);
    r_vec[1] = fabsf(scale[1]) * (bb->vec[2][1] - bb->vec[0][1]);
    r_vec[2] = fabsf(scale[2]) * (bb->vec[1][2] - bb->vec[0][2]);
  }
  else {
    zero_v3(r_vec);
  }
}

/**
 * The original scale and object matrix can be passed in so any difference
 * of the objects matrix and the final matrix can be accounted for,
 * typically this caused by parenting, constraints or delta-scale.
 *
 * Re-using these values from the object causes a feedback loop
 * when multiple values are modified at once in some situations. see: T69536.
 */
void BKE_object_dimensions_set_ex(Object *ob,
                                  const float value[3],
                                  int axis_mask,
                                  const float ob_scale_orig[3],
                                  const float ob_obmat_orig[4][4])
{
  BoundBox *bb = NULL;

  bb = BKE_object_boundbox_get(ob);
  if (bb) {
    float len[3];

    len[0] = bb->vec[4][0] - bb->vec[0][0];
    len[1] = bb->vec[2][1] - bb->vec[0][1];
    len[2] = bb->vec[1][2] - bb->vec[0][2];

    for (int i = 0; i < 3; i++) {
      if (((1 << i) & axis_mask) == 0) {

        if (ob_scale_orig != NULL) {
          const float scale_delta = len_v3(ob_obmat_orig[i]) / ob_scale_orig[i];
          if (isfinite(scale_delta)) {
            len[i] *= scale_delta;
          }
        }

        const float scale = copysignf(value[i] / len[i], ob->scale[i]);
        if (isfinite(scale)) {
          ob->scale[i] = scale;
        }
      }
    }
  }
}

void BKE_object_dimensions_set(Object *ob, const float value[3], int axis_mask)
{
  BKE_object_dimensions_set_ex(ob, value, axis_mask, NULL, NULL);
}

void BKE_object_minmax(Object *ob, float r_min[3], float r_max[3], const bool use_hidden)
{
  BoundBox bb;
  float vec[3];
  bool changed = false;

  switch (ob->type) {
    case OB_CURVE:
    case OB_FONT:
    case OB_SURF: {
      bb = *BKE_curve_boundbox_get(ob);
      BKE_boundbox_minmax(&bb, ob->obmat, r_min, r_max);
      changed = true;
      break;
    }
    case OB_MESH: {
      bb = *BKE_mesh_boundbox_get(ob);
      BKE_boundbox_minmax(&bb, ob->obmat, r_min, r_max);
      changed = true;
      break;
    }
    case OB_GPENCIL: {
      bb = *BKE_gpencil_boundbox_get(ob);
      BKE_boundbox_minmax(&bb, ob->obmat, r_min, r_max);
      changed = true;
      break;
    }
    case OB_LATTICE: {
      Lattice *lt = ob->data;
      BPoint *bp = lt->def;
      int u, v, w;

      for (w = 0; w < lt->pntsw; w++) {
        for (v = 0; v < lt->pntsv; v++) {
          for (u = 0; u < lt->pntsu; u++, bp++) {
            mul_v3_m4v3(vec, ob->obmat, bp->vec);
            minmax_v3v3_v3(r_min, r_max, vec);
          }
        }
      }
      changed = true;
      break;
    }
    case OB_ARMATURE: {
      changed = BKE_pose_minmax(ob, r_min, r_max, use_hidden, false);
      break;
    }
    case OB_MBALL: {
      float ob_min[3], ob_max[3];

      changed = BKE_mball_minmax_ex(ob->data, ob_min, ob_max, ob->obmat, 0);
      if (changed) {
        minmax_v3v3_v3(r_min, r_max, ob_min);
        minmax_v3v3_v3(r_min, r_max, ob_max);
      }
      break;
    }
    case OB_HAIR: {
      bb = *BKE_hair_boundbox_get(ob);
      BKE_boundbox_minmax(&bb, ob->obmat, r_min, r_max);
      changed = true;
      break;
    }

    case OB_POINTCLOUD: {
      bb = *BKE_pointcloud_boundbox_get(ob);
      BKE_boundbox_minmax(&bb, ob->obmat, r_min, r_max);
      changed = true;
      break;
    }
    case OB_VOLUME: {
      bb = *BKE_volume_boundbox_get(ob);
      BKE_boundbox_minmax(&bb, ob->obmat, r_min, r_max);
      changed = true;
      break;
    }
  }

  if (changed == false) {
    float size[3];

    copy_v3_v3(size, ob->scale);
    if (ob->type == OB_EMPTY) {
      mul_v3_fl(size, ob->empty_drawsize);
    }

    minmax_v3v3_v3(r_min, r_max, ob->obmat[3]);

    copy_v3_v3(vec, ob->obmat[3]);
    add_v3_v3(vec, size);
    minmax_v3v3_v3(r_min, r_max, vec);

    copy_v3_v3(vec, ob->obmat[3]);
    sub_v3_v3(vec, size);
    minmax_v3v3_v3(r_min, r_max, vec);
  }
}

void BKE_object_empty_draw_type_set(Object *ob, const int value)
{
  ob->empty_drawtype = value;

  if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
    if (!ob->iuser) {
      ob->iuser = MEM_callocN(sizeof(ImageUser), "image user");
      ob->iuser->ok = 1;
      ob->iuser->flag |= IMA_ANIM_ALWAYS;
      ob->iuser->frames = 100;
      ob->iuser->sfra = 1;
    }
  }
  else {
    if (ob->iuser) {
      MEM_freeN(ob->iuser);
      ob->iuser = NULL;
    }
  }
}

bool BKE_object_empty_image_frame_is_visible_in_view3d(const Object *ob, const RegionView3D *rv3d)
{
  const char visibility_flag = ob->empty_image_visibility_flag;
  if (rv3d->is_persp) {
    return (visibility_flag & OB_EMPTY_IMAGE_HIDE_PERSPECTIVE) == 0;
  }

  return (visibility_flag & OB_EMPTY_IMAGE_HIDE_ORTHOGRAPHIC) == 0;
}

bool BKE_object_empty_image_data_is_visible_in_view3d(const Object *ob, const RegionView3D *rv3d)
{
  /* Caller is expected to check this. */
  BLI_assert(BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d));

  const char visibility_flag = ob->empty_image_visibility_flag;

  if ((visibility_flag & (OB_EMPTY_IMAGE_HIDE_BACK | OB_EMPTY_IMAGE_HIDE_FRONT)) != 0) {
    float eps, dot;
    if (rv3d->is_persp) {
      /* Note, we could normalize the 'view_dir' then use 'eps'
       * however the issue with empty objects being visible when viewed from the side
       * is only noticeable in orthographic views. */
      float view_dir[3];
      sub_v3_v3v3(view_dir, rv3d->viewinv[3], ob->obmat[3]);
      dot = dot_v3v3(ob->obmat[2], view_dir);
      eps = 0.0f;
    }
    else {
      dot = dot_v3v3(ob->obmat[2], rv3d->viewinv[2]);
      eps = 1e-5f;
    }
    if (visibility_flag & OB_EMPTY_IMAGE_HIDE_BACK) {
      if (dot < eps) {
        return false;
      }
    }
    if (visibility_flag & OB_EMPTY_IMAGE_HIDE_FRONT) {
      if (dot > -eps) {
        return false;
      }
    }
  }

  if (visibility_flag & OB_EMPTY_IMAGE_HIDE_NON_AXIS_ALIGNED) {
    float proj[3];
    project_plane_v3_v3v3(proj, ob->obmat[2], rv3d->viewinv[2]);
    const float proj_length_sq = len_squared_v3(proj);
    if (proj_length_sq > 1e-5f) {
      return false;
    }
  }

  return true;
}

bool BKE_object_minmax_dupli(Depsgraph *depsgraph,
                             Scene *scene,
                             Object *ob,
                             float r_min[3],
                             float r_max[3],
                             const bool use_hidden)
{
  bool ok = false;
  if ((ob->transflag & OB_DUPLI) == 0) {
    return ok;
  }

  ListBase *lb;
  DupliObject *dob;
  lb = object_duplilist(depsgraph, scene, ob);
  for (dob = lb->first; dob; dob = dob->next) {
    if ((use_hidden == false) && (dob->no_draw != 0)) {
      /* pass */
    }
    else {
      BoundBox *bb = BKE_object_boundbox_get(dob->ob);

      if (bb) {
        int i;
        for (i = 0; i < 8; i++) {
          float vec[3];
          mul_v3_m4v3(vec, dob->mat, bb->vec[i]);
          minmax_v3v3_v3(r_min, r_max, vec);
        }

        ok = true;
      }
    }
  }
  free_object_duplilist(lb); /* does restore */

  return ok;
}

void BKE_object_foreach_display_point(Object *ob,
                                      const float obmat[4][4],
                                      void (*func_cb)(const float[3], void *),
                                      void *user_data)
{
  /* TODO: pointcloud and hair objects support */
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  float co[3];

  if (mesh_eval != NULL) {
    const MVert *mv = mesh_eval->mvert;
    const int totvert = mesh_eval->totvert;
    for (int i = 0; i < totvert; i++, mv++) {
      mul_v3_m4v3(co, obmat, mv->co);
      func_cb(co, user_data);
    }
  }
  else if (ob->runtime.curve_cache && ob->runtime.curve_cache->disp.first) {
    DispList *dl;

    for (dl = ob->runtime.curve_cache->disp.first; dl; dl = dl->next) {
      const float *v3 = dl->verts;
      int totvert = dl->nr;
      int i;

      for (i = 0; i < totvert; i++, v3 += 3) {
        mul_v3_m4v3(co, obmat, v3);
        func_cb(co, user_data);
      }
    }
  }
}

void BKE_scene_foreach_display_point(Depsgraph *depsgraph,
                                     void (*func_cb)(const float[3], void *),
                                     void *user_data)
{
  DEG_OBJECT_ITER_BEGIN (depsgraph,
                         ob,
                         DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_VISIBLE |
                             DEG_ITER_OBJECT_FLAG_DUPLI) {
    if ((ob->base_flag & BASE_SELECTED) != 0) {
      BKE_object_foreach_display_point(ob, ob->obmat, func_cb, user_data);
    }
  }
  DEG_OBJECT_ITER_END;
}

/**
 * Struct members from DNA_object_types.h
 */
typedef struct ObTfmBack {
  float loc[3], dloc[3];
  /** scale and delta scale. */
  float scale[3], dscale[3];
  /** euler rotation. */
  float rot[3], drot[3];
  /** quaternion rotation. */
  float quat[4], dquat[4];
  /** axis angle rotation - axis part. */
  float rotAxis[3], drotAxis[3];
  /** axis angle rotation - angle part. */
  float rotAngle, drotAngle;
  /** final worldspace matrix with constraints & animsys applied. */
  float obmat[4][4];
  /** inverse result of parent, so that object doesn't 'stick' to parent. */
  float parentinv[4][4];
  /** inverse result of constraints. doesn't include effect of parent or object local transform.
   */
  float constinv[4][4];
  /** inverse matrix of 'obmat' for during render, temporally: ipokeys of transform. */
  float imat[4][4];
} ObTfmBack;

void *BKE_object_tfm_backup(Object *ob)
{
  ObTfmBack *obtfm = MEM_mallocN(sizeof(ObTfmBack), "ObTfmBack");
  copy_v3_v3(obtfm->loc, ob->loc);
  copy_v3_v3(obtfm->dloc, ob->dloc);
  copy_v3_v3(obtfm->scale, ob->scale);
  copy_v3_v3(obtfm->dscale, ob->dscale);
  copy_v3_v3(obtfm->rot, ob->rot);
  copy_v3_v3(obtfm->drot, ob->drot);
  copy_qt_qt(obtfm->quat, ob->quat);
  copy_qt_qt(obtfm->dquat, ob->dquat);
  copy_v3_v3(obtfm->rotAxis, ob->rotAxis);
  copy_v3_v3(obtfm->drotAxis, ob->drotAxis);
  obtfm->rotAngle = ob->rotAngle;
  obtfm->drotAngle = ob->drotAngle;
  copy_m4_m4(obtfm->obmat, ob->obmat);
  copy_m4_m4(obtfm->parentinv, ob->parentinv);
  copy_m4_m4(obtfm->constinv, ob->constinv);
  copy_m4_m4(obtfm->imat, ob->imat);

  return (void *)obtfm;
}

void BKE_object_tfm_restore(Object *ob, void *obtfm_pt)
{
  ObTfmBack *obtfm = (ObTfmBack *)obtfm_pt;
  copy_v3_v3(ob->loc, obtfm->loc);
  copy_v3_v3(ob->dloc, obtfm->dloc);
  copy_v3_v3(ob->scale, obtfm->scale);
  copy_v3_v3(ob->dscale, obtfm->dscale);
  copy_v3_v3(ob->rot, obtfm->rot);
  copy_v3_v3(ob->drot, obtfm->drot);
  copy_qt_qt(ob->quat, obtfm->quat);
  copy_qt_qt(ob->dquat, obtfm->dquat);
  copy_v3_v3(ob->rotAxis, obtfm->rotAxis);
  copy_v3_v3(ob->drotAxis, obtfm->drotAxis);
  ob->rotAngle = obtfm->rotAngle;
  ob->drotAngle = obtfm->drotAngle;
  copy_m4_m4(ob->obmat, obtfm->obmat);
  copy_m4_m4(ob->parentinv, obtfm->parentinv);
  copy_m4_m4(ob->constinv, obtfm->constinv);
  copy_m4_m4(ob->imat, obtfm->imat);
}

bool BKE_object_parent_loop_check(const Object *par, const Object *ob)
{
  /* test if 'ob' is a parent somewhere in par's parents */
  if (par == NULL) {
    return false;
  }
  if (ob == par) {
    return true;
  }
  return BKE_object_parent_loop_check(par->parent, ob);
}

static void object_handle_update_proxy(Depsgraph *depsgraph,
                                       Scene *scene,
                                       Object *object,
                                       const bool do_proxy_update)
{
  /* The case when this is a collection proxy, object_update is called in collection.c */
  if (object->proxy == NULL) {
    return;
  }
  /* set pointer in library proxy target, for copying, but restore it */
  object->proxy->proxy_from = object;
  // printf("set proxy pointer for later collection stuff %s\n", ob->id.name);

  /* the no-group proxy case, we call update */
  if (object->proxy_group == NULL) {
    if (do_proxy_update) {
      // printf("call update, lib ob %s proxy %s\n", ob->proxy->id.name, ob->id.name);
      BKE_object_handle_update(depsgraph, scene, object->proxy);
    }
  }
}

/**
 * Proxy rule:
 * - lib_object->proxy_from == the one we borrow from, only set temporal and cleared here.
 * - local_object->proxy    == pointer to library object, saved in files and read.
 *
 * Function below is polluted with proxy exceptions, cleanup will follow!
 *
 * The main object update call, for object matrix, constraints, keys and displist (modifiers)
 * requires flags to be set!
 *
 * Ideally we shouldn't have to pass the rigid body world,
 * but need bigger restructuring to avoid id.
 */
void BKE_object_handle_update_ex(Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob,
                                 RigidBodyWorld *rbw,
                                 const bool do_proxy_update)
{
  const ID *object_data = ob->data;
  const bool recalc_object = (ob->id.recalc & ID_RECALC_ALL) != 0;
  const bool recalc_data = (object_data != NULL) ? ((object_data->recalc & ID_RECALC_ALL) != 0) :
                                                   0;
  if (!recalc_object && !recalc_data) {
    object_handle_update_proxy(depsgraph, scene, ob, do_proxy_update);
    return;
  }
  /* Speed optimization for animation lookups. */
  if (ob->pose != NULL) {
    BKE_pose_channels_hash_make(ob->pose);
    if (ob->pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
      BKE_pose_update_constraint_flags(ob->pose);
    }
  }
  if (recalc_data) {
    if (ob->type == OB_ARMATURE) {
      /* this happens for reading old files and to match library armatures
       * with poses we do it ahead of BKE_object_where_is_calc to ensure animation
       * is evaluated on the rebuilt pose, otherwise we get incorrect poses
       * on file load */
      if (ob->pose == NULL || (ob->pose->flag & POSE_RECALC)) {
        /* No need to pass bmain here, we assume we do not need to rebuild DEG from here... */
        BKE_pose_rebuild(NULL, ob, ob->data, true);
      }
    }
  }
  /* XXX new animsys warning: depsgraph tag ID_RECALC_GEOMETRY should not skip drivers,
   * which is only in BKE_object_where_is_calc now */
  /* XXX: should this case be ID_RECALC_TRANSFORM instead? */
  if (recalc_object || recalc_data) {
    if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) {
      printf("recalcob %s\n", ob->id.name + 2);
    }
    /* Handle proxy copy for target. */
    if (!BKE_object_eval_proxy_copy(depsgraph, ob)) {
      BKE_object_where_is_calc_ex(depsgraph, scene, rbw, ob, NULL);
    }
  }

  if (recalc_data) {
    BKE_object_handle_data_update(depsgraph, scene, ob);
  }

  ob->id.recalc &= ID_RECALC_ALL;

  object_handle_update_proxy(depsgraph, scene, ob, do_proxy_update);
}

/**
 * \warning "scene" here may not be the scene object actually resides in.
 * When dealing with background-sets, "scene" is actually the active scene.
 * e.g. "scene" <-- set 1 <-- set 2 ("ob" lives here) <-- set 3 <-- ... <-- set n
 * rigid bodies depend on their world so use #BKE_object_handle_update_ex()
 * to also pass along the current rigid body world.
 */
void BKE_object_handle_update(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  BKE_object_handle_update_ex(depsgraph, scene, ob, NULL, true);
}

void BKE_object_sculpt_data_create(Object *ob)
{
  BLI_assert((ob->sculpt == NULL) && (ob->mode & OB_MODE_ALL_SCULPT));
  ob->sculpt = MEM_callocN(sizeof(SculptSession), __func__);
  ob->sculpt->mode_type = ob->mode;
}

bool BKE_object_obdata_texspace_get(Object *ob, short **r_texflag, float **r_loc, float **r_size)
{

  if (ob->data == NULL) {
    return false;
  }

  switch (GS(((ID *)ob->data)->name)) {
    case ID_ME: {
      BKE_mesh_texspace_get_reference((Mesh *)ob->data, r_texflag, r_loc, r_size);
      break;
    }
    case ID_CU: {
      Curve *cu = ob->data;
      BKE_curve_texspace_ensure(cu);
      if (r_texflag) {
        *r_texflag = &cu->texflag;
      }
      if (r_loc) {
        *r_loc = cu->loc;
      }
      if (r_size) {
        *r_size = cu->size;
      }
      break;
    }
    case ID_MB: {
      MetaBall *mb = ob->data;
      if (r_texflag) {
        *r_texflag = &mb->texflag;
      }
      if (r_loc) {
        *r_loc = mb->loc;
      }
      if (r_size) {
        *r_size = mb->size;
      }
      break;
    }
    default:
      return false;
  }
  return true;
}

/** Get evaluated mesh for given object. */
Mesh *BKE_object_get_evaluated_mesh(Object *object)
{
  ID *data_eval = object->runtime.data_eval;
  return (data_eval && GS(data_eval->name) == ID_ME) ? (Mesh *)data_eval : NULL;
}

/**
 * Get mesh which is not affected by modifiers:
 * - For original objects it will be same as `object->data`, and it is a mesh
 *   which is in the corresponding #Main.
 * - For copied-on-write objects it will give pointer to a copied-on-write
 *   mesh which corresponds to original object's mesh.
 */
Mesh *BKE_object_get_pre_modified_mesh(Object *object)
{
  if (object->type == OB_MESH && object->runtime.data_orig != NULL) {
    BLI_assert(object->id.tag & LIB_TAG_COPIED_ON_WRITE);
    BLI_assert(object->id.orig_id != NULL);
    BLI_assert(object->runtime.data_orig->orig_id == ((Object *)object->id.orig_id)->data);
    Mesh *result = (Mesh *)object->runtime.data_orig;
    BLI_assert((result->id.tag & LIB_TAG_COPIED_ON_WRITE) != 0);
    BLI_assert((result->id.tag & LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT) == 0);
    return result;
  }
  BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0);
  return object->data;
}

/**
 * Get a mesh which corresponds to very very original mesh from #Main.
 * - For original objects it will be object->data.
 * - For evaluated objects it will be same mesh as corresponding original
 *   object uses as data.
 */
Mesh *BKE_object_get_original_mesh(Object *object)
{
  Mesh *result = NULL;
  if (object->id.orig_id == NULL) {
    BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0);
    result = object->data;
  }
  else {
    BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) != 0);
    result = ((Object *)object->id.orig_id)->data;
  }
  BLI_assert(result != NULL);
  BLI_assert((result->id.tag & (LIB_TAG_COPIED_ON_WRITE | LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT)) ==
             0);
  return result;
}

static int pc_cmp(const void *a, const void *b)
{
  const LinkData *ad = a, *bd = b;
  if (POINTER_AS_INT(ad->data) > POINTER_AS_INT(bd->data)) {
    return 1;
  }

  return 0;
}

int BKE_object_insert_ptcache(Object *ob)
{
  LinkData *link = NULL;
  int i = 0;

  BLI_listbase_sort(&ob->pc_ids, pc_cmp);

  for (link = ob->pc_ids.first, i = 0; link; link = link->next, i++) {
    int index = POINTER_AS_INT(link->data);

    if (i < index) {
      break;
    }
  }

  link = MEM_callocN(sizeof(LinkData), "PCLink");
  link->data = POINTER_FROM_INT(i);
  BLI_addtail(&ob->pc_ids, link);

  return i;
}

static int pc_findindex(ListBase *listbase, int index)
{
  LinkData *link = NULL;
  int number = 0;

  if (listbase == NULL) {
    return -1;
  }

  link = listbase->first;
  while (link) {
    if (POINTER_AS_INT(link->data) == index) {
      return number;
    }

    number++;
    link = link->next;
  }

  return -1;
}

void BKE_object_delete_ptcache(Object *ob, int index)
{
  int list_index = pc_findindex(&ob->pc_ids, index);
  LinkData *link = BLI_findlink(&ob->pc_ids, list_index);
  BLI_freelinkN(&ob->pc_ids, link);
}

/* -------------------------------------------------------------------- */
/** \name Object Data Shape Key Insert
 * \{ */

/** Mesh */
static KeyBlock *insert_meshkey(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
  Mesh *me = ob->data;
  Key *key = me->key;
  KeyBlock *kb;
  int newkey = 0;

  if (key == NULL) {
    key = me->key = BKE_key_add(bmain, (ID *)me);
    key->type = KEY_RELATIVE;
    newkey = 1;
  }

  if (newkey || from_mix == false) {
    /* create from mesh */
    kb = BKE_keyblock_add_ctime(key, name, false);
    BKE_keyblock_convert_from_mesh(me, key, kb);
  }
  else {
    /* copy from current values */
    int totelem;
    float *data = BKE_key_evaluate_object(ob, &totelem);

    /* create new block with prepared data */
    kb = BKE_keyblock_add_ctime(key, name, false);
    kb->data = data;
    kb->totelem = totelem;
  }

  return kb;
}
/** Lattice */
static KeyBlock *insert_lattkey(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
  Lattice *lt = ob->data;
  Key *key = lt->key;
  KeyBlock *kb;
  int newkey = 0;

  if (key == NULL) {
    key = lt->key = BKE_key_add(bmain, (ID *)lt);
    key->type = KEY_RELATIVE;
    newkey = 1;
  }

  if (newkey || from_mix == false) {
    kb = BKE_keyblock_add_ctime(key, name, false);
    if (!newkey) {
      KeyBlock *basekb = (KeyBlock *)key->block.first;
      kb->data = MEM_dupallocN(basekb->data);
      kb->totelem = basekb->totelem;
    }
    else {
      BKE_keyblock_convert_from_lattice(lt, kb);
    }
  }
  else {
    /* copy from current values */
    int totelem;
    float *data = BKE_key_evaluate_object(ob, &totelem);

    /* create new block with prepared data */
    kb = BKE_keyblock_add_ctime(key, name, false);
    kb->totelem = totelem;
    kb->data = data;
  }

  return kb;
}
/** Curve */
static KeyBlock *insert_curvekey(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
  Curve *cu = ob->data;
  Key *key = cu->key;
  KeyBlock *kb;
  ListBase *lb = BKE_curve_nurbs_get(cu);
  int newkey = 0;

  if (key == NULL) {
    key = cu->key = BKE_key_add(bmain, (ID *)cu);
    key->type = KEY_RELATIVE;
    newkey = 1;
  }

  if (newkey || from_mix == false) {
    /* create from curve */
    kb = BKE_keyblock_add_ctime(key, name, false);
    if (!newkey) {
      KeyBlock *basekb = (KeyBlock *)key->block.first;
      kb->data = MEM_dupallocN(basekb->data);
      kb->totelem = basekb->totelem;
    }
    else {
      BKE_keyblock_convert_from_curve(cu, kb, lb);
    }
  }
  else {
    /* copy from current values */
    int totelem;
    float *data = BKE_key_evaluate_object(ob, &totelem);

    /* create new block with prepared data */
    kb = BKE_keyblock_add_ctime(key, name, false);
    kb->totelem = totelem;
    kb->data = data;
  }

  return kb;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Shape Key API
 * \{ */

KeyBlock *BKE_object_shapekey_insert(Main *bmain,
                                     Object *ob,
                                     const char *name,
                                     const bool from_mix)
{
  KeyBlock *key = NULL;

  switch (ob->type) {
    case OB_MESH:
      key = insert_meshkey(bmain, ob, name, from_mix);
      break;
    case OB_CURVE:
    case OB_SURF:
      key = insert_curvekey(bmain, ob, name, from_mix);
      break;
    case OB_LATTICE:
      key = insert_lattkey(bmain, ob, name, from_mix);
      break;
    default:
      break;
  }

  /* Set the first active when none is set when called from RNA. */
  if (key != NULL) {
    if (ob->shapenr <= 0) {
      ob->shapenr = 1;
    }
  }

  return key;
}

bool BKE_object_shapekey_free(Main *bmain, Object *ob)
{
  Key **key_p, *key;

  key_p = BKE_key_from_object_p(ob);
  if (ELEM(NULL, key_p, *key_p)) {
    return false;
  }

  key = *key_p;
  *key_p = NULL;

  BKE_id_free_us(bmain, key);

  return true;
}

bool BKE_object_shapekey_remove(Main *bmain, Object *ob, KeyBlock *kb)
{
  KeyBlock *rkb;
  Key *key = BKE_key_from_object(ob);
  short kb_index;

  if (key == NULL) {
    return false;
  }

  kb_index = BLI_findindex(&key->block, kb);
  BLI_assert(kb_index != -1);

  for (rkb = key->block.first; rkb; rkb = rkb->next) {
    if (rkb->relative == kb_index) {
      /* remap to the 'Basis' */
      rkb->relative = 0;
    }
    else if (rkb->relative >= kb_index) {
      /* Fix positional shift of the keys when kb is deleted from the list */
      rkb->relative -= 1;
    }
  }

  BLI_remlink(&key->block, kb);
  key->totkey--;
  if (key->refkey == kb) {
    key->refkey = key->block.first;

    if (key->refkey) {
      /* apply new basis key on original data */
      switch (ob->type) {
        case OB_MESH:
          BKE_keyblock_convert_to_mesh(key->refkey, ob->data);
          break;
        case OB_CURVE:
        case OB_SURF:
          BKE_keyblock_convert_to_curve(key->refkey, ob->data, BKE_curve_nurbs_get(ob->data));
          break;
        case OB_LATTICE:
          BKE_keyblock_convert_to_lattice(key->refkey, ob->data);
          break;
      }
    }
  }

  if (kb->data) {
    MEM_freeN(kb->data);
  }
  MEM_freeN(kb);

  /* Unset active when all are freed. */
  if (BLI_listbase_is_empty(&key->block)) {
    ob->shapenr = 0;
  }
  else if (ob->shapenr > 1) {
    ob->shapenr--;
  }

  if (key->totkey == 0) {
    BKE_object_shapekey_free(bmain, ob);
  }

  return true;
}

/** \} */

bool BKE_object_flag_test_recursive(const Object *ob, short flag)
{
  if (ob->flag & flag) {
    return true;
  }
  if (ob->parent) {
    return BKE_object_flag_test_recursive(ob->parent, flag);
  }

  return false;
}

bool BKE_object_is_child_recursive(const Object *ob_parent, const Object *ob_child)
{
  for (ob_child = ob_child->parent; ob_child; ob_child = ob_child->parent) {
    if (ob_child == ob_parent) {
      return true;
    }
  }
  return false;
}

/**
 * Most important if this is modified it should _always_ return true, in certain
 * cases false positives are hard to avoid (shape keys for example).
 */
int BKE_object_is_modified(Scene *scene, Object *ob)
{
  /* Always test on original object since evaluated object may no longer
   * have shape keys or modifiers that were used to evaluate it. */
  ob = DEG_get_original_object(ob);

  int flag = 0;

  if (BKE_key_from_object(ob)) {
    flag |= eModifierMode_Render | eModifierMode_Realtime;
  }
  else {
    ModifierData *md;
    VirtualModifierData virtualModifierData;
    /* cloth */
    for (md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
         md && (flag != (eModifierMode_Render | eModifierMode_Realtime));
         md = md->next) {
      if ((flag & eModifierMode_Render) == 0 &&
          BKE_modifier_is_enabled(scene, md, eModifierMode_Render)) {
        flag |= eModifierMode_Render;
      }

      if ((flag & eModifierMode_Realtime) == 0 &&
          BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        flag |= eModifierMode_Realtime;
      }
    }
  }

  return flag;
}

/**
 * Check of objects moves in time.
 *
 * \note This function is currently optimized for usage in combination
 * with modifier deformation checks (#eModifierTypeType_OnlyDeform),
 * so modifiers can quickly check if their target objects moves
 * (causing deformation motion blur) or not.
 *
 * This makes it possible to give some degree of false-positives here,
 * but it's currently an acceptable tradeoff between complexity and check
 * speed. In combination with checks of modifier stack and real life usage
 * percentage of false-positives shouldn't be that high.
 *
 * \note This function does not consider physics systems.
 */
bool BKE_object_moves_in_time(const Object *object, bool recurse_parent)
{
  /* If object has any sort of animation data assume it is moving. */
  if (BKE_animdata_id_is_animated(&object->id)) {
    return true;
  }
  if (!BLI_listbase_is_empty(&object->constraints)) {
    return true;
  }
  if (recurse_parent && object->parent != NULL) {
    return BKE_object_moves_in_time(object->parent, true);
  }
  return false;
}

static bool object_moves_in_time(const Object *object)
{
  return BKE_object_moves_in_time(object, true);
}

static bool object_deforms_in_time(Object *object)
{
  if (BKE_key_from_object(object) != NULL) {
    return true;
  }
  if (!BLI_listbase_is_empty(&object->modifiers)) {
    return true;
  }
  return object_moves_in_time(object);
}

static bool constructive_modifier_is_deform_modified(ModifierData *md)
{
  /* TODO(sergey): Consider generalizing this a bit so all modifier logic
   * is concentrated in MOD_{modifier}.c file,
   */
  if (md->type == eModifierType_Array) {
    ArrayModifierData *amd = (ArrayModifierData *)md;
    /* TODO(sergey): Check if curve is deformed. */
    return (amd->start_cap != NULL && object_moves_in_time(amd->start_cap)) ||
           (amd->end_cap != NULL && object_moves_in_time(amd->end_cap)) ||
           (amd->curve_ob != NULL && object_moves_in_time(amd->curve_ob)) ||
           (amd->offset_ob != NULL && object_moves_in_time(amd->offset_ob));
  }
  if (md->type == eModifierType_Mirror) {
    MirrorModifierData *mmd = (MirrorModifierData *)md;
    return mmd->mirror_ob != NULL && object_moves_in_time(mmd->mirror_ob);
  }
  if (md->type == eModifierType_Screw) {
    ScrewModifierData *smd = (ScrewModifierData *)md;
    return smd->ob_axis != NULL && object_moves_in_time(smd->ob_axis);
  }
  if (md->type == eModifierType_MeshSequenceCache) {
    /* NOTE: Not ideal because it's unknown whether topology changes or not.
     * This will be detected later, so by assuming it's only deformation
     * going on here we allow to bake deform-only mesh to Alembic and have
     * proper motion blur after that.
     */
    return true;
  }
  return false;
}

static bool modifiers_has_animation_check(const Object *ob)
{
  /* TODO(sergey): This is a bit code duplication with depsgraph, but
   * would be nicer to solve this as a part of new dependency graph
   * work, so we avoid conflicts and so.
   */
  if (ob->adt != NULL) {
    AnimData *adt = ob->adt;
    FCurve *fcu;
    if (adt->action != NULL) {
      for (fcu = adt->action->curves.first; fcu; fcu = fcu->next) {
        if (fcu->rna_path && strstr(fcu->rna_path, "modifiers[")) {
          return true;
        }
      }
    }
    for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
      if (fcu->rna_path && strstr(fcu->rna_path, "modifiers[")) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Test if object is affected by deforming modifiers (for motion blur). again
 * most important is to avoid false positives, this is to skip computations
 * and we can still if there was actual deformation afterwards.
 */
int BKE_object_is_deform_modified(Scene *scene, Object *ob)
{
  /* Always test on original object since evaluated object may no longer
   * have shape keys or modifiers that were used to evaluate it. */
  ob = DEG_get_original_object(ob);

  ModifierData *md;
  VirtualModifierData virtualModifierData;
  int flag = 0;
  const bool is_modifier_animated = modifiers_has_animation_check(ob);

  if (BKE_key_from_object(ob)) {
    flag |= eModifierMode_Realtime | eModifierMode_Render;
  }

  if (ob->type == OB_CURVE) {
    Curve *cu = (Curve *)ob->data;
    if (cu->taperobj != NULL && object_deforms_in_time(cu->taperobj)) {
      flag |= eModifierMode_Realtime | eModifierMode_Render;
    }
  }

  /* cloth */
  for (md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);
       md && (flag != (eModifierMode_Render | eModifierMode_Realtime));
       md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
    bool can_deform = mti->type == eModifierTypeType_OnlyDeform || is_modifier_animated;

    if (!can_deform) {
      can_deform = constructive_modifier_is_deform_modified(md);
    }

    if (can_deform) {
      if (!(flag & eModifierMode_Render) &&
          BKE_modifier_is_enabled(scene, md, eModifierMode_Render)) {
        flag |= eModifierMode_Render;
      }

      if (!(flag & eModifierMode_Realtime) &&
          BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        flag |= eModifierMode_Realtime;
      }
    }
  }

  return flag;
}

/** Return the number of scenes using (instantiating) that object in their collections. */
int BKE_object_scenes_users_get(Main *bmain, Object *ob)
{
  int num_scenes = 0;
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    if (BKE_collection_has_object_recursive(scene->master_collection, ob)) {
      num_scenes++;
    }
  }
  return num_scenes;
}

MovieClip *BKE_object_movieclip_get(Scene *scene, Object *ob, bool use_default)
{
  MovieClip *clip = use_default ? scene->clip : NULL;
  bConstraint *con = ob->constraints.first, *scon = NULL;

  while (con) {
    if (con->type == CONSTRAINT_TYPE_CAMERASOLVER) {
      if (scon == NULL || (scon->flag & CONSTRAINT_OFF)) {
        scon = con;
      }
    }

    con = con->next;
  }

  if (scon) {
    bCameraSolverConstraint *solver = scon->data;
    if ((solver->flag & CAMERASOLVER_ACTIVECLIP) == 0) {
      clip = solver->clip;
    }
    else {
      clip = scene->clip;
    }
  }

  return clip;
}

void BKE_object_runtime_reset(Object *object)
{
  memset(&object->runtime, 0, sizeof(object->runtime));
}

/**
 * Reset all pointers which we don't want to be shared when copying the object.
 */
void BKE_object_runtime_reset_on_copy(Object *object, const int UNUSED(flag))
{
  Object_Runtime *runtime = &object->runtime;
  runtime->data_eval = NULL;
  runtime->mesh_deform_eval = NULL;
  runtime->curve_cache = NULL;
}

/**
 * Find an associated armature object.
 */
static Object *obrel_armature_find(Object *ob)
{
  Object *ob_arm = NULL;

  if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
    ob_arm = ob->parent;
  }
  else {
    ModifierData *mod;
    for (mod = (ModifierData *)ob->modifiers.first; mod; mod = mod->next) {
      if (mod->type == eModifierType_Armature) {
        ob_arm = ((ArmatureModifierData *)mod)->object;
      }
    }
  }

  return ob_arm;
}

static bool obrel_list_test(Object *ob)
{
  return ob && !(ob->id.tag & LIB_TAG_DOIT);
}

static void obrel_list_add(LinkNode **links, Object *ob)
{
  BLI_linklist_prepend(links, ob);
  ob->id.tag |= LIB_TAG_DOIT;
}

/**
 * Iterates over all objects of the given scene layer.
 * Depending on the #eObjectSet flag:
 * collect either #OB_SET_ALL, #OB_SET_VISIBLE or #OB_SET_SELECTED objects.
 * If #OB_SET_VISIBLE or#OB_SET_SELECTED are collected,
 * then also add related objects according to the given \a includeFilter.
 */
LinkNode *BKE_object_relational_superset(struct ViewLayer *view_layer,
                                         eObjectSet objectSet,
                                         eObRelationTypes includeFilter)
{
  LinkNode *links = NULL;

  Base *base;

  /* Remove markers from all objects */
  for (base = view_layer->object_bases.first; base; base = base->next) {
    base->object->id.tag &= ~LIB_TAG_DOIT;
  }

  /* iterate over all selected and visible objects */
  for (base = view_layer->object_bases.first; base; base = base->next) {
    if (objectSet == OB_SET_ALL) {
      /* as we get all anyways just add it */
      Object *ob = base->object;
      obrel_list_add(&links, ob);
    }
    else {
      if ((objectSet == OB_SET_SELECTED && BASE_SELECTED_EDITABLE(((View3D *)NULL), base)) ||
          (objectSet == OB_SET_VISIBLE && BASE_EDITABLE(((View3D *)NULL), base))) {
        Object *ob = base->object;

        if (obrel_list_test(ob)) {
          obrel_list_add(&links, ob);
        }

        /* parent relationship */
        if (includeFilter & (OB_REL_PARENT | OB_REL_PARENT_RECURSIVE)) {
          Object *parent = ob->parent;
          if (obrel_list_test(parent)) {

            obrel_list_add(&links, parent);

            /* recursive parent relationship */
            if (includeFilter & OB_REL_PARENT_RECURSIVE) {
              parent = parent->parent;
              while (obrel_list_test(parent)) {

                obrel_list_add(&links, parent);
                parent = parent->parent;
              }
            }
          }
        }

        /* child relationship */
        if (includeFilter & (OB_REL_CHILDREN | OB_REL_CHILDREN_RECURSIVE)) {
          Base *local_base;
          for (local_base = view_layer->object_bases.first; local_base;
               local_base = local_base->next) {
            if (BASE_EDITABLE(((View3D *)NULL), local_base)) {

              Object *child = local_base->object;
              if (obrel_list_test(child)) {
                if ((includeFilter & OB_REL_CHILDREN_RECURSIVE &&
                     BKE_object_is_child_recursive(ob, child)) ||
                    (includeFilter & OB_REL_CHILDREN && child->parent && child->parent == ob)) {
                  obrel_list_add(&links, child);
                }
              }
            }
          }
        }

        /* include related armatures */
        if (includeFilter & OB_REL_MOD_ARMATURE) {
          Object *arm = obrel_armature_find(ob);
          if (obrel_list_test(arm)) {
            obrel_list_add(&links, arm);
          }
        }
      }
    }
  }

  return links;
}

/**
 * return all groups this object is a part of, caller must free.
 */
struct LinkNode *BKE_object_groups(Main *bmain, Scene *scene, Object *ob)
{
  LinkNode *collection_linknode = NULL;
  Collection *collection = NULL;
  while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
    BLI_linklist_prepend(&collection_linknode, collection);
  }

  return collection_linknode;
}

void BKE_object_groups_clear(Main *bmain, Scene *scene, Object *ob)
{
  Collection *collection = NULL;
  while ((collection = BKE_collection_object_find(bmain, scene, collection, ob))) {
    BKE_collection_object_remove(bmain, collection, ob, false);
    DEG_id_tag_update(&collection->id, ID_RECALC_COPY_ON_WRITE);
  }
}

/**
 * Return a KDTree_3d from the deformed object (in worldspace)
 *
 * \note Only mesh objects currently support deforming, others are TODO.
 *
 * \param ob:
 * \param r_tot:
 * \return The kdtree or NULL if it can't be created.
 */
KDTree_3d *BKE_object_as_kdtree(Object *ob, int *r_tot)
{
  KDTree_3d *tree = NULL;
  unsigned int tot = 0;

  switch (ob->type) {
    case OB_MESH: {
      Mesh *me = ob->data;
      unsigned int i;

      Mesh *me_eval = ob->runtime.mesh_deform_eval ? ob->runtime.mesh_deform_eval :
                                                     ob->runtime.mesh_deform_eval;
      const int *index;

      if (me_eval && (index = CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX))) {
        MVert *mvert = me_eval->mvert;
        uint totvert = me_eval->totvert;

        /* tree over-allocs in case where some verts have ORIGINDEX_NONE */
        tot = 0;
        tree = BLI_kdtree_3d_new(totvert);

        /* we don't how how many verts from the DM we can use */
        for (i = 0; i < totvert; i++) {
          if (index[i] != ORIGINDEX_NONE) {
            float co[3];
            mul_v3_m4v3(co, ob->obmat, mvert[i].co);
            BLI_kdtree_3d_insert(tree, index[i], co);
            tot++;
          }
        }
      }
      else {
        MVert *mvert = me->mvert;

        tot = me->totvert;
        tree = BLI_kdtree_3d_new(tot);

        for (i = 0; i < tot; i++) {
          float co[3];
          mul_v3_m4v3(co, ob->obmat, mvert[i].co);
          BLI_kdtree_3d_insert(tree, i, co);
        }
      }

      BLI_kdtree_3d_balance(tree);
      break;
    }
    case OB_CURVE:
    case OB_SURF: {
      /* TODO: take deformation into account */
      Curve *cu = ob->data;
      unsigned int i, a;

      Nurb *nu;

      tot = BKE_nurbList_verts_count_without_handles(&cu->nurb);
      tree = BLI_kdtree_3d_new(tot);
      i = 0;

      nu = cu->nurb.first;
      while (nu) {
        if (nu->bezt) {
          BezTriple *bezt;

          bezt = nu->bezt;
          a = nu->pntsu;
          while (a--) {
            float co[3];
            mul_v3_m4v3(co, ob->obmat, bezt->vec[1]);
            BLI_kdtree_3d_insert(tree, i++, co);
            bezt++;
          }
        }
        else {
          BPoint *bp;

          bp = nu->bp;
          a = nu->pntsu * nu->pntsv;
          while (a--) {
            float co[3];
            mul_v3_m4v3(co, ob->obmat, bp->vec);
            BLI_kdtree_3d_insert(tree, i++, co);
            bp++;
          }
        }
        nu = nu->next;
      }

      BLI_kdtree_3d_balance(tree);
      break;
    }
    case OB_LATTICE: {
      /* TODO: take deformation into account */
      Lattice *lt = ob->data;
      BPoint *bp;
      unsigned int i;

      tot = lt->pntsu * lt->pntsv * lt->pntsw;
      tree = BLI_kdtree_3d_new(tot);
      i = 0;

      for (bp = lt->def; i < tot; bp++) {
        float co[3];
        mul_v3_m4v3(co, ob->obmat, bp->vec);
        BLI_kdtree_3d_insert(tree, i++, co);
      }

      BLI_kdtree_3d_balance(tree);
      break;
    }
  }

  *r_tot = tot;
  return tree;
}

bool BKE_object_modifier_use_time(Object *ob, ModifierData *md)
{
  if (BKE_modifier_depends_ontime(md)) {
    return true;
  }

  /* Check whether modifier is animated. */
  /* TODO: this should be handled as part of build_animdata() -- Aligorith */
  if (ob->adt) {
    AnimData *adt = ob->adt;
    FCurve *fcu;

    char pattern[MAX_NAME + 16];
    BLI_snprintf(pattern, sizeof(pattern), "modifiers[\"%s\"]", md->name);

    /* action - check for F-Curves with paths containing 'modifiers[' */
    if (adt->action) {
      for (fcu = (FCurve *)adt->action->curves.first; fcu != NULL; fcu = (FCurve *)fcu->next) {
        if (fcu->rna_path && strstr(fcu->rna_path, pattern)) {
          return true;
        }
      }
    }

    /* This here allows modifier properties to get driven and still update properly
     *
     * Workaround to get T26764 (e.g. subsurf levels not updating when animated/driven)
     * working, without the updating problems (T28525 T28690 T28774 T28777) caused
     * by the RNA updates cache introduced in r.38649
     */
    for (fcu = (FCurve *)adt->drivers.first; fcu != NULL; fcu = (FCurve *)fcu->next) {
      if (fcu->rna_path && strstr(fcu->rna_path, pattern)) {
        return true;
      }
    }

    /* XXX: also, should check NLA strips, though for now assume that nobody uses
     * that and we can omit that for performance reasons... */
  }

  return false;
}

bool BKE_object_modifier_gpencil_use_time(Object *ob, GpencilModifierData *md)
{
  if (BKE_gpencil_modifier_depends_ontime(md)) {
    return true;
  }

  /* Check whether modifier is animated. */
  /* TODO(Aligorith): this should be handled as part of build_animdata() */
  if (ob->adt) {
    AnimData *adt = ob->adt;
    FCurve *fcu;

    char pattern[MAX_NAME + 32];
    BLI_snprintf(pattern, sizeof(pattern), "grease_pencil_modifiers[\"%s\"]", md->name);

    /* action - check for F-Curves with paths containing 'grease_pencil_modifiers[' */
    if (adt->action) {
      for (fcu = adt->action->curves.first; fcu != NULL; fcu = fcu->next) {
        if (fcu->rna_path && strstr(fcu->rna_path, pattern)) {
          return true;
        }
      }
    }

    /* This here allows modifier properties to get driven and still update properly */
    for (fcu = adt->drivers.first; fcu != NULL; fcu = fcu->next) {
      if (fcu->rna_path && strstr(fcu->rna_path, pattern)) {
        return true;
      }
    }
  }

  return false;
}

bool BKE_object_shaderfx_use_time(Object *ob, ShaderFxData *fx)
{
  if (BKE_shaderfx_depends_ontime(fx)) {
    return true;
  }

  /* Check whether effect is animated. */
  /* TODO(Aligorith): this should be handled as part of build_animdata() */
  if (ob->adt) {
    AnimData *adt = ob->adt;
    FCurve *fcu;

    char pattern[MAX_NAME + 32];
    BLI_snprintf(pattern, sizeof(pattern), "shader_effects[\"%s\"]", fx->name);

    /* action - check for F-Curves with paths containing string[' */
    if (adt->action) {
      for (fcu = adt->action->curves.first; fcu != NULL; fcu = fcu->next) {
        if (fcu->rna_path && strstr(fcu->rna_path, pattern)) {
          return true;
        }
      }
    }

    /* This here allows properties to get driven and still update properly */
    for (fcu = adt->drivers.first; fcu != NULL; fcu = fcu->next) {
      if (fcu->rna_path && strstr(fcu->rna_path, pattern)) {
        return true;
      }
    }
  }

  return false;
}

/**
 * Set "ignore cache" flag for all caches on this object.
 */
static void object_cacheIgnoreClear(Object *ob, int state)
{
  ListBase pidlist;
  PTCacheID *pid;
  BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

  for (pid = pidlist.first; pid; pid = pid->next) {
    if (pid->cache) {
      if (state) {
        pid->cache->flag |= PTCACHE_IGNORE_CLEAR;
      }
      else {
        pid->cache->flag &= ~PTCACHE_IGNORE_CLEAR;
      }
    }
  }

  BLI_freelistN(&pidlist);
}

/**
 * \note this function should eventually be replaced by depsgraph functionality.
 * Avoid calling this in new code unless there is a very good reason for it!
 */
bool BKE_object_modifier_update_subframe(Depsgraph *depsgraph,
                                         Scene *scene,
                                         Object *ob,
                                         bool update_mesh,
                                         int parent_recursion,
                                         float frame,
                                         int type)
{
  const bool flush_to_original = DEG_is_active(depsgraph);
  ModifierData *md = BKE_modifiers_findby_type(ob, (ModifierType)type);
  bConstraint *con;

  if (type == eModifierType_DynamicPaint) {
    DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

    /* if other is dynamic paint canvas, don't update */
    if (pmd && pmd->canvas) {
      return true;
    }
  }
  else if (type == eModifierType_Fluid) {
    FluidModifierData *fmd = (FluidModifierData *)md;

    if (fmd && (fmd->type & MOD_FLUID_TYPE_DOMAIN) != 0) {
      return true;
    }
  }

  /* if object has parents, update them too */
  if (parent_recursion) {
    int recursion = parent_recursion - 1;
    bool no_update = false;
    if (ob->parent) {
      no_update |= BKE_object_modifier_update_subframe(
          depsgraph, scene, ob->parent, 0, recursion, frame, type);
    }
    if (ob->track) {
      no_update |= BKE_object_modifier_update_subframe(
          depsgraph, scene, ob->track, 0, recursion, frame, type);
    }

    /* skip subframe if object is parented
     * to vertex of a dynamic paint canvas */
    if (no_update && (ob->partype == PARVERT1 || ob->partype == PARVERT3)) {
      return false;
    }

    /* also update constraint targets */
    for (con = ob->constraints.first; con; con = con->next) {
      const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
      ListBase targets = {NULL, NULL};

      if (cti && cti->get_constraint_targets) {
        bConstraintTarget *ct;
        cti->get_constraint_targets(con, &targets);
        for (ct = targets.first; ct; ct = ct->next) {
          if (ct->tar) {
            BKE_object_modifier_update_subframe(
                depsgraph, scene, ct->tar, 0, recursion, frame, type);
          }
        }
        /* free temp targets */
        if (cti->flush_constraint_targets) {
          cti->flush_constraint_targets(con, &targets, 0);
        }
      }
    }
  }

  /* was originally ID_RECALC_ALL - TODO - which flags are really needed??? */
  /* TODO(sergey): What about animation? */
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    frame);

  ob->id.recalc |= ID_RECALC_ALL;
  if (update_mesh) {
    BKE_animsys_evaluate_animdata(
        &ob->id, ob->adt, &anim_eval_context, ADT_RECALC_ANIM, flush_to_original);
    /* ignore cache clear during subframe updates
     * to not mess up cache validity */
    object_cacheIgnoreClear(ob, 1);
    BKE_object_handle_update(depsgraph, scene, ob);
    object_cacheIgnoreClear(ob, 0);
  }
  else {
    BKE_object_where_is_calc_time(depsgraph, scene, ob, frame);
  }

  /* for curve following objects, parented curve has to be updated too */
  if (ob->type == OB_CURVE) {
    Curve *cu = ob->data;
    BKE_animsys_evaluate_animdata(
        &cu->id, cu->adt, &anim_eval_context, ADT_RECALC_ANIM, flush_to_original);
  }
  /* and armatures... */
  if (ob->type == OB_ARMATURE) {
    bArmature *arm = ob->data;
    BKE_animsys_evaluate_animdata(
        &arm->id, arm->adt, &anim_eval_context, ADT_RECALC_ANIM, flush_to_original);
    BKE_pose_where_is(depsgraph, scene, ob);
  }

  return false;
}

/**
 * Updates select_id of all objects in the given \a bmain.
 */
void BKE_object_update_select_id(struct Main *bmain)
{
  Object *ob = bmain->objects.first;
  int select_id = 1;
  while (ob) {
    ob->runtime.select_id = select_id++;
    ob = ob->id.next;
  }
}

Mesh *BKE_object_to_mesh(Depsgraph *depsgraph, Object *object, bool preserve_all_data_layers)
{
  BKE_object_to_mesh_clear(object);

  Mesh *mesh = BKE_mesh_new_from_object(depsgraph, object, preserve_all_data_layers);
  object->runtime.object_as_temp_mesh = mesh;
  return mesh;
}

void BKE_object_to_mesh_clear(Object *object)
{
  if (object->runtime.object_as_temp_mesh == NULL) {
    return;
  }
  BKE_id_free(NULL, object->runtime.object_as_temp_mesh);
  object->runtime.object_as_temp_mesh = NULL;
}

void BKE_object_check_uuids_unique_and_report(const Object *object)
{
  BKE_pose_check_uuids_unique_and_report(object->pose);
  BKE_modifier_check_uuids_unique_and_report(object);
}

void BKE_object_modifiers_lib_link_common(void *userData,
                                          struct Object *ob,
                                          struct ID **idpoin,
                                          int cb_flag)
{
  BlendLibReader *reader = userData;

  BLO_read_id_address(reader, ob->id.lib, idpoin);
  if (*idpoin != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus_no_lib(*idpoin);
  }
}
