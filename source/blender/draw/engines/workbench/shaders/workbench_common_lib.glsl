/* 4x4 bayer matrix prepared for 8bit UNORM precision error. */
#define P(x) (((x + 0.5) * (1.0 / 16.0) - 0.5) * (1.0 / 255.0))
const vec4 dither_mat4x4[4] = vec4[4](
	vec4( P(0.0),  P(8.0),  P(2.0), P(10.0)),
	vec4(P(12.0),  P(4.0), P(14.0),  P(6.0)),
	vec4( P(3.0), P(11.0),  P(1.0),  P(9.0)),
	vec4(P(15.0),  P(7.0), P(13.0),  P(5.0))
);

float bayer_dither_noise() {
	ivec2 tx1 = ivec2(gl_FragCoord.xy) % 4;
	ivec2 tx2 = ivec2(gl_FragCoord.xy) % 2;
	return dither_mat4x4[tx1.x][tx1.y];
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec3 normal_decode(vec2 enc)
{
	vec2 fenc = enc.xy * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(1.0 - f / 4.0);
	vec3 n;
	n.xy = fenc*g;
	n.z = 1 - f / 2;
	return n;
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec2 normal_encode(vec3 n)
{
	float p = sqrt(n.z * 8.0 + 8.0);
	return vec2(n.xy / p + 0.5);
}
