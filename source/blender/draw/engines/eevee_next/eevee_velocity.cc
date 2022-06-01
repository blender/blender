/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * The velocity pass outputs motion vectors to use for either
 * temporal re-projection or motion blur.
 *
 * It is the module that tracks the objects between frames updates.
 *
 * #VelocityModule contains all motion steps data and logic.
 * #VelocityPass contains the resolve pass for static geometry.
 * #VelocityView is a per view instance that contain the velocity buffer.
 */

#include "BKE_duplilist.h"
#include "BKE_object.h"
#include "BLI_map.hh"
#include "DEG_depsgraph_query.h"
#include "DNA_rigidbody_types.h"

#include "eevee_instance.hh"
// #include "eevee_renderpasses.hh"
#include "eevee_shader.hh"
#include "eevee_shader_shared.hh"
#include "eevee_velocity.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name VelocityModule
 *
 * \{ */

void VelocityModule::init()
{
#if 0 /* TODO renderpasses */
  if (inst_.render && (inst_.render_passes.vector != nullptr)) {
    /* No motion blur and the vector pass was requested. Do the step sync here. */
    const Scene *scene = inst_.scene;
    float initial_time = scene->r.cfra + scene->r.subframe;
    step_sync(STEP_PREVIOUS, initial_time - 1.0f);
    step_sync(STEP_NEXT, initial_time + 1.0f);
    inst_.set_time(initial_time);
  }
#endif
}

static void step_object_sync_render(void *velocity,
                                    Object *ob,
                                    RenderEngine *UNUSED(engine),
                                    Depsgraph *UNUSED(depsgraph))
{
  ObjectKey object_key(ob);
  reinterpret_cast<VelocityModule *>(velocity)->step_object_sync(ob, object_key);
}

void VelocityModule::step_sync(eVelocityStep step, float time)
{
  inst_.set_time(time);
  step_ = step;
  object_steps_usage[step_] = 0;
  step_camera_sync();
  DRW_render_object_iter(this, inst_.render, inst_.depsgraph, step_object_sync_render);
}

void VelocityModule::step_camera_sync()
{
  inst_.camera.sync();
  *camera_steps[step_] = inst_.camera.data_get();
}

bool VelocityModule::step_object_sync(Object *ob,
                                      ObjectKey &object_key,
                                      int /*IDRecalcFlag*/ recalc)
{
  bool has_motion = object_has_velocity(ob) || (recalc & ID_RECALC_TRANSFORM);
  /* NOTE: Fragile. This will only work with 1 frame of lag since we can't record every geometry
   * just in case there might be an update the next frame. */
  bool has_deform = object_is_deform(ob) || (recalc & ID_RECALC_GEOMETRY);

  if (!has_motion && !has_deform) {
    return false;
  }

  uint32_t resource_id = DRW_object_resource_id_get(ob);

  /* Object motion. */
  /* FIXME(fclem) As we are using original objects pointers, there is a chance the previous
   * object key matches a totally different object if the scene was changed by user or python
   * callback. In this case, we cannot correctly match objects between updates.
   * What this means is that there will be incorrect motion vectors for these objects.
   * We live with that until we have a correct way of identifying new objects. */
  VelocityObjectData &vel = velocity_map.lookup_or_add_default(object_key);
  vel.obj.ofs[step_] = object_steps_usage[step_]++;
  vel.obj.resource_id = resource_id;
  vel.id = (ID *)ob->data;
  object_steps[step_]->get_or_resize(vel.obj.ofs[step_]) = ob->obmat;
  if (step_ == STEP_CURRENT) {
    /* Replace invalid steps. Can happen if object was hidden in one of those steps. */
    if (vel.obj.ofs[STEP_PREVIOUS] == -1) {
      vel.obj.ofs[STEP_PREVIOUS] = object_steps_usage[STEP_PREVIOUS]++;
      object_steps[STEP_PREVIOUS]->get_or_resize(vel.obj.ofs[STEP_PREVIOUS]) = ob->obmat;
    }
    if (vel.obj.ofs[STEP_NEXT] == -1) {
      vel.obj.ofs[STEP_NEXT] = object_steps_usage[STEP_NEXT]++;
      object_steps[STEP_NEXT]->get_or_resize(vel.obj.ofs[STEP_NEXT]) = ob->obmat;
    }
  }

  /* Geometry motion. */
  if (has_deform) {
    auto add_cb = [&]() {
      VelocityGeometryData data;
      switch (ob->type) {
        case OB_CURVES:
          data.pos_buf = DRW_curves_pos_buffer_get(ob);
          break;
        default:
          data.pos_buf = DRW_cache_object_pos_vertbuf_get(ob);
          break;
      }
      return data;
    };

    const VelocityGeometryData &data = geometry_map.lookup_or_add_cb(vel.id, add_cb);

    if (data.pos_buf == nullptr) {
      has_deform = false;
    }
  }

  /* Avoid drawing object that has no motions but were tagged as such. */
  if (step_ == STEP_CURRENT && has_motion == true && has_deform == false) {
    float4x4 &obmat_curr = (*object_steps[STEP_CURRENT])[vel.obj.ofs[STEP_CURRENT]];
    float4x4 &obmat_prev = (*object_steps[STEP_PREVIOUS])[vel.obj.ofs[STEP_PREVIOUS]];
    float4x4 &obmat_next = (*object_steps[STEP_NEXT])[vel.obj.ofs[STEP_NEXT]];
    if (inst_.is_viewport()) {
      has_motion = (obmat_curr != obmat_prev);
    }
    else {
      has_motion = (obmat_curr != obmat_prev || obmat_curr != obmat_next);
    }
  }

#if 0
  if (!has_motion && !has_deform) {
    std::cout << "Detected no motion on " << ob->id.name << std::endl;
  }
  if (has_deform) {
    std::cout << "Geometry Motion on " << ob->id.name << std::endl;
  }
  if (has_motion) {
    std::cout << "Object Motion on " << ob->id.name << std::endl;
  }
#endif

  if (!has_motion && !has_deform) {
    return false;
  }

  /* TODO(@fclem): Reset sampling here? Should ultimately be covered by depsgraph update tags. */
  // inst_.sampling.reset();

  return true;
}

/**
 * Moves next frame data to previous frame data. Nullify next frame data.
 * IMPORTANT: This runs AFTER drawing in the viewport (so after `begin_sync()`) but BEFORE drawing
 * in render mode (so before `begin_sync()`). In viewport the data will be used the next frame.
 */
void VelocityModule::step_swap()
{
  {
    /* Now that vertex buffers are guaranteed to be updated, proceed with
     * offset computation and copy into the geometry step buffer. */
    uint dst_ofs = 0;
    for (VelocityGeometryData &geom : geometry_map.values()) {
      uint src_len = GPU_vertbuf_get_vertex_len(geom.pos_buf);
      geom.len = src_len;
      geom.ofs = dst_ofs;
      dst_ofs += src_len;
    }
    /* TODO(@fclem): Fail gracefully (disable motion blur + warning print) if
     * `tot_len * sizeof(float4)` is greater than max SSBO size. */
    geometry_steps[step_]->resize(max_ii(16, dst_ofs));

    for (VelocityGeometryData &geom : geometry_map.values()) {
      GPU_storagebuf_copy_sub_from_vertbuf(*geometry_steps[step_],
                                           geom.pos_buf,
                                           geom.ofs * sizeof(float4),
                                           0,
                                           geom.len * sizeof(float4));
    }
    /* Copy back the #VelocityGeometryIndex into #VelocityObjectData which are
     * indexed using persistent keys (unlike geometries which are indexed by volatile ID). */
    for (VelocityObjectData &vel : velocity_map.values()) {
      const VelocityGeometryData &geom = geometry_map.lookup_default(vel.id,
                                                                     VelocityGeometryData());
      vel.geo.len[step_] = geom.len;
      vel.geo.ofs[step_] = geom.ofs;
      /* Avoid reuse. */
      vel.id = nullptr;
    }

    geometry_map.clear();
  }

  auto swap_steps = [&](eVelocityStep step_a, eVelocityStep step_b) {
    SWAP(VelocityObjectBuf *, object_steps[step_a], object_steps[step_b]);
    SWAP(VelocityGeometryBuf *, geometry_steps[step_a], geometry_steps[step_b]);
    SWAP(CameraDataBuf *, camera_steps[step_a], camera_steps[step_b]);

    for (VelocityObjectData &vel : velocity_map.values()) {
      vel.obj.ofs[step_a] = vel.obj.ofs[step_b];
      vel.obj.ofs[step_b] = (uint)-1;
      vel.geo.ofs[step_a] = vel.geo.ofs[step_b];
      vel.geo.len[step_a] = vel.geo.len[step_b];
      vel.geo.ofs[step_b] = (uint)-1;
      vel.geo.len[step_b] = (uint)-1;
    }
  };

  if (inst_.is_viewport()) {
    /* For viewport we only use the last rendered redraw as previous frame.
     * We swap current with previous step at the end of a redraw.
     * We do not support motion blur as it is rendered to avoid conflicting motions
     * for temporal reprojection. */
    swap_steps(eVelocityStep::STEP_PREVIOUS, eVelocityStep::STEP_CURRENT);
  }
  else {
    /* Render case: The STEP_CURRENT is left untouched. */
    swap_steps(eVelocityStep::STEP_PREVIOUS, eVelocityStep::STEP_NEXT);
  }
}

void VelocityModule::begin_sync()
{
  if (inst_.is_viewport()) {
    /* Viewport always evaluate current step. */
    step_ = STEP_CURRENT;
  }
  step_camera_sync();
  object_steps_usage[step_] = 0;
}

/* This is the end of the current frame sync. Not the step_sync. */
void VelocityModule::end_sync()
{
  Vector<ObjectKey, 0> deleted_obj;

  uint32_t max_resource_id_ = 0u;

  for (Map<ObjectKey, VelocityObjectData>::Item item : velocity_map.items()) {
    if (item.value.obj.resource_id == (uint)-1) {
      deleted_obj.append(item.key);
    }
    else {
      max_resource_id_ = max_uu(max_resource_id_, item.value.obj.resource_id);
    }
  }

  if (deleted_obj.size() > 0) {
    // inst_.sampling.reset();
  }

  for (auto key : deleted_obj) {
    velocity_map.remove(key);
  }

  indirection_buf.resize(power_of_2_max_u(max_resource_id_ + 1));

  /* Avoid uploading more data to the GPU as well as an extra level of
   * indirection on the GPU by copying back offsets the to VelocityIndex. */
  for (VelocityObjectData &vel : velocity_map.values()) {
    /* Disable deform if vertex count mismatch. */
    if (inst_.is_viewport()) {
      /* Current geometry step will be copied at the end of the frame.
       * Thus vel.geo.len[STEP_CURRENT] is not yet valid and the current length is manually
       * retrieved. */
      GPUVertBuf *pos_buf = geometry_map.lookup_default(vel.id, VelocityGeometryData()).pos_buf;
      vel.geo.do_deform = pos_buf != nullptr &&
                          (vel.geo.len[STEP_PREVIOUS] == GPU_vertbuf_get_vertex_len(pos_buf));
    }
    else {
      vel.geo.do_deform = (vel.geo.len[STEP_PREVIOUS] == vel.geo.len[STEP_CURRENT]) &&
                          (vel.geo.len[STEP_NEXT] == vel.geo.len[STEP_CURRENT]);
    }
    indirection_buf[vel.obj.resource_id] = vel;
    /* Reset for next sync. */
    vel.obj.resource_id = (uint)-1;
  }

  object_steps[STEP_PREVIOUS]->push_update();
  object_steps[STEP_NEXT]->push_update();
  camera_steps[STEP_PREVIOUS]->push_update();
  camera_steps[STEP_CURRENT]->push_update();
  camera_steps[STEP_NEXT]->push_update();
  indirection_buf.push_update();

  {
    resolve_ps_ = DRW_pass_create("Velocity.Resolve", (DRWState)0);
    GPUShader *sh = inst_.shaders.static_shader_get(VELOCITY_RESOLVE);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, resolve_ps_);
    DRW_shgroup_uniform_texture_ref(grp, "depth_tx", &input_depth_tx_);
    DRW_shgroup_uniform_image_ref(grp, "velocity_view_img", &velocity_view_tx_);
    DRW_shgroup_uniform_image_ref(grp, "velocity_camera_img", &velocity_camera_tx_);
    DRW_shgroup_uniform_block(grp, "camera_prev", *camera_steps[STEP_PREVIOUS]);
    DRW_shgroup_uniform_block(grp, "camera_curr", *camera_steps[STEP_CURRENT]);
    DRW_shgroup_uniform_block(grp, "camera_next", *camera_steps[STEP_NEXT]);
    DRW_shgroup_call_compute_ref(grp, resolve_dispatch_size_);
  }
}

