/* SPDX-FileCopyrightText: 2013 Jorge Jimenez <jorge@iryoku.com>
 * SPDX-FileCopyrightText: 2013 Jose I. Echevarria <joseignacioechevarria@gmail.com>
 * SPDX-FileCopyrightText: 2013 Belen Masia <bmasia@unizar.es>
 * SPDX-FileCopyrightText: 2013 Fernando Navarro <fernandn@microsoft.com>
 * SPDX-FileCopyrightText: 2013 Diego Gutierrez <diegog@unizar.es>
 * SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: MIT AND GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 *                  _______  ___  ___       ___           ___
 *                 /       ||   \/   |     /   \         /   \
 *                |   (---- |  \  /  |    /  ^  \       /  ^  \
 *                 \   \    |  |\/|  |   /  /_\  \     /  /_\  \
 *              ----)   |   |  |  |  |  /  _____  \   /  _____  \
 *             |_______/    |__|  |__| /__/     \__\ /__/     \__\
 *
 *                               E N H A N C E D
 *       S U B P I X E L   M O R P H O L O G I C A L   A N T I A L I A S I N G
 *
 *                         http://www.iryoku.com/smaa/
 *
 * Hi, welcome aboard!
 *
 * Here you'll find instructions to get the shader up and running as fast as
 * possible.
 *
 * IMPORTANTE NOTICE: when updating, remember to update both this file and the
 * precomputed textures! They may change from version to version.
 *
 * The shader has three passes, chained together as follows:
 *
 *                           |input|------------------�
 *                              v                     |
 *                    [ SMAA*EdgeDetection ]          |
 *                              v                     |
 *                          |edgesTex|                |
 *                              v                     |
 *              [ SMAABlendingWeightCalculation ]     |
 *                              v                     |
 *                          |blendTex|                |
 *                              v                     |
 *                [ SMAANeighborhoodBlending ] <------�
 *                              v
 *                           |output|
 *
 * Note that each [pass] has its own vertex and pixel shader. Remember to use
 * oversized triangles instead of quads to avoid overshading along the
 * diagonal.
 *
 * You've three edge detection methods to choose from: luma, color or depth.
 * They represent different quality/performance and anti-aliasing/sharpness
 * tradeoffs, so our recommendation is for you to choose the one that best
 * suits your particular scenario:
 *
 * - Depth edge detection is usually the fastest but it may miss some edges.
 *
 * - Luma edge detection is usually more expensive than depth edge detection,
 *   but catches visible edges that depth edge detection can miss.
 *
 * - Color edge detection is usually the most expensive one but catches
 *   chroma-only edges.
 *
 * For quickstarters: just use luma edge detection.
 *
 * The general advice is to not rush the integration process and ensure each
 * step is done correctly (don't try to integrate SMAA T2x with predicated edge
 * detection from the start!). Ok then, let's go!
 *
 *  1. The first step is to create two RGBA temporal render targets for holding
 *     |edgesTex| and |blendTex|.
 *
 *     In DX10 or DX11, you can use a RG render target for the edges texture.
 *     In the case of NVIDIA GPUs, using RG render targets seems to actually be
 *     slower.
 *
 *     On the Xbox 360, you can use the same render target for resolving both
 *     |edgesTex| and |blendTex|, as they aren't needed simultaneously.
 *
 *  2. Both temporal render targets |edgesTex| and |blendTex| must be cleared
 *     each frame. Do not forget to clear the alpha channel!
 *
 *  3. The next step is loading the two supporting precalculated textures,
 *     'areaTex' and 'searchTex'. You'll find them in the 'Textures' folder as
 *     C++ headers, and also as regular DDS files. They'll be needed for the
 *     'SMAABlendingWeightCalculation' pass.
 *
 *     If you use the C++ headers, be sure to load them in the format specified
 *     inside of them.
 *
 *     You can also compress 'areaTex' and 'searchTex' using BC5 and BC4
 *     respectively, if you have that option in your content processor pipeline.
 *     When compressing then, you get a non-perceptible quality decrease, and a
 *     marginal performance increase.
 *
 *  4. All samplers must be set to linear filtering and clamp.
 *
 *     After you get the technique working, remember that 64-bit inputs have
 *     half-rate linear filtering on GCN.
 *
 *     If SMAA is applied to 64-bit color buffers, switching to point filtering
 *     when accessing them will increase the performance. Search for
 *     'SMAASamplePoint' to see which textures may benefit from point
 *     filtering, and where (which is basically the color input in the edge
 *     detection and resolve passes).
 *
 *  5. All texture reads and buffer writes must be non-sRGB, with the exception
 *     of the input read and the output write in
 *     'SMAANeighborhoodBlending' (and only in this pass!). If sRGB reads in
 *     this last pass are not possible, the technique will work anyway, but
 *     will perform antialiasing in gamma space.
 *
 *     IMPORTANT: for best results the input read for the color/luma edge
 *     detection should *NOT* be sRGB.
 *
 *  6. Before including SMAA.h you'll have to setup the render target metrics,
 *     the target and any optional configuration defines. Optionally you can
 *     use a preset.
 *
 *     You have the following targets available:
 *         SMAA_HLSL_3
 *         SMAA_HLSL_4
 *         SMAA_HLSL_4_1
 *         SMAA_GLSL_3 *
 *         SMAA_GLSL_4 *
 *
 *         * (See SMAA_INCLUDE_VS and SMAA_INCLUDE_PS below).
 *
 *     And four presets:
 *         SMAA_PRESET_LOW          (%60 of the quality)
 *         SMAA_PRESET_MEDIUM       (%80 of the quality)
 *         SMAA_PRESET_HIGH         (%95 of the quality)
 *         SMAA_PRESET_ULTRA        (%99 of the quality)
 *
 *     For example:
 *         #define SMAA_RT_METRICS float4(1.0f / 1280.0f, 1.0f / 720.0f, 1280.0f, 720.0f)
 *         #define SMAA_HLSL_4
 *         #define SMAA_PRESET_HIGH
 *         #include "SMAA.h"
 *
 *     Note that SMAA_RT_METRICS doesn't need to be a macro, it can be a
 *     uniform variable. The code is designed to minimize the impact of not
 *     using a constant value, but it is still better to hardcode it.
 *
 *     Depending on how you encoded 'areaTex' and 'searchTex', you may have to
 *     add (and customize) the following defines before including SMAA.h:
 *          #define SMAA_AREATEX_SELECT(sample) sample.rg
 *          #define SMAA_SEARCHTEX_SELECT(sample) sample.r
 *
 *     If your engine is already using porting macros, you can define
 *     SMAA_CUSTOM_SL, and define the porting functions by yourself.
 *
 *  7. Then, you'll have to setup the passes as indicated in the scheme above.
 *     You can take a look into SMAA.fx, to see how we did it for our demo.
 *     Checkout the function wrappers, you may want to copy-paste them!
 *
 *  8. It's recommended to validate the produced |edgesTex| and |blendTex|.
 *     You can use a screenshot from your engine to compare the |edgesTex|
 *     and |blendTex| produced inside of the engine with the results obtained
 *     with the reference demo.
 *
 *  9. After you get the last pass to work, it's time to optimize. You'll have
 *     to initialize a stencil buffer in the first pass (discard is already in
 *     the code), then mask execution by using it the second pass. The last
 *     pass should be executed in all pixels.
 *
 *
 * After this point you can choose to enable predicated thresholding,
 * temporal supersampling and motion blur integration:
 *
 * a) If you want to use predicated thresholding, take a look into
 *    SMAA_PREDICATION; you'll need to pass an extra texture in the edge
 *    detection pass.
 *
 * b) If you want to enable temporal supersampling (SMAA T2x):
 *
 * 1. The first step is to render using subpixel jitters. I won't go into
 *    detail, but it's as simple as moving each vertex position in the
 *    vertex shader, you can check how we do it in our DX10 demo.
 *
 * 2. Then, you must setup the temporal resolve. You may want to take a look
 *    into SMAAResolve for resolving 2x modes. After you get it working, you'll
 *    probably see ghosting everywhere. But fear not, you can enable the
 *    CryENGINE temporal reprojection by setting the SMAA_REPROJECTION macro.
 *    Check out SMAA_DECODE_VELOCITY if your velocity buffer is encoded.
 *
 * 3. The next step is to apply SMAA to each subpixel jittered frame, just as
 *    done for 1x.
 *
 * 4. At this point you should already have something usable, but for best
 *    results the proper area textures must be set depending on current jitter.
 *    For this, the parameter 'subsampleIndices' of
 *    'SMAABlendingWeightCalculationPS' must be set as follows, for our T2x
 *    mode:
 *
 *    @SUBSAMPLE_INDICES
 *
 *    | S# |  Camera Jitter   |  subsampleIndices    |
 *    +----+------------------+---------------------+
 *    |  0 |  ( 0.25f, -0.25f)  |  float4(1, 1, 1, 0)  |
 *    |  1 |  (-0.25f,  0.25f)  |  float4(2, 2, 2, 0)  |
 *
 *    These jitter positions assume a bottom-to-top y axis. S# stands for the
 *    sample number.
 *
 * More information about temporal supersampling here:
 *    http://iryoku.com/aacourse/downloads/13-Anti-Aliasing-Methods-in-CryENGINE-3.pdf
 *
 * c) If you want to enable spatial multisampling (SMAA S2x):
 *
 * 1. The scene must be rendered using MSAA 2x. The MSAA 2x buffer must be
 *    created with:
 *      - DX10:     see below (*)
 *      - DX10.1:   D3D10_STANDARD_MULTISAMPLE_PATTERN or
 *      - DX11:     D3D11_STANDARD_MULTISAMPLE_PATTERN
 *
 *    This allows to ensure that the subsample order matches the table in
 *    @SUBSAMPLE_INDICES.
 *
 *    (*) In the case of DX10, we refer the reader to:
 *      - SMAA::detectMSAAOrder and
 *      - SMAA::msaaReorder
 *
 *    These functions allow matching the standard multisample patterns by
 *    detecting the subsample order for a specific GPU, and reordering
 *    them appropriately.
 *
 * 2. A shader must be run to output each subsample into a separate buffer
 *    (DX10 is required). You can use SMAASeparate for this purpose, or just do
 *    it in an existing pass (for example, in the tone mapping pass, which has
 *    the advantage of feeding tone mapped subsamples to SMAA, which will yield
 *    better results).
 *
 * 3. The full SMAA 1x pipeline must be run for each separated buffer, storing
 *    the results in the final buffer. The second run should alpha blend with
 *    the existing final buffer using a blending factor of 0.5.
 *    'subsampleIndices' must be adjusted as in the SMAA T2x case (see point
 *    b).
 *
 * d) If you want to enable temporal supersampling on top of SMAA S2x
 *    (which actually is SMAA 4x):
 *
 * 1. SMAA 4x consists on temporally jittering SMAA S2x, so the first step is
 *    to calculate SMAA S2x for current frame. In this case, 'subsampleIndices'
 *    must be set as follows:
 *
 *    | F# | S# |   Camera Jitter    |    Net Jitter     |   subsampleIndices   |
 *    +----+----+--------------------+-------------------+----------------------+
 *    |  0 |  0 |  ( 0.125f,  0.125f)  |  ( 0.375f, -0.125f) |  float4(5, 3, 1, 3)  |
 *    |  0 |  1 |  ( 0.125f,  0.125f)  |  (-0.125f,  0.375f) |  float4(4, 6, 2, 3)  |
 *    +----+----+--------------------+-------------------+----------------------+
 *    |  1 |  2 |  (-0.125f, -0.125f)  |  ( 0.125f, -0.375f) |  float4(3, 5, 1, 4)  |
 *    |  1 |  3 |  (-0.125f, -0.125f)  |  (-0.375f,  0.125f) |  float4(6, 4, 2, 4)  |
 *
 *    These jitter positions assume a bottom-to-top y axis. F# stands for the
 *    frame number. S# stands for the sample number.
 *
 * 2. After calculating SMAA S2x for current frame (with the new subsample
 *    indices), previous frame must be reprojected as in SMAA T2x mode (see
 *    point b).
 *
 * e) If motion blur is used, you may want to do the edge detection pass
 *    together with motion blur. This has two advantages:
 *
 * 1. Pixels under heavy motion can be omitted from the edge detection process.
 *    For these pixels we can just store "no edge", as motion blur will take
 *    care of them.
 * 2. The center pixel tap is reused.
 *
 * Note that in this case depth testing should be used instead of stenciling,
 * as we have to write all the pixels in the motion blur pass.
 *
 * That's it!
 */

