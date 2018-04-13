uniform int mat_id;

#ifdef USE_FLAT_NORMAL
flat in vec3 normal;
#else
in vec3 normal;
#endif

layout(location = 0) out vec2 outNormals;
layout(location = 1) out int outIndex;

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec2 normal_encode(vec3 n)
{
	float p = sqrt(n.z * 8.0 + 8.0);
	return n.xy / p + 0.5;
}

/* 4x4 bayer matrix prepared for 8bit UNORM precision error. */
#define P(x) (((x + 0.5) * (1.0 / 16.0) - 0.5) * (1.0 / 255.0))
const vec4 dither_mat[4] = vec4[4](
	vec4( P(0.0),  P(8.0),  P(2.0), P(10.0)),
	vec4(P(12.0),  P(4.0), P(14.0),  P(6.0)),
	vec4( P(3.0), P(11.0),  P(1.0),  P(9.0)),
	vec4(P(15.0),  P(7.0), P(13.0),  P(5.0))
);

void main() {
	outIndex = (mat_id + 1); /* 0 is clear color */
	/**
	 * To fix the normal buffer precision issue for backfaces,
	 * we invert normals and use the sign of the index buffer
	 * to tag them, and re-invert in deferred pass.
	 **/
	vec3 N = (gl_FrontFacing) ? normal : -normal;
	outIndex = (gl_FrontFacing) ? outIndex : -outIndex;

	outNormals = normal_encode(N);

	/* Dither the output to fight low quality. */
	ivec2 tx = ivec2(gl_FragCoord.xy) % 4;
	outNormals += dither_mat[tx.x][tx.y];
}
