
flat in int vertFlag;

out vec4 FragColor;

void main()
{
	/* TODO: vertex size */

	if ((vertFlag & VERT_SELECTED) != 0) {
		FragColor = colorVertexSelect;
	}
	else if ((vertFlag & VERT_ACTIVE) != 0) {
		FragColor = colorEditMeshActive;
	}
	else {
		FragColor = colorVertex;
	}
}