/* ----------------------------------------------------------------------------
 * SMAA Presets */

/**
 * Note that if you use one of these presets, the following configuration
 * macros will be ignored if set in the "Configurable Defines" section.
 */

#if defined(SMAA_PRESET_LOW)
#  define SMAA_THRESHOLD 0.15f
#  define SMAA_MAX_SEARCH_STEPS 4
#  define SMAA_DISABLE_DIAG_DETECTION
#  define SMAA_DISABLE_CORNER_DETECTION
#elif defined(SMAA_PRESET_MEDIUM)
#  define SMAA_THRESHOLD 0.1f
#  define SMAA_MAX_SEARCH_STEPS 8
#  define SMAA_DISABLE_DIAG_DETECTION
#  define SMAA_DISABLE_CORNER_DETECTION
#elif defined(SMAA_PRESET_HIGH)
#  define SMAA_THRESHOLD 0.1f
#  define SMAA_MAX_SEARCH_STEPS 16
#  define SMAA_MAX_SEARCH_STEPS_DIAG 8
#  define SMAA_CORNER_ROUNDING 25
#elif defined(SMAA_PRESET_ULTRA)
#  define SMAA_THRESHOLD 0.05f
#  define SMAA_MAX_SEARCH_STEPS 32
#  define SMAA_MAX_SEARCH_STEPS_DIAG 16
#  define SMAA_CORNER_ROUNDING 25
#endif

/* ----------------------------------------------------------------------------
 * Configurable Defines */

/**
 * SMAA_THRESHOLD specifies the threshold or sensitivity to edges.
 * Lowering this value you will be able to detect more edges at the expense of
 * performance.
 *
 * Range: [0, 0.5f]
 *   0.1 is a reasonable value, and allows to catch most visible edges.
 *   0.05 is a rather overkill value, that allows to catch 'em all.
 *
 *   If temporal supersampling is used, 0.2 could be a reasonable value, as low
 *   contrast edges are properly filtered by just 2x.
 */
#ifndef SMAA_THRESHOLD
#  define SMAA_THRESHOLD 0.1f
#endif

/**
 * SMAA_DEPTH_THRESHOLD specifies the threshold for depth edge detection.
 *
 * Range: depends on the depth range of the scene.
 */
#ifndef SMAA_DEPTH_THRESHOLD
#  define SMAA_DEPTH_THRESHOLD (0.1f * SMAA_THRESHOLD)
#endif

