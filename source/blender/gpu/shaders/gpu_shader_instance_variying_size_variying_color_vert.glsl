
uniform mat4 ViewProjectionMatrix;
uniform mat4 ModelMatrix;

/* ---- Instantiated Attrs ---- */
in vec3 pos;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;
#ifdef UNIFORM_SCALE
in float size;
#else
in vec3 size;
#endif

flat out vec4 finalColor;

void main()
{
	finalColor = color;

	vec4 pos_4d = vec4(pos * size, 1.0);
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * pos_4d;

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * InstanceModelMatrix * pos_4d).xyz);
#endif
}
