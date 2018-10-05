
uniform mat4 ModelViewProjectionMatrix;
uniform vec4 vertColor;
uniform vec4 selectColor;
uniform vec4 pinnedColor;
uniform float pointSize;
uniform float outlineWidth;

in vec2 pos;
in int flag;

out vec4 fillColor;
out vec4 outlineColor;
out vec4 radii;

#define VERTEX_SELECT (1 << 0)
#define VERTEX_PINNED (1 << 1)

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	gl_PointSize = pointSize;

	bool is_selected = (flag & VERTEX_SELECT) != 0;
	bool is_pinned = (flag & VERTEX_PINNED) != 0;

	vec4 deselect_col = (is_pinned) ? pinnedColor : vertColor;
	fillColor = (is_selected) ? selectColor : deselect_col;
	outlineColor = (is_pinned) ? pinnedColor : vec4(fillColor.rgb, 0.0);

	// calculate concentric radii in pixels
	float radius = 0.5 * pointSize;

	// start at the outside and progress toward the center
	radii[0] = radius;
	radii[1] = radius - 1.0;
	radii[2] = radius - outlineWidth;
	radii[3] = radius - outlineWidth - 1.0;

	// convert to PointCoord units
	radii /= pointSize;
}