/**
 * SMAA_MAX_SEARCH_STEPS specifies the maximum steps performed in the
 * horizontal/vertical pattern searches, at each side of the pixel.
 *
 * In number of pixels, it's actually the double. So the maximum line length
 * perfectly handled by, for example 16, is 64 (by perfectly, we meant that
 * longer lines won't look as good, but still antialiased).
 *
 * Range: [0, 112]
 */
#ifndef SMAA_MAX_SEARCH_STEPS
#  define SMAA_MAX_SEARCH_STEPS 16
#endif

/**
 * SMAA_MAX_SEARCH_STEPS_DIAG specifies the maximum steps performed in the
 * diagonal pattern searches, at each side of the pixel. In this case we jump
 * one pixel at time, instead of two.
 *
 * Range: [0, 20]
 *
 * On high-end machines it is cheap (between a 0.8x and 0.9x slower for 16
 * steps), but it can have a significant impact on older machines.
 *
 * Define SMAA_DISABLE_DIAG_DETECTION to disable diagonal processing.
 */
#ifndef SMAA_MAX_SEARCH_STEPS_DIAG
#  define SMAA_MAX_SEARCH_STEPS_DIAG 8
#endif

/**
 * SMAA_CORNER_ROUNDING specifies how much sharp corners will be rounded.
 *
 * Range: [0, 100]
 *
 * Define SMAA_DISABLE_CORNER_DETECTION to disable corner processing.
 */
#ifndef SMAA_CORNER_ROUNDING
#  define SMAA_CORNER_ROUNDING 25
#endif

/**
 * If there is an neighbor edge that has SMAA_LOCAL_CONTRAST_FACTOR times
 * bigger contrast than current edge, current edge will be discarded.
 *
 * This allows to eliminate spurious crossing edges, and is based on the fact
 * that, if there is too much contrast in a direction, that will hide
 * perceptually contrast in the other neighbors.
 */
#ifndef SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR
#  define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR 2.0f
#endif

/**
 * Predicated thresholding allows to better preserve texture details and to
 * improve performance, by decreasing the number of detected edges using an
 * additional buffer like the light accumulation buffer, object ids or even the
 * depth buffer (the depth buffer usage may be limited to indoor or short range
 * scenes).
 *
 * It locally decreases the luma or color threshold if an edge is found in an
 * additional buffer (so the global threshold can be higher).
 *
 * This method was developed by Playstation EDGE MLAA team, and used in
 * Killzone 3, by using the light accumulation buffer. More information here:
 *     http://iryoku.com/aacourse/downloads/06-MLAA-on-PS3.pptx
 */
#ifndef SMAA_PREDICATION
#  define SMAA_PREDICATION 0
#endif

/**
 * Threshold to be used in the additional predication buffer.
 *
 * Range: depends on the input, so you'll have to find the magic number that
 * works for you.
 */
#ifndef SMAA_PREDICATION_THRESHOLD
#  define SMAA_PREDICATION_THRESHOLD 0.01f
#endif

/**
 * How much to scale the global threshold used for luma or color edge
 * detection when using predication.
 *
 * Range: [1, 5]
 */
#ifndef SMAA_PREDICATION_SCALE
#  define SMAA_PREDICATION_SCALE 2.0f
#endif

/**
 * How much to locally decrease the threshold.
 *
 * Range: [0, 1]
 */
#ifndef SMAA_PREDICATION_STRENGTH
#  define SMAA_PREDICATION_STRENGTH 0.4f
#endif

/**
 * Temporal reprojection allows to remove ghosting artifacts when using
 * temporal supersampling. We use the CryEngine 3 method which also introduces
 * velocity weighting. This feature is of extreme importance for totally
 * removing ghosting. More information here:
 *    http://iryoku.com/aacourse/downloads/13-Anti-Aliasing-Methods-in-CryENGINE-3.pdf
 *
 * Note that you'll need to setup a velocity buffer for enabling reprojection.
 * For static geometry, saving the previous depth buffer is a viable
 * alternative.
 */
#ifndef SMAA_REPROJECTION
#  define SMAA_REPROJECTION 0
#endif

/**
 * SMAA_REPROJECTION_WEIGHT_SCALE controls the velocity weighting. It allows to
 * remove ghosting trails behind the moving object, which are not removed by
 * just using reprojection. Using low values will exhibit ghosting, while using
 * high values will disable temporal supersampling under motion.
 *
 * Behind the scenes, velocity weighting removes temporal supersampling when
 * the velocity of the subsamples differs (meaning they are different objects).
 *
 * Range: [0, 80]
 */
#ifndef SMAA_REPROJECTION_WEIGHT_SCALE
#  define SMAA_REPROJECTION_WEIGHT_SCALE 30.0f
#endif

/**
 * On some compilers, discard cannot be used in vertex shaders. Thus, they need
 * to be compiled separately.
 */
#ifndef SMAA_INCLUDE_VS
#  define SMAA_INCLUDE_VS 1
#endif
#ifndef SMAA_INCLUDE_PS
#  define SMAA_INCLUDE_PS 1
#endif

/* ----------------------------------------------------------------------------
 * Texture Access Defines */

#ifndef SMAA_AREATEX_SELECT
#  if defined(SMAA_HLSL_3)
#    define SMAA_AREATEX_SELECT(sample) sample.ra
#  else
#    define SMAA_AREATEX_SELECT(sample) sample.rg
#  endif
#endif

#ifndef SMAA_SEARCHTEX_SELECT
#  define SMAA_SEARCHTEX_SELECT(sample) sample.r
#endif

#ifndef SMAA_DECODE_VELOCITY
#  define SMAA_DECODE_VELOCITY(sample) sample.rg
#endif

/* ----------------------------------------------------------------------------
 * Non-Configurable Defines */

#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_PIXEL_SIZE (1.0f / float2(160.0f, 560.0f))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0f / 7.0f)
#define SMAA_SEARCHTEX_SIZE float2(66.0f, 33.0f)
#define SMAA_SEARCHTEX_PACKED_SIZE float2(64.0f, 16.0f)
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0f)

/* ----------------------------------------------------------------------------
 * Porting Functions */