bool VelocityModule::object_has_velocity(const Object *ob)
{
#if 0
    RigidBodyOb *rbo = ob->rigidbody_object;
    /* Active rigidbody objects only, as only those are affected by sim. */
    const bool has_rigidbody = (rbo && (rbo->type == RBO_TYPE_ACTIVE));
    /* For now we assume dupli objects are moving. */
    const bool is_dupli = (ob->base_flag & BASE_FROM_DUPLI) != 0;
    const bool object_moves = is_dupli || has_rigidbody || BKE_object_moves_in_time(ob, true);
#else
  UNUSED_VARS(ob);
  /* BKE_object_moves_in_time does not work in some cases.
   * Better detect non moving object after evaluation. */
  const bool object_moves = true;
#endif
  return object_moves;
}

bool VelocityModule::object_is_deform(const Object *ob)
{
  RigidBodyOb *rbo = ob->rigidbody_object;
  /* Active rigidbody objects only, as only those are affected by sim. */
  const bool has_rigidbody = (rbo && (rbo->type == RBO_TYPE_ACTIVE));
  const bool is_deform = BKE_object_is_deform_modified(inst_.scene, (Object *)ob) ||
                         (has_rigidbody && (rbo->flag & RBO_FLAG_USE_DEFORM) != 0);

  return is_deform;
}

