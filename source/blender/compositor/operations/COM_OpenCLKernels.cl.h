/* clkernelstoh output of file <COM_OpenCLKernels_cl> */

const char * clkernelstoh_COM_OpenCLKernels_cl = "/// This file contains all opencl kernels for node-operation implementations\n" \
"\n" \
"// Global SAMPLERS\n" \
"const sampler_t SAMPLER_NEAREST      = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;\n" \
"\n" \
"__constant const int2 zero = {0,0};\n" \
"\n" \
"// KERNEL --- BOKEH BLUR ---\n" \
"__kernel void bokehBlurKernel(__read_only image2d_t boundingBox, __read_only image2d_t inputImage,\n" \
"                              __read_only image2d_t bokehImage, __write_only image2d_t output,\n" \
"                              int2 offsetInput, int2 offsetOutput, int radius, int step, int2 dimension, int2 offset)\n" \
"{\n" \
"	int2 coords = {get_global_id(0), get_global_id(1)};\n" \
"	coords += offset;\n" \
"	float tempBoundingBox;\n" \
"	float4 color = {0.0f,0.0f,0.0f,0.0f};\n" \
"	float4 multiplyer = {0.0f,0.0f,0.0f,0.0f};\n" \
"	float4 bokeh;\n" \
"	const float radius2 = radius*2.0f;\n" \
"	const int2 realCoordinate = coords + offsetOutput;\n" \
"\n" \
"	tempBoundingBox = read_imagef(boundingBox, SAMPLER_NEAREST, coords).s0;\n" \
"\n" \
"	if (tempBoundingBox > 0.0f && radius > 0 ) {\n" \
"		const int2 bokehImageDim = get_image_dim(bokehImage);\n" \
"		const int2 bokehImageCenter = bokehImageDim/2;\n" \
"		const int2 minXY = max(realCoordinate - radius, zero);\n" \
"		const int2 maxXY = min(realCoordinate + radius, dimension);\n" \
"		int nx, ny;\n" \
"\n" \
"		float2 uv;\n" \
"		int2 inputXy;\n" \
"\n" \
"		for (ny = minXY.y, inputXy.y = ny - offsetInput.y ; ny < maxXY.y ; ny +=step, inputXy.y+=step) {\n" \
"			uv.y = ((realCoordinate.y-ny)/radius2)*bokehImageDim.y+bokehImageCenter.y;\n" \
"\n" \
"			for (nx = minXY.x, inputXy.x = nx - offsetInput.x; nx < maxXY.x ; nx +=step, inputXy.x+=step) {\n" \
"				uv.x = ((realCoordinate.x-nx)/radius2)*bokehImageDim.x+bokehImageCenter.x;\n" \
"				bokeh = read_imagef(bokehImage, SAMPLER_NEAREST, uv);\n" \
"				color += bokeh * read_imagef(inputImage, SAMPLER_NEAREST, inputXy);\n" \
"				multiplyer += bokeh;\n" \
"			}\n" \
"		}\n" \
"		color /= multiplyer;\n" \
"\n" \
"	} else {\n" \
"		int2 imageCoordinates = realCoordinate - offsetInput;\n" \
"		color = read_imagef(inputImage, SAMPLER_NEAREST, imageCoordinates);\n" \
"	}\n" \
"\n" \
"	write_imagef(output, coords, color);\n" \
"}\n" \
"\n" \
"//KERNEL --- DEFOCUS /VARIABLESIZEBOKEHBLUR ---\n" \
"__kernel void defocusKernel(__read_only image2d_t inputImage, __read_only image2d_t bokehImage,\n" \
"					__read_only image2d_t inputSize,\n" \
"					__write_only image2d_t output, int2 offsetInput, int2 offsetOutput,\n" \
"					int step, int maxBlur, float threshold, int2 dimension, int2 offset)\n" \
"{\n" \
"	float4 color = {1.0f, 0.0f, 0.0f, 1.0f};\n" \
"	int2 coords = {get_global_id(0), get_global_id(1)};\n" \
"	coords += offset;\n" \
"	const int2 realCoordinate = coords + offsetOutput;\n" \
"\n" \
"	float4 readColor;\n" \
"	float4 bokeh;\n" \
"	float tempSize;\n" \
"	float4 multiplier_accum = {1.0f, 1.0f, 1.0f, 1.0f};\n" \
"	float4 color_accum;\n" \
"\n" \
"	int minx = max(realCoordinate.s0 - maxBlur, 0);\n" \
"	int miny = max(realCoordinate.s1 - maxBlur, 0);\n" \
"	int maxx = min(realCoordinate.s0 + maxBlur, dimension.s0);\n" \
"	int maxy = min(realCoordinate.s1 + maxBlur, dimension.s1);\n" \
"\n" \
"	{\n" \
"		int2 inputCoordinate = realCoordinate - offsetInput;\n" \
"		float size = read_imagef(inputSize, SAMPLER_NEAREST, inputCoordinate).s0;\n" \
"		color_accum = read_imagef(inputImage, SAMPLER_NEAREST, inputCoordinate);\n" \
"\n" \
"		for (int ny = miny; ny < maxy; ny += step) {\n" \
"			for (int nx = minx; nx < maxx; nx += step) {\n" \
"				if (nx >= 0 && nx < dimension.s0 && ny >= 0 && ny < dimension.s1) {\n" \
"					inputCoordinate.s0 = nx - offsetInput.s0;\n" \
"					inputCoordinate.s1 = ny - offsetInput.s1;\n" \
"					tempSize = read_imagef(inputSize, SAMPLER_NEAREST, inputCoordinate).s0;\n" \
"					if (size > threshold && tempSize > threshold) {\n" \
"						float dx = nx - realCoordinate.s0;\n" \
"						float dy = ny - realCoordinate.s1;\n" \
"						if (dx != 0 || dy != 0) {\n" \
"							if (tempSize >= fabs(dx) && tempSize >= fabs(dy)) {\n" \
"								float2 uv = { 256.0f + dx * 256.0f / tempSize, 256.0f + dy * 256.0f / tempSize};\n" \
"								bokeh = read_imagef(bokehImage, SAMPLER_NEAREST, uv);\n" \
"								readColor = read_imagef(inputImage, SAMPLER_NEAREST, inputCoordinate);\n" \
"								color_accum += bokeh*readColor;\n" \
"								multiplier_accum += bokeh;\n" \
"							}\n" \
"						}\n" \
"					}\n" \
"				}\n" \
"			}\n" \
"		}\n" \
"	}\n" \
"\n" \
"	color = color_accum * (1.0f / multiplier_accum);\n" \
"	write_imagef(output, coords, color);\n" \
"}\n" \
"\n" \
"\n" \
"// KERNEL --- DILATE ---\n" \
"__kernel void dilateKernel(__read_only image2d_t inputImage,  __write_only image2d_t output,\n" \
"                           int2 offsetInput, int2 offsetOutput, int scope, int distanceSquared, int2 dimension,\n" \
"                           int2 offset)\n" \
"{\n" \
"	int2 coords = {get_global_id(0), get_global_id(1)};\n" \
"	coords += offset;\n" \
"	const int2 realCoordinate = coords + offsetOutput;\n" \
"\n" \
"	const int2 minXY = max(realCoordinate - scope, zero);\n" \
"	const int2 maxXY = min(realCoordinate + scope, dimension);\n" \
"\n" \
"	float value = 0.0f;\n" \
"	int nx, ny;\n" \
"	int2 inputXy;\n" \
"\n" \
"	for (ny = minXY.y, inputXy.y = ny - offsetInput.y ; ny < maxXY.y ; ny ++, inputXy.y++) {\n" \
"		const float deltaY = (realCoordinate.y - ny);\n" \
"		for (nx = minXY.x, inputXy.x = nx - offsetInput.x; nx < maxXY.x ; nx ++, inputXy.x++) {\n" \
"			const float deltaX = (realCoordinate.x - nx);\n" \
"			const float measuredDistance = deltaX*deltaX+deltaY*deltaY;\n" \
"			if (measuredDistance <= distanceSquared) {\n" \
"				value = max(value, read_imagef(inputImage, SAMPLER_NEAREST, inputXy).s0);\n" \
"			}\n" \
"		}\n" \
"	}\n" \
"\n" \
"	float4 color = {value,0.0f,0.0f,0.0f};\n" \
"	write_imagef(output, coords, color);\n" \
"}\n" \
"\n" \
"// KERNEL --- DILATE ---\n" \
"__kernel void erodeKernel(__read_only image2d_t inputImage,  __write_only image2d_t output,\n" \
"                           int2 offsetInput, int2 offsetOutput, int scope, int distanceSquared, int2 dimension,\n" \
"                           int2 offset)\n" \
"{\n" \
"	int2 coords = {get_global_id(0), get_global_id(1)};\n" \
"	coords += offset;\n" \
"	const int2 realCoordinate = coords + offsetOutput;\n" \
"\n" \
"	const int2 minXY = max(realCoordinate - scope, zero);\n" \
"	const int2 maxXY = min(realCoordinate + scope, dimension);\n" \
"\n" \
"	float value = 1.0f;\n" \
"	int nx, ny;\n" \
"	int2 inputXy;\n" \
"\n" \
"	for (ny = minXY.y, inputXy.y = ny - offsetInput.y ; ny < maxXY.y ; ny ++, inputXy.y++) {\n" \
"		for (nx = minXY.x, inputXy.x = nx - offsetInput.x; nx < maxXY.x ; nx ++, inputXy.x++) {\n" \
"			const float deltaX = (realCoordinate.x - nx);\n" \
"			const float deltaY = (realCoordinate.y - ny);\n" \
"			const float measuredDistance = deltaX*deltaX+deltaY*deltaY;\n" \
"			if (measuredDistance <= distanceSquared) {\n" \
"				value = min(value, read_imagef(inputImage, SAMPLER_NEAREST, inputXy).s0);\n" \
"			}\n" \
"		}\n" \
"	}\n" \
"\n" \
"	float4 color = {value,0.0f,0.0f,0.0f};\n" \
"	write_imagef(output, coords, color);\n" \
"}\n" \
"\0";
