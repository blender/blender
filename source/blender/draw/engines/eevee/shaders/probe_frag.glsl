
in vec3 worldPosition;

uniform sampler2D probeLatLong;

out vec4 FragColor;

float hypot(float x, float y) { return sqrt(x*x + y*y); }

void node_tex_environment_equirectangular(vec3 co, sampler2D ima, out vec4 color)
{
	vec3 nco = normalize(co);
	float u = -atan(nco.y, nco.x) / (2.0 * 3.1415) + 0.5;
	float v = atan(nco.z, hypot(nco.x, nco.y)) / 3.1415 + 0.5;

	color = texture2D(ima, vec2(u, v));
}

void main() {
	vec3 L = normalize(worldPosition);
	vec2 uvs = gl_FragCoord.xy / 256.0;
	float dist = dot(L, vec3(0.0,1.0,0.0));
	dist = (dist > 0.99) ? 1e1 : 0.0;
	FragColor = vec4(dist,dist,dist, 1.0);
	node_tex_environment_equirectangular(L, probeLatLong, FragColor);
}