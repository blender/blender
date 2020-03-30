
/* Must match C declaration. */
struct gpMaterial {
  vec4 stroke_color;
  vec4 fill_color;
  vec4 fill_mix_color;
  vec4 fill_uv_rot_scale;
  vec4 fill_uv_offset;
  /* Put float/int at the end to avoid padding error */
  float stroke_texture_mix;
  float stroke_u_scale;
  float fill_texture_mix;
  int flag;
  /* Please ensure 16 byte alignment (multiple of vec4). */
};

/* flag */
#define GP_STROKE_ALIGNMENT_STROKE 1
#define GP_STROKE_ALIGNMENT_OBJECT 2
#define GP_STROKE_ALIGNMENT_FIXED 3
#define GP_STROKE_ALIGNMENT 0x3
#define GP_STROKE_OVERLAP (1 << 2)
#define GP_STROKE_TEXTURE_USE (1 << 3)
#define GP_STROKE_TEXTURE_STENCIL (1 << 4)
#define GP_STROKE_TEXTURE_PREMUL (1 << 5)
#define GP_STROKE_DOTS (1 << 6)
#define GP_FILL_TEXTURE_USE (1 << 10)
#define GP_FILL_TEXTURE_PREMUL (1 << 11)
#define GP_FILL_TEXTURE_CLIP (1 << 12)
#define GP_FILL_GRADIENT_USE (1 << 13)
#define GP_FILL_GRADIENT_RADIAL (1 << 14)
/* High bits are used to pass material ID to fragment shader. */
#define GP_MATID_SHIFT 16

/* Multiline defines can crash blender with certain GPU drivers. */
/* clang-format off */
#define GP_FILL_FLAGS (GP_FILL_TEXTURE_USE | GP_FILL_TEXTURE_PREMUL | GP_FILL_TEXTURE_CLIP | GP_FILL_GRADIENT_USE | GP_FILL_GRADIENT_RADIAL)
/* clang-format on */

#define GP_FLAG_TEST(flag, val) (((flag) & (val)) != 0)

/* Must match C declaration. */
struct gpLight {
  vec4 color_type;
  vec4 right;
  vec4 up;
  vec4 forward;
  vec4 position;
  /* Please ensure 16 byte alignment (multiple of vec4). */
};

#define spot_size right.w
#define spot_blend up.w

#define GP_LIGHT_TYPE_POINT 0.0
#define GP_LIGHT_TYPE_SPOT 1.0
#define GP_LIGHT_TYPE_SUN 2.0
#define GP_LIGHT_TYPE_AMBIENT 3.0

#ifdef GP_MATERIAL_BUFFER_LEN

layout(std140) uniform gpMaterialBlock
{
  gpMaterial materials[GP_MATERIAL_BUFFER_LEN];
};

#endif

#ifdef GPENCIL_LIGHT_BUFFER_LEN

layout(std140) uniform gpLightBlock
{
  gpLight lights[GPENCIL_LIGHT_BUFFER_LEN];
};

#endif

/* Must match eGPLayerBlendModes */
#define MODE_REGULAR 0
#define MODE_HARDLIGHT 1
#define MODE_ADD 2
#define MODE_SUB 3
#define MODE_MULTIPLY 4
#define MODE_DIVIDE 5
#define MODE_HARDLIGHT_SECOND_PASS 999

