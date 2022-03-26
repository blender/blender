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
 * engine.  */

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
#  define colorWire drw_globals.colorWire
#  define colorWireEdit drw_globals.colorWireEdit
#  define colorActive drw_globals.colorActive
#  define colorSelect drw_globals.colorSelect
#  define colorLibrarySelect drw_globals.colorLibrarySelect
#  define colorLibrary drw_globals.colorLibrary
#  define colorTransform drw_globals.colorTransform
#  define colorLight drw_globals.colorLight
#  define colorSpeaker drw_globals.colorSpeaker
#  define colorCamera drw_globals.colorCamera
#  define colorCameraPath drw_globals.colorCameraPath
#  define colorEmpty drw_globals.colorEmpty
#  define colorVertex drw_globals.colorVertex
#  define colorVertexSelect drw_globals.colorVertexSelect
#  define colorVertexUnreferenced drw_globals.colorVertexUnreferenced
#  define colorVertexMissingData drw_globals.colorVertexMissingData
#  define colorEditMeshActive drw_globals.colorEditMeshActive
#  define colorEdgeSelect drw_globals.colorEdgeSelect
#  define colorEdgeSeam drw_globals.colorEdgeSeam
#  define colorEdgeSharp drw_globals.colorEdgeSharp
#  define colorEdgeCrease drw_globals.colorEdgeCrease
#  define colorEdgeBWeight drw_globals.colorEdgeBWeight
#  define colorEdgeFaceSelect drw_globals.colorEdgeFaceSelect
#  define colorEdgeFreestyle drw_globals.colorEdgeFreestyle
#  define colorFace drw_globals.colorFace
#  define colorFaceSelect drw_globals.colorFaceSelect
#  define colorFaceFreestyle drw_globals.colorFaceFreestyle
#  define colorGpencilVertex drw_globals.colorGpencilVertex
#  define colorGpencilVertexSelect drw_globals.colorGpencilVertexSelect
#  define colorNormal drw_globals.colorNormal
#  define colorVNormal drw_globals.colorVNormal
#  define colorLNormal drw_globals.colorLNormal
#  define colorFaceDot drw_globals.colorFaceDot
#  define colorSkinRoot drw_globals.colorSkinRoot
#  define colorDeselect drw_globals.colorDeselect
#  define colorOutline drw_globals.colorOutline
#  define colorLightNoAlpha drw_globals.colorLightNoAlpha
#  define colorBackground drw_globals.colorBackground
#  define colorBackgroundGradient drw_globals.colorBackgroundGradient
#  define colorCheckerPrimary drw_globals.colorCheckerPrimary
#  define colorCheckerSecondary drw_globals.colorCheckerSecondary
#  define colorClippingBorder drw_globals.colorClippingBorder
#  define colorEditMeshMiddle drw_globals.colorEditMeshMiddle
#  define colorHandleFree drw_globals.colorHandleFree
#  define colorHandleAuto drw_globals.colorHandleAuto
#  define colorHandleVect drw_globals.colorHandleVect
#  define colorHandleAlign drw_globals.colorHandleAlign
#  define colorHandleAutoclamp drw_globals.colorHandleAutoclamp
#  define colorHandleSelFree drw_globals.colorHandleSelFree
#  define colorHandleSelAuto drw_globals.colorHandleSelAuto
#  define colorHandleSelVect drw_globals.colorHandleSelVect
#  define colorHandleSelAlign drw_globals.colorHandleSelAlign
#  define colorHandleSelAutoclamp drw_globals.colorHandleSelAutoclamp
#  define colorNurbUline drw_globals.colorNurbUline
#  define colorNurbVline drw_globals.colorNurbVline
#  define colorNurbSelUline drw_globals.colorNurbSelUline
#  define colorNurbSelVline drw_globals.colorNurbSelVline
#  define colorActiveSpline drw_globals.colorActiveSpline
#  define colorBonePose drw_globals.colorBonePose
#  define colorBonePoseActive drw_globals.colorBonePoseActive
#  define colorBonePoseActiveUnsel drw_globals.colorBonePoseActiveUnsel
#  define colorBonePoseConstraint drw_globals.colorBonePoseConstraint
#  define colorBonePoseIK drw_globals.colorBonePoseIK
#  define colorBonePoseSplineIK drw_globals.colorBonePoseSplineIK
#  define colorBonePoseTarget drw_globals.colorBonePoseTarget
#  define colorBoneSolid drw_globals.colorBoneSolid
#  define colorBoneLocked drw_globals.colorBoneLocked
#  define colorBoneActive drw_globals.colorBoneActive
#  define colorBoneActiveUnsel drw_globals.colorBoneActiveUnsel
#  define colorBoneSelect drw_globals.colorBoneSelect
#  define colorBoneIKLine drw_globals.colorBoneIKLine
#  define colorBoneIKLineNoTarget drw_globals.colorBoneIKLineNoTarget
#  define colorBoneIKLineSpline drw_globals.colorBoneIKLineSpline
#  define colorText drw_globals.colorText
#  define colorTextHi drw_globals.colorTextHi
#  define colorBundleSolid drw_globals.colorBundleSolid
#  define colorMballRadius drw_globals.colorMballRadius
#  define colorMballRadiusSelect drw_globals.colorMballRadiusSelect
#  define colorMballStiffness drw_globals.colorMballStiffness
#  define colorMballStiffnessSelect drw_globals.colorMballStiffnessSelect
#  define colorCurrentFrame drw_globals.colorCurrentFrame
#  define colorGrid drw_globals.colorGrid
#  define colorGridEmphasis drw_globals.colorGridEmphasis
#  define colorGridAxisX drw_globals.colorGridAxisX
#  define colorGridAxisY drw_globals.colorGridAxisY
#  define colorGridAxisZ drw_globals.colorGridAxisZ
#  define colorFaceBack drw_globals.colorFaceBack
#  define colorFaceFront drw_globals.colorFaceFront
#  define colorUVShadow drw_globals.colorUVShadow
#  define screenVecs drw_globals.screenVecs
#  define sizeViewport drw_globals.sizeViewport
#  define sizePixel drw_globals.sizePixel
#  define pixelFac drw_globals.pixelFac
#  define sizeObjectCenter drw_globals.sizeObjectCenter
#  define sizeLightCenter drw_globals.sizeLightCenter
#  define sizeLightCircle drw_globals.sizeLightCircle
#  define sizeLightCircleShadow drw_globals.sizeLightCircleShadow
#  define sizeVertex drw_globals.sizeVertex
#  define sizeEdge drw_globals.sizeEdge
#  define sizeEdgeFix drw_globals.sizeEdgeFix
#  define sizeFaceDot drw_globals.sizeFaceDot
#  define sizeChecker drw_globals.sizeChecker
#  define sizeVertexGpencil drw_globals.sizeVertexGpencil
#endif
