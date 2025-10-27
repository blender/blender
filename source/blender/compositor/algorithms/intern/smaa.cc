/* SPDX-FileCopyrightText: 2013 Jorge Jimenez <jorge@iryoku.com>
 * SPDX-FileCopyrightText: 2013 Jose I. Echevarria <joseignacioechevarria@gmail.com>
 * SPDX-FileCopyrightText: 2013 Belen Masia <bmasia@unizar.es>
 * SPDX-FileCopyrightText: 2013 Fernando Navarro <fernandn@microsoft.com>
 * SPDX-FileCopyrightText: 2013 Diego Gutierrez <diegog@unizar.es>
 * SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: MIT AND GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_vector.hh"

#include "IMB_colormanagement.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_smaa.hh"

#include "COM_smaa_precomputed_textures.hh"

namespace blender::compositor {

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
 * over-sized triangles instead of quads to avoid over-shading along the
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
 * For quick-starters: just use luma edge detection.
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
 *     will perform anti-aliasing in gamma space.
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
 *         #define SMAA_RT_METRICS float4(1.0 / 1280.0, 1.0 / 720.0, 1280.0, 720.0)
 *         #define SMAA_HLSL_4
 *         #define SMAA_PRESET_HIGH
 *         #include "SMAA.h"
 *
 *     Note that SMAA_RT_METRICS doesn't need to be a macro, it can be a
 *     uniform variable. The code is designed to minimize the impact of not
 *     using a constant value, but it is still better to hard-code it.
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
 * 1. The first step is to render using sub-pixel jitters. I won't go into
 *    detail, but it's as simple as moving each vertex position in the
 *    vertex shader, you can check how we do it in our DX10 demo.
 *
 * 2. Then, you must setup the temporal resolve. You may want to take a look
 *    into SMAAResolve for resolving 2x modes. After you get it working, you'll
 *    probably see ghosting everywhere. But fear not, you can enable the
 *    CryENGINE temporal reprojection by setting the SMAA_REPROJECTION macro.
 *    Check out SMAA_DECODE_VELOCITY if your velocity buffer is encoded.
 *
 * 3. The next step is to apply SMAA to each sub-pixel jittered frame, just as
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
 *    |  0 |  ( 0.25, -0.25)  |  float4(1, 1, 1, 0)  |
 *    |  1 |  (-0.25,  0.25)  |  float4(2, 2, 2, 0)  |
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
 *    |  0 |  0 |  ( 0.125,  0.125)  |  ( 0.375, -0.125) |  float4(5, 3, 1, 3)  |
 *    |  0 |  1 |  ( 0.125,  0.125)  |  (-0.125,  0.375) |  float4(4, 6, 2, 3)  |
 *    +----+----+--------------------+-------------------+----------------------+
 *    |  1 |  2 |  (-0.125, -0.125)  |  ( 0.125, -0.375) |  float4(3, 5, 1, 4)  |
 *    |  1 |  3 |  (-0.125, -0.125)  |  (-0.375,  0.125) |  float4(6, 4, 2, 4)  |
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
 * Blender's Defines */

#define SMAA_CUSTOM_SL
#define SMAA_AREATEX_SELECT(sample) sample.xy()
#define SMAA_SEARCHTEX_SELECT(sample) sample.x
#define SMAATexture2D(tex) const Result &tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex, coord) tex.sample_bilinear_extended(coord)
#define SMAASampleLevelZeroPoint(tex, coord) tex.sample_bilinear_extended(coord)
#define SMAASampleLevelZeroOffset(tex, coord, offset, size) \
  tex.sample_bilinear_extended(coord + float2(offset) / float2(size))
#define SMAASample(tex, coord) tex.sample_bilinear_extended(coord)
#define SMAASamplePoint(tex, coord) tex.sample_nearest_extended(coord)
#define SMAASamplePointOffset(tex, coord, offset, size) \
  tex.sample_nearest_extended(coord + float2(offset) / float2(size))
#define SMAASampleOffset(tex, coord, offset, size) \
  tex.sample_bilinear_extended(coord + float2(offset) / float2(size))
#define SMAA_FLATTEN
#define SMAA_BRANCH
#define lerp(a, b, t) math::interpolate(a, b, t)
#define saturate(a) math::clamp(a, 0.0f, 1.0f)
#define mad(a, b, c) (a * b + c)

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
 * Range: [0, 0.5]
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
 * longer lines won't look as good, but still anti-aliased).
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
#  define SMAASampleLevelZero(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))
#  define SMAASampleLevelZeroPoint(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))
/* clang-format off */
#  define SMAASampleLevelZeroOffset(tex, coord, offset) tex2Dlod(tex, float4(coord + offset * SMAA_RT_METRICS.xy, 0.0, 0.0))
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
#  define SMAASampleLevelZero(tex, coord) textureLod(tex, coord, 0.0)
#  define SMAASampleLevelZeroPoint(tex, coord) textureLod(tex, coord, 0.0)
#  define SMAASampleLevelZeroOffset(tex, coord, offset) textureLodOffset(tex, coord, 0.0, offset)
#  define SMAASample(tex, coord) texture(tex, coord)
#  define SMAASamplePoint(tex, coord) texture(tex, coord)
#  define SMAASampleOffset(tex, coord, offset) texture(tex, coord, offset)
#  define SMAA_FLATTEN
#  define SMAA_BRANCH
#  define lerp(a, b, t) mix(a, b, t)
#  define saturate(a) clamp(a, 0.0, 1.0)
#  if defined(SMAA_GLSL_4)
#    define SMAAGather(tex, coord) textureGather(tex, coord)
#  endif
#  if defined(SMAA_GLSL_4)
#    define mad(a, b, c) fma(a, b, c)
#  elif defined(GPU_VULKAN)
/* NOTE(Vulkan) mad macro doesn't work, define each override as work-around. */
vec4 mad(vec4 a, vec4 b, vec4 c)
{
  return fma(a, b, c);
}
vec3 mad(vec3 a, vec3 b, vec3 c)
{
  return fma(a, b, c);
}
vec2 mad(vec2 a, vec2 b, vec2 c)
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
/* NOTE(Metal): Types already natively declared in MSL. */
#  ifndef GPU_METAL
#    define float2 vec2
#    define float3 vec3
#    define float4 vec4
#    define int2 ivec2
#    define int3 ivec3
#    define int4 ivec4
#    define bool2 bvec2
#    define bool3 bvec3
#    define bool4 bvec4
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
 * Conditional move:
 */
