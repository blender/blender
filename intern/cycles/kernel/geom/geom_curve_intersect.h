/*
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

CCL_NAMESPACE_BEGIN

/* Curve primitive intersection functions. */

#ifdef __HAIR__

/* On CPU pass P and dir by reference to aligned vector. */
ccl_device_forceinline bool curve_intersect(KernelGlobals *kg,
                                            Intersection *isect,
                                            const float3 ccl_ref P,
                                            const float3 ccl_ref dir,
                                            uint visibility,
                                            int object,
                                            int curveAddr,
                                            float time,
                                            int type)
{
  const bool is_curve_primitive = (type & PRIMITIVE_CURVE);

#  ifndef __KERNEL_OPTIX__ /* see OptiX motion flag OPTIX_MOTION_FLAG_[START|END]_VANISH */
  if (!is_curve_primitive && kernel_data.bvh.use_bvh_steps) {
    const float2 prim_time = kernel_tex_fetch(__prim_time, curveAddr);
    if (time < prim_time.x || time > prim_time.y) {
      return false;
    }
  }
#  endif

  int segment = PRIMITIVE_UNPACK_SEGMENT(type);
  float epsilon = 0.0f;
  float r_st, r_en;

  int depth = kernel_data.curve.subdivisions;
  int flags = kernel_data.curve.curveflags;
  int prim = kernel_tex_fetch(__prim_index, curveAddr);

  float3 curve_coef[4];

  /* curve Intersection check */
  /* obtain curve parameters */
  {
    /* ray transform created - this should be created at beginning of intersection loop */
    Transform htfm;
    float d = sqrtf(dir.x * dir.x + dir.z * dir.z);
    htfm = make_transform(dir.z / d,
                          0,
                          -dir.x / d,
                          0,
                          -dir.x * dir.y / d,
                          d,
                          -dir.y * dir.z / d,
                          0,
                          dir.x,
                          dir.y,
                          dir.z,
                          0);

    float4 v00 = kernel_tex_fetch(__curves, prim);

    int k0 = __float_as_int(v00.x) + segment;
    int k1 = k0 + 1;

    int ka = max(k0 - 1, __float_as_int(v00.x));
    int kb = min(k1 + 1, __float_as_int(v00.x) + __float_as_int(v00.y) - 1);

    float4 P_curve[4];

    if (is_curve_primitive) {
      P_curve[0] = kernel_tex_fetch(__curve_keys, ka);
      P_curve[1] = kernel_tex_fetch(__curve_keys, k0);
      P_curve[2] = kernel_tex_fetch(__curve_keys, k1);
      P_curve[3] = kernel_tex_fetch(__curve_keys, kb);
    }
    else {
      int fobject = (object == OBJECT_NONE) ? kernel_tex_fetch(__prim_object, curveAddr) : object;
      motion_curve_keys(kg, fobject, prim, time, ka, k0, k1, kb, P_curve);
    }

    float3 p0 = transform_point(&htfm, float4_to_float3(P_curve[0]) - P);
    float3 p1 = transform_point(&htfm, float4_to_float3(P_curve[1]) - P);
    float3 p2 = transform_point(&htfm, float4_to_float3(P_curve[2]) - P);
    float3 p3 = transform_point(&htfm, float4_to_float3(P_curve[3]) - P);

    float fc = 0.71f;
    curve_coef[0] = p1;
    curve_coef[1] = -fc * p0 + fc * p2;
    curve_coef[2] = 2.0f * fc * p0 + (fc - 3.0f) * p1 + (3.0f - 2.0f * fc) * p2 - fc * p3;
    curve_coef[3] = -fc * p0 + (2.0f - fc) * p1 + (fc - 2.0f) * p2 + fc * p3;
    r_st = P_curve[1].w;
    r_en = P_curve[2].w;
  }

  float r_curr = max(r_st, r_en);

  epsilon = 2 * r_curr;

  /* find bounds - this is slow for cubic curves */
  float upper, lower;

  float zextrem[4];
  curvebounds(&lower,
              &upper,
              &zextrem[0],
              &zextrem[1],
              &zextrem[2],
              &zextrem[3],
              curve_coef[0].z,
              curve_coef[1].z,
              curve_coef[2].z,
              curve_coef[3].z);
  if (lower - r_curr > isect->t || upper + r_curr < epsilon)
    return false;

  /* minimum width extension */
  float xextrem[4];
  curvebounds(&lower,
              &upper,
              &xextrem[0],
              &xextrem[1],
              &xextrem[2],
              &xextrem[3],
              curve_coef[0].x,
              curve_coef[1].x,
              curve_coef[2].x,
              curve_coef[3].x);
  if (lower > r_curr || upper < -r_curr)
    return false;

  float yextrem[4];
  curvebounds(&lower,
              &upper,
              &yextrem[0],
              &yextrem[1],
              &yextrem[2],
              &yextrem[3],
              curve_coef[0].y,
              curve_coef[1].y,
              curve_coef[2].y,
              curve_coef[3].y);
  if (lower > r_curr || upper < -r_curr)
    return false;

  /* setup recurrent loop */
  int level = 1 << depth;
  int tree = 0;
  float resol = 1.0f / (float)level;
  bool hit = false;

  /* begin loop */
  while (!(tree >> (depth))) {
    const float i_st = tree * resol;
    const float i_en = i_st + (level * resol);

    float3 p_st = ((curve_coef[3] * i_st + curve_coef[2]) * i_st + curve_coef[1]) * i_st +
                  curve_coef[0];
    float3 p_en = ((curve_coef[3] * i_en + curve_coef[2]) * i_en + curve_coef[1]) * i_en +
                  curve_coef[0];

    float bminx = min(p_st.x, p_en.x);
    float bmaxx = max(p_st.x, p_en.x);
    float bminy = min(p_st.y, p_en.y);
    float bmaxy = max(p_st.y, p_en.y);
    float bminz = min(p_st.z, p_en.z);
    float bmaxz = max(p_st.z, p_en.z);

    if (xextrem[0] >= i_st && xextrem[0] <= i_en) {
      bminx = min(bminx, xextrem[1]);
      bmaxx = max(bmaxx, xextrem[1]);
    }
    if (xextrem[2] >= i_st && xextrem[2] <= i_en) {
      bminx = min(bminx, xextrem[3]);
      bmaxx = max(bmaxx, xextrem[3]);
    }
    if (yextrem[0] >= i_st && yextrem[0] <= i_en) {
      bminy = min(bminy, yextrem[1]);
      bmaxy = max(bmaxy, yextrem[1]);
    }
    if (yextrem[2] >= i_st && yextrem[2] <= i_en) {
      bminy = min(bminy, yextrem[3]);
      bmaxy = max(bmaxy, yextrem[3]);
    }
    if (zextrem[0] >= i_st && zextrem[0] <= i_en) {
      bminz = min(bminz, zextrem[1]);
      bmaxz = max(bmaxz, zextrem[1]);
    }
    if (zextrem[2] >= i_st && zextrem[2] <= i_en) {
      bminz = min(bminz, zextrem[3]);
      bmaxz = max(bmaxz, zextrem[3]);
    }

    float r1 = r_st + (r_en - r_st) * i_st;
    float r2 = r_st + (r_en - r_st) * i_en;
    r_curr = max(r1, r2);

    if (bminz - r_curr > isect->t || bmaxz + r_curr < epsilon || bminx > r_curr ||
        bmaxx < -r_curr || bminy > r_curr || bmaxy < -r_curr) {
      /* the bounding box does not overlap the square centered at O */
      tree += level;
      level = tree & -tree;
    }
    else if (level == 1) {

      /* the maximum recursion depth is reached.
       * check if dP0.(Q-P0)>=0 and dPn.(Pn-Q)>=0.
       * dP* is reversed if necessary.*/
      float t = isect->t;
      float u = 0.0f;
      float gd = 0.0f;

      if (flags & CURVE_KN_RIBBONS) {
        float3 tg = (p_en - p_st);
        float w = tg.x * tg.x + tg.y * tg.y;
        if (w == 0) {
          tree++;
          level = tree & -tree;
          continue;
        }
        w = -(p_st.x * tg.x + p_st.y * tg.y) / w;
        w = saturate(w);

        /* compute u on the curve segment */
        u = i_st * (1 - w) + i_en * w;
        r_curr = r_st + (r_en - r_st) * u;
        /* compare x-y distances */
        float3 p_curr = ((curve_coef[3] * u + curve_coef[2]) * u + curve_coef[1]) * u +
                        curve_coef[0];

        float3 dp_st = (3 * curve_coef[3] * i_st + 2 * curve_coef[2]) * i_st + curve_coef[1];
        if (dot(tg, dp_st) < 0)
          dp_st *= -1;
        if (dot(dp_st, -p_st) + p_curr.z * dp_st.z < 0) {
          tree++;
          level = tree & -tree;
          continue;
        }
        float3 dp_en = (3 * curve_coef[3] * i_en + 2 * curve_coef[2]) * i_en + curve_coef[1];
        if (dot(tg, dp_en) < 0)
          dp_en *= -1;
        if (dot(dp_en, p_en) - p_curr.z * dp_en.z < 0) {
          tree++;
          level = tree & -tree;
          continue;
        }

        if (p_curr.x * p_curr.x + p_curr.y * p_curr.y >= r_curr * r_curr || p_curr.z <= epsilon ||
            isect->t < p_curr.z) {
          tree++;
          level = tree & -tree;
          continue;
        }

        t = p_curr.z;
      }
      else {
        float l = len(p_en - p_st);
        float invl = 1.0f / l;
        float3 tg = (p_en - p_st) * invl;
        gd = (r2 - r1) * invl;
        float difz = -dot(p_st, tg);
        float cyla = 1.0f - (tg.z * tg.z * (1 + gd * gd));
        float invcyla = 1.0f / cyla;
        float halfb = (-p_st.z - tg.z * (difz + gd * (difz * gd + r1)));
        float tcentre = -halfb * invcyla;
        float zcentre = difz + (tg.z * tcentre);
        float3 tdif = -p_st;
        tdif.z += tcentre;
        float tdifz = dot(tdif, tg);
        float tb = 2 * (tdif.z - tg.z * (tdifz + gd * (tdifz * gd + r1)));
        float tc = dot(tdif, tdif) - tdifz * tdifz * (1 + gd * gd) - r1 * r1 - 2 * r1 * tdifz * gd;
        float td = tb * tb - 4 * cyla * tc;
        if (td < 0.0f) {
          tree++;
          level = tree & -tree;
          continue;
        }

        float rootd = sqrtf(td);
        float correction = (-tb - rootd) * 0.5f * invcyla;
        t = tcentre + correction;

        float3 dp_st = (3 * curve_coef[3] * i_st + 2 * curve_coef[2]) * i_st + curve_coef[1];
        if (dot(tg, dp_st) < 0)
          dp_st *= -1;
        float3 dp_en = (3 * curve_coef[3] * i_en + 2 * curve_coef[2]) * i_en + curve_coef[1];
        if (dot(tg, dp_en) < 0)
          dp_en *= -1;

        if (dot(dp_st, -p_st) + t * dp_st.z < 0 || dot(dp_en, p_en) - t * dp_en.z < 0 ||
            isect->t < t || t <= 0.0f) {
          tree++;
          level = tree & -tree;
          continue;
        }

        float w = (zcentre + (tg.z * correction)) * invl;
        w = saturate(w);
        /* compute u on the curve segment */
        u = i_st * (1 - w) + i_en * w;
      }
      /* we found a new intersection */

#  ifdef __VISIBILITY_FLAG__
      /* visibility flag test. we do it here under the assumption
       * that most triangles are culled by node flags */
      if (kernel_tex_fetch(__prim_visibility, curveAddr) & visibility)
#  endif
      {
        /* record intersection */
        isect->t = t;
        isect->u = u;
        isect->v = gd;
        isect->prim = curveAddr;
        isect->object = object;
        isect->type = type;
        hit = true;
      }

      tree++;
      level = tree & -tree;
    }
    else {
      /* split the curve into two curves and process */
      level = level >> 1;
    }
  }

  return hit;
}

