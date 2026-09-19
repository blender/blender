/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/curve.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/attribute.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* Geometry Node */

template<typename Float3Type>
ccl_device_inline Float3Type svm_node_geometry_eval(KernelGlobals kg,
                                                    ccl_private ShaderData *sd,
                                                    const NodeGeometry type)
{
  Float3Type data;

  switch (type) {
    case NODE_GEOM_P:
      data = shading_position<Float3Type>(sd);
      break;
    case NODE_GEOM_N:
      data = Float3Type(sd->N);
      break;
#ifdef __DPDU__
    case NODE_GEOM_T:
      data = primitive_tangent<Float3Type>(kg, sd);
      break;
#endif
    case NODE_GEOM_I:
      data = shading_incoming<Float3Type>(sd);
      break;
    case NODE_GEOM_Ng:
      data = Float3Type(sd->Ng);
      break;
    case NODE_GEOM_uv:
      data = Float3Type(make_float3(1.0f - sd->u - sd->v, sd->u, 0.0f));
      if constexpr (is_dual_v<Float3Type>) {
        data.dx = make_float3(-sd->du.dx - sd->dv.dx, sd->du.dx, 0.0f);
        data.dy = make_float3(-sd->du.dy - sd->dv.dy, sd->du.dy, 0.0f);
      }
      break;
    default:
      data = Float3Type(make_float3(0.0f, 0.0f, 0.0f));
  }

  return data;
}

template<typename Float3Type>
ccl_device_noinline void svm_node_geometry(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *ccl_restrict stack,
                                           const ccl_global SVMNodeGeometry &ccl_restrict node)
{
  Float3Type data = svm_node_geometry_eval<Float3Type>(kg, sd, node.geom_type);

  if constexpr (is_dual_v<Float3Type>) {
    /* Apply first-order bump offset. */
    if (node.bump_offset == NODE_BUMP_OFFSET_DX) {
      data.val += data.dx * node.bump_filter_width;
    }
    else if (node.bump_offset == NODE_BUMP_OFFSET_DY) {
      data.val += data.dy * node.bump_filter_width;
    }
    if (node.store_derivatives) {
      stack_store(stack, node.out_offset, data);
    }
    else {
      stack_store(stack, node.out_offset, data.val);
    }
  }
  else {
    stack_store(stack, node.out_offset, data);
  }
}

/* Object Info */

ccl_device_noinline void svm_node_object_info(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *ccl_restrict stack,
                                              const ccl_global SVMNodeObjectInfo &ccl_restrict
                                                  node)
{
  float data;

  switch (node.info_type) {
    case NODE_INFO_OB_LOCATION: {
      stack_store_float3(stack, node.out_offset, object_location(kg, sd));
      return;
    }
    case NODE_INFO_OB_COLOR: {
      stack_store_float3(stack, node.out_offset, object_color(kg, sd->object));
      return;
    }
    case NODE_INFO_OB_ALPHA:
      data = object_alpha(kg, sd->object);
      break;
    case NODE_INFO_OB_INDEX:
      data = object_pass_id(kg, sd->object);
      break;
    case NODE_INFO_MAT_INDEX:
      data = shader_pass_id(kg, sd);
      break;
    case NODE_INFO_OB_RANDOM: {
      data = object_random_number(kg, sd->object);
      break;
    }
    default:
      data = 0.0f;
      break;
  }

  stack_store_float(stack, node.out_offset, data);
}

/* Particle Info */

ccl_device_noinline void svm_node_particle_info(KernelGlobals kg,
                                                ccl_private ShaderData *sd,
                                                ccl_private float *ccl_restrict stack,
                                                const ccl_global SVMNodeParticleInfo &ccl_restrict
                                                    node)
{
  switch (node.info_type) {
    case NODE_INFO_PAR_INDEX: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, node.out_offset, particle_index(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_RANDOM: {
      const int particle_id = object_particle_id(kg, sd->object);
      const float random = hash_uint2_to_float(particle_index(kg, particle_id), 0);
      stack_store_float(stack, node.out_offset, random);
      break;
    }
    case NODE_INFO_PAR_AGE: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, node.out_offset, particle_age(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_LIFETIME: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, node.out_offset, particle_lifetime(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_LOCATION: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float3(stack, node.out_offset, particle_location(kg, particle_id));
      break;
    }
#if 0 /* XXX float4 currently not supported in SVM stack */
    case NODE_INFO_PAR_ROTATION: {
      int particle_id = object_particle_id(kg, sd->object);
      stack_store_float4(stack, node.out_offset, particle_rotation(kg, particle_id));
      break;
    }
#endif
    case NODE_INFO_PAR_SIZE: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, node.out_offset, particle_size(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_VELOCITY: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float3(stack, node.out_offset, particle_velocity(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_ANGULAR_VELOCITY: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float3(stack, node.out_offset, particle_angular_velocity(kg, particle_id));
      break;
    }
  }
}

/* Hair Info */

ccl_device_noinline void svm_node_hair_info(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            ccl_private float *ccl_restrict stack,
                                            const ccl_global SVMNodeHairInfo &ccl_restrict node)
{
  float data;
  float3 data3;

  switch (node.info_type) {
    case NODE_INFO_CURVE_IS_STRAND: {
      data = (sd->type & PRIMITIVE_CURVE) != 0;
      stack_store_float(stack, node.out_offset, data);
      break;
    }
    case NODE_INFO_CURVE_INTERCEPT:
      break; /* handled as attribute */
    case NODE_INFO_CURVE_LENGTH:
      break; /* handled as attribute */
    case NODE_INFO_CURVE_RANDOM:
      break; /* handled as attribute */
    case NODE_INFO_CURVE_THICKNESS: {
#ifdef __HAIR__
      data = curve_thickness(kg, sd);
#else
      data = 0.0f;
#endif
      stack_store_float(stack, node.out_offset, data);
      break;
    }
    case NODE_INFO_CURVE_TANGENT_NORMAL: {
#ifdef __HAIR__
      data3 = curve_tangent_normal(sd);
#else
      data3 = zero_float3();
#endif
      stack_store_float3(stack, node.out_offset, data3);
      break;
    }
  }

  (void)kg;
}

/* Point Info */

ccl_device_noinline void svm_node_point_info(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *ccl_restrict stack,
                                             const ccl_global SVMNodePointInfo &ccl_restrict node)
{
  switch (node.info_type) {
    case NODE_INFO_POINT_POSITION:
#ifdef __POINTCLOUD__
      stack_store_float3(stack, node.out_offset, point_position(kg, sd));
#else
      stack_store_float3(stack, node.out_offset, zero_float3());
#endif
      break;
    case NODE_INFO_POINT_RADIUS:
#ifdef __POINTCLOUD__
      stack_store_float(stack, node.out_offset, point_radius(kg, sd));
#else
      stack_store_float(stack, node.out_offset, 0.0f);
#endif
      break;
    case NODE_INFO_POINT_RANDOM:
      break; /* handled as attribute */
  }

  (void)kg;
  (void)sd;
}

CCL_NAMESPACE_END
