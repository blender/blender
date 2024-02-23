/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Stubs to reduce linking time for shader_builder.
 */

#include "BLI_utildefines.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_node.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DNA_userdef_types.h"

#include "NOD_shader.h"

#include "DRW_engine.hh"

#include "bmesh.hh"

#include "UI_resources.hh"

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
/** \name Stubs of UI_resources.hh
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
/** \name Stubs of BKE_paint.hh
 * \{ */

void BKE_paint_face_set_overlay_color_get(const int /*face_set*/,
                                          const int /*seed*/,
                                          uchar[4] /*col*/)
{
  BLI_assert_unreachable();
}

bool paint_is_grid_face_hidden(blender::BoundedBitSpan /*grid_hidden*/,
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

extern "C" void BKE_material_defaults_free_gpu()
{
  /* This function is reachable via GPU_exit. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_customdata.hh
 * \{ */

int CustomData_get_offset(const struct CustomData * /*data*/, eCustomDataType /*type*/)
{
  BLI_assert_unreachable();
  return 0;
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
/** \name Stubs of BKE_node.h
 * \{ */
extern "C" void ntreeGPUMaterialNodes(struct bNodeTree * /*localtree*/,
                                      struct GPUMaterial * /*mat*/)
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
/** \name Stubs of DRW_engine.hh
 * \{ */
extern void DRW_deferred_shader_remove(struct GPUMaterial * /*mat*/)
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of IMB_imbuf.hh
 * \{ */
struct ImBuf *IMB_ibImageFromMemory(const unsigned char * /*mem*/,
                                    size_t /*size*/,
                                    int /*flags*/,
                                    char /*colorspace*/[IM_MAX_SPACE],
                                    const char * /*descr*/)
{
  BLI_assert_unreachable();
  return nullptr;
}

struct ImBuf *IMB_allocFromBuffer(const uint8_t * /*rect*/,
                                  const float * /*rectf*/,
                                  unsigned int /*w*/,
                                  unsigned int /*h*/,
                                  unsigned int /*channels*/)
{
  BLI_assert_unreachable();
  return nullptr;
}

bool IMB_saveiff(struct ImBuf * /*ibuf*/, const char * /*filepath*/, int /*flags*/)
{
  BLI_assert_unreachable();
  return false;
}

/** \} */
