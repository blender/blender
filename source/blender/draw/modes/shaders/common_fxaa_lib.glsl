//----------------------------------------------------------------------------------
// File:        es3-kepler\FXAA/FXAA3_11.h
// SDK Version: v3.00
// Email:       gameworks@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------

/* BLENDER MODIFICATIONS:
 *
 * - (#B1#) Compute luma on the fly using BT. 709 luma function
 * - (#B2#) main function instead of #include, due to lack of
 *          ARB_shading_language_include in 3.3
 * - (#B3#) version and extension directives
 * - removed "FXAA Console" algorithm support and shader parameters
 * - removed  HLSL support shims
 * - (#B4#) change luma sampling to compute, not use A channel
 *          (this also removes GATHER4_ALPHA support)
 * - removed all the console shaders (only remaining algorithm is "FXAA PC
 * Quality")
 *
 * Note that this file doesn't follow the coding style guidelines.
 */

/*============================================================================
                        FXAA QUALITY - TUNING KNOBS
------------------------------------------------------------------------------
NOTE the other tuning knobs are now in the shader function inputs!
============================================================================*/
#ifndef FXAA_QUALITY__PRESET
//
// Choose the quality preset.
// This needs to be compiled into the shader as it effects code.
// Best option to include multiple presets is to
// in each shader define the preset, then include this file.
//
// OPTIONS
// -----------------------------------------------------------------------
// 10 to 15 - default medium dither (10=fastest, 15=highest quality)
// 20 to 29 - less dither, more expensive (20=fastest, 29=highest quality)
// 39       - no dither, very expensive
//
// NOTES
// -----------------------------------------------------------------------
// 12 = slightly faster then FXAA 3.9 and higher edge quality (default)
// 13 = about same speed as FXAA 3.9 and better than 12
// 23 = closest to FXAA 3.9 visually and performance wise
//  _ = the lowest digit is directly related to performance
// _  = the highest digit is directly related to style
//
#  define FXAA_QUALITY__PRESET 12
#endif

/*============================================================================

                           FXAA QUALITY - PRESETS

============================================================================*/

/*============================================================================
                     FXAA QUALITY - MEDIUM DITHER PRESETS
============================================================================*/
#if (FXAA_QUALITY__PRESET == 10)
#  define FXAA_QUALITY__PS 3
#  define FXAA_QUALITY__P0 1.5
#  define FXAA_QUALITY__P1 3.0
#  define FXAA_QUALITY__P2 12.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 11)
#  define FXAA_QUALITY__PS 4
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 3.0
#  define FXAA_QUALITY__P3 12.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 12)
#  define FXAA_QUALITY__PS 5
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 4.0
#  define FXAA_QUALITY__P4 12.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 13)
#  define FXAA_QUALITY__PS 6
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 4.0
#  define FXAA_QUALITY__P5 12.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 14)
#  define FXAA_QUALITY__PS 7
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 4.0
#  define FXAA_QUALITY__P6 12.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 15)
#  define FXAA_QUALITY__PS 8
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 2.0
#  define FXAA_QUALITY__P6 4.0
#  define FXAA_QUALITY__P7 12.0
#endif

