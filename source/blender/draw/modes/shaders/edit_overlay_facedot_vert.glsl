
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec4 norAndFlag;

flat out int isSelected;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = sizeFaceDot;
	isSelected = int(norAndFlag.w);
}
