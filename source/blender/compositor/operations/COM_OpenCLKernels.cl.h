/* clkernelstoh output of file <COM_OpenCLKernels_cl> */

const char * clkernelstoh_COM_OpenCLKernels_cl = "/// This file contains all opencl kernels for node-operation implementations\n" \
"\n" \
"// Global SAMPLERS\n" \
"const sampler_t SAMPLER_NEAREST      = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;\n" \
"\n" \
"__constant const int2 zero = {0,0};\n" \
"\n" \
"// KERNEL --- BOKEH BLUR ---\n" \
"__kernel void bokehBlurKernel(__global __read_only image2d_t boundingBox, __global __read_only image2d_t inputImage,\n" \
"                              __global __read_only image2d_t bokehImage, __global __write_only image2d_t output,\n" \
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
"	if (tempBoundingBox > 0.0f) {\n" \
"		const int2 bokehImageDim = get_image_dim(bokehImage);\n" \
"		const int2 bokehImageCenter = bokehImageDim/2;\n" \
"		const int2 minXY = max(realCoordinate - radius, zero);;\n" \
"		const int2 maxXY = min(realCoordinate + radius, dimension);;\n" \
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
"\0";
