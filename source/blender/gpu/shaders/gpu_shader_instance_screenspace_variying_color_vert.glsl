
uniform mat4 ViewProjectionMatrix;
uniform vec3 screen_vecs[2];
uniform float size;
uniform float pixel_size;

/* ---- Instantiated Attrs ---- */
in vec2 pos;

/* ---- Per instance Attrs ---- */
in vec3 world_pos;
in vec3 color;

flat out vec4 finalColor;

float mul_project_m4_v3_zfac(in vec3 co)
{
	return (ViewProjectionMatrix[0][3] * co.x) +
	       (ViewProjectionMatrix[1][3] * co.y) +
	       (ViewProjectionMatrix[2][3] * co.z) + ViewProjectionMatrix[3][3];
}

void main()
{
	float pix_size = mul_project_m4_v3_zfac(world_pos) * pixel_size;
	vec3 screen_pos = screen_vecs[0].xyz * pos.x + screen_vecs[1].xyz * pos.y;
	gl_Position = ViewProjectionMatrix * vec4(world_pos + screen_pos * size * pix_size, 1.0);
	finalColor = vec4(color, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
   world_clip_planes_calc_clip_distance(world_pos);
#endif
}
