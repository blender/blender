
/* Simpler version of gpu_shader_text_vert that supports only 2D translation. */

uniform mat4 ModelViewProjectionMatrix;

in vec4 pos; /* rect */
in vec4 tex; /* rect */
in vec4 col;

out vec4 pos_rect;
out vec4 tex_rect;
out vec4 color;

void main()
{
	/* Manual mat4*vec2 */
	pos_rect  = ModelViewProjectionMatrix[0].xyxy * pos.xxzz;
	pos_rect += ModelViewProjectionMatrix[1].xyxy * pos.yyww;
	pos_rect += ModelViewProjectionMatrix[3].xyxy;
	tex_rect = tex;
	color = col;
}
