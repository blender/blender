
in vec3 pos;
in int vclass;

/* Instance */
in mat4 inst_obmat;
in vec4 color;

#define lamp_area_size inst_data.xy
#define lamp_clip_sta inst_data.z
#define lamp_clip_end inst_data.w

#define lamp_spot_cosine inst_data.x
#define lamp_spot_blend inst_data.y

#define camera_corner inst_data.xy
#define camera_center inst_data.zw
#define camera_dist inst_color_data
#define camera_dist_sta inst_data.z
#define camera_dist_end inst_data.w
#define camera_distance_color inst_data.x

#define empty_size inst_data.xyz
#define empty_scale inst_data.w

#define VCLASS_LIGHT_AREA_SHAPE (1 << 0)
#define VCLASS_LIGHT_SPOT_SHAPE (1 << 1)
#define VCLASS_LIGHT_SPOT_BLEND (1 << 2)
#define VCLASS_LIGHT_SPOT_CONE (1 << 3)
#define VCLASS_LIGHT_DIST (1 << 4)

#define VCLASS_CAMERA_FRAME (1 << 5)
#define VCLASS_CAMERA_DIST (1 << 6)
#define VCLASS_CAMERA_VOLUME (1 << 7)

#define VCLASS_SCREENSPACE (1 << 8)
#define VCLASS_SCREENALIGNED (1 << 9)

#define VCLASS_EMPTY_SCALED (1 << 10)
#define VCLASS_EMPTY_AXES (1 << 11)
#define VCLASS_EMPTY_AXES_NAME (1 << 12)
#define VCLASS_EMPTY_AXES_SHADOW (1 << 13)
#define VCLASS_EMPTY_SIZE (1 << 14)

flat out vec4 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;