#if defined(SMAA_HLSL_3)
#  define SMAATexture2D(tex) sampler2D tex
#  define SMAATexturePass2D(tex) tex
#  define SMAASampleLevelZero(tex, coord) tex2Dlod(tex, float4(coord, 0.0f, 0.0f))
#  define SMAASampleLevelZeroPoint(tex, coord) tex2Dlod(tex, float4(coord, 0.0f, 0.0f))
/* clang-format off */
#  define SMAASampleLevelZeroOffset(tex, coord, offset) tex2Dlod(tex, float4(coord + offset * SMAA_RT_METRICS.xy, 0.0f, 0.0f))
/* clang-format on */
#  define SMAASample(tex, coord) tex2D(tex, coord)
#  define SMAASamplePoint(tex, coord) tex2D(tex, coord)
#  define SMAASampleOffset(tex, coord, offset) tex2D(tex, coord + offset * SMAA_RT_METRICS.xy)
#  define SMAA_FLATTEN [flatten]
#  define SMAA_BRANCH [branch]
#endif
#if defined(SMAA_HLSL_4) || defined(SMAA_HLSL_4_1)
SamplerState LinearSampler
{
  Filter = MIN_MAG_LINEAR_MIP_POINT;
  AddressU = Clamp;
  AddressV = Clamp;
};
SamplerState PointSampler
{
  Filter = MIN_MAG_MIP_POINT;
  AddressU = Clamp;
  AddressV = Clamp;
};
#  define SMAATexture2D(tex) Texture2D tex
#  define SMAATexturePass2D(tex) tex
#  define SMAASampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#  define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
/* clang-format off */
#  define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
/* clang-format on */
#  define SMAASample(tex, coord) tex.Sample(LinearSampler, coord)
#  define SMAASamplePoint(tex, coord) tex.Sample(PointSampler, coord)
#  define SMAASampleOffset(tex, coord, offset) tex.Sample(LinearSampler, coord, offset)
#  define SMAA_FLATTEN [flatten]
#  define SMAA_BRANCH [branch]
#  define SMAATexture2DMS2(tex) Texture2DMS<float4, 2> tex
#  define SMAALoad(tex, pos, sample) tex.Load(pos, sample)
#  if defined(SMAA_HLSL_4_1)
#    define SMAAGather(tex, coord) tex.Gather(LinearSampler, coord, 0)
#  endif
#endif
#if defined(SMAA_GLSL_3) || defined(SMAA_GLSL_4) || defined(GPU_METAL) || defined(GPU_VULKAN)
#  define SMAATexture2D(tex) sampler2D tex
#  define SMAATexturePass2D(tex) tex
#  define SMAASampleLevelZero(tex, coord) textureLod(tex, coord, 0.0f)
#  define SMAASampleLevelZeroPoint(tex, coord) textureLod(tex, coord, 0.0f)
#  define SMAASampleLevelZeroOffset(tex, coord, offset) textureLodOffset(tex, coord, 0.0f, offset)
#  define SMAASample(tex, coord) texture(tex, coord)
#  define SMAASamplePoint(tex, coord) texture(tex, coord)
#  define SMAASampleOffset(tex, coord, offset) texture(tex, coord, offset)
#  define SMAA_FLATTEN
#  define SMAA_BRANCH
#  define lerp(a, b, t) mix(a, b, t)
#  define saturate(a) clamp(a, 0.0f, 1.0f)
#  if defined(SMAA_GLSL_4)
#    define SMAAGather(tex, coord) textureGather(tex, coord)
#  endif
#  if defined(SMAA_GLSL_4)
#    define mad(a, b, c) fma(a, b, c)
#  elif defined(GPU_VULKAN)
/* NOTE(Vulkan) mad macro doesn't work, define each override as work-around. */
float4 mad(float4 a, float4 b, float4 c)
{
  return fma(a, b, c);
}
float3 mad(float3 a, float3 b, float3 c)
{
  return fma(a, b, c);
}
float2 mad(float2 a, float2 b, float2 c)
{
  return fma(a, b, c);
}
float mad(float a, float b, float c)
{
  return fma(a, b, c);
}
#  else
#    define mad(a, b, c) (a * b + c)
#  endif
#endif

/* clang-format off */
#if !defined(SMAA_HLSL_3) && !defined(SMAA_HLSL_4) && !defined(SMAA_HLSL_4_1) && !defined(SMAA_GLSL_3) && !defined(SMAA_GLSL_4) && !defined(SMAA_CUSTOM_SL)
#  error you must define the shading language: SMAA_HLSL_*, SMAA_GLSL_* or SMAA_CUSTOM_SL
#endif
/* clang-format on */

/* ----------------------------------------------------------------------------
 * Misc functions */

/**
 * Gathers current pixel, and the top-left neighbors.
 */
float3 SMAAGatherNeighbors(float2 texcoord, float4 offset[3], SMAATexture2D(tex))
{
#ifdef SMAAGather
  return SMAAGather(tex, texcoord + SMAA_RT_METRICS.xy * float2(-0.5f, -0.5f)).grb;
#else
  float P = SMAASamplePoint(tex, texcoord).r;
  float Pleft = SMAASamplePoint(tex, offset[0].xy).r;
  float Ptop = SMAASamplePoint(tex, offset[0].zw).r;
  return float3(P, Pleft, Ptop);
#endif
}

/**
 * Adjusts the threshold by means of predication.
 */
float2 SMAACalculatePredicatedThreshold(float2 texcoord,
                                        float4 offset[3],
                                        SMAATexture2D(predicationTex))
{
  float3 neighbors = SMAAGatherNeighbors(texcoord, offset, SMAATexturePass2D(predicationTex));
  float2 delta = abs(neighbors.xx - neighbors.yz);
  float2 edges = step(SMAA_PREDICATION_THRESHOLD, delta);
  return SMAA_PREDICATION_SCALE * SMAA_THRESHOLD * (1.0f - SMAA_PREDICATION_STRENGTH * edges);
}

/**
 * Conditional move:
 */
void SMAAMovc(bool2 cond, inout float2 variable, float2 value)
{
  /* Use select function (select(genType A, genType B, genBType cond)). */
  variable = select(variable, value, cond);
}

void SMAAMovc(bool4 cond, inout float4 variable, float4 value)
{
  /* Use select function (select(genType A, genType B, genBType cond)). */
  variable = select(variable, value, cond);
}

#if SMAA_INCLUDE_VS
/* ----------------------------------------------------------------------------
 * Vertex Shaders */

/**
 * Edge Detection Vertex Shader
 */
void SMAAEdgeDetectionVS(float2 texcoord, float4 (&offset)[3])
{
  offset[0] = mad(SMAA_RT_METRICS.xyxy, float4(-1.0f, 0.0f, 0.0f, -1.0f), texcoord.xyxy);
  offset[1] = mad(SMAA_RT_METRICS.xyxy, float4(1.0f, 0.0f, 0.0f, 1.0f), texcoord.xyxy);
  offset[2] = mad(SMAA_RT_METRICS.xyxy, float4(-2.0f, 0.0f, 0.0f, -2.0f), texcoord.xyxy);
}

/**
 * Blend Weight Calculation Vertex Shader
 */
void SMAABlendingWeightCalculationVS(float2 texcoord, out float2 pixcoord, float4 (&offset)[3])
{
  pixcoord = texcoord * SMAA_RT_METRICS.zw;

  // We will use these offsets for the searches later on (see @PSEUDO_GATHER4):
  offset[0] = mad(SMAA_RT_METRICS.xyxy, float4(-0.25f, -0.125f, 1.25f, -0.125f), texcoord.xyxy);
  offset[1] = mad(SMAA_RT_METRICS.xyxy, float4(-0.125f, -0.25f, -0.125f, 1.25f), texcoord.xyxy);

  // And these for the searches, they indicate the ends of the loops:
  offset[2] = mad(SMAA_RT_METRICS.xxyy,
                  float4(-2.0f, 2.0f, -2.0f, 2.0f) * float(SMAA_MAX_SEARCH_STEPS),
                  float4(offset[0].xz, offset[1].yw));
}

/**
 * Neighborhood Blending Vertex Shader
 */
void SMAANeighborhoodBlendingVS(float2 texcoord, out float4 offset)
{
  offset = mad(SMAA_RT_METRICS.xyxy, float4(1.0f, 0.0f, 0.0f, 1.0f), texcoord.xyxy);
}
#endif  // SMAA_INCLUDE_VS

#if SMAA_INCLUDE_PS
/* ----------------------------------------------------------------------------
 * Edge Detection Pixel Shaders (First Pass) */

#  ifndef SMAA_LUMA_WEIGHT
#    define SMAA_LUMA_WEIGHT float4(0.2126f, 0.7152f, 0.0722f, 0.0f)
#  endif

/**
 * Luma Edge Detection
 *
 * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colors, and
 * thus 'colorTex' should be a non-sRGB texture.
 */
