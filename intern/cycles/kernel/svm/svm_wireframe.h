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

/* Wireframe Node */

ccl_device_inline float wireframe(KernelGlobals *kg,
                                  ShaderData *sd,
                                  float size,
                                  int pixel_size,
                                  float3 *P)
{
#ifdef __HAIR__
	if(sd->prim != PRIM_NONE && sd->type & PRIMITIVE_ALL_TRIANGLE)
#else
	if(sd->prim != PRIM_NONE)
#endif
	{
		float3 Co[3];
		float pixelwidth = 1.0f;

		/* Triangles */
		int np = 3;

		if(sd->type & PRIMITIVE_TRIANGLE)
			triangle_vertices(kg, sd->prim, Co);
		else
			motion_triangle_vertices(kg, sd->object, sd->prim, sd->time, Co);

		if(!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
			object_position_transform(kg, sd, &Co[0]);
			object_position_transform(kg, sd, &Co[1]);
			object_position_transform(kg, sd, &Co[2]);
		}

		if(pixel_size) {
			// Project the derivatives of P to the viewing plane defined
			// by I so we have a measure of how big is a pixel at this point
			float pixelwidth_x = len(sd->dP.dx - dot(sd->dP.dx, sd->I) * sd->I);
			float pixelwidth_y = len(sd->dP.dy - dot(sd->dP.dy, sd->I) * sd->I);
			// Take the average of both axis' length
			pixelwidth = (pixelwidth_x + pixelwidth_y) * 0.5f;
		}

		// Use half the width as the neighbor face will render the
		// other half. And take the square for fast comparison
		pixelwidth *= 0.5f * size;
		pixelwidth *= pixelwidth;
		for(int i = 0; i < np; i++) {
			int i2 = i ? i - 1 : np - 1;
			float3 dir = *P - Co[i];
			float3 edge = Co[i] - Co[i2];
			float3 crs = cross(edge, dir);
			// At this point dot(crs, crs) / dot(edge, edge) is
			// the square of area / length(edge) == square of the
			// distance to the edge.
			if(dot(crs, crs) < (dot(edge, edge) * pixelwidth))
				return 1.0f;
		}
	}
	return 0.0f;
}

ccl_device void svm_node_wireframe(KernelGlobals *kg,
                                   ShaderData *sd,
                                   float *stack,
                                   uint4 node)
{
	uint in_size = node.y;
	uint out_fac = node.z;
	uint use_pixel_size, bump_offset;
	decode_node_uchar4(node.w, &use_pixel_size, &bump_offset, NULL, NULL);

	/* Input Data */
	float size = stack_load_float(stack, in_size);
	int pixel_size = (int)use_pixel_size;

	/* Calculate wireframe */
#ifdef __SPLIT_KERNEL__
	/* TODO(sergey): This is because sd is actually a global space,
	 * which makes it difficult to re-use same wireframe() function.
	 *
	 * With OpenCL 2.0 it's possible to avoid this change, but for until
	 * then we'll be living with such an exception.
	 */
	float3 P = sd->P;
	float f = wireframe(kg, sd, size, pixel_size, &P);
#else
	float f = wireframe(kg, sd, size, pixel_size, &sd->P);
#endif

	/* TODO(sergey): Think of faster way to calculate derivatives. */
	if(bump_offset == NODE_BUMP_OFFSET_DX) {
		float3 Px = sd->P - sd->dP.dx;
		f += (f - wireframe(kg, sd, size, pixel_size, &Px)) / len(sd->dP.dx);
	}
	else if(bump_offset == NODE_BUMP_OFFSET_DY) {
		float3 Py = sd->P - sd->dP.dy;
		f += (f - wireframe(kg, sd, size, pixel_size, &Py)) / len(sd->dP.dy);
	}

	if(stack_valid(out_fac))
		stack_store_float(stack, out_fac, f);
}

CCL_NAMESPACE_END

