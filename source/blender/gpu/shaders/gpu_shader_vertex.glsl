
varying vec3 varposition;
varying vec3 varnormal;

void main()
{
	vec4 co = gl_ModelViewMatrix * gl_Vertex;

	varposition = co.xyz;
	varnormal = normalize(gl_NormalMatrix * gl_Normal);
	gl_Position = gl_ProjectionMatrix * co;

#ifndef GPU_ATI
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex; 
#endif 