float2 SMAALumaEdgeDetectionPS(float2 texcoord,
                               float4 offset[3],
                               SMAATexture2D(colorTex)
#  if SMAA_PREDICATION
                                   ,
                               SMAATexture2D(predicationTex)
#  endif
)
{
// Calculate the threshold:
#  if SMAA_PREDICATION
  float2 threshold = SMAACalculatePredicatedThreshold(
      texcoord, offset, SMAATexturePass2D(predicationTex));
#  else
  float2 threshold = float2(SMAA_THRESHOLD, SMAA_THRESHOLD);
#  endif

  // Calculate lumas:
  // float4 weights = float4(0.2126f, 0.7152f, 0.0722f, 0.0f);
  float4 weights = SMAA_LUMA_WEIGHT;
  float L = dot(SMAASamplePoint(colorTex, texcoord).rgba, weights);

  float Lleft = dot(SMAASamplePoint(colorTex, offset[0].xy).rgba, weights);
  float Ltop = dot(SMAASamplePoint(colorTex, offset[0].zw).rgba, weights);

  // We do the usual threshold:
  float4 delta;
  delta.xy = abs(L - float2(Lleft, Ltop));
  float2 edges = step(threshold, delta.xy);

#  ifdef GPU_FRAGMENT_SHADER
#    ifndef SMAA_NO_DISCARD
  // Then discard if there is no edge:
  if (dot(edges, float2(1.0f, 1.0f)) == 0.0f) {
    gpu_discard_fragment();
    return float2(0.0f, 0.0f);
  }
#    endif
#  elif defined(GPU_COMPUTE_SHADER)
  // Then return early if there is no edge:
  if (dot(edges, float2(1.0f, 1.0f)) == 0.0f) {
    return float2(0.0f);
  }
#  endif

  // Calculate right and bottom deltas:
  float Lright = dot(SMAASamplePoint(colorTex, offset[1].xy).rgba, weights);
  float Lbottom = dot(SMAASamplePoint(colorTex, offset[1].zw).rgba, weights);
  delta.zw = abs(L - float2(Lright, Lbottom));

  // Calculate the maximum delta in the direct neighborhood:
  float2 maxDelta = max(delta.xy, delta.zw);

  // Calculate left-left and top-top deltas:
  float Lleftleft = dot(SMAASamplePoint(colorTex, offset[2].xy).rgba, weights);
  float Ltoptop = dot(SMAASamplePoint(colorTex, offset[2].zw).rgba, weights);
  delta.zw = abs(float2(Lleft, Ltop) - float2(Lleftleft, Ltoptop));

  // Calculate the final maximum delta:
  maxDelta = max(maxDelta.xy, delta.zw);
  float finalDelta = max(maxDelta.x, maxDelta.y);

  // Local contrast adaptation:
  edges.xy *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);

  return edges;
}

/**
 * Color Edge Detection
 *
 * IMPORTANT NOTICE: color edge detection requires gamma-corrected colors, and
 * thus 'colorTex' should be a non-sRGB texture.
 */
float2 SMAAColorEdgeDetectionPS(float2 texcoord,
                                float4 offset[3],
                                SMAATexture2D(colorTex)
#  if SMAA_PREDICATION
                                    ,
                                SMAATexture2D(predicationTex)
#  endif
)
{
// Calculate the threshold:
#  if SMAA_PREDICATION
  float2 threshold = SMAACalculatePredicatedThreshold(texcoord, offset, predicationTex);
#  else
  float2 threshold = float2(SMAA_THRESHOLD, SMAA_THRESHOLD);
#  endif

  // Calculate color deltas:
  float4 delta;
  float3 C = SMAASamplePoint(colorTex, texcoord).rgb;

  float3 Cleft = SMAASamplePoint(colorTex, offset[0].xy).rgb;
  float3 t = abs(C - Cleft);
  delta.x = max(max(t.r, t.g), t.b);

  float3 Ctop = SMAASamplePoint(colorTex, offset[0].zw).rgb;
  t = abs(C - Ctop);
  delta.y = max(max(t.r, t.g), t.b);

  // We do the usual threshold:
  float2 edges = step(threshold, delta.xy);

#  ifndef SMAA_NO_DISCARD
#    ifdef GPU_FRAGMENT_SHADER
  // Then discard if there is no edge:
  if (dot(edges, float2(1.0f, 1.0f)) == 0.0f) {
    gpu_discard_fragment();
    return float2(0.0f, 0.0f);
  }
#    endif
#  endif

  // Calculate right and bottom deltas:
  float3 Cright = SMAASamplePoint(colorTex, offset[1].xy).rgb;
  t = abs(C - Cright);
  delta.z = max(max(t.r, t.g), t.b);

  float3 Cbottom = SMAASamplePoint(colorTex, offset[1].zw).rgb;
  t = abs(C - Cbottom);
  delta.w = max(max(t.r, t.g), t.b);

  // Calculate the maximum delta in the direct neighborhood:
  float2 maxDelta = max(delta.xy, delta.zw);

  // Calculate left-left and top-top deltas:
  float3 Cleftleft = SMAASamplePoint(colorTex, offset[2].xy).rgb;
  t = abs(C - Cleftleft);
  delta.z = max(max(t.r, t.g), t.b);

  float3 Ctoptop = SMAASamplePoint(colorTex, offset[2].zw).rgb;
  t = abs(C - Ctoptop);
  delta.w = max(max(t.r, t.g), t.b);

  // Calculate the final maximum delta:
  maxDelta = max(maxDelta.xy, delta.zw);
  float finalDelta = max(maxDelta.x, maxDelta.y);

  // Local contrast adaptation:
  edges.xy *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);

  return edges;
}

/**
 * Depth Edge Detection
 */
float2 SMAADepthEdgeDetectionPS(float2 texcoord, float4 offset[3], SMAATexture2D(depthTex))
{
  float3 neighbors = SMAAGatherNeighbors(texcoord, offset, SMAATexturePass2D(depthTex));
  float2 delta = abs(neighbors.xx - float2(neighbors.y, neighbors.z));
  float2 edges = step(SMAA_DEPTH_THRESHOLD, delta);

#  ifdef GPU_FRAGMENT_SHADER
  if (dot(edges, float2(1.0f, 1.0f)) == 0.0f) {
    gpu_discard_fragment();
    return float2(0.0f, 0.0f);
  }
#  endif

  return edges;
}

/* ----------------------------------------------------------------------------
 * Diagonal Search Functions */

#  if !defined(SMAA_DISABLE_DIAG_DETECTION)

/**
 * Allows to decode two binary values from a bilinear-filtered access.
 */
float2 SMAADecodeDiagBilinearAccess(float2 e)
{
  // Bilinear access for fetching 'e' have a 0.25f offset, and we are
  // interested in the R and G edges:
  //
  // +---G---+-------+
  // |   x o R   x   |
  // +-------+-------+
  //
  // Then, if one of these edge is enabled:
  //   Red:   (0.75f * X + 0.25f * 1) => 0.25f or 1.0f
  //   Green: (0.75f * 1 + 0.25f * X) => 0.75f or 1.0f
  //
  // This function will unpack the values (mad + mul + round):
  // wolframalpha.com: round(x * abs(5 * x - 5 * 0.75f)) plot 0 to 1
  e.r = e.r * abs(5.0f * e.r - 5.0f * 0.75f);
  return round(e);
}

float4 SMAADecodeDiagBilinearAccess(float4 e)
{
  e.rb = e.rb * abs(5.0f * e.rb - 5.0f * 0.75f);
  return round(e);
}

/**
 * These functions allows to perform diagonal pattern searches.
 */
