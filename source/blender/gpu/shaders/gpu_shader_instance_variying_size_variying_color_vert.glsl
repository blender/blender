
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attribs ---- */
in vec3 pos;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;
#ifdef UNIFORM_SCALE
in float size;
#else
in vec3 size;
#endif

flat out vec4 finalColor;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos * size, 1.0);
	finalColor = color;
}
