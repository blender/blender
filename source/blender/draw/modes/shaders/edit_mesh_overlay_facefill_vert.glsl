
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform ivec4 dataMask = ivec4(0xFF);

in vec3 pos;
in ivec4 data;

flat out vec4 faceColor;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	ivec4 data_m = data & dataMask;

	faceColor = EDIT_MESH_face_color(data_m.x);

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
