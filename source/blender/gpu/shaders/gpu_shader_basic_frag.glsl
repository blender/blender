
/* Options:
 *
 * USE_COLOR: use glColor for diffuse colors
 * USE_TEXTURE: use texture for diffuse colors
 * USE_TEXTURE_RECTANGLE: use GL_TEXTURE_RECTANGLE instead of GL_TEXTURE_2D
 * USE_SCENE_LIGHTING: use lights (up to 8)
 * USE_SOLID_LIGHTING: assume 3 directional lights for solid draw mode
 * USE_TWO_SIDED: flip normal towards viewer
 * NO_SPECULAR: use specular component
 */

#define NUM_SOLID_LIGHTS 3
#define NUM_SCENE_LIGHTS 8

/* Keep these in sync with GPU_basic_shader.h */
#define STIPPLE_HALFTONE                               0
#define STIPPLE_QUARTTONE                              1
#define STIPPLE_CHECKER_8PX                            2
#define STIPPLE_HEXAGON                                3
#define STIPPLE_DIAG_STRIPES                           4
#define STIPPLE_DIAG_STRIPES_SWAP                      5
#define STIPPLE_S3D_INTERLACE_ROW                      6
#define STIPPLE_S3D_INTERLACE_ROW_SWAP                 7
#define STIPPLE_S3D_INTERLACE_COLUMN                   8
#define STIPPLE_S3D_INTERLACE_COLUMN_SWAP              9
#define STIPPLE_S3D_INTERLACE_CHECKERBOARD             10
#define STIPPLE_S3D_INTERLACE_CHECKERBOARD_SWAP        11

#if defined(USE_SOLID_LIGHTING) || defined(USE_SCENE_LIGHTING)
#if defined(USE_FLAT_NORMAL)
varying vec3 eyespace_vert_pos;
#else
varying vec3 varying_normal;
#endif
#ifndef USE_SOLID_LIGHTING
varying vec3 varying_position;
#endif
#endif

#ifdef USE_COLOR
varying vec4 varying_vertex_color;
#endif

#ifdef USE_TEXTURE
#ifdef USE_TEXTURE_RECTANGLE
#define sampler2D_default sampler2DRect
#define texture2D_default texture2DRect
#else
#define sampler2D_default sampler2D
#define texture2D_default texture2D
#endif

varying vec2 varying_texture_coord;
uniform sampler2D_default texture_map;
#endif

#ifdef USE_STIPPLE
uniform int stipple_id;
#if defined(DRAW_LINE)
varying in float t;
uniform int stipple_pattern;
#endif
#endif

