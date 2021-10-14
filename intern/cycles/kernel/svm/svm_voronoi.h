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

CCL_NAMESPACE_BEGIN

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

/* **** 1D Voronoi **** */

ccl_device float voronoi_distance_1d(float a,
                                     float b,
                                     NodeVoronoiDistanceMetric metric,
                                     float exponent)
{
  return fabsf(b - a);
}

ccl_device void voronoi_f1_1d(float w,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float *outW)
{
  float cellPosition = floorf(w);
  float localPosition = w - cellPosition;

  float minDistance = 8.0f;
  float targetOffset = 0.0f;
  float targetPosition = 0.0f;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = voronoi_distance_1d(pointPosition, localPosition, metric, exponent);
    if (distanceToPoint < minDistance) {
      targetOffset = cellOffset;
      minDistance = distanceToPoint;
      targetPosition = pointPosition;
    }
  }
  *outDistance = minDistance;
  *outColor = hash_float_to_float3(cellPosition + targetOffset);
  *outW = targetPosition + cellPosition;
}

ccl_device void voronoi_smooth_f1_1d(float w,
                                     float smoothness,
                                     float exponent,
                                     float randomness,
                                     NodeVoronoiDistanceMetric metric,
                                     ccl_private float *outDistance,
                                     ccl_private float3 *outColor,
                                     ccl_private float *outW)
{
  float cellPosition = floorf(w);
  float localPosition = w - cellPosition;

  float smoothDistance = 8.0f;
  float smoothPosition = 0.0f;
  float3 smoothColor = make_float3(0.0f, 0.0f, 0.0f);
  for (int i = -2; i <= 2; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = voronoi_distance_1d(pointPosition, localPosition, metric, exponent);
    float h = smoothstep(
        0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness);
    float correctionFactor = smoothness * h * (1.0f - h);
    smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
    correctionFactor /= 1.0f + 3.0f * smoothness;
    float3 cellColor = hash_float_to_float3(cellPosition + cellOffset);
    smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
    smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
  }
  *outDistance = smoothDistance;
  *outColor = smoothColor;
  *outW = cellPosition + smoothPosition;
}

ccl_device void voronoi_f2_1d(float w,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float *outW)
{
  float cellPosition = floorf(w);
  float localPosition = w - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float offsetF1 = 0.0f;
  float positionF1 = 0.0f;
  float offsetF2 = 0.0f;
  float positionF2 = 0.0f;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = voronoi_distance_1d(pointPosition, localPosition, metric, exponent);
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
  *outDistance = distanceF2;
  *outColor = hash_float_to_float3(cellPosition + offsetF2);
  *outW = positionF2 + cellPosition;
}

ccl_device void voronoi_distance_to_edge_1d(float w,
                                            float randomness,
                                            ccl_private float *outDistance)
{
  float cellPosition = floorf(w);
  float localPosition = w - cellPosition;

  float midPointPosition = hash_float_to_float(cellPosition) * randomness;
  float leftPointPosition = -1.0f + hash_float_to_float(cellPosition - 1.0f) * randomness;
  float rightPointPosition = 1.0f + hash_float_to_float(cellPosition + 1.0f) * randomness;
  float distanceToMidLeft = fabsf((midPointPosition + leftPointPosition) / 2.0f - localPosition);
  float distanceToMidRight = fabsf((midPointPosition + rightPointPosition) / 2.0f - localPosition);

  *outDistance = min(distanceToMidLeft, distanceToMidRight);
}

