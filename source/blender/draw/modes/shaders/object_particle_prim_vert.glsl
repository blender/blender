
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ViewProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;
uniform int screen_space;
uniform float draw_size;
uniform vec3 color;
uniform sampler1D ramp;

in vec3 pos;
in vec4 rot;
in float val;
in vec3 inst_pos;
in int axis;

flat out vec4 finalColor;

vec3 rotate(vec3 vec, vec4 quat)
{
	/* The quaternion representation here stores the w component in the first index */
	return vec + 2.0 * cross(quat.yzw, cross(quat.yzw, vec) + quat.x * vec);
}

void main()
{
	if (screen_space == 1) {
		gl_Position = ModelViewMatrix * vec4(pos, 1.0) + vec4(inst_pos * draw_size, 0.0);
		gl_Position = ProjectionMatrix * gl_Position;
	}
	else {
		float size = draw_size;

		if (axis > -1) {
			size *= 2;
		}

		gl_Position = ModelViewProjectionMatrix * vec4(pos + rotate(inst_pos * size, rot), 1.0);
	}

#ifdef USE_AXIS
	if (axis == 0)
		finalColor = vec4(1.0, 0.0, 0.0, 1.0);
	else if (axis == 1)
		finalColor = vec4(0.0, 1.0, 0.0, 1.0);
	else
		finalColor = vec4(0.0, 0.0, 1.0, 1.0);
#else
	if (val < 0.0) {
		finalColor = vec4(color, 1.0);
	}
	else {
		finalColor = vec4(texture(ramp, val).rgb, 1.0);
	}
#endif
}
