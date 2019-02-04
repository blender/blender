
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 vertColor;
uniform vec4 selectColor;

in vec2 pos;
in int flag;

out vec4 finalColor;

/* TODO: Port drawing to draw manager and
 * remove constants duplications. */
#define FACE_UV_SELECT  (1 << 7)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	finalColor = ((flag & FACE_UV_SELECT) != 0) ? selectColor : vertColor;
}
