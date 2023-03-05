
/**
 * Adapted from :
 * Real-Time Polygonal-Light Shading with Linearly Transformed Cosines.
 * Eric Heitz, Jonathan Dupuy, Stephen Hill and David Neubelt.
 * ACM Transactions on Graphics (Proceedings of ACM SIGGRAPH 2016) 35(4), 2016.
 * Project page: https://eheitzresearch.wordpress.com/415-2/
 */

/* Diffuse *clipped* sphere integral. */
float ltc_diffuse_sphere_integral(sampler2DArray utility_tx, float avg_dir_z, float form_factor)
{
#if 1
  /* use tabulated horizon-clipped sphere */
  vec2 uv = vec2(avg_dir_z * 0.5 + 0.5, form_factor);
  uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;

  return texture(utility_tx, vec3(uv, UTIL_DISK_INTEGRAL_LAYER))[UTIL_DISK_INTEGRAL_COMP];
#else
  /* Cheap approximation. Less smooth and have energy issues. */
  return max((form_factor * form_factor + avg_dir_z) / (form_factor + 1.0), 0.0);
#endif
}

/**
 * An extended version of the implementation from
 * "How to solve a cubic equation, revisited"
 * http://momentsingraphics.de/?p=105
 */
vec3 ltc_solve_cubic(vec4 coefs)
{
  /* Normalize the polynomial */
  coefs.xyz /= coefs.w;
  /* Divide middle coefficients by three */
  coefs.yz /= 3.0;

  float A = coefs.w;
  float B = coefs.z;
  float C = coefs.y;
  float D = coefs.x;

  /* Compute the Hessian and the discriminant */
  vec3 delta = vec3(-coefs.zy * coefs.zz + coefs.yx, dot(vec2(coefs.z, -coefs.y), coefs.xy));

  /* Discriminant */
  float discr = dot(vec2(4.0 * delta.x, -delta.y), delta.zy);

  /* Clamping avoid NaN output on some platform. (see #67060) */
  float sqrt_discr = sqrt(clamp(discr, 0.0, FLT_MAX));

  vec2 xlc, xsc;

  /* Algorithm A */
  {
    float A_a = 1.0;
    float C_a = delta.x;
    float D_a = -2.0 * B * delta.x + delta.y;

    /* Take the cubic root of a normalized complex number */
    float theta = atan(sqrt_discr, -D_a) / 3.0;

    float _2_sqrt_C_a = 2.0 * sqrt(-C_a);
    float x_1a = _2_sqrt_C_a * cos(theta);
    float x_3a = _2_sqrt_C_a * cos(theta + (2.0 / 3.0) * M_PI);

    float xl;
    if ((x_1a + x_3a) > 2.0 * B) {
      xl = x_1a;
    }
    else {
      xl = x_3a;
    }

    xlc = vec2(xl - B, A);
  }

  /* Algorithm D */
  {
    float A_d = D;
    float C_d = delta.z;
    float D_d = -D * delta.y + 2.0 * C * delta.z;

    /* Take the cubic root of a normalized complex number */
    float theta = atan(D * sqrt_discr, -D_d) / 3.0;

    float _2_sqrt_C_d = 2.0 * sqrt(-C_d);
    float x_1d = _2_sqrt_C_d * cos(theta);
    float x_3d = _2_sqrt_C_d * cos(theta + (2.0 / 3.0) * M_PI);

    float xs;
    if (x_1d + x_3d < 2.0 * C) {
      xs = x_1d;
    }
    else {
      xs = x_3d;
    }

    xsc = vec2(-D, xs + C);
  }

  float E = xlc.y * xsc.y;
  float F = -xlc.x * xsc.y - xlc.y * xsc.x;
  float G = xlc.x * xsc.x;

  vec2 xmc = vec2(C * F - B * G, -B * F + C * E);

  vec3 root = vec3(xsc.x / xsc.y, xmc.x / xmc.y, xlc.x / xlc.y);

  if (root.x < root.y && root.x < root.z) {
    root.xyz = root.yxz;
  }
  else if (root.z < root.x && root.z < root.y) {
    root.xyz = root.xzy;
  }

  return root;
}

/* from Real-Time Area Lighting: a Journey from Research to Production
 * Stephen Hill and Eric Heitz */
vec3 ltc_edge_integral_vec(vec3 v1, vec3 v2)
{
  float x = dot(v1, v2);
  float y = abs(x);

  float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
  float b = 3.4175940 + (4.1616724 + y) * y;
  float v = a / b;

  float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

  return cross(v1, v2) * theta_sintheta;
}

mat3 ltc_matrix(vec4 lut)
{
  /* Load inverse matrix. */
  return mat3(vec3(lut.x, 0, lut.y), vec3(0, 1, 0), vec3(lut.z, 0, lut.w));
}

void ltc_transform_quad(vec3 N, vec3 V, mat3 Minv, inout vec3 corners[4])
{
  /* Avoid dot(N, V) == 1 in ortho mode, leading T1 normalize to fail. */
  V = normalize(V + 1e-8);

  /* Construct orthonormal basis around N. */
  vec3 T1, T2;
  T1 = normalize(V - N * dot(N, V));
  T2 = cross(N, T1);

  /* Rotate area light in (T1, T2, R) basis. */
  Minv = Minv * transpose(mat3(T1, T2, N));

  /* Apply LTC inverse matrix. */
  corners[0] = normalize(Minv * corners[0]);
  corners[1] = normalize(Minv * corners[1]);
  corners[2] = normalize(Minv * corners[2]);
  corners[3] = normalize(Minv * corners[3]);
}

