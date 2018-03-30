
uniform mat4 ModelViewProjectionMatrix;

in vec4 pos; /* rect */
in vec4 tex; /* rect */
in vec4 col;

out vec4 pos_rect;
out vec4 tex_rect;
out vec4 color;

void main()
{
	pos_rect = pos;
	tex_rect = tex;
	color = col;
}
