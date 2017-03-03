
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

/* This shader follows the principles of
 * http://developer.download.nvidia.com/SDK/10/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf */

flat in vec4 faceColor;
flat in int faceActive;

out vec4 FragColor;

const mat4 stipple_matrix = mat4(vec4(1.0, 0.0, 0.0, 0.0),
                                 vec4(0.0, 0.0, 0.0, 0.0),
                                 vec4(0.0, 0.0, 1.0, 0.0),
                                 vec4(0.0, 0.0, 0.0, 0.0));

void main()
{
	FragColor = faceColor;

	if (faceActive == 1) {
		int x = int(gl_FragCoord.x) & 0x3; /* mod 4 */
		int y = int(gl_FragCoord.y) & 0x3; /* mod 4 */
		FragColor *= stipple_matrix[x][y];
	}
}
