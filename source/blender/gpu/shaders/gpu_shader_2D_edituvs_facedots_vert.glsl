
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 vertColor;
uniform vec4 selectColor;

in vec2 pos;
in int flag;

out vec4 finalColor;

#define FACE_SELECT (1 << 2)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	finalColor = ((flag & FACE_SELECT) != 0) ? selectColor : vertColor;
}