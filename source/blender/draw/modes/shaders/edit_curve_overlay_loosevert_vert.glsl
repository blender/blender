/* Draw Curve Vertices */
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform vec2 viewportSize;

in vec3 pos;
in int data;

out vec4 finalColor;

void main()
{
	if ((data & VERT_SELECTED) != 0) {
		if ((data & VERT_ACTIVE) != 0) {
			finalColor = colorEditMeshActive;
		}
		else {
			finalColor = colorVertexSelect;
		}
	}
	else {
		finalColor = colorVertex;
	}

	vec4 pos_4d = vec4(pos, 1.0);
	gl_Position = ModelViewProjectionMatrix * pos_4d;
	gl_PointSize = sizeVertex * 2.0;
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
