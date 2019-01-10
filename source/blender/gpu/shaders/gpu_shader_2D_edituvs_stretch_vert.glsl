
uniform mat4 ModelViewProjectionMatrix;
uniform vec2 aspect;

in vec2 pos;

#ifndef STRETCH_ANGLE
in float stretch;
#else

in vec4 uv_adj;
in float angle;
#endif

noperspective out vec4 finalColor;

vec3 weight_to_rgb(float weight)
{
	vec3 r_rgb;
	float blend = ((weight / 2.0) + 0.5);

	if (weight <= 0.25) {    /* blue->cyan */
		r_rgb[0] = 0.0;
		r_rgb[1] = blend * weight * 4.0;
		r_rgb[2] = blend;
	}
	else if (weight <= 0.50) {  /* cyan->green */
		r_rgb[0] = 0.0;
		r_rgb[1] = blend;
		r_rgb[2] = blend * (1.0 - ((weight - 0.25) * 4.0));
	}
	else if (weight <= 0.75) {  /* green->yellow */
		r_rgb[0] = blend * ((weight - 0.50) * 4.0);
		r_rgb[1] = blend;
		r_rgb[2] = 0.0;
	}
	else if (weight <= 1.0) {  /* yellow->red */
		r_rgb[0] = blend;
		r_rgb[1] = blend * (1.0 - ((weight - 0.75) * 4.0));
		r_rgb[2] = 0.0;
	}
	else {
		/* exceptional value, unclamped or nan,
		 * avoid uninitialized memory use */
		r_rgb[0] = 1.0;
		r_rgb[1] = 0.0;
		r_rgb[2] = 1.0;
	}

	return r_rgb;
}

#define M_PI       3.1415926535897932

/* Adapted from BLI_math_vector.h */
float angle_normalized_v2v2(vec2 v1, vec2 v2)
{
	v1 = normalize(v1 * aspect);
	v2 = normalize(v2 * aspect);
	/* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
	bool q = (dot(v1, v2) >= 0.0);
	vec2 v = (q) ? (v1 - v2) : (v1 + v2);
	float a = 2.0 * asin(length(v) / 2.0);
	return (q) ? a : M_PI - a;
}

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

#ifdef STRETCH_ANGLE
	float uv_angle = angle_normalized_v2v2(uv_adj.xy, uv_adj.zw) / M_PI;
	float stretch = 1.0 - abs(uv_angle - angle);
	stretch = stretch;
	stretch = 1.0 - stretch * stretch;
#endif

	finalColor = vec4(weight_to_rgb(stretch), 1.0);
}
