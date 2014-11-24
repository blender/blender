
varying vec3 varposition;
varying vec3 varnormal;

void main()
{
	/* position does not need to be transformed, we already have it */
	gl_Position = gl_Vertex;

	varposition = gl_Vertex.xyz;

	varnormal = normalize(-varposition);

