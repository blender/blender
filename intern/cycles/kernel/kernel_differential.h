/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

CCL_NAMESPACE_BEGIN

/* See "Tracing Ray Differentials", Homan Igehy, 1999. */

ccl_device void differential_transfer(ccl_private differential3 *surface_dP,
                                      const differential3 ray_dP,
                                      float3 ray_D,
                                      const differential3 ray_dD,
                                      float3 surface_Ng,
                                      float ray_t)
{
  /* ray differential transfer through homogeneous medium, to
   * compute dPdx/dy at a shading point from the incoming ray */

  float3 tmp = ray_D / dot(ray_D, surface_Ng);
  float3 tmpx = ray_dP.dx + ray_t * ray_dD.dx;
  float3 tmpy = ray_dP.dy + ray_t * ray_dD.dy;

  surface_dP->dx = tmpx - dot(tmpx, surface_Ng) * tmp;
  surface_dP->dy = tmpy - dot(tmpy, surface_Ng) * tmp;
}

ccl_device void differential_incoming(ccl_private differential3 *dI, const differential3 dD)
{
  /* compute dIdx/dy at a shading point, we just need to negate the
   * differential of the ray direction */

  dI->dx = -dD.dx;
  dI->dy = -dD.dy;
}

ccl_device void differential_dudv(ccl_private differential *du,
                                  ccl_private differential *dv,
                                  float3 dPdu,
                                  float3 dPdv,
                                  differential3 dP,
                                  float3 Ng)
{
  /* now we have dPdx/dy from the ray differential transfer, and dPdu/dv
   * from the primitive, we can compute dudx/dy and dvdx/dy. these are
   * mainly used for differentials of arbitrary mesh attributes. */

  /* find most stable axis to project to 2D */
  float xn = fabsf(Ng.x);
  float yn = fabsf(Ng.y);
  float zn = fabsf(Ng.z);

  if (zn < xn || zn < yn) {
    if (yn < xn || yn < zn) {
      dPdu.x = dPdu.y;
      dPdv.x = dPdv.y;
      dP.dx.x = dP.dx.y;
      dP.dy.x = dP.dy.y;
    }

    dPdu.y = dPdu.z;
    dPdv.y = dPdv.z;
    dP.dx.y = dP.dx.z;
    dP.dy.y = dP.dy.z;
  }

  /* using Cramer's rule, we solve for dudx and dvdx in a 2x2 linear system,
   * and the same for dudy and dvdy. the denominator is the same for both
   * solutions, so we compute it only once.
   *
   * dP.dx = dPdu * dudx + dPdv * dvdx;
   * dP.dy = dPdu * dudy + dPdv * dvdy; */

  float det = (dPdu.x * dPdv.y - dPdv.x * dPdu.y);

  if (det != 0.0f)
    det = 1.0f / det;

  du->dx = (dP.dx.x * dPdv.y - dP.dx.y * dPdv.x) * det;
  dv->dx = (dP.dx.y * dPdu.x - dP.dx.x * dPdu.y) * det;

  du->dy = (dP.dy.x * dPdv.y - dP.dy.y * dPdv.x) * det;
  dv->dy = (dP.dy.y * dPdu.x - dP.dy.x * dPdu.y) * det;
}

ccl_device differential differential_zero()
{
  differential d;
  d.dx = 0.0f;
  d.dy = 0.0f;

  return d;
}

ccl_device differential3 differential3_zero()
{
  differential3 d;
  d.dx = zero_float3();
  d.dy = zero_float3();

  return d;
}

/* Compact ray differentials that are just a scale to reduce memory usage and
 * access cost in GPU.
 *
 * See above for more accurate reference implementations.
 *
 * TODO: also store the more compact version in ShaderData and recompute where
 * needed? */

ccl_device_forceinline float differential_zero_compact()
{
  return 0.0f;
}

ccl_device_forceinline float differential_make_compact(const differential3 D)
{
  return 0.5f * (len(D.dx) + len(D.dy));
}

ccl_device_forceinline void differential_transfer_compact(ccl_private differential3 *surface_dP,
                                                          const float ray_dP,
                                                          const float3 /* ray_D */,
                                                          const float ray_dD,
                                                          const float3 surface_Ng,
                                                          const float ray_t)
{
  /* ray differential transfer through homogeneous medium, to
   * compute dPdx/dy at a shading point from the incoming ray */
  float scale = ray_dP + ray_t * ray_dD;

  float3 dx, dy;
  make_orthonormals(surface_Ng, &dx, &dy);
  surface_dP->dx = dx * scale;
  surface_dP->dy = dy * scale;
}

ccl_device_forceinline void differential_incoming_compact(ccl_private differential3 *dI,
                                                          const float3 D,
                                                          const float dD)
{
  /* compute dIdx/dy at a shading point, we just need to negate the
   * differential of the ray direction */

  float3 dx, dy;
  make_orthonormals(D, &dx, &dy);

  dI->dx = dD * dx;
  dI->dy = dD * dy;
}

CCL_NAMESPACE_END