static void SMAAMovc(float2 cond, float2 &variable, float2 value)
{
  /* Use select function (select(genType A, genType B, genBType cond)). */
  variable = math::interpolate(variable, value, cond);
}

static void SMAAMovc(float4 cond, float4 &variable, float4 value)
{
  /* Use select function (select(genType A, genType B, genBType cond)). */
  variable = math::interpolate(variable, value, cond);
}

#if SMAA_INCLUDE_VS
/* ----------------------------------------------------------------------------
 * Vertex Shaders */

/**
 * Edge Detection Vertex Shader
 */
static void SMAAEdgeDetectionVS(float2 texcoord, int2 size, float4 offset[3])
{
  offset[0] = float4(texcoord.xy(), texcoord.xy()) +
              float4(-1.0f, 0.0f, 0.0f, -1.0f) / float4(size, size);
  offset[1] = float4(texcoord.xy(), texcoord.xy()) +
              float4(1.0f, 0.0f, 0.0f, 1.0f) / float4(size, size);
  offset[2] = float4(texcoord.xy(), texcoord.xy()) +
              float4(-2.0f, 0.0f, 0.0f, -2.0f) / float4(size, size);
}

/**
 * Blend Weight Calculation Vertex Shader
 */
static void SMAABlendingWeightCalculationVS(float2 texcoord,
                                            int2 size,
                                            float2 &pixcoord,
                                            float4 offset[3])
{
  pixcoord = texcoord * float2(size);

  /* We will use these offsets for the searches later on (see @PSEUDO_GATHER4): */
  offset[0] = float4(texcoord.xy(), texcoord.xy()) +
              float4(-0.25f, -0.125f, 1.25f, -0.125f) / float4(size, size);
  offset[1] = float4(texcoord.xy(), texcoord.xy()) +
              float4(-0.125f, -0.25f, -0.125f, 1.25f) / float4(size, size);

  /* And these for the searches, they indicate the ends of the loops: */
  offset[2] = float4(offset[0].x, offset[0].z, offset[1].y, offset[1].w) +
              (float4(-2.0f, 2.0f, -2.0f, 2.0f) * float(SMAA_MAX_SEARCH_STEPS)) /
                  float4(float2(size.x), float2(size.y));
}

/**
 * Neighborhood Blending Vertex Shader
 */
static void SMAANeighborhoodBlendingVS(float2 texcoord, int2 size, float4 &offset)
{
  offset = float4(texcoord, texcoord) + float4(1.0f, 0.0f, 0.0f, 1.0f) / float4(size, size);
}
#endif /* SMAA_INCLUDE_VS */

/**
 * Luma Edge Detection
 *
 * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colors, and
 * thus 'colorTex' should be a non-sRGB texture.
 */
static float2 SMAALumaEdgeDetectionPS(float2 texcoord,
                                      float4 offset[3],
                                      SMAATexture2D(colorTex),
#if SMAA_PREDICATION
                                      SMAATexture2D(predicationTex),
#endif
                                      float edge_threshold,
                                      float3 luminance_coefficients,
                                      float local_contrast_adaptation_factor)
{
#if SMAA_PREDICATION
  float2 threshold = SMAACalculatePredicatedThreshold(
      texcoord, offset, SMAATexturePass2D(predicationTex));
#else
  /* Calculate the threshold: */
  float2 threshold = float2(edge_threshold, edge_threshold);
#endif

  /* Calculate lumas: */
  // float4 weights = float4(0.2126, 0.7152, 0.0722, 0.0);
  float4 weights = float4(luminance_coefficients, 0.0f);
  float L = math::dot(SMAASamplePoint(colorTex, texcoord), weights);

  float Lleft = math::dot(SMAASamplePoint(colorTex, offset[0].xy()), weights);
  float Ltop = math::dot(SMAASamplePoint(colorTex, offset[0].zw()), weights);

  /* We do the usual threshold: */
  float4 delta;
  float2 delta_left_top = math::abs(L - float2(Lleft, Ltop));
  delta.x = delta_left_top.x;
  delta.y = delta_left_top.y;
  float2 edges = math::step(threshold, delta.xy());

  /* Then return early if there is no edge: */
  if (math::dot(edges, float2(1.0f, 1.0f)) == 0.0f) {
    return float2(0.0f);
  }

  /* Calculate right and bottom deltas: */
  float Lright = math::dot(SMAASamplePoint(colorTex, offset[1].xy()), weights);
  float Lbottom = math::dot(SMAASamplePoint(colorTex, offset[1].zw()), weights);
  float2 delta_right_bottom = math::abs(L - float2(Lright, Lbottom));
  delta.z = delta_right_bottom.x;
  delta.w = delta_right_bottom.y;

  /* Calculate the maximum delta in the direct neighborhood: */
  float2 maxDelta = math::max(delta.xy(), delta.zw());

  /* Calculate left-left and top-top deltas: */
  float Lleftleft = math::dot(SMAASamplePoint(colorTex, offset[2].xy()), weights);
  float Ltoptop = math::dot(SMAASamplePoint(colorTex, offset[2].zw()), weights);
  float2 delta_left_left_top_top = math::abs(float2(Lleft, Ltop) - float2(Lleftleft, Ltoptop));
  delta.z = delta_left_left_top_top.x;
  delta.w = delta_left_left_top_top.y;

  /* Calculate the final maximum delta: */
  maxDelta = math::max(maxDelta.xy(), delta.zw());
  float finalDelta = math::max(maxDelta.x, maxDelta.y);

  /* Local contrast adaptation: */
  edges *= math::step(finalDelta, local_contrast_adaptation_factor * delta.xy());

  return edges;
}

