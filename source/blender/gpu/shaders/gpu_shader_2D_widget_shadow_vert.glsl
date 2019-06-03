#define BIT_RANGE(x) uint((1 << x) - 1)

/* 2 bits for corner */
/* Attention! Not the same order as in UI_interface.h!
 * Ordered by drawing order. */
#define BOTTOM_LEFT 0u
#define BOTTOM_RIGHT 1u
#define TOP_RIGHT 2u
#define TOP_LEFT 3u
#define CNR_FLAG_RANGE BIT_RANGE(2)

/* 4bits for corner id */
#define CORNER_VEC_OFS 2u
#define CORNER_VEC_RANGE BIT_RANGE(4)
const vec2 cornervec[36] = vec2[36](vec2(0.0, 1.0),
                                    vec2(0.02, 0.805),
                                    vec2(0.067, 0.617),
                                    vec2(0.169, 0.45),
                                    vec2(0.293, 0.293),
                                    vec2(0.45, 0.169),
                                    vec2(0.617, 0.076),
                                    vec2(0.805, 0.02),
                                    vec2(1.0, 0.0),
                                    vec2(-1.0, 0.0),
                                    vec2(-0.805, 0.02),
                                    vec2(-0.617, 0.067),
                                    vec2(-0.45, 0.169),
                                    vec2(-0.293, 0.293),
                                    vec2(-0.169, 0.45),
                                    vec2(-0.076, 0.617),
                                    vec2(-0.02, 0.805),
                                    vec2(0.0, 1.0),
                                    vec2(0.0, -1.0),
                                    vec2(-0.02, -0.805),
                                    vec2(-0.067, -0.617),
                                    vec2(-0.169, -0.45),
                                    vec2(-0.293, -0.293),
                                    vec2(-0.45, -0.169),
                                    vec2(-0.617, -0.076),
                                    vec2(-0.805, -0.02),
                                    vec2(-1.0, 0.0),
                                    vec2(1.0, 0.0),
                                    vec2(0.805, -0.02),
                                    vec2(0.617, -0.067),
                                    vec2(0.45, -0.169),
                                    vec2(0.293, -0.293),
                                    vec2(0.169, -0.45),
                                    vec2(0.076, -0.617),
                                    vec2(0.02, -0.805),
                                    vec2(0.0, -1.0));

#define INNER_FLAG uint(1 << 10) /* is inner vert */

uniform mat4 ModelViewProjectionMatrix;

uniform vec4 parameters[4];
/* radi and rad per corner */
#define recti parameters[0]
#define rect parameters[1]
#define radsi parameters[2].x
#define rads parameters[2].y
#define roundCorners parameters[3]

in uint vflag;

out float shadowFalloff;

void main()
{
  uint cflag = vflag & CNR_FLAG_RANGE;
  uint vofs = (vflag >> CORNER_VEC_OFS) & CORNER_VEC_RANGE;

  vec2 v = cornervec[cflag * 9u + vofs];

  bool is_inner = (vflag & INNER_FLAG) != 0u;

  shadowFalloff = (is_inner) ? 1.0 : 0.0;

  /* Scale by corner radius */
  v *= roundCorners[cflag] * ((is_inner) ? radsi : rads);

  /* Position to corner */
  vec4 rct = (is_inner) ? recti : rect;
  if (cflag == BOTTOM_LEFT) {
    v += rct.xz;
  }
  else if (cflag == BOTTOM_RIGHT) {
    v += rct.yz;
  }
  else if (cflag == TOP_RIGHT) {
    v += rct.yw;
  }
  else /* (cflag == TOP_LEFT) */ {
    v += rct.xw;
  }

  gl_Position = ModelViewProjectionMatrix * vec4(v, 0.0, 1.0);
}
