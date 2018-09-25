
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attribs ---- */
in vec3 pos;

/* ---- Per instance Attribs ---- */
in vec3 color;
in float start;
in float end;
in mat4 InstanceModelMatrix;

uniform float size;

flat out vec4 finalColor;

void main()
{
	float len = end - start;
	vec3 sta = vec3(0.0, 0.0, -start);

	gl_Position = ViewProjectionMatrix * InstanceModelMatrix * vec4(pos * -len + sta, 1.0);
	gl_PointSize = size;
	finalColor = vec4(color, 1.0);
}
