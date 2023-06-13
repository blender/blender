/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "node_hash.h"
#include "stdcycles.h"
#include "vector2.h"
#include "vector4.h"

#define vector3 point

struct VoronoiParams {
  float scale;
  float detail;
  float roughness;
  float lacunarity;
  float smoothness;
  float exponent;
  float randomness;
  float max_distance;
  int normalize;
  string feature;
  string metric;
};

struct VoronoiOutput {
  float Distance;
  color Color;
  vector4 Position;
};

/* **** Distance Functions **** */

float distance(float a, float b)
{
  return abs(a - b);
}

float distance(vector2 a, vector2 b)
{
  return length(a - b);
}

float distance(vector4 a, vector4 b)
{
  return length(a - b);
}

float voronoi_distance(float a, float b)
{
  return abs(a - b);
}

float voronoi_distance(vector2 a, vector2 b, VoronoiParams params)
{
  if (params.metric == "euclidean") {
    return distance(a, b);
  }
  else if (params.metric == "manhattan") {
    return abs(a.x - b.x) + abs(a.y - b.y);
  }
  else if (params.metric == "chebychev") {
    return max(abs(a.x - b.x), abs(a.y - b.y));
  }
  else if (params.metric == "minkowski") {
    return pow(pow(abs(a.x - b.x), params.exponent) + pow(abs(a.y - b.y), params.exponent),
               1.0 / params.exponent);
  }
  else {
    return 0.0;
  }
}

float voronoi_distance(vector3 a, vector3 b, VoronoiParams params)
{
  if (params.metric == "euclidean") {
    return distance(a, b);
  }
  else if (params.metric == "manhattan") {
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
  }
  else if (params.metric == "chebychev") {
    return max(abs(a[0] - b[0]), max(abs(a[1] - b[1]), abs(a[2] - b[2])));
  }
  else if (params.metric == "minkowski") {
    return pow(pow(abs(a[0] - b[0]), params.exponent) + pow(abs(a[1] - b[1]), params.exponent) +
                   pow(abs(a[2] - b[2]), params.exponent),
               1.0 / params.exponent);
  }
  else {
    return 0.0;
  }
}

float voronoi_distance(vector4 a, vector4 b, VoronoiParams params)
{
  if (params.metric == "euclidean") {
    return distance(a, b);
  }
  else if (params.metric == "manhattan") {
    return abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z) + abs(a.w - b.w);
  }
  else if (params.metric == "chebychev") {
    return max(abs(a.x - b.x), max(abs(a.y - b.y), max(abs(a.z - b.z), abs(a.w - b.w))));
  }
  else if (params.metric == "minkowski") {
    return pow(pow(abs(a.x - b.x), params.exponent) + pow(abs(a.y - b.y), params.exponent) +
                   pow(abs(a.z - b.z), params.exponent) + pow(abs(a.w - b.w), params.exponent),
               1.0 / params.exponent);
  }
  else {
    return 0.0;
  }
}

/* **** Safe Division **** */

vector2 safe_divide(vector2 a, float b)
{
  return vector2((b != 0.0) ? a.x / b : 0.0, (b != 0.0) ? a.y / b : 0.0);
}

vector4 safe_divide(vector4 a, float b)
{
  return vector4((b != 0.0) ? a.x / b : 0.0,
                 (b != 0.0) ? a.y / b : 0.0,
                 (b != 0.0) ? a.z / b : 0.0,
                 (b != 0.0) ? a.w / b : 0.0);
}

