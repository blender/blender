
in vec2 pos;

out vec3 varposition;
out vec3 varnormal;
out vec3 viewPosition;

#ifndef VOLUMETRICS
/* necessary for compilation*/
out vec3 worldPosition;
out vec3 worldNormal;
out vec3 viewNormal;
#endif

void main()
{
	gl_Position = vec4(pos, 1.0, 1.0);
	varposition = viewPosition = vec3(pos, -1.0);
	varnormal = normalize(-varposition);

#ifndef VOLUMETRICS
	/* Not used in practice but needed to avoid compilation errors. */
	worldPosition = viewPosition;
	worldNormal = viewNormal = varnormal;
#endif

#ifdef ATTRIB
	pass_attrib(viewPosition);
#endif
}
