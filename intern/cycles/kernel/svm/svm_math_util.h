/*
 * Copyright 2011-2014 Blender Foundation
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
      *vector = safe_divide_float3_float3(a, b);
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
      *vector = floor(safe_divide_float3_float3(a, b)) * b;
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

ccl_device float3 svm_math_blackbody_color(float t)
{
  /* TODO(lukas): Reimplement in XYZ. */

  /* Calculate color in range 800..12000 using an approximation
   * a/x+bx+c for R and G and ((at + b)t + c)t + d) for B
   * Max absolute error for RGB is (0.00095, 0.00077, 0.00057),
   * which is enough to get the same 8 bit/channel color.
   */

  const float blackbody_table_r[6][3] = {
      {2.52432244e+03f, -1.06185848e-03f, 3.11067539e+00f},
      {3.37763626e+03f, -4.34581697e-04f, 1.64843306e+00f},
      {4.10671449e+03f, -8.61949938e-05f, 6.41423749e-01f},
      {4.66849800e+03f, 2.85655028e-05f, 1.29075375e-01f},
      {4.60124770e+03f, 2.89727618e-05f, 1.48001316e-01f},
      {3.78765709e+03f, 9.36026367e-06f, 3.98995841e-01f},
  };

  const float blackbody_table_g[6][3] = {
      {-7.50343014e+02f, 3.15679613e-04f, 4.73464526e-01f},
      {-1.00402363e+03f, 1.29189794e-04f, 9.08181524e-01f},
      {-1.22075471e+03f, 2.56245413e-05f, 1.20753416e+00f},
      {-1.42546105e+03f, -4.01730887e-05f, 1.44002695e+00f},
      {-1.18134453e+03f, -2.18913373e-05f, 1.30656109e+00f},
      {-5.00279505e+02f, -4.59745390e-06f, 1.09090465e+00f},
  };

  const float blackbody_table_b[6][4] = {
      {0.0f, 0.0f, 0.0f, 0.0f}, /* zeros should be optimized by compiler */
      {0.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 0.0f},
      {-2.02524603e-11f, 1.79435860e-07f, -2.60561875e-04f, -1.41761141e-02f},
      {-2.22463426e-13f, -1.55078698e-08f, 3.81675160e-04f, -7.30646033e-01f},
      {6.72595954e-13f, -2.73059993e-08f, 4.24068546e-04f, -7.52204323e-01f},
  };

  if (t >= 12000.0f) {
    return make_float3(0.826270103f, 0.994478524f, 1.56626022f);
  }
  else if (t < 965.0f) {
    /* For 800 <= t < 965 color does not change in OSL implementation, so keep color the same */
    return make_float3(4.70366907f, 0.0f, 0.0f);
  }

  /* Manually align for readability. */
  /* clang-format off */
  int i = (t >= 6365.0f) ? 5 :
          (t >= 3315.0f) ? 4 :
          (t >= 1902.0f) ? 3 :
          (t >= 1449.0f) ? 2 :
          (t >= 1167.0f) ? 1 :
                           0;
  /* clang-format on */

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
