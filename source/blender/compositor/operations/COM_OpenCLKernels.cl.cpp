/// @todo: this source needs to be generated from COM_OpenCLKernels.cl.
/// not implemented yet. new data to h

const char* sourcecode = "/// This file contains all opencl kernels for node-operation implementations \n" \
"\n" \
"__kernel void testKernel(__global __write_only image2d_t output){\n" \
"	int x = get_global_id(0);\n" \
"	int y = get_global_id(1);\n" \
"	int2 coords = {x, y}; \n" \
"	float4 color = {0.0f, 1.0f, 0.0f, 1.0f};\n" \
"	write_imagef(output, coords, color);\n" \
"}\n" \
"\0\n";

