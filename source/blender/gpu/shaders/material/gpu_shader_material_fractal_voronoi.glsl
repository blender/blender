#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_voronoi.glsl)

#define FRACTAL_VORONOI_X_FX(T) \
  VoronoiOutput fractal_voronoi_x_fx(VoronoiParams params, T coord) \
  { \
    float amplitude = 1.0; \
    float max_amplitude = 0.0; \
    float scale = 1.0; \
\
    VoronoiOutput Output; \
    Output.Distance = 0.0; \
    Output.Color = vec3(0.0, 0.0, 0.0); \
    Output.Position = vec4(0.0, 0.0, 0.0, 0.0); \
    bool zero_input = params.detail == 0.0 || params.roughness == 0.0 || \
                      params.lacunarity == 0.0; \
\
    for (int i = 0; i <= ceil(params.detail); ++i) { \
      VoronoiOutput octave; \
      if (params.feature == 0) /* SHD_VORONOI_F1 */ { \
        octave = voronoi_f1(params, coord * scale); \
      } \
      else if (params.feature == 2) /* SHD_VORONOI_SMOOTH_F1 */ { \
        octave = voronoi_smooth_f1(params, coord * scale); \
      } \
      else { \
        octave = voronoi_f2(params, coord * scale); \
      } \
\
      if (zero_input) { \
        max_amplitude = 1.0; \
        Output = octave; \
        break; \
      } \
      else if (i <= params.detail) { \
        max_amplitude += amplitude; \
        Output.Distance += octave.Distance * amplitude; \
        Output.Color += octave.Color * amplitude; \
        Output.Position = mix(Output.Position, octave.Position / scale, amplitude); \
        scale *= params.lacunarity; \
        amplitude *= params.roughness; \
      } \
      else { \
        float remainder = params.detail - floor(params.detail); \
        if (remainder != 0.0) { \
          max_amplitude = mix(max_amplitude, max_amplitude + amplitude, remainder); \
          Output.Distance = mix( \
              Output.Distance, Output.Distance + octave.Distance * amplitude, remainder); \
          Output.Color = mix(Output.Color, Output.Color + octave.Color * amplitude, remainder); \
          Output.Position = mix(Output.Position, \
                                mix(Output.Position, octave.Position / scale, amplitude), \
                                remainder); \
        } \
      } \
    } \
\
    if (params.normalize) { \
      Output.Distance /= max_amplitude * params.max_distance; \
      Output.Color /= max_amplitude; \
    } \
\
    Output.Position = safe_divide(Output.Position, params.scale); \
\
    return Output; \
  }

#define FRACTAL_VORONOI_DISTANCE_TO_EDGE_FUNCTION(T) \
  float fractal_voronoi_distance_to_edge(VoronoiParams params, T coord) \
  { \
    float amplitude = 1.0; \
    float max_amplitude = 0.5 + 0.5 * params.randomness; \
    float scale = 1.0; \
    float distance = 8.0; \
\
    bool zero_input = params.detail == 0.0 || params.roughness == 0.0 || \
                      params.lacunarity == 0.0; \
\
    for (int i = 0; i <= ceil(params.detail); ++i) { \
      float octave_distance = voronoi_distance_to_edge(params, coord * scale); \
\
      if (zero_input) { \
        distance = octave_distance; \
        break; \
      } \
      else if (i <= params.detail) { \
        max_amplitude = mix(max_amplitude, (0.5 + 0.5 * params.randomness) / scale, amplitude); \
        distance = mix(distance, min(distance, octave_distance / scale), amplitude); \
        scale *= params.lacunarity; \
        amplitude *= params.roughness; \
      } \
      else { \
        float remainder = params.detail - floor(params.detail); \
        if (remainder != 0.0) { \
          float lerp_amplitude = mix( \
              max_amplitude, (0.5 + 0.5 * params.randomness) / scale, amplitude); \
          max_amplitude = mix(max_amplitude, lerp_amplitude, remainder); \
          float lerp_distance = mix(distance, min(distance, octave_distance / scale), amplitude); \
          distance = mix(distance, min(distance, lerp_distance), remainder); \
        } \
      } \
    } \
\
    if (params.normalize) { \
      distance /= max_amplitude; \
    } \
\
    return distance; \
  }

/* **** 1D Fractal Voronoi **** */

FRACTAL_VORONOI_X_FX(float)

FRACTAL_VORONOI_DISTANCE_TO_EDGE_FUNCTION(float)

/* **** 2D Fractal Voronoi **** */

FRACTAL_VORONOI_X_FX(vec2)

FRACTAL_VORONOI_DISTANCE_TO_EDGE_FUNCTION(vec2)

/* **** 3D Fractal Voronoi **** */

FRACTAL_VORONOI_X_FX(vec3)

FRACTAL_VORONOI_DISTANCE_TO_EDGE_FUNCTION(vec3)

/* **** 4D Fractal Voronoi **** */

FRACTAL_VORONOI_X_FX(vec4)

FRACTAL_VORONOI_DISTANCE_TO_EDGE_FUNCTION(vec4)