ccl_device void voronoi_n_sphere_radius_1d(float w, float randomness, ccl_private float *outRadius)
{
  float cellPosition = floorf(w);
  float localPosition = w - cellPosition;

  float closestPoint = 0.0f;
  float closestPointOffset = 0.0f;
  float minDistance = 8.0f;
  for (int i = -1; i <= 1; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = fabsf(pointPosition - localPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPoint = pointPosition;
      closestPointOffset = cellOffset;
    }
  }

  minDistance = 8.0f;
  float closestPointToClosestPoint = 0.0f;
  for (int i = -1; i <= 1; i++) {
    if (i == 0) {
      continue;
    }
    float cellOffset = i + closestPointOffset;
    float pointPosition = cellOffset + hash_float_to_float(cellPosition + cellOffset) * randomness;
    float distanceToPoint = fabsf(closestPoint - pointPosition);
    if (distanceToPoint < minDistance) {
      minDistance = distanceToPoint;
      closestPointToClosestPoint = pointPosition;
    }
  }
  *outRadius = fabsf(closestPointToClosestPoint - closestPoint) / 2.0f;
}

/* **** 2D Voronoi **** */

ccl_device float voronoi_distance_2d(float2 a,
                                     float2 b,
                                     NodeVoronoiDistanceMetric metric,
                                     float exponent)
{
  if (metric == NODE_VORONOI_EUCLIDEAN) {
    return distance(a, b);
  }
  else if (metric == NODE_VORONOI_MANHATTAN) {
    return fabsf(a.x - b.x) + fabsf(a.y - b.y);
  }
  else if (metric == NODE_VORONOI_CHEBYCHEV) {
    return max(fabsf(a.x - b.x), fabsf(a.y - b.y));
  }
  else if (metric == NODE_VORONOI_MINKOWSKI) {
    return powf(powf(fabsf(a.x - b.x), exponent) + powf(fabsf(a.y - b.y), exponent),
                1.0f / exponent);
  }
  else {
    return 0.0f;
  }
}

ccl_device void voronoi_f1_2d(float2 coord,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float2 *outPosition)
{
  float2 cellPosition = floor(coord);
  float2 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float2 targetOffset = make_float2(0.0f, 0.0f);
  float2 targetPosition = make_float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset = make_float2(i, j);
      float2 pointPosition = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance_2d(pointPosition, localPosition, metric, exponent);
      if (distanceToPoint < minDistance) {
        targetOffset = cellOffset;
        minDistance = distanceToPoint;
        targetPosition = pointPosition;
      }
    }
  }
  *outDistance = minDistance;
  *outColor = hash_float2_to_float3(cellPosition + targetOffset);
  *outPosition = targetPosition + cellPosition;
}

ccl_device void voronoi_smooth_f1_2d(float2 coord,
                                     float smoothness,
                                     float exponent,
                                     float randomness,
                                     NodeVoronoiDistanceMetric metric,
                                     ccl_private float *outDistance,
                                     ccl_private float3 *outColor,
                                     ccl_private float2 *outPosition)
{
  float2 cellPosition = floor(coord);
  float2 localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float3 smoothColor = make_float3(0.0f, 0.0f, 0.0f);
  float2 smoothPosition = make_float2(0.0f, 0.0f);
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      float2 cellOffset = make_float2(i, j);
      float2 pointPosition = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance_2d(pointPosition, localPosition, metric, exponent);
      float h = smoothstep(
          0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness);
      float correctionFactor = smoothness * h * (1.0f - h);
      smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
      correctionFactor /= 1.0f + 3.0f * smoothness;
      float3 cellColor = hash_float2_to_float3(cellPosition + cellOffset);
      smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
      smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
    }
  }
  *outDistance = smoothDistance;
  *outColor = smoothColor;
  *outPosition = cellPosition + smoothPosition;
}

ccl_device void voronoi_f2_2d(float2 coord,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float2 *outPosition)
{
  float2 cellPosition = floor(coord);
  float2 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float2 offsetF1 = make_float2(0.0f, 0.0f);
  float2 positionF1 = make_float2(0.0f, 0.0f);
  float2 offsetF2 = make_float2(0.0f, 0.0f);
  float2 positionF2 = make_float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset = make_float2(i, j);
      float2 pointPosition = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = voronoi_distance_2d(pointPosition, localPosition, metric, exponent);
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
  *outDistance = distanceF2;
  *outColor = hash_float2_to_float3(cellPosition + offsetF2);
  *outPosition = positionF2 + cellPosition;
}