/* ----------------------------------------------------------------------------
 * Diagonal Search Functions */

#if !defined(SMAA_DISABLE_DIAG_DETECTION)

/**
 * Allows to decode two binary values from a bilinear-filtered access.
 */
static float2 SMAADecodeDiagBilinearAccess(float2 e)
{
  /* Bilinear access for fetching 'e' have a 0.25 offset, and we are
   * interested in the R and G edges:
   *
   * +---G---+-------+
   * |   x o R   x   |
   * +-------+-------+
   *
   * Then, if one of these edge is enabled:
   *   Red:   `(0.75 * X + 0.25 * 1) => 0.25 or 1.0`
   *   Green: `(0.75 * 1 + 0.25 * X) => 0.75 or 1.0`
   *
   * This function will unpack the values `(mad + mul + round)`:
   * wolframalpha.com: `round(x * abs(5 * x - 5 * 0.75))` plot 0 to 1
   */
  e.x = e.x * math::abs(5.0f * e.x - 5.0f * 0.75f);
  return math::round(e);
}

static float4 SMAADecodeDiagBilinearAccess(float4 e)
{
  e.x = e.x * math::abs(5.0f * e.x - 5.0f * 0.75f);
  e.z = e.z * math::abs(5.0f * e.z - 5.0f * 0.75f);
  return math::round(e);
}

/**
 * These functions allows to perform diagonal pattern searches.
 */
static float2 SMAASearchDiag1(
    SMAATexture2D(edgesTex), float2 texcoord, float2 dir, int2 size, float2 &e)
{
  float4 coord = float4(texcoord, -1.0f, 1.0f);
  float3 t = float3(1.0f / float2(size), 1.0f);
  while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9f) {
    float3 increment = mad(t, float3(dir, 1.0f), coord.xyz());
    coord.x = increment.x;
    coord.y = increment.y;
    coord.z = increment.z;
    e = SMAASamplePoint(edgesTex, coord.xy()).xy();
    coord.w = math::dot(e, float2(0.5f, 0.5f));
  }
  return coord.zw();
}

static float2 SMAASearchDiag2(
    SMAATexture2D(edgesTex), float2 texcoord, float2 dir, int2 size, float2 &e)
{
  float4 coord = float4(texcoord, -1.0f, 1.0f);
  coord.x += 0.25f / size.x; /* See @SearchDiag2Optimization */
  float3 t = float3(1.0f / float2(size), 1.0f);
  while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9f) {
    float3 increment = mad(t, float3(dir, 1.0f), coord.xyz());
    coord.x = increment.x;
    coord.y = increment.y;
    coord.z = increment.z;

    /* @SearchDiag2Optimization */
    /* Fetch both edges at once using bilinear filtering: */
    e = SMAASampleLevelZero(edgesTex, coord.xy()).xy();
    e = SMAADecodeDiagBilinearAccess(e);

    /* Non-optimized version: */
    // e.g = SMAASampleLevelZero(edgesTex, coord.xy).g;
    // e.r = SMAASampleLevelZeroOffset(edgesTex, coord.xy, int2(1, 0), size).r;

    coord.w = math::dot(e, float2(0.5f, 0.5f));
  }
  return coord.zw();
}

/**
 * Similar to SMAAArea, this calculates the area corresponding to a certain
 * diagonal distance and crossing edges 'e'.
 */
static float2 SMAAAreaDiag(SMAATexture2D(areaTex), float2 dist, float2 e, float offset)
{
  float2 texcoord = mad(
      float2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);

  /* We do a scale and bias for mapping to texel space: */
  texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5f * SMAA_AREATEX_PIXEL_SIZE);

  /* Diagonal areas are on the second half of the texture: */
  texcoord.x += 0.5f;

  /* Move to proper place, according to the sub-pixel offset: */
  texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;

  /* Do it! */
  return SMAA_AREATEX_SELECT(SMAASampleLevelZero(areaTex, texcoord));
}

/**
 * This searches for diagonal patterns and returns the corresponding weights.
 */