void blend_mode_output(
    int blend_mode, vec4 color, float opacity, out vec4 frag_color, out vec4 frag_revealage)
{
  switch (blend_mode) {
    case MODE_REGULAR:
      /* Reminder: Blending func is premult alpha blend (dst.rgba * (1 - src.a) + src.rgb).*/
      color *= opacity;
      frag_color = color;
      frag_revealage = vec4(0.0, 0.0, 0.0, color.a);
      break;
    case MODE_MULTIPLY:
      /* Reminder: Blending func is multiply blend (dst.rgba * src.rgba).*/
      color.a *= opacity;
      frag_revealage = frag_color = (1.0 - color.a) + color.a * color;
      break;
    case MODE_DIVIDE:
      /* Reminder: Blending func is multiply blend (dst.rgba * src.rgba).*/
      color.a *= opacity;
      frag_revealage = frag_color = clamp(1.0 / max(vec4(1e-6), 1.0 - color * color.a), 0.0, 1e18);
      break;
    case MODE_HARDLIGHT:
      /* Reminder: Blending func is multiply blend (dst.rgba * src.rgba).*/
      /**
       * We need to separate the overlay equation into 2 term (one mul and one add).
       * This is the standard overlay equation (per channel):
       * rtn = (src < 0.5) ? (2.0 * src * dst) : (1.0 - 2.0 * (1.0 - src) * (1.0 - dst));
       * We rewrite the second branch like this:
       * rtn = 1 - 2 * (1 - src) * (1 - dst);
       * rtn = 1 - 2 (1 - dst + src * dst - src);
       * rtn = 1 - 2 (1 - dst * (1 - src) - src);
       * rtn = 1 - 2 + dst * (2 - 2 * src) + 2 * src;
       * rtn = (- 1 + 2 * src) + dst * (2 - 2 * src);
       **/
      color = mix(vec4(0.5), color, color.a * opacity);
      vec4 s = step(-0.5, -color);
      frag_revealage = frag_color = 2.0 * s + 2.0 * color * (1.0 - s * 2.0);
      frag_revealage = max(vec4(0.0), frag_revealage);
      break;
    case MODE_HARDLIGHT_SECOND_PASS:
      /* Reminder: Blending func is additive blend (dst.rgba + src.rgba).*/
      color = mix(vec4(0.5), color, color.a * opacity);
      frag_revealage = frag_color = (-1.0 + 2.0 * color) * step(-0.5, -color);
      frag_revealage = max(vec4(0.0), frag_revealage);
      break;
    case MODE_SUB:
    case MODE_ADD:
      /* Reminder: Blending func is additive / subtractive blend (dst.rgba +/- src.rgba).*/
      frag_color = color * color.a * opacity;
      frag_revealage = vec4(0.0);
      break;
  }
}

#ifdef GPU_VERTEX_SHADER
#  define IN_OUT out
#else
#  define IN_OUT in
#endif

/* Shader interface. */
IN_OUT vec4 finalColorMul;
IN_OUT vec4 finalColorAdd;
IN_OUT vec3 finalPos;
IN_OUT vec2 finalUvs;
noperspective IN_OUT float strokeThickness;
noperspective IN_OUT float strokeHardeness;
flat IN_OUT vec2 strokeAspect;
flat IN_OUT vec2 strokePt1;
flat IN_OUT vec2 strokePt2;
flat IN_OUT int matFlag;
flat IN_OUT float depth;

#ifdef GPU_FRAGMENT_SHADER

#  define linearstep(p0, p1, v) (clamp(((v) - (p0)) / abs((p1) - (p0)), 0.0, 1.0))

float stroke_round_cap_mask(vec2 p1, vec2 p2, vec2 aspect, float thickness, float hardfac)
{
  /* We create our own uv space to avoid issues with triangulation and linear
   * interpolation artifacts. */
  vec2 line = p2.xy - p1.xy;
  vec2 pos = gl_FragCoord.xy - p1.xy;
  float line_len = length(line);
  float half_line_len = line_len * 0.5;
  /* Normalize */
  line = (line_len > 0.0) ? (line / line_len) : vec2(1.0, 0.0);
  /* Create a uv space that englobe the whole segment into a capsule. */
  vec2 uv_end;
  uv_end.x = max(abs(dot(line, pos) - half_line_len) - half_line_len, 0.0);
  uv_end.y = dot(vec2(-line.y, line.x), pos);
  /* Divide by stroke radius. */
  uv_end /= thickness;
  uv_end *= aspect;

  float dist = clamp(1.0 - length(uv_end) * 2.0, 0.0, 1.0);
  if (hardfac > 0.999) {
    return step(1e-8, dist);
  }
  else {
    /* Modulate the falloff profile */
    float hardness = 1.0 - hardfac;
    dist = pow(dist, mix(0.01, 10.0, hardness));
    return smoothstep(0.0, 1.0, dist);
  }
}