ccl_device void voronoi_distance_to_edge_2d(float2 coord,
                                            float randomness,
                                            ccl_private float *outDistance)
{
  float2 cellPosition = floor(coord);
  float2 localPosition = coord - cellPosition;

  float2 vectorToClosest = make_float2(0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset = make_float2(i, j);
      float2 vectorToPoint = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness -
                             localPosition;
      float distanceToPoint = dot(vectorToPoint, vectorToPoint);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        vectorToClosest = vectorToPoint;
      }
    }
  }

  minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset = make_float2(i, j);
      float2 vectorToPoint = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness -
                             localPosition;
      float2 perpendicularToEdge = vectorToPoint - vectorToClosest;
      if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
        float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0f,
                                   normalize(perpendicularToEdge));
        minDistance = min(minDistance, distanceToEdge);
      }
    }
  }
  *outDistance = minDistance;
}

ccl_device void voronoi_n_sphere_radius_2d(float2 coord,
                                           float randomness,
                                           ccl_private float *outRadius)
{
  float2 cellPosition = floor(coord);
  float2 localPosition = coord - cellPosition;

  float2 closestPoint = make_float2(0.0f, 0.0f);
  float2 closestPointOffset = make_float2(0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      float2 cellOffset = make_float2(i, j);
      float2 pointPosition = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = distance(pointPosition, localPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPoint = pointPosition;
        closestPointOffset = cellOffset;
      }
    }
  }

  minDistance = 8.0f;
  float2 closestPointToClosestPoint = make_float2(0.0f, 0.0f);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      if (i == 0 && j == 0) {
        continue;
      }
      float2 cellOffset = make_float2(i, j) + closestPointOffset;
      float2 pointPosition = cellOffset +
                             hash_float2_to_float2(cellPosition + cellOffset) * randomness;
      float distanceToPoint = distance(closestPoint, pointPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPointToClosestPoint = pointPosition;
      }
    }
  }
  *outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** 3D Voronoi **** */

ccl_device float voronoi_distance_3d(float3 a,
                                     float3 b,
                                     NodeVoronoiDistanceMetric metric,
                                     float exponent)
{
  if (metric == NODE_VORONOI_EUCLIDEAN) {
    return distance(a, b);
  }
  else if (metric == NODE_VORONOI_MANHATTAN) {
    return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z);
  }
  else if (metric == NODE_VORONOI_CHEBYCHEV) {
    return max(fabsf(a.x - b.x), max(fabsf(a.y - b.y), fabsf(a.z - b.z)));
  }
  else if (metric == NODE_VORONOI_MINKOWSKI) {
    return powf(powf(fabsf(a.x - b.x), exponent) + powf(fabsf(a.y - b.y), exponent) +
                    powf(fabsf(a.z - b.z), exponent),
                1.0f / exponent);
  }
  else {
    return 0.0f;
  }
}

ccl_device void voronoi_f1_3d(float3 coord,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float3 *outPosition)
{
  float3 cellPosition = floor(coord);
  float3 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float3 targetOffset = make_float3(0.0f, 0.0f, 0.0f);
  float3 targetPosition = make_float3(0.0f, 0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset = make_float3(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = voronoi_distance_3d(
            pointPosition, localPosition, metric, exponent);
        if (distanceToPoint < minDistance) {
          targetOffset = cellOffset;
          minDistance = distanceToPoint;
          targetPosition = pointPosition;
        }
      }
    }
  }
  *outDistance = minDistance;
  *outColor = hash_float3_to_float3(cellPosition + targetOffset);
  *outPosition = targetPosition + cellPosition;
}