static float2 SMAACalculateDiagWeights(SMAATexture2D(edgesTex),
                                       SMAATexture2D(areaTex),
                                       float2 texcoord,
                                       float2 e,
                                       float4 subsampleIndices,
                                       int2 size)
{
  float2 weights = float2(0.0f, 0.0f);

  /* Search for the line ends: */
  float4 d;
  float2 end;
  if (e.x > 0.0f) {
    float2 negative_diagonal = SMAASearchDiag1(
        SMAATexturePass2D(edgesTex), texcoord, float2(-1.0f, 1.0f), size, end);
    d.x = negative_diagonal.x;
    d.z = negative_diagonal.y;
    d.x += float(end.y > 0.9f);
  }
  else {
    d.x = 0.0f;
    d.z = 0.0f;
  }
  float2 positive_diagonal = SMAASearchDiag1(
      SMAATexturePass2D(edgesTex), texcoord, float2(1.0, -1.0), size, end);
  d.y = positive_diagonal.x;
  d.w = positive_diagonal.y;

  SMAA_BRANCH
  if (d.x + d.y > 2.0f) { /* `d.x + d.y + 1 > 3`. */
    /* Fetch the crossing edges: */
    float4 coords = float4(texcoord, texcoord) +
                    float4(-d.x + 0.25f, d.x, d.y, -d.y - 0.25f) / float4(size, size);
    float4 c;
    float2 left_edge = SMAASampleLevelZeroOffset(edgesTex, coords.xy(), int2(-1, 0), size).xy();
    float2 right_edge = SMAASampleLevelZeroOffset(edgesTex, coords.zw(), int2(1, 0), size).xy();
    c.x = left_edge.x;
    c.y = left_edge.y;
    c.z = right_edge.x;
    c.w = right_edge.y;
    float4 decoded_access = SMAADecodeDiagBilinearAccess(c);
    c.y = decoded_access.x;
    c.x = decoded_access.y;
    c.w = decoded_access.z;
    c.z = decoded_access.w;

    /* Non-optimized version: */
    // float4 coords = mad(float4(-d.x, d.x, d.y, -d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
    // float4 c;
    // c.x = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2(-1,  0), size).g;
    // c.y = SMAASampleLevelZeroOffset(edgesTex, coords.xy, int2( 0,  0), size).r;
    // c.z = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1,  0), size).g;
    // c.w = SMAASampleLevelZeroOffset(edgesTex, coords.zw, int2( 1, -1), size).r;

    /* Merge crossing edges at each side into a single value: */
    float2 cc = mad(float2(2.0f, 2.0f), float2(c.x, c.z), float2(c.y, c.w));

    /* Remove the crossing edge if we didn't found the end of the line: */
    SMAAMovc(math::step(0.9f, d.zw()), cc, float2(0.0f, 0.0f));

    /* Fetch the areas for this line: */
    weights += SMAAAreaDiag(SMAATexturePass2D(areaTex), d.xy(), cc, subsampleIndices.z);
  }

  /* Search for the line ends: */
  float2 negative_diagonal = SMAASearchDiag2(
      SMAATexturePass2D(edgesTex), texcoord, float2(-1.0f, -1.0f), size, end);
  d.x = negative_diagonal.x;
  d.z = negative_diagonal.y;
  if (SMAASamplePointOffset(edgesTex, texcoord, int2(1, 0), size).x > 0.0f) {
    float2 positive_diagonal = SMAASearchDiag2(
        SMAATexturePass2D(edgesTex), texcoord, float2(1.0f, 1.0f), size, end);
    d.y = positive_diagonal.x;
    d.w = positive_diagonal.y;
    d.y += float(end.y > 0.9f);
  }
  else {
    d.y = 0.0f;
    d.w = 0.0f;
  }

  SMAA_BRANCH
  if (d.x + d.y > 2.0f) { /* `d.x + d.y + 1 > 3` */
    /* Fetch the crossing edges: */
    float4 coords = float4(texcoord, texcoord) + float4(-d.x, -d.x, d.y, d.y) / float4(size, size);
    float4 c;
    c.x = SMAASampleLevelZeroOffset(edgesTex, coords.xy(), int2(-1, 0), size).y;
    c.y = SMAASampleLevelZeroOffset(edgesTex, coords.xy(), int2(0, -1), size).x;
    float2 left_edge = SMAASampleLevelZeroOffset(edgesTex, coords.zw(), int2(1, 0), size).xy();
    c.z = left_edge.y;
    c.w = left_edge.x;
    float2 cc = mad(float2(2.0f, 2.0f), float2(c.x, c.z), float2(c.y, c.w));

    /* Remove the crossing edge if we didn't found the end of the line: */
    SMAAMovc(math::step(0.9f, d.zw()), cc, float2(0.0f, 0.0f));

    /* Fetch the areas for this line: */
    float2 area = SMAAAreaDiag(SMAATexturePass2D(areaTex), d.xy(), cc, subsampleIndices.w).xy();
    weights.x += area.y;
    weights.y += area.x;
  }

  return weights;
}
#endif

/* ----------------------------------------------------------------------------
 * Horizontal/Vertical Search Functions */

/**
 * This allows to determine how much length should we add in the last step
 * of the searches. It takes the bilinearly interpolated edge (see
 * @PSEUDO_GATHER4), and adds 0, 1 or 2, depending on which edges and
 * crossing edges are active.
 */
static float SMAASearchLength(SMAATexture2D(searchTex), float2 e, float offset)
{
  /* The texture is flipped vertically, with left and right cases taking half
   * of the space horizontally: */
  float2 scale = SMAA_SEARCHTEX_SIZE * float2(0.5f, -1.0f);
  float2 bias = SMAA_SEARCHTEX_SIZE * float2(offset, 1.0f);

  /* Scale and bias to access texel centers: */
  scale += float2(-1.0f, 1.0f);
  bias += float2(0.5f, -0.5f);

  /* Convert from pixel coordinates to texcoords:
   * (We use SMAA_SEARCHTEX_PACKED_SIZE because the texture is cropped). */
  scale *= 1.0f / SMAA_SEARCHTEX_PACKED_SIZE;
  bias *= 1.0f / SMAA_SEARCHTEX_PACKED_SIZE;

  /* Lookup the search texture: */
  return SMAA_SEARCHTEX_SELECT(SMAASampleLevelZero(searchTex, mad(scale, e, bias)));
}

