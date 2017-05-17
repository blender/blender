
uniform mat4 ModelViewProjectionMatrix;

in vec2 texCoord;
in vec3 pos;
out vec2 texCoord_interp;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos.xyz, 1.0f);
	texCoord_interp = texCoord;
}
