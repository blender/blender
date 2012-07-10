/// This file contains all opencl kernels for node-operation implementations 

// Global SAMPLERS
const sampler_t SAMPLER_NEAREST      = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__constant const int2 zero = {0,0};

// KERNEL --- BOKEH BLUR ---
__kernel void bokehBlurKernel(__read_only image2d_t boundingBox, __read_only image2d_t inputImage, 
                              __read_only image2d_t bokehImage, __write_only image2d_t output, 
                              int2 offsetInput, int2 offsetOutput, int radius, int step, int2 dimension, int2 offset) 
{
	int2 coords = {get_global_id(0), get_global_id(1)}; 
	coords += offset;
	float tempBoundingBox;
	float4 color = {0.0f,0.0f,0.0f,0.0f};
	float4 multiplyer = {0.0f,0.0f,0.0f,0.0f};
	float4 bokeh;
	const float radius2 = radius*2.0f;
	const int2 realCoordinate = coords + offsetOutput;

	tempBoundingBox = read_imagef(boundingBox, SAMPLER_NEAREST, coords).s0;

	if (tempBoundingBox > 0.0f) {
		const int2 bokehImageDim = get_image_dim(bokehImage);
		const int2 bokehImageCenter = bokehImageDim/2;
		const int2 minXY = max(realCoordinate - radius, zero);
		const int2 maxXY = min(realCoordinate + radius, dimension);
		int nx, ny;
		
		float2 uv;
		int2 inputXy;
		
		for (ny = minXY.y, inputXy.y = ny - offsetInput.y ; ny < maxXY.y ; ny +=step, inputXy.y+=step) {
			uv.y = ((realCoordinate.y-ny)/radius2)*bokehImageDim.y+bokehImageCenter.y;
			
			for (nx = minXY.x, inputXy.x = nx - offsetInput.x; nx < maxXY.x ; nx +=step, inputXy.x+=step) {
				uv.x = ((realCoordinate.x-nx)/radius2)*bokehImageDim.x+bokehImageCenter.x;
				bokeh = read_imagef(bokehImage, SAMPLER_NEAREST, uv);
				color += bokeh * read_imagef(inputImage, SAMPLER_NEAREST, inputXy);
				multiplyer += bokeh;
			}
		}
		color /= multiplyer;
		
	} else {
		int2 imageCoordinates = realCoordinate - offsetInput;
		color = read_imagef(inputImage, SAMPLER_NEAREST, imageCoordinates);
	}
	
	write_imagef(output, coords, color);
}

//KERNEL --- DEFOCUS /VARIABLESIZEBOKEHBLUR ---
__kernel void defocusKernel(__read_only image2d_t inputImage, __read_only image2d_t bokehImage, 
					__read_only image2d_t inputSize,
					__write_only image2d_t output, int2 offsetInput, int2 offsetOutput, 
					int step, int maxBlur, float threshold, int2 dimension, int2 offset) 
{
	float4 color = {1.0f, 0.0f, 0.0f, 1.0f};
	int2 coords = {get_global_id(0), get_global_id(1)};
	coords += offset;
	const int2 realCoordinate = coords + offsetOutput;

	float4 readColor;
	float4 bokeh;
	float tempSize;
	float4 multiplier_accum = {1.0f, 1.0f, 1.0f, 1.0f};
	float4 color_accum;
	
	int minx = max(realCoordinate.s0 - maxBlur, 0);
	int miny = max(realCoordinate.s1 - maxBlur, 0);
	int maxx = min(realCoordinate.s0 + maxBlur, dimension.s0);
	int maxy = min(realCoordinate.s1 + maxBlur, dimension.s1);
	
	{
		int2 inputCoordinate = realCoordinate - offsetInput;
		float size = read_imagef(inputSize, SAMPLER_NEAREST, inputCoordinate).s0;
		color_accum = read_imagef(inputImage, SAMPLER_NEAREST, inputCoordinate);

		for (int ny = miny; ny < maxy; ny += step) {
			for (int nx = minx; nx < maxx; nx += step) {
				if (nx >= 0 && nx < dimension.s0 && ny >= 0 && ny < dimension.s1) {
					inputCoordinate.s0 = nx - offsetInput.s0;
					inputCoordinate.s1 = ny - offsetInput.s1;
					tempSize = read_imagef(inputSize, SAMPLER_NEAREST, inputCoordinate).s0;
					if (size > threshold && tempSize > threshold) {
						float dx = nx - realCoordinate.s0;
						float dy = ny - realCoordinate.s1;
						if (dx != 0 || dy != 0) {
							if (tempSize >= fabs(dx) && tempSize >= fabs(dy)) {
								float2 uv = { 256.0f + dx * 256.0f / tempSize, 256.0f + dy * 256.0f / tempSize};
								bokeh = read_imagef(bokehImage, SAMPLER_NEAREST, uv);
								readColor = read_imagef(inputImage, SAMPLER_NEAREST, inputCoordinate);
								color_accum += bokeh*readColor;
								multiplier_accum += bokeh;
							}
						}
					}
				}
			}
		} 
	}

	color = color_accum * (1.0f / multiplier_accum);
	write_imagef(output, coords, color);
}


