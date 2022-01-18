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
 *
 */

/* **** 1D Voronoi **** */

float voronoi_distance(float a, float b, float metric, float exponent)
{
  return distance(a, b);
}

void node_tex_voronoi_f1_1d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  float scaledCoord = w * scale;
  float cellPosition = floor(scaledCoord);
  float localPosition = scaledCoord - cellPosition;

  float minDistance = 8.0;
  float targetOffset, targetPosition;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = float(i);
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
    if (distanceToPoint < minDistance) {
      targetOffset = cellOffset;
      minDistance = distanceToPoint;
      targetPosition = pointPosition;
    }
  }
  outDistance = minDistance;
  outColor.xyz = hash_float_to_vec3(cellPosition + targetOffset);
  outW = safe_divide(targetPosition + cellPosition, scale);
}

void node_tex_voronoi_smooth_f1_1d(vec3 coord,
                                   float w,
                                   float scale,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   out float outDistance,
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);
  smoothness = clamp(smoothness / 2.0, 0.0, 0.5);

  float scaledCoord = w * scale;
  float cellPosition = floor(scaledCoord);
  float localPosition = scaledCoord - cellPosition;

  float smoothDistance = 8.0;
  float smoothPosition = 0.0;
  vec3 smoothColor = vec3(0.0);
  for (int i = -2; i <= 2; i++) {
    float cellOffset = float(i);
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
    float h = smoothstep(0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / smoothness);
    float correctionFactor = smoothness * h * (1.0 - h);
    smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
    correctionFactor /= 1.0 + 3.0 * smoothness;
    vec3 cellColor = hash_float_to_vec3(cellPosition + cellOffset);
    smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
    smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
  }
  outDistance = smoothDistance;
  outColor.xyz = smoothColor;
  outW = safe_divide(cellPosition + smoothPosition, scale);
}

void node_tex_voronoi_f2_1d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  float scaledCoord = w * scale;
  float cellPosition = floor(scaledCoord);
  float localPosition = scaledCoord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  float offsetF1 = 0.0;
  float positionF1 = 0.0;
  float offsetF2, positionF2;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = float(i);
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
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
  outDistance = distanceF2;
  outColor.xyz = hash_float_to_vec3(cellPosition + offsetF2);
  outW = safe_divide(positionF2 + cellPosition, scale);
}

void node_tex_voronoi_distance_to_edge_1d(vec3 coord,
                                          float w,
                                          float scale,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          out float outDistance,
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  float scaledCoord = w * scale;
  float cellPosition = floor(scaledCoord);
  float localPosition = scaledCoord - cellPosition;

  float midPointPosition = hash_float_to_float(cellPosition) * randomness;
  float leftPointPosition = -1.0 + hash_float_to_float(cellPosition - 1.0) * randomness;
  float rightPointPosition = 1.0 + hash_float_to_float(cellPosition + 1.0) * randomness;
  float distanceToMidLeft = distance((midPointPosition + leftPointPosition) / 2.0, localPosition);
  float distanceToMidRight = distance((midPointPosition + rightPointPosition) / 2.0,
                                      localPosition);

  outDistance = min(distanceToMidLeft, distanceToMidRight);
}

void node_tex_voronoi_n_sphere_radius_1d(vec3 coord,
                                         float w,
                                         float scale,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         out float outDistance,
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  float scaledCoord = w * scale;
  float cellPosition = floor(scaledCoord);
  float localPosition = scaledCoord - cellPosition;

  float closestPoint;
  float closestPointOffset;
  float minDistance = 8.0;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = float(i);
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = distance(pointPosition, localPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPoint = pointPosition;
      closestPointOffset = cellOffset;
    }
  }

  minDistance = 8.0;
  float closestPointToClosestPoint;
  for (int i = -1; i <= 1; i++) {
    if (i == 0) {
      continue;
    }
    float cellOffset = float(i) + closestPointOffset;
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = distance(closestPoint, pointPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPointToClosestPoint = pointPosition;
    }
  }
  outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0;
}

