#define OBJECT_OVERLAP_OFFSET 1
#define NO_OBJECT_ID uint(0)

float calculate_object_overlap(usampler2D objectId, ivec2 texel, uint object_id)
{
	uvec4 oid_offset = uvec4(
	    texelFetchOffset(objectId, texel, 0, ivec2(0,  OBJECT_OVERLAP_OFFSET)).r,
	    texelFetchOffset(objectId, texel, 0, ivec2(0, -OBJECT_OVERLAP_OFFSET)).r,
	    texelFetchOffset(objectId, texel, 0, ivec2(-OBJECT_OVERLAP_OFFSET, 0)).r,
	    texelFetchOffset(objectId, texel, 0, ivec2( OBJECT_OVERLAP_OFFSET, 0)).r);

	return dot(vec4(equal(uvec4(object_id), oid_offset)), vec4(0.25));
}
