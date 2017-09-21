
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec4 norAndFlag;

flat out int isSelected;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	/* Bias Facedot Z position in clipspace. */
	gl_Position.z -= 0.0002;
	gl_PointSize = sizeFaceDot;
	isSelected = int(norAndFlag.w);
}