#endif

uniform vec2 sizeViewport;
uniform vec2 sizeViewportInv;

/* Per Object */
uniform bool strokeOrder3d;
uniform int gpMaterialOffset;
uniform float thicknessScale;
uniform float thicknessWorldScale;
#define thicknessIsScreenSpace (thicknessWorldScale < 0.0)
#define MATERIAL(m) materials[m + gpMaterialOffset]

#ifdef GPU_VERTEX_SHADER

/* Per Layer */
uniform float thicknessOffset;
uniform float vertexColorOpacity;
uniform vec4 layerTint;
uniform float layerOpacity; /* Used for onion skin. */
uniform float strokeIndexOffset = 0.0;

/* All of these attribs are quad loaded the same way
 * as GL_LINES_ADJACENCY would feed a geometry shader:
 * - ma reference the previous adjacency point.
 * - ma1 reference the current line first point.
 * - ma2 reference the current line second point.
 * - ma3 reference the next adjacency point.
 * Note that we are rendering quad instances and not using any index buffer (except for fills).
 */
/* x is material index, y is stroke_id, z is point_id, w is aspect & rotation & hardness packed. */
in ivec4 ma;
in ivec4 ma1;
in ivec4 ma2;
in ivec4 ma3;
/* Position contains thickness in 4th component. */
in vec4 pos;  /* Prev adj vert */
in vec4 pos1; /* Current edge */
in vec4 pos2; /* Current edge */
in vec4 pos3; /* Next adj vert */
/* xy is UV for fills, z is U of stroke, w is strength.  */
in vec4 uv1;
in vec4 uv2;
in vec4 col1;
in vec4 col2;
in vec4 fcol1;
/* WARNING: Max attrib count is actually 14 because OSX OpenGL implementation
 * considers gl_VertexID and gl_InstanceID as vertex attribute. (see T74536) */
#  define stroke_id1 ma1.y
#  define point_id1 ma1.z
#  define thickness1 pos1.w
#  define thickness2 pos2.w
#  define strength1 uv1.w
#  define strength2 uv2.w
/* Packed! need to be decoded. */
#  define hardness1 ma1.w
#  define hardness2 ma2.w
#  define uvrot1 ma1.w
#  define aspect1 ma1.w

vec2 decode_aspect(int packed_data)
{
  float asp = float(uint(packed_data) & 0x1FFu) * (1.0 / 255.0);
  return (asp > 1.0) ? vec2(1.0, (asp - 1.0)) : vec2(asp, 1.0);
}

float decode_uvrot(int packed_data)
{
  uint udata = uint(packed_data);
  float uvrot = 1e-8 + float((udata & 0x1FE00u) >> 9u) * (1.0 / 255.0);
  return ((udata & 0x20000u) != 0u) ? -uvrot : uvrot;
}

float decode_hardness(int packed_data)
{
  return float((uint(packed_data) & 0x3FC0000u) >> 18u) * (1.0 / 255.0);
}

void discard_vert()
{
  /* We set the vertex at the camera origin to generate 0 fragments. */
  gl_Position = vec4(0.0, 0.0, -3e36, 0.0);
}

vec2 project_to_screenspace(vec4 v)
{
  return ((v.xy / v.w) * 0.5 + 0.5) * sizeViewport;
}

vec2 rotate_90deg(vec2 v)
{
  /* Counter Clock-Wise. */
  return vec2(-v.y, v.x);
}

mat4 model_matrix_get()
{
  return ModelMatrix;
}

vec3 transform_point(mat4 m, vec3 v)
{
  return (m * vec4(v, 1.0)).xyz;
}

vec2 safe_normalize(vec2 v)
{
  float len_sqr = dot(v, v);
  if (len_sqr > 0.0) {
    return v / sqrt(len_sqr);
  }
  else {
    return vec2(1.0, 0.0);
  }
}