/* If corners have already pass through ltc_transform_quad(),
 * then N **MUST** be vec3(0.0, 0.0, 1.0), corresponding to the Up axis of the shading basis. */
float ltc_evaluate_quad(sampler2DArray utility_tx, vec3 corners[4], vec3 N)
{
  /* Approximation using a sphere of the same solid angle than the quad.
   * Finding the clipped sphere diffuse integral is easier than clipping the quad. */
  vec3 avg_dir;
  avg_dir = ltc_edge_integral_vec(corners[0], corners[1]);
  avg_dir += ltc_edge_integral_vec(corners[1], corners[2]);
  avg_dir += ltc_edge_integral_vec(corners[2], corners[3]);
  avg_dir += ltc_edge_integral_vec(corners[3], corners[0]);

  float form_factor = length(avg_dir);
  float avg_dir_z = dot(N, avg_dir / form_factor);
  return form_factor * ltc_diffuse_sphere_integral(utility_tx, avg_dir_z, form_factor);
}

/* If disk does not need to be transformed and is already front facing. */
float ltc_evaluate_disk_simple(sampler2DArray utility_tx, float disk_radius, float NL)
{
  float r_sqr = disk_radius * disk_radius;
  float one_r_sqr = 1.0 + r_sqr;
  float form_factor = r_sqr * inversesqrt(one_r_sqr * one_r_sqr);
  return form_factor * ltc_diffuse_sphere_integral(utility_tx, NL, form_factor);
}

/* disk_points are WS vectors from the shading point to the disk "bounding domain" */
float ltc_evaluate_disk(sampler2DArray utility_tx, vec3 N, vec3 V, mat3 Minv, vec3 disk_points[3])
{
  /* Avoid dot(N, V) == 1 in ortho mode, leading T1 normalize to fail. */
  V = normalize(V + 1e-8);

  /* construct orthonormal basis around N */
  vec3 T1, T2;
  T1 = normalize(V - N * dot(V, N));
  T2 = cross(N, T1);

  /* rotate area light in (T1, T2, R) basis */
  mat3 R = transpose(mat3(T1, T2, N));

  /* Intermediate step: init ellipse. */
  vec3 L_[3];
  L_[0] = mul(R, disk_points[0]);
  L_[1] = mul(R, disk_points[1]);
  L_[2] = mul(R, disk_points[2]);

  vec3 C = 0.5 * (L_[0] + L_[2]);
  vec3 V1 = 0.5 * (L_[1] - L_[2]);
  vec3 V2 = 0.5 * (L_[1] - L_[0]);

  /* Transform ellipse by Minv. */
  C = Minv * C;
  V1 = Minv * V1;
  V2 = Minv * V2;

  /* Compute eigenvectors of new ellipse. */

  float d11 = dot(V1, V1);
  float d22 = dot(V2, V2);
  float d12 = dot(V1, V2);
  float a, b;                     /* Eigenvalues */
  const float threshold = 0.0007; /* Can be adjusted. Fix artifacts. */
  if (abs(d12) / sqrt(d11 * d22) > threshold) {
    float tr = d11 + d22;
    float det = -d12 * d12 + d11 * d22;

    /* use sqrt matrix to solve for eigenvalues */
    det = sqrt(det);
    float u = 0.5 * sqrt(tr - 2.0 * det);
    float v = 0.5 * sqrt(tr + 2.0 * det);
    float e_max = (u + v);
    float e_min = (u - v);
    e_max *= e_max;
    e_min *= e_min;

    vec3 V1_, V2_;
    if (d11 > d22) {
      V1_ = d12 * V1 + (e_max - d11) * V2;
      V2_ = d12 * V1 + (e_min - d11) * V2;
    }
    else {
      V1_ = d12 * V2 + (e_max - d22) * V1;
      V2_ = d12 * V2 + (e_min - d22) * V1;
    }

    a = 1.0 / e_max;
    b = 1.0 / e_min;
    V1 = normalize(V1_);
    V2 = normalize(V2_);
  }
  else {
    a = 1.0 / d11;
    b = 1.0 / d22;
    V1 *= sqrt(a);
    V2 *= sqrt(b);
  }

  /* Now find front facing ellipse with same solid angle. */

  vec3 V3 = normalize(cross(V1, V2));
  if (dot(C, V3) < 0.0) {
    V3 *= -1.0;
  }

  float L = dot(V3, C);
  float inv_L = 1.0 / L;
  float x0 = dot(V1, C) * inv_L;
  float y0 = dot(V2, C) * inv_L;

  float L_sqr = L * L;
  a *= L_sqr;
  b *= L_sqr;

  float t = 1.0 + x0 * x0;
  float c0 = a * b;
  float c1 = c0 * (t + y0 * y0) - a - b;
  float c2 = (1.0 - a * t) - b * (1.0 + y0 * y0);
  float c3 = 1.0;

  vec3 roots = ltc_solve_cubic(vec4(c0, c1, c2, c3));
  float e1 = roots.x;
  float e2 = roots.y;
  float e3 = roots.z;

  vec3 avg_dir = vec3(a * x0 / (a - e2), b * y0 / (b - e2), 1.0);

  mat3 rotate = mat3(V1, V2, V3);

  avg_dir = rotate * avg_dir;
  avg_dir = normalize(avg_dir);

  /* L1, L2 are the extends of the front facing ellipse. */
  float L1 = sqrt(-e2 / e3);
  float L2 = sqrt(-e2 / e1);

  /* Find the sphere and compute lighting. */
  float form_factor = max(0.0, L1 * L2 * inversesqrt((1.0 + L1 * L1) * (1.0 + L2 * L2)));
  return form_factor * ltc_diffuse_sphere_integral(utility_tx, avg_dir.z, form_factor);
}