void main()
{
#if defined(USE_STIPPLE)
#if defined(DRAW_LINE)
	/* GLSL 1.3 */
	if (!bool((1 << int(mod(t, 16))) & stipple_pattern))
		discard;
#else
	/* We have to use mod function and integer casting.
	 * This can be optimized further with the bitwise operations
	 * when GLSL 1.3 is supported. */
	if (stipple_id == STIPPLE_HALFTONE ||
	    stipple_id == STIPPLE_S3D_INTERLACE_CHECKERBOARD ||
	    stipple_id == STIPPLE_S3D_INTERLACE_CHECKERBOARD_SWAP)
	{
		int result = int(mod(gl_FragCoord.x + gl_FragCoord.y, 2));
		bool dis = result == 0;
		if (stipple_id == STIPPLE_S3D_INTERLACE_CHECKERBOARD_SWAP)
			dis = !dis;
		if (dis)
			discard;
	}
	else if (stipple_id == STIPPLE_QUARTTONE) {
		int mody = int(mod(gl_FragCoord.y, 4));
		int modx = int(mod(gl_FragCoord.x, 4));
		if (mody == 0) {
			if (modx != 2)
				discard;
		}
		else if (mody == 2) {
			if (modx != 0)
				discard;
		}
		else
			discard;
	}
	else if (stipple_id == STIPPLE_CHECKER_8PX) {
		int result = int(mod(int(gl_FragCoord.x) / 8 + int(gl_FragCoord.y) / 8, 2));
		if (result != 0)
			discard;
	}
	else if (stipple_id == STIPPLE_DIAG_STRIPES) {
		int mody = int(mod(gl_FragCoord.y, 16));
		int modx = int(mod(gl_FragCoord.x, 16));
		if ((16 - modx > mody && mody > 8 - modx) || mody > 24 - modx)
			discard;
	}
	else if (stipple_id == STIPPLE_DIAG_STRIPES_SWAP) {
		int mody = int(mod(gl_FragCoord.y, 16));
		int modx = int(mod(gl_FragCoord.x, 16));
		if (!((16 - modx > mody && mody > 8 - modx) || mody > 24 - modx))
			discard;
	}
	else if (stipple_id == STIPPLE_S3D_INTERLACE_ROW || stipple_id == STIPPLE_S3D_INTERLACE_ROW_SWAP) {
		int result = int(mod(gl_FragCoord.y, 2));
		bool dis = result == 0;
		if (stipple_id == STIPPLE_S3D_INTERLACE_ROW_SWAP)
			dis = !dis;
		if (dis)
			discard;
	}
	else if (stipple_id == STIPPLE_S3D_INTERLACE_COLUMN || stipple_id == STIPPLE_S3D_INTERLACE_COLUMN_SWAP) {
		int result = int(mod(gl_FragCoord.x, 2));
		bool dis = result != 0;
		if (stipple_id == STIPPLE_S3D_INTERLACE_COLUMN_SWAP)
			dis = !dis;
		if (dis)
			discard;
	}
	else if (stipple_id == STIPPLE_HEXAGON) {
		int mody = int(mod(gl_FragCoord.y, 2));
		int modx = int(mod(gl_FragCoord.x, 4));
		if (mody != 0) {
			if (modx != 1)
				discard;
		}
		else {
			if (modx != 3)
				discard;
		}
	}
#endif /* !DRAW_LINE */
#endif /* USE_STIPPLE */

#if defined(USE_SOLID_LIGHTING) || defined(USE_SCENE_LIGHTING)
	/* compute normal */
#if defined(USE_FLAT_NORMAL)
	vec3 N = normalize(cross(dFdx(eyespace_vert_pos), dFdy(eyespace_vert_pos)));
#else
	vec3 N = normalize(varying_normal);
#endif

#ifdef USE_TWO_SIDED
	if (!gl_FrontFacing)
		N = -N;
#endif

	/* compute diffuse and specular lighting */
	vec3 L_diffuse = vec3(0.0);
#ifndef NO_SPECULAR
	vec3 L_specular = vec3(0.0);
#endif

#ifdef USE_SOLID_LIGHTING
	/* assume 3 directional lights */
	for (int i = 0; i < NUM_SOLID_LIGHTS; i++) {
		vec3 light_direction = gl_LightSource[i].position.xyz;

		/* diffuse light */
		vec3 light_diffuse = gl_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse * diffuse_bsdf;

#ifndef NO_SPECULAR
		/* specular light */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = gl_LightSource[i].halfVector.xyz;

		float specular_bsdf = pow(max(dot(N, H), 0.0), gl_FrontMaterial.shininess);
		L_specular += light_specular * specular_bsdf;
#endif
	}
#else
	/* all 8 lights, makes no assumptions, potentially slow */

#ifndef NO_SPECULAR
	/* view vector computation, depends on orthographics or perspective */
	vec3 V = (gl_ProjectionMatrix[3][3] == 0.0) ? normalize(varying_position) : vec3(0.0, 0.0, -1.0);
#endif

	for (int i = 0; i < NUM_SCENE_LIGHTS; i++) {
		/* todo: this is a slow check for disabled lights */
		if (gl_LightSource[i].specular.a == 0.0)
			continue;

		float intensity = 1.0;
		vec3 light_direction;

		if (gl_LightSource[i].position.w == 0.0) {
			/* directional light */
			light_direction = gl_LightSource[i].position.xyz;
		}
		else {
			/* point light */
			vec3 d = gl_LightSource[i].position.xyz - varying_position;
			light_direction = normalize(d);

			/* spot light cone */
			if (gl_LightSource[i].spotCutoff < 90.0) {
				float cosine = max(dot(light_direction, -gl_LightSource[i].spotDirection), 0.0);
				intensity = pow(cosine, gl_LightSource[i].spotExponent);
				intensity *= step(gl_LightSource[i].spotCosCutoff, cosine);
			}

			/* falloff */
			float distance = length(d);

			intensity /= gl_LightSource[i].constantAttenuation +
			             gl_LightSource[i].linearAttenuation * distance +
			             gl_LightSource[i].quadraticAttenuation * distance * distance;
		}

		/* diffuse light */
		vec3 light_diffuse = gl_LightSource[i].diffuse.rgb;
		float diffuse_bsdf = max(dot(N, light_direction), 0.0);
		L_diffuse += light_diffuse * diffuse_bsdf * intensity;

#ifndef NO_SPECULAR
		/* specular light */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = normalize(light_direction - V);

		float specular_bsdf = pow(max(dot(N, H), 0.0), gl_FrontMaterial.shininess);
		L_specular += light_specular * specular_bsdf * intensity;
#endif
	}
#endif

	/* compute diffuse color, possibly from texture or vertex colors */
	float alpha;

#if defined(USE_TEXTURE) && defined(USE_COLOR)
	vec4 texture_color = texture2D_default(texture_map, varying_texture_coord);

	L_diffuse *= texture_color.rgb * varying_vertex_color.rgb;
	alpha = texture_color.a * varying_vertex_color.a;
#elif defined(USE_TEXTURE)
	vec4 texture_color = texture2D_default(texture_map, varying_texture_coord);

	L_diffuse *= texture_color.rgb;
	alpha = texture_color.a;
#elif defined(USE_COLOR)
	L_diffuse *= varying_vertex_color.rgb;
	alpha = varying_vertex_color.a;
#else
	L_diffuse *= gl_FrontMaterial.diffuse.rgb;
	alpha = gl_FrontMaterial.diffuse.a;
#endif

	/* sum lighting */
	vec3 L = gl_FrontLightModelProduct.sceneColor.rgb + L_diffuse;

#ifndef NO_SPECULAR
	L += L_specular * gl_FrontMaterial.specular.rgb;
#endif

	/* write out fragment color */
	gl_FragColor = vec4(L, alpha);
#else

	/* no lighting */
#if defined(USE_TEXTURE) && defined(USE_COLOR)
	gl_FragColor = texture2D_default(texture_map, varying_texture_coord) * varying_vertex_color;
#elif defined(USE_TEXTURE)
	gl_FragColor = texture2D_default(texture_map, varying_texture_coord);
#elif defined(USE_COLOR)
	gl_FragColor = varying_vertex_color;
#else
	gl_FragColor = gl_FrontMaterial.diffuse;
#endif

#endif
}

