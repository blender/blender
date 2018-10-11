
uniform mat4 ModelViewProjectionMatrix;
uniform ivec4 dataMask = ivec4(0xFF);

in vec3 pos;
in ivec4 data;

flat out vec4 faceColor;

#define FACE_ACTIVE     (1 << 3)
#define FACE_SELECTED   (1 << 4)
#define FACE_FREESTYLE  (1 << 5)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	ivec4 data_m = data & dataMask;

	if ((data_m.x & FACE_ACTIVE) != 0)
		faceColor = colorFaceSelect;
	else if ((data_m.x & FACE_SELECTED) != 0)
		faceColor = colorFaceSelect;
	else if ((data_m.x & FACE_FREESTYLE) != 0)
		faceColor = colorFaceFreestyle;
	else
		faceColor = colorFace;
}