vec2 safe_normalize_len(vec2 v, out float len)
{
  len = sqrt(dot(v, v));
  if (len > 0.0) {
    return v / len;
  }
  else {
    return vec2(1.0, 0.0);
  }
}

float stroke_thickness_modulate(float thickness)
{
  /* Modify stroke thickness by object and layer factors.-*/
  thickness *= thicknessScale;
  thickness += thicknessOffset;
  thickness = max(1.0, thickness);

  if (thicknessIsScreenSpace) {
    /* Multiply offset by view Z so that offset is constant in screenspace.
     * (e.i: does not change with the distance to camera) */
    thickness *= gl_Position.w;
  }
  else {
    /* World space point size. */
    thickness *= thicknessWorldScale * ProjectionMatrix[1][1] * sizeViewport.y;
  }
  return thickness;
}

#  ifdef GP_MATERIAL_BUFFER_LEN
void color_output(vec4 stroke_col, vec4 vert_col, float vert_strength, float mix_tex)
{
  /* Mix stroke with other colors. */
  vec4 mixed_col = stroke_col;
  mixed_col.rgb = mix(mixed_col.rgb, vert_col.rgb, vert_col.a * vertexColorOpacity);
  mixed_col.rgb = mix(mixed_col.rgb, layerTint.rgb, layerTint.a);
  mixed_col.a *= vert_strength * layerOpacity;
  /**
   * This is what the fragment shader looks like.
   * out = col * finalColorMul + col.a * finalColorAdd.
   * finalColorMul is how much of the texture color to keep.
   * finalColorAdd is how much of the mixed color to add.
   * Note that we never add alpha. This is to keep the texture act as a stencil.
   * We do however, modulate the alpha (reduce it).
   **/
  /* We add the mixed color. This is 100% mix (no texture visible). */
  finalColorMul = vec4(mixed_col.aaa, mixed_col.a);
  finalColorAdd = vec4(mixed_col.rgb * mixed_col.a, 0.0);
  /* Then we blend according to the texture mix factor.
   * Note that we keep the alpha modulation. */
  finalColorMul.rgb *= mix_tex;
  finalColorAdd.rgb *= 1.0 - mix_tex;
}
#  endif

