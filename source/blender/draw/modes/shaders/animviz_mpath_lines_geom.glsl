
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 ProjectionMatrix;
uniform vec2 viewportSize;
uniform int lineThickness = 2;

in vec4 finalColor_geom[];
in vec2 ssPos[];

out vec4 finalColor;

vec2 compute_dir(vec2 v0, vec2 v1)
{
	vec2 dir = normalize(v1 - v0 + 1e-8);
	dir = vec2(-dir.y, dir.x);
	return dir;
}

void main(void)
{
	vec2 t;
	vec2 edge_dir = compute_dir(ssPos[0], ssPos[1]) / viewportSize;

	bool is_persp = (ProjectionMatrix[3][3] == 0.0);

	finalColor = finalColor_geom[0];
	t = edge_dir * (float(lineThickness) * (is_persp ? gl_in[0].gl_Position.w : 1.0));
	gl_Position = gl_in[0].gl_Position + vec4(t, 0.0, 0.0); EmitVertex();
	gl_Position = gl_in[0].gl_Position - vec4(t, 0.0, 0.0); EmitVertex();

	finalColor = finalColor_geom[1];
	t = edge_dir * (float(lineThickness) * (is_persp ? gl_in[1].gl_Position.w : 1.0));
	gl_Position = gl_in[1].gl_Position + vec4(t, 0.0, 0.0); EmitVertex();
	gl_Position = gl_in[1].gl_Position - vec4(t, 0.0, 0.0); EmitVertex();
	EndPrimitive();
}
