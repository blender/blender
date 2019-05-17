/**
 * Library to create hairs dynamically from control points.
 * This is less bandwidth intensive than fetching the vertex attributes
 * but does more ALU work per vertex. This also reduce the number
 * of data the CPU has to precompute and transfert for each update.
 */

/**
 * hairStrandsRes: Number of points per hair strand.
 * 2 - no subdivision
 * 3+ - 1 or more interpolated points per hair.
 */
uniform int hairStrandsRes = 8;

/**
 * hairThicknessRes : Subdiv around the hair.
 * 1 - Wire Hair: Only one pixel thick, independent of view distance.
 * 2 - Polystrip Hair: Correct width, flat if camera is parallel.
 * 3+ - Cylinder Hair: Massive calculation but potentially perfect. Still need proper support.
 */
uniform int hairThicknessRes = 1;

/* Hair thickness shape. */
uniform float hairRadRoot = 0.01;
uniform float hairRadTip = 0.0;
uniform float hairRadShape = 0.5;
uniform bool hairCloseTip = true;

uniform mat4 hairDupliMatrix;

/* -- Per control points -- */
uniform samplerBuffer hairPointBuffer; /* RGBA32F */
#define point_position xyz
#define point_time w /* Position along the hair length */

/* -- Per strands data -- */
uniform usamplerBuffer hairStrandBuffer;    /* R32UI */
uniform usamplerBuffer hairStrandSegBuffer; /* R16UI */

/* Not used, use one buffer per uv layer */
// uniform samplerBuffer hairUVBuffer; /* RG32F */
// uniform samplerBuffer hairColBuffer; /* RGBA16 linear color */

/* -- Subdivision stage -- */
/**
 * We use a transform feedback to preprocess the strands and add more subdivision to it.
 * For the moment theses are simple smooth interpolation but one could hope to see the full
 * children particle modifiers being evaluated at this stage.
 *
 * If no more subdivision is needed, we can skip this step.
 */

#ifdef HAIR_PHASE_SUBDIV
int hair_get_base_id(float local_time, int strand_segments, out float interp_time)
{
  float time_per_strand_seg = 1.0 / float(strand_segments);

  float ratio = local_time / time_per_strand_seg;
  interp_time = fract(ratio);

  return int(ratio);
}

void hair_get_interp_attrs(
    out vec4 data0, out vec4 data1, out vec4 data2, out vec4 data3, out float interp_time)
{
  float local_time = float(gl_VertexID % hairStrandsRes) / float(hairStrandsRes - 1);

  int hair_id = gl_VertexID / hairStrandsRes;
  int strand_offset = int(texelFetch(hairStrandBuffer, hair_id).x);
  int strand_segments = int(texelFetch(hairStrandSegBuffer, hair_id).x);

  int id = hair_get_base_id(local_time, strand_segments, interp_time);

  int ofs_id = id + strand_offset;

  data0 = texelFetch(hairPointBuffer, ofs_id - 1);
  data1 = texelFetch(hairPointBuffer, ofs_id);
  data2 = texelFetch(hairPointBuffer, ofs_id + 1);
  data3 = texelFetch(hairPointBuffer, ofs_id + 2);

  if (id <= 0) {
    /* root points. Need to reconstruct previous data. */
    data0 = data1 * 2.0 - data2;
  }
  if (id + 1 >= strand_segments) {
    /* tip points. Need to reconstruct next data. */
    data3 = data2 * 2.0 - data1;
  }
}
#endif

/* -- Drawing stage -- */
/**
 * For final drawing, the vertex index and the number of vertex per segment
 */

#ifndef HAIR_PHASE_SUBDIV
int hair_get_strand_id(void)
{
  return gl_VertexID / (hairStrandsRes * hairThicknessRes);
}

int hair_get_base_id(void)
{
  return gl_VertexID / hairThicknessRes;
}

/* Copied from cycles. */
float hair_shaperadius(float shape, float root, float tip, float time)
{
  float radius = 1.0 - time;

  if (shape < 0.0) {
    radius = pow(radius, 1.0 + shape);
  }
  else {
    radius = pow(radius, 1.0 / (1.0 - shape));
  }

  if (hairCloseTip && (time > 0.99)) {
    return 0.0;
  }

  return (radius * (root - tip)) + tip;
}

#  ifdef OS_MAC
in float dummy;
#  endif

void hair_get_pos_tan_binor_time(bool is_persp,
                                 mat4 invmodel_mat,
                                 vec3 camera_pos,
                                 vec3 camera_z,
                                 out vec3 wpos,
                                 out vec3 wtan,
                                 out vec3 wbinor,
                                 out float time,
                                 out float thickness,
                                 out float thick_time)
{
  int id = hair_get_base_id();
  vec4 data = texelFetch(hairPointBuffer, id);
  wpos = data.point_position;
  time = data.point_time;

#  ifdef OS_MAC
  /* Generate a dummy read to avoid the driver bug with shaders having no
   * vertex reads on macOS (T60171) */
  wpos.y += dummy * 0.0;
#  endif

  if (time == 0.0) {
    /* Hair root */
    wtan = texelFetch(hairPointBuffer, id + 1).point_position - wpos;
  }
  else {
    wtan = wpos - texelFetch(hairPointBuffer, id - 1).point_position;
  }

  wpos = (hairDupliMatrix * vec4(wpos, 1.0)).xyz;
  wtan = -normalize(mat3(hairDupliMatrix) * wtan);

  vec3 camera_vec = (is_persp) ? camera_pos - wpos : camera_z;
  wbinor = normalize(cross(camera_vec, wtan));

  thickness = hair_shaperadius(hairRadShape, hairRadRoot, hairRadTip, time);

  if (hairThicknessRes > 1) {
    thick_time = float(gl_VertexID % hairThicknessRes) / float(hairThicknessRes - 1);
    thick_time = thickness * (thick_time * 2.0 - 1.0);

    /* Take object scale into account.
     * NOTE: This only works fine with uniform scaling. */
    float scale = 1.0 / length(mat3(invmodel_mat) * wbinor);

    wpos += wbinor * thick_time * scale;
  }
}

vec2 hair_get_customdata_vec2(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).rg;
}

vec3 hair_get_customdata_vec3(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).rgb;
}

vec4 hair_get_customdata_vec4(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).rgba;
}

vec3 hair_get_strand_pos(void)
{
  int id = hair_get_strand_id() * hairStrandsRes;
  return texelFetch(hairPointBuffer, id).point_position;
}

#endif