ccl_device_inline float3 curve_refine(KernelGlobals *kg,
                                      ShaderData *sd,
                                      const Intersection *isect,
                                      const Ray *ray)
{
  float t = isect->t;
  float3 P = ray->P;
  float3 D = ray->D;

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_itfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
    D = transform_direction(&tfm, D * t);
    D = normalize_len(D, &t);
  }

  int prim = kernel_tex_fetch(__prim_index, isect->prim);
  float4 v00 = kernel_tex_fetch(__curves, prim);

  int k0 = __float_as_int(v00.x) + PRIMITIVE_UNPACK_SEGMENT(sd->type);
  int k1 = k0 + 1;

  int ka = max(k0 - 1, __float_as_int(v00.x));
  int kb = min(k1 + 1, __float_as_int(v00.x) + __float_as_int(v00.y) - 1);

  float4 P_curve[4];

  if (sd->type & PRIMITIVE_CURVE) {
    P_curve[0] = kernel_tex_fetch(__curve_keys, ka);
    P_curve[1] = kernel_tex_fetch(__curve_keys, k0);
    P_curve[2] = kernel_tex_fetch(__curve_keys, k1);
    P_curve[3] = kernel_tex_fetch(__curve_keys, kb);
  }
  else {
    motion_curve_keys(kg, sd->object, sd->prim, sd->time, ka, k0, k1, kb, P_curve);
  }

  float3 p[4];
  p[0] = float4_to_float3(P_curve[0]);
  p[1] = float4_to_float3(P_curve[1]);
  p[2] = float4_to_float3(P_curve[2]);
  p[3] = float4_to_float3(P_curve[3]);

  P = P + D * t;

  sd->u = isect->u;
  sd->v = 0.0f;

  float3 tg = normalize(curvetangent(isect->u, p[0], p[1], p[2], p[3]));

  if (kernel_data.curve.curveflags & CURVE_KN_RIBBONS) {
    sd->Ng = normalize(-(D - tg * (dot(tg, D))));
  }
  else {
#  ifdef __EMBREE__
    if (kernel_data.bvh.scene) {
      sd->Ng = normalize(isect->Ng);
    }
    else
#  endif
    {
      /* direction from inside to surface of curve */
      float3 p_curr = curvepoint(isect->u, p[0], p[1], p[2], p[3]);
      sd->Ng = normalize(P - p_curr);

      /* adjustment for changing radius */
      float gd = isect->v;

      if (gd != 0.0f) {
        sd->Ng = sd->Ng - gd * tg;
        sd->Ng = normalize(sd->Ng);
      }
    }
  }

  /* todo: sometimes the normal is still so that this is detected as
   * backfacing even if cull backfaces is enabled */

  sd->N = sd->Ng;

#  ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = tg;
  sd->dPdv = cross(tg, sd->Ng);
#  endif

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_tfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
  }

  return P;
}

#endif

CCL_NAMESPACE_END
