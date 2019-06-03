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

/* 4bits for jitter id */
#define JIT_OFS 6u
#define JIT_RANGE BIT_RANGE(4)
const vec2 jit[9] = vec2[9](vec2(0.468813, -0.481430),
                            vec2(-0.155755, -0.352820),
                            vec2(0.219306, -0.238501),
                            vec2(-0.393286, -0.110949),
                            vec2(-0.024699, 0.013908),
                            vec2(0.343805, 0.147431),
                            vec2(-0.272855, 0.269918),
                            vec2(0.095909, 0.388710),
                            vec2(0.0, 0.0));

/* 2bits for other flags */
#define INNER_FLAG uint(1 << 10)  /* is inner vert */
#define EMBOSS_FLAG uint(1 << 11) /* is emboss vert */

/* 2bits for color */
#define COLOR_OFS 12u
#define COLOR_RANGE BIT_RANGE(2)
#define COLOR_INNER 0u
#define COLOR_EDGE 1u
#define COLOR_EMBOSS 2u

/* 2bits for trias type */
#define TRIA_FLAG uint(1 << 14) /* is tria vert */
#define TRIA_FIRST INNER_FLAG   /* is first tria (reuse INNER_FLAG) */

/* We can reuse the CORNER_* bits for tria */
#define TRIA_VEC_RANGE BIT_RANGE(6)

/* Some GPUs have performanse issues with this array being const (Doesn't fit in the registers?).
 * To resolve this issue, store the array as a uniform buffer.
 * (The array is still stored in the registry, but indexing is done in the uniform buffer.) */
uniform vec2 triavec[43] = vec2[43](

    /* ROUNDBOX_TRIA_ARROWS */
    vec2(-0.170000, 0.400000),
    vec2(-0.050000, 0.520000),
    vec2(0.250000, 0.000000),
    vec2(0.470000, -0.000000),
    vec2(-0.170000, -0.400000),
    vec2(-0.050000, -0.520000),
    vec2(0.170000, 0.400000),
    vec2(0.050000, 0.520000),
    vec2(-0.250000, 0.000000),
    vec2(-0.470000, -0.000000),
    vec2(0.170000, -0.400000),
    vec2(0.050000, -0.520000),

    /* ROUNDBOX_TRIA_SCROLL - circle tria (triangle strip) */
    vec2(0.000000, 1.000000),
    vec2(0.382684, 0.923879),
    vec2(-0.382683, 0.923880),
    vec2(0.707107, 0.707107),
    vec2(-0.707107, 0.707107),
    vec2(0.923879, 0.382684),
    vec2(-0.923879, 0.382684),
    vec2(1.000000, 0.000000),
    vec2(-1.000000, 0.000000),
    vec2(0.923879, -0.382684),
    vec2(-0.923879, -0.382684),
    vec2(0.707107, -0.707107),
    vec2(-0.707107, -0.707107),
    vec2(0.382684, -0.923879),
    vec2(-0.382683, -0.923880),
    vec2(0.000000, -1.000000),

    /* ROUNDBOX_TRIA_MENU - menu arrows */
    vec2(-0.51, 0.07),
    vec2(-0.4, 0.18),
    vec2(-0.05, -0.39),
    vec2(-0.05, -0.17),
    vec2(0.41, 0.07),
    vec2(0.3, 0.18),

    /* ROUNDBOX_TRIA_CHECK - check mark */
    vec2(-0.67000, 0.020000),
    vec2(-0.500000, 0.190000),
    vec2(-0.130000, -0.520000),
    vec2(-0.130000, -0.170000),
    vec2(0.720000, 0.430000),
    vec2(0.530000, 0.590000),

/* ROUNDBOX_TRIA_HOLD_ACTION_ARROW - hold action arrows */
#define OX (-0.32)
#define OY (0.1)
#define SC (0.35 * 2)
    //  vec2(-0.5 + SC, 1.0 + OY),  vec2( 0.5, 1.0 + OY),  vec2( 0.5, 0.0 + OY + SC),
    vec2((0.5 - SC) + OX, 1.0 + OY),
    vec2(-0.5 + OX, 1.0 + OY),
    vec2(-0.5 + OX, SC + OY)
#undef OX
#undef OY
#undef SC
);

