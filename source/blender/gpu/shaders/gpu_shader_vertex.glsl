
varying vec3 varposition;
varying vec3 varnormal;

void main()
{
	vec4 co = gl_ModelViewMatrix * gl_Vertex;

	varposition = co.xyz;
	varnormal = normalize(gl_NormalMatrix * gl_Normal);
	gl_Position = gl_ProjectionMatrix * co;

	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA graphic cards.
	// gl_ClipVertex works only on NVIDIA graphic cards so we have to check with 
	// __GLSL_CG_DATA_TYPES if a NVIDIA graphic card is used (Cg support).
	// gl_ClipVerte is supported up to GLSL 1.20.
	#ifdef __GLSL_CG_DATA_TYPES 
		gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex; 
	#endif 

