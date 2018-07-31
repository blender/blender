uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec2 texCoord;
out vec2 texCoord_interp;

void main(void)
{
	gl_Position = ModelViewProjectionMatrix * vec4( pos, 1.0 );
	texCoord_interp = texCoord;
}
