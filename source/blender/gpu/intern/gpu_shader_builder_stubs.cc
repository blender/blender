/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Stubs to reduce linking time for shader_builder.
 */

#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "DNA_userdef_types.h"

#include "NOD_shader.h"

#include "DRW_engine.h"

#include "bmesh.h"

#include "UI_resources.h"

extern "C" {

Global G;
UserDef U;

/* -------------------------------------------------------------------- */
/** \name Stubs of BLI_imbuf_types.h
 * \{ */

void IMB_freeImBuf(ImBuf * /*ibuf*/)
{
  BLI_assert_unreachable();
}

struct ImBuf *IMB_allocImBuf(unsigned int /*x*/,
                             unsigned int /*y*/,
                             unsigned char /*planes*/,
                             unsigned int /*flags*/)
{
  BLI_assert_unreachable();
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of UI_resources.h
 * \{ */

void UI_GetThemeColor4fv(int /*colorid*/, float[4] /*col*/)
{
  BLI_assert_unreachable();
}

void UI_GetThemeColor3fv(int /*colorid*/, float[3] /*col*/)
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorShade4fv(int /*colorid*/, int /*offset*/, float[4] /*col*/)
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorShadeAlpha4fv(int /*colorid*/,
                                   int /*coloffset*/,
                                   int /*alphaoffset*/,
                                   float[4] /*col*/)
{
  BLI_assert_unreachable();
}
void UI_GetThemeColorBlendShade4fv(
    int /*colorid1*/, int /*colorid2*/, float /*fac*/, int /*offset*/, float[4] /*col*/)
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorBlend3ubv(int /*colorid1*/, int /*colorid2*/, float /*fac*/, uchar[3] /*col*/)
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorShadeAlpha4ubv(int /*colorid*/,
                                    int /*coloffset*/,
                                    int /*alphaoffset*/,
                                    uchar[4] /*col*/)
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_attribute.h
 * \{ */

eAttrDomain BKE_id_attribute_domain(const struct ID * /*id*/,
                                    const struct CustomDataLayer * /*layer*/)
{
  return ATTR_DOMAIN_AUTO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_paint.h
 * \{ */
bool paint_is_face_hidden(const int * /*looptri_polys*/,
                          const bool * /*hide_poly*/,
                          int /*tri_index*/)
{
  BLI_assert_unreachable();
  return false;
}

void BKE_paint_face_set_overlay_color_get(const int /*face_set*/,
                                          const int /*seed*/,
                                          uchar[4] /*col*/)
{
  BLI_assert_unreachable();
}

bool paint_is_grid_face_hidden(const uint * /*grid_hidden*/,
                               int /*gridsize*/,
                               int /*x*/,
                               int /*y*/)
{
  BLI_assert_unreachable();
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_mesh.h
 * \{ */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_material.h
 * \{ */

void BKE_material_defaults_free_gpu()
{
  /* This function is reachable via GPU_exit. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_customdata.h
 * \{ */

int CustomData_get_offset(const struct CustomData * /*data*/, eCustomDataType /*type*/)
{
  BLI_assert_unreachable();
  return 0;
}

int CustomData_get_named_layer_index(const struct CustomData * /*data*/,
                                     eCustomDataType /*type*/,
                                     const char * /*name*/)
{
  return -1;
}

int CustomData_get_active_layer_index(const struct CustomData * /*data*/, eCustomDataType /*type*/)
{
  return -1;
}

int CustomData_get_render_layer_index(const struct CustomData * /*data*/, eCustomDataType /*type*/)
{
  return -1;
}

bool CustomData_has_layer(const struct CustomData * /*data*/, eCustomDataType /*type*/)
{
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_pbvh.h
 * \{ */

int BKE_pbvh_count_grid_quads(BLI_bitmap ** /*grid_hidden*/,
                              const int * /*grid_indices*/,
                              int /*totgrid*/,
                              int /*gridsize*/,
                              int /*display_gridsize*/)
{
  BLI_assert_unreachable();
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_subdiv_ccg.h
 * \{ */
int BKE_subdiv_ccg_grid_to_face_index(const SubdivCCG * /*subdiv_ccg*/, const int /*grid_index*/)
{
  BLI_assert_unreachable();
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_node.h
 * \{ */
void ntreeGPUMaterialNodes(struct bNodeTree * /*localtree*/, struct GPUMaterial * /*mat*/)
{
  BLI_assert_unreachable();
}

struct bNodeTree *ntreeLocalize(struct bNodeTree * /*ntree*/)
{
  BLI_assert_unreachable();
  return nullptr;
}

void ntreeFreeLocalTree(struct bNodeTree * /*ntree*/)
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of bmesh.h
 * \{ */
void BM_face_as_array_vert_tri(BMFace * /*f*/, BMVert *[3] /*r_verts*/)
{
  BLI_assert_unreachable();
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of DRW_engine.h
 * \{ */
void DRW_deferred_shader_remove(struct GPUMaterial * /*mat*/)
{
  BLI_assert_unreachable();
}

void DRW_cdlayer_attr_aliases_add(struct GPUVertFormat * /*format*/,
                                  const char * /*base_name*/,
                                  const struct CustomData * /*data*/,
                                  const struct CustomDataLayer * /*cl*/,
                                  bool /*is_active_render*/,
                                  bool /*is_active_layer*/)
{
}

/** \} */
}