// KERNEL --- DILATE ---
__kernel void dilateKernel(__read_only image2d_t inputImage,  __write_only image2d_t output,
                           int2 offsetInput, int2 offsetOutput, int scope, int distanceSquared, int2 dimension, 
                           int2 offset)
{
	int2 coords = {get_global_id(0), get_global_id(1)}; 
	coords += offset;
	const int2 realCoordinate = coords + offsetOutput;

	const int2 minXY = max(realCoordinate - scope, zero);
	const int2 maxXY = min(realCoordinate + scope, dimension);
	
	float value = 0.0f;
	int nx, ny;
	int2 inputXy;
	
	for (ny = minXY.y, inputXy.y = ny - offsetInput.y ; ny < maxXY.y ; ny ++, inputXy.y++) {
		const float deltaY = (realCoordinate.y - ny);
		for (nx = minXY.x, inputXy.x = nx - offsetInput.x; nx < maxXY.x ; nx ++, inputXy.x++) {
			const float deltaX = (realCoordinate.x - nx);
			const float measuredDistance = deltaX*deltaX+deltaY*deltaY;
			if (measuredDistance <= distanceSquared) {
				value = max(value, read_imagef(inputImage, SAMPLER_NEAREST, inputXy).s0);
			}
		}
	}

	float4 color = {value,0.0f,0.0f,0.0f};
	write_imagef(output, coords, color);
}

// KERNEL --- DILATE ---
__kernel void erodeKernel(__read_only image2d_t inputImage,  __write_only image2d_t output,
                           int2 offsetInput, int2 offsetOutput, int scope, int distanceSquared, int2 dimension, 
                           int2 offset)
{
	int2 coords = {get_global_id(0), get_global_id(1)}; 
	coords += offset;
	const int2 realCoordinate = coords + offsetOutput;

	const int2 minXY = max(realCoordinate - scope, zero);
	const int2 maxXY = min(realCoordinate + scope, dimension);
	
	float value = 1.0f;
	int nx, ny;
	int2 inputXy;
	
	for (ny = minXY.y, inputXy.y = ny - offsetInput.y ; ny < maxXY.y ; ny ++, inputXy.y++) {
		for (nx = minXY.x, inputXy.x = nx - offsetInput.x; nx < maxXY.x ; nx ++, inputXy.x++) {
			const float deltaX = (realCoordinate.x - nx);
			const float deltaY = (realCoordinate.y - ny);
			const float measuredDistance = deltaX*deltaX+deltaY*deltaY;
			if (measuredDistance <= distanceSquared) {
				value = min(value, read_imagef(inputImage, SAMPLER_NEAREST, inputXy).s0);
			}
		}
	}

	float4 color = {value,0.0f,0.0f,0.0f};
	write_imagef(output, coords, color);
}
