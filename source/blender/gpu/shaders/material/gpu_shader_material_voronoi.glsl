#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

/*
 * Original code is under the MIT License, Copyright (c) 2013 Inigo Quilez.
 *
 * Smooth Voronoi:
 *
 * - https://wiki.blender.org/wiki/User:OmarSquircleArt/GSoC2019/Documentation/Smooth_Voronoi
 *
 * Distance To Edge based on:
 *
 * - https://www.iquilezles.org/www/articles/voronoilines/voronoilines.htm
 * - https://www.shadertoy.com/view/ldl3W8
 *
 * With optimization to change -2..2 scan window to -1..1 for better performance,
 * as explained in https://www.shadertoy.com/view/llG3zy.
 */

#define SHD_VORONOI_EUCLIDEAN 0
#define SHD_VORONOI_MANHATTAN 1
#define SHD_VORONOI_CHEBYCHEV 2
#define SHD_VORONOI_MINKOWSKI 3

#define SHD_VORONOI_F1 0
#define SHD_VORONOI_F2 1
#define SHD_VORONOI_SMOOTH_F1 2
#define SHD_VORONOI_DISTANCE_TO_EDGE 3
#define SHD_VORONOI_N_SPHERE_RADIUS 4

struct VoronoiParams {
  float scale;
  float detail;
  float roughness;
  float lacunarity;
  float smoothness;
  float exponent;
  float randomness;
  float max_distance;
  bool normalize;
  int feature;
  int metric;
};

struct VoronoiOutput {
  float Distance;
  vec3 Color;
  vec4 Position;
};

/* **** Distance Functions **** */

float voronoi_distance(float a, float b)
{
  return abs(a - b);
}

float voronoi_distance(vec2 a, vec2 b, VoronoiParams params)
{
  if (params.metric == SHD_VORONOI_EUCLIDEAN) {
    return distance(a, b);
  }
  else if (params.metric == SHD_VORONOI_MANHATTAN) {
    return abs(a.x - b.x) + abs(a.y - b.y);
  }
  else if (params.metric == SHD_VORONOI_CHEBYCHEV) {
    return max(abs(a.x - b.x), abs(a.y - b.y));
  }
  else if (params.metric == SHD_VORONOI_MINKOWSKI) {
    return pow(pow(abs(a.x - b.x), params.exponent) + pow(abs(a.y - b.y), params.exponent),
               1.0 / params.exponent);
  }
  else {
    return 0.0;
  }
}

float voronoi_distance(vec3 a, vec3 b, VoronoiParams params)
{
  if (params.metric == SHD_VORONOI_EUCLIDEAN) {
    return distance(a, b);
  }
  else if (params.metric == SHD_VORONOI_MANHATTAN) {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z);
  }
  else if (params.metric == SHD_VORONOI_CHEBYCHEV) {
    return max(abs(a.x - b.x), max(abs(a.y - b.y), abs(a.z - b.z)));
  }
  else if (params.metric == SHD_VORONOI_MINKOWSKI) {
    return pow(pow(abs(a.x - b.x), params.exponent) + pow(abs(a.y - b.y), params.exponent) +
                   pow(abs(a.z - b.z), params.exponent),
               1.0 / params.exponent);
  }
  else {
    return 0.0;
  }
}

float voronoi_distance(vec4 a, vec4 b, VoronoiParams params)
{
  if (params.metric == SHD_VORONOI_EUCLIDEAN) {
    return distance(a, b);
  }
  else if (params.metric == SHD_VORONOI_MANHATTAN) {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z) + abs(a.w - b.w);
  }
  else if (params.metric == SHD_VORONOI_CHEBYCHEV) {
    return max(abs(a.x - b.x), max(abs(a.y - b.y), max(abs(a.z - b.z), abs(a.w - b.w))));
  }
  else if (params.metric == SHD_VORONOI_MINKOWSKI) {
    return pow(pow(abs(a.x - b.x), params.exponent) + pow(abs(a.y - b.y), params.exponent) +
                   pow(abs(a.z - b.z), params.exponent) + pow(abs(a.w - b.w), params.exponent),
               1.0 / params.exponent);
  }
  else {
    return 0.0;
  }
}