ccl_device void voronoi_smooth_f1_3d(float3 coord,
                                     float smoothness,
                                     float exponent,
                                     float randomness,
                                     NodeVoronoiDistanceMetric metric,
                                     ccl_private float *outDistance,
                                     ccl_private float3 *outColor,
                                     ccl_private float3 *outPosition)
{
  float3 cellPosition = floor(coord);
  float3 localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float3 smoothColor = make_float3(0.0f, 0.0f, 0.0f);
  float3 smoothPosition = make_float3(0.0f, 0.0f, 0.0f);
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        float3 cellOffset = make_float3(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = voronoi_distance_3d(
            pointPosition, localPosition, metric, exponent);
        float h = smoothstep(
            0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness);
        float correctionFactor = smoothness * h * (1.0f - h);
        smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
        correctionFactor /= 1.0f + 3.0f * smoothness;
        float3 cellColor = hash_float3_to_float3(cellPosition + cellOffset);
        smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
        smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
      }
    }
  }
  *outDistance = smoothDistance;
  *outColor = smoothColor;
  *outPosition = cellPosition + smoothPosition;
}

ccl_device void voronoi_f2_3d(float3 coord,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float3 *outPosition)
{
  float3 cellPosition = floor(coord);
  float3 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float3 offsetF1 = make_float3(0.0f, 0.0f, 0.0f);
  float3 positionF1 = make_float3(0.0f, 0.0f, 0.0f);
  float3 offsetF2 = make_float3(0.0f, 0.0f, 0.0f);
  float3 positionF2 = make_float3(0.0f, 0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset = make_float3(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = voronoi_distance_3d(
            pointPosition, localPosition, metric, exponent);
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
  *outDistance = distanceF2;
  *outColor = hash_float3_to_float3(cellPosition + offsetF2);
  *outPosition = positionF2 + cellPosition;
}

ccl_device void voronoi_distance_to_edge_3d(float3 coord,
                                            float randomness,
                                            ccl_private float *outDistance)
{
  float3 cellPosition = floor(coord);
  float3 localPosition = coord - cellPosition;

  float3 vectorToClosest = make_float3(0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset = make_float3(i, j, k);
        float3 vectorToPoint = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness -
                               localPosition;
        float distanceToPoint = dot(vectorToPoint, vectorToPoint);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          vectorToClosest = vectorToPoint;
        }
      }
    }
  }

  minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset = make_float3(i, j, k);
        float3 vectorToPoint = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness -
                               localPosition;
        float3 perpendicularToEdge = vectorToPoint - vectorToClosest;
        if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
          float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0f,
                                     normalize(perpendicularToEdge));
          minDistance = min(minDistance, distanceToEdge);
        }
      }
    }
  }
  *outDistance = minDistance;
}

ccl_device void voronoi_n_sphere_radius_3d(float3 coord,
                                           float randomness,
                                           ccl_private float *outRadius)
{
  float3 cellPosition = floor(coord);
  float3 localPosition = coord - cellPosition;

  float3 closestPoint = make_float3(0.0f, 0.0f, 0.0f);
  float3 closestPointOffset = make_float3(0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        float3 cellOffset = make_float3(i, j, k);
        float3 pointPosition = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = distance(pointPosition, localPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPoint = pointPosition;
          closestPointOffset = cellOffset;
        }
      }
    }
  }

  minDistance = 8.0f;
  float3 closestPointToClosestPoint = make_float3(0.0f, 0.0f, 0.0f);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0 && k == 0) {
          continue;
        }
        float3 cellOffset = make_float3(i, j, k) + closestPointOffset;
        float3 pointPosition = cellOffset +
                               hash_float3_to_float3(cellPosition + cellOffset) * randomness;
        float distanceToPoint = distance(closestPoint, pointPosition);
        if (distanceToPoint < minDistance) {
          minDistance = distanceToPoint;
          closestPointToClosestPoint = pointPosition;
        }
      }
    }
  }
  *outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