void stroke_vertex()
{
  int m = ma1.x;
  bool is_dot = false;
  bool is_squares = false;

#  ifdef GP_MATERIAL_BUFFER_LEN
  if (m != -1) {
    is_dot = GP_FLAG_TEST(MATERIAL(m).flag, GP_STROKE_ALIGNMENT);
    is_squares = !GP_FLAG_TEST(MATERIAL(m).flag, GP_STROKE_DOTS);
  }
#  endif

  /* Special Case. Stroke with single vert are rendered as dots. Do not discard them. */
  if (!is_dot && ma.x == -1 && ma2.x == -1) {
    is_dot = true;
    is_squares = false;
  }

  /* Enpoints, we discard the vertices. */
  if (ma1.x == -1 || (!is_dot && ma2.x == -1)) {
    discard_vert();
    return;
  }

  mat4 model_mat = model_matrix_get();

  /* Avoid using a vertex attrib for quad positioning. */
  float x = float(gl_VertexID & 1) * 2.0 - 1.0; /* [-1..1] */
  float y = float(gl_VertexID & 2) - 1.0;       /* [-1..1] */

  bool use_curr = is_dot || (x == -1.0);

  vec3 wpos_adj = transform_point(model_mat, (use_curr) ? pos.xyz : pos3.xyz);
  vec3 wpos1 = transform_point(model_mat, pos1.xyz);
  vec3 wpos2 = transform_point(model_mat, pos2.xyz);

  vec4 ndc_adj = point_world_to_ndc(wpos_adj);
  vec4 ndc1 = point_world_to_ndc(wpos1);
  vec4 ndc2 = point_world_to_ndc(wpos2);

  gl_Position = (use_curr) ? ndc1 : ndc2;
  finalPos = (use_curr) ? wpos1 : wpos2;

  vec2 ss_adj = project_to_screenspace(ndc_adj);
  vec2 ss1 = project_to_screenspace(ndc1);
  vec2 ss2 = project_to_screenspace(ndc2);
  /* Screenspace Lines tangents. */
  float line_len;
  vec2 line = safe_normalize_len(ss2 - ss1, line_len);
  vec2 line_adj = safe_normalize((use_curr) ? (ss1 - ss_adj) : (ss_adj - ss2));

  float thickness = abs((use_curr) ? thickness1 : thickness2);
  thickness = stroke_thickness_modulate(thickness);

  finalUvs = vec2(x, y) * 0.5 + 0.5;
  strokeHardeness = decode_hardness(use_curr ? hardness1 : hardness2);

  if (is_dot) {
#  ifdef GP_MATERIAL_BUFFER_LEN
    int alignement = MATERIAL(m).flag & GP_STROKE_ALIGNMENT;
#  endif

    vec2 x_axis;
#  ifdef GP_MATERIAL_BUFFER_LEN
    if (alignement == GP_STROKE_ALIGNMENT_STROKE) {
      x_axis = (ma2.x == -1) ? line_adj : line;
    }
    else if (alignement == GP_STROKE_ALIGNMENT_FIXED) {
      /* Default for no-material drawing. */
      x_axis = vec2(1.0, 0.0);
    }
    else
#  endif
    { /* GP_STROKE_ALIGNMENT_OBJECT */
      vec4 ndc_x = point_world_to_ndc(wpos1 + model_mat[0].xyz);
      vec2 ss_x = project_to_screenspace(ndc_x);
      x_axis = safe_normalize(ss_x - ss1);
    }

    /* Rotation: Encoded as Cos + Sin sign. */
    float uv_rot = decode_uvrot(uvrot1);
    float rot_sin = sqrt(max(0.0, 1.0 - uv_rot * uv_rot)) * sign(uv_rot);
    float rot_cos = abs(uv_rot);
    x_axis = mat2(rot_cos, -rot_sin, rot_sin, rot_cos) * x_axis;

    vec2 y_axis = rotate_90deg(x_axis);

    strokeAspect = decode_aspect(aspect1);

    x *= strokeAspect.x;
    y *= strokeAspect.y;

    /* Invert for vertex shader. */
    strokeAspect = 1.0 / strokeAspect;

    gl_Position.xy += (x * x_axis + y * y_axis) * sizeViewportInv.xy * thickness;

    strokePt1 = ss1;
    strokePt2 = ss1 + x_axis * 0.5;
    strokeThickness = (is_squares) ? 1e18 : (thickness / gl_Position.w);
  }
  else {
    bool is_stroke_start = (ma.x == -1 && x == -1);
    bool is_stroke_end = (ma3.x == -1 && x == 1);

    /* Mitter tangent vector. */
    vec2 miter_tan = safe_normalize(line_adj + line);
    float miter_dot = dot(miter_tan, line_adj);
    /* Break corners after a certain angle to avoid really thick corners. */
    const float miter_limit = 0.5; /* cos(60°) */
    bool miter_break = (miter_dot < miter_limit) || is_stroke_start || is_stroke_end;
    miter_tan = (miter_break) ? line : (miter_tan / miter_dot);

    vec2 miter = rotate_90deg(miter_tan);

    strokePt1.xy = ss1;
    strokePt2.xy = ss2;
    strokeThickness = thickness / gl_Position.w;
    strokeAspect = vec2(1.0);

    vec2 screen_ofs = miter * y;

    /* Reminder: we packed the cap flag into the sign of stength and thickness sign. */
    if ((is_stroke_start && strength1 > 0.0) || (is_stroke_end && thickness1 > 0.0) ||
        miter_break) {
      screen_ofs += line * x;
    }

    gl_Position.xy += screen_ofs * sizeViewportInv.xy * thickness;

    finalUvs.x = (use_curr) ? uv1.z : uv2.z;
#  ifdef GP_MATERIAL_BUFFER_LEN
    finalUvs.x *= MATERIAL(m).stroke_u_scale;
#  endif
  }

#  ifdef GP_MATERIAL_BUFFER_LEN
  vec4 vert_col = (use_curr) ? col1 : col2;
  float vert_strength = abs((use_curr) ? strength1 : strength2);
  vec4 stroke_col = MATERIAL(m).stroke_color;
  float mix_tex = MATERIAL(m).stroke_texture_mix;

  color_output(stroke_col, vert_col, vert_strength, mix_tex);

  matFlag = MATERIAL(m).flag & ~GP_FILL_FLAGS;
#  endif

  if (strokeOrder3d) {
    /* Use the fragment depth (see fragment shader). */
    depth = -1.0;
  }
#  ifdef GP_MATERIAL_BUFFER_LEN
  else if (GP_FLAG_TEST(MATERIAL(m).flag, GP_STROKE_OVERLAP)) {
    /* Use the index of the point as depth.
     * This means the stroke can overlap itself. */
    depth = (point_id1 + strokeIndexOffset + 1.0) * 0.0000002;
  }
#  endif
  else {
    /* Use the index of first point of the stroke as depth.
     * We render using a greater depth test this means the stroke
     * cannot overlap itself.
     * We offset by one so that the fill can be overlapped by its stroke.
     * The offset is ok since we pad the strokes data because of adjacency infos. */
    depth = (stroke_id1 + strokeIndexOffset + 1.0) * 0.0000002;
  }
}