/**
 * Horizontal/vertical search functions for the 2nd pass.
 */
static float SMAASearchXLeft(
    SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end, int2 size)
{
  /**
   * @PSEUDO_GATHER4
   * This texcoord has been offset by (-0.25, -0.125) in the vertex shader to
   * sample between edge, thus fetching four edges in a row.
   * Sampling with different offsets in each direction allows to disambiguate
   * which edges are active from the four fetched ones.
   */
  float2 e = float2(0.0f, 1.0f);
  while (texcoord.x > end && e.y > 0.8281f && /* Is there some edge not activated? */
         e.x == 0.0f) /* Or is there a crossing edge that breaks the line? */
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).xy();
    texcoord = texcoord - float2(2.0f, 0.0f) / float2(size);
  }

  float offset = mad(
      -(255.0f / 127.0f), SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.0f), 3.25f);
  return texcoord.x + offset / size.x;

  /* Non-optimized version:
   * We correct the previous (-0.25, -0.125) offset we applied: */
  // texcoord.x += 0.25 * SMAA_RT_METRICS.x;

  /* The searches are bias by 1, so adjust the coords accordingly: */
  // texcoord.x += SMAA_RT_METRICS.x;

  /* Disambiguate the length added by the last step: */
  // texcoord.x += 2.0 * SMAA_RT_METRICS.x; /* Undo last step. */
  // texcoord.x -= SMAA_RT_METRICS.x * (255.0 / 127.0) *
  //               SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.0);
  // return mad(SMAA_RT_METRICS.x, offset, texcoord.x);
}

static float SMAASearchXRight(
    SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end, int2 size)
{
  float2 e = float2(0.0f, 1.0f);
  while (texcoord.x < end && e.y > 0.8281f && /* Is there some edge not activated? */
         e.x == 0.0f) /* Or is there a crossing edge that breaks the line? */
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).xy();
    texcoord = texcoord + float2(2.0f, 0.0f) / float2(size);
  }
  float offset = mad(
      -(255.0f / 127.0f), SMAASearchLength(SMAATexturePass2D(searchTex), e, 0.5f), 3.25f);
  return texcoord.x - offset / size.x;
}

static float SMAASearchYUp(
    SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end, int2 size)
{
  float2 e = float2(1.0f, 0.0f);
  while (texcoord.y > end && e.x > 0.8281f && /* Is there some edge not activated? */
         e.y == 0.0f) /* Or is there a crossing edge that breaks the line? */
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).xy();
    texcoord = texcoord - float2(0.0f, 2.0f) / float2(size);
  }
  float2 flipped_edge = float2(e.y, e.x);
  float offset = mad(-(255.0f / 127.0f),
                     SMAASearchLength(SMAATexturePass2D(searchTex), flipped_edge, 0.0f),
                     3.25f);
  return texcoord.y + offset / size.y;
}

static float SMAASearchYDown(
    SMAATexture2D(edgesTex), SMAATexture2D(searchTex), float2 texcoord, float end, int2 size)
{
  float2 e = float2(1.0f, 0.0f);
  while (texcoord.y < end && e.x > 0.8281f && /* Is there some edge not activated? */
         e.y == 0.0f) /* Or is there a crossing edge that breaks the line? */
  {
    e = SMAASampleLevelZero(edgesTex, texcoord).xy();
    texcoord = texcoord + float2(0.0f, 2.0f) / float2(size);
  }
  float2 flipped_edge = float2(e.y, e.x);
  float offset = mad(-(255.0f / 127.0f),
                     SMAASearchLength(SMAATexturePass2D(searchTex), flipped_edge, 0.5f),
                     3.25f);
  return texcoord.y - offset / size.y;
}

/**
 * Ok, we have the distance and both crossing edges. So, what are the areas
 * at each side of current edge?
 */
static float2 SMAAArea(SMAATexture2D(areaTex), float2 dist, float e1, float e2, float offset)
{
  /* Rounding prevents precision errors of bilinear filtering: */
  float2 texcoord = mad(float2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE),
                        math::round(4.0f * float2(e1, e2)),
                        dist);

  /* We do a scale and bias for mapping to texel space: */
  texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5f * SMAA_AREATEX_PIXEL_SIZE);

  /* Move to proper place, according to the sub-pixel offset: */
  texcoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, offset, texcoord.y);

  /* Do it! */
  return SMAA_AREATEX_SELECT(SMAASampleLevelZero(areaTex, texcoord));
}

/* ----------------------------------------------------------------------------
 * Corner Detection Functions */

static void SMAADetectHorizontalCornerPattern(SMAATexture2D(edgesTex),
                                              float2 &weights,
                                              float4 texcoord,
                                              float2 d,
                                              int2 size,
                                              int corner_rounding)
{
#if !defined(SMAA_DISABLE_CORNER_DETECTION)
  float2 leftRight = math::step(d, float2(d.y, d.x));
  float2 rounding = (1.0f - corner_rounding / 100.0f) * leftRight;

  rounding /= leftRight.x + leftRight.y; /* Reduce blending for pixels in the center of a line. */

  float2 factor = float2(1.0f, 1.0f);
  factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy(), int2(0, 1), size).x;
  factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw(), int2(1, 1), size).x;
  factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy(), int2(0, -2), size).x;
  factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw(), int2(1, -2), size).x;

  weights *= saturate(factor);