/* **** 2D Voronoi **** */

float voronoi_distance(vec2 a, vec2 b, float metric, float exponent)
{
  if (metric == 0.0)  // SHD_VORONOI_EUCLIDEAN
  {
    return distance(a, b);
  }
  else if (metric == 1.0)  // SHD_VORONOI_MANHATTAN
  {
    return abs(a.x - b.x) + abs(a.y - b.y);
  }
  else if (metric == 2.0)  // SHD_VORONOI_CHEBYCHEV
  {
    return max(abs(a.x - b.x), abs(a.y - b.y));
  }
  else if (metric == 3.0)  // SHD_VORONOI_MINKOWSKI
  {
    return pow(pow(abs(a.x - b.x), exponent) + pow(abs(a.y - b.y), exponent), 1.0 / exponent);
  }
  else {
    return 0.0;
  }
}

void node_tex_voronoi_f1_2d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec2 scaledCoord = coord.xy * scale;
  vec2 cellPosition = floor(scaledCoord);
  vec2 localPosition = scaledCoord - cellPosition;

  float minDistance = 8.0;
  vec2 targetOffset, targetPosition;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
      if (distanceToPoint < minDistance) {
        targetOffset = cellOffset;
        minDistance = distanceToPoint;
        targetPosition = pointPosition;
      }
    }
  }
  outDistance = minDistance;
  outColor.xyz = hash_vec2_to_vec3(cellPosition + targetOffset);
  outPosition = vec3(safe_divide(targetPosition + cellPosition, scale), 0.0);
}

void node_tex_voronoi_smooth_f1_2d(vec3 coord,
                                   float w,
                                   float scale,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   out float outDistance,
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);
  smoothness = clamp(smoothness / 2.0, 0.0, 0.5);

  vec2 scaledCoord = coord.xy * scale;
  vec2 cellPosition = floor(scaledCoord);
  vec2 localPosition = scaledCoord - cellPosition;

  float smoothDistance = 8.0;
  vec3 smoothColor = vec3(0.0);
  vec2 smoothPosition = vec2(0.0);
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
      float h = smoothstep(0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / smoothness);
      float correctionFactor = smoothness * h * (1.0 - h);
      smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
      correctionFactor /= 1.0 + 3.0 * smoothness;
      vec3 cellColor = hash_vec2_to_vec3(cellPosition + cellOffset);
      smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
      smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
    }
  }
  outDistance = smoothDistance;
  outColor.xyz = smoothColor;
  outPosition = vec3(safe_divide(cellPosition + smoothPosition, scale), 0.0);
}

void node_tex_voronoi_f2_2d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec2 scaledCoord = coord.xy * scale;
  vec2 cellPosition = floor(scaledCoord);
  vec2 localPosition = scaledCoord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  vec2 offsetF1 = vec2(0.0);
  vec2 positionF1 = vec2(0.0);
  vec2 offsetF2, positionF2;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
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
  outDistance = distanceF2;
  outColor.xyz = hash_vec2_to_vec3(cellPosition + offsetF2);
  outPosition = vec3(safe_divide(positionF2 + cellPosition, scale), 0.0);
}

void node_tex_voronoi_distance_to_edge_2d(vec3 coord,
                                          float w,
                                          float scale,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          out float outDistance,
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec2 scaledCoord = coord.xy * scale;
  vec2 cellPosition = floor(scaledCoord);
  vec2 localPosition = scaledCoord - cellPosition;

  vec2 vectorToClosest;
  float minDistance = 8.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 vectorToPoint = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness -
                           localPosition;
      float distanceToPoint = dot(vectorToPoint, vectorToPoint);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        vectorToClosest = vectorToPoint;
      }
    }
  }

  minDistance = 8.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 vectorToPoint = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness -
                           localPosition;
      vec2 perpendicularToEdge = vectorToPoint - vectorToClosest;
      if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001) {
        float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0,
                                   normalize(perpendicularToEdge));
        minDistance = min(minDistance, distanceToEdge);
      }
    }
  }
  outDistance = minDistance;
}

