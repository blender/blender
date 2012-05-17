/// This file contains all opencl kernels for node-operation implementations 

__kernel void testKernel(__global __write_only image2d_t output){
	int x = get_global_id(0);
	int y = get_global_id(1);
	int2 coords = {x, y}; 
	float4 color = {0.0f, 1.0f, 0.0f, 1.0f};
	write_imagef(output, coords, color);
}