float2 SMAASearchDiag1(SMAATexture2D(edgesTex), float2 texcoord, float2 dir, out float2 e)
{
  float4 coord = float4(texcoord, -1.0f, 1.0f);
  float3 t = float3(SMAA_RT_METRICS.xy, 1.0f);
  while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9f) {
    coord.xyz = mad(t, float3(dir, 1.0f), coord.xyz);
    e = SMAASampleLevelZero(edgesTex, coord.xy).rg;
    coord.w = dot(e, float2(0.5f, 0.5f));
  }
  return coord.zw;
}

float2 SMAASearchDiag2(SMAATexture2D(edgesTex), float2 texcoord, float2 dir, out float2 e)
{
  float4 coord = float4(texcoord, -1.0f, 1.0f);
  coord.x += 0.25f * SMAA_RT_METRICS.x;  // See @SearchDiag2Optimization
  float3 t = float3(SMAA_RT_METRICS.xy, 1.0f);
  while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9f) {
    coord.xyz = mad(t, float3(dir, 1.0f), coord.xyz);

    // @SearchDiag2Optimization
    // Fetch both edges at once using bilinear filtering:
    e = SMAASampleLevelZero(edgesTex, coord.xy).rg;
    e = SMAADecodeDiagBilinearAccess(e);

    // Non-optimized version:
    // e.g = SMAASampleLevelZero(edgesTex, coord.xy).g;
    // e.r = SMAASampleLevelZeroOffset(edgesTex, coord.xy, int2(1, 0)).r;

    coord.w = dot(e, float2(0.5f, 0.5f));
  }
  return coord.zw;
}

/**
 * Similar to SMAAArea, this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
float2 SMAAAreaDiag(SMAATexture2D(areaTex), float2 dist, float2 e, float offset)
{
  float2 texcoord = mad(
      float2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);

  // We do a scale and bias for mapping to texel space:
  texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5f * SMAA_AREATEX_PIXEL_SIZE);

  // Diagonal areas are on the second half of the texture:
  texcoord.x += 0.5f;

  // Move to proper place, according to the subpixel offset:
  texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;

  // Do it!
  return SMAA_AREATEX_SELECT(SMAASampleLevelZero(areaTex, texcoord));
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
float2 SMAACalculateDiagWeights(SMAATexture2D(edgesTex),
                                SMAATexture2D(areaTex),
                                float2 texcoord,
                                float2 e,
                                float4 subsampleIndices)
{
  float2 weights = float2(0.0f, 0.0f);

  // Search for the line ends:
  float4 d;
  float2 end;
  if (e.r > 0.0f) {
    d.xz = SMAASearchDiag1(SMAATexturePass2D(edgesTex), texcoord, float2(-1.0f, 1.0f), end);
    d.x += float(end.y > 0.9f);
  }
  else
    d.xz = float2(0.0f, 0.0f);
  d.yw = SMAASearchDiag1(SMAATexturePass2D(edgesTex), texcoord, float2(1.0f, -1.0f), end);

  SMAA_BRANCH
  if (d.x + d.y > 2.0f) {  // d.x + d.y + 1 > 3
    // Fetch the crossing edges:
    float4 coords = mad(
        float4(-d.x + 0.25f, d.x, d.y, -d.y - 0.25f), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
    float4 c;
    c.xy = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1, 0)).rg;
    c.zw = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2(1, 0)).rg;
    c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);

    // Non-optimized version:
    // float4 coords = mad(float4(-d.x, d.x, d.y, -d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
    // float4 c;
    // c.x = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1,  0)).g;
    // c.y = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2( 0,  0)).r;
    // c.z = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1,  0)).g;
    // c.w = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1, -1)).r;

    // Merge crossing edges at each side into a single value:
    float2 cc = mad(float2(2.0f, 2.0f), c.xz, c.yw);

    // Remove the crossing edge if we didn't found the end of the line:
    SMAAMovc(bool2(step(0.9f, d.zw)), cc, float2(0.0f, 0.0f));

    // Fetch the areas for this line:
    weights += SMAAAreaDiag(SMAATexturePass2D(areaTex), d.xy, cc, subsampleIndices.z);
  }

  // Search for the line ends:
  d.xz = SMAASearchDiag2(SMAATexturePass2D(edgesTex), texcoord, float2(-1.0f, -1.0f), end);
  if (SMAASampleLevelZeroOffset(edgesTex, texcoord, int2(1, 0)).r > 0.0f) {
    d.yw = SMAASearchDiag2(SMAATexturePass2D(edgesTex), texcoord, float2(1.0f, 1.0f), end);
    d.y += float(end.y > 0.9f);
  }
  else
    d.yw = float2(0.0f, 0.0f);

  SMAA_BRANCH
  if (d.x + d.y > 2.0f) {  // d.x + d.y + 1 > 3
    // Fetch the crossing edges:
    float4 coords = mad(float4(-d.x, -d.x, d.y, d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
    float4 c;
    c.x = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1, 0)).g;
    c.y = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(0, -1)).r;
    c.zw = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2(1, 0)).gr;
    float2 cc = mad(float2(2.0f, 2.0f), c.xz, c.yw);

    // Remove the crossing edge if we didn't found the end of the line:
    SMAAMovc(bool2(step(0.9f, d.zw)), cc, float2(0.0f, 0.0f));

    // Fetch the areas for this line:
    weights += SMAAAreaDiag(SMAATexturePass2D(areaTex), d.xy, cc, subsampleIndices.w).gr;
  }

  return weights;
}
#  endif

/* ----------------------------------------------------------------------------
 * Horizontal/Vertical Search Functions */

/**
 * This allows to determine how much length should we add in the last step
 * of the searches. It takes the bilinearly interpolated edge (see
 * @PSEUDO_GATHER4), and adds 0, 1 or 2, depending on which edges and
 * crossing edges are active.
 */
float SMAASearchLength(SMAATexture2D(searchTex), float2 e, float offset)
{
  // The texture is flipped vertically, with left and right cases taking half
  // of the space horizontally:
  float2 scale = SMAA_SEARCHTEX_SIZE * float2(0.5f, -1.0f);
  float2 bias = SMAA_SEARCHTEX_SIZE * float2(offset, 1.0f);

  // Scale and bias to access texel centers:
  scale += float2(-1.0f, 1.0f);
  bias += float2(0.5f, -0.5f);

  // Convert from pixel coordinates to texcoords:
  // (We use SMAA_SEARCHTEX_PACKED_SIZE because the texture is cropped)
  scale *= 1.0f / SMAA_SEARCHTEX_PACKED_SIZE;
  bias *= 1.0f / SMAA_SEARCHTEX_PACKED_SIZE;

  // Lookup the search texture:
  return SMAA_SEARCHTEX_SELECT(SMAASampleLevelZero(searchTex, mad(scale, e, bias)));
}

/**
 * Horizontal/vertical search functions for the 2nd pass.
 */
