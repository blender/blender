
in vec2 pos;

out vec3 varposition;
out vec3 varnormal;
out vec3 viewPosition;

/* necessary for compilation*/
out vec3 worldPosition;
out vec3 worldNormal;

void main()
{
	gl_Position = vec4(pos, 1.0, 1.0);
	varposition = viewPosition = vec3(pos, -1.0);
	varnormal = normalize(-varposition);
}
