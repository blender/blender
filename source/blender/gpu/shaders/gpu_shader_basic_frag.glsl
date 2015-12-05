
/* Options:
 *
 * USE_COLOR: use glColor for diffuse colors
 * USE_TEXTURE: use texture for diffuse colors
 * USE_SCENE_LIGHTING: use lights (up to 8)
 * USE_SOLID_LIGHTING: assume 3 directional lights for solid draw mode
 * USE_TWO_SIDED: flip normal towards viewer
 * NO_SPECULAR: use specular component
 */

#define NUM_SOLID_LIGHTS 3
#define NUM_SCENE_LIGHTS 8

#if defined(USE_SOLID_LIGHTING) || defined(USE_SCENE_LIGHTING)
varying vec3 varying_normal;

#ifndef USE_SOLID_LIGHTING
varying vec3 varying_position;
#endif
#endif

#ifdef USE_COLOR
varying vec4 varying_vertex_color;
#endif

#ifdef USE_TEXTURE
varying vec2 varying_texture_coord;
uniform sampler2D texture_map;
#endif

void main()
{
#if defined(USE_SOLID_LIGHTING) || defined(USE_SCENE_LIGHTING)
	/* compute normal */
	vec3 N = normalize(varying_normal);

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
		L_diffuse += light_diffuse*diffuse_bsdf;

#ifndef NO_SPECULAR
		/* specular light */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = gl_LightSource[i].halfVector.xyz;

		float specular_bsdf = pow(max(dot(N, H), 0.0), gl_FrontMaterial.shininess);
		L_specular += light_specular*specular_bsdf;
#endif
	}
#else
	/* all 8 lights, makes no assumptions, potentially slow */

#ifndef NO_SPECULAR
	/* view vector computation, depends on orthographics or perspective */
	vec3 V = (gl_ProjectionMatrix[3][3] == 0.0) ? normalize(varying_position): vec3(0.0, 0.0, -1.0);
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
		L_diffuse += light_diffuse*diffuse_bsdf*intensity;

#ifndef NO_SPECULAR
		/* specular light */
		vec3 light_specular = gl_LightSource[i].specular.rgb;
		vec3 H = normalize(light_direction - V);

		float specular_bsdf = pow(max(dot(N, H), 0.0), gl_FrontMaterial.shininess);
		L_specular += light_specular*specular_bsdf*intensity;
#endif
	}
#endif

	/* compute diffuse color, possibly from texture or vertex colors */
	float alpha;

#if defined(USE_TEXTURE) && defined(USE_COLOR)
	vec4 texture_color = texture2D(texture_map, varying_texture_coord);

	L_diffuse *= texture_color.rgb * varying_vertex_color.rgb;
	alpha = texture_color.a * varying_vertex_color.a;
#elif defined(USE_TEXTURE)
	vec4 texture_color = texture2D(texture_map, varying_texture_coord);

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
	L += L_specular*gl_FrontMaterial.specular.rgb;
#endif

	/* write out fragment color */
	gl_FragColor = vec4(L, alpha);
#else

	/* no lighting */
#if defined(USE_TEXTURE) && defined(USE_COLOR)
	gl_FragColor = texture2D(texture_map, varying_texture_coord) * varying_vertex_color;
#elif defined(USE_TEXTURE)
	gl_FragColor = texture2D(texture_map, varying_texture_coord);
#elif defined(USE_COLOR)
	gl_FragColor = varying_vertex_color;
#else
	gl_FragColor = gl_FrontMaterial.diffuse;
#endif

#endif
}

