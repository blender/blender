
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 faceColor;
uniform vec4 selectColor;
uniform vec4 activeColor;

in vec2 pos;
in int flag;

flat out vec4 finalColor;

/* TODO: Port drawing to draw manager and
 * remove constants duplications. */
#define FACE_UV_ACTIVE  (1 << 6)
#define FACE_UV_SELECT  (1 << 7)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	bool is_selected = (flag & FACE_UV_SELECT) != 0;
	bool is_active = (flag & FACE_UV_ACTIVE) != 0;

	finalColor = (is_selected) ? selectColor : faceColor;
	finalColor = (is_active) ? activeColor : finalColor;
}
