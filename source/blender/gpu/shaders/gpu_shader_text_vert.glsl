
uniform mat4 ModelViewProjectionMatrix;

in vec4 pos; /* rect */
in vec4 tex; /* rect */
in vec4 col;

out vec4 pos_rect;
out vec4 tex_rect;
out vec4 color;

void main()
{
	/* This won't work for real 3D ModelViewProjectionMatrix. */
	vec2 v1 = (ModelViewProjectionMatrix * vec4(pos.xy, 0.0, 1.0)).xy;
	vec2 v2 = (ModelViewProjectionMatrix * vec4(pos.zw, 0.0, 1.0)).xy;

	pos_rect.xy = min(v1, v2);
	pos_rect.zw = max(v1, v2);

	tex_rect = tex;
	color = col;
}
