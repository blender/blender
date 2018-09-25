
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attribs ---- */
in vec3 pos;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos, 1.0);
}
