
uniform mat4 ProjectionMatrix;
uniform mat4 ViewMatrixInverse;

uniform sampler2DArray probeCubes;
uniform float lodMax;

flat in int pid;
in vec3 worldNormal;
in vec3 worldPosition;

out vec4 FragColor;

#define cameraForward   normalize(ViewMatrixInverse[2].xyz)
#define cameraPos       ViewMatrixInverse[3].xyz

void main()
{
	vec3 V = (ProjectionMatrix[3][3] == 0.0) /* if perspective */
	            ? normalize(cameraPos - worldPosition)
	            : cameraForward;
	vec3 N = normalize(worldNormal);
	FragColor = vec4(textureLod_octahedron(probeCubes, vec4(reflect(-V, N), pid), 0.0, lodMax).rgb, 1.0);
}
