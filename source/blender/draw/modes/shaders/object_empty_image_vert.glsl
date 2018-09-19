
uniform mat4 ViewProjectionMatrix;
uniform vec2 aspect;

/* ---- Instantiated Attribs ---- */
in vec2 texCoord;
in vec2 pos;
/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;

#ifdef USE_WIRE
in vec3 color;
#else
in vec4 objectColor;
#endif

in float size;
in vec2 offset;

flat out vec4 finalColor;

#ifndef USE_WIRE
out vec2 texCoord_interp;
#endif

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(
		(pos[0] + offset[0]) * (size * aspect[0]),
		(pos[1] + offset[1]) * (size * aspect[1]),
		0.0, 1.0);
#ifdef USE_WIRE
	finalColor = vec4(color, 1.0);
#else
	texCoord_interp = texCoord;
	finalColor = objectColor;
#endif
}
