
uniform mat4 ViewProjectionMatrix;
uniform int baseId;

/* ---- Instantiated Attrs ---- */
in vec3 pos;

/* ---- Per instance Attrs ---- */
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
