/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device void svm_vector_math(ccl_private float *value,
                                ccl_private float3 *vector,
                                NodeVectorMathType type,
                                float3 a,
                                float3 b,
                                float3 c,
                                float param1)
{
  switch (type) {
    case NODE_VECTOR_MATH_ADD:
      *vector = a + b;
      break;
    case NODE_VECTOR_MATH_SUBTRACT:
      *vector = a - b;
      break;
    case NODE_VECTOR_MATH_MULTIPLY:
      *vector = a * b;
      break;
    case NODE_VECTOR_MATH_DIVIDE:
      *vector = safe_divide(a, b);
      break;
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      *vector = cross(a, b);
      break;
    case NODE_VECTOR_MATH_PROJECT:
      *vector = project(a, b);
      break;
    case NODE_VECTOR_MATH_REFLECT:
      *vector = reflect(a, b);
      break;
    case NODE_VECTOR_MATH_REFRACT:
      *vector = refract(a, normalize(b), param1);
      break;
    case NODE_VECTOR_MATH_FACEFORWARD:
      *vector = faceforward(a, b, c);
      break;
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      *vector = a * b + c;
      break;
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      *value = dot(a, b);
      break;
    case NODE_VECTOR_MATH_DISTANCE:
      *value = distance(a, b);
      break;
    case NODE_VECTOR_MATH_LENGTH:
      *value = len(a);
      break;
    case NODE_VECTOR_MATH_SCALE:
      *vector = a * param1;
      break;
    case NODE_VECTOR_MATH_NORMALIZE:
      *vector = safe_normalize(a);
      break;
    case NODE_VECTOR_MATH_SNAP:
      *vector = floor(safe_divide(a, b)) * b;
      break;
    case NODE_VECTOR_MATH_FLOOR:
      *vector = floor(a);
      break;
    case NODE_VECTOR_MATH_CEIL:
      *vector = ceil(a);
      break;
    case NODE_VECTOR_MATH_MODULO:
      *vector = make_float3(safe_modulo(a.x, b.x), safe_modulo(a.y, b.y), safe_modulo(a.z, b.z));
      break;
    case NODE_VECTOR_MATH_WRAP:
      *vector = make_float3(wrapf(a.x, b.x, c.x), wrapf(a.y, b.y, c.y), wrapf(a.z, b.z, c.z));
      break;
    case NODE_VECTOR_MATH_FRACTION:
      *vector = a - floor(a);
      break;
    case NODE_VECTOR_MATH_ABSOLUTE:
      *vector = fabs(a);
      break;
    case NODE_VECTOR_MATH_MINIMUM:
      *vector = min(a, b);
      break;
    case NODE_VECTOR_MATH_MAXIMUM:
      *vector = max(a, b);
      break;
    case NODE_VECTOR_MATH_SINE:
      *vector = make_float3(sinf(a.x), sinf(a.y), sinf(a.z));
      break;
    case NODE_VECTOR_MATH_COSINE:
      *vector = make_float3(cosf(a.x), cosf(a.y), cosf(a.z));
      break;
    case NODE_VECTOR_MATH_TANGENT:
      *vector = make_float3(tanf(a.x), tanf(a.y), tanf(a.z));
      break;
    default:
      *vector = zero_float3();
      *value = 0.0f;
  }
}