/* **** 4D Voronoi **** */

ccl_device float voronoi_distance_4d(float4 a,
                                     float4 b,
                                     NodeVoronoiDistanceMetric metric,
                                     float exponent)
{
  if (metric == NODE_VORONOI_EUCLIDEAN) {
    return distance(a, b);
  }
  else if (metric == NODE_VORONOI_MANHATTAN) {
    return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z) + fabsf(a.w - b.w);
  }
  else if (metric == NODE_VORONOI_CHEBYCHEV) {
    return max(fabsf(a.x - b.x), max(fabsf(a.y - b.y), max(fabsf(a.z - b.z), fabsf(a.w - b.w))));
  }
  else if (metric == NODE_VORONOI_MINKOWSKI) {
    return powf(powf(fabsf(a.x - b.x), exponent) + powf(fabsf(a.y - b.y), exponent) +
                    powf(fabsf(a.z - b.z), exponent) + powf(fabsf(a.w - b.w), exponent),
                1.0f / exponent);
  }
  else {
    return 0.0f;
  }
}

ccl_device void voronoi_f1_4d(float4 coord,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float4 *outPosition)
{
  float4 cellPosition = floor(coord);
  float4 localPosition = coord - cellPosition;

  float minDistance = 8.0f;
  float4 targetOffset = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 targetPosition = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      ccl_loop_no_unroll for (int j = -1; j <= 1; j++)
      {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset = make_float4(i, j, k, u);
          float4 pointPosition = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = voronoi_distance_4d(
              pointPosition, localPosition, metric, exponent);
          if (distanceToPoint < minDistance) {
            targetOffset = cellOffset;
            minDistance = distanceToPoint;
            targetPosition = pointPosition;
          }
        }
      }
    }
  }
  *outDistance = minDistance;
  *outColor = hash_float4_to_float3(cellPosition + targetOffset);
  *outPosition = targetPosition + cellPosition;
}

ccl_device void voronoi_smooth_f1_4d(float4 coord,
                                     float smoothness,
                                     float exponent,
                                     float randomness,
                                     NodeVoronoiDistanceMetric metric,
                                     ccl_private float *outDistance,
                                     ccl_private float3 *outColor,
                                     ccl_private float4 *outPosition)
{
  float4 cellPosition = floor(coord);
  float4 localPosition = coord - cellPosition;

  float smoothDistance = 8.0f;
  float3 smoothColor = make_float3(0.0f, 0.0f, 0.0f);
  float4 smoothPosition = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      ccl_loop_no_unroll for (int j = -2; j <= 2; j++)
      {
        for (int i = -2; i <= 2; i++) {
          float4 cellOffset = make_float4(i, j, k, u);
          float4 pointPosition = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = voronoi_distance_4d(
              pointPosition, localPosition, metric, exponent);
          float h = smoothstep(
              0.0f, 1.0f, 0.5f + 0.5f * (smoothDistance - distanceToPoint) / smoothness);
          float correctionFactor = smoothness * h * (1.0f - h);
          smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
          correctionFactor /= 1.0f + 3.0f * smoothness;
          float3 cellColor = hash_float4_to_float3(cellPosition + cellOffset);
          smoothColor = mix(smoothColor, cellColor, h) - correctionFactor;
          smoothPosition = mix(smoothPosition, pointPosition, h) - correctionFactor;
        }
      }
    }
  }
  *outDistance = smoothDistance;
  *outColor = smoothColor;
  *outPosition = cellPosition + smoothPosition;
}