uniform mat4 ModelViewProjectionMatrix;

#define MAX_PARAM 11
#ifdef USE_INSTANCE
#  define MAX_INSTANCE 6
uniform vec4 parameters[MAX_PARAM * MAX_INSTANCE];
#else
uniform vec4 parameters[MAX_PARAM];
#endif

/* gl_InstanceID is 0 if not drawing instances. */
#define recti parameters[gl_InstanceID * MAX_PARAM + 0]
#define rect parameters[gl_InstanceID * MAX_PARAM + 1]
#define radsi parameters[gl_InstanceID * MAX_PARAM + 2].x
#define rads parameters[gl_InstanceID * MAX_PARAM + 2].y
#define faci parameters[gl_InstanceID * MAX_PARAM + 2].zw
#define roundCorners parameters[gl_InstanceID * MAX_PARAM + 3]
#define colorInner1 parameters[gl_InstanceID * MAX_PARAM + 4]
#define colorInner2 parameters[gl_InstanceID * MAX_PARAM + 5]
#define colorEdge parameters[gl_InstanceID * MAX_PARAM + 6]
#define colorEmboss parameters[gl_InstanceID * MAX_PARAM + 7]
#define colorTria parameters[gl_InstanceID * MAX_PARAM + 8]
#define tria1Center parameters[gl_InstanceID * MAX_PARAM + 9].xy
#define tria2Center parameters[gl_InstanceID * MAX_PARAM + 9].zw
#define tria1Size parameters[gl_InstanceID * MAX_PARAM + 10].x
#define tria2Size parameters[gl_InstanceID * MAX_PARAM + 10].y
#define shadeDir parameters[gl_InstanceID * MAX_PARAM + 10].z
#define alphaDiscard parameters[gl_InstanceID * MAX_PARAM + 10].w

/* We encode alpha check and discard factor together. */
#define doAlphaCheck (alphaDiscard < 0.0)
#define discardFactor abs(alphaDiscard)

in uint vflag;

noperspective out vec4 finalColor;
noperspective out float butCo;
flat out float discardFac;

vec2 do_widget(void)
{
  uint cflag = vflag & CNR_FLAG_RANGE;
  uint vofs = (vflag >> CORNER_VEC_OFS) & CORNER_VEC_RANGE;

  vec2 v = cornervec[cflag * 9u + vofs];

  bool is_inner = (vflag & INNER_FLAG) != 0u;

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

  vec2 uv = faci * (v - recti.xz);

  /* compute uv and color gradient */
  uint color_id = (vflag >> COLOR_OFS) & COLOR_RANGE;
  if (color_id == COLOR_INNER) {
    float fac = clamp((shadeDir > 0.0) ? uv.y : uv.x, 0.0, 1.0);

    if (doAlphaCheck) {
      finalColor = colorInner1;
      butCo = uv.x;
    }
    else {
      finalColor = mix(colorInner2, colorInner1, fac);
      butCo = -abs(uv.x);
    }
  }
  else if (color_id == COLOR_EDGE) {
    finalColor = colorEdge;
    butCo = -abs(uv.x);
  }
  else /* (color_id == COLOR_EMBOSS) */ {
    finalColor = colorEmboss;
    butCo = -abs(uv.x);
  }

  bool is_emboss = (vflag & EMBOSS_FLAG) != 0u;
  v.y -= (is_emboss) ? 1.0f : 0.0;

  return v;
}

vec2 do_tria()
{
  uint vofs = vflag & TRIA_VEC_RANGE;

  vec2 v = triavec[vofs];

  finalColor = colorTria;
  butCo = -1.0;

  bool is_tria_first = (vflag & TRIA_FIRST) != 0u;

  if (is_tria_first) {
    v = v * tria1Size + tria1Center;
  }
  else {
    v = v * tria2Size + tria2Center;
  }

  return v;
}

void main()
{
  discardFac = discardFactor;
  bool is_tria = (vflag & TRIA_FLAG) != 0u;

  vec2 v = (is_tria) ? do_tria() : do_widget();

  /* Antialiasing offset */
  v += jit[(vflag >> JIT_OFS) & JIT_RANGE];

  gl_Position = ModelViewProjectionMatrix * vec4(v, 0.0, 1.0);
}
