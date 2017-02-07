
uniform mat4 ViewProjectionMatrix;

in vec3 pos;
in mat4 InstanceModelMatrix;

void main()
{
	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos, 1.0);
}
