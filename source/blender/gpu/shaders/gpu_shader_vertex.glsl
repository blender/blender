
varying vec3 varposition;
varying vec3 varnormal;

#ifdef CLIP_WORKAROUND
varying float gl_ClipDistance[6];
#endif

void main()
{
	vec4 co = gl_ModelViewMatrix * gl_Vertex;

	varposition = co.xyz;
	varnormal = normalize(gl_NormalMatrix * gl_Normal);
	gl_Position = gl_ProjectionMatrix * co;

#ifdef CLIP_WORKAROUND
	int i;
	for(i = 0; i < 6; i++)
		gl_ClipDistance[i] = dot(co, gl_ClipPlane[i]);
#elif !defined(GPU_ATI)
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = co; 
#endif 

