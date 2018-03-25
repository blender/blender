uniform vec2 invscreenres;
uniform mat4 WinMatrix;

/* Matcap */
uniform sampler2DArray matcaps;

/* Screen Space Occlusion */
/* store the view space vectors for the corners of the view frustum here.
 * It helps to quickly reconstruct view space vectors by using uv coordinates,
 * see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
uniform vec4 viewvecs[3];
uniform vec4 ssao_params;

uniform sampler2D ssao_jitter;

/* Material Parameters packed in an UBO */
struct Material {
	vec4 ssao_params_var;
	vec4 matcap_hsv_id;
	vec4 matcap_rot; /* vec4 to ensure 16 bytes alignement (don't trust compiler) */
};

layout(std140) uniform samples_block {
	vec4 ssao_samples[500];
};

layout(std140) uniform matcaps_block {
	vec4 matcaps_color[24];
};

layout(std140) uniform material_block {
	Material matcaps_param[MAX_MATERIAL];
};

#ifdef DEFERRED_SHADING
uniform sampler2D depthtex;
uniform sampler2D normaltex;
uniform isampler2D idtex;
int mat_id; /* global */
#else
uniform int mat_id;
#endif

/* Aliases */
#define ssao_samples_num	ssao_params.x
#define jitter_tilling		ssao_params.yz
#define dfdy_sign			ssao_params.w

#define matcap_hsv			matcaps_param[mat_id].matcap_hsv_id.xyz
#define matcap_index		matcaps_param[mat_id].matcap_hsv_id.w
#define matcap_rotation		matcaps_param[mat_id].matcap_rot.xy

#ifndef DEFERRED_SHADING
#  ifdef USE_FLAT_NORMAL
flat in vec3 normal;
#  else
in vec3 normal;
#  endif
#endif

out vec4 fragColor;

/* TODO Move this to SSAO modules */
/* simple depth reconstruction, see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer
 * we change the factors from the article to fit the OpennGL model.  */
vec3 get_view_space_from_depth(in vec2 uvcoords, in float depth)
{
	if (WinMatrix[3][3] == 0.0) {
		/* Perspective */
		float d = 2.0 * depth - 1.0;

		float zview = -WinMatrix[3][2] / (d + WinMatrix[2][2]);

		return zview * (viewvecs[0].xyz + vec3(uvcoords, 0.0) * viewvecs[1].xyz);
	}
	else {
		/* Orthographic */
		vec3 offset = vec3(uvcoords, depth);

		return viewvecs[0].xyz + offset * viewvecs[1].xyz;
	}
}

#ifdef USE_HSV
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
#endif

#ifdef USE_AO
/* Prototype */
void ssao_factors(
        in float depth, in vec3 normal, in vec3 position, in vec2 screenco,
        out float cavities, out float edges);
#endif

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec3 normal_decode(vec2 enc)
{
	vec2 fenc = enc * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(1.0 - f / 4.0);
	vec3 n;
	n.xy = fenc*g;
	n.z = 1 - f / 2;
	return n;
}

vec3 shade(vec3 N, vec3 position, float depth, vec2 screenco)
{
#ifdef USE_ROTATION
	/* Rotate texture coordinates */
	vec2 rotY = vec2(-matcap_rotation.y, matcap_rotation.x);
	vec2 texco = abs(vec2(dot(N.xy, matcap_rotation), dot(N.xy, rotY)) * .49 + 0.5);
#else
	vec2 texco = abs(N.xy * .49 + 0.5);
#endif
	vec3 col = texture(matcaps, vec3(texco, matcap_index)).rgb;

#ifdef USE_AO
	float cavity = 0.0, edges = 0.0;
	ssao_factors(depth, N, position, screenco, cavity, edges);

	col *= mix(vec3(1.0), matcaps_color[int(matcap_index)].rgb, cavity);
#endif

#ifdef USE_HSV
	hue_sat(matcap_hsv.x, matcap_hsv.y, matcap_hsv.z, col);
#endif

#ifdef USE_AO
	/* Apply highlights after hue shift */
	col *= edges + 1.0;
#endif

	return col;
}

void main()
{
	vec2 screenco = vec2(gl_FragCoord.xy) * invscreenres;

#ifdef DEFERRED_SHADING
	ivec2 texel = ivec2(gl_FragCoord.xy);
	mat_id = texelFetch(idtex, texel, 0).r;

	/* early out (manual stencil test) */
	if (mat_id == 0)
		discard;

	float depth = texelFetch(depthtex, texel, 0).r;
	vec3 N = normal_decode(texelFetch(normaltex, texel, 0).rg);
	/* see the prepass for explanations. */
	if (mat_id < 0) {
		N = -N;
	}
	mat_id = abs(mat_id) - 1;
#else
	float depth = gl_FragCoord.z;
	vec3 N = normal;
#endif

	vec3 position = get_view_space_from_depth(screenco, depth);

	vec3 col = shade(N, position, depth, screenco);

	fragColor = vec4(col, 1.0);
}