/*
 * SPDX-License-Identifier: MIT
 * Original code is copyright (c) 2013 Inigo Quilez.
 *
 * Smooth Voronoi:
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

vector4 voronoi_position(float coord)
{
  return vector4(0.0, 0.0, 0.0, coord);
}

VoronoiOutput voronoi_f1(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float minDistance = 8.0;
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
  octave.Color = hash_float_to_color(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, float coord)
{
  float cellPosition = floor(coord);
  float localPosition = coord - cellPosition;

  float smoothDistance = 8.0;
  float smoothPosition = 0.0;
  vector3 smoothColor = vector3(0.0, 0.0, 0.0);
  for (int i = -2; i <= 2; i++) {
    float cellOffset = i;
    float pointPosition = cellOffset +
                          hash_float_to_float(cellPosition + cellOffset) * params.randomness;
    float distanceToPoint = voronoi_distance(pointPosition, localPosition);
    float h = smoothstep(
        0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
    float correctionFactor = params.smoothness * h * (1.0 - h);
    smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
    correctionFactor /= 1.0 + 3.0 * params.smoothness;
    color cellColor = hash_float_to_color(cellPosition + cellOffset);
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

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
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
  octave.Color = hash_float_to_color(cellPosition + offsetF2);
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
  float minDistance = 8.0;
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

  minDistance = 8.0;
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

vector4 voronoi_position(vector2 coord)
{
  return vector4(coord.x, coord.y, 0.0, 0.0);
}

VoronoiOutput voronoi_f1(VoronoiParams params, vector2 coord)
{
  vector2 cellPosition = floor(coord);
  vector2 localPosition = coord - cellPosition;

  float minDistance = 8.0;
  vector2 targetOffset = vector2(0.0, 0.0);
  vector2 targetPosition = vector2(0.0, 0.0);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vector2 cellOffset = vector2(i, j);
      vector2 pointPosition = cellOffset + hash_vector2_to_vector2(cellPosition + cellOffset) *
                                               params.randomness;
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
  octave.Color = hash_vector2_to_color(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, vector2 coord)
{
  vector2 cellPosition = floor(coord);
  vector2 localPosition = coord - cellPosition;

  float smoothDistance = 8.0;
  vector3 smoothColor = vector3(0.0, 0.0, 0.0);
  vector2 smoothPosition = vector2(0.0, 0.0);
  for (int j = -2; j <= 2; j++) {
    for (int i = -2; i <= 2; i++) {
      vector2 cellOffset = vector2(i, j);
      vector2 pointPosition = cellOffset + hash_vector2_to_vector2(cellPosition + cellOffset) *
                                               params.randomness;
      float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
      float h = smoothstep(
          0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
      float correctionFactor = params.smoothness * h * (1.0 - h);
      smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
      correctionFactor /= 1.0 + 3.0 * params.smoothness;
      color cellColor = hash_vector2_to_color(cellPosition + cellOffset);
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

VoronoiOutput voronoi_f2(VoronoiParams params, vector2 coord)
{
  vector2 cellPosition = floor(coord);
  vector2 localPosition = coord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  vector2 offsetF1 = vector2(0.0, 0.0);
  vector2 positionF1 = vector2(0.0, 0.0);
  vector2 offsetF2 = vector2(0.0, 0.0);
  vector2 positionF2 = vector2(0.0, 0.0);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vector2 cellOffset = vector2(i, j);
      vector2 pointPosition = cellOffset + hash_vector2_to_vector2(cellPosition + cellOffset) *
                                               params.randomness;
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
  octave.Color = hash_vector2_to_color(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, vector2 coord)
{
  vector2 cellPosition = floor(coord);
  vector2 localPosition = coord - cellPosition;

  vector2 vectorToClosest = vector2(0.0, 0.0);
  float minDistance = 8.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vector2 cellOffset = vector2(i, j);
      vector2 vectorToPoint = cellOffset +
                              hash_vector2_to_vector2(cellPosition + cellOffset) *
                                  params.randomness -
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
      vector2 cellOffset = vector2(i, j);
      vector2 vectorToPoint = cellOffset +
                              hash_vector2_to_vector2(cellPosition + cellOffset) *
                                  params.randomness -
                              localPosition;
      vector2 perpendicularToEdge = vectorToPoint - vectorToClosest;
      if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001) {
        float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0,
                                   normalize(perpendicularToEdge));
        minDistance = min(minDistance, distanceToEdge);
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(VoronoiParams params, vector2 coord)
{
  vector2 cellPosition = floor(coord);
  vector2 localPosition = coord - cellPosition;

  vector2 closestPoint = vector2(0.0, 0.0);
  vector2 closestPointOffset = vector2(0.0, 0.0);
  float minDistance = 8.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vector2 cellOffset = vector2(i, j);
      vector2 pointPosition = cellOffset + hash_vector2_to_vector2(cellPosition + cellOffset) *
                                               params.randomness;
      float distanceToPoint = distance(pointPosition, localPosition);
      if (distanceToPoint < minDistance) {
        minDistance = distanceToPoint;
        closestPoint = pointPosition;
        closestPointOffset = cellOffset;
      }
    }
  }

  minDistance = 8.0;
  vector2 closestPointToClosestPoint = vector2(0.0, 0.0);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      if (i == 0 && j == 0) {
        continue;
      }
      vector2 cellOffset = vector2(i, j) + closestPointOffset;
      vector2 pointPosition = cellOffset + hash_vector2_to_vector2(cellPosition + cellOffset) *
                                               params.randomness;
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

vector4 voronoi_position(vector3 coord)
{
  return vector4(coord.x, coord.y, coord.z, 0.0);
}

VoronoiOutput voronoi_f1(VoronoiParams params, vector3 coord)
{
  vector3 cellPosition = floor(coord);
  vector3 localPosition = coord - cellPosition;

  float minDistance = 8.0;
  vector3 targetOffset = vector3(0.0, 0.0, 0.0);
  vector3 targetPosition = vector3(0.0, 0.0, 0.0);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vector3 cellOffset = vector3(i, j, k);
        vector3 pointPosition = cellOffset + hash_vector3_to_vector3(cellPosition + cellOffset) *
                                                 params.randomness;
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
  octave.Color = hash_vector3_to_color(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, vector3 coord)
{
  vector3 cellPosition = floor(coord);
  vector3 localPosition = coord - cellPosition;

  float smoothDistance = 8.0;
  vector3 smoothColor = vector3(0.0, 0.0, 0.0);
  vector3 smoothPosition = vector3(0.0, 0.0, 0.0);
  for (int k = -2; k <= 2; k++) {
    for (int j = -2; j <= 2; j++) {
      for (int i = -2; i <= 2; i++) {
        vector3 cellOffset = vector3(i, j, k);
        vector3 pointPosition = cellOffset + hash_vector3_to_vector3(cellPosition + cellOffset) *
                                                 params.randomness;
        float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
        float h = smoothstep(
            0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
        float correctionFactor = params.smoothness * h * (1.0 - h);
        smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
        correctionFactor /= 1.0 + 3.0 * params.smoothness;
        color cellColor = hash_vector3_to_color(cellPosition + cellOffset);
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

VoronoiOutput voronoi_f2(VoronoiParams params, vector3 coord)
{
  vector3 cellPosition = floor(coord);
  vector3 localPosition = coord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  vector3 offsetF1 = vector3(0.0, 0.0, 0.0);
  vector3 positionF1 = vector3(0.0, 0.0, 0.0);
  vector3 offsetF2 = vector3(0.0, 0.0, 0.0);
  vector3 positionF2 = vector3(0.0, 0.0, 0.0);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vector3 cellOffset = vector3(i, j, k);
        vector3 pointPosition = cellOffset + hash_vector3_to_vector3(cellPosition + cellOffset) *
                                                 params.randomness;
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
  octave.Color = hash_vector3_to_color(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, vector3 coord)
{
  vector3 cellPosition = floor(coord);
  vector3 localPosition = coord - cellPosition;

  vector3 vectorToClosest = vector3(0.0, 0.0, 0.0);
  float minDistance = 8.0;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vector3 cellOffset = vector3(i, j, k);
        vector3 vectorToPoint = cellOffset +
                                hash_vector3_to_vector3(cellPosition + cellOffset) *
                                    params.randomness -
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
        vector3 cellOffset = vector3(i, j, k);
        vector3 vectorToPoint = cellOffset +
                                hash_vector3_to_vector3(cellPosition + cellOffset) *
                                    params.randomness -
                                localPosition;
        vector3 perpendicularToEdge = vectorToPoint - vectorToClosest;
        if (dot(perpendicularToEdge, perpendicularToEdge) > 0.0001) {
          float distanceToEdge = dot((vectorToClosest + vectorToPoint) / 2.0,
                                     normalize((vector)perpendicularToEdge));
          minDistance = min(minDistance, distanceToEdge);
        }
      }
    }
  }

  return minDistance;
}

float voronoi_n_sphere_radius(VoronoiParams params, vector3 coord)
{
  vector3 cellPosition = floor(coord);
  vector3 localPosition = coord - cellPosition;

  vector3 closestPoint = vector3(0.0, 0.0, 0.0);
  vector3 closestPointOffset = vector3(0.0, 0.0, 0.0);
  float minDistance = 8.0;
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        vector3 cellOffset = vector3(i, j, k);
        vector3 pointPosition = cellOffset + hash_vector3_to_vector3(cellPosition + cellOffset) *
                                                 params.randomness;
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
  vector3 closestPointToClosestPoint = vector3(0.0, 0.0, 0.0);
  for (int k = -1; k <= 1; k++) {
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        if (i == 0 && j == 0 && k == 0) {
          continue;
        }
        vector3 cellOffset = vector3(i, j, k) + closestPointOffset;
        vector3 pointPosition = cellOffset + hash_vector3_to_vector3(cellPosition + cellOffset) *
                                                 params.randomness;
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

vector4 voronoi_position(vector4 coord)
{
  return coord;
}

VoronoiOutput voronoi_f1(VoronoiParams params, vector4 coord)
{
  vector4 cellPosition = floor(coord);
  vector4 localPosition = coord - cellPosition;

  float minDistance = 8.0;
  vector4 targetOffset = vector4(0.0, 0.0, 0.0, 0.0);
  vector4 targetPosition = vector4(0.0, 0.0, 0.0, 0.0);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vector4 cellOffset = vector4(i, j, k, u);
          vector4 pointPosition = cellOffset + hash_vector4_to_vector4(cellPosition + cellOffset) *
                                                   params.randomness;
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
  octave.Color = hash_vector4_to_color(cellPosition + targetOffset);
  octave.Position = voronoi_position(targetPosition + cellPosition);
  return octave;
}

VoronoiOutput voronoi_smooth_f1(VoronoiParams params, vector4 coord)
{
  vector4 cellPosition = floor(coord);
  vector4 localPosition = coord - cellPosition;

  float smoothDistance = 8.0;
  vector3 smoothColor = vector3(0.0, 0.0, 0.0);
  vector4 smoothPosition = vector4(0.0, 0.0, 0.0, 0.0);
  for (int u = -2; u <= 2; u++) {
    for (int k = -2; k <= 2; k++) {
      for (int j = -2; j <= 2; j++) {
        for (int i = -2; i <= 2; i++) {
          vector4 cellOffset = vector4(i, j, k, u);
          vector4 pointPosition = cellOffset + hash_vector4_to_vector4(cellPosition + cellOffset) *
                                                   params.randomness;
          float distanceToPoint = voronoi_distance(pointPosition, localPosition, params);
          float h = smoothstep(
              0.0, 1.0, 0.5 + 0.5 * (smoothDistance - distanceToPoint) / params.smoothness);
          float correctionFactor = params.smoothness * h * (1.0 - h);
          smoothDistance = mix(smoothDistance, distanceToPoint, h) - correctionFactor;
          correctionFactor /= 1.0 + 3.0 * params.smoothness;
          color cellColor = hash_vector4_to_color(cellPosition + cellOffset);
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

VoronoiOutput voronoi_f2(VoronoiParams params, vector4 coord)
{
  vector4 cellPosition = floor(coord);
  vector4 localPosition = coord - cellPosition;

  float distanceF1 = 8.0;
  float distanceF2 = 8.0;
  vector4 offsetF1 = vector4(0.0, 0.0, 0.0, 0.0);
  vector4 positionF1 = vector4(0.0, 0.0, 0.0, 0.0);
  vector4 offsetF2 = vector4(0.0, 0.0, 0.0, 0.0);
  vector4 positionF2 = vector4(0.0, 0.0, 0.0, 0.0);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vector4 cellOffset = vector4(i, j, k, u);
          vector4 pointPosition = cellOffset + hash_vector4_to_vector4(cellPosition + cellOffset) *
                                                   params.randomness;
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
  octave.Color = hash_vector4_to_color(cellPosition + offsetF2);
  octave.Position = voronoi_position(positionF2 + cellPosition);
  return octave;
}

float voronoi_distance_to_edge(VoronoiParams params, vector4 coord)
{
  vector4 cellPosition = floor(coord);
  vector4 localPosition = coord - cellPosition;

  vector4 vectorToClosest = vector4(0.0, 0.0, 0.0, 0.0);
  float minDistance = 8.0;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vector4 cellOffset = vector4(i, j, k, u);
          vector4 vectorToPoint = cellOffset +
                                  hash_vector4_to_vector4(cellPosition + cellOffset) *
                                      params.randomness -
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
          vector4 cellOffset = vector4(i, j, k, u);
          vector4 vectorToPoint = cellOffset +
                                  hash_vector4_to_vector4(cellPosition + cellOffset) *
                                      params.randomness -
                                  localPosition;
          vector4 perpendicularToEdge = vectorToPoint - vectorToClosest;
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

float voronoi_n_sphere_radius(VoronoiParams params, vector4 coord)
{
  vector4 cellPosition = floor(coord);
  vector4 localPosition = coord - cellPosition;

  vector4 closestPoint = vector4(0.0, 0.0, 0.0, 0.0);
  vector4 closestPointOffset = vector4(0.0, 0.0, 0.0, 0.0);
  float minDistance = 8.0;
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          vector4 cellOffset = vector4(i, j, k, u);
          vector4 pointPosition = cellOffset + hash_vector4_to_vector4(cellPosition + cellOffset) *
                                                   params.randomness;
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
  vector4 closestPointToClosestPoint = vector4(0.0, 0.0, 0.0, 0.0);
  for (int u = -1; u <= 1; u++) {
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          if (i == 0 && j == 0 && k == 0 && u == 0) {
            continue;
          }
          vector4 cellOffset = vector4(i, j, k, u) + closestPointOffset;
          vector4 pointPosition = cellOffset + hash_vector4_to_vector4(cellPosition + cellOffset) *
                                                   params.randomness;
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