void node_tex_voronoi_n_sphere_radius_2d(vec3 coord,
                                         float w,
                                         float scale,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         out float outDistance,
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec2 scaledCoord = coord.xy * scale;
  vec2 cellPosition = floor(scaledCoord);
  vec2 localPosition = scaledCoord - cellPosition;

  vec2 closestPoint;
  vec2 closestPointOffset;
  float minDistance = 8.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 cellOffset = vec2(i, j);
      vec2 pointPosition = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = distance(pointPosition, localPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPoint = pointPosition;
        closestPointOffset = cellOffset;
      }
    }
  }

  minDistance = 8.0;
  vec2 closestPointToClosestPoint;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      if (i == 0 && j == 0) {
        continue;
      }
      vec2 cellOffset = vec2(i, j) + closestPointOffset;
      vec2 pointPosition = cellOffset + hash_vec2_to_vec2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = distance(closestPoint, pointPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPointToClosestPoint = pointPosition;
      }
    }
  }
  outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0;
}

/* **** 3D Voronoi **** */

float voronoi_distance(vec3 a, vec3 b, float metric, float exponent)
{
  if (metric == 0.0)  // SHD_VORONOI_EUCLIDEAN
  {
    return distance(a, b);
  }
  else if (metric == 1.0)  // SHD_VORONOI_MANHATTAN
  {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z);
  }
  else if (metric == 2.0)  // SHD_VORONOI_CHEBYCHEV
  {
    return max(abs(a.x - b.x), max(abs(a.y - b.y), abs(a.z - b.z)));
  }
  else if (metric == 3.0)  // SHD_VORONOI_MINKOWSKI
  {
    return pow(pow(abs(a.x - b.x), exponent) + pow(abs(a.y - b.y), exponent) +
                   pow(abs(a.z - b.z), exponent),
               1.0 / exponent);
  }
  else {
    return 0.0;
  }
}

void node_tex_voronoi_f1_3d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec3 scaledCoord = coord * scale;
  vec3 cellPosition = floor(scaledCoord);
  vec3 localPosition = scaledCoord - cellPosition;

  float minDistance = 8.0;
  vec3 targetOffset, targetPosition;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
        if (distanceToPoint < minDistance) {
          targetOffset = cellOffset;
          minDistance = distanceToPoint;
          targetPosition = pointPosition;
        }
      }
    }
  }
  outDistance = minDistance;
  outColor.xyz = hash_vec3_to_vec3(cellPosition + targetOffset);
  outPosition = safe_divide(targetPosition + cellPosition, scale);
}

void node_tex_voronoi_smooth_f1_3d(vec3 coord,
                                   float w,
                                   float scale,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   out float outDistance,
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);
  smoothness = clamp(smoothness / 2.0, 0.0, 0.5);

  vec3 scaledCoord = coord * scale;
  vec3 cellPosition = floor(scaledCoord);
  vec3 localPosition = scaledCoord - cellPosition;

  float smoothDistance = 8.0;
  vec3 smoothColor = vec3(0.0);
  vec3 smoothPosition = vec3(0.0);
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
        float h = smoothstep(
            0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / smoothness);
        float correctionFactor = smoothness * h * (1.0 - h);
        smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
        correctionFactor /= 1.0 + 3.0 * smoothness;
        vec3 cellColor = hash_vec3_to_vec3(cellPosition + cellOffset);
        smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
        smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
      }
    }
  }
  outDistance = smoothDistance;
  outColor.xyz = smoothColor;
  outPosition = safe_divide(cellPosition + smoothPosition, scale);
}