/*============================================================================
                     FXAA QUALITY - LOW DITHER PRESETS
============================================================================*/
#if (FXAA_QUALITY__PRESET == 20)
#  define FXAA_QUALITY__PS 3
#  define FXAA_QUALITY__P0 1.5
#  define FXAA_QUALITY__P1 2.0
#  define FXAA_QUALITY__P2 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 21)
#  define FXAA_QUALITY__PS 4
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 22)
#  define FXAA_QUALITY__PS 5
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 23)
#  define FXAA_QUALITY__PS 6
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 24)
#  define FXAA_QUALITY__PS 7
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 3.0
#  define FXAA_QUALITY__P6 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 25)
#  define FXAA_QUALITY__PS 8
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 2.0
#  define FXAA_QUALITY__P6 4.0
#  define FXAA_QUALITY__P7 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 26)
#  define FXAA_QUALITY__PS 9
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 2.0
#  define FXAA_QUALITY__P6 2.0
#  define FXAA_QUALITY__P7 4.0
#  define FXAA_QUALITY__P8 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 27)
#  define FXAA_QUALITY__PS 10
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 2.0
#  define FXAA_QUALITY__P6 2.0
#  define FXAA_QUALITY__P7 2.0
#  define FXAA_QUALITY__P8 4.0
#  define FXAA_QUALITY__P9 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 28)
#  define FXAA_QUALITY__PS 11
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 2.0
#  define FXAA_QUALITY__P6 2.0
#  define FXAA_QUALITY__P7 2.0
#  define FXAA_QUALITY__P8 2.0
#  define FXAA_QUALITY__P9 4.0
#  define FXAA_QUALITY__P10 8.0
#endif
/*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PRESET == 29)
#  define FXAA_QUALITY__PS 12
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.5
#  define FXAA_QUALITY__P2 2.0
#  define FXAA_QUALITY__P3 2.0
#  define FXAA_QUALITY__P4 2.0
#  define FXAA_QUALITY__P5 2.0
#  define FXAA_QUALITY__P6 2.0
#  define FXAA_QUALITY__P7 2.0
#  define FXAA_QUALITY__P8 2.0
#  define FXAA_QUALITY__P9 2.0
#  define FXAA_QUALITY__P10 4.0
#  define FXAA_QUALITY__P11 8.0
#endif

/*============================================================================
                     FXAA QUALITY - EXTREME QUALITY
============================================================================*/
#if (FXAA_QUALITY__PRESET == 39)
#  define FXAA_QUALITY__PS 12
#  define FXAA_QUALITY__P0 1.0
#  define FXAA_QUALITY__P1 1.0
#  define FXAA_QUALITY__P2 1.0
#  define FXAA_QUALITY__P3 1.0
#  define FXAA_QUALITY__P4 1.0
#  define FXAA_QUALITY__P5 1.5
#  define FXAA_QUALITY__P6 2.0
#  define FXAA_QUALITY__P7 2.0
#  define FXAA_QUALITY__P8 2.0
#  define FXAA_QUALITY__P9 2.0
#  define FXAA_QUALITY__P10 4.0
#  define FXAA_QUALITY__P11 8.0
#endif

#define FxaaSat(x) clamp(x, 0.0, 1.0)

#ifdef FXAA_ALPHA

#  define FxaaTexTop(t, p) textureLod(t, p, 0.0).aaaa
#  define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o).aaaa
#  define FxaaLuma(rgba) rgba.a

#else

#  define FxaaTexTop(t, p) textureLod(t, p, 0.0)
#  define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)

/* (#B1#) */
float FxaaLuma(vec4 rgba)
{
  // note: sqrt because the sampled colors are in a linear colorspace!
  // this approximates a perceptual conversion, which is good enough for the
  // algorithm
  return sqrt(dot(rgba.rgb, vec3(0.2126, 0.7152, 0.0722)));
}

#endif