float SMAASearchXLeft(SMAATexture2D(edgesTex),
                      SMAATexture2D(searchTex),
                      float2 texcoord,
                      float end)
{
  /**
   * @PSEUDO_GATHER4
   * This texcoord has been offset by (-0.25, -0.125) in the vertex shader to
   * sample between edge, thus fetching four edges in a row.
   * Sampling with different offsets in each direction allows to disambiguate
   * which edges are active from the four fetched ones.
   */
  float2 e = float2(0.0f, 1.0f);
  while (texcoord.x > end && e.g > 0.8281f &&  // Is there some edge not activated?
         e.r == 0.0f)                          // Or is there a crossing edge that breaks the line?
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).rg;
    texcoord = mad(-float2(2.0f, 0.0f), SMAA_RT_METRICS.xy, texcoord);
  }

  float offset = mad(
      -(255.0f / 127.0f), SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.0f), 3.25f);
  return mad(SMAA_RT_METRICS.x, offset, texcoord.x);

  // Non-optimized version:
  // We correct the previous (-0.25f, -0.125f) offset we applied:
  // texcoord.x += 0.25f * SMAA_RT_METRICS.x;

  // The searches are bias by 1, so adjust the coords accordingly:
  // texcoord.x += SMAA_RT_METRICS.x;

  // Disambiguate the length added by the last step:
  // texcoord.x += 2.0f * SMAA_RT_METRICS.x; // Undo last step
  // texcoord.x -= SMAA_RT_METRICS.x * (255.0f / 127.0f) *
  // SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.0f); return mad(SMAA_RT_METRICS.x, offset,
  // texcoord.x);
}

float SMAASearchXRight(SMAATexture2D(edgesTex),
                       SMAATexture2D(searchTex),
                       float2 texcoord,
                       float end)
{
  float2 e = float2(0.0f, 1.0f);
  while (texcoord.x < end && e.g > 0.8281f &&  // Is there some edge not activated?
         e.r == 0.0f)                          // Or is there a crossing edge that breaks the line?
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).rg;
    texcoord = mad(float2(2.0f, 0.0f), SMAA_RT_METRICS.xy, texcoord);
  }
  float offset = mad(
      -(255.0f / 127.0f), SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.5f), 3.25f);
  return mad(-SMAA_RT_METRICS.x, offset, texcoord.x);
}

float SMAASearchYUp(SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end)
{
  float2 e = float2(1.0f, 0.0f);
  while (texcoord.y > end && e.r > 0.8281f &&  // Is there some edge not activated?
         e.g == 0.0f)                          // Or is there a crossing edge that breaks the line?
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).rg;
    texcoord = mad(-float2(0.0f, 2.0f), SMAA_RT_METRICS.xy, texcoord);
  }
  float offset = mad(
      -(255.0f / 127.0f), SMAASearchLength(SMAATexturePass2D(searchTex), e.gr, 0.0f), 3.25f);
  return mad(SMAA_RT_METRICS.y, offset, texcoord.y);
}

float SMAASearchYDown(SMAATexture2D(edgesTex),
                      SMAATexture2D(searchTex),
                      float2 texcoord,
                      float end)
{
  float2 e = float2(1.0f, 0.0f);
  while (texcoord.y < end && e.r > 0.8281f &&  // Is there some edge not activated?
         e.g == 0.0f)                          // Or is there a crossing edge that breaks the line?
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).rg;
    texcoord = mad(float2(0.0f, 2.0f), SMAA_RT_METRICS.xy, texcoord);
  }
  float offset = mad(
      -(255.0f / 127.0f), SMAASearchLength(SMAATexturePass2D(searchTex), e.gr, 0.5f), 3.25f);
  return mad(-SMAA_RT_METRICS.y, offset, texcoord.y);
}

/**
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
float2 SMAAArea(SMAATexture2D(areaTex), float2 dist, float e1, float e2, float offset)
{
  // Rounding prevents precision errors of bilinear filtering:
  float2 texcoord = mad(float2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE),
                        round(4.0f * float2(e1, e2)),
                        dist);

  // We do a scale and bias for mapping to texel space:
  texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5f * SMAA_AREATEX_PIXEL_SIZE);

  // Move to proper place, according to the subpixel offset:
  texcoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, offset, texcoord.y);

  // Do it!
  return SMAA_AREATEX_SELECT(SMAASampleLevelZero(areaTex, texcoord));
}

/* ----------------------------------------------------------------------------
 * Corner Detection Functions */

void SMAADetectHorizontalCornerPattern(SMAATexture2D(edgesTex),
                                       inout float2 weights,
                                       float4 texcoord,
                                       float2 d)
{
#  if !defined(SMAA_DISABLE_CORNER_DETECTION)
  float2 leftRight = step(d.xy, d.yx);
  float2 rounding = (1.0f - SMAA_CORNER_ROUNDING_NORM) * leftRight;

  rounding /= leftRight.x + leftRight.y;  // Reduce blending for pixels in the center of a line.

  float2 factor = float2(1.0f, 1.0f);
  factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(0, 1)).r;
  factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(1, 1)).r;
  factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(0, -2)).r;
  factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(1, -2)).r;

  weights *= saturate(factor);
#  endif
}

void SMAADetectVerticalCornerPattern(SMAATexture2D(edgesTex),
                                     inout float2 weights,
                                     float4 texcoord,
                                     float2 d)
{
#  if !defined(SMAA_DISABLE_CORNER_DETECTION)
  float2 leftRight = step(d.xy, d.yx);
  float2 rounding = (1.0f - SMAA_CORNER_ROUNDING_NORM) * leftRight;

  rounding /= leftRight.x + leftRight.y;

  float2 factor = float2(1.0f, 1.0f);
  factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(1, 0)).g;
  factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(1, 1)).g;
  factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, int2(-2, 0)).g;
  factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, int2(-2, 1)).g;

  weights *= saturate(factor);
#  endif
}

/* ----------------------------------------------------------------------------
 * Blending Weight Calculation Pixel Shader (Second Pass) */