/* **** 1D Voronoi **** */

vec4 voronoi_position(float coord)
{
  return vec4(0.0, 0.0, 0.0, coord);
}

VoronoiOutput voronoi_f1(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float minDistance = FLT_MAX;
  float targetOffset = 0.0;
  float targetPosition = 0.0;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    if (distanceToPoint < minDistance) {
      targetOffset = cellOffset;
      minDistance = distanceToPoint;
      targetPosition = pointPosition;
    }
  }

  VoronoiOutput octave;
  octave.Distance = minDistance;
  octave.Color = hash_float_to_vec3(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float smoothDistance = 0.0;
  float smoothPosition = 0.0;
  vec3 smoothColor = vec3(0.0);
  float h = -1.0;
  for (int i = -2; i <= 2; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    h = h == -1.0 ? 1.0 :
                    smoothstep(0.0,
                               1.0,
                               0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
    float correctionFactor = params.smoothness * h * (1.0 - h);
    smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
    correctionFactor /= 1.0 + 3.0 * params.smoothness;
    vec3 cellColor = hash_float_to_vec3(cellPosition + cellOffset);
    smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
    smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
  }

  VoronoiOutput octave;
  octave.Distance = smoothDistance;
  octave.Color = smoothColor;
  octave.Position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
  float offsetF1 = 0.0;
  float positionF1 = 0.0;
  float offsetF2 = 0.0;
  float positionF2 = 0.0;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    if (distanceToPoint < distanceF1) {
      distanceF2 = distanceF1;
      distanceF1 = distanceToPoint;
      offsetF2 = offsetF1;
      offsetF1 = cellOffset;
      positionF2 = positionF1;
      positionF1 = pointPosition;
    }
    else if (distanceToPoint < distanceF2) {
      distanceF2 = distanceToPoint;
      offsetF2 = cellOffset;
      positionF2 = pointPosition;
    }
  }

  VoronoiOutput octave;
  octave.Distance = distanceF2;
  octave.Color = hash_float_to_vec3(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float midPointPosition = hash_float_to_float(cellPosition) * params.randomness;
  float leftPointPosition = -1.0 + hash_float_to_float(cellPosition - 1.0) * params.randomness;
  float rightPointPosition = 1.0 + hash_float_to_float(cellPosition + 1.0) * params.randomness;
  float distanceToMidLeft = abs((midPointPosition + leftPointPosition) / 2.0 - localPosition);
  float distanceToMidRight = abs((midPointPosition + rightPointPosition) / 2.0 - localPosition);

  return min(distanceToMidLeft, distanceToMidRight);
}

float voronoi_n_sphere_radius(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float closestPoint = 0.0;
  float closestPointOffset = 0.0;
  float minDistance = FLT_MAX;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = abs(pointPosition - localPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPoint = pointPosition;
      closestPointOffset = cellOffset;
    }
  }

  minDistance = FLT_MAX;
  float closestPointToClosestPoint = 0.0;
  for (int i = -1; i <= 1; i++) {
    if (i == 0) {
      continue;
    }
    float cellOffset = i + closestPointOffset;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = abs(closestPoint - pointPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPointToClosestPoint = pointPosition;
    }
  }

  return abs(closestPointToClosestPoint - closestPoint) / 2.0;
}

/* **** 2D Voronoi **** */

vec4 voronoi_position(vec2 coord)
{
  return vec4(coord.x, coord.y, 0.0, 0.0);
}

