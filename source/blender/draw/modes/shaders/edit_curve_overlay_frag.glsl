
flat in int vertFlag;

#define VERTEX_SELECTED	(1 << 0)
#define VERTEX_ACTIVE	(1 << 1)

#if __VERSION__ == 120
  #define FragColor gl_FragColor
#else
  out vec4 FragColor;
#endif

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
