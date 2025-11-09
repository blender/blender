/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_kdopbvh.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "BKE_node_legacy_types.hh"
#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_defaults.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_texture_types.h"

#include "BKE_brush.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_node_runtime.hh"
#include "BKE_preview_image.hh"
#include "BKE_texture.h"

#include "NOD_texture.h"

#include "RE_texture.h"

#include "DRW_engine.hh"

#include "BLO_read_write.hh"

static void texture_init_data(ID *id)
{
  Tex *texture = (Tex *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(texture, id));

  MEMCPY_STRUCT_AFTER(texture, DNA_struct_default_get(Tex), id);

  BKE_imageuser_default(&texture->iuser);
}

static void texture_copy_data(Main *bmain,
                              std::optional<Library *> owner_library,
                              ID *id_dst,
                              const ID *id_src,
                              const int flag)
{
  Tex *texture_dst = (Tex *)id_dst;
  const Tex *texture_src = (const Tex *)id_src;

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* Never handle user-count here for own sub-data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;
  /* Always need allocation of the embedded ID data. */
  const int flag_embedded_id_data = flag_subdata & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (!BKE_texture_is_image_user(texture_src)) {
    texture_dst->ima = nullptr;
  }

  if (texture_dst->coba) {
    texture_dst->coba = static_cast<ColorBand *>(MEM_dupallocN(texture_dst->coba));
  }
  if (texture_src->nodetree) {
    if (texture_src->nodetree->runtime->execdata) {
      ntreeTexEndExecTree(texture_src->nodetree->runtime->execdata);
    }

    if (is_localized) {
      texture_dst->nodetree = blender::bke::node_tree_localize(texture_src->nodetree,
                                                               &texture_dst->id);
    }
    else {
      BKE_id_copy_in_lib(bmain,
                         owner_library,
                         &texture_src->nodetree->id,
                         &texture_dst->id,
                         reinterpret_cast<ID **>(&texture_dst->nodetree),
                         flag_embedded_id_data);
    }
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&texture_dst->id, &texture_src->id);
  }
  else {
    texture_dst->preview = nullptr;
  }
}

static void texture_free_data(ID *id)
{
  Tex *texture = (Tex *)id;

  /* is no lib link block, but texture extension */
  if (texture->nodetree) {
    blender::bke::node_tree_free_embedded_tree(texture->nodetree);
    MEM_freeN(texture->nodetree);
    texture->nodetree = nullptr;
  }

  MEM_SAFE_FREE(texture->coba);

  BKE_icon_id_delete((ID *)texture);
  BKE_previewimg_free(&texture->preview);
}

static void texture_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Tex *texture = reinterpret_cast<Tex *>(id);

  if (texture->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&texture->nodetree));
  }
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, texture->ima, IDWALK_CB_USER);
}

static void texture_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Tex *tex = (Tex *)id;

  /* write LibData */
  BLO_write_id_struct(writer, Tex, id_address, &tex->id);
  BKE_id_blend_write(writer, &tex->id);

  /* direct data */
  if (tex->coba) {
    BLO_write_struct(writer, ColorBand, tex->coba);
  }

  /* nodetree is integral part of texture, no libdata */
  if (tex->nodetree) {
    BLO_Write_IDBuffer temp_embedded_id_buffer{tex->nodetree->id, writer};
    BLO_write_struct_at_address(writer, bNodeTree, tex->nodetree, temp_embedded_id_buffer.get());
    blender::bke::node_tree_blend_write(
        writer, reinterpret_cast<bNodeTree *>(temp_embedded_id_buffer.get()));
  }

  BKE_previewimg_blend_write(writer, tex->preview);
}

static void texture_blend_read_data(BlendDataReader *reader, ID *id)
{
  Tex *tex = (Tex *)id;

  BLO_read_struct(reader, ColorBand, &tex->coba);

  BLO_read_struct(reader, PreviewImage, &tex->preview);
  BKE_previewimg_blend_read(reader, tex->preview);

  tex->iuser.scene = nullptr;

  tex->runtime.last_update = 0;
}

IDTypeInfo IDType_ID_TE = {
    /*id_code*/ Tex::id_type,
    /*id_filter*/ FILTER_ID_TE,
    /*dependencies_id_types*/ FILTER_ID_IM | FILTER_ID_OB,
    /*main_listbase_index*/ INDEX_ID_TE,
    /*struct_size*/ sizeof(Tex),
    /*name*/ "Texture",
    /*name_plural*/ N_("textures"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_TEXTURE,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ texture_init_data,
    /*copy_data*/ texture_copy_data,
    /*free_data*/ texture_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ texture_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ texture_blend_write,
    /*blend_read_data*/ texture_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

void BKE_texture_mtex_foreach_id(LibraryForeachIDData *data, MTex *mtex)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, mtex->object, IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, mtex->tex, IDWALK_CB_USER);
}

/* ****************** Mapping ******************* */

TexMapping *BKE_texture_mapping_add(int type)
{
  TexMapping *texmap = MEM_callocN<TexMapping>("TexMapping");

  BKE_texture_mapping_default(texmap, type);

  return texmap;
}

