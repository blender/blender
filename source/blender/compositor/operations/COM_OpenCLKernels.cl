/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* This file contains all opencl kernels for node-operation implementations. */

/* Global SAMPLERS. */
const sampler_t SAMPLER_NEAREST       = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
const sampler_t SAMPLER_NEAREST_CLAMP = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

__constant const int2 zero = {0,0};

// KERNEL --- BOKEH BLUR ---
__kernel void bokeh_blur_kernel(__read_only image2d_t bounding_box, __read_only image2d_t input_image, 
                              __read_only image2d_t bokeh_image, __write_only image2d_t output, 
                              int2 offset_input, int2 offset_output, int radius, int step, int2 dimension, int2 offset) 
{
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	float temp_bounding_box;
	float4 color = {0.0f,0.0f,0.0f,0.0f};
	float4 multiplyer = {0.0f,0.0f,0.0f,0.0f};
	float4 bokeh;
	const float radius2 = radius*2.0f;
	const int2 real_coordinate = coords + offset_output;
	int2 image_coordinates = real_coordinate - offset_input;

	temp_bounding_box = read_imagef(bounding_box, SAMPLER_NEAREST, coords).s0;

	if (temp_bounding_box > 0.0f && radius > 0 ) {
		const int2 bokeh_image_dim = get_image_dim(bokeh_image);
		const int2 bokeh_image_center = bokeh_image_dim/2;
		const int2 minXY = max(real_coordinate - radius, zero);
		const int2 maxXY = min(real_coordinate + radius, dimension);
		int nx, ny;
		
		float2 uv;
		int2 input_xy;
		
		if (radius < 2) {
			color = read_imagef(input_image, SAMPLER_NEAREST, image_coordinates);
			multiplyer = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
		}
		
		for (ny = minXY.y, input_xy.y = ny - offset_input.y ; ny < maxXY.y ; ny += step, input_xy.y += step) {
			uv.y = ((real_coordinate.y-ny)/radius2)*bokeh_image_dim.y+bokeh_image_center.y;
			
			for (nx = minXY.x, input_xy.x = nx - offset_input.x; nx < maxXY.x ; nx += step, input_xy.x += step) {
				uv.x = ((real_coordinate.x-nx)/radius2)*bokeh_image_dim.x+bokeh_image_center.x;
				bokeh = read_imagef(bokeh_image, SAMPLER_NEAREST, uv);
				color += bokeh * read_imagef(input_image, SAMPLER_NEAREST, input_xy);
				multiplyer += bokeh;
			}
		}
		color /= multiplyer;
	}
	else {
		color = read_imagef(input_image, SAMPLER_NEAREST, image_coordinates);
	}
	
	write_imagef(output, coords, color);
}