VoronoiOutput voronoi_f1(VoronoiParams params, vec2 coord)
{
  vec2 cellPosition = floor(coord);
  vec2 localPosition = coord - cellPosition;

  float minDistance = FLT_MAX;
  vec2 targetOffset = vec2(0.0);
  vec2 targetPosition = vec2(0.0);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      if (distanceToPoint < minDistance) {
        targetOffset = cellOffset;
        minDistance = distanceToPoint;
        targetPosition = pointPosition;
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = minDistance;
  octave.Color = hash_vec2_to_vec3(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, vec2 coord)
{
  vec2 cellPosition = floor(coord);
  vec2 localPosition = coord - cellPosition;

  float smoothDistance = 0.0;
  vec3 smoothColor = vec3(0.0);
  vec2 smoothPosition = vec2(0.0);
  float h = -1.0;
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      h = h == -1.0 ?
              1.0 :
              smoothstep(
                  0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
      float correctionFactor = params.smoothness * h * (1.0 - h);
      smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
      correctionFactor /= 1.0 + 3.0 * params.smoothness;
      vec3 cellColor = hash_vec2_to_vec3(cellPosition + cellOffset);
      smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
      smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
    }
  }

  VoronoiOutput octave;
  octave.Distance = smoothDistance;
  octave.Color = smoothColor;
  octave.Position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(VoronoiParams params, vec2 coord)
{
  vec2 cellPosition = floor(coord);
  vec2 localPosition = coord - cellPosition;

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
  vec2 offsetF1 = vec2(0.0);
  vec2 positionF1 = vec2(0.0);
  vec2 offsetF2 = vec2(0.0);
  vec2 positionF2 = vec2(0.0);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      if (distanceToPoint < distanceF1) {
        distanceF2 = distanceF1;
        distanceF1 = distanceToPoint;
        offsetF2 = offsetF1;
        offsetF1 = cellOffset;
        positionF2 = positionF1;
        positionF1 = pointPosition;
      }
      else if (distanceToPoint < distanceF2) {
        distanceF2 = distanceToPoint;
        offsetF2 = cellOffset;
        positionF2 = pointPosition;
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = distanceF2;
  octave.Color = hash_vec2_to_vec3(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, vec2 coord)
{
  vec2 cellPosition = floor(coord);
  vec2 localPosition = coord - cellPosition;

  vec2 vectorToClosest = vec2(0.0);
  float minDistance = FLT_MAX;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 vectorToPoint = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness -
                           localPosition;
      float distanceToPoint = dot(vectorToPoint, vectorToPoint);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        vectorToClosest = vectorToPoint;
      }
    }
  }

  minDistance = FLT_MAX;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 vectorToPoint = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness -
                           localPosition;
      vec2 perpendicularToEdge = vectorToPoint - vectorToClosest;
      if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001) {
        float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0,
                                   normalize(perpendicularToEdge));
        minDistance = min(minDistance, distanceToEdge);
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(VoronoiParams params, vec2 coord)
{
  vec2 cellPosition = floor(coord);
  vec2 localPosition = coord - cellPosition;

  vec2 closestPoint = vec2(0.0);
  vec2 closestPointOffset = vec2(0.0);
  float minDistance = FLT_MAX;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = distance(pointPosition, localPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPoint = pointPosition;
        closestPointOffset = cellOffset;
      }
    }
  }

  minDistance = FLT_MAX;
  vec2 closestPointToClosestPoint = vec2(0.0);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      if (i == 0 && j == 0) {
        continue;
      }
      vec2 cellOffset = vec2(i, j) + closestPointOffset;
      vec2 pointPosition = cellOffset +
                           hash_vec2_to_vec2(cellPosition + cellOffset) * params.randomness;
      float distanceToPoint = distance(closestPoint, pointPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPointToClosestPoint = pointPosition;
      }
    }
  }

  return distance(closestPointToClosestPoint, closestPoint) / 2.0;
}

/* **** 3D Voronoi **** */

vec4 voronoi_position(vec3 coord)
{
  return vec4(coord.x, coord.y, coord.z, 0.0);
}

