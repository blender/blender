/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.h"

typedef struct GlobalsUboStorage GlobalsUboStorage;
#endif

/* Future Plan: These globals were once shared between multiple overlay engines. But now that they
 * have been merged into one engine, there is no reasons to keep these globals out of the overlay
 * engine. */

#define UBO_FIRST_COLOR colorWire
#define UBO_LAST_COLOR colorUVShadow

/* Used as ubo but colors can be directly referenced as well */
/* NOTE: Also keep all color as vec4 and between #UBO_FIRST_COLOR and #UBO_LAST_COLOR. */
struct GlobalsUboStorage {
  /* UBOs data needs to be 16 byte aligned (size of vec4) */
  float4 colorWire;
  float4 colorWireEdit;
  float4 colorActive;
  float4 colorSelect;
  float4 colorLibrarySelect;
  float4 colorLibrary;
  float4 colorTransform;
  float4 colorLight;
  float4 colorSpeaker;
  float4 colorCamera;
  float4 colorCameraPath;
  float4 colorEmpty;
  float4 colorVertex;
  float4 colorVertexSelect;
  float4 colorVertexUnreferenced;
  float4 colorVertexMissingData;
  float4 colorEditMeshActive;
  float4 colorEdgeSelect;
  float4 colorEdgeSeam;
  float4 colorEdgeSharp;
  float4 colorEdgeCrease;
  float4 colorEdgeBWeight;
  float4 colorEdgeFaceSelect;
  float4 colorEdgeFreestyle;
  float4 colorFace;
  float4 colorFaceSelect;
  float4 colorFaceFreestyle;
  float4 colorGpencilVertex;
  float4 colorGpencilVertexSelect;
  float4 colorNormal;
  float4 colorVNormal;
  float4 colorLNormal;
  float4 colorFaceDot;
  float4 colorSkinRoot;

  float4 colorDeselect;
  float4 colorOutline;
  float4 colorLightNoAlpha;

  float4 colorBackground;
  float4 colorBackgroundGradient;
  float4 colorCheckerPrimary;
  float4 colorCheckerSecondary;
  float4 colorClippingBorder;
  float4 colorEditMeshMiddle;

  float4 colorHandleFree;
  float4 colorHandleAuto;
  float4 colorHandleVect;
  float4 colorHandleAlign;
  float4 colorHandleAutoclamp;
  float4 colorHandleSelFree;
  float4 colorHandleSelAuto;
  float4 colorHandleSelVect;
  float4 colorHandleSelAlign;
  float4 colorHandleSelAutoclamp;
  float4 colorNurbUline;
  float4 colorNurbVline;
  float4 colorNurbSelUline;
  float4 colorNurbSelVline;
  float4 colorActiveSpline;

  float4 colorBonePose;
  float4 colorBonePoseActive;
  float4 colorBonePoseActiveUnsel;
  float4 colorBonePoseConstraint;
  float4 colorBonePoseIK;
  float4 colorBonePoseSplineIK;
  float4 colorBonePoseTarget;
  float4 colorBoneSolid;
  float4 colorBoneLocked;
  float4 colorBoneActive;
  float4 colorBoneActiveUnsel;
  float4 colorBoneSelect;
  float4 colorBoneIKLine;
  float4 colorBoneIKLineNoTarget;
  float4 colorBoneIKLineSpline;

  float4 colorText;
  float4 colorTextHi;

  float4 colorBundleSolid;

  float4 colorMballRadius;
  float4 colorMballRadiusSelect;
  float4 colorMballStiffness;
  float4 colorMballStiffnessSelect;

  float4 colorCurrentFrame;

  float4 colorGrid;
  float4 colorGridEmphasis;
  float4 colorGridAxisX;
  float4 colorGridAxisY;
  float4 colorGridAxisZ;

  float4 colorFaceBack;
  float4 colorFaceFront;

  float4 colorUVShadow;

  /* NOTE: Put all color before #UBO_LAST_COLOR. */
  float4 screenVecs[2]; /* Padded as vec4. */
  float4 sizeViewport;  /* Packed as vec4. */

  /* Pack individual float at the end of the buffer to avoid alignment errors */
  float sizePixel, pixelFac;
  float sizeObjectCenter, sizeLightCenter, sizeLightCircle, sizeLightCircleShadow;
  float sizeVertex, sizeEdge, sizeEdgeFix, sizeFaceDot;
  float sizeChecker;
  float sizeVertexGpencil;
};
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

