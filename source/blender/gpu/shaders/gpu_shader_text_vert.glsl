
uniform mat4 ModelViewProjectionMatrix;

in vec2 pos;
in vec2 texCoord;
in vec4 color;
flat out vec4 color_flat;
noperspective out vec2 texCoord_interp;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	color_flat = color;
	texCoord_interp = texCoord;
}
