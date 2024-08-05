/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_mball.hh"

#include "BLI_bounds_types.hh"
#include "BLI_utildefines.h"

#include "DNA_rigidbody_types.h"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {
class Bounds {
  using BoundsInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  PassSimple ps_ = {"Bounds"};

  struct CallBuffers {
    const SelectionType selection_type_;

    BoundsInstanceBuf box = {selection_type_, "bound_box"};
    BoundsInstanceBuf sphere = {selection_type_, "bound_sphere"};
    BoundsInstanceBuf cylinder = {selection_type_, "bound_cylinder"};
    BoundsInstanceBuf cone = {selection_type_, "bound_cone"};
    BoundsInstanceBuf capsule_body = {selection_type_, "bound_capsule_body"};
    BoundsInstanceBuf capsule_cap = {selection_type_, "bound_capsule_cap"};
  } call_buffers_;

 public:
  Bounds(const SelectionType selection_type) : call_buffers_{selection_type} {}

  void begin_sync()
  {
    call_buffers_.box.clear();
    call_buffers_.sphere.clear();
    call_buffers_.cylinder.clear();
    call_buffers_.cone.clear();
    call_buffers_.capsule_body.clear();
    call_buffers_.capsule_cap.clear();
  }

  void object_sync(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    const Object *ob = ob_ref.object;
    const bool from_dupli = (ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI)) != 0;
    const bool has_bounds = !ELEM(
        ob->type, OB_LAMP, OB_CAMERA, OB_EMPTY, OB_SPEAKER, OB_LIGHTPROBE);
    const bool draw_bounds = has_bounds && ((ob->dt == OB_BOUNDBOX) ||
                                            ((ob->dtx & OB_DRAWBOUNDOX) && !from_dupli));
    const float4 color = res.object_wire_color(ob_ref, state);

    auto add_bounds = [&](const bool around_origin, const char bound_type) {
      if (ob->type == OB_MBALL && !BKE_mball_is_basis(ob)) {
        return;
      }
      const float4x4 object_mat{ob->object_to_world().ptr()};
      const blender::Bounds<float3> bounds = BKE_object_boundbox_get(ob).value_or(
          blender::Bounds(float3(-1.0f), float3(1.0f)));
      const float3 size = (bounds.max - bounds.min) * 0.5f;
      const float3 center = around_origin ? float3(0) : math::midpoint(bounds.min, bounds.max);
      const select::ID select_id = res.select_id(ob_ref);

      switch (bound_type) {
        case OB_BOUND_BOX: {
          float4x4 scale = math::from_scale<float4x4>(size);
          scale.location() = center;
          ExtraInstanceData data(object_mat * scale, color, 1.0f);
          call_buffers_.box.append(data, select_id);
          break;
        }
        case OB_BOUND_SPHERE: {
          float4x4 scale = math::from_scale<float4x4>(float3(math::reduce_max(size)));
          scale.location() = center;
          ExtraInstanceData data(object_mat * scale, color, 1.0f);
          call_buffers_.sphere.append(data, select_id);
          break;
        }
        case OB_BOUND_CYLINDER: {
          float4x4 scale = math::from_scale<float4x4>(
              float3(float2(math::max(size.x, size.y)), size.z));
          scale.location() = center;
          ExtraInstanceData data(object_mat * scale, color, 1.0f);
          call_buffers_.cylinder.append(data, select_id);
          break;
        }
        case OB_BOUND_CONE: {
          float4x4 mat = math::from_scale<float4x4>(
              float3(float2(math::max(size.x, size.y)), size.z));
          mat.location() = center;
          /* Cone batch has base at 0 and is pointing towards +Y. */
          std::swap(mat[1], mat[2]);
          mat.location().z -= size.z;
          ExtraInstanceData data(object_mat * mat, color, 1.0f);
          call_buffers_.cone.append(data, select_id);
          break;
        }
        case OB_BOUND_CAPSULE: {
          float4x4 mat = math::from_scale<float4x4>(float3(math::max(size.x, size.y)));
          mat.location() = center;
          mat.location().z = center.z + std::max(0.0f, size.z - size.x);
          ExtraInstanceData data(object_mat * mat, color, 1.0f);
          call_buffers_.capsule_cap.append(data, select_id);
          mat.z_axis() *= -1;
          mat.location().z = center.z - std::max(0.0f, size.z - size.x);
          data.object_to_world_ = object_mat * mat;
          call_buffers_.capsule_cap.append(data, select_id);
          mat.z_axis().z = std::max(0.0f, size.z * 2.0f - size.x * 2.0f);
          data.object_to_world_ = object_mat * mat;
          call_buffers_.capsule_body.append(data, select_id);
          break;
        }
      }
    };

    if (draw_bounds) {
      add_bounds(false, ob->boundtype);
    }
    if (!from_dupli && ob->rigidbody_object != nullptr) {
      switch (ob->rigidbody_object->shape) {
        case RB_SHAPE_BOX:
          add_bounds(true, OB_BOUND_BOX);
          break;
        case RB_SHAPE_SPHERE:
          add_bounds(true, OB_BOUND_SPHERE);
          break;
        case RB_SHAPE_CONE:
          add_bounds(true, OB_BOUND_CONE);
          break;
        case RB_SHAPE_CYLINDER:
          add_bounds(true, OB_BOUND_CYLINDER);
          break;
        case RB_SHAPE_CAPSULE:
          add_bounds(true, OB_BOUND_CAPSULE);
          break;
      };
    }
  }

  void end_sync(Resources &res, ShapeCache &shapes, const State &state)
  {
    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                  state.clipping_state);
    ps_.shader_set(res.shaders.extra_shape.get());
    ps_.bind_ubo("globalsBlock", &res.globals_buf);
    res.select_bind(ps_);

    call_buffers_.box.end_sync(ps_, shapes.cube.get());
    call_buffers_.sphere.end_sync(ps_, shapes.empty_sphere.get());
    call_buffers_.cylinder.end_sync(ps_, shapes.cylinder.get());
    call_buffers_.cone.end_sync(ps_, shapes.empty_cone.get());
    call_buffers_.capsule_body.end_sync(ps_, shapes.capsule_body.get());
    call_buffers_.capsule_cap.end_sync(ps_, shapes.capsule_cap.get());
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};
}  // namespace blender::draw::overlay
