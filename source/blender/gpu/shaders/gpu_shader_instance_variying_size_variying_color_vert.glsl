
uniform mat4 ViewProjectionMatrix;

in vec3 pos;
/* Instance attrib */
in mat4 InstanceModelMatrix;
in vec3 color;
in float size;

flat out vec4 finalColor;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos * size, 1.0);
	finalColor = vec4(color, 1.0);
}
