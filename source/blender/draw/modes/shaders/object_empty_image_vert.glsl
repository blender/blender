uniform mat4 ModelViewProjectionMatrix;
uniform float aspectX;
uniform float aspectY;
uniform float size;
uniform vec2 offset;
#ifdef USE_WIRE
uniform vec3 color;
#else
uniform vec4 objectColor;
#endif

in vec2 texCoord;
in vec2 pos;

flat out vec4 finalColor;

#ifndef USE_WIRE
out vec2 texCoord_interp;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(
		(pos + offset) * (size * vec2(aspectX, aspectY)),
		0.0, 1.0);
#ifdef USE_WIRE
	finalColor = vec4(color, 1.0);
#else
	texCoord_interp = texCoord;
	finalColor = objectColor;
#endif
}