void fill_vertex()
{
  mat4 model_mat = model_matrix_get();

  vec3 wpos = transform_point(model_mat, pos1.xyz);
  gl_Position = point_world_to_ndc(wpos);
  finalPos = wpos;

#  ifdef GP_MATERIAL_BUFFER_LEN
  int m = ma1.x;

  vec4 fill_col = MATERIAL(m).fill_color;
  float mix_tex = MATERIAL(m).fill_texture_mix;

  /* Special case: We don't modulate alpha in gradient mode. */
  if (GP_FLAG_TEST(MATERIAL(m).flag, GP_FILL_GRADIENT_USE)) {
    fill_col.a = 1.0;
  }

  /* Decode fill opacity. */
  vec4 fcol_decode = vec4(fcol1.rgb, floor(fcol1.a / 10.0));
  float fill_opacity = fcol1.a - (fcol_decode.a * 10);
  fcol_decode.a /= 10000.0;

  /* Apply opacity. */
  fill_col.a *= fill_opacity;
  /* If factor is > 1 force opacity. */
  if (fill_opacity > 1.0) {
    fill_col.a += fill_opacity - 1.0;
  }

  fill_col.a = clamp(fill_col.a, 0.0, 1.0);

  color_output(fill_col, fcol_decode, 1.0, mix_tex);

  matFlag = MATERIAL(m).flag & GP_FILL_FLAGS;
  matFlag |= m << GP_MATID_SHIFT;

  vec2 loc = MATERIAL(m).fill_uv_offset.xy;
  mat2x2 rot_scale = mat2x2(MATERIAL(m).fill_uv_rot_scale.xy, MATERIAL(m).fill_uv_rot_scale.zw);
  finalUvs = rot_scale * uv1.xy + loc;
#  endif

  strokeHardeness = 1.0;
  strokeThickness = 1e18;
  strokeAspect = vec2(1.0);
  strokePt1 = strokePt2 = vec2(0.0);

  if (strokeOrder3d) {
    /* Use the fragment depth (see fragment shader). */
    depth = -1.0;
    /* We still offset the fills a little to avoid overlaps */
    gl_Position.z += 0.000002;
  }
  else {
    /* Use the index of first point of the stroke as depth. */
    depth = (stroke_id1 + strokeIndexOffset) * 0.0000002;
  }
}

void gpencil_vertex()
{
  /* Trick to detect if a drawcall is stroke or fill.
   * This does mean that we need to draw an empty stroke segment before starting
   * to draw the real stroke segments. */
  bool is_fill = (gl_InstanceID == 0);

  if (!is_fill) {
    stroke_vertex();
  }
  else {
    fill_vertex();
  }
}

#endif
