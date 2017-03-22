
flat in int isSelected;

out vec4 FragColor;

void main()
{
	if (isSelected != 0)
		FragColor = colorFaceDot;
	else
		FragColor = colorWireEdit;
}