/*============================================================================

                             FXAA3 QUALITY - PC

============================================================================*/
/*--------------------------------------------------------------------------*/
vec4 FxaaPixelShader(
    //
    // Use noperspective interpolation here (turn off perspective interpolation).
    // {xy} = center of pixel
    vec2 pos,
    //
    // Input color texture.
    // {rgb_} = color in linear or perceptual color space
    sampler2D tex,
    //
    // Only used on FXAA Quality.
    // This must be from a constant/uniform.
    // {x_} = 1.0/screenWidthInPixels
    // {_y} = 1.0/screenHeightInPixels
    vec2 fxaaQualityRcpFrame,
    //
    // Only used on FXAA Quality.
    // This used to be the FXAA_QUALITY__SUBPIX define.
    // It is here now to allow easier tuning.
    // Choose the amount of sub-pixel aliasing removal.
    // This can effect sharpness.
    //   1.00 - upper limit (softer)
    //   0.75 - default amount of filtering
    //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
    //   0.25 - almost off
    //   0.00 - completely off
    float fxaaQualitySubpix,
    //
    // Only used on FXAA Quality.
    // This used to be the FXAA_QUALITY__EDGE_THRESHOLD define.
    // It is here now to allow easier tuning.
    // The minimum amount of local contrast required to apply algorithm.
    //   0.333 - too little (faster)
    //   0.250 - low quality
    //   0.166 - default
    //   0.125 - high quality
    //   0.063 - overkill (slower)
    float fxaaQualityEdgeThreshold,
    //
    // Only used on FXAA Quality.
    // This used to be the FXAA_QUALITY__EDGE_THRESHOLD_MIN define.
    // It is here now to allow easier tuning.
    // Trims the algorithm from processing darks.
    //   0.0833 - upper limit (default, the start of visible unfiltered edges)
    //   0.0625 - high quality (faster)
    //   0.0312 - visible limit (slower)
    float fxaaQualityEdgeThresholdMin)
{
  /*--------------------------------------------------------------------------*/
  vec2 posM;
  posM.x = pos.x;
  posM.y = pos.y;
  vec4 rgbyM = FxaaTexTop(tex, posM);
  float lumaM = FxaaLuma(rgbyM);  // (#B4#)
  float lumaS = FxaaLuma(FxaaTexOff(tex, posM, ivec2(0, 1), fxaaQualityRcpFrame.xy));
  float lumaE = FxaaLuma(FxaaTexOff(tex, posM, ivec2(1, 0), fxaaQualityRcpFrame.xy));
  float lumaN = FxaaLuma(FxaaTexOff(tex, posM, ivec2(0, -1), fxaaQualityRcpFrame.xy));
  float lumaW = FxaaLuma(FxaaTexOff(tex, posM, ivec2(-1, 0), fxaaQualityRcpFrame.xy));
  /*--------------------------------------------------------------------------*/
  float maxSM = max(lumaS, lumaM);
  float minSM = min(lumaS, lumaM);
  float maxESM = max(lumaE, maxSM);
  float minESM = min(lumaE, minSM);
  float maxWN = max(lumaN, lumaW);
  float minWN = min(lumaN, lumaW);
  float rangeMax = max(maxWN, maxESM);
  float rangeMin = min(minWN, minESM);
  float rangeMaxScaled = rangeMax * fxaaQualityEdgeThreshold;
  float range = rangeMax - rangeMin;
  float rangeMaxClamped = max(fxaaQualityEdgeThresholdMin, rangeMaxScaled);
  bool earlyExit = range < rangeMaxClamped;
  /*--------------------------------------------------------------------------*/
  if (earlyExit) {
    return rgbyM;
  }
  /*--------------------------------------------------------------------------*/
  float lumaNW = FxaaLuma(FxaaTexOff(tex, posM, ivec2(-1, -1), fxaaQualityRcpFrame.xy));
  float lumaSE = FxaaLuma(FxaaTexOff(tex, posM, ivec2(1, 1), fxaaQualityRcpFrame.xy));
  float lumaNE = FxaaLuma(FxaaTexOff(tex, posM, ivec2(1, -1), fxaaQualityRcpFrame.xy));
  float lumaSW = FxaaLuma(FxaaTexOff(tex, posM, ivec2(-1, 1), fxaaQualityRcpFrame.xy));
  /*--------------------------------------------------------------------------*/
  float lumaNS = lumaN + lumaS;
  float lumaWE = lumaW + lumaE;
  float subpixRcpRange = 1.0 / range;
  float subpixNSWE = lumaNS + lumaWE;
  float edgeHorz1 = (-2.0 * lumaM) + lumaNS;
  float edgeVert1 = (-2.0 * lumaM) + lumaWE;
  /*--------------------------------------------------------------------------*/
  float lumaNESE = lumaNE + lumaSE;
  float lumaNWNE = lumaNW + lumaNE;
  float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
  float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;
  /*--------------------------------------------------------------------------*/
  float lumaNWSW = lumaNW + lumaSW;
  float lumaSWSE = lumaSW + lumaSE;
  float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
  float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
  float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
  float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
  float edgeHorz = abs(edgeHorz3) + edgeHorz4;
  float edgeVert = abs(edgeVert3) + edgeVert4;
  /*--------------------------------------------------------------------------*/
  float subpixNWSWNESE = lumaNWSW + lumaNESE;
  float lengthSign = fxaaQualityRcpFrame.x;
  bool horzSpan = edgeHorz >= edgeVert;
  float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
  /*--------------------------------------------------------------------------*/
  if (!horzSpan)
    lumaN = lumaW;
  if (!horzSpan)
    lumaS = lumaE;
  if (horzSpan)
    lengthSign = fxaaQualityRcpFrame.y;
  float subpixB = (subpixA * (1.0 / 12.0)) - lumaM;
  /*--------------------------------------------------------------------------*/
  float gradientN = lumaN - lumaM;
  float gradientS = lumaS - lumaM;
  float lumaNN = lumaN + lumaM;
  float lumaSS = lumaS + lumaM;
  bool pairN = abs(gradientN) >= abs(gradientS);
  float gradient = max(abs(gradientN), abs(gradientS));
  if (pairN)
    lengthSign = -lengthSign;
  float subpixC = FxaaSat(abs(subpixB) * subpixRcpRange);
  /*--------------------------------------------------------------------------*/
  vec2 posB;
  posB.x = posM.x;
  posB.y = posM.y;
  vec2 offNP;
  offNP.x = (!horzSpan) ? 0.0 : fxaaQualityRcpFrame.x;
  offNP.y = (horzSpan) ? 0.0 : fxaaQualityRcpFrame.y;
  if (!horzSpan)
    posB.x += lengthSign * 0.5;
  if (horzSpan)
    posB.y += lengthSign * 0.5;
  /*--------------------------------------------------------------------------*/
  vec2 posN;
  posN.x = posB.x - offNP.x * FXAA_QUALITY__P0;
  posN.y = posB.y - offNP.y * FXAA_QUALITY__P0;
  vec2 posP;
  posP.x = posB.x + offNP.x * FXAA_QUALITY__P0;
  posP.y = posB.y + offNP.y * FXAA_QUALITY__P0;
  float subpixD = ((-2.0) * subpixC) + 3.0;
  float lumaEndN = FxaaLuma(FxaaTexTop(tex, posN));
  float subpixE = subpixC * subpixC;
  float lumaEndP = FxaaLuma(FxaaTexTop(tex, posP));
  /*--------------------------------------------------------------------------*/
  if (!pairN)
    lumaNN = lumaSS;
  float gradientScaled = gradient * 1.0 / 4.0;
  float lumaMM = lumaM - lumaNN * 0.5;
  float subpixF = subpixD * subpixE;
  bool lumaMLTZero = lumaMM < 0.0;
  /*--------------------------------------------------------------------------*/
  lumaEndN -= lumaNN * 0.5;
  lumaEndP -= lumaNN * 0.5;
  bool doneN = abs(lumaEndN) >= gradientScaled;
  bool doneP = abs(lumaEndP) >= gradientScaled;
  if (!doneN)
    posN.x -= offNP.x * FXAA_QUALITY__P1;
  if (!doneN)
    posN.y -= offNP.y * FXAA_QUALITY__P1;
  bool doneNP = (!doneN) || (!doneP);
  if (!doneP)
    posP.x += offNP.x * FXAA_QUALITY__P1;
  if (!doneP)
    posP.y += offNP.y * FXAA_QUALITY__P1;
  /*--------------------------------------------------------------------------*/
  if (doneNP) {
    if (!doneN)
      lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if (!doneP)
      lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if (!doneN)
      lumaEndN = lumaEndN - lumaNN * 0.5;
    if (!doneP)
      lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if (!doneN)
      posN.x -= offNP.x * FXAA_QUALITY__P2;
    if (!doneN)
      posN.y -= offNP.y * FXAA_QUALITY__P2;
    doneNP = (!doneN) || (!doneP);
    if (!doneP)
      posP.x += offNP.x * FXAA_QUALITY__P2;
    if (!doneP)
      posP.y += offNP.y * FXAA_QUALITY__P2;
      /*--------------------------------------------------------------------------*/
#if (FXAA_QUALITY__PS > 3)
    if (doneNP) {
      if (!doneN)
        lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
      if (!doneP)
        lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
      if (!doneN)
        lumaEndN = lumaEndN - lumaNN * 0.5;
      if (!doneP)
        lumaEndP = lumaEndP - lumaNN * 0.5;
      doneN = abs(lumaEndN) >= gradientScaled;
      doneP = abs(lumaEndP) >= gradientScaled;
      if (!doneN)
        posN.x -= offNP.x * FXAA_QUALITY__P3;
      if (!doneN)
        posN.y -= offNP.y * FXAA_QUALITY__P3;
      doneNP = (!doneN) || (!doneP);
      if (!doneP)
        posP.x += offNP.x * FXAA_QUALITY__P3;
      if (!doneP)
        posP.y += offNP.y * FXAA_QUALITY__P3;
        /*--------------------------------------------------------------------------*/
#  if (FXAA_QUALITY__PS > 4)
      if (doneNP) {
        if (!doneN)
          lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
        if (!doneP)
          lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
        if (!doneN)
          lumaEndN = lumaEndN - lumaNN * 0.5;
        if (!doneP)
          lumaEndP = lumaEndP - lumaNN * 0.5;
        doneN = abs(lumaEndN) >= gradientScaled;
        doneP = abs(lumaEndP) >= gradientScaled;
        if (!doneN)
          posN.x -= offNP.x * FXAA_QUALITY__P4;
        if (!doneN)
          posN.y -= offNP.y * FXAA_QUALITY__P4;
        doneNP = (!doneN) || (!doneP);
        if (!doneP)
          posP.x += offNP.x * FXAA_QUALITY__P4;
        if (!doneP)
          posP.y += offNP.y * FXAA_QUALITY__P4;
          /*--------------------------------------------------------------------------*/
#    if (FXAA_QUALITY__PS > 5)
        if (doneNP) {
          if (!doneN)
            lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
          if (!doneP)
            lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
          if (!doneN)
            lumaEndN = lumaEndN - lumaNN * 0.5;
          if (!doneP)
            lumaEndP = lumaEndP - lumaNN * 0.5;
          doneN = abs(lumaEndN) >= gradientScaled;
          doneP = abs(lumaEndP) >= gradientScaled;
          if (!doneN)
            posN.x -= offNP.x * FXAA_QUALITY__P5;
          if (!doneN)
            posN.y -= offNP.y * FXAA_QUALITY__P5;
          doneNP = (!doneN) || (!doneP);
          if (!doneP)
            posP.x += offNP.x * FXAA_QUALITY__P5;
          if (!doneP)
            posP.y += offNP.y * FXAA_QUALITY__P5;
            /*--------------------------------------------------------------------------*/
#      if (FXAA_QUALITY__PS > 6)
          if (doneNP) {
            if (!doneN)
              lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
            if (!doneP)
              lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
            if (!doneN)
              lumaEndN = lumaEndN - lumaNN * 0.5;
            if (!doneP)
              lumaEndP = lumaEndP - lumaNN * 0.5;
            doneN = abs(lumaEndN) >= gradientScaled;
            doneP = abs(lumaEndP) >= gradientScaled;
            if (!doneN)
              posN.x -= offNP.x * FXAA_QUALITY__P6;
            if (!doneN)
              posN.y -= offNP.y * FXAA_QUALITY__P6;
            doneNP = (!doneN) || (!doneP);
            if (!doneP)
              posP.x += offNP.x * FXAA_QUALITY__P6;
            if (!doneP)
              posP.y += offNP.y * FXAA_QUALITY__P6;
              /*--------------------------------------------------------------------------*/
#        if (FXAA_QUALITY__PS > 7)
            if (doneNP) {
              if (!doneN)
                lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
              if (!doneP)
                lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
              if (!doneN)
                lumaEndN = lumaEndN - lumaNN * 0.5;
              if (!doneP)
                lumaEndP = lumaEndP - lumaNN * 0.5;
              doneN = abs(lumaEndN) >= gradientScaled;
              doneP = abs(lumaEndP) >= gradientScaled;
              if (!doneN)
                posN.x -= offNP.x * FXAA_QUALITY__P7;
              if (!doneN)
                posN.y -= offNP.y * FXAA_QUALITY__P7;
              doneNP = (!doneN) || (!doneP);
              if (!doneP)
                posP.x += offNP.x * FXAA_QUALITY__P7;
              if (!doneP)
                posP.y += offNP.y * FXAA_QUALITY__P7;
                /*--------------------------------------------------------------------------*/
#          if (FXAA_QUALITY__PS > 8)
              if (doneNP) {
                if (!doneN)
                  lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
                if (!doneP)
                  lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
                if (!doneN)
                  lumaEndN = lumaEndN - lumaNN * 0.5;
                if (!doneP)
                  lumaEndP = lumaEndP - lumaNN * 0.5;
                doneN = abs(lumaEndN) >= gradientScaled;
                doneP = abs(lumaEndP) >= gradientScaled;
                if (!doneN)
                  posN.x -= offNP.x * FXAA_QUALITY__P8;
                if (!doneN)
                  posN.y -= offNP.y * FXAA_QUALITY__P8;
                doneNP = (!doneN) || (!doneP);
                if (!doneP)
                  posP.x += offNP.x * FXAA_QUALITY__P8;
                if (!doneP)
                  posP.y += offNP.y * FXAA_QUALITY__P8;
                  /*--------------------------------------------------------------------------*/
#            if (FXAA_QUALITY__PS > 9)
                if (doneNP) {
                  if (!doneN)
                    lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
                  if (!doneP)
                    lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
                  if (!doneN)
                    lumaEndN = lumaEndN - lumaNN * 0.5;
                  if (!doneP)
                    lumaEndP = lumaEndP - lumaNN * 0.5;
                  doneN = abs(lumaEndN) >= gradientScaled;
                  doneP = abs(lumaEndP) >= gradientScaled;
                  if (!doneN)
                    posN.x -= offNP.x * FXAA_QUALITY__P9;
                  if (!doneN)
                    posN.y -= offNP.y * FXAA_QUALITY__P9;
                  doneNP = (!doneN) || (!doneP);
                  if (!doneP)
                    posP.x += offNP.x * FXAA_QUALITY__P9;
                  if (!doneP)
                    posP.y += offNP.y * FXAA_QUALITY__P9;
                    /*--------------------------------------------------------------------------*/
#              if (FXAA_QUALITY__PS > 10)
                  if (doneNP) {
                    if (!doneN)
                      lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
                    if (!doneP)
                      lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
                    if (!doneN)
                      lumaEndN = lumaEndN - lumaNN * 0.5;
                    if (!doneP)
                      lumaEndP = lumaEndP - lumaNN * 0.5;
                    doneN = abs(lumaEndN) >= gradientScaled;
                    doneP = abs(lumaEndP) >= gradientScaled;
                    if (!doneN)
                      posN.x -= offNP.x * FXAA_QUALITY__P10;
                    if (!doneN)
                      posN.y -= offNP.y * FXAA_QUALITY__P10;
                    doneNP = (!doneN) || (!doneP);
                    if (!doneP)
                      posP.x += offNP.x * FXAA_QUALITY__P10;
                    if (!doneP)
                      posP.y += offNP.y * FXAA_QUALITY__P10;
                      /*-------------------------------------------------------------------------*/
#                if (FXAA_QUALITY__PS > 11)
                    if (doneNP) {
                      if (!doneN)
                        lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
                      if (!doneP)
                        lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
                      if (!doneN)
                        lumaEndN = lumaEndN - lumaNN * 0.5;
                      if (!doneP)
                        lumaEndP = lumaEndP - lumaNN * 0.5;
                      doneN = abs(lumaEndN) >= gradientScaled;
                      doneP = abs(lumaEndP) >= gradientScaled;
                      if (!doneN)
                        posN.x -= offNP.x * FXAA_QUALITY__P11;
                      if (!doneN)
                        posN.y -= offNP.y * FXAA_QUALITY__P11;
                      doneNP = (!doneN) || (!doneP);
                      if (!doneP)
                        posP.x += offNP.x * FXAA_QUALITY__P11;
                      if (!doneP)
                        posP.y += offNP.y * FXAA_QUALITY__P11;
                        /*-----------------------------------------------------------------------*/
#                  if (FXAA_QUALITY__PS > 12)
                      if (doneNP) {
                        if (!doneN)
                          lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
                        if (!doneP)
                          lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
                        if (!doneN)
                          lumaEndN = lumaEndN - lumaNN * 0.5;
                        if (!doneP)
                          lumaEndP = lumaEndP - lumaNN * 0.5;
                        doneN = abs(lumaEndN) >= gradientScaled;
                        doneP = abs(lumaEndP) >= gradientScaled;
                        if (!doneN)
                          posN.x -= offNP.x * FXAA_QUALITY__P12;
                        if (!doneN)
                          posN.y -= offNP.y * FXAA_QUALITY__P12;
                        doneNP = (!doneN) || (!doneP);
                        if (!doneP)
                          posP.x += offNP.x * FXAA_QUALITY__P12;
                        if (!doneP)
                          posP.y += offNP.y * FXAA_QUALITY__P12;
                        /*-----------------------------------------------------------------------*/
                      }
#                  endif
                      /*-------------------------------------------------------------------------*/
                    }
#                endif
                    /*--------------------------------------------------------------------------*/
                  }
#              endif
                  /*--------------------------------------------------------------------------*/
                }
#            endif
                /*--------------------------------------------------------------------------*/
              }
#          endif
              /*--------------------------------------------------------------------------*/
            }
#        endif
            /*--------------------------------------------------------------------------*/
          }
#      endif
          /*--------------------------------------------------------------------------*/
        }
#    endif
        /*--------------------------------------------------------------------------*/
      }
#  endif
      /*--------------------------------------------------------------------------*/
    }
#endif
    /*--------------------------------------------------------------------------*/
  }
  /*--------------------------------------------------------------------------*/
  float dstN = posM.x - posN.x;
  float dstP = posP.x - posM.x;
  if (!horzSpan)
    dstN = posM.y - posN.y;
  if (!horzSpan)
    dstP = posP.y - posM.y;
  /*--------------------------------------------------------------------------*/
  bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
  float spanLength = (dstP + dstN);
  bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
  float spanLengthRcp = 1.0 / spanLength;
  /*--------------------------------------------------------------------------*/
  bool directionN = dstN < dstP;
  float dst = min(dstN, dstP);
  bool goodSpan = directionN ? goodSpanN : goodSpanP;
  float subpixG = subpixF * subpixF;
  float pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
  float subpixH = subpixG * fxaaQualitySubpix;
  /*--------------------------------------------------------------------------*/
  float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
  float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
  if (!horzSpan)
    posM.x += pixelOffsetSubpix * lengthSign;
  if (horzSpan)
    posM.y += pixelOffsetSubpix * lengthSign;
  return vec4(FxaaTexTop(tex, posM).xyz, lumaM);
}
/*==========================================================================*/
