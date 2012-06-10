/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Adapted from code with license:
 * 
 * Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
 * Digital Ltd. LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Industrial Light & Magic nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "util_math.h"
#include "util_transform.h"

CCL_NAMESPACE_BEGIN

/* Transform Inverse */

static bool transform_matrix4_gj_inverse(float R[][4], float M[][4])
{
	/* forward elimination */
	for(int i = 0; i < 4; i++) {
		int pivot = i;
		float pivotsize = M[i][i];

		if(pivotsize < 0)
			pivotsize = -pivotsize;

		for(int j = i + 1; j < 4; j++) {
			float tmp = M[j][i];

			if(tmp < 0)
				tmp = -tmp;

			if(tmp > pivotsize) {
				pivot = j;
				pivotsize = tmp;
			}
		}

		if(pivotsize == 0)
			return false;

		if(pivot != i) {
			for(int j = 0; j < 4; j++) {
				float tmp;

				tmp = M[i][j];
				M[i][j] = M[pivot][j];
				M[pivot][j] = tmp;

				tmp = R[i][j];
				R[i][j] = R[pivot][j];
				R[pivot][j] = tmp;
			}
		}

		for(int j = i + 1; j < 4; j++) {
			float f = M[j][i] / M[i][i];

			for(int k = 0; k < 4; k++) {
				M[j][k] -= f*M[i][k];
				R[j][k] -= f*R[i][k];
			}
		}
	}

	/* backward substitution */
	for(int i = 3; i >= 0; --i) {
		float f;

		if((f = M[i][i]) == 0)
			return false;

		for(int j = 0; j < 4; j++) {
			M[i][j] /= f;
			R[i][j] /= f;
		}

		for(int j = 0; j < i; j++) {
			f = M[j][i];

			for(int k = 0; k < 4; k++) {
				M[j][k] -= f*M[i][k];
				R[j][k] -= f*R[i][k];
			}
		}
	}

	return true;
}

Transform transform_inverse(const Transform& tfm)
{
	Transform tfmR = transform_identity();
	float M[4][4], R[4][4];

	memcpy(R, &tfmR, sizeof(R));
	memcpy(M, &tfm, sizeof(M));

	if(!transform_matrix4_gj_inverse(R, M)) {
		/* matrix is degenerate (e.g. 0 scale on some axis), ideally we should
		 * never be in this situation, but try to invert it anyway with tweak */
		M[0][0] += 1e-8f;
		M[1][1] += 1e-8f;
		M[2][2] += 1e-8f;

		if(!transform_matrix4_gj_inverse(R, M))
			return transform_identity();
	}

	memcpy(&tfmR, R, sizeof(R));

	return tfmR;
}

/* Motion Transform */

static float4 transform_to_quat(const Transform& tfm)
{
	double trace = tfm[0][0] + tfm[1][1] + tfm[2][2];
	float4 qt;

	if(trace > 0.0) {
		double s = sqrt(trace + 1.0);

		qt.w = (float)(s/2.0);
		s = 0.5/s;

		qt.x = (float)((double)(tfm[2][1] - tfm[1][2]) * s);
		qt.y = (float)((double)(tfm[0][2] - tfm[2][0]) * s);
		qt.z = (float)((double)(tfm[1][0] - tfm[0][1]) * s);
	}
	else {
		int i = 0;

		if(tfm[1][1] > tfm[i][i])
			i = 1;
		if(tfm[2][2] > tfm[i][i])
			i = 2;

		int j = (i + 1)%3;
		int k = (j + 1)%3;

		double s = sqrt((double)(tfm[i][i] - (tfm[j][j] + tfm[k][k])) + 1.0);

		double q[3];
		q[i] = s * 0.5;
		if(s != 0.0)
			s = 0.5/s;

		double w = (double)(tfm[k][j] - tfm[j][k]) * s;
		q[j] = (double)(tfm[j][i] + tfm[i][j]) * s;
		q[k] = (double)(tfm[k][i] + tfm[i][k]) * s;

		qt.x = (float)q[0];
		qt.y = (float)q[1];
		qt.z = (float)q[2];
		qt.w = (float)w;
	}

	return qt;
}

static void transform_decompose(Transform *decomp, const Transform *tfm)
{
	/* extract translation */
	decomp->y = make_float4(tfm->x.w, tfm->y.w, tfm->z.w, 0.0f);

	/* extract rotation */
	Transform M = *tfm;
	M.x.w = 0.0f; M.y.w = 0.0f; M.z.w = 0.0f; M.w.w = 1.0f;

	Transform R = M;
	float norm;
	int iteration = 0;

	do {
		Transform Rnext;
		Transform Rit = transform_inverse(transform_transpose(R));

		for(int i = 0; i < 4; i++)
			for(int j = 0; j < 4; j++)
				Rnext[i][j] = 0.5f * (R[i][j] + Rit[i][j]);
		
		norm = 0.0f;
		for(int i = 0; i < 3; i++) {
			norm = max(norm,
				fabsf(R[i][0] - Rnext[i][0]) +
				fabsf(R[i][1] - Rnext[i][1]) +
				fabsf(R[i][2] - Rnext[i][2]));
		}

		R = Rnext;
		iteration++;
	} while(iteration < 100 && norm > 1e-4f);

	if(transform_negative_scale(R))
		R = R * transform_scale(-1.0f, -1.0f, -1.0f); /* todo: test scale */

	decomp->x = transform_to_quat(R);

	/* extract scale and pack it */
	Transform scale = transform_inverse(R) * M;
	decomp->y.w = scale.x.x;
	decomp->z = make_float4(scale.x.y, scale.x.z, scale.y.x, scale.y.y);
	decomp->w = make_float4(scale.y.z, scale.z.x, scale.z.y, scale.z.z);
}

void transform_motion_decompose(MotionTransform *decomp, const MotionTransform *motion)
{
	transform_decompose(&decomp->pre, &motion->pre);
	transform_decompose(&decomp->post, &motion->post);
}

CCL_NAMESPACE_END

