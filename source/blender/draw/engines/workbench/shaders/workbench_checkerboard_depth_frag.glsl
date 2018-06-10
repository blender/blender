
/* 4x4 bayer matrix. */
#define P(x) ((x + 0.5) * (1.0 / 16.0))
const vec4 dither_mat[4] = vec4[4](
	vec4( P(0.0),  P(8.0),  P(2.0), P(10.0)),
	vec4(P(12.0),  P(4.0), P(14.0),  P(6.0)),
	vec4( P(3.0), P(11.0),  P(1.0),  P(9.0)),
	vec4(P(15.0),  P(7.0), P(13.0),  P(5.0))
);

uniform float threshold = 0.5;

/* Noise dithering pattern
 * 0 - Bayer matrix
 * 1 - Interlieved gradient noise
 */
#define NOISE 1

void main()
{
#if NOISE == 0
	ivec2 tx = ivec2(gl_FragCoord.xy) % 4;
	float noise = dither_mat[tx.x][tx.y];
#elif NOISE == 1
	/* Interlieved gradient noise by Jorge Jimenez
	 * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
	float noise = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
#else
#error
#endif

	if (noise > threshold) {
		discard;
	} else {
		gl_FragDepth = 1.0;
	}
}