void BKE_texture_mapping_default(TexMapping *texmap, int type)
{
  *texmap = TexMapping{};

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
      is_zero_v3(texmap->loc) && is_zero_v3(texmap->rot) && is_one_v3(texmap->size))
  {
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

ColorMapping *BKE_texture_colormapping_add()
{
  ColorMapping *colormap = MEM_callocN<ColorMapping>("ColorMapping");

  BKE_texture_colormapping_default(colormap);

  return colormap;
}

void BKE_texture_colormapping_default(ColorMapping *colormap)
{
  *colormap = ColorMapping{};

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

  tex = BKE_id_new<Tex>(bmain, name);

  return tex;
}

/* ------------------------------------------------------------------------- */

void BKE_texture_mtex_default(MTex *mtex)
{
  *mtex = blender::dna::shallow_copy(*DNA_struct_default_get(MTex));
}

/* ------------------------------------------------------------------------- */

MTex *BKE_texture_mtex_add()
{
  MTex *mtex;

  mtex = MEM_callocN<MTex>("BKE_texture_mtex_add");

  BKE_texture_mtex_default(mtex);

  return mtex;
}

MTex *BKE_texture_mtex_add_id(ID *id, int slot)
{
  MTex **mtex_ar;
  short act;

  give_active_mtex(id, &mtex_ar, &act);

  if (mtex_ar == nullptr) {
    return nullptr;
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
      return nullptr;
    }
  }
  else {
    /* make sure slot is valid */
    if (slot < 0 || slot >= MAX_MTEX) {
      return nullptr;
    }
  }

  if (mtex_ar[slot]) {
    id_us_min((ID *)mtex_ar[slot]->tex);
    MEM_freeN(mtex_ar[slot]);
    mtex_ar[slot] = nullptr;
  }

  mtex_ar[slot] = BKE_texture_mtex_add();

  return mtex_ar[slot];
}

/* ------------------------------------------------------------------------- */

Tex *give_current_linestyle_texture(FreestyleLineStyle *linestyle)
{
  MTex *mtex = nullptr;
  Tex *tex = nullptr;

  if (linestyle) {
    mtex = linestyle->mtex[int(linestyle->texact)];
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
  else {
    MEM_SAFE_FREE(linestyle->mtex[act]);
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
      *mtex_ar = nullptr;
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
  BKE_brush_tag_unsaved_changes(br);
}

Tex *give_current_particle_texture(ParticleSettings *part)
{
  MTex *mtex = nullptr;
  Tex *tex = nullptr;

  if (!part) {
    return nullptr;
  }

  mtex = part->mtex[int(part->texact)];
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
  else {
    MEM_SAFE_FREE(part->mtex[act]);
  }
}

/* ------------------------------------------------------------------------- */

bool BKE_texture_is_image_user(const Tex *tex)
{
  switch (tex->type) {
    case TEX_IMAGE: {
      return true;
    }
  }

  return false;
}

bool BKE_texture_dependsOnTime(const Tex *texture)
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

void BKE_texture_get_value_ex(Tex *texture,
                              const float *tex_co,
                              TexResult *texres,
                              ImagePool *pool,
                              bool use_color_management)
{
  /* no node textures for now */
  const int result_type = multitex_ext_safe(
      texture, tex_co, texres, pool, use_color_management, false);

  /* if the texture gave an RGB value, we assume it didn't give a valid
   * intensity, since this is in the context of modifiers don't use perceptual color conversion.
   * if the texture didn't give an RGB value, copy the intensity across
   */
  if (result_type & TEX_RGB) {
    texres->tin = (1.0f / 3.0f) * (texres->trgba[0] + texres->trgba[1] + texres->trgba[2]);
  }
  else {
    copy_v3_fl(texres->trgba, texres->tin);
  }
}

void BKE_texture_get_value(Tex *texture,
                           const float *tex_co,
                           TexResult *texres,
                           bool use_color_management)
{
  BKE_texture_get_value_ex(texture, tex_co, texres, nullptr, use_color_management);
}

static void texture_nodes_fetch_images_for_pool(Tex *texture, bNodeTree *ntree, ImagePool *pool)
{
  for (bNode *node : ntree->all_nodes()) {
    if (node->type_legacy == SH_NODE_TEX_IMAGE && node->id != nullptr) {
      Image *image = (Image *)node->id;
      BKE_image_pool_acquire_ibuf(image, &texture->iuser, pool);
    }
    else if (node->is_group() && node->id != nullptr) {
      /* TODO(sergey): Do we need to control recursion here? */
      bNodeTree *nested_tree = (bNodeTree *)node->id;
      texture_nodes_fetch_images_for_pool(texture, nested_tree, pool);
    }
  }
}

void BKE_texture_fetch_images_for_pool(Tex *texture, ImagePool *pool)
{
  if (texture->nodetree != nullptr) {
    texture_nodes_fetch_images_for_pool(texture, texture->nodetree, pool);
  }
  else {
    if (texture->type == TEX_IMAGE) {
      if (texture->ima != nullptr) {
        BKE_image_pool_acquire_ibuf(texture->ima, &texture->iuser, pool);
      }
    }
  }
}