float4 SMAABlendingWeightCalculationPS(float2 texcoord,
                                       float2 pixcoord,
                                       float4 offset[3],
                                       SMAATexture2D(edgesTex),
                                       SMAATexture2D(areaTex),
                                       SMAATexture2D(searchTex),
                                       float4 subsampleIndices)
{  // Just pass zero for SMAA 1x, see @SUBSAMPLE_INDICES.
  float4 weights = float4(0.0f, 0.0f, 0.0f, 0.0f);

  float2 e = SMAASample(edgesTex, texcoord).rg;

  SMAA_BRANCH
  if (e.g > 0.0f) {  // Edge at north
#  if !defined(SMAA_DISABLE_DIAG_DETECTION)
    // Diagonals have both north and west edges, so searching for them in
    // one of the boundaries is enough.
    weights.rg = SMAACalculateDiagWeights(
        SMAATexturePass2D(edgesTex), SMAATexturePass2D(areaTex), texcoord, e, subsampleIndices);

    // We give priority to diagonals, so if we find a diagonal we skip
    // horizontal/vertical processing.
    SMAA_BRANCH
    if (weights.r == -weights.g) {  // weights.r + weights.g == 0.0f
#  endif

      float2 d;

      // Find the distance to the left:
      float3 coords;
      coords.x = SMAASearchXLeft(
          SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[0].xy, offset[2].x);
      coords.y =
          offset[1].y;  // offset[1].y = texcoord.y - 0.25f * SMAA_RT_METRICS.y (@CROSSING_OFFSET)
      d.x = coords.x;

      // Now fetch the left crossing edges, two at a time using bilinear
      // filtering. Sampling at -0.25f (see @CROSSING_OFFSET) enables to
      // discern what value each edge has:
      float e1 = SMAASampleLevelZero(edgesTex, coords.xy).r;

      // Find the distance to the right:
      coords.z = SMAASearchXRight(
          SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[0].zw, offset[2].y);
      d.y = coords.z;

      // We want the distances to be in pixel units (doing this here allows
      // better interleaving of arithmetic and memory accesses):
      d = abs(round(mad(SMAA_RT_METRICS.zz, d, -pixcoord.xx)));

      // SMAAArea below needs a sqrt, as the areas texture is compressed
      // quadratically:
      float2 sqrt_d = sqrt(d);

      // Fetch the right crossing edges:
      float e2 = SMAASampleLevelZeroOffset(edgesTex, coords.zy, int2(1, 0)).r;

      // Ok, we know how this pattern looks like, now it is time for getting
      // the actual area:
      weights.rg = SMAAArea(SMAATexturePass2D(areaTex), sqrt_d, e1, e2, subsampleIndices.y);

      // Fix corners:
      coords.y = texcoord.y;

#  ifdef GPU_METAL
      /* Partial vector references are unsupported in MSL. */
      float2 _weights = weights.rg;
      SMAADetectHorizontalCornerPattern(SMAATexturePass2D(edgesTex), _weights, coords.xyzy, d);
      weights.rg = _weights;
#  else
    SMAADetectHorizontalCornerPattern(SMAATexturePass2D(edgesTex), weights.rg, coords.xyzy, d);
#  endif

#  if !defined(SMAA_DISABLE_DIAG_DETECTION)
    }
    else
      e.r = 0.0f;  // Skip vertical processing.
#  endif
  }

  SMAA_BRANCH
  if (e.r > 0.0f) {  // Edge at west
    float2 d;

    // Find the distance to the top:
    float3 coords;
    coords.y = SMAASearchYUp(
        SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[1].xy, offset[2].z);
    coords.x = offset[0].x;  // offset[1].x = texcoord.x - 0.25f * SMAA_RT_METRICS.x;
    d.x = coords.y;

    // Fetch the top crossing edges:
    float e1 = SMAASampleLevelZero(edgesTex, coords.xy).g;

    // Find the distance to the bottom:
    coords.z = SMAASearchYDown(
        SMAATexturePass2D(edgesTex), SMAATexturePass2D(searchTex), offset[1].zw, offset[2].w);
    d.y = coords.z;

    // We want the distances to be in pixel units:
    d = abs(round(mad(SMAA_RT_METRICS.ww, d, -pixcoord.yy)));

    // SMAAArea below needs a sqrt, as the areas texture is compressed
    // quadratically:
    float2 sqrt_d = sqrt(d);

    // Fetch the bottom crossing edges:
    float e2 = SMAASampleLevelZeroOffset(edgesTex, coords.xz, int2(0, 1)).g;

    // Get the area for this direction:
    weights.zw = SMAAArea(SMAATexturePass2D(areaTex), sqrt_d, e1, e2, subsampleIndices.x);

    // Fix corners:
    coords.x = texcoord.x;

#  ifdef GPU_METAL
    /* Partial vector references are unsupported in MSL. */
    float2 _weights = weights.zw;
    SMAADetectVerticalCornerPattern(SMAATexturePass2D(edgesTex), _weights, coords.xyxz, d);
    weights.zw = _weights;
#  else
    SMAADetectVerticalCornerPattern(SMAATexturePass2D(edgesTex), weights.zw, coords.xyxz, d);
#  endif
  }

  return weights;
}

/* ----------------------------------------------------------------------------
 * Neighborhood Blending Pixel Shader (Third Pass) */

float4 SMAANeighborhoodBlendingPS(float2 texcoord,
                                  float4 offset,
                                  SMAATexture2D(colorTex),
                                  SMAATexture2D(blendTex)
#  if SMAA_REPROJECTION
                                      ,
                                  SMAATexture2D(velocityTex)
#  endif
)
{
  // Fetch the blending weights for current pixel:
  float4 a;
  a.x = SMAASample(blendTex, offset.xy).a;   // Right
  a.y = SMAASample(blendTex, offset.zw).g;   // Top
  a.wz = SMAASample(blendTex, texcoord).xz;  // Bottom / Left

  // Is there any blending weight with a value greater than 0.0f?
  SMAA_BRANCH
  if (dot(a, float4(1.0f, 1.0f, 1.0f, 1.0f)) < 1e-5f) {
    float4 color = SMAASampleLevelZero(colorTex, texcoord);

#  if SMAA_REPROJECTION
    float2 velocity = SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, texcoord));

    // Pack velocity into the alpha channel:
    color.a = sqrt(5.0f * length(velocity));
#  endif

    return color;
  }
  else {
    bool h = max(a.x, a.z) > max(a.y, a.w);  // max(horizontal) > max(vertical)

    // Calculate the blending offsets:
    float4 blendingOffset = float4(0.0f, a.y, 0.0f, a.w);
    float2 blendingWeight = a.yw;
    SMAAMovc(bool4(h, h, h, h), blendingOffset, float4(a.x, 0.0f, a.z, 0.0f));
    SMAAMovc(bool2(h, h), blendingWeight, a.xz);
    blendingWeight /= dot(blendingWeight, float2(1.0f, 1.0f));

    // Calculate the texture coordinates:
    float4 blendingCoord = mad(
        blendingOffset, float4(SMAA_RT_METRICS.xy, -SMAA_RT_METRICS.xy), texcoord.xyxy);

    // We exploit bilinear filtering to mix current pixel with the chosen
    // neighbor:
    float4 color = blendingWeight.x * SMAASampleLevelZero(colorTex, blendingCoord.xy);
    color += blendingWeight.y * SMAASampleLevelZero(colorTex, blendingCoord.zw);

#  if SMAA_REPROJECTION
    // Antialias velocity for proper reprojection in a later stage:
    float2 velocity = blendingWeight.x *
                      SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, blendingCoord.xy));
    velocity += blendingWeight.y *
                SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, blendingCoord.zw));

    // Pack velocity into the alpha channel:
    color.a = sqrt(5.0f * length(velocity));
#  endif

    return color;
  }
}

/* ----------------------------------------------------------------------------
 * Temporal Resolve Pixel Shader (Optional Pass) */

float4 SMAAResolvePS(float2 texcoord,
                     SMAATexture2D(currentColorTex),
                     SMAATexture2D(previousColorTex)
#  if SMAA_REPROJECTION
                         ,
                     SMAATexture2D(velocityTex)
#  endif
)
{
#  if SMAA_REPROJECTION
  // Velocity is assumed to be calculated for motion blur, so we need to
  // inverse it for reprojection:
  float2 velocity = -SMAA_DECODE_VELOCITY(SMAASamplePoint(velocityTex, texcoord).rg);

  // Fetch current pixel:
  float4 current = SMAASamplePoint(currentColorTex, texcoord);

  // Reproject current coordinates and fetch previous pixel:
  float4 previous = SMAASamplePoint(previousColorTex, texcoord + velocity);

  // Attenuate the previous pixel if the velocity is different:
  float delta = abs(current.a * current.a - previous.a * previous.a) / 5.0f;
  float weight = 0.5f * saturate(1.0f - sqrt(delta) * SMAA_REPROJECTION_WEIGHT_SCALE);

  // Blend the pixels according to the calculated weight:
  return lerp(current, previous, weight);
#  else
  // Just blend the pixels:
  float4 current = SMAASamplePoint(currentColorTex, texcoord);
  float4 previous = SMAASamplePoint(previousColorTex, texcoord);
  return lerp(current, previous, 0.5f);
#  endif
}

/* ----------------------------------------------------------------------------
 * Separate Multisamples Pixel Shader (Optional Pass) */

#  ifdef SMAALoad
void SMAASeparatePS(float4 position,
                    float2 texcoord,
                    out float4 target0,
                    out float4 target1,
                    SMAATexture2DMS2(colorTexMS))
{
  int2 pos = int2(position.xy);
  target0 = SMAALoad(colorTexMS, pos, 0);
  target1 = SMAALoad(colorTexMS, pos, 1);
}
#  endif

/* ---------------------------------------------------------------------------- */
#endif  // SMAA_INCLUDE_PS
