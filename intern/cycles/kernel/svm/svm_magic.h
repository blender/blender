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

CCL_NAMESPACE_BEGIN

/* Magic */

__device_noinline float3 svm_magic(float3 p, int n, float turbulence)
{
	float turb = turbulence/5.0f;

	float x = sinf((p.x + p.y + p.z)*5.0f);
	float y = cosf((-p.x + p.y - p.z)*5.0f);
	float z = -cosf((-p.x - p.y + p.z)*5.0f);

	if(n > 0) {
		x *= turb;
		y *= turb;
		z *= turb;
		y = -cosf(x-y+z);
		y *= turb;

		if(n > 1) {
			x= cosf(x-y-z);
			x *= turb;

			if(n > 2) {
				z= sinf(-x-y-z);
				z *= turb;

				if(n > 3) {
					x= -cosf(-x+y-z);
					x *= turb;

					if(n > 4) {
						y= -sinf(-x+y+z);
						y *= turb;

						if(n > 5) {
							y= -cosf(-x+y+z);
							y *= turb;

							if(n > 6) {
								x= cosf(x+y+z);
								x *= turb;

								if(n > 7) {
									z= sinf(x+y-z);
									z *= turb;

									if(n > 8) {
										x= -cosf(-x-y+z);
										x *= turb;

										if(n > 9) {
											y= -sinf(x-y+z);
											y *= turb;
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

	if(turb != 0.0f) {
		turb *= 2.0f;
		x /= turb;
		y /= turb;
		z /= turb;
	}

	return make_float3(0.5f - x, 0.5f - y, 0.5f - z);
}

__device void svm_node_tex_magic(ShaderData *sd, float *stack, uint4 node)
{
	uint depth, turbulence_offset, co_offset, color_offset;

	decode_node_uchar4(node.y, &depth, &turbulence_offset, &co_offset, &color_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float turbulence = stack_load_float_default(stack, turbulence_offset, node.z);

	float3 color = svm_magic(co, depth, turbulence);
	stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

