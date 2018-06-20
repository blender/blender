
flat in int isSelected;
#ifdef VERTEX_FACING
flat in float facing;
#endif

out vec4 FragColor;

void main()
{
	if (isSelected != 0)
		FragColor = colorFaceDot;
	else
		FragColor = colorVertex;

#ifdef VERTEX_FACING
	FragColor.a *= 1.0 - abs(facing) * 0.4;
#endif
}
