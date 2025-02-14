/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/curve.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/attribute.h"
#include "kernel/svm/util.h"

#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* Geometry Node */

ccl_device_noinline void svm_node_geometry(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           ccl_private float *stack,
                                           const uint type,
                                           const uint out_offset)
{
  float3 data;

  switch (type) {
    case NODE_GEOM_P:
      data = sd->P;
      break;
    case NODE_GEOM_N:
      data = sd->N;
      break;
#ifdef __DPDU__
    case NODE_GEOM_T:
      data = primitive_tangent(kg, sd);
      break;
#endif
    case NODE_GEOM_I:
      data = sd->wi;
      break;
    case NODE_GEOM_Ng:
      data = sd->Ng;
      break;
    case NODE_GEOM_uv:
      data = make_float3(1.0f - sd->u - sd->v, sd->u, 0.0f);
      break;
    default:
      data = make_float3(0.0f, 0.0f, 0.0f);
  }

  stack_store_float3(stack, out_offset, data);
}

ccl_device_noinline void svm_node_geometry_bump_dx(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private float *stack,
                                                   const uint type,
                                                   const uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
  float3 data;

  switch (type) {
    case NODE_GEOM_P:
      data = svm_node_bump_P_dx(sd);
      break;
    case NODE_GEOM_uv: {
      const float u_x = sd->u + sd->du.dx * BUMP_DX;
      const float v_x = sd->v + sd->dv.dx * BUMP_DX;
      data = make_float3(1.0f - u_x - v_x, u_x, 0.0f);
      break;
    }
    default:
      svm_node_geometry(kg, sd, stack, type, out_offset);
      return;
  }

  stack_store_float3(stack, out_offset, data);
#else
  svm_node_geometry(kg, sd, stack, type, out_offset);
#endif
}

ccl_device_noinline void svm_node_geometry_bump_dy(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private float *stack,
                                                   const uint type,
                                                   const uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
  float3 data;

  switch (type) {
    case NODE_GEOM_P:
      data = svm_node_bump_P_dy(sd);
      break;
    case NODE_GEOM_uv: {
      const float u_y = sd->u + sd->du.dy * BUMP_DY;
      const float v_y = sd->v + sd->dv.dy * BUMP_DY;
      data = make_float3(1.0f - u_y - v_y, u_y, 0.0f);
      break;
    }
    default:
      svm_node_geometry(kg, sd, stack, type, out_offset);
      return;
  }

  stack_store_float3(stack, out_offset, data);
#else
  svm_node_geometry(kg, sd, stack, type, out_offset);
#endif
}

/* Object Info */

ccl_device_noinline void svm_node_object_info(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              const uint type,
                                              const uint out_offset)
{
  float data;

  switch (type) {
    case NODE_INFO_OB_LOCATION: {
      stack_store_float3(stack, out_offset, object_location(kg, sd));
      return;
    }
    case NODE_INFO_OB_COLOR: {
      stack_store_float3(stack, out_offset, object_color(kg, sd->object));
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
      if (sd->lamp != LAMP_NONE) {
        data = lamp_random_number(kg, sd->lamp);
      }
      else {
        data = object_random_number(kg, sd->object);
      }
      break;
    }
    default:
      data = 0.0f;
      break;
  }

  stack_store_float(stack, out_offset, data);
}

/* Particle Info */

ccl_device_noinline void svm_node_particle_info(KernelGlobals kg,
                                                ccl_private ShaderData *sd,
                                                ccl_private float *stack,
                                                const uint type,
                                                const uint out_offset)
{
  switch ((NodeParticleInfo)type) {
    case NODE_INFO_PAR_INDEX: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, out_offset, particle_index(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_RANDOM: {
      const int particle_id = object_particle_id(kg, sd->object);
      const float random = hash_uint2_to_float(particle_index(kg, particle_id), 0);
      stack_store_float(stack, out_offset, random);
      break;
    }
    case NODE_INFO_PAR_AGE: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, out_offset, particle_age(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_LIFETIME: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, out_offset, particle_lifetime(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_LOCATION: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float3(stack, out_offset, particle_location(kg, particle_id));
      break;
    }
#if 0 /* XXX float4 currently not supported in SVM stack */
    case NODE_INFO_PAR_ROTATION: {
      int particle_id = object_particle_id(kg, sd->object);
      stack_store_float4(stack, out_offset, particle_rotation(kg, particle_id));
      break;
    }
#endif
    case NODE_INFO_PAR_SIZE: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float(stack, out_offset, particle_size(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_VELOCITY: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float3(stack, out_offset, particle_velocity(kg, particle_id));
      break;
    }
    case NODE_INFO_PAR_ANGULAR_VELOCITY: {
      const int particle_id = object_particle_id(kg, sd->object);
      stack_store_float3(stack, out_offset, particle_angular_velocity(kg, particle_id));
      break;
    }
  }
}

#ifdef __HAIR__

/* Hair Info */

ccl_device_noinline void svm_node_hair_info(KernelGlobals kg,
                                            ccl_private ShaderData *sd,
                                            ccl_private float *stack,
                                            const uint type,
                                            const uint out_offset)
{
  float data;
  float3 data3;

  switch ((NodeHairInfo)type) {
    case NODE_INFO_CURVE_IS_STRAND: {
      data = (sd->type & PRIMITIVE_CURVE) != 0;
      stack_store_float(stack, out_offset, data);
      break;
    }
    case NODE_INFO_CURVE_INTERCEPT:
      break; /* handled as attribute */
    case NODE_INFO_CURVE_LENGTH:
      break; /* handled as attribute */
    case NODE_INFO_CURVE_RANDOM:
      break; /* handled as attribute */
    case NODE_INFO_CURVE_THICKNESS: {
      data = curve_thickness(kg, sd);
      stack_store_float(stack, out_offset, data);
      break;
    }
    case NODE_INFO_CURVE_TANGENT_NORMAL: {
      data3 = curve_tangent_normal(kg, sd);
      stack_store_float3(stack, out_offset, data3);
      break;
    }
  }
}
#endif

#ifdef __POINTCLOUD__

/* Point Info */

ccl_device_noinline void svm_node_point_info(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             const uint type,
                                             const uint out_offset)
{
  switch ((NodePointInfo)type) {
    case NODE_INFO_POINT_POSITION:
      stack_store_float3(stack, out_offset, point_position(kg, sd));
      break;
    case NODE_INFO_POINT_RADIUS:
      stack_store_float(stack, out_offset, point_radius(kg, sd));
      break;
    case NODE_INFO_POINT_RANDOM:
      break; /* handled as attribute */
  }
}

#endif

CCL_NAMESPACE_END
