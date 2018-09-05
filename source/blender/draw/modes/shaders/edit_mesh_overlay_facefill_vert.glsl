
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in ivec4 data;

flat out vec4 faceColor;

#define FACE_ACTIVE     (1 << 2)
#define FACE_SELECTED   (1 << 3)
#define FACE_FREESTYLE  (1 << 4)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	if ((data.x & FACE_ACTIVE) != 0)
		faceColor = colorFaceSelect;
	else if ((data.x & FACE_SELECTED) != 0)
		faceColor = colorFaceSelect;
	else if ((data.x & FACE_FREESTYLE) != 0)
		faceColor = colorFaceFreestyle;
	else
		faceColor = colorFace;
}
