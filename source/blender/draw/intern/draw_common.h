/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRAW_COMMON_H__
#define __DRAW_COMMON_H__

struct DRWPass;
struct DRWShadingGroup;
struct GPUMaterial;
struct ModifierData;
struct Object;
struct ParticleSystem;
struct ViewLayer;

#define UBO_FIRST_COLOR colorWire
#define UBO_LAST_COLOR colorFaceFront

/* Used as ubo but colors can be directly referenced as well */
/* Keep in sync with: common_globals_lib.glsl (globalsBlock) */
/* NOTE! Also keep all color as vec4 and between UBO_FIRST_COLOR and UBO_LAST_COLOR */
typedef struct GlobalsUboStorage {
  /* UBOs data needs to be 16 byte aligned (size of vec4) */
  float colorWire[4];
  float colorWireEdit[4];
  float colorActive[4];
  float colorSelect[4];
  float colorDupliSelect[4];
  float colorDupli[4];
  float colorLibrarySelect[4];
  float colorLibrary[4];
  float colorTransform[4];
  float colorLight[4];
  float colorSpeaker[4];
  float colorCamera[4];
  float colorCameraPath[4];
  float colorEmpty[4];
  float colorVertex[4];
  float colorVertexSelect[4];
  float colorVertexUnreferenced[4];
  float colorVertexMissingData[4];
  float colorEditMeshActive[4];
  float colorEdgeSelect[4];
  float colorEdgeSeam[4];
  float colorEdgeSharp[4];
  float colorEdgeCrease[4];
  float colorEdgeBWeight[4];
  float colorEdgeFaceSelect[4];
  float colorEdgeFreestyle[4];
  float colorFace[4];
  float colorFaceSelect[4];
  float colorFaceFreestyle[4];
  float colorGpencilVertex[4];
  float colorGpencilVertexSelect[4];
  float colorNormal[4];
  float colorVNormal[4];
  float colorLNormal[4];
  float colorFaceDot[4];
  float colorSkinRoot[4];

  float colorDeselect[4];
  float colorOutline[4];
  float colorLightNoAlpha[4];

  float colorBackground[4];
  float colorBackgroundGradient[4];
  float colorCheckerPrimary[4];
  float colorCheckerSecondary[4];
  float colorClippingBorder[4];
  float colorEditMeshMiddle[4];

  float colorHandleFree[4];
  float colorHandleAuto[4];
  float colorHandleVect[4];
  float colorHandleAlign[4];
  float colorHandleAutoclamp[4];
  float colorHandleSelFree[4];
  float colorHandleSelAuto[4];
  float colorHandleSelVect[4];
  float colorHandleSelAlign[4];
  float colorHandleSelAutoclamp[4];
  float colorNurbUline[4];
  float colorNurbVline[4];
  float colorNurbSelUline[4];
  float colorNurbSelVline[4];
  float colorActiveSpline[4];

  float colorBonePose[4];
  float colorBonePoseActive[4];
  float colorBonePoseActiveUnsel[4];
  float colorBonePoseConstraint[4];
  float colorBonePoseIK[4];
  float colorBonePoseSplineIK[4];
  float colorBonePoseTarget[4];
  float colorBoneSolid[4];
  float colorBoneLocked[4];
  float colorBoneActive[4];
  float colorBoneActiveUnsel[4];
  float colorBoneSelect[4];
  float colorBoneIKLine[4];
  float colorBoneIKLineNoTarget[4];
  float colorBoneIKLineSpline[4];

  float colorText[4];
  float colorTextHi[4];

  float colorBundleSolid[4];

  float colorMballRadius[4];
  float colorMballRadiusSelect[4];
  float colorMballStiffness[4];
  float colorMballStiffnessSelect[4];

  float colorCurrentFrame[4];

  float colorGrid[4];
  float colorGridEmphasise[4];
  float colorGridAxisX[4];
  float colorGridAxisY[4];
  float colorGridAxisZ[4];

  float colorFaceBack[4];
  float colorFaceFront[4];

  /* NOTE! Put all color before UBO_LAST_COLOR */
  float screenVecs[2][4];                    /* padded as vec4  */
  float sizeViewport[2], sizeViewportInv[2]; /* packed as vec4 in glsl */

  /* Pack individual float at the end of the buffer to avoid alignment errors */
  float sizePixel, pixelFac;
  float sizeObjectCenter, sizeLightCenter, sizeLightCircle, sizeLightCircleShadow;
  float sizeVertex, sizeEdge, sizeEdgeFix, sizeFaceDot;
  float sizeChecker;

  float pad_globalsBlock;
} GlobalsUboStorage;
/* Keep in sync with globalsBlock in shaders */
BLI_STATIC_ASSERT_ALIGN(GlobalsUboStorage, 16)

void DRW_globals_update(void);
void DRW_globals_free(void);

struct DRWView *DRW_view_create_with_zoffset(const struct DRWView *parent_view,
                                             const RegionView3D *rv3d,
                                             float offset);

int DRW_object_wire_theme_get(struct Object *ob, struct ViewLayer *view_layer, float **r_color);
float *DRW_color_background_blend_get(int theme_id);

bool DRW_object_is_flat(Object *ob, int *r_axis);
bool DRW_object_axis_orthogonal_to_view(Object *ob, int axis);

/* draw_hair.c */

/* This creates a shading group with display hairs.
 * The draw call is already added by this function, just add additional uniforms. */
struct DRWShadingGroup *DRW_shgroup_hair_create_sub(struct Object *object,
                                                    struct ParticleSystem *psys,
                                                    struct ModifierData *md,
                                                    struct DRWShadingGroup *shgrp);
struct GPUVertBuf *DRW_hair_pos_buffer_get(struct Object *object,
                                           struct ParticleSystem *psys,
                                           struct ModifierData *md);
void DRW_hair_duplimat_get(struct Object *object,
                           struct ParticleSystem *psys,
                           struct ModifierData *md,
                           float (*dupli_mat)[4]);

void DRW_hair_init(void);
void DRW_hair_update(void);
void DRW_hair_free(void);

/* draw_common.c */
struct DRW_Global {
  /** If needed, contains all global/Theme colors
   * Add needed theme colors / values to DRW_globals_update() and update UBO
   * Not needed for constant color. */
  GlobalsUboStorage block;
  /** Define "globalsBlock" uniform for 'block'.  */
  struct GPUUniformBuffer *block_ubo;

  struct GPUTexture *ramp;
  struct GPUTexture *weight_ramp;

  struct GPUUniformBuffer *view_ubo;
};
extern struct DRW_Global G_draw;

#endif /* __DRAW_COMMON_H__ */
