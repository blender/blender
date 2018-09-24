
uniform mat4 ViewProjectionMatrix;
uniform float alpha;

/* ---- Instantiated Attribs ---- */
in vec3 pos;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec3 color;
#ifdef UNIFORM_SCALE
in float size;
#else
in vec3 size;
#endif

flat out vec4 finalColor;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos * size, 1.0);
	finalColor = vec4(color, alpha);
}
