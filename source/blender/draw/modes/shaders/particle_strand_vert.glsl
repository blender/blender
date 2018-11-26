
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in float color;

out vec4 finalColor;
#ifdef USE_POINTS
out vec2 radii;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	finalColor = mix(colorWire, colorEdgeSelect, color);

#ifdef USE_POINTS
	gl_PointSize = sizeVertex;

	/* calculate concentric radii in pixels */
	float radius = 0.5 * sizeVertex;

	/* start at the outside and progress toward the center */
	radii[0] = radius;
	radii[1] = radius - 1.0;

	/* convert to PointCoord units */
	radii /= sizeVertex;
#endif
}
