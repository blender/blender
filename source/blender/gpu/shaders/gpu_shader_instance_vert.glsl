
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attrs ---- */
in vec3 pos;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos, 1.0);
}
