
flat in int vertFlag;

#define VERTEX_SELECTED (1 << 0)
#define VERTEX_ACTIVE   (1 << 1)

out vec4 FragColor;

void main()
{
	/* TODO: vertex size */

	if ((vertFlag & VERTEX_SELECTED) != 0) {
		FragColor = colorVertexSelect;
	}
	else if ((vertFlag & VERTEX_ACTIVE) != 0) {
		FragColor = colorEditMeshActive;
	}
	else {
		FragColor = colorVertex;
	}
}
