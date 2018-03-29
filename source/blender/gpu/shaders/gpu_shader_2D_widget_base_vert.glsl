/* 2 bits for corner */
/* Attention! Not the same order as in UI_interface.h!
 * Ordered by drawing order. */
#define BOTTOM_LEFT 0u
#define BOTTOM_RIGHT 1u
#define TOP_RIGHT 2u
#define TOP_LEFT 3u
#define CNR_FLAG_RANGE ((1u << 2u) - 1u)

/* 4bits for corner id */
#define CORNER_VEC_OFS 2u
#define CORNER_VEC_RANGE ((1u << 4u) - 1u)
const vec2 cornervec[36] = vec2[36](
	vec2(0.0, 1.0), vec2(0.02, 0.805), vec2(0.067, 0.617), vec2(0.169, 0.45), vec2(0.293, 0.293), vec2(0.45, 0.169), vec2(0.617, 0.076), vec2(0.805, 0.02), vec2(1.0, 0.0),
	vec2(-1.0, 0.0), vec2(-0.805, 0.02), vec2(-0.617, 0.067), vec2(-0.45, 0.169), vec2(-0.293, 0.293), vec2(-0.169, 0.45), vec2(-0.076, 0.617), vec2(-0.02, 0.805), vec2(0.0, 1.0),
	vec2(0.0, -1.0), vec2(-0.02, -0.805), vec2(-0.067, -0.617), vec2(-0.169, -0.45), vec2(-0.293, -0.293), vec2(-0.45, -0.169), vec2(-0.617, -0.076), vec2(-0.805, -0.02), vec2(-1.0, 0.0),
	vec2(1.0, 0.0), vec2(0.805, -0.02), vec2(0.617, -0.067), vec2(0.45, -0.169), vec2(0.293, -0.293), vec2(0.169, -0.45), vec2(0.076, -0.617), vec2(0.02, -0.805), vec2(0.0, -1.0)
);

/* 4bits for jitter id */
#define JIT_OFS 6u
#define JIT_RANGE ((1u << 4u) - 1u)
const vec2 jit[9] = vec2[9](
	vec2( 0.468813, -0.481430), vec2(-0.155755, -0.352820),
	vec2( 0.219306, -0.238501), vec2(-0.393286, -0.110949),
	vec2(-0.024699,  0.013908), vec2( 0.343805,  0.147431),
	vec2(-0.272855,  0.269918), vec2( 0.095909,  0.388710),
	vec2( 0.0,  0.0)
);

/* 2bits for other flag */
#define INNER_FLAG (1u << 10u) /* is inner vert */
#define EMBOSS_FLAG (1u << 11u) /* is emboss vert */

uniform mat4 ModelViewProjectionMatrix;

uniform vec4 parameters[7];
/* radi and rad per corner */
#define recti parameters[0]
#define rect parameters[1]
#define radsi parameters[2].x
#define rads parameters[2].y
#define faci parameters[2].zw
#define roundCorners parameters[3]
#define color1 parameters[4]
#define color2 parameters[5]
#define shadeDir parameters[6].x

in uint vflag;

noperspective out vec4 finalColor;

void main()
{
	uint cflag = vflag & CNR_FLAG_RANGE;
	uint vofs = (vflag >> CORNER_VEC_OFS) & CORNER_VEC_RANGE;

	vec2 v = cornervec[cflag * 9u + vofs];

	bool is_inner = (vflag & INNER_FLAG) != 0.0;

	/* Scale by corner radius */
	v *= roundCorners[cflag] * ((is_inner) ? radsi : rads);

	/* Position to corner */
	vec4 rct = (is_inner) ? recti : rect;
	if (cflag == BOTTOM_LEFT)
		v += rct.xz;
	else if (cflag == BOTTOM_RIGHT)
		v += rct.yz;
	else if (cflag == TOP_RIGHT)
		v += rct.yw;
	else /* (cflag == TOP_LEFT) */
		v += rct.xw;

	/* compute uv and color gradient */
	vec2 uv = faci * (v - recti.xz);
	float fac = clamp((shadeDir > 0.0) ? uv.y : uv.x, 0.0, 1.0);
	finalColor = mix(color2, color1, fac);

	bool is_emboss = (vflag & EMBOSS_FLAG) != 0.0;
	v.y -= (is_emboss) ? 1.0f : 0.0;

	/* Antialiasing offset */
	v += jit[(vflag >> JIT_OFS) & JIT_RANGE];

	gl_Position = ModelViewProjectionMatrix * vec4(v, 0.0, 1.0);
}