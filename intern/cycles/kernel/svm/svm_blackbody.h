/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2013, Blender Foundation.
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

CCL_NAMESPACE_BEGIN

/* Blackbody Node */

ccl_device void svm_node_blackbody(KernelGlobals *kg, ShaderData *sd, float *stack, uint temperature_offset, uint col_offset)
{
	/* Output */
	float3 color_rgb = make_float3(0.0f, 0.0f, 0.0f);

	/* Input */
	float temperature = stack_load_float(stack, temperature_offset);

	if (temperature < BB_DRAPPER) {
		/* just return very very dim red */
		color_rgb = make_float3(1.0e-6f,0.0f,0.0f);
	}
	else if (temperature <= BB_MAX_TABLE_RANGE) {
		/* This is the overall size of the table */
		const int lookuptablesize = 956;
		const float lookuptablenormalize = 1.0f/956.0f;

		/* reconstruct a proper index for the table lookup, compared to OSL we don't look up two colors
		just one (the OSL-lerp is also automatically done for us by "lookup_table_read") */
		float t = powf((temperature - BB_DRAPPER) * (1.0f / BB_TABLE_SPACING), (1.0f / BB_TABLE_XPOWER));

		int blackbody_table_offset = kernel_data.tables.blackbody_offset;

		/* Retrieve colors from the lookup table */
		float lutval = t*lookuptablenormalize;
		float R = lookup_table_read(kg, lutval, blackbody_table_offset, lookuptablesize);
		lutval = (t + 319.0f*1.0f)*lookuptablenormalize;
		float G = lookup_table_read(kg, lutval, blackbody_table_offset, lookuptablesize);
		lutval = (t + 319.0f*2.0f)*lookuptablenormalize;
		float B = lookup_table_read(kg, lutval, blackbody_table_offset, lookuptablesize);

		R = powf(R, BB_TABLE_YPOWER);
		G = powf(G, BB_TABLE_YPOWER);
		B = powf(B, BB_TABLE_YPOWER);

		color_rgb = make_float3(R, G, B);
	}

	/* Luminance */
	float l = linear_rgb_to_gray(color_rgb);
	if (l != 0.0f)
		color_rgb /= l;

	if (stack_valid(col_offset))
		stack_store_float3(stack, col_offset, color_rgb);
}

CCL_NAMESPACE_END