ccl_device void voronoi_f2_4d(float4 coord,
                              float exponent,
                              float randomness,
                              NodeVoronoiDistanceMetric metric,
                              ccl_private float *outDistance,
                              ccl_private float3 *outColor,
                              ccl_private float4 *outPosition)
{
  float4 cellPosition = floor(coord);
  float4 localPosition = coord - cellPosition;

  float distanceF1 = 8.0f;
  float distanceF2 = 8.0f;
  float4 offsetF1 = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 positionF1 = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 offsetF2 = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 positionF2 = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      ccl_loop_no_unroll for (int j = -1; j <= 1; j++)
      {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset = make_float4(i, j, k, u);
          float4 pointPosition = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = voronoi_distance_4d(
              pointPosition, localPosition, metric, exponent);
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
  *outDistance = distanceF2;
  *outColor = hash_float4_to_float3(cellPosition + offsetF2);
  *outPosition = positionF2 + cellPosition;
}

ccl_device void voronoi_distance_to_edge_4d(float4 coord,
                                            float randomness,
                                            ccl_private float *outDistance)
{
  float4 cellPosition = floor(coord);
  float4 localPosition = coord - cellPosition;

  float4 vectorToClosest = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      ccl_loop_no_unroll for (int j = -1; j <= 1; j++)
      {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset = make_float4(i, j, k, u);
          float4 vectorToPoint = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness -
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

  minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      ccl_loop_no_unroll for (int j = -1; j <= 1; j++)
      {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset = make_float4(i, j, k, u);
          float4 vectorToPoint = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness -
                                 localPosition;
          float4 perpendicularToEdge = vectorToPoint - vectorToClosest;
          if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001f) {
            float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0f,
                                       normalize(perpendicularToEdge));
            minDistance = min(minDistance, distanceToEdge);
          }
        }
      }
    }
  }
  *outDistance = minDistance;
}

ccl_device void voronoi_n_sphere_radius_4d(float4 coord,
                                           float randomness,
                                           ccl_private float *outRadius)
{
  float4 cellPosition = floor(coord);
  float4 localPosition = coord - cellPosition;

  float4 closestPoint = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 closestPointOffset = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  float minDistance = 8.0f;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      ccl_loop_no_unroll for (int j = -1; j <= 1; j++)
      {
        for (int i = -1; i <= 1; i++) {
          float4 cellOffset = make_float4(i, j, k, u);
          float4 pointPosition = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness;
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

  minDistance = 8.0f;
  float4 closestPointToClosestPoint = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      ccl_loop_no_unroll for (int j = -1; j <= 1; j++)
      {
        for (int i = -1; i <= 1; i++) {
          if (i == 0 && j == 0 && k == 0 && u == 0) {
            continue;
          }
          float4 cellOffset = make_float4(i, j, k, u) + closestPointOffset;
          float4 pointPosition = cellOffset +
                                 hash_float4_to_float4(cellPosition + cellOffset) * randomness;
          float distanceToPoint = distance(closestPoint, pointPosition);
          if (distanceToPoint < minDistance) {
            minDistance = distanceToPoint;
            closestPointToClosestPoint = pointPosition;
          }
        }
      }
    }
  }
  *outRadius = distance(closestPointToClosestPoint, closestPoint) / 2.0f;
}