ccl_device float svm_math(NodeMathType type, float a, float b, float c)
{
  switch (type) {
    case NODE_MATH_ADD:
      return a + b;
    case NODE_MATH_SUBTRACT:
      return a - b;
    case NODE_MATH_MULTIPLY:
      return a * b;
    case NODE_MATH_DIVIDE:
      return safe_divide(a, b);
    case NODE_MATH_POWER:
      return safe_powf(a, b);
    case NODE_MATH_LOGARITHM:
      return safe_logf(a, b);
    case NODE_MATH_SQRT:
      return safe_sqrtf(a);
    case NODE_MATH_INV_SQRT:
      return inversesqrtf(a);
    case NODE_MATH_ABSOLUTE:
      return fabsf(a);
    case NODE_MATH_RADIANS:
      return a * (M_PI_F / 180.0f);
    case NODE_MATH_DEGREES:
      return a * (180.0f / M_PI_F);
    case NODE_MATH_MINIMUM:
      return fminf(a, b);
    case NODE_MATH_MAXIMUM:
      return fmaxf(a, b);
    case NODE_MATH_LESS_THAN:
      return a < b;
    case NODE_MATH_GREATER_THAN:
      return a > b;
    case NODE_MATH_ROUND:
      return floorf(a + 0.5f);
    case NODE_MATH_FLOOR:
      return floorf(a);
    case NODE_MATH_CEIL:
      return ceilf(a);
    case NODE_MATH_FRACTION:
      return a - floorf(a);
    case NODE_MATH_MODULO:
      return safe_modulo(a, b);
    case NODE_MATH_TRUNC:
      return a >= 0.0f ? floorf(a) : ceilf(a);
    case NODE_MATH_SNAP:
      return floorf(safe_divide(a, b)) * b;
    case NODE_MATH_WRAP:
      return wrapf(a, b, c);
    case NODE_MATH_PINGPONG:
      return pingpongf(a, b);
    case NODE_MATH_SINE:
      return sinf(a);
    case NODE_MATH_COSINE:
      return cosf(a);
    case NODE_MATH_TANGENT:
      return tanf(a);
    case NODE_MATH_SINH:
      return sinhf(a);
    case NODE_MATH_COSH:
      return coshf(a);
    case NODE_MATH_TANH:
      return tanhf(a);
    case NODE_MATH_ARCSINE:
      return safe_asinf(a);
    case NODE_MATH_ARCCOSINE:
      return safe_acosf(a);
    case NODE_MATH_ARCTANGENT:
      return atanf(a);
    case NODE_MATH_ARCTAN2:
      return atan2f(a, b);
    case NODE_MATH_SIGN:
      return compatible_signf(a);
    case NODE_MATH_EXPONENT:
      return expf(a);
    case NODE_MATH_COMPARE:
      return ((a == b) || (fabsf(a - b) <= fmaxf(c, FLT_EPSILON))) ? 1.0f : 0.0f;
    case NODE_MATH_MULTIPLY_ADD:
      return a * b + c;
    case NODE_MATH_SMOOTH_MIN:
      return smoothminf(a, b, c);
    case NODE_MATH_SMOOTH_MAX:
      return -smoothminf(-a, -b, c);
    default:
      return 0.0f;
  }
}

ccl_device float3 svm_math_blackbody_color_rec709(float t)
{
  /* Calculate color in range 800..12000 using an approximation
   * a/x+bx+c for R and G and ((at + b)t + c)t + d) for B.
   *
   * The result of this can be negative to support gamut wider than
   * than rec.709, just needs to be clamped. */

  if (t >= 12000.0f) {
    return make_float3(0.8262954810464208f, 0.9945080501520986f, 1.566307710274283f);
  }
  else if (t < 800.0f) {
    /* Arbitrary lower limit where light is very dim, matching OSL. */
    return make_float3(5.413294490189271f, -0.20319390035873933f, -0.0822535242887164f);
  }

  int i = (t >= 6365.0f) ? 6 :
          (t >= 3315.0f) ? 5 :
          (t >= 1902.0f) ? 4 :
          (t >= 1449.0f) ? 3 :
          (t >= 1167.0f) ? 2 :
          (t >= 965.0f)  ? 1 :
                           0;

  ccl_constant float *r = blackbody_table_r[i];
  ccl_constant float *g = blackbody_table_g[i];
  ccl_constant float *b = blackbody_table_b[i];

  const float t_inv = 1.0f / t;
  return make_float3(r[0] * t_inv + r[1] * t + r[2],
                     g[0] * t_inv + g[1] * t + g[2],
                     ((b[0] * t + b[1]) * t + b[2]) * t + b[3]);
}

ccl_device_inline float3 svm_math_gamma_color(float3 color, float gamma)
{
  if (gamma == 0.0f)
    return make_float3(1.0f, 1.0f, 1.0f);

  if (color.x > 0.0f)
    color.x = powf(color.x, gamma);
  if (color.y > 0.0f)
    color.y = powf(color.y, gamma);
  if (color.z > 0.0f)
    color.z = powf(color.z, gamma);

  return color;
}

CCL_NAMESPACE_END