#endif
}

static void SMAADetectVerticalCornerPattern(SMAATexture2D(edgesTex),
                                            float2 &weights,
                                            float4 texcoord,
                                            float2 d,
                                            int2 size,
                                            int corner_rounding)
{
#if !defined(SMAA_DISABLE_CORNER_DETECTION)
  float2 leftRight = math::step(d, float2(d.y, d.x));
  float2 rounding = (1.0f - corner_rounding / 100.0f) * leftRight;

  rounding /= leftRight.x + leftRight.y;

  float2 factor = float2(1.0f, 1.0f);
  factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy(), int2(1, 0), size).y;
  factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw(), int2(1, 1), size).y;
  factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy(), int2(-2, 0), size).y;
  factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw(), int2(-2, 1), size).y;

  weights *= saturate(factor);
#endif
}

/* ----------------------------------------------------------------------------
 * Blending Weight Calculation Pixel Shader (Second Pass) */

static float4 SMAABlendingWeightCalculationPS(float2 texcoord,
                                              float2 pixcoord,
                                              float4 offset[3],
                                              SMAATexture2D(edgesTex),
                                              SMAATexture2D(areaTex),
                                              SMAATexture2D(searchTex),
                                              float4 subsampleIndices,
                                              int2 size,
                                              int corner_rounding)
{
  /* Just pass zero for SMAA 1x, see @SUBSAMPLE_INDICES. */
  float4 weights = float4(0.0f, 0.0f, 0.0f, 0.0f);

  float2 e = SMAASamplePoint(edgesTex, texcoord).xy();

  SMAA_BRANCH
  if (e.y > 0.0f) { /* Edge at north. */
#if !defined(SMAA_DISABLE_DIAG_DETECTION)
    /* Diagonals have both north and west edges, so searching for them in
     * one of the boundaries is enough. */
    float2 diagonal_weights = SMAACalculateDiagWeights(SMAATexturePass2D(edgesTex),
                                                       SMAATexturePass2D(areaTex),
                                                       texcoord,
                                                       e,
                                                       subsampleIndices,
                                                       size);

    weights.x = diagonal_weights.x;
    weights.y = diagonal_weights.y;

    /* We give priority to diagonals, so if we find a diagonal we skip
     * horizontal/vertical processing. */
    SMAA_BRANCH
    if (weights.x == -weights.y) { /* `weights.x + weights.y == 0.0` */
#endif

      float2 d;

      /* Find the distance to the left: */
      float3 coords;
      coords.x = SMAASearchXLeft(SMAATexturePass2D(edgesTex),
                                 SMAATexturePass2D(searchTex),
                                 offset[0].xy(),
                                 offset[2].x,
                                 size);
      coords.y =
          offset[1].y;  // offset[1].y = texcoord.y - 0.25 * SMAA_RT_METRICS.y (@CROSSING_OFFSET)
      d.x = coords.x;

      /* Now fetch the left crossing edges, two at a time using bilinear
       * filtering. Sampling at -0.25 (see @CROSSING_OFFSET) enables to
       * discern what value each edge has: */
      float e1 = SMAASampleLevelZero(edgesTex, coords.xy()).x;

      /* Find the distance to the right: */
      coords.z = SMAASearchXRight(SMAATexturePass2D(edgesTex),
                                  SMAATexturePass2D(searchTex),
                                  offset[0].zw(),
                                  offset[2].y,
                                  size);
      d.y = coords.z;

      /* We want the distances to be in pixel units (doing this here allows
       * better interleaving of arithmetic and memory accesses): */
      d = math::abs(math::round(mad(float2(size.x), d, -float2(pixcoord.x))));

      /* SMAAArea below needs a sqrt, as the areas texture is compressed quadratically: */
      float2 sqrt_d = math::sqrt(d);

      /* Fetch the right crossing edges: */
      float e2 =
          SMAASampleLevelZeroOffset(edgesTex, float2(coords.z, coords.y), int2(1, 0), size).x;

      /* Ok, we know how this pattern looks like, now it is time for getting the actual area: */
      float2 area = SMAAArea(SMAATexturePass2D(areaTex), sqrt_d, e1, e2, subsampleIndices.y);
      weights.x = area.x;
      weights.y = area.y;

      /* Fix corners: */
      coords.y = texcoord.y;

      float2 corner_weight = weights.xy();
      SMAADetectHorizontalCornerPattern(SMAATexturePass2D(edgesTex),
                                        corner_weight,
                                        float4(coords.xy(), coords.z, coords.y),
                                        d,
                                        size,
                                        corner_rounding);
      weights.x = corner_weight.x;
      weights.y = corner_weight.y;

#if !defined(SMAA_DISABLE_DIAG_DETECTION)
    }
    else {
      e.x = 0.0f; /* Skip vertical processing. */
    }
#endif
  }

  SMAA_BRANCH
  if (e.x > 0.0f) { /* Edge at west. */
    float2 d;

    /* Find the distance to the top: */
    float3 coords;
    coords.y = SMAASearchYUp(SMAATexturePass2D(edgesTex),
                             SMAATexturePass2D(searchTex),
                             offset[1].xy(),
                             offset[2].z,
                             size);
    coords.x = offset[0].x;  // offset[1].x = texcoord.x - 0.25 * SMAA_RT_METRICS.x;
    d.x = coords.y;

    /* Fetch the top crossing edges: */
    float e1 = SMAASampleLevelZero(edgesTex, coords.xy()).y;

    /* Find the distance to the bottom: */
    coords.z = SMAASearchYDown(SMAATexturePass2D(edgesTex),
                               SMAATexturePass2D(searchTex),
                               offset[1].zw(),
                               offset[2].w,
                               size);
    d.y = coords.z;

    /* We want the distances to be in pixel units: */
    d = math::abs(math::round(mad(float2(size.y), d, -float2(pixcoord.y))));

    /* SMAAArea below needs a sqrt, as the areas texture is compressed quadratically: */
    float2 sqrt_d = math::sqrt(d);

    /* Fetch the bottom crossing edges: */
    float e2 = SMAASampleLevelZeroOffset(edgesTex, float2(coords.x, coords.z), int2(0, 1), size).y;

    /* Get the area for this direction: */
    float2 area = SMAAArea(SMAATexturePass2D(areaTex), sqrt_d, e1, e2, subsampleIndices.x);
    weights.z = area.x;
    weights.w = area.y;

    /* Fix corners: */
    coords.x = texcoord.x;

    float2 corner_weight = weights.zw();
    SMAADetectVerticalCornerPattern(SMAATexturePass2D(edgesTex),
                                    corner_weight,
                                    float4(coords.xy(), coords.x, coords.z),
                                    d,
                                    size,
                                    corner_rounding);
    weights.z = corner_weight.x;
    weights.w = corner_weight.y;
  }

  return weights;
}