template<uint node_feature_mask>
ccl_device_noinline int svm_node_tex_voronoi(ccl_global const KernelGlobals *kg,
                                             ccl_private ShaderData *sd,
                                             ccl_private float *stack,
                                             uint dimensions,
                                             uint feature,
                                             uint metric,
                                             int offset)
{
  uint4 stack_offsets = read_node(kg, &offset);
  uint4 defaults = read_node(kg, &offset);

  uint coord_stack_offset, w_stack_offset, scale_stack_offset, smoothness_stack_offset;
  uint exponent_stack_offset, randomness_stack_offset, distance_out_stack_offset,
      color_out_stack_offset;
  uint position_out_stack_offset, w_out_stack_offset, radius_out_stack_offset;

  svm_unpack_node_uchar4(stack_offsets.x,
                         &coord_stack_offset,
                         &w_stack_offset,
                         &scale_stack_offset,
                         &smoothness_stack_offset);
  svm_unpack_node_uchar4(stack_offsets.y,
                         &exponent_stack_offset,
                         &randomness_stack_offset,
                         &distance_out_stack_offset,
                         &color_out_stack_offset);
  svm_unpack_node_uchar3(
      stack_offsets.z, &position_out_stack_offset, &w_out_stack_offset, &radius_out_stack_offset);

  float3 coord = stack_load_float3(stack, coord_stack_offset);
  float w = stack_load_float_default(stack, w_stack_offset, stack_offsets.w);
  float scale = stack_load_float_default(stack, scale_stack_offset, defaults.x);
  float smoothness = stack_load_float_default(stack, smoothness_stack_offset, defaults.y);
  float exponent = stack_load_float_default(stack, exponent_stack_offset, defaults.z);
  float randomness = stack_load_float_default(stack, randomness_stack_offset, defaults.w);

  NodeVoronoiFeature voronoi_feature = (NodeVoronoiFeature)feature;
  NodeVoronoiDistanceMetric voronoi_metric = (NodeVoronoiDistanceMetric)metric;

  float distance_out = 0.0f, w_out = 0.0f, radius_out = 0.0f;
  float3 color_out = make_float3(0.0f, 0.0f, 0.0f);
  float3 position_out = make_float3(0.0f, 0.0f, 0.0f);

  randomness = clamp(randomness, 0.0f, 1.0f);
  smoothness = clamp(smoothness / 2.0f, 0.0f, 0.5f);

  w *= scale;
  coord *= scale;

  switch (dimensions) {
    case 1: {
      switch (voronoi_feature) {
        case NODE_VORONOI_F1:
          voronoi_f1_1d(
              w, exponent, randomness, voronoi_metric, &distance_out, &color_out, &w_out);
          break;
        case NODE_VORONOI_SMOOTH_F1:
          voronoi_smooth_f1_1d(w,
                               smoothness,
                               exponent,
                               randomness,
                               voronoi_metric,
                               &distance_out,
                               &color_out,
                               &w_out);
          break;
        case NODE_VORONOI_F2:
          voronoi_f2_1d(
              w, exponent, randomness, voronoi_metric, &distance_out, &color_out, &w_out);
          break;
        case NODE_VORONOI_DISTANCE_TO_EDGE:
          voronoi_distance_to_edge_1d(w, randomness, &distance_out);
          break;
        case NODE_VORONOI_N_SPHERE_RADIUS:
          voronoi_n_sphere_radius_1d(w, randomness, &radius_out);
          break;
        default:
          kernel_assert(0);
      }
      w_out = safe_divide(w_out, scale);
      break;
    }
    case 2: {
      float2 coord_2d = make_float2(coord.x, coord.y);
      float2 position_out_2d;
      switch (voronoi_feature) {
        case NODE_VORONOI_F1:
          voronoi_f1_2d(coord_2d,
                        exponent,
                        randomness,
                        voronoi_metric,
                        &distance_out,
                        &color_out,
                        &position_out_2d);
          break;
        case NODE_VORONOI_SMOOTH_F1:
          if (KERNEL_NODES_FEATURE(VORONOI_EXTRA)) {
            voronoi_smooth_f1_2d(coord_2d,
                                 smoothness,
                                 exponent,
                                 randomness,
                                 voronoi_metric,
                                 &distance_out,
                                 &color_out,
                                 &position_out_2d);
          }
          break;
        case NODE_VORONOI_F2:
          voronoi_f2_2d(coord_2d,
                        exponent,
                        randomness,
                        voronoi_metric,
                        &distance_out,
                        &color_out,
                        &position_out_2d);
          break;
        case NODE_VORONOI_DISTANCE_TO_EDGE:
          voronoi_distance_to_edge_2d(coord_2d, randomness, &distance_out);
          break;
        case NODE_VORONOI_N_SPHERE_RADIUS:
          voronoi_n_sphere_radius_2d(coord_2d, randomness, &radius_out);
          break;
        default:
          kernel_assert(0);
      }
      position_out_2d = safe_divide_float2_float(position_out_2d, scale);
      position_out = make_float3(position_out_2d.x, position_out_2d.y, 0.0f);
      break;
    }
    case 3: {
      switch (voronoi_feature) {
        case NODE_VORONOI_F1:
          voronoi_f1_3d(coord,
                        exponent,
                        randomness,
                        voronoi_metric,
                        &distance_out,
                        &color_out,
                        &position_out);
          break;
        case NODE_VORONOI_SMOOTH_F1:
          if (KERNEL_NODES_FEATURE(VORONOI_EXTRA)) {
            voronoi_smooth_f1_3d(coord,
                                 smoothness,
                                 exponent,
                                 randomness,
                                 voronoi_metric,
                                 &distance_out,
                                 &color_out,
                                 &position_out);
          }
          break;
        case NODE_VORONOI_F2:
          voronoi_f2_3d(coord,
                        exponent,
                        randomness,
                        voronoi_metric,
                        &distance_out,
                        &color_out,
                        &position_out);
          break;
        case NODE_VORONOI_DISTANCE_TO_EDGE:
          voronoi_distance_to_edge_3d(coord, randomness, &distance_out);
          break;
        case NODE_VORONOI_N_SPHERE_RADIUS:
          voronoi_n_sphere_radius_3d(coord, randomness, &radius_out);
          break;
        default:
          kernel_assert(0);
      }
      position_out = safe_divide_float3_float(position_out, scale);
      break;
    }

    case 4: {
      if (KERNEL_NODES_FEATURE(VORONOI_EXTRA)) {
        float4 coord_4d = make_float4(coord.x, coord.y, coord.z, w);
        float4 position_out_4d;
        switch (voronoi_feature) {
          case NODE_VORONOI_F1:
            voronoi_f1_4d(coord_4d,
                          exponent,
                          randomness,
                          voronoi_metric,
                          &distance_out,
                          &color_out,
                          &position_out_4d);
            break;
          case NODE_VORONOI_SMOOTH_F1:
            voronoi_smooth_f1_4d(coord_4d,
                                 smoothness,
                                 exponent,
                                 randomness,
                                 voronoi_metric,
                                 &distance_out,
                                 &color_out,
                                 &position_out_4d);
            break;
          case NODE_VORONOI_F2:
            voronoi_f2_4d(coord_4d,
                          exponent,
                          randomness,
                          voronoi_metric,
                          &distance_out,
                          &color_out,
                          &position_out_4d);
            break;
          case NODE_VORONOI_DISTANCE_TO_EDGE:
            voronoi_distance_to_edge_4d(coord_4d, randomness, &distance_out);
            break;
          case NODE_VORONOI_N_SPHERE_RADIUS:
            voronoi_n_sphere_radius_4d(coord_4d, randomness, &radius_out);
            break;
          default:
            kernel_assert(0);
        }
        position_out_4d = safe_divide_float4_float(position_out_4d, scale);
        position_out = make_float3(position_out_4d.x, position_out_4d.y, position_out_4d.z);
        w_out = position_out_4d.w;
      }
      break;
    }
    default:
      kernel_assert(0);
  }

  if (stack_valid(distance_out_stack_offset))
    stack_store_float(stack, distance_out_stack_offset, distance_out);
  if (stack_valid(color_out_stack_offset))
    stack_store_float3(stack, color_out_stack_offset, color_out);
  if (stack_valid(position_out_stack_offset))
    stack_store_float3(stack, position_out_stack_offset, position_out);
  if (stack_valid(w_out_stack_offset))
    stack_store_float(stack, w_out_stack_offset, w_out);
  if (stack_valid(radius_out_stack_offset))
    stack_store_float(stack, radius_out_stack_offset, radius_out);
  return offset;
}

CCL_NAMESPACE_END
