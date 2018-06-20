
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in ivec4 data;

flat out vec4 faceColor;

#define FACE_SELECTED (1 << 3)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	faceColor = ((data.x & FACE_SELECTED) != 0)? colorFaceSelect: colorFace;
}
