/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

/* Straight Port from BKE_defvert_weight_to_rgb()
 * TODO: port this to a color ramp. */
vec3 weight_to_color(float weight)
{
  vec3 r_rgb = vec3(0.0);
  float blend = ((weight / 2.0) + 0.5);

  if (weight <= 0.25) { /* blue->cyan */
    r_rgb.g = blend * weight * 4.0;
    r_rgb.b = blend;
  }
  else if (weight <= 0.50) { /* cyan->green */
    r_rgb.g = blend;
    r_rgb.b = blend * (1.0 - ((weight - 0.25) * 4.0));
  }
  else if (weight <= 0.75) { /* green->yellow */
    r_rgb.r = blend * ((weight - 0.50) * 4.0);
    r_rgb.g = blend;
  }
  else if (weight <= 1.0) { /* yellow->red */
    r_rgb.r = blend;
    r_rgb.g = blend * (1.0 - ((weight - 0.75) * 4.0));
  }
  else {
    /* exceptional value, unclamped or nan,
     * avoid uninitialized memory use */
    r_rgb = vec3(1.0, 0.0, 1.0);
  }

  return r_rgb;
}

mat3 rotation_from_vector(vec3 v)
{
  /* Add epsilon to avoid NaN. */
  vec3 N = normalize(v + 1e-8);
  vec3 UpVector = abs(N.z) < 0.99999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  vec3 T = normalize(cross(UpVector, N));
  vec3 B = cross(N, T);
  return mat3(T, B, N);
}

vec3 get_vector(ivec3 cell_co)
{
  vec3 vector;

  vector.x = texelFetch(velocityX, cell_co, 0).r;
  vector.y = texelFetch(velocityY, cell_co, 0).r;
  vector.z = texelFetch(velocityZ, cell_co, 0).r;

  return vector;
}

/* Interpolate MAC information for cell-centered vectors. */
vec3 get_vector_centered(ivec3 cell_co)
{
  vec3 vector;

  vector.x = 0.5 * (texelFetch(velocityX, cell_co, 0).r +
                    texelFetch(velocityX, ivec3(cell_co.x + 1, cell_co.yz), 0).r);
  vector.y = 0.5 * (texelFetch(velocityY, cell_co, 0).r +
                    texelFetch(velocityY, ivec3(cell_co.x, cell_co.y + 1, cell_co.z), 0).r);
  vector.z = 0.5 * (texelFetch(velocityZ, cell_co, 0).r +
                    texelFetch(velocityZ, ivec3(cell_co.xy, cell_co.z + 1), 0).r);

  return vector;
}

/* Interpolate cell-centered information for MAC vectors. */
vec3 get_vector_mac(ivec3 cell_co)
{
  vec3 vector;

  vector.x = 0.5 * (texelFetch(velocityX, ivec3(cell_co.x - 1, cell_co.yz), 0).r +
                    texelFetch(velocityX, cell_co, 0).r);
  vector.y = 0.5 * (texelFetch(velocityY, ivec3(cell_co.x, cell_co.y - 1, cell_co.z), 0).r +
                    texelFetch(velocityY, cell_co, 0).r);
  vector.z = 0.5 * (texelFetch(velocityZ, ivec3(cell_co.xy, cell_co.z - 1), 0).r +
                    texelFetch(velocityZ, cell_co, 0).r);

  return vector;
}

void main()
{
#ifdef USE_NEEDLE
  int cell = gl_VertexID / 12;
#elif defined(USE_MAC)
  int cell = gl_VertexID / 6;
#else
  int cell = gl_VertexID / 2;
#endif

  ivec3 volume_size = textureSize(velocityX, 0);

  ivec3 cell_ofs = ivec3(0);
  ivec3 cell_div = volume_size;
  if (sliceAxis == 0) {
    cell_ofs.x = int(slicePosition * float(volume_size.x));
    cell_div.x = 1;
  }
  else if (sliceAxis == 1) {
    cell_ofs.y = int(slicePosition * float(volume_size.y));
    cell_div.y = 1;
  }
  else if (sliceAxis == 2) {
    cell_ofs.z = int(slicePosition * float(volume_size.z));
    cell_div.z = 1;
  }

  ivec3 cell_co;
  cell_co.x = cell % cell_div.x;
  cell_co.y = (cell / cell_div.x) % cell_div.y;
  cell_co.z = cell / (cell_div.x * cell_div.y);
  cell_co += cell_ofs;

  vec3 pos = domainOriginOffset + cellSize * (vec3(cell_co + adaptiveCellOffset) + 0.5);

  vec3 vector;

#ifdef USE_MAC
  vec3 color;
  vector = (isCellCentered) ? get_vector_mac(cell_co) : get_vector(cell_co);

  switch (gl_VertexID % 6) {
    case 0: /* Tail of X component. */
      pos.x += (drawMACX) ? -0.5 * cellSize.x : 0.0;
      color = vec3(1.0, 0.0, 0.0); /* red */
      break;
    case 1: /* Head of X component. */
      pos.x += (drawMACX) ? (-0.5 + vector.x * displaySize) * cellSize.x : 0.0;
      color = vec3(1.0, 1.0, 0.0); /* yellow */
      break;
    case 2: /* Tail of Y component. */
      pos.y += (drawMACY) ? -0.5 * cellSize.y : 0.0;
      color = vec3(0.0, 1.0, 0.0); /* green */
      break;
    case 3: /* Head of Y component. */
      pos.y += (drawMACY) ? (-0.5 + vector.y * displaySize) * cellSize.y : 0.0;
      color = vec3(1.0, 1.0, 0.0); /* yellow */
      break;
    case 4: /* Tail of Z component. */
      pos.z += (drawMACZ) ? -0.5 * cellSize.z : 0.0;
      color = vec3(0.0, 0.0, 1.0); /* blue */
      break;
    case 5: /* Head of Z component. */
      pos.z += (drawMACZ) ? (-0.5 + vector.z * displaySize) * cellSize.z : 0.0;
      color = vec3(1.0, 1.0, 0.0); /* yellow */
      break;
  }

  finalColor = vec4(color, 1.0);
#else
  vector = (isCellCentered) ? get_vector(cell_co) : get_vector_centered(cell_co);

  finalColor = vec4(weight_to_color(length(vector)), 1.0);

  float vector_length = 1.0;

  if (scaleWithMagnitude) {
    vector_length = length(vector);
  }
  else if (length(vector) == 0.0) {
    vector_length = 0.0;
  }

  mat3 rot_mat = rotation_from_vector(vector);

#  ifdef USE_NEEDLE
  /* NOTE(Metal): Declaring constant arrays in function scope to avoid increasing local shader
   * memory pressure. */
  const vec3 corners[4] = vec3[4](vec3(0.0, 0.2, -0.5),
                                  vec3(-0.2 * 0.866, -0.2 * 0.5, -0.5),
                                  vec3(0.2 * 0.866, -0.2 * 0.5, -0.5),
                                  vec3(0.0, 0.0, 0.5));

  const int indices[12] = int[12](0, 1, 1, 2, 2, 0, 0, 3, 1, 3, 2, 3);

  vec3 rotated_pos = rot_mat * corners[indices[gl_VertexID % 12]];
  pos += rotated_pos * vector_length * displaySize * cellSize;
#  else
  vec3 rotated_pos = rot_mat * vec3(0.0, 0.0, 1.0);
  pos += ((gl_VertexID % 2) == 1) ? rotated_pos * vector_length * displaySize * cellSize :
                                    vec3(0.0);
#  endif
#endif

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
}
