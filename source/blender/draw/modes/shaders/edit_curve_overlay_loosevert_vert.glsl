
/* Draw Curve Vertices */

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewportSize;

in vec3 pos;
in int data;

out vec4 finalColor;

void main()
{
	if ((data & VERT_SELECTED) != 0) {
		if ((data & VERT_ACTIVE) != 0) {
			finalColor = colorEditMeshActive;
		}
		else {
			finalColor = colorVertexSelect;
		}
	}
	else {
		finalColor = colorVertex;
	}

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = sizeVertex * 2.0;
}
