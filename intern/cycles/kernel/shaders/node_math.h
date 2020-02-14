/*
 * Copyright 2011-2020 Blender Foundation
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

float safe_divide(float a, float b)
{
  return (b != 0.0) ? a / b : 0.0;
}

vector safe_divide(vector a, vector b)
{
  return vector((b[0] != 0.0) ? a[0] / b[0] : 0.0,
                (b[1] != 0.0) ? a[1] / b[1] : 0.0,
                (b[2] != 0.0) ? a[2] / b[2] : 0.0);
}

float safe_modulo(float a, float b)
{
  return (b != 0.0) ? fmod(a, b) : 0.0;
}

float fract(float a)
{
  return a - floor(a);
}

/* See: https://www.iquilezles.org/www/articles/smin/smin.htm. */
float smoothmin(float a, float b, float c)
{
  if (c != 0.0) {
    float h = max(c - abs(a - b), 0.0) / c;
    return min(a, b) - h * h * h * c * (1.0 / 6.0);
  }
  else {
    return min(a, b);
  }
}

float pingpong(float a, float b)
{
  return (b != 0.0) ? abs(fract((a - b) / (b * 2.0)) * b * 2.0 - b) : 0.0;
}

float safe_sqrt(float a)
{
  return (a > 0.0) ? sqrt(a) : 0.0;
}

float safe_log(float a, float b)
{
  return (a > 0.0 && b > 0.0) ? log(a) / log(b) : 0.0;
}

float safe_divide(float a, float b)
{
  return (b != 0.0) ? a / b : 0.0;
}

vector project(vector v, vector v_proj)
{
  float lenSquared = dot(v_proj, v_proj);
  return (lenSquared != 0.0) ? (dot(v, v_proj) / lenSquared) * v_proj : vector(0.0);
}

vector snap(vector a, vector b)
{
  return floor(safe_divide(a, b)) * b;
}

/* Adapted from godotengine math_funcs.h. */
float wrap(float value, float max, float min)
{
  float range = max - min;
  return (range != 0.0) ? value - (range * floor((value - min) / range)) : min;
}

point wrap(point value, point max, point min)
{
  return point(wrap(value[0], max[0], min[0]),
               wrap(value[1], max[1], min[1]),
               wrap(value[2], max[2], min[2]));
}