/* ----------------------------------------------------------------------------
 * Neighborhood Blending Pixel Shader (Third Pass) */

static float4 SMAANeighborhoodBlendingPS(float2 texcoord,
                                         float4 offset,
                                         SMAATexture2D(colorTex),
                                         SMAATexture2D(blendTex),
#if SMAA_REPROJECTION
                                         SMAATexture2D(velocityTex),
#endif
                                         int2 size)
{
  /* Fetch the blending weights for current pixel: */
  float4 a;
  a.x = SMAASample(blendTex, offset.xy()).w;  // Right
  a.y = SMAASample(blendTex, offset.zw()).y;  // Top
  a.z = SMAASample(blendTex, texcoord).z;     // Left
  a.w = SMAASample(blendTex, texcoord).x;     // Bottom

  /* Is there any blending weight with a value greater than 0.0? */
  SMAA_BRANCH
  if (math::dot(a, float4(1.0f, 1.0f, 1.0f, 1.0f)) < 1e-5f) {
    float4 color = SMAASampleLevelZero(colorTex, texcoord);

#if SMAA_REPROJECTION
    float2 velocity = SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, texcoord));

    /* Pack velocity into the alpha channel: */
    color.a = math::sqrt(5.0f * math::length(velocity));
#endif

    return color;
  }

  bool h = math::max(a.x, a.z) > math::max(a.y, a.w); /* `max(horizontal) > max(vertical)`. */

  /* Calculate the blending offsets: */
  float4 blendingOffset = float4(0.0f, a.y, 0.0f, a.w);
  float2 blendingWeight = float2(a.y, a.w);
  SMAAMovc(float4(h), blendingOffset, float4(a.x, 0.0f, a.z, 0.0f));
  SMAAMovc(float2(h), blendingWeight, float2(a.x, a.z));
  blendingWeight /= math::dot(blendingWeight, float2(1.0f, 1.0f));

  /* Calculate the texture coordinates: */
  float4 blendingCoord = float4(texcoord, texcoord) + blendingOffset / float4(size, -size);

  /* We exploit bilinear filtering to mix current pixel with the chosen neighbor: */
  float4 color = blendingWeight.x * SMAASampleLevelZero(colorTex, blendingCoord.xy());
  color += blendingWeight.y * SMAASampleLevelZero(colorTex, blendingCoord.zw());

#if SMAA_REPROJECTION
  /* Anti-alias velocity for proper reprojection in a later stage: */
  float2 velocity = blendingWeight.x *
                    SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, blendingCoord.xy()));
  velocity += blendingWeight.y *
              SMAA_DECODE_VELOCITY(SMAASampleLevelZero(velocityTex, blendingCoord.zw()));

  /* Pack velocity into the alpha channel: */
  color.a = math::sqrt(5.0f * math::length(velocity));
#endif

  return color;
}

static float3 get_luminance_coefficients(ResultType type)
{
  switch (type) {
    case ResultType::Color: {
      float3 luminance_coefficients;
      IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
      return luminance_coefficients;
    }
    case ResultType::Float:
      return float3(1.0f, 0.0f, 0.0f);
    default:
      break;
  }

  BLI_assert_unreachable();
  return float3(0.0f);
}

