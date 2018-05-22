#define OBJECT_OUTLINE_OFFSET 1

float calculate_object_outline(usampler2D objectId, ivec2 texel, uint object_id)
{
	uvec4 oid_offset = uvec4(
	    texelFetchOffset(objectId, texel, 0, ivec2(0,  OBJECT_OUTLINE_OFFSET)).r,
	    texelFetchOffset(objectId, texel, 0, ivec2(0, -OBJECT_OUTLINE_OFFSET)).r,
	    texelFetchOffset(objectId, texel, 0, ivec2(-OBJECT_OUTLINE_OFFSET, 0)).r,
	    texelFetchOffset(objectId, texel, 0, ivec2( OBJECT_OUTLINE_OFFSET, 0)).r);

	return dot(vec4(equal(uvec4(object_id), oid_offset)), vec4(0.25));
}
