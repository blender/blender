/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Prototype of functions to implement to load attributes data.
 * Implementation changes based on object data type. */

#ifndef GPU_METAL /* MSL does not require prototypes. */
vec3 attr_load_orco(vec4 orco);
vec4 attr_load_tangent(vec4 tangent);
vec4 attr_load_vec4(vec4 attr);
vec3 attr_load_vec3(vec3 attr);
vec2 attr_load_vec2(vec2 attr);
float attr_load_float(float attr);

vec3 attr_load_orco(samplerBuffer orco);
vec4 attr_load_tangent(samplerBuffer tangent);
vec4 attr_load_vec4(samplerBuffer attr);
vec3 attr_load_vec3(samplerBuffer attr);
vec2 attr_load_vec2(samplerBuffer attr);
float attr_load_float(samplerBuffer attr);

vec3 attr_load_orco(sampler3D orco);
vec4 attr_load_tangent(sampler3D tangent);
vec4 attr_load_vec4(sampler3D tex);
vec3 attr_load_vec3(sampler3D tex);
vec2 attr_load_vec2(sampler3D tex);
float attr_load_float(sampler3D tex);

float attr_load_temperature_post(float attr);
vec4 attr_load_color_post(vec4 attr);
vec4 attr_load_uniform(vec4 attr, const uint attr_hash);
#endif