void node_tex_voronoi_f2_3d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec3 scaledCoord = coord * scale;
  vec3 cellPosition = floor(scaledCoord);
  vec3 localPosition = scaledCoord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  vec3 offsetF1 = vec3(0.0);
  vec3 positionF1 = vec3(0.0);
  vec3 offsetF2, positionF2;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
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
  outDistance = distanceF2;
  outColor.xyz = hash_vec3_to_vec3(cellPosition + offsetF2);
  outPosition = safe_divide(positionF2 + cellPosition, scale);
}

void node_tex_voronoi_distance_to_edge_3d(vec3 coord,
                                          float w,
                                          float scale,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          out float outDistance,
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec3 scaledCoord = coord * scale;
  vec3 cellPosition = floor(scaledCoord);
  vec3 localPosition = scaledCoord - cellPosition;

  vec3 vectorToClosest;
  float minDistance = 8.0;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 vectorToPoint = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness -
                             localPosition;
        float distanceToPoint = dot(vectorToPoint, vectorToPoint);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          vectorToClosest = vectorToPoint;
        }
      }
    }
  }

  minDistance = 8.0;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 vectorToPoint = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness -
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
  outDistance = minDistance;
}

void node_tex_voronoi_n_sphere_radius_3d(vec3 coord,
                                         float w,
                                         float scale,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         out float outDistance,
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec3 scaledCoord = coord * scale;
  vec3 cellPosition = floor(scaledCoord);
  vec3 localPosition = scaledCoord - cellPosition;

  vec3 closestPoint;
  vec3 closestPointOffset;
  float minDistance = 8.0;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vec3 cellOffset = vec3(i, j, k);
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = distance(pointPosition, localPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPoint = pointPosition;
          closestPointOffset = cellOffset;
        }
      }
    }
  }

  minDistance = 8.0;
  vec3 closestPointToClosestPoint;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0 && k == 0) {
          continue;
        }
        vec3 cellOffset = vec3(i, j, k) + closestPointOffset;
        vec3 pointPosition = cellOffset +
                             hash_vec3_to_vec3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = distance(closestPoint, pointPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPointToClosestPoint = pointPosition;
        }
      }
    }
  }
  outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0;
}

/* **** 4D Voronoi **** */

float voronoi_distance(vec4 a, vec4 b, float metric, float exponent)
{
  if (metric == 0.0)  // SHD_VORONOI_EUCLIDEAN
  {
    return distance(a, b);
  }
  else if (metric == 1.0)  // SHD_VORONOI_MANHATTAN
  {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z) + abs(a.w - b.w);
  }
  else if (metric == 2.0)  // SHD_VORONOI_CHEBYCHEV
  {
    return max(abs(a.x - b.x), max(abs(a.y - b.y), max(abs(a.z - b.z), abs(a.w - b.w))));
  }
  else if (metric == 3.0)  // SHD_VORONOI_MINKOWSKI
  {
    return pow(pow(abs(a.x - b.x), exponent) + pow(abs(a.y - b.y), exponent) +
                   pow(abs(a.z - b.z), exponent) + pow(abs(a.w - b.w), exponent),
               1.0 / exponent);
  }
  else {
    return 0.0;
  }
}

void node_tex_voronoi_f1_4d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec4 scaledCoord = vec4(coord, w) * scale;
  vec4 cellPosition = floor(scaledCoord);
  vec4 localPosition = scaledCoord - cellPosition;

  float minDistance = 8.0;
  vec4 targetOffset, targetPosition;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
          if (distanceToPoint < minDistance) {
            targetOffset = cellOffset;
            minDistance = distanceToPoint;
            targetPosition = pointPosition;
          }
        }
      }
    }
  }
  outDistance = minDistance;
  outColor.xyz = hash_vec4_to_vec3(cellPosition + targetOffset);
  vec4 p = safe_divide(targetPosition + cellPosition, scale);
  outPosition = p.xyz;
  outW = p.w;
}

