#ifndef CURVATURE_OFFSET
#  define CURVATURE_OFFSET 1
#endif

float curvature_soft_clamp(float curvature, float control)
{
	if (curvature < 0.5 / control) {
		return curvature * (1.0 - curvature * control);
	}
	return 0.25 / control;
}

float calculate_curvature(usampler2D objectId, sampler2D normalBuffer, ivec2 texel, float ridge, float valley)
{
	uint object_up    = texelFetchOffset(objectId, texel, 0, ivec2(0,  CURVATURE_OFFSET)).r;
	uint object_down  = texelFetchOffset(objectId, texel, 0, ivec2(0, -CURVATURE_OFFSET)).r;
	uint object_left  = texelFetchOffset(objectId, texel, 0, ivec2(-CURVATURE_OFFSET, 0)).r;
	uint object_right = texelFetchOffset(objectId, texel, 0, ivec2( CURVATURE_OFFSET, 0)).r;

	if((object_up != object_down) || (object_right != object_left)) {
		return 0.0;
	}

	vec2 normal_up    = texelFetchOffset(normalBuffer, texel, 0, ivec2(0,  CURVATURE_OFFSET)).rg;
	vec2 normal_down  = texelFetchOffset(normalBuffer, texel, 0, ivec2(0, -CURVATURE_OFFSET)).rg;
	vec2 normal_left  = texelFetchOffset(normalBuffer, texel, 0, ivec2(-CURVATURE_OFFSET, 0)).rg;
	vec2 normal_right = texelFetchOffset(normalBuffer, texel, 0, ivec2( CURVATURE_OFFSET, 0)).rg;

	normal_up    = workbench_normal_decode(normal_up   ).rg;
	normal_down  = workbench_normal_decode(normal_down ).rg;
	normal_left  = workbench_normal_decode(normal_left ).rg;
	normal_right = workbench_normal_decode(normal_right).rg;

	float normal_diff = ((normal_up.g - normal_down.g) + (normal_right.r - normal_left.r));

	if (normal_diff < 0) {
		return -2.0 * curvature_soft_clamp(-normal_diff, valley);
	}

	return 2.0 * curvature_soft_clamp(normal_diff, ridge);
}
