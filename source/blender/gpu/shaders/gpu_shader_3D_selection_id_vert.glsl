
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;

#ifndef UNIFORM_ID
uniform uint offset;
in uint color;

flat out vec4 id;
#endif

void main()
{
#ifndef UNIFORM_ID
	id = vec4(
		(((color + offset)      ) & uint(0xFF)) * (1.0f / 255.0f),
		(((color + offset) >>  8) & uint(0xFF)) * (1.0f / 255.0f),
		(((color + offset) >> 16) & uint(0xFF)) * (1.0f / 255.0f),
		(((color + offset) >> 24)             ) * (1.0f / 255.0f));
#endif

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