void node_tex_voronoi_smooth_f1_4d(vec3 coord,
                                   float w,
                                   float scale,
                                   float smoothness,
                                   float exponent,
                                   float randomness,
                                   float metric,
                                   out float outDistance,
                                   out vec4 outColor,
                                   out vec3 outPosition,
                                   out float outW,
                                   out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);
  smoothness = clamp(smoothness / 2.0, 0.0, 0.5);

  vec4 scaledCoord = vec4(coord, w) * scale;
  vec4 cellPosition = floor(scaledCoord);
  vec4 localPosition = scaledCoord - cellPosition;

  float smoothDistance = 8.0;
  vec3 smoothColor = vec3(0.0);
  vec4 smoothPosition = vec4(0.0);
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
          float h = smoothstep(
              0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / smoothness);
          float correctionFactor = smoothness * h * (1.0 - h);
          smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
          correctionFactor /= 1.0 + 3.0 * smoothness;
          vec3 cellColor = hash_vec4_to_vec3(cellPosition + cellOffset);
          smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
          smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
        }
      }
    }
  }
  outDistance = smoothDistance;
  outColor.xyz = smoothColor;
  vec4 p = safe_divide(cellPosition + smoothPosition, scale);
  outPosition = p.xyz;
  outW = p.w;
}

void node_tex_voronoi_f2_4d(vec3 coord,
                            float w,
                            float scale,
                            float smoothness,
                            float exponent,
                            float randomness,
                            float metric,
                            out float outDistance,
                            out vec4 outColor,
                            out vec3 outPosition,
                            out float outW,
                            out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec4 scaledCoord = vec4(coord, w) * scale;
  vec4 cellPosition = floor(scaledCoord);
  vec4 localPosition = scaledCoord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  vec4 offsetF1 = vec4(0.0);
  vec4 positionF1 = vec4(0.0);
  vec4 offsetF2, positionF2;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, metric, exponent);
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
  outDistance = distanceF2;
  outColor.xyz = hash_vec4_to_vec3(cellPosition + offsetF2);
  vec4 p = safe_divide(positionF2 + cellPosition, scale);
  outPosition = p.xyz;
  outW = p.w;
}

void node_tex_voronoi_distance_to_edge_4d(vec3 coord,
                                          float w,
                                          float scale,
                                          float smoothness,
                                          float exponent,
                                          float randomness,
                                          float metric,
                                          out float outDistance,
                                          out vec4 outColor,
                                          out vec3 outPosition,
                                          out float outW,
                                          out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec4 scaledCoord = vec4(coord, w) * scale;
  vec4 cellPosition = floor(scaledCoord);
  vec4 localPosition = scaledCoord - cellPosition;

  vec4 vectorToClosest;
  float minDistance = 8.0;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 vectorToPoint = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness -
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

  minDistance = 8.0;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 vectorToPoint = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness -
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
  outDistance = minDistance;
}

void node_tex_voronoi_n_sphere_radius_4d(vec3 coord,
                                         float w,
                                         float scale,
                                         float smoothness,
                                         float exponent,
                                         float randomness,
                                         float metric,
                                         out float outDistance,
                                         out vec4 outColor,
                                         out vec3 outPosition,
                                         out float outW,
                                         out float outRadius)
{
  randomness = clamp(randomness, 0.0, 1.0);

  vec4 scaledCoord = vec4(coord, w) * scale;
  vec4 cellPosition = floor(scaledCoord);
  vec4 localPosition = scaledCoord - cellPosition;

  vec4 closestPoint;
  vec4 closestPointOffset;
  float minDistance = 8.0;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vec4 cellOffset = vec4(i, j, k, u);
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness;
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

  minDistance = 8.0;
  vec4 closestPointToClosestPoint;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          if (i == 0 && j == 0 && k == 0 && u == 0) {
            continue;
          }
          vec4 cellOffset = vec4(i, j, k, u) + closestPointOffset;
          vec4 pointPosition = cellOffset +
                               hash_vec4_to_vec4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = distance(closestPoint, pointPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPointToClosestPoint = pointPosition;
          }
        }
      }
    }
  }
  outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0;
}