void VelocityModule::bind_resources(DRWShadingGroup *grp)
{
  /* For viewport, only previous motion is supported.
   * Still bind previous step to avoid undefined behavior. */
  eVelocityStep next = inst_.is_viewport() ? STEP_PREVIOUS : STEP_NEXT;
  DRW_shgroup_storage_block_ref(grp, "velocity_obj_prev_buf", &(*object_steps[STEP_PREVIOUS]));
  DRW_shgroup_storage_block_ref(grp, "velocity_obj_next_buf", &(*object_steps[next]));
  DRW_shgroup_storage_block_ref(grp, "velocity_geo_prev_buf", &(*geometry_steps[STEP_PREVIOUS]));
  DRW_shgroup_storage_block_ref(grp, "velocity_geo_next_buf", &(*geometry_steps[next]));
  DRW_shgroup_uniform_block_ref(grp, "camera_prev", &(*camera_steps[STEP_PREVIOUS]));
  DRW_shgroup_uniform_block_ref(grp, "camera_curr", &(*camera_steps[STEP_CURRENT]));
  DRW_shgroup_uniform_block_ref(grp, "camera_next", &(*camera_steps[next]));
  DRW_shgroup_storage_block_ref(grp, "velocity_indirection_buf", &indirection_buf);
}

/* Resolve pass for static geometry and to camera space projection. */
void VelocityModule::resolve_camera_motion(GPUTexture *depth_tx,
                                           GPUTexture *velocity_view_tx,
                                           GPUTexture *velocity_camera_tx)
{
  input_depth_tx_ = depth_tx;
  velocity_view_tx_ = velocity_view_tx;
  velocity_camera_tx_ = velocity_camera_tx;

  resolve_dispatch_size_.x = divide_ceil_u(GPU_texture_width(depth_tx), 8);
  resolve_dispatch_size_.y = divide_ceil_u(GPU_texture_height(depth_tx), 8);

  DRW_draw_pass(resolve_ps_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Velocity View
 * \{ */

void VelocityView::sync()
{
  /* TODO: Remove. */
  velocity_view_tx_.sync();
  velocity_camera_tx_.sync();
}

void VelocityView::acquire(int2 extent)
{
  /* WORKAROUND: View name should be unique and static.
   * With this, we can reuse the same texture across views. */
  DrawEngineType *owner = (DrawEngineType *)view_name_.c_str();

  /* Only RG16F when only doing only reprojection or motion blur. */
  eGPUTextureFormat format = inst_.is_viewport() ? GPU_RG16F : GPU_RGBA16F;
  velocity_view_tx_.acquire(extent, format, owner);
  if (false /* TODO(fclem): Panoramic camera. */) {
    velocity_camera_tx_.acquire(extent, format, owner);
  }
  else {
    velocity_camera_tx_.acquire(int2(1), format, owner);
  }
}

void VelocityView::resolve(GPUTexture *depth_tx)
{
  inst_.velocity.resolve_camera_motion(depth_tx, velocity_view_tx_, velocity_camera_tx_);
}

void VelocityView::release()
{
  velocity_view_tx_.release();
  velocity_camera_tx_.release();
}

/** \} */

}  // namespace blender::eevee
