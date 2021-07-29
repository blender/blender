/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#include <OSL/genclosure.h>

#include "kernel/kernel_compat_cpu.h"
#include "kernel/osl/osl_closures.h"

#include "kernel/kernel_types.h"
#include "kernel/kernel_montecarlo.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;

class CBSSRDFClosure : public CClosurePrimitive {
public:
	Bssrdf params;
	float3 radius;
	float3 albedo;

	void alloc(ShaderData *sd, int path_flag, float3 weight, ClosureType type)
	{
		float sample_weight = fabsf(average(weight));

		/* disable in case of diffuse ancestor, can't see it well then and
		 * adds considerably noise due to probabilities of continuing path
		 * getting lower and lower */
		if(path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
			radius = make_float3(0.0f, 0.0f, 0.0f);
		}

		if(sample_weight > CLOSURE_WEIGHT_CUTOFF) {
			/* sharpness */
			float sharpness = params.sharpness;
			/* texture color blur */
			float texture_blur = params.texture_blur;

			/* create one closure per color channel */
			Bssrdf *bssrdf = bssrdf_alloc(sd, make_float3(weight.x, 0.0f, 0.0f));
			if(bssrdf) {
				bssrdf->sample_weight = sample_weight;
				bssrdf->radius = radius.x;
				bssrdf->texture_blur = texture_blur;
				bssrdf->albedo = albedo.x;
				bssrdf->sharpness = sharpness;
				bssrdf->N = params.N;
				bssrdf->roughness = params.roughness;
				sd->flag |= bssrdf_setup(bssrdf, (ClosureType)type);
			}

			bssrdf = bssrdf_alloc(sd, make_float3(0.0f, weight.y, 0.0f));
			if(bssrdf) {
				bssrdf->sample_weight = sample_weight;
				bssrdf->radius = radius.y;
				bssrdf->texture_blur = texture_blur;
				bssrdf->albedo = albedo.y;
				bssrdf->sharpness = sharpness;
				bssrdf->N = params.N;
				bssrdf->roughness = params.roughness;
				sd->flag |= bssrdf_setup(bssrdf, (ClosureType)type);
			}

			bssrdf = bssrdf_alloc(sd, make_float3(0.0f, 0.0f, weight.z));
			if(bssrdf) {
				bssrdf->sample_weight = sample_weight;
				bssrdf->radius = radius.z;
				bssrdf->texture_blur = texture_blur;
				bssrdf->albedo = albedo.z;
				bssrdf->sharpness = sharpness;
				bssrdf->N = params.N;
				bssrdf->roughness = params.roughness;
				sd->flag |= bssrdf_setup(bssrdf, (ClosureType)type);
			}
		}
	}
};

/* Cubic */

class CubicBSSRDFClosure : public CBSSRDFClosure {
public:
	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		alloc(sd, path_flag, weight, CLOSURE_BSSRDF_CUBIC_ID);
	}
};

ClosureParam *closure_bssrdf_cubic_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(CubicBSSRDFClosure, params.N),
		CLOSURE_FLOAT3_PARAM(CubicBSSRDFClosure, radius),
		CLOSURE_FLOAT_PARAM(CubicBSSRDFClosure, params.texture_blur),
		CLOSURE_FLOAT_PARAM(CubicBSSRDFClosure, params.sharpness),
		CLOSURE_STRING_KEYPARAM(CubicBSSRDFClosure, label, "label"),
		CLOSURE_FINISH_PARAM(CubicBSSRDFClosure)
	};
	return params;
}

CCLOSURE_PREPARE(closure_bssrdf_cubic_prepare, CubicBSSRDFClosure)

/* Gaussian */

class GaussianBSSRDFClosure : public CBSSRDFClosure {
public:
	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		alloc(sd, path_flag, weight, CLOSURE_BSSRDF_GAUSSIAN_ID);
	}
};

ClosureParam *closure_bssrdf_gaussian_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(GaussianBSSRDFClosure, params.N),
		CLOSURE_FLOAT3_PARAM(GaussianBSSRDFClosure, radius),
		CLOSURE_FLOAT_PARAM(GaussianBSSRDFClosure, params.texture_blur),
		CLOSURE_STRING_KEYPARAM(GaussianBSSRDFClosure, label, "label"),
		CLOSURE_FINISH_PARAM(GaussianBSSRDFClosure)
	};
	return params;
}

CCLOSURE_PREPARE(closure_bssrdf_gaussian_prepare, GaussianBSSRDFClosure)

/* Burley */

class BurleyBSSRDFClosure : public CBSSRDFClosure {
public:
	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		alloc(sd, path_flag, weight, CLOSURE_BSSRDF_BURLEY_ID);
	}
};

ClosureParam *closure_bssrdf_burley_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(BurleyBSSRDFClosure, params.N),
		CLOSURE_FLOAT3_PARAM(BurleyBSSRDFClosure, radius),
		CLOSURE_FLOAT_PARAM(BurleyBSSRDFClosure, params.texture_blur),
		CLOSURE_FLOAT3_PARAM(BurleyBSSRDFClosure, albedo),
		CLOSURE_STRING_KEYPARAM(BurleyBSSRDFClosure, label, "label"),
		CLOSURE_FINISH_PARAM(BurleyBSSRDFClosure)
	};
	return params;
}

CCLOSURE_PREPARE(closure_bssrdf_burley_prepare, BurleyBSSRDFClosure)

/* Disney principled */

class PrincipledBSSRDFClosure : public CBSSRDFClosure {
public:
	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		alloc(sd, path_flag, weight, CLOSURE_BSSRDF_PRINCIPLED_ID);
	}
};

ClosureParam *closure_bssrdf_principled_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(PrincipledBSSRDFClosure, params.N),
		CLOSURE_FLOAT3_PARAM(PrincipledBSSRDFClosure, radius),
		CLOSURE_FLOAT_PARAM(PrincipledBSSRDFClosure, params.texture_blur),
		CLOSURE_FLOAT3_PARAM(PrincipledBSSRDFClosure, albedo),
		CLOSURE_FLOAT_PARAM(PrincipledBSSRDFClosure, params.roughness),
		CLOSURE_STRING_KEYPARAM(PrincipledBSSRDFClosure, label, "label"),
		CLOSURE_FINISH_PARAM(PrincipledBSSRDFClosure)
	};
	return params;
}

CCLOSURE_PREPARE(closure_bssrdf_principled_prepare, PrincipledBSSRDFClosure)

CCL_NAMESPACE_END

