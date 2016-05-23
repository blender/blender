#ifdef USE_OPENSUBDIV
in vec3 normal;
in vec4 position;

out block {
	VertexData v;
} outpt;
#endif

varying vec3 varposition;
varying vec3 varnormal;

#ifdef CLIP_WORKAROUND
varying float gl_ClipDistance[6];
#endif

void main()
{
#ifndef USE_OPENSUBDIV
	vec4 position = gl_Vertex;
	vec3 normal = gl_Normal;
#endif

	vec4 co = gl_ModelViewMatrix * position;

	varposition = co.xyz;
	varnormal = normalize(gl_NormalMatrix * normal);
	gl_Position = gl_ProjectionMatrix * co;

#ifdef CLIP_WORKAROUND
	int i;
	for (i = 0; i < 6; i++)
		gl_ClipDistance[i] = dot(co, gl_ClipPlane[i]);
#elif !defined(GPU_ATI)
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = co; 
#endif 

#ifdef USE_OPENSUBDIV
	outpt.v.position = co;
	outpt.v.normal = varnormal;
#endif
