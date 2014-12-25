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

/* Parts adapted from code in the public domain in NVidia Mesh Tools. */

#include "mesh.h"

#include "subd_patch.h"

#include "util_math.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* De Casteljau Evaluation */

static void decasteljau_cubic(float3 *P, float3 *dt, float t, const float3 cp[4])
{
	float3 d0 = cp[0] + t*(cp[1] - cp[0]);
	float3 d1 = cp[1] + t*(cp[2] - cp[1]);
	float3 d2 = cp[2] + t*(cp[3] - cp[2]);

	d0 += t*(d1 - d0);
	d1 += t*(d2 - d1);

	*P = d0 + t*(d1 - d0);
	if(dt) *dt = d1 - d0;
}

static void decasteljau_bicubic(float3 *P, float3 *du, float3 *dv, const float3 cp[16], float u, float v)
{
	float3 ucp[4], utn[4];

	/* interpolate over u */
	decasteljau_cubic(ucp+0, utn+0, u, cp);
	decasteljau_cubic(ucp+1, utn+1, u, cp+4);
	decasteljau_cubic(ucp+2, utn+2, u, cp+8);
	decasteljau_cubic(ucp+3, utn+3, u, cp+12);

	/* interpolate over v */
	decasteljau_cubic(P, dv, v, ucp);
	if(du) decasteljau_cubic(du, NULL, v, utn);
}

/* Linear Quad Patch */

void LinearQuadPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	float3 d0 = interp(hull[0], hull[1], u);
	float3 d1 = interp(hull[2], hull[3], u);

	*P = interp(d0, d1, v);

	if(dPdu && dPdv) {
		*dPdu = interp(hull[1] - hull[0], hull[3] - hull[2], v);
		*dPdv = interp(hull[2] - hull[0], hull[3] - hull[1], u);
	}
}

BoundBox LinearQuadPatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 4; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

/* Linear Triangle Patch */

void LinearTrianglePatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	*P = u*hull[0] + v*hull[1] + (1.0f - u - v)*hull[2];

	if(dPdu && dPdv) {
		*dPdu = hull[0] - hull[2];
		*dPdv = hull[1] - hull[2];
	}
}

BoundBox LinearTrianglePatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 3; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

/* Bicubic Patch */

void BicubicPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v)
{
	decasteljau_bicubic(P, dPdu, dPdv, hull, u, v);
}

BoundBox BicubicPatch::bound()
{
	BoundBox bbox = BoundBox::empty;

	for(int i = 0; i < 16; i++)
		bbox.grow(hull[i]);
	
	return bbox;
}

CCL_NAMESPACE_END