void main()
{
  /* Extract data packed inside the unused mat4 members. */
  vec4 inst_data = vec4(inst_obmat[0][3], inst_obmat[1][3], inst_obmat[2][3], inst_obmat[3][3]);
  float inst_color_data = color.a;
  mat4 obmat = inst_obmat;
  obmat[0][3] = obmat[1][3] = obmat[2][3] = 0.0;
  obmat[3][3] = 1.0;

  finalColor = color;
  if (color.a < 0.0) {
    finalColor.a = 1.0;
  }

  float lamp_spot_sine;
  vec3 vpos = pos;
  vec3 vofs = vec3(0.0);
  /* Lights */
  if ((vclass & VCLASS_LIGHT_AREA_SHAPE) != 0) {
    /* HACK: use alpha color for spots to pass the area_size. */
    if (inst_color_data < 0.0) {
      lamp_area_size.xy = vec2(-inst_color_data);
    }
    vpos.xy *= lamp_area_size.xy;
  }
  else if ((vclass & VCLASS_LIGHT_SPOT_SHAPE) != 0) {
    lamp_spot_sine = sqrt(1.0 - lamp_spot_cosine * lamp_spot_cosine);
    lamp_spot_sine *= ((vclass & VCLASS_LIGHT_SPOT_BLEND) != 0) ? lamp_spot_blend : 1.0;
    vpos = vec3(pos.xy * lamp_spot_sine, -lamp_spot_cosine);
  }
  else if ((vclass & VCLASS_LIGHT_DIST) != 0) {
    /* Meh nasty mess. Select one of the 6 axes to display on. (see light_distance_z_get()) */
    int dist_axis = int(pos.z);
    float dist = pos.z - floor(pos.z) - 0.5;
    float inv = sign(dist);
    dist = (abs(dist) > 0.15) ? lamp_clip_end : lamp_clip_sta;
    vofs[dist_axis] = inv * dist / length(obmat[dist_axis].xyz);
    vpos.z = 0.0;
    if (lamp_clip_end < 0.0) {
      vpos = vofs = vec3(0.0);
    }
  }
  /* Camera */
  else if ((vclass & VCLASS_CAMERA_FRAME) != 0) {
    if ((vclass & VCLASS_CAMERA_VOLUME) != 0) {
      vpos.z = mix(color.b, color.a, pos.z);
    }
    else if (camera_dist > 0.0) {
      vpos.z = -abs(camera_dist);
    }
    else {
      vpos.z *= -abs(camera_dist);
    }
    vpos.xy = (camera_center + camera_corner * vpos.xy) * abs(vpos.z);
  }
  else if ((vclass & VCLASS_CAMERA_DIST) != 0) {
    vofs.xy = vec2(0.0);
    vofs.z = -mix(camera_dist_sta, camera_dist_end, pos.z);
    vpos.z = 0.0;
    /* Distance line endpoints color */
    if (any(notEqual(pos.xy, vec2(0.0)))) {
      /* Override color. */
      switch (int(camera_distance_color)) {
        case 0: /* Mist */
          finalColor = vec4(0.5, 0.5, 0.5, 1.0);
          break;
        case 1: /* Mist Active */
          finalColor = vec4(1.0, 1.0, 1.0, 1.0);
          break;
        case 2: /* Clip */
          finalColor = vec4(0.5, 0.5, 0.25, 1.0);
          break;
        case 3: /* Clip Active */
          finalColor = vec4(1.0, 1.0, 0.5, 1.0);
          break;
      }
    }
    /* Focus cross */
    if (pos.z == 2.0) {
      vofs.z = 0.0;
      if (camera_dist < 0.0) {
        vpos.z = -abs(camera_dist);
      }
      else {
        /* Disabled */
        vpos = vec3(0.0);
      }
    }
  }
  /* Empties */
  else if ((vclass & VCLASS_EMPTY_SCALED) != 0) {
    /* This is a bit silly but we avoid scaling the object matrix on CPU (saving a mat4 mul) */
    vpos *= empty_scale;
  }
  else if ((vclass & VCLASS_EMPTY_SIZE) != 0) {
    /* This is a bit silly but we avoid scaling the object matrix on CPU (saving a mat4 mul) */
    vpos *= empty_size;
  }
  else if ((vclass & VCLASS_EMPTY_AXES) != 0) {
    float axis = vpos.z;
    vofs[int(axis)] = (1.0 + fract(axis)) * empty_scale;
    /* Scale uniformly by axis length */
    vpos *= length(obmat[int(axis)].xyz) * empty_scale;

    vec3 axis_color = vec3(0.0);
    axis_color[int(axis)] = 1.0;
    finalColor.rgb = mix(axis_color + fract(axis), color.rgb, color.a);
    finalColor.a = 1.0;
  }

  /* Not exclusive with previous flags. */
  if ((vclass & VCLASS_CAMERA_VOLUME) != 0) {
    /* Unpack final color. */
    int color_class = int(floor(color.r));
    float color_intensity = fract(color.r);
    switch (color_class) {
      case 0: /* No eye (convergence plane) */
        finalColor = vec4(1.0, 1.0, 1.0, 1.0);
        break;
      case 1: /* Left eye  */
        finalColor = vec4(0.0, 1.0, 1.0, 1.0);
        break;
      case 2: /* Right eye */
        finalColor = vec4(1.0, 0.0, 0.0, 1.0);
        break;
    }
    finalColor *= vec4(vec3(color_intensity), color.g);
  }

  vec3 world_pos;
  if ((vclass & VCLASS_SCREENSPACE) != 0) {
    /* Relative to DPI scaling. Have constant screen size. */
    vec3 screen_pos = screenVecs[0].xyz * vpos.x + screenVecs[1].xyz * vpos.y;
    vec3 p = (obmat * vec4(vofs, 1.0)).xyz;
    float screen_size = mul_project_m4_v3_zfac(p) * sizePixel;
    world_pos = p + screen_pos * screen_size;
  }
  else if ((vclass & VCLASS_SCREENALIGNED) != 0) {
    /* World sized, camera facing geometry. */
    vec3 screen_pos = screenVecs[0].xyz * vpos.x + screenVecs[1].xyz * vpos.y;
    world_pos = (obmat * vec4(vofs, 1.0)).xyz + screen_pos;
  }
  else {
    world_pos = (obmat * vec4(vofs + vpos, 1.0)).xyz;
  }

  if ((vclass & VCLASS_LIGHT_SPOT_CONE) != 0) {
    /* Compute point on the cone before and after this one. */
    vec2 perp = vec2(pos.y, -pos.x);
    const float incr_angle = 2.0 * 3.1415 / 32.0;
    const vec2 slope = vec2(cos(incr_angle), sin(incr_angle));
    vec3 p0 = vec3((pos.xy * slope.x + perp * slope.y) * lamp_spot_sine, -lamp_spot_cosine);
    vec3 p1 = vec3((pos.xy * slope.x - perp * slope.y) * lamp_spot_sine, -lamp_spot_cosine);
    p0 = (obmat * vec4(p0, 1.0)).xyz;
    p1 = (obmat * vec4(p1, 1.0)).xyz;
    /* Compute normals of each side. */
    vec3 edge = obmat[3].xyz - world_pos;
    vec3 n0 = normalize(cross(edge, p0 - world_pos));
    vec3 n1 = normalize(cross(edge, world_pos - p1));
    bool persp = (ProjectionMatrix[3][3] == 0.0);
    vec3 V = (persp) ? normalize(ViewMatrixInverse[3].xyz - world_pos) : ViewMatrixInverse[2].xyz;
    /* Discard non-silhouete edges.  */
    bool facing0 = dot(n0, V) > 0.0;
    bool facing1 = dot(n1, V) > 0.0;
    if (facing0 == facing1) {
      /* Hide line by making it cover 0 pixels. */
      world_pos = obmat[3].xyz;
    }
  }

  gl_Position = point_world_to_ndc(world_pos);

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
