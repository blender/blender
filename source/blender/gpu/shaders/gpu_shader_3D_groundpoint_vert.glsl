
/* Made to be used with dynamic batching so no Model Matrix needed */
uniform mat4 ViewProjectionMatrix;

in vec3 pos;

void main()
{
	gl_Position = ViewProjectionMatrix * vec4(pos.xy, 0.0, 1.0);
	gl_PointSize = 2.0;
}
