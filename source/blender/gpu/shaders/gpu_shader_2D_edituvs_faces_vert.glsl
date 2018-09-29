
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 faceColor;
uniform vec4 selectColor;
uniform vec4 activeColor;

in vec2 pos;
in int flag;

flat out vec4 finalColor;

#define FACE_SELECT (1 << 2)
#define FACE_ACTIVE (1 << 3)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	bool is_selected = (flag & FACE_SELECT) != 0;
	bool is_active = (flag & FACE_ACTIVE) != 0;

	finalColor = (is_selected) ? selectColor : faceColor;
	finalColor = (is_active) ? activeColor : finalColor;
}
