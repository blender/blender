
uniform mat4 ModelViewProjectionMatrix;

in vec2 pos;
in vec2 uvs;
out vec2 texCoord_interp;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	texCoord_interp = uvs;
}
