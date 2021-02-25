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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_kdopbvh.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "IMB_imbuf.h"

#include "BKE_main.h"

#include "BKE_anim_data.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_texture.h"

#include "RE_texture.h"

#include "BLO_read_write.h"

static void texture_init_data(ID *id)
{
  Tex *texture = (Tex *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(texture, id));

  MEMCPY_STRUCT_AFTER(texture, DNA_struct_default_get(Tex), id);

  BKE_imageuser_default(&texture->iuser);
}

static void texture_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Tex *texture_dst = (Tex *)id_dst;
  const Tex *texture_src = (const Tex *)id_src;

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (!BKE_texture_is_image_user(texture_src)) {
    texture_dst->ima = NULL;
  }

  if (texture_dst->coba) {
    texture_dst->coba = MEM_dupallocN(texture_dst->coba);
  }
  if (texture_src->nodetree) {
    if (texture_src->nodetree->execdata) {
      ntreeTexEndExecTree(texture_src->nodetree->execdata);
    }

    if (is_localized) {
      texture_dst->nodetree = ntreeLocalize(texture_src->nodetree);
    }
    else {
      BKE_id_copy_ex(
          bmain, (ID *)texture_src->nodetree, (ID **)&texture_dst->nodetree, flag_private_id_data);
    }
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&texture_dst->id, &texture_src->id);
  }
  else {
    texture_dst->preview = NULL;
  }
}

static void texture_free_data(ID *id)
{
  Tex *texture = (Tex *)id;

  /* is no lib link block, but texture extension */
  if (texture->nodetree) {
    ntreeFreeEmbeddedTree(texture->nodetree);
    MEM_freeN(texture->nodetree);
    texture->nodetree = NULL;
  }

  MEM_SAFE_FREE(texture->coba);

  BKE_icon_id_delete((ID *)texture);
  BKE_previewimg_free(&texture->preview);
}

static void texture_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Tex *texture = (Tex *)id;
  if (texture->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&texture->nodetree);
  }
  BKE_LIB_FOREACHID_PROCESS(data, texture->ima, IDWALK_CB_USER);
}

static void texture_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Tex *tex = (Tex *)id;
  if (tex->id.us > 0 || BLO_write_is_undo(writer)) {
    /* write LibData */
    BLO_write_id_struct(writer, Tex, id_address, &tex->id);
    BKE_id_blend_write(writer, &tex->id);

    if (tex->adt) {
      BKE_animdata_blend_write(writer, tex->adt);
    }

    /* direct data */
    if (tex->coba) {
      BLO_write_struct(writer, ColorBand, tex->coba);
    }

    /* nodetree is integral part of texture, no libdata */
    if (tex->nodetree) {
      BLO_write_struct(writer, bNodeTree, tex->nodetree);
      ntreeBlendWrite(writer, tex->nodetree);
    }

    BKE_previewimg_blend_write(writer, tex->preview);
  }
}

static void texture_blend_read_data(BlendDataReader *reader, ID *id)
{
  Tex *tex = (Tex *)id;
  BLO_read_data_address(reader, &tex->adt);
  BKE_animdata_blend_read_data(reader, tex->adt);

  BLO_read_data_address(reader, &tex->coba);

  BLO_read_data_address(reader, &tex->preview);
  BKE_previewimg_blend_read(reader, tex->preview);

  tex->iuser.ok = 1;
  tex->iuser.scene = NULL;
}

static void texture_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Tex *tex = (Tex *)id;
  BLO_read_id_address(reader, tex->id.lib, &tex->ima);
  BLO_read_id_address(reader, tex->id.lib, &tex->ipo); /* XXX deprecated - old animation system */
}

static void texture_blend_read_expand(BlendExpander *expander, ID *id)
{
  Tex *tex = (Tex *)id;
  BLO_expand(expander, tex->ima);
  BLO_expand(expander, tex->ipo); /* XXX deprecated - old animation system */
}

