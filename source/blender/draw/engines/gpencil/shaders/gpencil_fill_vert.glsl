uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec4 color;
in vec2 texCoord;
out vec4 finalColor;
out vec2 texCoord_interp;

void main(void)
{
	gl_Position = ModelViewProjectionMatrix * vec4( pos, 1.0 );
	finalColor = color;
	texCoord_interp = texCoord;
}