//KERNEL --- DEFOCUS /VARIABLESIZEBOKEHBLUR ---
__kernel void defocus_kernel(__read_only image2d_t input_image, __read_only image2d_t bokeh_image,
                            __read_only image2d_t input_size,
                            __write_only image2d_t output, int2 offset_input, int2 offset_output,
                            int step, int max_blur_scalar, float threshold, float scalar, int2 dimension, int2 offset)
{
	float4 color = {1.0f, 0.0f, 0.0f, 1.0f};
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 real_coordinate = coords + offset_output;

	float4 read_color;
	float4 temp_color;
	float4 bokeh;
	float size;
	float4 multiplier_accum = {1.0f, 1.0f, 1.0f, 1.0f};
	float4 color_accum;
	
	int minx = max(real_coordinate.s0 - max_blur_scalar, 0);
	int miny = max(real_coordinate.s1 - max_blur_scalar, 0);
	int maxx = min(real_coordinate.s0 + max_blur_scalar, dimension.s0);
	int maxy = min(real_coordinate.s1 + max_blur_scalar, dimension.s1);
	
	{
		int2 input_coordinate = real_coordinate - offset_input;
		float size_center = read_imagef(input_size, SAMPLER_NEAREST, input_coordinate).s0 * scalar;
		color_accum = read_imagef(input_image, SAMPLER_NEAREST, input_coordinate);
		read_color = color_accum;

		if (size_center > threshold) {
			for (int ny = miny; ny < maxy; ny += step) {
				input_coordinate.s1 = ny - offset_input.s1;
				float dy = ny - real_coordinate.s1;
				for (int nx = minx; nx < maxx; nx += step) {
					float dx = nx - real_coordinate.s0;
					if (dx != 0 || dy != 0) {
						input_coordinate.s0 = nx - offset_input.s0;
						size = min(read_imagef(input_size, SAMPLER_NEAREST, input_coordinate).s0 * scalar, size_center);
						if (size > threshold) {
							if (size >= fabs(dx) && size >= fabs(dy)) {
								float2 uv = {256.0f + dx * 255.0f / size,
								             256.0f + dy * 255.0f / size};
								bokeh = read_imagef(bokeh_image, SAMPLER_NEAREST, uv);
								temp_color = read_imagef(input_image, SAMPLER_NEAREST, input_coordinate);
								color_accum += bokeh * temp_color;
								multiplier_accum += bokeh;
							}
						}
					}
				}
			}
		}

		color = color_accum * (1.0f / multiplier_accum);
		
		/* blend in out values over the threshold, otherwise we get sharp, ugly transitions */
		if ((size_center > threshold) &&
		    (size_center < threshold * 2.0f))
		{
			/* factor from 0-1 */
			float fac = (size_center - threshold) / threshold;
			color = (read_color * (1.0f - fac)) +  (color * fac);
		}
		
		write_imagef(output, coords, color);
	}
}


// KERNEL --- DILATE ---
__kernel void dilate_kernel(__read_only image2d_t input_image,  __write_only image2d_t output,
                           int2 offset_input, int2 offset_output, int scope, int distance_squared, int2 dimension, 
                           int2 offset)
{
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 real_coordinate = coords + offset_output;

	const int2 minXY = max(real_coordinate - scope, zero);
	const int2 maxXY = min(real_coordinate + scope, dimension);
	
	float value = 0.0f;
	int nx, ny;
	int2 input_xy;
	
	for (ny = minXY.y, input_xy.y = ny - offset_input.y ; ny < maxXY.y ; ny ++, input_xy.y++) {
		const float deltaY = (real_coordinate.y - ny);
		for (nx = minXY.x, input_xy.x = nx - offset_input.x; nx < maxXY.x ; nx ++, input_xy.x++) {
			const float deltaX = (real_coordinate.x - nx);
			const float measured_distance = deltaX * deltaX + deltaY * deltaY;
			if (measured_distance <= distance_squared) {
				value = max(value, read_imagef(input_image, SAMPLER_NEAREST, input_xy).s0);
			}
		}
	}

	float4 color = {value,0.0f,0.0f,0.0f};
	write_imagef(output, coords, color);
}

// KERNEL --- DILATE ---
__kernel void erode_kernel(__read_only image2d_t input_image,  __write_only image2d_t output,
                           int2 offset_input, int2 offset_output, int scope, int distance_squared, int2 dimension, 
                           int2 offset)
{
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 real_coordinate = coords + offset_output;

	const int2 minXY = max(real_coordinate - scope, zero);
	const int2 maxXY = min(real_coordinate + scope, dimension);
	
	float value = 1.0f;
	int nx, ny;
	int2 input_xy;
	
	for (ny = minXY.y, input_xy.y = ny - offset_input.y ; ny < maxXY.y ; ny ++, input_xy.y++) {
		for (nx = minXY.x, input_xy.x = nx - offset_input.x; nx < maxXY.x ; nx ++, input_xy.x++) {
			const float deltaX = (real_coordinate.x - nx);
			const float deltaY = (real_coordinate.y - ny);
			const float measured_distance = deltaX * deltaX+deltaY * deltaY;
			if (measured_distance <= distance_squared) {
				value = min(value, read_imagef(input_image, SAMPLER_NEAREST, input_xy).s0);
			}
		}
	}

	float4 color = {value,0.0f,0.0f,0.0f};
	write_imagef(output, coords, color);
}

