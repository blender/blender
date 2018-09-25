
/* Draw Curve Vertices */

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewportSize;

in vec3 pos;
in int data;

out vec4 finalColor;

#define VERTEX_ACTIVE   (1 << 0)
#define VERTEX_SELECTED (1 << 1)

void main()
{
	if ((data & VERTEX_ACTIVE) != 0) {
		finalColor = colorEditMeshActive;
	}
	if ((data & VERTEX_SELECTED) != 0) {
		finalColor = colorVertexSelect;
	}
	else {
		finalColor = colorVertex;
	}

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = sizeVertex * 2.0;
}
