
uniform mat4 ViewProjectionMatrix;
uniform int baseId;

/* ---- Instantiated Attribs ---- */
in vec3 pos;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
#ifdef UNIFORM_SCALE
in float size;
#else
in vec3 size;
#endif
in int callId;

flat out uint finalId;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos * size, 1.0);
	finalId = uint(baseId + callId);
}