VoronoiOutput voronoi_f1(VoronoiParams params, vec3 coord)
{
  vec3 cellPosition = floor(coord);
  vec3 localPosition = coord - cellPosition;

  float minDistance = FLT_MAX;
  vec3 targetOffset = vec3(0.0);
  vec3 targetPosition = vec3(0.0);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        if (distanceToPoint < minDistance) {
          targetOffset = cellOffset;
          minDistance = distanceToPoint;
          targetPosition = pointPosition;
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = minDistance;
  octave.Color = hash_vec3_to_vec3(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, vec3 coord)
{
  vec3 cellPosition = floor(coord);
  vec3 localPosition = coord - cellPosition;

  float smoothDistance = 0.0;
  vec3 smoothColor = vec3(0.0);
  vec3 smoothPosition = vec3(0.0);
  float h = -1.0;
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        h = h == -1.0 ?
                1.0 :
                smoothstep(
                    0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
        float correctionFactor = params.smoothness * h * (1.0 - h);
        smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
        correctionFactor /= 1.0 + 3.0 * params.smoothness;
        vec3 cellColor = hash_vec3_to_vec3(cellPosition + cellOffset);
        smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
        smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = smoothDistance;
  octave.Color = smoothColor;
  octave.Position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(VoronoiParams params, vec3 coord)
{
  vec3 cellPosition = floor(coord);
  vec3 localPosition = coord - cellPosition;

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
  vec3 offsetF1 = vec3(0.0);
  vec3 positionF1 = vec3(0.0);
  vec3 offsetF2 = vec3(0.0);
  vec3 positionF2 = vec3(0.0);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        if (distanceToPoint < distanceF1) {
          distanceF2 = distanceF1;
          distanceF1 = distanceToPoint;
          offsetF2 = offsetF1;
          offsetF1 = cellOffset;
          positionF2 = positionF1;
          positionF1 = pointPosition;
        }
        else if (distanceToPoint < distanceF2) {
          distanceF2 = distanceToPoint;
          offsetF2 = cellOffset;
          positionF2 = pointPosition;
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = distanceF2;
  octave.Color = hash_vec3_to_vec3(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, vec3 coord)
{
  vec3 cellPosition = floor(coord);
  vec3 localPosition = coord - cellPosition;

  vec3 vectorToClosest = vec3(0.0);
  float minDistance = FLT_MAX;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 vectorToPoint = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness -
                             localPosition;
        float distanceToPoint = dot(vectorToPoint, vectorToPoint);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          vectorToClosest = vectorToPoint;
        }
      }
    }
  }

  minDistance = FLT_MAX;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 vectorToPoint = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness -
                             localPosition;
        vec3 perpendicularToEdge = vectorToPoint - vectorToClosest;
        if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001) {
          float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0,
                                     normalize(perpendicularToEdge));
          minDistance = min(minDistance, distanceToEdge);
        }
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(VoronoiParams params, vec3 coord)
{
  vec3 cellPosition = floor(coord);
  vec3 localPosition = coord - cellPosition;

  vec3 closestPoint = vec3(0.0);
  vec3 closestPointOffset = vec3(0.0);
  float minDistance = FLT_MAX;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = distance(pointPosition, localPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPoint = pointPosition;
          closestPointOffset = cellOffset;
        }
      }
    }
  }

  minDistance = FLT_MAX;
  vec3 closestPointToClosestPoint = vec3(0.0);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0 && k == 0) {
          continue;
        }
        vec3 cellOffset = vec3(i, j, k) + closestPointOffset;
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * params.randomness;
        float distanceToPoint = distance(closestPoint, pointPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPointToClosestPoint = pointPosition;
        }
      }
    }
  }

  return distance(closestPointToClosestPoint, closestPoint) / 2.0;
}

/* **** 4D Voronoi **** */

vec4 voronoi_position(vec4 coord)
{
  return coord;
}

