/*
* Copyright 2011-2015 Blender Foundation
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

#ifndef SD_VAR
#define SD_VAR(type, what)
#endif
#ifndef SD_CLOSURE_VAR
#define SD_CLOSURE_VAR(type, what, max_closure)
#endif

/* position */
SD_VAR(float3, P)
/* smooth normal for shading */
SD_VAR(float3, N)
/* true geometric normal */
SD_VAR(float3, Ng)
/* view/incoming direction */
SD_VAR(float3, I)
/* shader id */
SD_VAR(int, shader)
/* booleans describing shader, see ShaderDataFlag */
SD_VAR(int, flag)

/* primitive id if there is one, ~0 otherwise */
SD_VAR(int, prim)

/* combined type and curve segment for hair */
SD_VAR(int, type)

/* parametric coordinates
* - barycentric weights for triangles */
SD_VAR(float, u)
SD_VAR(float, v)
/* object id if there is one, ~0 otherwise */
SD_VAR(int, object)

/* motion blur sample time */
SD_VAR(float, time)

/* length of the ray being shaded */
SD_VAR(float, ray_length)

#ifdef __RAY_DIFFERENTIALS__
/* differential of P. these are orthogonal to Ng, not N */
SD_VAR(differential3, dP)
/* differential of I */
SD_VAR(differential3, dI)
/* differential of u, v */
SD_VAR(differential, du)
SD_VAR(differential, dv)
#endif
#ifdef __DPDU__
/* differential of P w.r.t. parametric coordinates. note that dPdu is
* not readily suitable as a tangent for shading on triangles. */
SD_VAR(float3, dPdu)
SD_VAR(float3, dPdv)
#endif

#ifdef __OBJECT_MOTION__
/* object <-> world space transformations, cached to avoid
* re-interpolating them constantly for shading */
SD_VAR(Transform, ob_tfm)
SD_VAR(Transform, ob_itfm)
#endif

/* Closure data, we store a fixed array of closures */
SD_CLOSURE_VAR(ShaderClosure, closure, MAX_CLOSURE)
SD_VAR(int, num_closure)
SD_VAR(float, randb_closure)

/* ray start position, only set for backgrounds */
SD_VAR(float3, ray_P)
SD_VAR(differential3, ray_dP)

#ifdef __OSL__
SD_VAR(struct KernelGlobals *, osl_globals)
SD_VAR(struct PathState *, osl_path_state)
#endif

#undef SD_VAR
#undef SD_CLOSURE_VAR
