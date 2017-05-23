
/* Material Parameters packed in an UBO */
struct Material {
	vec4 one;
	vec4 two;
};

layout(std140) uniform material_block {
	Material shader_param[MAX_MATERIAL];
};

uniform mat4 ProjectionMatrix;
uniform sampler2DArray matcaps;
uniform int mat_id;

#define randomness			shader_param[mat_id].one.x
#define matcap_index		shader_param[mat_id].one.y
#define matcap_rotation		shader_param[mat_id].one.zw
#define matcap_hsv			shader_param[mat_id].two.xyz

in vec3 tangent;
in vec3 viewPosition;
flat in float colRand;
out vec4 fragColor;

vec3 rotate(vec3 norm, vec3 ortho, float ang)
{
	return norm * cos(ang) + ortho * sin(ang);
}

void rgb_to_hsv(vec3 rgb, out vec3 outcol)
{
	float cmax, cmin, h, s, v, cdelta;
	vec3 c;

	cmax = max(rgb[0], max(rgb[1], rgb[2]));
	cmin = min(rgb[0], min(rgb[1], rgb[2]));
	cdelta = cmax - cmin;

	v = cmax;
	if (cmax != 0.0)
		s = cdelta / cmax;
	else {
		s = 0.0;
		h = 0.0;
	}

	if (s == 0.0) {
		h = 0.0;
	}
	else {
		c = (vec3(cmax, cmax, cmax) - rgb.xyz) / cdelta;

		if (rgb.x == cmax) h = c[2] - c[1];
		else if (rgb.y == cmax) h = 2.0 + c[0] -  c[2];
		else h = 4.0 + c[1] - c[0];

		h /= 6.0;

		if (h < 0.0)
			h += 1.0;
	}

	outcol = vec3(h, s, v);
}

void hsv_to_rgb(vec3 hsv, out vec3 outcol)
{
	float i, f, p, q, t, h, s, v;
	vec3 rgb;

	h = hsv[0];
	s = hsv[1];
	v = hsv[2];

	if (s == 0.0) {
		rgb = vec3(v, v, v);
	}
	else {
		if (h == 1.0)
			h = 0.0;

		h *= 6.0;
		i = floor(h);
		f = h - i;
		rgb = vec3(f, f, f);
		p = v * (1.0 - s);
		q = v * (1.0 - (s * f));
		t = v * (1.0 - (s * (1.0 - f)));

		if (i == 0.0) rgb = vec3(v, t, p);
		else if (i == 1.0) rgb = vec3(q, v, p);
		else if (i == 2.0) rgb = vec3(p, v, t);
		else if (i == 3.0) rgb = vec3(p, q, v);
		else if (i == 4.0) rgb = vec3(t, p, v);
		else rgb = vec3(v, p, q);
	}

	outcol = rgb;
}

void hue_sat(float hue, float sat, float value, inout vec3 col)
{
	vec3 hsv;

	rgb_to_hsv(col, hsv);

	hsv.x += hue;
	hsv.x -= floor(hsv.x);
	hsv.y *= sat;
	hsv.y = clamp(hsv.y, 0.0, 1.0);
	hsv.z *= value;
	hsv.z = clamp(hsv.z, 0.0, 1.0);

	hsv_to_rgb(hsv, col);
}

void main()
{
	vec3 viewvec = (ProjectionMatrix[3][3] == 0.0) ? normalize(viewPosition) : vec3(0.0, 0.0, -1.0);
	vec3 ortho = normalize(cross(viewvec, tangent));
	vec3 norm = normalize(cross(ortho, tangent));

	vec3 col = vec3(0);

	vec2 rotY = vec2(-matcap_rotation.y, matcap_rotation.x);

	for (int i = 0; i < 9; i++) {
		vec3 rotNorm = rotate(norm, ortho, -0.5 + (i * 0.125));
		vec3 ray = reflect(viewvec, rotNorm);
		vec2 texco = abs(vec2(dot(ray.xy, matcap_rotation), dot(ray.xy, rotY)) * .49 + 0.5);

		col += texture(matcaps, vec3(texco, matcap_index)).rgb / 9.0;
	}

	hue_sat(matcap_hsv.x, matcap_hsv.y, matcap_hsv.z, col);

	float maxChan = max(max(col.r, col.g), col.b);

	col += (colRand * maxChan * randomness * 1.5) - (maxChan * randomness * 0.75);

	fragColor.rgb = col;
	fragColor.a = 1.0;
}