VoronoiOutput voronoi_f1(VoronoiParams params, vec4 coord)
{
  vec4 cellPosition = floor(coord);
  vec4 localPosition = coord - cellPosition;

  float minDistance = FLT_MAX;
  vec4 targetOffset = vec4(0.0);
  vec4 targetPosition = vec4(0.0);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          if (distanceToPoint < minDistance) {
            targetOffset = cellOffset;
            minDistance = distanceToPoint;
            targetPosition = pointPosition;
          }
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = minDistance;
  octave.Color = hash_vec4_to_vec3(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, vec4 coord)
{
  vec4 cellPosition = floor(coord);
  vec4 localPosition = coord - cellPosition;

  float smoothDistance = 0.0;
  vec3 smoothColor = vec3(0.0);
  vec4 smoothPosition = vec4(0.0);
  float h = -1.0;
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          h = h == -1.0 ?
                  1.0 :
                  smoothstep(0.0,
                             1.0,
                             0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
          float correctionFactor = params.smoothness * h * (1.0 - h);
          smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
          correctionFactor /= 1.0 + 3.0 * params.smoothness;
          vec3 cellColor = hash_vec4_to_vec3(cellPosition + cellOffset);
          smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
          smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = smoothDistance;
  octave.Color = smoothColor;
  octave.Position = voronoi_position(cellPosition + smoothPosition);
  return octave;
}

VoronoiOutput voronoi_f2(VoronoiParams params, vec4 coord)
{
  vec4 cellPosition = floor(coord);
  vec4 localPosition = coord - cellPosition;

  float distanceF1 = FLT_MAX;
  float distanceF2 = FLT_MAX;
  vec4 offsetF1 = vec4(0.0);
  vec4 positionF1 = vec4(0.0);
  vec4 offsetF2 = vec4(0.0);
  vec4 positionF2 = vec4(0.0);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          if (distanceToPoint < distanceF1) {
            distanceF2 = distanceF1;
            distanceF1 = distanceToPoint;
            offsetF2 = offsetF1;
            offsetF1 = cellOffset;
            positionF2 = positionF1;
            positionF1 = pointPosition;
          }
          else if (distanceToPoint < distanceF2) {
            distanceF2 = distanceToPoint;
            offsetF2 = cellOffset;
            positionF2 = pointPosition;
          }
        }
      }
    }
  }

  VoronoiOutput octave;
  octave.Distance = distanceF2;
  octave.Color = hash_vec4_to_vec3(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, vec4 coord)
{
  vec4 cellPosition = floor(coord);
  vec4 localPosition = coord - cellPosition;

  vec4 vectorToClosest = vec4(0.0);
  float minDistance = FLT_MAX;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 vectorToPoint = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness -
                               localPosition;
          float distanceToPoint = dot(vectorToPoint, vectorToPoint);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            vectorToClosest = vectorToPoint;
          }
        }
      }
    }
  }

  minDistance = FLT_MAX;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 vectorToPoint = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness -
                               localPosition;
          vec4 perpendicularToEdge = vectorToPoint - vectorToClosest;
          if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001) {
            float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0,
                                       normalize(perpendicularToEdge));
            minDistance = min(minDistance, distanceToEdge);
          }
        }
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(VoronoiParams params, vec4 coord)
{
  vec4 cellPosition = floor(coord);
  vec4 localPosition = coord - cellPosition;

  vec4 closestPoint = vec4(0.0);
  vec4 closestPointOffset = vec4(0.0);
  float minDistance = FLT_MAX;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness;
          float distanceToPoint = distance(pointPosition, localPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPoint = pointPosition;
            closestPointOffset = cellOffset;
          }
        }
      }
    }
  }

  minDistance = FLT_MAX;
  vec4 closestPointToClosestPoint = vec4(0.0);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          if (i == 0 && j == 0 && k == 0 && u == 0) {
            continue;
          }
          vec4 cellOffset = vec4(i, j, k, u) + closestPointOffset;
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * params.randomness;
          float distanceToPoint = distance(closestPoint, pointPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPointToClosestPoint = pointPosition;
          }
        }
      }
    }
  }

  return distance(closestPointToClosestPoint, closestPoint) / 2.0;
}
