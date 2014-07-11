
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
#endif

#ifdef CLIP_WORKAROUND
varying float gl_ClipDistance[6];
#endif

void main()
{
	vec4 co = gl_ModelViewMatrix * gl_Vertex;

#if defined(USE_SOLID_LIGHTING) || defined(USE_SCENE_LIGHTING)
	varying_normal = normalize(gl_NormalMatrix * gl_Normal);

#ifndef USE_SOLID_LIGHTING
	varying_position = co.xyz;
#endif
#endif

	gl_Position = gl_ProjectionMatrix * co;

#ifdef CLIP_WORKAROUND
	int i;
	for(i = 0; i < 6; i++)
		gl_ClipDistance[i] = dot(co, gl_ClipPlane[i]);
#else
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = co; 
#endif 

#ifdef USE_COLOR
	varying_vertex_color = gl_Color;
#endif

#ifdef USE_TEXTURE
	varying_texture_coord = (gl_TextureMatrix[0] * gl_MultiTexCoord0).st;
#endif
}

