/* Draw Curve Normals */
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform float normalSize;

in vec3 pos;
in vec3 nor;
in vec3 tan;
in float rad;

void main()
{
	vec3 final_pos = pos;

	float flip = (gl_InstanceID != 0) ? -1.0 : 1.0;

	if (gl_VertexID % 2 == 0) {
		final_pos += normalSize * rad * (flip * nor - tan);
	}

	vec4 final_pos_4d = vec4(final_pos, 1.0);
	gl_Position = ModelViewProjectionMatrix * final_pos_4d;

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * final_pos_4d).xyz);
#endif
}