IDTypeInfo IDType_ID_TE = {
    .id_code = ID_TE,
    .id_filter = FILTER_ID_TE,
    .main_listbase_index = INDEX_ID_TE,
    .struct_size = sizeof(Tex),
    .name = "Texture",
    .name_plural = "textures",
    .translation_context = BLT_I18NCONTEXT_ID_TEXTURE,
    .flags = 0,

    .init_data = texture_init_data,
    .copy_data = texture_copy_data,
    .free_data = texture_free_data,
    .make_local = NULL,
    .foreach_id = texture_foreach_id,
    .foreach_cache = NULL,
    .owner_get = NULL,

    .blend_write = texture_blend_write,
    .blend_read_data = texture_blend_read_data,
    .blend_read_lib = texture_blend_read_lib,
    .blend_read_expand = texture_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/* Utils for all IDs using those texture slots. */
void BKE_texture_mtex_foreach_id(LibraryForeachIDData *data, MTex *mtex)
{
  BKE_LIB_FOREACHID_PROCESS(data, mtex->object, IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_PROCESS(data, mtex->tex, IDWALK_CB_USER);
}

/* ****************** Mapping ******************* */

TexMapping *BKE_texture_mapping_add(int type)
{
  TexMapping *texmap = MEM_callocN(sizeof(TexMapping), "TexMapping");

  BKE_texture_mapping_default(texmap, type);

  return texmap;
}

void BKE_texture_mapping_default(TexMapping *texmap, int type)
{
  memset(texmap, 0, sizeof(TexMapping));

  texmap->size[0] = texmap->size[1] = texmap->size[2] = 1.0f;
  texmap->max[0] = texmap->max[1] = texmap->max[2] = 1.0f;
  unit_m4(texmap->mat);

  texmap->projx = PROJ_X;
  texmap->projy = PROJ_Y;
  texmap->projz = PROJ_Z;
  texmap->mapping = MTEX_FLAT;
  texmap->type = type;
}

void BKE_texture_mapping_init(TexMapping *texmap)
{
  float smat[4][4], rmat[4][4], tmat[4][4], proj[4][4], size[3];

  if (texmap->projx == PROJ_X && texmap->projy == PROJ_Y && texmap->projz == PROJ_Z &&
      is_zero_v3(texmap->loc) && is_zero_v3(texmap->rot) && is_one_v3(texmap->size)) {
    unit_m4(texmap->mat);

    texmap->flag |= TEXMAP_UNIT_MATRIX;
  }
  else {
    /* axis projection */
    zero_m4(proj);
    proj[3][3] = 1.0f;

    if (texmap->projx != PROJ_N) {
      proj[texmap->projx - 1][0] = 1.0f;
    }
    if (texmap->projy != PROJ_N) {
      proj[texmap->projy - 1][1] = 1.0f;
    }
    if (texmap->projz != PROJ_N) {
      proj[texmap->projz - 1][2] = 1.0f;
    }

    /* scale */
    copy_v3_v3(size, texmap->size);

    if (ELEM(texmap->type, TEXMAP_TYPE_TEXTURE, TEXMAP_TYPE_NORMAL)) {
      /* keep matrix invertible */
      if (fabsf(size[0]) < 1e-5f) {
        size[0] = signf(size[0]) * 1e-5f;
      }
      if (fabsf(size[1]) < 1e-5f) {
        size[1] = signf(size[1]) * 1e-5f;
      }
      if (fabsf(size[2]) < 1e-5f) {
        size[2] = signf(size[2]) * 1e-5f;
      }
    }

    size_to_mat4(smat, texmap->size);

    /* rotation */
    eul_to_mat4(rmat, texmap->rot);

    /* translation */
    unit_m4(tmat);
    copy_v3_v3(tmat[3], texmap->loc);

    if (texmap->type == TEXMAP_TYPE_TEXTURE) {
      /* to transform a texture, the inverse transform needs
       * to be applied to the texture coordinate */
      mul_m4_series(texmap->mat, tmat, rmat, smat);
      invert_m4(texmap->mat);
    }
    else if (texmap->type == TEXMAP_TYPE_POINT) {
      /* forward transform */
      mul_m4_series(texmap->mat, tmat, rmat, smat);
    }
    else if (texmap->type == TEXMAP_TYPE_VECTOR) {
      /* no translation for vectors */
      mul_m4_m4m4(texmap->mat, rmat, smat);
    }
    else if (texmap->type == TEXMAP_TYPE_NORMAL) {
      /* no translation for normals, and inverse transpose */
      mul_m4_m4m4(texmap->mat, rmat, smat);
      invert_m4(texmap->mat);
      transpose_m4(texmap->mat);
    }

    /* projection last */
    mul_m4_m4m4(texmap->mat, texmap->mat, proj);

    texmap->flag &= ~TEXMAP_UNIT_MATRIX;
  }
}

ColorMapping *BKE_texture_colormapping_add(void)
{
  ColorMapping *colormap = MEM_callocN(sizeof(ColorMapping), "ColorMapping");

  BKE_texture_colormapping_default(colormap);

  return colormap;
}

void BKE_texture_colormapping_default(ColorMapping *colormap)
{
  memset(colormap, 0, sizeof(ColorMapping));

  BKE_colorband_init(&colormap->coba, true);

  colormap->bright = 1.0;
  colormap->contrast = 1.0;
  colormap->saturation = 1.0;

  colormap->blend_color[0] = 0.8f;
  colormap->blend_color[1] = 0.8f;
  colormap->blend_color[2] = 0.8f;
  colormap->blend_type = MA_RAMP_BLEND;
  colormap->blend_factor = 0.0f;
}

/* ******************* TEX ************************ */

/* ------------------------------------------------------------------------- */

void BKE_texture_default(Tex *tex)
{
  texture_init_data(&tex->id);
}

void BKE_texture_type_set(Tex *tex, int type)
{
  tex->type = type;
}

/* ------------------------------------------------------------------------- */

Tex *BKE_texture_add(Main *bmain, const char *name)
{
  Tex *tex;

  tex = BKE_id_new(bmain, ID_TE, name);

  return tex;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_mtex_default(MTex *mtex)
{
  memcpy(mtex, DNA_struct_default_get(MTex), sizeof(*mtex));
}

/* ------------------------------------------------------------------------- */

MTex *BKE_texture_mtex_add(void)
{
  MTex *mtex;

  mtex = MEM_callocN(sizeof(MTex), "BKE_texture_mtex_add");

  BKE_texture_mtex_default(mtex);

  return mtex;
}

/* slot -1 for first free ID */
MTex *BKE_texture_mtex_add_id(ID *id, int slot)
{
  MTex **mtex_ar;
  short act;

  give_active_mtex(id, &mtex_ar, &act);

  if (mtex_ar == NULL) {
    return NULL;
  }

  if (slot == -1) {
    /* find first free */
    int i;
    for (i = 0; i < MAX_MTEX; i++) {
      if (!mtex_ar[i]) {
        slot = i;
        break;
      }
    }
    if (slot == -1) {
      return NULL;
    }
  }
  else {
    /* make sure slot is valid */
    if (slot < 0 || slot >= MAX_MTEX) {
      return NULL;
    }
  }

  if (mtex_ar[slot]) {
    id_us_min((ID *)mtex_ar[slot]->tex);
    MEM_freeN(mtex_ar[slot]);
    mtex_ar[slot] = NULL;
  }

  mtex_ar[slot] = BKE_texture_mtex_add();

  return mtex_ar[slot];
}

/* ------------------------------------------------------------------------- */

Tex *give_current_linestyle_texture(FreestyleLineStyle *linestyle)
{
  MTex *mtex = NULL;
  Tex *tex = NULL;

  if (linestyle) {
    mtex = linestyle->mtex[(int)(linestyle->texact)];
    if (mtex) {
      tex = mtex->tex;
    }
  }

  return tex;
}

void set_current_linestyle_texture(FreestyleLineStyle *linestyle, Tex *newtex)
{
  int act = linestyle->texact;

  if (linestyle->mtex[act] && linestyle->mtex[act]->tex) {
    id_us_min(&linestyle->mtex[act]->tex->id);
  }

  if (newtex) {
    if (!linestyle->mtex[act]) {
      linestyle->mtex[act] = BKE_texture_mtex_add();
      linestyle->mtex[act]->texco = TEXCO_STROKE;
    }

    linestyle->mtex[act]->tex = newtex;
    id_us_plus(&newtex->id);
  }
  else if (linestyle->mtex[act]) {
    MEM_freeN(linestyle->mtex[act]);
    linestyle->mtex[act] = NULL;
  }
}

bool give_active_mtex(ID *id, MTex ***mtex_ar, short *act)
{
  switch (GS(id->name)) {
    case ID_LS:
      *mtex_ar = ((FreestyleLineStyle *)id)->mtex;
      if (act) {
        *act = (((FreestyleLineStyle *)id)->texact);
      }
      break;
    case ID_PA:
      *mtex_ar = ((ParticleSettings *)id)->mtex;
      if (act) {
        *act = (((ParticleSettings *)id)->texact);
      }
      break;
    default:
      *mtex_ar = NULL;
      if (act) {
        *act = 0;
      }
      return false;
  }

  return true;
}

void set_active_mtex(ID *id, short act)
{
  if (act < 0) {
    act = 0;
  }
  else if (act >= MAX_MTEX) {
    act = MAX_MTEX - 1;
  }

  switch (GS(id->name)) {
    case ID_LS:
      ((FreestyleLineStyle *)id)->texact = act;
      break;
    case ID_PA:
      ((ParticleSettings *)id)->texact = act;
      break;
    default:
      break;
  }
}

Tex *give_current_brush_texture(Brush *br)
{
  return br->mtex.tex;
}

void set_current_brush_texture(Brush *br, Tex *newtex)
{
  if (br->mtex.tex) {
    id_us_min(&br->mtex.tex->id);
  }

  if (newtex) {
    br->mtex.tex = newtex;
    id_us_plus(&newtex->id);
  }
}

Tex *give_current_particle_texture(ParticleSettings *part)
{
  MTex *mtex = NULL;
  Tex *tex = NULL;

  if (!part) {
    return NULL;
  }

  mtex = part->mtex[(int)(part->texact)];
  if (mtex) {
    tex = mtex->tex;
  }

  return tex;
}

void set_current_particle_texture(ParticleSettings *part, Tex *newtex)
{
  int act = part->texact;

  if (part->mtex[act] && part->mtex[act]->tex) {
    id_us_min(&part->mtex[act]->tex->id);
  }

  if (newtex) {
    if (!part->mtex[act]) {
      part->mtex[act] = BKE_texture_mtex_add();
      part->mtex[act]->texco = TEXCO_ORCO;
      part->mtex[act]->blendtype = MTEX_MUL;
    }

    part->mtex[act]->tex = newtex;
    id_us_plus(&newtex->id);
  }
  else if (part->mtex[act]) {
    MEM_freeN(part->mtex[act]);
    part->mtex[act] = NULL;
  }
}

/* ------------------------------------------------------------------------- */

void BKE_texture_pointdensity_init_data(PointDensity *pd)
{
  pd->flag = 0;
  pd->radius = 0.3f;
  pd->falloff_type = TEX_PD_FALLOFF_STD;
  pd->falloff_softness = 2.0;
  pd->source = TEX_PD_PSYS;
  pd->point_tree = NULL;
  pd->point_data = NULL;
  pd->noise_size = 0.5f;
  pd->noise_depth = 1;
  pd->noise_fac = 1.0f;
  pd->noise_influence = TEX_PD_NOISE_STATIC;
  pd->coba = BKE_colorband_add(true);
  pd->speed_scale = 1.0f;
  pd->totpoints = 0;
  pd->object = NULL;
  pd->psys = 0;
  pd->psys_cache_space = TEX_PD_WORLDSPACE;
  pd->falloff_curve = BKE_curvemapping_add(1, 0, 0, 1, 1);

  pd->falloff_curve->preset = CURVE_PRESET_LINE;
  pd->falloff_curve->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  BKE_curvemap_reset(pd->falloff_curve->cm,
                     &pd->falloff_curve->clipr,
                     pd->falloff_curve->preset,
                     CURVEMAP_SLOPE_POSITIVE);
  BKE_curvemapping_changed(pd->falloff_curve, false);
}

PointDensity *BKE_texture_pointdensity_add(void)
{
  PointDensity *pd = MEM_callocN(sizeof(PointDensity), "pointdensity");
  BKE_texture_pointdensity_init_data(pd);
  return pd;
}

PointDensity *BKE_texture_pointdensity_copy(const PointDensity *pd, const int UNUSED(flag))
{
  PointDensity *pdn;

  pdn = MEM_dupallocN(pd);
  pdn->point_tree = NULL;
  pdn->point_data = NULL;
  if (pdn->coba) {
    pdn->coba = MEM_dupallocN(pdn->coba);
  }
  pdn->falloff_curve = BKE_curvemapping_copy(pdn->falloff_curve); /* can be NULL */
  return pdn;
}

void BKE_texture_pointdensity_free_data(PointDensity *pd)
{
  if (pd->point_tree) {
    BLI_bvhtree_free(pd->point_tree);
    pd->point_tree = NULL;
  }
  if (pd->point_data) {
    MEM_freeN(pd->point_data);
    pd->point_data = NULL;
  }
  if (pd->coba) {
    MEM_freeN(pd->coba);
    pd->coba = NULL;
  }

  BKE_curvemapping_free(pd->falloff_curve); /* can be NULL */
}

void BKE_texture_pointdensity_free(PointDensity *pd)
{
  BKE_texture_pointdensity_free_data(pd);
  MEM_freeN(pd);
}
/* ------------------------------------------------------------------------- */

/**
 * \returns true if this texture can use its #Texture.ima (even if its NULL)
 */
bool BKE_texture_is_image_user(const struct Tex *tex)
{
  switch (tex->type) {
    case TEX_IMAGE: {
      return true;
    }
  }

  return false;
}

/* ------------------------------------------------------------------------- */
bool BKE_texture_dependsOnTime(const struct Tex *texture)
{
  if (texture->ima && BKE_image_is_animated(texture->ima)) {
    return true;
  }
  if (texture->adt) {
    /* assume anything in adt means the texture is animated */
    return true;
  }
  if (texture->type == TEX_NOISE) {
    /* noise always varies with time */
    return true;
  }
  return false;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_get_value_ex(const Scene *scene,
                              Tex *texture,
                              const float *tex_co,
                              TexResult *texres,
                              struct ImagePool *pool,
                              bool use_color_management)
{
  int result_type;
  bool do_color_manage = false;

  if (scene && use_color_management) {
    do_color_manage = BKE_scene_check_color_management_enabled(scene);
  }

  /* no node textures for now */
  result_type = multitex_ext_safe(texture, tex_co, texres, pool, do_color_manage, false);

  /* if the texture gave an RGB value, we assume it didn't give a valid
   * intensity, since this is in the context of modifiers don't use perceptual color conversion.
   * if the texture didn't give an RGB value, copy the intensity across
   */
  if (result_type & TEX_RGB) {
    texres->tin = (1.0f / 3.0f) * (texres->tr + texres->tg + texres->tb);
  }
  else {
    copy_v3_fl(&texres->tr, texres->tin);
  }
}

void BKE_texture_get_value(const Scene *scene,
                           Tex *texture,
                           const float *tex_co,
                           TexResult *texres,
                           bool use_color_management)
{
  BKE_texture_get_value_ex(scene, texture, tex_co, texres, NULL, use_color_management);
}

static void texture_nodes_fetch_images_for_pool(Tex *texture,
                                                bNodeTree *ntree,
                                                struct ImagePool *pool)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == SH_NODE_TEX_IMAGE && node->id != NULL) {
      Image *image = (Image *)node->id;
      BKE_image_pool_acquire_ibuf(image, &texture->iuser, pool);
    }
    else if (node->type == NODE_GROUP && node->id != NULL) {
      /* TODO(sergey): Do we need to control recursion here? */
      bNodeTree *nested_tree = (bNodeTree *)node->id;
      texture_nodes_fetch_images_for_pool(texture, nested_tree, pool);
    }
  }
}

/* Make sure all images used by texture are loaded into pool. */
void BKE_texture_fetch_images_for_pool(Tex *texture, struct ImagePool *pool)
{
  if (texture->nodetree != NULL) {
    texture_nodes_fetch_images_for_pool(texture, texture->nodetree, pool);
  }
  else {
    if (texture->type == TEX_IMAGE) {
      if (texture->ima != NULL) {
        BKE_image_pool_acquire_ibuf(texture->ima, &texture->iuser, pool);
      }
    }
  }
}