static Result detect_edges_gpu(Context &context,
                               const Result &input,
                               const float threshold,
                               const float local_contrast_adaptation_factor)
{
  gpu::Shader *shader = context.get_shader("compositor_smaa_edge_detection");
  GPU_shader_bind(shader);

  const float3 luminance_coefficients = get_luminance_coefficients(input.type());
  GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
  GPU_shader_uniform_1f(shader, "smaa_threshold", threshold);
  GPU_shader_uniform_1f(
      shader, "smaa_local_contrast_adaptation_factor", local_contrast_adaptation_factor);

  GPU_texture_filter_mode(input, true);
  input.bind_as_texture(shader, "input_tx");

  Result edges = context.create_result(ResultType::Color);
  edges.allocate_texture(input.domain());
  edges.bind_as_image(shader, "edges_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  edges.unbind_as_image();

  return edges;
}

static Result detect_edges_cpu(Context &context,
                               const Result &input,
                               const float threshold,
                               const float local_contrast_adaptation_factor)
{
  const float3 luminance_coefficients = get_luminance_coefficients(input.type());

  Result edges = context.create_result(ResultType::Float2);
  edges.allocate_texture(input.domain());

  const int2 size = input.domain().size;
  parallel_for(size, [&](const int2 texel) {
    const float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

    float4 offset[3];
    SMAAEdgeDetectionVS(coordinates, size, offset);

    const float2 edge = SMAALumaEdgeDetectionPS(coordinates,
                                                offset,
                                                input,
                                                threshold,
                                                luminance_coefficients,
                                                local_contrast_adaptation_factor);
    edges.store_pixel(texel, edge);
  });

  return edges;
}

static Result detect_edges(Context &context,
                           const Result &input,
                           const float threshold,
                           const float local_contrast_adaptation_factor)
{
  if (context.use_gpu()) {
    return detect_edges_gpu(context, input, threshold, local_contrast_adaptation_factor);
  }

  return detect_edges_cpu(context, input, threshold, local_contrast_adaptation_factor);
}

static Result calculate_blending_weights_gpu(Context &context,
                                             const Result &edges,
                                             const int corner_rounding)
{
  gpu::Shader *shader = context.get_shader("compositor_smaa_blending_weight_calculation");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1i(shader, "smaa_corner_rounding", corner_rounding);

  GPU_texture_filter_mode(edges, true);
  edges.bind_as_texture(shader, "edges_tx");

  const SMAAPrecomputedTextures &smaa_precomputed_textures =
      context.cache_manager().smaa_precomputed_textures.get(context);
  smaa_precomputed_textures.bind_area_texture(shader, "area_tx");
  smaa_precomputed_textures.bind_search_texture(shader, "search_tx");

  Result weights = context.create_result(ResultType::Float4);
  weights.allocate_texture(edges.domain());
  weights.bind_as_image(shader, "weights_img");

  compute_dispatch_threads_at_least(shader, edges.domain().size);

  GPU_shader_unbind();
  edges.unbind_as_texture();
  smaa_precomputed_textures.unbind_area_texture();
  smaa_precomputed_textures.unbind_search_texture();
  weights.unbind_as_image();

  return weights;
}

static Result calculate_blending_weights_cpu(Context &context,
                                             const Result &edges,
                                             const int corner_rounding)
{
  const SMAAPrecomputedTextures &smaa_precomputed_textures =
      context.cache_manager().smaa_precomputed_textures.get(context);

  Result weights_result = context.create_result(ResultType::Float4);
  weights_result.allocate_texture(edges.domain());

  const int2 size = edges.domain().size;
  parallel_for(size, [&](const int2 texel) {
    const float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

    float4 offset[3];
    float2 pixel_coordinates;
    SMAABlendingWeightCalculationVS(coordinates, size, pixel_coordinates, offset);

    const float4 weights = SMAABlendingWeightCalculationPS(
        coordinates,
        pixel_coordinates,
        offset,
        edges,
        smaa_precomputed_textures.area_texture,
        smaa_precomputed_textures.search_texture,
        float4(0.0f),
        size,
        corner_rounding);
    weights_result.store_pixel(texel, weights);
  });

  return weights_result;
}

static Result calculate_blending_weights(Context &context,
                                         const Result &edges,
                                         const int corner_rounding)
{
  if (context.use_gpu()) {
    return calculate_blending_weights_gpu(context, edges, corner_rounding);
  }

  return calculate_blending_weights_cpu(context, edges, corner_rounding);
}

static const char *get_blend_shader_name(ResultType type)
{
  switch (type) {
    case ResultType::Color:
      return "compositor_smaa_neighborhood_blending_float4";
    case ResultType::Float:
      return "compositor_smaa_neighborhood_blending_float";
    default:
      break;
  }

  BLI_assert_unreachable();
  return "";
}

static void blend_neighborhood_gpu(Context &context,
                                   const Result &input,
                                   const Result &weights,
                                   Result &output)
{
  gpu::Shader *shader = context.get_shader(get_blend_shader_name(input.type()));
  GPU_shader_bind(shader);

  GPU_texture_filter_mode(input, true);
  input.bind_as_texture(shader, "input_tx");

  GPU_texture_filter_mode(weights, true);
  weights.bind_as_texture(shader, "weights_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.unbind_as_texture();
  output.unbind_as_image();
}

static void blend_neighborhood_cpu(const Result &input, const Result &weights, Result &output)
{
  output.allocate_texture(input.domain());

  const int2 size = input.domain().size;
  parallel_for(size, [&](const int2 texel) {
    const float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

    float4 offset;
    SMAANeighborhoodBlendingVS(coordinates, size, offset);

    const float4 result = SMAANeighborhoodBlendingPS(coordinates, offset, input, weights, size);
    output.store_pixel_generic_type(texel, result);
  });
}

static void blend_neighborhood(Context &context,
                               const Result &input,
                               const Result &weights,
                               Result &output)
{
  if (context.use_gpu()) {
    blend_neighborhood_gpu(context, input, weights, output);
  }
  else {
    blend_neighborhood_cpu(input, weights, output);
  }
}

void smaa(Context &context,
          const Result &input,
          Result &output,
          const float threshold,
          const float local_contrast_adaptation_factor,
          const int corner_rounding)
{
  if (input.is_single_value()) {
    output.share_data(input);
    return;
  }

  Result edges = detect_edges(context, input, threshold, local_contrast_adaptation_factor);
  Result weights = calculate_blending_weights(context, edges, corner_rounding);
  edges.release();
  blend_neighborhood(context, input, weights, output);
  weights.release();
}

}  // namespace blender::compositor
