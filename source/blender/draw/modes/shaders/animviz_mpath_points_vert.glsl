
uniform mat4 ViewProjectionMatrix;

uniform int pointSize = 2;
uniform int frameCurrent;
uniform int cacheStart;
uniform bool showKeyFrames = true;
uniform bool useCustomColor;
uniform vec3 customColor;

in vec3 pos;
in int flag;

#define MOTIONPATH_VERT_SEL (1 << 0)
#define MOTIONPATH_VERT_KEY (1 << 1)

out vec4 finalColor;

void main()
{
	gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = float(pointSize + 2);

	int frame = gl_VertexID + cacheStart;
	finalColor = (useCustomColor) ? vec4(customColor, 1.0) : vec4(1.0);

	/* Bias to reduce z fighting with the path */
	gl_Position.z -= 1e-4;

	if (showKeyFrames) {
		if ((flag & MOTIONPATH_VERT_KEY) != 0) {
			gl_PointSize = float(pointSize + 5);
			finalColor = colorVertexSelect;
			/* Bias more to get these on top of regular points */
			gl_Position.z -= 1e-4;
		}
		/* Draw big green dot where the current frame is.
		 * NOTE: this is only done when keyframes are shown, since this adds similar types of clutter
		 */
		if (frame == frameCurrent) {
			gl_PointSize = float(pointSize + 8);
			finalColor = colorCurrentFrame;
			/* Bias more to get these on top of keyframes */
			gl_Position.z -= 1e-4;
		}
	}
}
