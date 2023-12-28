/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Fast store variant macro. In GLSL this is the same as imageStore, but assumes no bounds
 * checking. */
#define imageStoreFast imageStore
#define imageLoadFast imageLoad

/* Texture format tokens -- Type explicitness required by other Graphics APIs. */
#define depth2D sampler2D
#define depth2DArray sampler2DArray
#define depth2DMS sampler2DMS
#define depth2DMSArray sampler2DMSArray
#define depthCube samplerCube
#define depthCubeArray samplerCubeArray
#define depth2DArrayShadow sampler2DArrayShadow

#define usampler2DArrayAtomic usampler2DArray
#define usampler2DAtomic usampler2D
#define usampler3DAtomic usampler3D
#define isampler2DArrayAtomic isampler2DArray
#define isampler2DAtomic isampler2D
#define isampler3DAtomic isampler3D

/* Backend Functions. */
#define select(A, B, mask) mix(A, B, mask)

bool is_zero(vec2 A)
{
  return all(equal(A, vec2(0.0)));
}

bool is_zero(vec3 A)
{
  return all(equal(A, vec3(0.0)));
}

bool is_zero(vec4 A)
{
  return all(equal(A, vec4(0.0)));
}