#ifdef GPU_SHADER
/* Keep compatibility_with old global scope syntax. */
/* TODO(@fclem) Mass rename and remove the camel case. */
#  define colorWire globalsBlock.colorWire
#  define colorWireEdit globalsBlock.colorWireEdit
#  define colorActive globalsBlock.colorActive
#  define colorSelect globalsBlock.colorSelect
#  define colorLibrarySelect globalsBlock.colorLibrarySelect
#  define colorLibrary globalsBlock.colorLibrary
#  define colorTransform globalsBlock.colorTransform
#  define colorLight globalsBlock.colorLight
#  define colorSpeaker globalsBlock.colorSpeaker
#  define colorCamera globalsBlock.colorCamera
#  define colorCameraPath globalsBlock.colorCameraPath
#  define colorEmpty globalsBlock.colorEmpty
#  define colorVertex globalsBlock.colorVertex
#  define colorVertexSelect globalsBlock.colorVertexSelect
#  define colorVertexUnreferenced globalsBlock.colorVertexUnreferenced
#  define colorVertexMissingData globalsBlock.colorVertexMissingData
#  define colorEditMeshActive globalsBlock.colorEditMeshActive
#  define colorEdgeSelect globalsBlock.colorEdgeSelect
#  define colorEdgeSeam globalsBlock.colorEdgeSeam
#  define colorEdgeSharp globalsBlock.colorEdgeSharp
#  define colorEdgeCrease globalsBlock.colorEdgeCrease
#  define colorEdgeBWeight globalsBlock.colorEdgeBWeight
#  define colorEdgeFaceSelect globalsBlock.colorEdgeFaceSelect
#  define colorEdgeFreestyle globalsBlock.colorEdgeFreestyle
#  define colorFace globalsBlock.colorFace
#  define colorFaceSelect globalsBlock.colorFaceSelect
#  define colorFaceFreestyle globalsBlock.colorFaceFreestyle
#  define colorGpencilVertex globalsBlock.colorGpencilVertex
#  define colorGpencilVertexSelect globalsBlock.colorGpencilVertexSelect
#  define colorNormal globalsBlock.colorNormal
#  define colorVNormal globalsBlock.colorVNormal
#  define colorLNormal globalsBlock.colorLNormal
#  define colorFaceDot globalsBlock.colorFaceDot
#  define colorSkinRoot globalsBlock.colorSkinRoot
#  define colorDeselect globalsBlock.colorDeselect
#  define colorOutline globalsBlock.colorOutline
#  define colorLightNoAlpha globalsBlock.colorLightNoAlpha
#  define colorBackground globalsBlock.colorBackground
#  define colorBackgroundGradient globalsBlock.colorBackgroundGradient
#  define colorCheckerPrimary globalsBlock.colorCheckerPrimary
#  define colorCheckerSecondary globalsBlock.colorCheckerSecondary
#  define colorClippingBorder globalsBlock.colorClippingBorder
#  define colorEditMeshMiddle globalsBlock.colorEditMeshMiddle
#  define colorHandleFree globalsBlock.colorHandleFree
#  define colorHandleAuto globalsBlock.colorHandleAuto
#  define colorHandleVect globalsBlock.colorHandleVect
#  define colorHandleAlign globalsBlock.colorHandleAlign
#  define colorHandleAutoclamp globalsBlock.colorHandleAutoclamp
#  define colorHandleSelFree globalsBlock.colorHandleSelFree
#  define colorHandleSelAuto globalsBlock.colorHandleSelAuto
#  define colorHandleSelVect globalsBlock.colorHandleSelVect
#  define colorHandleSelAlign globalsBlock.colorHandleSelAlign
#  define colorHandleSelAutoclamp globalsBlock.colorHandleSelAutoclamp
#  define colorNurbUline globalsBlock.colorNurbUline
#  define colorNurbVline globalsBlock.colorNurbVline
#  define colorNurbSelUline globalsBlock.colorNurbSelUline
#  define colorNurbSelVline globalsBlock.colorNurbSelVline
#  define colorActiveSpline globalsBlock.colorActiveSpline
#  define colorBonePose globalsBlock.colorBonePose
#  define colorBonePoseActive globalsBlock.colorBonePoseActive
#  define colorBonePoseActiveUnsel globalsBlock.colorBonePoseActiveUnsel
#  define colorBonePoseConstraint globalsBlock.colorBonePoseConstraint
#  define colorBonePoseIK globalsBlock.colorBonePoseIK
#  define colorBonePoseSplineIK globalsBlock.colorBonePoseSplineIK
#  define colorBonePoseTarget globalsBlock.colorBonePoseTarget
#  define colorBoneSolid globalsBlock.colorBoneSolid
#  define colorBoneLocked globalsBlock.colorBoneLocked
#  define colorBoneActive globalsBlock.colorBoneActive
#  define colorBoneActiveUnsel globalsBlock.colorBoneActiveUnsel
#  define colorBoneSelect globalsBlock.colorBoneSelect
#  define colorBoneIKLine globalsBlock.colorBoneIKLine
#  define colorBoneIKLineNoTarget globalsBlock.colorBoneIKLineNoTarget
#  define colorBoneIKLineSpline globalsBlock.colorBoneIKLineSpline
#  define colorText globalsBlock.colorText
#  define colorTextHi globalsBlock.colorTextHi
#  define colorBundleSolid globalsBlock.colorBundleSolid
#  define colorMballRadius globalsBlock.colorMballRadius
#  define colorMballRadiusSelect globalsBlock.colorMballRadiusSelect
#  define colorMballStiffness globalsBlock.colorMballStiffness
#  define colorMballStiffnessSelect globalsBlock.colorMballStiffnessSelect
#  define colorCurrentFrame globalsBlock.colorCurrentFrame
#  define colorGrid globalsBlock.colorGrid
#  define colorGridEmphasis globalsBlock.colorGridEmphasis
#  define colorGridAxisX globalsBlock.colorGridAxisX
#  define colorGridAxisY globalsBlock.colorGridAxisY
#  define colorGridAxisZ globalsBlock.colorGridAxisZ
#  define colorFaceBack globalsBlock.colorFaceBack
#  define colorFaceFront globalsBlock.colorFaceFront
#  define colorUVShadow globalsBlock.colorUVShadow
#  define screenVecs globalsBlock.screenVecs
#  define sizeViewport globalsBlock.sizeViewport.xy
#  define sizePixel globalsBlock.sizePixel
#  define pixelFac globalsBlock.pixelFac
#  define sizeObjectCenter globalsBlock.sizeObjectCenter
#  define sizeLightCenter globalsBlock.sizeLightCenter
#  define sizeLightCircle globalsBlock.sizeLightCircle
#  define sizeLightCircleShadow globalsBlock.sizeLightCircleShadow
#  define sizeVertex globalsBlock.sizeVertex
#  define sizeEdge globalsBlock.sizeEdge
#  define sizeEdgeFix globalsBlock.sizeEdgeFix
#  define sizeFaceDot globalsBlock.sizeFaceDot
#  define sizeChecker globalsBlock.sizeChecker
#  define sizeVertexGpencil globalsBlock.sizeVertexGpencil
#endif

/* See: 'draw_cache_impl.h' for matching includes. */
#define VERT_GPENCIL_BEZT_HANDLE (1 << 30)
/* data[0] (1st byte flags) */
#define FACE_ACTIVE (1 << 0)
#define FACE_SELECTED (1 << 1)
#define FACE_FREESTYLE (1 << 2)
#define VERT_UV_SELECT (1 << 3)
#define VERT_UV_PINNED (1 << 4)
#define EDGE_UV_SELECT (1 << 5)
#define FACE_UV_ACTIVE (1 << 6)
#define FACE_UV_SELECT (1 << 7)
/* data[1] (2st byte flags) */
#define VERT_ACTIVE (1 << 0)
#define VERT_SELECTED (1 << 1)
#define VERT_SELECTED_BEZT_HANDLE (1 << 2)
#define EDGE_ACTIVE (1 << 3)
#define EDGE_SELECTED (1 << 4)
#define EDGE_SEAM (1 << 5)
#define EDGE_SHARP (1 << 6)
#define EDGE_FREESTYLE (1 << 7)

#define COMMON_GLOBALS_LIB