// KERNEL --- DIRECTIONAL BLUR ---
__kernel void directional_blur_kernel(__read_only image2d_t input_image,  __write_only image2d_t output,
                                    int2 offset_output, int iterations, float scale, float rotation, float2 translate,
                                     float2 center, int2 offset)
{
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 real_coordinate = coords + offset_output;

	float4 col;
	float2 ltxy = translate;
	float lsc = scale;
	float lrot = rotation;
	
	col = read_imagef(input_image, SAMPLER_NEAREST, real_coordinate);

	/* blur the image */
	for (int i = 0; i < iterations; ++i) {
		const float cs = cos(lrot), ss = sin(lrot);
		const float isc = 1.0f / (1.0f + lsc);

		const float v = isc * (real_coordinate.s1 - center.s1) + ltxy.s1;
		const float u = isc * (real_coordinate.s0 - center.s0) + ltxy.s0;
		float2 uv = {
			cs * u + ss * v + center.s0,
			cs * v - ss * u + center.s1
		};

		col += read_imagef(input_image, SAMPLER_NEAREST_CLAMP, uv);

		/* double transformations */
		ltxy += translate;
		lrot += rotation;
		lsc += scale;
	}

	col *= (1.0f/(iterations+1));

	write_imagef(output, coords, col);
}

// KERNEL --- GAUSSIAN BLUR ---
__kernel void gaussian_xblur_operation_kernel(__read_only image2d_t input_image,
                                           int2 offset_input,
                                           __write_only image2d_t output,
                                           int2 offset_output,
                                           int filter_size,
                                           int2 dimension,
                                           __global float *gausstab,
                                           int2 offset)
{
	float4 color = {0.0f, 0.0f, 0.0f, 0.0f};
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 real_coordinate = coords + offset_output;
	int2 input_coordinate = real_coordinate - offset_input;
	float weight = 0.0f;

	int xmin = max(real_coordinate.x - filter_size,     0) - offset_input.x;
	int xmax = min(real_coordinate.x + filter_size + 1, dimension.x) - offset_input.x;

	for (int nx = xmin, i = max(filter_size - real_coordinate.x, 0); nx < xmax; ++nx, ++i) {
		float w = gausstab[i];
		input_coordinate.x = nx;
		color += read_imagef(input_image, SAMPLER_NEAREST, input_coordinate) * w;
		weight += w;
	}

	color *= (1.0f / weight);

	write_imagef(output, coords, color);
}

__kernel void gaussian_yblur_operation_kernel(__read_only image2d_t input_image,
                                           int2 offset_input,
                                           __write_only image2d_t output,
                                           int2 offset_output,
                                           int filter_size,
                                           int2 dimension,
                                           __global float *gausstab,
                                           int2 offset)
{
	float4 color = {0.0f, 0.0f, 0.0f, 0.0f};
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 real_coordinate = coords + offset_output;
	int2 input_coordinate = real_coordinate - offset_input;
	float weight = 0.0f;

	int ymin = max(real_coordinate.y - filter_size,     0) - offset_input.y;
	int ymax = min(real_coordinate.y + filter_size + 1, dimension.y) - offset_input.y;

	for (int ny = ymin, i = max(filter_size - real_coordinate.y, 0); ny < ymax; ++ny, ++i) {
		float w = gausstab[i];
		input_coordinate.y = ny;
		color += read_imagef(input_image, SAMPLER_NEAREST, input_coordinate) * w;
		weight += w;
	}

	color *= (1.0f / weight);

	write_imagef(output, coords, color);
}
