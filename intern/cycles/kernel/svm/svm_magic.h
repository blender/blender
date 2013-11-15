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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* Magic */

ccl_device_noinline float3 svm_magic(float3 p, int n, float distortion)
{
	float x = sinf((p.x + p.y + p.z)*5.0f);
	float y = cosf((-p.x + p.y - p.z)*5.0f);
	float z = -cosf((-p.x - p.y + p.z)*5.0f);

	if(n > 0) {
		x *= distortion;
		y *= distortion;
		z *= distortion;
		y = -cosf(x-y+z);
		y *= distortion;

		if(n > 1) {
			x = cosf(x-y-z);
			x *= distortion;

			if(n > 2) {
				z = sinf(-x-y-z);
				z *= distortion;

				if(n > 3) {
					x = -cosf(-x+y-z);
					x *= distortion;

					if(n > 4) {
						y = -sinf(-x+y+z);
						y *= distortion;

						if(n > 5) {
							y = -cosf(-x+y+z);
							y *= distortion;

							if(n > 6) {
								x = cosf(x+y+z);
								x *= distortion;

								if(n > 7) {
									z = sinf(x+y-z);
									z *= distortion;

									if(n > 8) {
										x = -cosf(-x-y+z);
										x *= distortion;

										if(n > 9) {
											y = -sinf(x-y+z);
											y *= distortion;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(distortion != 0.0f) {
		distortion *= 2.0f;
		x /= distortion;
		y /= distortion;
		z /= distortion;
	}

	return make_float3(0.5f - x, 0.5f - y, 0.5f - z);
}

ccl_device void svm_node_tex_magic(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint depth;
	uint scale_offset, distortion_offset, co_offset, fac_offset, color_offset;

	decode_node_uchar4(node.y, &depth, &color_offset, &fac_offset, NULL);
	decode_node_uchar4(node.z, &co_offset, &scale_offset, &distortion_offset, NULL);

	uint4 node2 = read_node(kg, offset);
	float3 co = stack_load_float3(stack, co_offset);
	float scale = stack_load_float_default(stack, scale_offset, node2.x);
	float distortion = stack_load_float_default(stack, distortion_offset, node2.y);

	float3 color = svm_magic(co*scale, depth, distortion);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, average(color));
	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

