/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* NOTE:
 *
 * This file is no longer used to provide tools for the deprecated IPO system. Instead, it
 * is only used to house the conversion code to the new system.
 *
 * -- Joshua Leung, Jan 2009
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

/* since we have versioning code here */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_endian_switch.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.h"
#include "BKE_idtype.hh"
#include "BKE_ipo.h"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_nla.h"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "SEQ_iterator.hh"

#include "BLO_read_write.hh"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

static CLG_LogRef LOG = {"bke.ipo"};

static void ipo_free_data(ID *id)
{
  Ipo *ipo = (Ipo *)id;

  IpoCurve *icu, *icn;
  int n = 0;

  for (icu = static_cast<IpoCurve *>(ipo->curve.first); icu; icu = icn) {
    icn = icu->next;
    n++;

    if (icu->bezt) {
      MEM_freeN(icu->bezt);
    }
    if (icu->bp) {
      MEM_freeN(icu->bp);
    }
    if (icu->driver) {
      MEM_freeN(icu->driver);
    }

    BLI_freelinkN(&ipo->curve, icu);
  }

  if (G.debug & G_DEBUG) {
    printf("Freed %d (Unconverted) Ipo-Curves from IPO '%s'\n", n, ipo->id.name + 2);
  }
}

static void ipo_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Ipo *ipo = reinterpret_cast<Ipo *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    LISTBASE_FOREACH (IpoCurve *, icu, &ipo->curve) {
      if (icu->driver) {
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, icu->driver->ob, IDWALK_CB_NOP);
      }
    }
  }
}

static void ipo_blend_read_data(BlendDataReader *reader, ID *id)
{
  Ipo *ipo = (Ipo *)id;

  BLO_read_list(reader, &(ipo->curve));

  LISTBASE_FOREACH (IpoCurve *, icu, &ipo->curve) {
    BLO_read_data_address(reader, &icu->bezt);
    BLO_read_data_address(reader, &icu->bp);
    BLO_read_data_address(reader, &icu->driver);

    /* Undo generic endian switching. */
    if (BLO_read_requires_endian_switch(reader)) {
      BLI_endian_switch_int16(&icu->blocktype);
      if (icu->driver != nullptr) {

        /* Undo generic endian switching. */
        if (BLO_read_requires_endian_switch(reader)) {
          BLI_endian_switch_int16(&icu->blocktype);
          if (icu->driver != nullptr) {
            BLI_endian_switch_int16(&icu->driver->blocktype);
          }
        }
      }

      /* Undo generic endian switching. */
      if (BLO_read_requires_endian_switch(reader)) {
        BLI_endian_switch_int16(&ipo->blocktype);
        if (icu->driver != nullptr) {
          BLI_endian_switch_int16(&icu->driver->blocktype);
        }
      }
    }
  }

  /* Undo generic endian switching. */
  if (BLO_read_requires_endian_switch(reader)) {
    BLI_endian_switch_int16(&ipo->blocktype);
  }
}

IDTypeInfo IDType_ID_IP = {
    /*id_code*/ ID_IP,
    /*id_filter*/ 0,
    /*main_listbase_index*/ INDEX_ID_IP,
    /*struct_size*/ sizeof(Ipo),
    /*name*/ "Ipo",
    /*name_plural*/ N_("ipos"),
    /*translation_context*/ "",
    /*flags*/ IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING | IDTYPE_FLAGS_NO_ANIMDATA,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*free_data*/ ipo_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ ipo_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ nullptr,
    /*blend_read_data*/ ipo_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/* *************************************************** */
/* Old-Data Freeing Tools */

/* *************************************************** */
/* ADRCODE to RNA-Path Conversion Code  - Special (Bitflags) */

/* Mapping Table for bitflag <-> RNA path */
struct AdrBit2Path {
  int bit;
  const char *path;
  int array_index;
};

/* ----------------- */
/* Mapping Tables to use bits <-> RNA paths */

/* Object layers */
static AdrBit2Path ob_layer_bits[] = {
    {(1 << 0), "layers", 0},   {(1 << 1), "layers", 1},   {(1 << 2), "layers", 2},
    {(1 << 3), "layers", 3},   {(1 << 4), "layers", 4},   {(1 << 5), "layers", 5},
    {(1 << 6), "layers", 6},   {(1 << 7), "layers", 7},   {(1 << 8), "layers", 8},
    {(1 << 9), "layers", 9},   {(1 << 10), "layers", 10}, {(1 << 11), "layers", 11},
    {(1 << 12), "layers", 12}, {(1 << 13), "layers", 13}, {(1 << 14), "layers", 14},
    {(1 << 15), "layers", 15}, {(1 << 16), "layers", 16}, {(1 << 17), "layers", 17},
    {(1 << 18), "layers", 18}, {(1 << 19), "layers", 19},
};

/* ----------------- */

/* quick macro for returning the appropriate array for adrcode_bitmaps_to_paths() */
#define RET_ABP(items) \
  { \
    *tot = ARRAY_SIZE(items); \
    return items; \
  } \
  (void)0

/* This function checks if a `blocktype+adrcode` combination, returning a mapping table. */
static AdrBit2Path *adrcode_bitmaps_to_paths(int blocktype, int adrcode, int *tot)
{
  /* Object layers */
  if ((blocktype == ID_OB) && (adrcode == OB_LAY)) {
    RET_ABP(ob_layer_bits);
  }
  /* XXX TODO: add other types... */

  /* Normal curve */
  return nullptr;
}
#undef RET_ABP

/* *************************************************** */
/* ADRCODE to RNA-Path Conversion Code  - Standard */

/* Object types */
static const char *ob_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case OB_LOC_X:
      *r_array_index = 0;
      return "location";
    case OB_LOC_Y:
      *r_array_index = 1;
      return "location";
    case OB_LOC_Z:
      *r_array_index = 2;
      return "location";
    case OB_DLOC_X:
      *r_array_index = 0;
      return "delta_location";
    case OB_DLOC_Y:
      *r_array_index = 1;
      return "delta_location";
    case OB_DLOC_Z:
      *r_array_index = 2;
      return "delta_location";

    case OB_ROT_X:
      *r_array_index = 0;
      return "rotation_euler";
    case OB_ROT_Y:
      *r_array_index = 1;
      return "rotation_euler";
    case OB_ROT_Z:
      *r_array_index = 2;
      return "rotation_euler";
    case OB_DROT_X:
      *r_array_index = 0;
      return "delta_rotation_euler";
    case OB_DROT_Y:
      *r_array_index = 1;
      return "delta_rotation_euler";
    case OB_DROT_Z:
      *r_array_index = 2;
      return "delta_rotation_euler";

    case OB_SIZE_X:
      *r_array_index = 0;
      return "scale";
    case OB_SIZE_Y:
      *r_array_index = 1;
      return "scale";
    case OB_SIZE_Z:
      *r_array_index = 2;
      return "scale";
    case OB_DSIZE_X:
      *r_array_index = 0;
      return "delta_scale";
    case OB_DSIZE_Y:
      *r_array_index = 1;
      return "delta_scale";
    case OB_DSIZE_Z:
      *r_array_index = 2;
      return "delta_scale";
    case OB_COL_R:
      *r_array_index = 0;
      return "color";
    case OB_COL_G:
      *r_array_index = 1;
      return "color";
    case OB_COL_B:
      *r_array_index = 2;
      return "color";
    case OB_COL_A:
      *r_array_index = 3;
      return "color";
#if 0
    case OB_PD_FSTR:
      if (ob->pd) {
        poin = &(ob->pd->f_strength);
      }
      break;
    case OB_PD_FFALL:
      if (ob->pd) {
        poin = &(ob->pd->f_power);
      }
      break;
    case OB_PD_SDAMP:
      if (ob->pd) {
        poin = &(ob->pd->pdef_damp);
      }
      break;
    case OB_PD_RDAMP:
      if (ob->pd) {
        poin = &(ob->pd->pdef_rdamp);
      }
      break;
    case OB_PD_PERM:
      if (ob->pd) {
        poin = &(ob->pd->pdef_perm);
      }
      break;
    case OB_PD_FMAXD:
      if (ob->pd) {
        poin = &(ob->pd->maxdist);
      }
      break;
#endif
  }

  return nullptr;
}

/* PoseChannel types
 * NOTE: pchan name comes from 'actname' added earlier...
 */
static const char *pchan_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case AC_QUAT_W:
      *r_array_index = 0;
      return "rotation_quaternion";
    case AC_QUAT_X:
      *r_array_index = 1;
      return "rotation_quaternion";
    case AC_QUAT_Y:
      *r_array_index = 2;
      return "rotation_quaternion";
    case AC_QUAT_Z:
      *r_array_index = 3;
      return "rotation_quaternion";

    case AC_EUL_X:
      *r_array_index = 0;
      return "rotation_euler";
    case AC_EUL_Y:
      *r_array_index = 1;
      return "rotation_euler";
    case AC_EUL_Z:
      *r_array_index = 2;
      return "rotation_euler";

    case AC_LOC_X:
      *r_array_index = 0;
      return "location";
    case AC_LOC_Y:
      *r_array_index = 1;
      return "location";
    case AC_LOC_Z:
      *r_array_index = 2;
      return "location";

    case AC_SIZE_X:
      *r_array_index = 0;
      return "scale";
    case AC_SIZE_Y:
      *r_array_index = 1;
      return "scale";
    case AC_SIZE_Z:
      *r_array_index = 2;
      return "scale";
  }

  /* for debugging only */
  CLOG_ERROR(&LOG, "unmatched PoseChannel setting (code %d)", adrcode);
  return nullptr;
}

/* Constraint types */
static const char *constraint_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case CO_ENFORCE:
      return "influence";
    case CO_HEADTAIL:
      /* XXX this needs to be wrapped in RNA.. probably then this path will be invalid. */
      return "data.head_tail";
  }

  return nullptr;
}

/* ShapeKey types
 * NOTE: as we don't have access to the keyblock where the data comes from (for now),
 *       we'll just use numerical indices for now...
 */
static char *shapekey_adrcodes_to_paths(ID *id, int adrcode, int * /*r_array_index*/)
{
  static char buf[128];

  /* block will be attached to ID_KE block... */
  if (adrcode == 0) {
    /* adrcode=0 was the misnamed "speed" curve (now "evaluation time") */
    STRNCPY(buf, "eval_time");
  }
  else {
    /* Find the name of the ShapeKey (i.e. KeyBlock) to look for */
    Key *key = (Key *)id;
    KeyBlock *kb = BKE_keyblock_find_by_index(key, adrcode);

    /* setting that we alter is the "value" (i.e. keyblock.curval) */
    if (kb) {
      /* Use the keyblock name, escaped, so that path lookups for this will work */
      char kb_name_esc[sizeof(kb->name) * 2];
      BLI_str_escape(kb_name_esc, kb->name, sizeof(kb_name_esc));
      SNPRINTF(buf, "key_blocks[\"%s\"].value", kb_name_esc);
    }
    else {
      /* Fallback - Use the adrcode as index directly, so that this can be manually fixed */
      SNPRINTF(buf, "key_blocks[%d].value", adrcode);
    }
  }
  return buf;
}

/* MTex (Texture Slot) types */
static const char *mtex_adrcodes_to_paths(int adrcode, int * /*r_array_index*/)
{
  const char *base = nullptr, *prop = nullptr;
  static char buf[128];

  /* base part of path */
  if (adrcode & MA_MAP1) {
    base = "textures[0]";
  }
  else if (adrcode & MA_MAP2) {
    base = "textures[1]";
  }
  else if (adrcode & MA_MAP3) {
    base = "textures[2]";
  }
  else if (adrcode & MA_MAP4) {
    base = "textures[3]";
  }
  else if (adrcode & MA_MAP5) {
    base = "textures[4]";
  }
  else if (adrcode & MA_MAP6) {
    base = "textures[5]";
  }
  else if (adrcode & MA_MAP7) {
    base = "textures[6]";
  }
  else if (adrcode & MA_MAP8) {
    base = "textures[7]";
  }
  else if (adrcode & MA_MAP9) {
    base = "textures[8]";
  }
  else if (adrcode & MA_MAP10) {
    base = "textures[9]";
  }
  else if (adrcode & MA_MAP11) {
    base = "textures[10]";
  }
  else if (adrcode & MA_MAP12) {
    base = "textures[11]";
  }
  else if (adrcode & MA_MAP13) {
    base = "textures[12]";
  }
  else if (adrcode & MA_MAP14) {
    base = "textures[13]";
  }
  else if (adrcode & MA_MAP15) {
    base = "textures[14]";
  }
  else if (adrcode & MA_MAP16) {
    base = "textures[15]";
  }
  else if (adrcode & MA_MAP17) {
    base = "textures[16]";
  }
  else if (adrcode & MA_MAP18) {
    base = "textures[17]";
  }

  /* property identifier for path */
  adrcode = (adrcode & (MA_MAP1 - 1));
  switch (adrcode) {
#if 0 /* XXX these are not wrapped in RNA yet! */
    case MAP_OFS_X:
      poin = &(mtex->ofs[0]);
      break;
    case MAP_OFS_Y:
      poin = &(mtex->ofs[1]);
      break;
    case MAP_OFS_Z:
      poin = &(mtex->ofs[2]);
      break;
    case MAP_SIZE_X:
      poin = &(mtex->size[0]);
      break;
    case MAP_SIZE_Y:
      poin = &(mtex->size[1]);
      break;
    case MAP_SIZE_Z:
      poin = &(mtex->size[2]);
      break;
    case MAP_R:
      poin = &(mtex->r);
      break;
    case MAP_G:
      poin = &(mtex->g);
      break;
    case MAP_B:
      poin = &(mtex->b);
      break;
    case MAP_DVAR:
      poin = &(mtex->def_var);
      break;
    case MAP_COLF:
      poin = &(mtex->colfac);
      break;
    case MAP_NORF:
      poin = &(mtex->norfac);
      break;
    case MAP_VARF:
      poin = &(mtex->varfac);
      break;
#endif
    case MAP_DISP:
      prop = "warp_factor";
      break;
  }

  /* only build and return path if there's a property */
  if (prop) {
    SNPRINTF(buf, "%s.%s", base, prop);
    return buf;
  }

  return nullptr;
}

/* Texture types */
static const char *texture_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case TE_NSIZE:
      return "noise_size";
    case TE_TURB:
      return "turbulence";

    case TE_NDEPTH: /* XXX texture RNA undefined */
      // poin= &(tex->noisedepth); *type= IPO_SHORT; break;
      break;
    case TE_NTYPE: /* XXX texture RNA undefined */
      // poin= &(tex->noisetype); *type= IPO_SHORT; break;
      break;

    case TE_N_BAS1:
      return "noise_basis";
    case TE_N_BAS2:
      return "noise_basis"; /* XXX this is not yet defined in RNA... */

    /* voronoi */
    case TE_VNW1:
      *r_array_index = 0;
      return "feature_weights";
    case TE_VNW2:
      *r_array_index = 1;
      return "feature_weights";
    case TE_VNW3:
      *r_array_index = 2;
      return "feature_weights";
    case TE_VNW4:
      *r_array_index = 3;
      return "feature_weights";
    case TE_VNMEXP:
      return "minkovsky_exponent";
    case TE_VN_DISTM:
      return "distance_metric";
    case TE_VN_COLT:
      return "color_type";

    /* distorted noise / voronoi */
    case TE_ISCA:
      return "noise_intensity";

    /* distorted noise */
    case TE_DISTA:
      return "distortion_amount";

    /* musgrave */
    case TE_MG_TYP: /* XXX texture RNA undefined */
      //  poin= &(tex->stype); *type= IPO_SHORT; break;
      break;
    case TE_MGH:
      return "highest_dimension";
    case TE_MG_LAC:
      return "lacunarity";
    case TE_MG_OCT:
      return "octaves";
    case TE_MG_OFF:
      return "offset";
    case TE_MG_GAIN:
      return "gain";

    case TE_COL_R:
      *r_array_index = 0;
      return "rgb_factor";
    case TE_COL_G:
      *r_array_index = 1;
      return "rgb_factor";
    case TE_COL_B:
      *r_array_index = 2;
      return "rgb_factor";

    case TE_BRIGHT:
      return "brightness";
    case TE_CONTRA:
      return "contrast";
  }

  return nullptr;
}

/* Material Types */
static const char *material_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case MA_COL_R:
      *r_array_index = 0;
      return "diffuse_color";
    case MA_COL_G:
      *r_array_index = 1;
      return "diffuse_color";
    case MA_COL_B:
      *r_array_index = 2;
      return "diffuse_color";

    case MA_SPEC_R:
      *r_array_index = 0;
      return "specular_color";
    case MA_SPEC_G:
      *r_array_index = 1;
      return "specular_color";
    case MA_SPEC_B:
      *r_array_index = 2;
      return "specular_color";

    case MA_MIR_R:
      *r_array_index = 0;
      return "mirror_color";
    case MA_MIR_G:
      *r_array_index = 1;
      return "mirror_color";
    case MA_MIR_B:
      *r_array_index = 2;
      return "mirror_color";

    case MA_ALPHA:
      return "alpha";

    case MA_REF:
      return "diffuse_intensity";

    case MA_EMIT:
      return "emit";

    case MA_AMB:
      return "ambient";

    case MA_SPEC:
      return "specular_intensity";

    case MA_HARD:
      return "specular_hardness";

    case MA_SPTR:
      return "specular_opacity";

    case MA_IOR:
      return "ior";

    case MA_HASIZE:
      return "halo.size";

    case MA_TRANSLU:
      return "translucency";

    case MA_RAYM:
      return "raytrace_mirror.reflect";

    case MA_FRESMIR:
      return "raytrace_mirror.fresnel";

    case MA_FRESMIRI:
      return "raytrace_mirror.fresnel_factor";

    case MA_FRESTRA:
      return "raytrace_transparency.fresnel";

    case MA_FRESTRAI:
      return "raytrace_transparency.fresnel_factor";

    case MA_ADD:
      return "halo.add";

    default: /* for now, we assume that the others were MTex channels */
      return mtex_adrcodes_to_paths(adrcode, r_array_index);
  }

  return nullptr;
}

/* Camera Types */
static const char *camera_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case CAM_LENS:
#if 0 /* XXX this cannot be resolved easily... \
       * perhaps we assume camera is perspective (works for most cases... */
      if (ca->type == CAM_ORTHO) {
        return "ortho_scale";
      }
      else {
        return "lens";
      }
#else /* XXX lazy hack for now... */
      return "lens";
#endif /* XXX this cannot be resolved easily */

    case CAM_STA:
      return "clip_start";
    case CAM_END:
      return "clip_end";

#if 0  /* XXX these are not defined in RNA */
    case CAM_YF_APERT:
      poin = &(ca->YF_aperture);
      break;
    case CAM_YF_FDIST:
      poin = &(ca->dof_distance);
      break;
#endif /* XXX these are not defined in RNA */

    case CAM_SHIFT_X:
      return "shift_x";
    case CAM_SHIFT_Y:
      return "shift_y";
  }

  /* unrecognized adrcode, or not-yet-handled ones! */
  return nullptr;
}

/* Light Types */
static const char *light_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case LA_ENERGY:
      return "energy";

    case LA_COL_R:
      *r_array_index = 0;
      return "color";
    case LA_COL_G:
      *r_array_index = 1;
      return "color";
    case LA_COL_B:
      *r_array_index = 2;
      return "color";

    case LA_DIST:
      return "distance";

    case LA_SPOTSI:
      return "spot_size";
    case LA_SPOTBL:
      return "spot_blend";

    case LA_QUAD1:
      return "linear_attenuation";
    case LA_QUAD2:
      return "quadratic_attenuation";

    case LA_HALOINT:
      return "halo_intensity";

    default: /* for now, we assume that the others were MTex channels */
      return mtex_adrcodes_to_paths(adrcode, r_array_index);
  }

  /* unrecognized adrcode, or not-yet-handled ones! */
  return nullptr;
}

/* Sound Types */
static const char *sound_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case SND_VOLUME:
      return "volume";
    case SND_PITCH:
      return "pitch";
/* XXX Joshua -- I had wrapped panning in rna,
 * but someone commented out, calling it "unused" */
#if 0
    case SND_PANNING:
      return "panning";
#endif
    case SND_ATTEN:
      return "attenuation";
  }

  /* unrecognized adrcode, or not-yet-handled ones! */
  return nullptr;
}

/* World Types */
static const char *world_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case WO_HOR_R:
      *r_array_index = 0;
      return "horizon_color";
    case WO_HOR_G:
      *r_array_index = 1;
      return "horizon_color";
    case WO_HOR_B:
      *r_array_index = 2;
      return "horizon_color";
    case WO_ZEN_R:
      *r_array_index = 0;
      return "zenith_color";
    case WO_ZEN_G:
      *r_array_index = 1;
      return "zenith_color";
    case WO_ZEN_B:
      *r_array_index = 2;
      return "zenith_color";

    case WO_EXPOS:
      return "exposure";

    case WO_MISI:
      return "mist.intensity";
    case WO_MISTDI:
      return "mist.depth";
    case WO_MISTSTA:
      return "mist.start";
    case WO_MISTHI:
      return "mist.height";

    default: /* for now, we assume that the others were MTex channels */
      return mtex_adrcodes_to_paths(adrcode, r_array_index);
  }

  return nullptr;
}

/* Particle Types */
static const char *particle_adrcodes_to_paths(int adrcode, int *r_array_index)
{
  /* Set array index like this in-case nothing sets it correctly. */
  *r_array_index = 0;

  /* result depends on adrcode */
  switch (adrcode) {
    case PART_CLUMP:
      return "settings.clump_factor";
    case PART_AVE:
      return "settings.angular_velocity_factor";
    case PART_SIZE:
      return "settings.particle_size";
    case PART_DRAG:
      return "settings.drag_factor";
    case PART_BROWN:
      return "settings.brownian_factor";
    case PART_DAMP:
      return "settings.damp_factor";
    case PART_LENGTH:
      return "settings.length";
    case PART_GRAV_X:
      *r_array_index = 0;
      return "settings.acceleration";
    case PART_GRAV_Y:
      *r_array_index = 1;
      return "settings.acceleration";
    case PART_GRAV_Z:
      *r_array_index = 2;
      return "settings.acceleration";
    case PART_KINK_AMP:
      return "settings.kink_amplitude";
    case PART_KINK_FREQ:
      return "settings.kink_frequency";
    case PART_KINK_SHAPE:
      return "settings.kink_shape";
    case PART_BB_TILT:
      return "settings.billboard_tilt";

/* PartDeflect needs to be sorted out properly in rna_object_force;
 * If anyone else works on this, but is unfamiliar, these particular
 * settings reference the particles of the system themselves
 * being used as forces -- it will use the same rna structure
 * as the similar object forces */
#if 0
    case PART_PD_FSTR:
      if (part->pd) {
        poin = &(part->pd->f_strength);
      }
      break;
    case PART_PD_FFALL:
      if (part->pd) {
        poin = &(part->pd->f_power);
      }
      break;
    case PART_PD_FMAXD:
      if (part->pd) {
        poin = &(part->pd->maxdist);
      }
      break;
    case PART_PD2_FSTR:
      if (part->pd2) {
        poin = &(part->pd2->f_strength);
      }
      break;
    case PART_PD2_FFALL:
      if (part->pd2) {
        poin = &(part->pd2->f_power);
      }
      break;
    case PART_PD2_FMAXD:
      if (part->pd2) {
        poin = &(part->pd2->maxdist);
      }
      break;
#endif
  }

  return nullptr;
}

/* ------- */

/* Allocate memory for RNA-path for some property given a blocktype, adrcode,
 * and 'root' parts of path.
 *
 * Input:
 *     - id                      - the data-block that the curve's IPO block
 *                                 is attached to and/or which the new paths will start from
 *     - blocktype, adrcode      - determines setting to get
 *     - actname, constname, seq - used to build path
 * Output:
 *     - r_array_index           - index in property's array (if applicable) to use
 *     - return                  - the allocated path...
 */
static char *get_rna_access(ID *id,
                            int blocktype,
                            int adrcode,
                            char actname[],
                            char constname[],
                            Sequence *seq,
                            int *r_array_index)
{
  DynStr *path = BLI_dynstr_new();
  const char *propname = nullptr;
  char *rpath = nullptr;
  char buf[512];
  int dummy_index = 0;

  /* hack: if constname is set, we can only be dealing with an Constraint curve */
  if (constname) {
    blocktype = ID_CO;
  }

  /* get property name based on blocktype */
  switch (blocktype) {
    case ID_OB: /* object */
      propname = ob_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_PO: /* pose channel */
      propname = pchan_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_KE: /* shapekeys */
      propname = shapekey_adrcodes_to_paths(id, adrcode, &dummy_index);
      break;

    case ID_CO: /* constraint */
      propname = constraint_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_TE: /* texture */
      propname = texture_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_MA: /* material */
      propname = material_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_CA: /* camera */
      propname = camera_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_LA: /* light */
      propname = light_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_SO: /* sound */
      propname = sound_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_WO: /* world */
      propname = world_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_PA: /* particle */
      propname = particle_adrcodes_to_paths(adrcode, &dummy_index);
      break;

    case ID_CU_LEGACY: /* curve */
      /* this used to be a 'dummy' curve which got evaluated on the fly...
       * now we've got real var for this!
       */
      propname = "eval_time";
      break;

    /* XXX problematic block-types. */
    case ID_SEQ: /* sequencer strip */
      /* SEQ_FAC1: */
      switch (adrcode) {
        case SEQ_FAC1:
          propname = "effect_fader";
          break;
        case SEQ_FAC_SPEED:
          propname = "speed_fader";
          break;
        case SEQ_FAC_OPACITY:
          propname = "blend_alpha";
          break;
      }
      /* XXX this doesn't seem to be included anywhere in sequencer RNA... */
      // poin= &(seq->facf0);
      break;

    /* special hacks */
    case -1:
      /* special case for rotdiff drivers... we don't need a property for this... */
      break;

    /* TODO: add other block-types. */
    default:
      CLOG_WARN(&LOG, "No path for blocktype %d, adrcode %d yet", blocktype, adrcode);
      break;
  }

  /* check if any property found
   * - blocktype < 0 is special case for a specific type of driver,
   *   where we don't need a property name...
   */
  if ((propname == nullptr) && (blocktype > 0)) {
    /* nothing was found, so exit */
    if (r_array_index) {
      *r_array_index = 0;
    }

    BLI_dynstr_free(path);

    return nullptr;
  }

  if (r_array_index) {
    *r_array_index = dummy_index;
  }

  /* 'buf' _must_ be initialized in this block */
  /* append preceding bits to path */
  /* NOTE: strings are not escaped and they should be! */
  if ((actname && actname[0]) && (constname && constname[0])) {
    /* Constraint in Pose-Channel */
    char actname_esc[sizeof(bActionChannel::name) * 2];
    char constname_esc[sizeof(bConstraint::name) * 2];
    BLI_str_escape(actname_esc, actname, sizeof(actname_esc));
    BLI_str_escape(constname_esc, constname, sizeof(constname_esc));
    SNPRINTF(buf, "pose.bones[\"%s\"].constraints[\"%s\"]", actname_esc, constname_esc);
  }
  else if (actname && actname[0]) {
    if ((blocktype == ID_OB) && STREQ(actname, "Object")) {
      /* Actionified "Object" IPO's... no extra path stuff needed */
      buf[0] = '\0'; /* empty string */
    }
    else if ((blocktype == ID_KE) && STREQ(actname, "Shape")) {
      /* Actionified "Shape" IPO's -
       * these are forced onto object level via the action container there... */
      STRNCPY(buf, "data.shape_keys");
    }
    else {
      /* Pose-Channel */
      char actname_esc[sizeof(bActionChannel::name) * 2];
      BLI_str_escape(actname_esc, actname, sizeof(actname_esc));
      SNPRINTF(buf, "pose.bones[\"%s\"]", actname_esc);
    }
  }
  else if (constname && constname[0]) {
    /* Constraint in Object */
    char constname_esc[sizeof(bConstraint::name) * 2];
    BLI_str_escape(constname_esc, constname, sizeof(constname_esc));
    SNPRINTF(buf, "constraints[\"%s\"]", constname_esc);
  }
  else if (seq) {
    /* Sequence names in Scene */
    char seq_name_esc[(sizeof(seq->name) - 2) * 2];
    BLI_str_escape(seq_name_esc, seq->name + 2, sizeof(seq_name_esc));
    SNPRINTF(buf, "sequence_editor.sequences_all[\"%s\"]", seq_name_esc);
  }
  else {
    buf[0] = '\0'; /* empty string */
  }

  BLI_dynstr_append(path, buf);

  /* need to add dot before property if there was anything preceding this */
  if (buf[0]) {
    BLI_dynstr_append(path, ".");
  }

  /* now write name of property */
  BLI_dynstr_append(path, propname);

  /* if there was no array index pointer provided, add it to the path */
  if (r_array_index == nullptr) {
    SNPRINTF(buf, "[\"%d\"]", dummy_index);
    BLI_dynstr_append(path, buf);
  }

  /* convert to normal MEM_malloc'd string */
  rpath = BLI_dynstr_get_cstring(path);
  BLI_dynstr_free(path);

  /* return path... */
  return rpath;
}

/* *************************************************** */
/* Conversion Utilities */

/* Convert adrcodes to driver target transform channel types */
static short adrcode_to_dtar_transchan(short adrcode)
{
  switch (adrcode) {
    case OB_LOC_X:
      return DTAR_TRANSCHAN_LOCX;
    case OB_LOC_Y:
      return DTAR_TRANSCHAN_LOCY;
    case OB_LOC_Z:
      return DTAR_TRANSCHAN_LOCZ;

    case OB_ROT_X:
      return DTAR_TRANSCHAN_ROTX;
    case OB_ROT_Y:
      return DTAR_TRANSCHAN_ROTY;
    case OB_ROT_Z:
      return DTAR_TRANSCHAN_ROTZ;

    case OB_SIZE_X:
      return DTAR_TRANSCHAN_SCALEX;
    case OB_SIZE_Y:
      return DTAR_TRANSCHAN_SCALEX;
    case OB_SIZE_Z:
      return DTAR_TRANSCHAN_SCALEX;

    default:
      return 0;
  }
}

/* Convert IpoDriver to ChannelDriver - will free the old data (i.e. the old driver) */
static ChannelDriver *idriver_to_cdriver(IpoDriver *idriver)
{
  ChannelDriver *cdriver;

  /* allocate memory for new driver */
  cdriver = static_cast<ChannelDriver *>(MEM_callocN(sizeof(ChannelDriver), "ChannelDriver"));

  /* if 'pydriver', just copy data across */
  if (idriver->type == IPO_DRIVER_TYPE_PYTHON) {
    /* PyDriver only requires the expression to be copied */
    /* FIXME: expression will be useless due to API changes, but at least not totally lost */
    cdriver->type = DRIVER_TYPE_PYTHON;
    if (idriver->name[0]) {
      STRNCPY(cdriver->expression, idriver->name);
    }
  }
  else {
    DriverVar *dvar = nullptr;
    DriverTarget *dtar = nullptr;

    /* this should be ok for all types here... */
    cdriver->type = DRIVER_TYPE_AVERAGE;

    /* what to store depends on the 'blocktype' - object or posechannel */
    if (idriver->blocktype == ID_AR) { /* PoseChannel */
      if (idriver->adrcode == OB_ROT_DIFF) {
        /* Rotational Difference requires a special type of variable */
        dvar = driver_add_new_variable(cdriver);
        driver_change_variable_type(dvar, DVAR_TYPE_ROT_DIFF);

        /* first bone target */
        dtar = &dvar->targets[0];
        dtar->id = (ID *)idriver->ob;
        dtar->idtype = ID_OB;
        if (idriver->name[0]) {
          STRNCPY(dtar->pchan_name, idriver->name);
        }

        /* second bone target (name was stored in same var as the first one) */
        dtar = &dvar->targets[1];
        dtar->id = (ID *)idriver->ob;
        dtar->idtype = ID_OB;
        if (idriver->name[0]) { /* XXX: for safety. */
          STRNCPY(dtar->pchan_name, idriver->name + DRIVER_NAME_OFFS);
        }
      }
      else {
        /* only a single variable, of type 'transform channel' */
        dvar = driver_add_new_variable(cdriver);
        driver_change_variable_type(dvar, DVAR_TYPE_TRANSFORM_CHAN);

        /* only requires a single target */
        dtar = &dvar->targets[0];
        dtar->id = (ID *)idriver->ob;
        dtar->idtype = ID_OB;
        if (idriver->name[0]) {
          STRNCPY(dtar->pchan_name, idriver->name);
        }
        dtar->transChan = adrcode_to_dtar_transchan(idriver->adrcode);
        dtar->flag |= DTAR_FLAG_LOCALSPACE; /* old drivers took local space */
      }
    }
    else { /* Object */
      /* only a single variable, of type 'transform channel' */
      dvar = driver_add_new_variable(cdriver);
      driver_change_variable_type(dvar, DVAR_TYPE_TRANSFORM_CHAN);

      /* only requires single target */
      dtar = &dvar->targets[0];
      dtar->id = (ID *)idriver->ob;
      dtar->idtype = ID_OB;
      dtar->transChan = adrcode_to_dtar_transchan(idriver->adrcode);
    }
  }

  /* return the new one */
  return cdriver;
}

/* Add F-Curve to the correct list
 * - grpname is needed to be used as group name where relevant, and is usually derived from actname
 */
static void fcurve_add_to_list(
    ListBase *groups, ListBase *list, FCurve *fcu, char *grpname, int muteipo)
{
  /* If we're adding to an action, we will have groups to write to... */
  if (groups && grpname) {
    /* wrap the pointers given into a dummy action that we pass to the API func
     * and extract the resultant lists...
     */
    bAction tmp_act;
    bActionGroup *agrp = nullptr;

    /* init the temp action */
    memset(&tmp_act, 0, sizeof(bAction)); /* XXX: Only enable this line if we get errors. */
    tmp_act.groups.first = groups->first;
    tmp_act.groups.last = groups->last;
    tmp_act.curves.first = list->first;
    tmp_act.curves.last = list->last;
    /* XXX: The other vars don't need to be filled in. */

    /* get the group to use */
    agrp = BKE_action_group_find_name(&tmp_act, grpname);
    /* no matching group, so add one */
    if (agrp == nullptr) {
      /* Add a new group, and make it active */
      agrp = static_cast<bActionGroup *>(MEM_callocN(sizeof(bActionGroup), "bActionGroup"));

      agrp->flag = AGRP_SELECTED;
      if (muteipo) {
        agrp->flag |= AGRP_MUTED;
      }

      STRNCPY(agrp->name, grpname);

      BLI_addtail(&tmp_act.groups, agrp);
      BLI_uniquename(&tmp_act.groups,
                     agrp,
                     DATA_("Group"),
                     '.',
                     offsetof(bActionGroup, name),
                     sizeof(agrp->name));
    }

    /* add F-Curve to group */
    /* WARNING: this func should only need to look at the stuff we initialized,
     * if not, things may crash. */
    action_groups_add_channel(&tmp_act, agrp, fcu);

    if (agrp->flag & AGRP_MUTED) { /* flush down */
      fcu->flag |= FCURVE_MUTED;
    }

    /* set the output lists based on the ones in the temp action */
    groups->first = tmp_act.groups.first;
    groups->last = tmp_act.groups.last;
    list->first = tmp_act.curves.first;
    list->last = tmp_act.curves.last;
  }
  else {
    /* simply add the F-Curve to the end of the given list */
    BLI_addtail(list, fcu);
  }
}

/**
 * Convert IPO-Curve to F-Curve (including Driver data), and free any of the old data that
 * is not relevant, BUT do not free the IPO-Curve itself...
 *
 * \param actname: name of Action-Channel (if applicable) that IPO-Curve's IPO-block belonged to.
 * \param constname: name of Constraint-Channel (if applicable)
 * that IPO-Curve's IPO-block belonged to \a seq.
 * \param seq: sequencer-strip (if applicable) that IPO-Curve's IPO-block belonged to.
 */
static void icu_to_fcurves(ID *id,
                           ListBase *groups,
                           ListBase *list,
                           IpoCurve *icu,
                           char *actname,
                           char *constname,
                           Sequence *seq,
                           int muteipo)
{
  AdrBit2Path *abp;
  FCurve *fcu;
  int totbits;

  /* allocate memory for a new F-Curve */
  fcu = BKE_fcurve_create();

  /* convert driver */
  if (icu->driver) {
    fcu->driver = idriver_to_cdriver(icu->driver);
  }

  /* copy flags */
  if (icu->flag & IPO_VISIBLE) {
    fcu->flag |= FCURVE_VISIBLE;
  }
  if (icu->flag & IPO_SELECT) {
    fcu->flag |= FCURVE_SELECTED;
  }
  if (icu->flag & IPO_ACTIVE) {
    fcu->flag |= FCURVE_ACTIVE;
  }
  if (icu->flag & IPO_MUTE) {
    fcu->flag |= FCURVE_MUTED;
  }
  if (icu->flag & IPO_PROTECT) {
    fcu->flag |= FCURVE_PROTECTED;
  }

  /* set extrapolation */
  switch (icu->extrap) {
    case IPO_HORIZ: /* constant extrapolation */
    case IPO_DIR:   /* linear extrapolation */
    {
      /* just copy, as the new defines match the old ones... */
      fcu->extend = icu->extrap;
      break;
    }
    case IPO_CYCL:  /* cyclic extrapolation */
    case IPO_CYCLX: /* cyclic extrapolation + offset */
    {
      /* Add a new FModifier (Cyclic) instead of setting extend value
       * as that's the new equivalent of that option.
       */
      FModifier *fcm = add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, fcu);
      FMod_Cycles *data = (FMod_Cycles *)fcm->data;

      /* if 'offset' one is in use, set appropriate settings */
      if (icu->extrap == IPO_CYCLX) {
        data->before_mode = data->after_mode = FCM_EXTRAPOLATE_CYCLIC_OFFSET;
      }
      else {
        data->before_mode = data->after_mode = FCM_EXTRAPOLATE_CYCLIC;
      }
      break;
    }
  }

  /* -------- */

  /* get adrcode <-> bitflags mapping to handle nasty bitflag curves? */
  abp = adrcode_bitmaps_to_paths(icu->blocktype, icu->adrcode, &totbits);
  if (abp && totbits) {
    FCurve *fcurve;
    int b;

    if (G.debug & G_DEBUG) {
      printf("\tconvert bitflag ipocurve, totbits = %d\n", totbits);
    }

    /* add the 'only int values' flag */
    fcu->flag |= (FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES);

    /* for each bit we have to remap + check for:
     * 1) we need to make copy the existing F-Curve data (fcu -> fcurve),
     *    except for the last one which will use the original
     * 2) copy the relevant path info across
     * 3) filter the keyframes for the flag of interest
     */
    for (b = 0; b < totbits; b++, abp++) {
      uint i = 0;

      /* make a copy of existing base-data if not the last curve */
      if (b < (totbits - 1)) {
        fcurve = BKE_fcurve_copy(fcu);
      }
      else {
        fcurve = fcu;
      }

      /* set path */
      fcurve->rna_path = BLI_strdup(abp->path);
      fcurve->array_index = abp->array_index;

      /* Convert keyframes:
       * - Beztriples and bpoints are mutually exclusive,
       *   so we won't have both at the same time.
       * - Beztriples are more likely to be encountered as they are keyframes
       *   (the other type wasn't used yet).
       */
      fcurve->totvert = icu->totvert;

      if (icu->bezt) {
        BezTriple *dst, *src;

        /* allocate new array for keyframes/beztriples */
        fcurve->bezt = static_cast<BezTriple *>(
            MEM_callocN(sizeof(BezTriple) * fcurve->totvert, "BezTriples"));

        /* loop through copying all BezTriples individually, as we need to modify a few things */
        for (dst = fcurve->bezt, src = icu->bezt, i = 0; i < fcurve->totvert; i++, dst++, src++) {
          /* firstly, copy BezTriple data */
          *dst = *src;

          /* interpolation can only be constant... */
          dst->ipo = BEZT_IPO_CONST;

          /* 'hide' flag is now used for keytype - only 'keyframes' existed before */
          dst->hide = BEZT_KEYTYPE_KEYFRAME;

          /* auto-handles - per curve to per handle */
          if (icu->flag & IPO_AUTO_HORIZ) {
            if (dst->h1 == HD_AUTO) {
              dst->h1 = HD_AUTO_ANIM;
            }
            if (dst->h2 == HD_AUTO) {
              dst->h2 = HD_AUTO_ANIM;
            }
          }

          /* correct values, by checking if the flag of interest is set */
          if (int(dst->vec[1][1]) & (abp->bit)) {
            dst->vec[0][1] = dst->vec[1][1] = dst->vec[2][1] = 1.0f;
          }
          else {
            dst->vec[0][1] = dst->vec[1][1] = dst->vec[2][1] = 0.0f;
          }
        }
      }
      else if (icu->bp) {
        /* TODO: need to convert from BPoint type to the more compact FPoint type...
         * but not priority, since no data used this. */
        // BPoint *bp;
        // FPoint *fpt;
      }

      /* add new F-Curve to list */
      fcurve_add_to_list(groups, list, fcurve, actname, muteipo);
    }
  }
  else {
    uint i = 0;

    /* get rna-path
     * - we will need to set the 'disabled' flag if no path is able to be made (for now)
     */
    fcu->rna_path = get_rna_access(
        id, icu->blocktype, icu->adrcode, actname, constname, seq, &fcu->array_index);
    if (fcu->rna_path == nullptr) {
      fcu->flag |= FCURVE_DISABLED;
    }

    /* Convert keyframes:
     * - Beztriples and bpoints are mutually exclusive, so we won't have both at the same time.
     * - Beztriples are more likely to be encountered as they are keyframes
     *   (the other type wasn't used yet).
     */
    fcu->totvert = icu->totvert;

    if (icu->bezt) {
      BezTriple *dst, *src;

      /* allocate new array for keyframes/beztriples */
      fcu->bezt = static_cast<BezTriple *>(
          MEM_callocN(sizeof(BezTriple) * fcu->totvert, "BezTriples"));

      /* loop through copying all BezTriples individually, as we need to modify a few things */
      for (dst = fcu->bezt, src = icu->bezt, i = 0; i < fcu->totvert; i++, dst++, src++) {
        /* firstly, copy BezTriple data */
        *dst = *src;

        /* now copy interpolation from curve (if not already set) */
        if (icu->ipo != IPO_MIXED) {
          dst->ipo = icu->ipo;
        }

        /* 'hide' flag is now used for keytype - only 'keyframes' existed before */
        dst->hide = BEZT_KEYTYPE_KEYFRAME;

        /* auto-handles - per curve to per handle */
        if (icu->flag & IPO_AUTO_HORIZ) {
          if (dst->h1 == HD_AUTO) {
            dst->h1 = HD_AUTO_ANIM;
          }
          if (dst->h2 == HD_AUTO) {
            dst->h2 = HD_AUTO_ANIM;
          }
        }

        /* correct values for euler rotation curves
         * - they were degrees/10
         * - we need radians for RNA to do the right thing
         */
        if (((icu->blocktype == ID_OB) && ELEM(icu->adrcode, OB_ROT_X, OB_ROT_Y, OB_ROT_Z)) ||
            ((icu->blocktype == ID_PO) && ELEM(icu->adrcode, AC_EUL_X, AC_EUL_Y, AC_EUL_Z)))
        {
          const float fac = float(M_PI) / 18.0f; /* `10.0f * M_PI/180.0f`. */

          dst->vec[0][1] *= fac;
          dst->vec[1][1] *= fac;
          dst->vec[2][1] *= fac;
        }

        /* correct values for path speed curves
         * - their values were 0-1
         * - we now need as 'frames'
         */
        if ((id) && (icu->blocktype == GS(id->name)) &&
            (fcu->rna_path && STREQ(fcu->rna_path, "eval_time")))
        {
          Curve *cu = (Curve *)id;

          dst->vec[0][1] *= cu->pathlen;
          dst->vec[1][1] *= cu->pathlen;
          dst->vec[2][1] *= cu->pathlen;
        }

        /* correct times for rotation drivers
         * - need to go from degrees to radians...
         * - there's only really 1 target to worry about
         * - were also degrees/10
         */
        if (fcu->driver && fcu->driver->variables.first) {
          DriverVar *dvar = static_cast<DriverVar *>(fcu->driver->variables.first);
          DriverTarget *dtar = &dvar->targets[0];

          if (ELEM(dtar->transChan, DTAR_TRANSCHAN_ROTX, DTAR_TRANSCHAN_ROTY, DTAR_TRANSCHAN_ROTZ))
          {
            const float fac = float(M_PI) / 18.0f;

            dst->vec[0][0] *= fac;
            dst->vec[1][0] *= fac;
            dst->vec[2][0] *= fac;
          }
        }

        /* correct values for sequencer curves, that were not locked to frame */
        if (seq && (seq->flag & SEQ_IPO_FRAME_LOCKED) == 0) {
          const float mul = (seq->enddisp - seq->startdisp) / 100.0f;
          const float offset = seq->startdisp;

          dst->vec[0][0] *= mul;
          dst->vec[0][0] += offset;

          dst->vec[1][0] *= mul;
          dst->vec[1][0] += offset;

          dst->vec[2][0] *= mul;
          dst->vec[2][0] += offset;
        }
      }
    }
    else if (icu->bp) {
      /* TODO: need to convert from BPoint type to the more compact FPoint type...
       * but not priority, since no data used this */
      // BPoint *bp;
      // FPoint *fpt;
    }

    /* add new F-Curve to list */
    fcurve_add_to_list(groups, list, fcu, actname, muteipo);
  }
}

/* ------------------------- */

/* Convert IPO-block (i.e. all its IpoCurves) to the new system.
 * This does not assume that any ID or AnimData uses it, but does assume that
 * it is given two lists, which it will perform driver/animation-data separation.
 */
static void ipo_to_animato(ID *id,
                           Ipo *ipo,
                           char actname[],
                           char constname[],
                           Sequence *seq,
                           ListBase *animgroups,
                           ListBase *anim,
                           ListBase *drivers)
{
  IpoCurve *icu;

  /* sanity check */
  if (ELEM(nullptr, ipo, anim, drivers)) {
    return;
  }

  if (G.debug & G_DEBUG) {
    printf("ipo_to_animato\n");
  }

  /* validate actname and constname
   * - clear actname if it was one of the generic <builtin> ones (i.e. 'Object', or 'Shapes')
   * - actname can then be used to assign F-Curves in Action to Action Groups
   *   (i.e. thus keeping the benefits that used to be provided by Action Channels for grouping
   *   F-Curves for bones). This may be added later... for now let's just dump without them...
   */
  if (actname) {
    if ((ipo->blocktype == ID_OB) && STREQ(actname, "Object")) {
      actname = nullptr;
    }
    else if ((ipo->blocktype == ID_OB) && STREQ(actname, "Shape")) {
      actname = nullptr;
    }
  }

  /* loop over IPO-Curves, freeing as we progress */
  LISTBASE_FOREACH (IpoCurve *, icu, &ipo->curve) {
    /* Since an IPO-Curve may end up being made into many F-Curves (i.e. bitflag curves),
     * we figure out the best place to put the channel,
     * then tell the curve-converter to just dump there. */
    if (icu->driver) {
      /* Blender 2.4x allowed empty drivers,
       * but we don't now, since they cause more trouble than they're worth. */
      if ((icu->driver->ob) || (icu->driver->type == IPO_DRIVER_TYPE_PYTHON)) {
        icu_to_fcurves(id, nullptr, drivers, icu, actname, constname, seq, ipo->muteipo);
      }
      else {
        MEM_freeN(icu->driver);
        icu->driver = nullptr;
      }
    }
    else {
      icu_to_fcurves(id, animgroups, anim, icu, actname, constname, seq, ipo->muteipo);
    }
  }

  /* if this IPO block doesn't have any users after this one, free... */
  id_us_min(&ipo->id);
  if (ID_REAL_USERS(ipo) <= 0) {
    IpoCurve *icn;

    for (icu = static_cast<IpoCurve *>(ipo->curve.first); icu; icu = icn) {
      icn = icu->next;

      /* free driver */
      if (icu->driver) {
        MEM_freeN(icu->driver);
      }

      /* free old data of curve now that it's no longer needed for converting any more curves */
      if (icu->bezt) {
        MEM_freeN(icu->bezt);
      }
      if (icu->bp) {
        MEM_freeN(icu->bezt);
      }

      /* free this IPO-Curve */
      BLI_freelinkN(&ipo->curve, icu);
    }
  }
}

/* Convert Action-block to new system, separating animation and drivers
 * New curves may not be converted directly into the given Action (i.e. for Actions linked
 * to Objects, where ob->ipo and ob->action need to be combined).
 * NOTE: we need to be careful here, as same data-structs are used for new system too!
 */
static void action_to_animato(
    ID *id, bAction *act, ListBase *groups, ListBase *curves, ListBase *drivers)
{
  bActionChannel *achan, *achann;
  bConstraintChannel *conchan, *conchann;

  /* only continue if there are Action Channels (indicating unconverted data) */
  if (BLI_listbase_is_empty(&act->chanbase)) {
    return;
  }

  /* get rid of all Action Groups */
  /* XXX this is risky if there's some old + some new data in the Action... */
  if (act->groups.first) {
    BLI_freelistN(&act->groups);
  }

  /* loop through Action-Channels, converting data, freeing as we go */
  for (achan = static_cast<bActionChannel *>(act->chanbase.first); achan; achan = achann) {
    /* get pointer to next Action Channel */
    achann = achan->next;

    /* convert Action Channel's IPO data */
    if (achan->ipo) {
      ipo_to_animato(id, achan->ipo, achan->name, nullptr, nullptr, groups, curves, drivers);
      id_us_min(&achan->ipo->id);
      achan->ipo = nullptr;
    }

    /* convert constraint channel IPO-data */
    for (conchan = static_cast<bConstraintChannel *>(achan->constraintChannels.first); conchan;
         conchan = conchann)
    {
      /* get pointer to next Constraint Channel */
      conchann = conchan->next;

      /* convert Constraint Channel's IPO data */
      if (conchan->ipo) {
        ipo_to_animato(
            id, conchan->ipo, achan->name, conchan->name, nullptr, groups, curves, drivers);
        id_us_min(&conchan->ipo->id);
        conchan->ipo = nullptr;
      }

      /* free Constraint Channel */
      BLI_freelinkN(&achan->constraintChannels, conchan);
    }

    /* free Action Channel */
    BLI_freelinkN(&act->chanbase, achan);
  }
}

/* ------------------------- */

/* Convert IPO-block (i.e. all its IpoCurves) for some ID to the new system
 * This assumes that AnimData has been added already. Separation of drivers
 * from animation data is accomplished here too...
 */
static void ipo_to_animdata(
    Main *bmain, ID *id, Ipo *ipo, char actname[], char constname[], Sequence *seq)
{
  AnimData *adt = BKE_animdata_from_id(id);
  ListBase anim = {nullptr, nullptr};
  ListBase drivers = {nullptr, nullptr};

  /* sanity check */
  if (ELEM(nullptr, id, ipo)) {
    return;
  }
  if (adt == nullptr) {
    CLOG_ERROR(&LOG, "adt invalid");
    return;
  }

  if (G.debug & G_DEBUG) {
    printf("ipo to animdata - ID:%s, IPO:%s, actname:%s constname:%s seqname:%s  curves:%d\n",
           id->name + 2,
           ipo->id.name + 2,
           (actname) ? actname : "<None>",
           (constname) ? constname : "<None>",
           (seq) ? (seq->name + 2) : "<None>",
           BLI_listbase_count(&ipo->curve));
  }

  /* Convert curves to animato system
   * (separated into separate lists of F-Curves for animation and drivers),
   * and the try to put these lists in the right places, but do not free the lists here. */
  /* XXX there shouldn't be any need for the groups, so don't supply pointer for that now... */
  ipo_to_animato(id, ipo, actname, constname, seq, nullptr, &anim, &drivers);

  /* deal with animation first */
  if (anim.first) {
    if (G.debug & G_DEBUG) {
      printf("\thas anim\n");
    }
    /* try to get action */
    if (adt->action == nullptr) {
      char nameBuf[MAX_ID_NAME];

      SNPRINTF(nameBuf, "CDA:%s", ipo->id.name + 2);

      adt->action = BKE_action_add(bmain, nameBuf);
      if (G.debug & G_DEBUG) {
        printf("\t\tadded new action - '%s'\n", nameBuf);
      }
    }

    /* add F-Curves to action */
    BLI_movelisttolist(&adt->action->curves, &anim);
  }

  /* deal with drivers */
  if (drivers.first) {
    if (G.debug & G_DEBUG) {
      printf("\thas drivers\n");
    }
    /* add drivers to end of driver stack */
    BLI_movelisttolist(&adt->drivers, &drivers);
  }
}

/* Convert Action-block to new system
 * NOTE: we need to be careful here, as same data-structs are used for new system too!
 */
static void action_to_animdata(ID *id, bAction *act)
{
  AnimData *adt = BKE_animdata_from_id(id);

  /* only continue if there are Action Channels (indicating unconverted data) */
  if (ELEM(nullptr, adt, act->chanbase.first)) {
    return;
  }

  /* check if we need to set this Action as the AnimData's action */
  if (adt->action == nullptr) {
    /* set this Action as AnimData's Action */
    if (G.debug & G_DEBUG) {
      printf("act_to_adt - set adt action to act\n");
    }
    adt->action = act;
  }

  /* convert Action data */
  action_to_animato(id, act, &adt->action->groups, &adt->action->curves, &adt->drivers);
}

/* ------------------------- */

/* TODO:
 * - NLA group duplicators info
 * - NLA curve/stride modifiers... */

/* Convert NLA-Strip to new system */
static void nlastrips_to_animdata(ID *id, ListBase *strips)
{
  AnimData *adt = BKE_animdata_from_id(id);
  NlaTrack *nlt = nullptr;
  NlaStrip *strip;
  bActionStrip *as, *asn;

  /* for each one of the original strips, convert to a new strip and free the old... */
  for (as = static_cast<bActionStrip *>(strips->first); as; as = asn) {
    asn = as->next;

    /* this old strip is only worth something if it had an action... */
    if (as->act) {
      /* convert Action data (if not yet converted), storing the results in the same Action */
      action_to_animato(id, as->act, &as->act->groups, &as->act->curves, &adt->drivers);

      /* Create a new-style NLA-strip which references this Action,
       * then copy over relevant settings. */
      {
        /* init a new strip, and assign the action to it
         * - no need to muck around with the user-counts, since this is just
         *   passing over the ref to the new owner, not creating an additional ref
         */
        strip = static_cast<NlaStrip *>(MEM_callocN(sizeof(NlaStrip), "NlaStrip"));
        strip->act = as->act;

        /* endpoints */
        strip->start = as->start;
        strip->end = as->end;
        strip->actstart = as->actstart;
        strip->actend = as->actend;

        /* action reuse */
        strip->repeat = as->repeat;
        strip->scale = as->scale;
        if (as->flag & ACTSTRIP_LOCK_ACTION) {
          strip->flag |= NLASTRIP_FLAG_SYNC_LENGTH;
        }

        /* blending */
        strip->blendin = as->blendin;
        strip->blendout = as->blendout;
        strip->blendmode = (as->mode == ACTSTRIPMODE_ADD) ? NLASTRIP_MODE_ADD :
                                                            NLASTRIP_MODE_REPLACE;
        if (as->flag & ACTSTRIP_AUTO_BLENDS) {
          strip->flag |= NLASTRIP_FLAG_AUTO_BLENDS;
        }

        /* assorted setting flags */
        if (as->flag & ACTSTRIP_SELECT) {
          strip->flag |= NLASTRIP_FLAG_SELECT;
        }
        if (as->flag & ACTSTRIP_ACTIVE) {
          strip->flag |= NLASTRIP_FLAG_ACTIVE;
        }

        if (as->flag & ACTSTRIP_MUTE) {
          strip->flag |= NLASTRIP_FLAG_MUTED;
        }
        if (as->flag & ACTSTRIP_REVERSE) {
          strip->flag |= NLASTRIP_FLAG_REVERSE;
        }

        /* by default, we now always extrapolate, while in the past this was optional */
        if ((as->flag & ACTSTRIP_HOLDLASTFRAME) == 0) {
          strip->extendmode = NLASTRIP_EXTEND_NOTHING;
        }
      }

      /* Try to add this strip to the current NLA-Track
       * (i.e. the 'last' one on the stack at the moment). */
      if (BKE_nlatrack_add_strip(nlt, strip, false) == 0) {
        /* trying to add to the current failed (no space),
         * so add a new track to the stack, and add to that...
         */
        nlt = BKE_nlatrack_new_tail(&adt->nla_tracks, false);
        BKE_nlatrack_set_active(&adt->nla_tracks, nlt);
        BKE_nlatrack_add_strip(nlt, strip, false);
      }

      /* ensure that strip has a name */
      BKE_nlastrip_validate_name(adt, strip);
    }

    /* modifiers */
    /* FIXME: for now, we just free them... */
    if (as->modifiers.first) {
      BLI_freelistN(&as->modifiers);
    }

    /* free the old strip */
    BLI_freelinkN(strips, as);
  }
}

struct Seq_callback_data {
  Main *bmain;
  Scene *scene;
  AnimData *adt;
};

static bool seq_convert_callback(Sequence *seq, void *userdata)
{
  IpoCurve *icu = static_cast<IpoCurve *>((seq->ipo) ? seq->ipo->curve.first : nullptr);
  short adrcode = SEQ_FAC1;

  if (G.debug & G_DEBUG) {
    printf("\tconverting sequence strip %s\n", seq->name + 2);
  }

  if (ELEM(nullptr, seq->ipo, icu)) {
    seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;
    return true;
  }

  /* Patch `adrcode`, so that we can map to different DNA variables later (semi-hack (tm)). */
  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
    case SEQ_TYPE_META:
    case SEQ_TYPE_SCENE:
    case SEQ_TYPE_MOVIE:
    case SEQ_TYPE_COLOR:
      adrcode = SEQ_FAC_OPACITY;
      break;
    case SEQ_TYPE_SPEED:
      adrcode = SEQ_FAC_SPEED;
      break;
  }
  icu->adrcode = adrcode;

  Seq_callback_data *cd = (Seq_callback_data *)userdata;

  /* convert IPO */
  ipo_to_animdata(cd->bmain, (ID *)cd->scene, seq->ipo, nullptr, nullptr, seq);

  if (cd->adt->action) {
    cd->adt->action->idroot = ID_SCE; /* scene-rooted */
  }

  id_us_min(&seq->ipo->id);
  seq->ipo = nullptr;
  return true;
}

/* *************************************************** */
/* External API - Only Called from do_versions() */

void do_versions_ipos_to_animato(Main *bmain)
{
  ListBase drivers = {nullptr, nullptr};
  ID *id;

  if (bmain == nullptr) {
    CLOG_ERROR(&LOG, "Argh! Main is nullptr");
    return;
  }

  /* only convert if version is right */
  if (bmain->versionfile >= 250) {
    CLOG_WARN(&LOG, "Animation data too new to convert (Version %d)", bmain->versionfile);
    return;
  }
  if (G.debug & G_DEBUG) {
    printf("INFO: Converting to Animato...\n");
  }

  /* ----------- Animation Attached to Data -------------- */

  /* objects */
  for (id = static_cast<ID *>(bmain->objects.first); id; id = static_cast<ID *>(id->next)) {
    Object *ob = (Object *)id;
    bConstraintChannel *conchan, *conchann;

    if (G.debug & G_DEBUG) {
      printf("\tconverting ob %s\n", id->name + 2);
    }

    /* check if object has any animation data */
    if (ob->nlastrips.first) {
      /* Add AnimData block */
      BKE_animdata_ensure_id(id);

      /* IPO first to take into any non-NLA'd Object Animation */
      if (ob->ipo) {
        ipo_to_animdata(bmain, id, ob->ipo, nullptr, nullptr, nullptr);
        /* No need to id_us_min ipo ID here, ipo_to_animdata already does it. */
        ob->ipo = nullptr;
      }

      /* Action is skipped since it'll be used by some strip in the NLA anyway,
       * causing errors with evaluation in the new evaluation pipeline
       */
      if (ob->action) {
        id_us_min(&ob->action->id);
        ob->action = nullptr;
      }

      /* finally NLA */
      nlastrips_to_animdata(id, &ob->nlastrips);
    }
    else if ((ob->ipo) || (ob->action)) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Action first - so that Action name get conserved */
      if (ob->action) {
        action_to_animdata(id, ob->action);

        /* Only decrease user-count if this Action isn't now being used by AnimData. */
        if (ob->action != adt->action) {
          id_us_min(&ob->action->id);
          ob->action = nullptr;
        }
      }

      /* IPO second... */
      if (ob->ipo) {
        ipo_to_animdata(bmain, id, ob->ipo, nullptr, nullptr, nullptr);
        /* No need to id_us_min ipo ID here, ipo_to_animdata already does it. */
        ob->ipo = nullptr;
      }
    }

    /* check PoseChannels for constraints with local data */
    if (ob->pose) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
          /* if constraint has own IPO, convert add these to Object
           * (NOTE: they're most likely to be drivers too)
           */
          if (con->ipo) {
            /* Verify if there's AnimData block */
            BKE_animdata_ensure_id(id);

            /* although this was the constraint's local IPO, we still need to provide pchan + con
             * so that drivers can be added properly...
             */
            ipo_to_animdata(bmain, id, con->ipo, pchan->name, con->name, nullptr);
            id_us_min(&con->ipo->id);
            con->ipo = nullptr;
          }
        }
      }
    }

    /* check constraints for local IPO's */
    LISTBASE_FOREACH (bConstraint *, con, &ob->constraints) {
      /* if constraint has own IPO, convert add these to Object
       * (NOTE: they're most likely to be drivers too)
       */
      if (con->ipo) {
        /* Verify if there's AnimData block, just in case */
        BKE_animdata_ensure_id(id);

        /* although this was the constraint's local IPO, we still need to provide con
         * so that drivers can be added properly...
         */
        ipo_to_animdata(bmain, id, con->ipo, nullptr, con->name, nullptr);
        id_us_min(&con->ipo->id);
        con->ipo = nullptr;
      }

      /* check for Action Constraint */
      /* XXX do we really want to do this here? */
    }

    /* check constraint channels - we need to remove them anyway... */
    if (ob->constraintChannels.first) {
      for (conchan = static_cast<bConstraintChannel *>(ob->constraintChannels.first); conchan;
           conchan = conchann)
      {
        /* get pointer to next Constraint Channel */
        conchann = conchan->next;

        /* convert Constraint Channel's IPO data */
        if (conchan->ipo) {
          /* Verify if there's AnimData block */
          BKE_animdata_ensure_id(id);

          ipo_to_animdata(bmain, id, conchan->ipo, nullptr, conchan->name, nullptr);
          id_us_min(&conchan->ipo->id);
          conchan->ipo = nullptr;
        }

        /* free Constraint Channel */
        BLI_freelinkN(&ob->constraintChannels, conchan);
      }
    }

    /* object's action will always be object-rooted */
    {
      AnimData *adt = BKE_animdata_from_id(id);
      if (adt && adt->action) {
        adt->action->idroot = ID_OB;
      }
    }
  }

  /* shapekeys */
  for (id = static_cast<ID *>(bmain->shapekeys.first); id; id = static_cast<ID *>(id->next)) {
    Key *key = (Key *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting key %s\n", id->name + 2);
    }

    /* we're only interested in the IPO
     * NOTE: for later, it might be good to port these over to Object instead, as many of these
     * are likely to be drivers, but it's hard to trace that from here, so move this to Ob loop?
     */
    if (key->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert Shape-key data... */
      ipo_to_animdata(bmain, id, key->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = key->ipo->blocktype;
      }

      id_us_min(&key->ipo->id);
      key->ipo = nullptr;
    }
  }

  /* materials */
  for (id = static_cast<ID *>(bmain->materials.first); id; id = static_cast<ID *>(id->next)) {
    Material *ma = (Material *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting material %s\n", id->name + 2);
    }

    /* we're only interested in the IPO */
    if (ma->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert Material data... */
      ipo_to_animdata(bmain, id, ma->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = ma->ipo->blocktype;
      }

      id_us_min(&ma->ipo->id);
      ma->ipo = nullptr;
    }
  }

  /* worlds */
  for (id = static_cast<ID *>(bmain->worlds.first); id; id = static_cast<ID *>(id->next)) {
    World *wo = (World *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting world %s\n", id->name + 2);
    }

    /* we're only interested in the IPO */
    if (wo->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert World data... */
      ipo_to_animdata(bmain, id, wo->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = wo->ipo->blocktype;
      }

      id_us_min(&wo->ipo->id);
      wo->ipo = nullptr;
    }
  }

  /* sequence strips */
  for (id = static_cast<ID *>(bmain->scenes.first); id; id = static_cast<ID *>(id->next)) {
    Scene *scene = (Scene *)id;
    Editing *ed = scene->ed;
    if (ed && ed->seqbasep) {
      Seq_callback_data cb_data = {bmain, scene, BKE_animdata_ensure_id(id)};
      SEQ_for_each_callback(&ed->seqbase, seq_convert_callback, &cb_data);
    }
  }

  /* textures */
  for (id = static_cast<ID *>(bmain->textures.first); id; id = static_cast<ID *>(id->next)) {
    Tex *te = (Tex *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting texture %s\n", id->name + 2);
    }

    /* we're only interested in the IPO */
    if (te->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert Texture data... */
      ipo_to_animdata(bmain, id, te->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = te->ipo->blocktype;
      }

      id_us_min(&te->ipo->id);
      te->ipo = nullptr;
    }
  }

  /* cameras */
  for (id = static_cast<ID *>(bmain->cameras.first); id; id = static_cast<ID *>(id->next)) {
    Camera *ca = (Camera *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting camera %s\n", id->name + 2);
    }

    /* we're only interested in the IPO */
    if (ca->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert Camera data... */
      ipo_to_animdata(bmain, id, ca->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = ca->ipo->blocktype;
      }

      id_us_min(&ca->ipo->id);
      ca->ipo = nullptr;
    }
  }

  /* lights */
  for (id = static_cast<ID *>(bmain->lights.first); id; id = static_cast<ID *>(id->next)) {
    Light *la = (Light *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting light %s\n", id->name + 2);
    }

    /* we're only interested in the IPO */
    if (la->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert Light data... */
      ipo_to_animdata(bmain, id, la->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = la->ipo->blocktype;
      }

      id_us_min(&la->ipo->id);
      la->ipo = nullptr;
    }
  }

  /* curves */
  for (id = static_cast<ID *>(bmain->curves.first); id; id = static_cast<ID *>(id->next)) {
    Curve *cu = (Curve *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting curve %s\n", id->name + 2);
    }

    /* we're only interested in the IPO */
    if (cu->ipo) {
      /* Add AnimData block */
      AnimData *adt = BKE_animdata_ensure_id(id);

      /* Convert Curve data... */
      ipo_to_animdata(bmain, id, cu->ipo, nullptr, nullptr, nullptr);

      if (adt->action) {
        adt->action->idroot = cu->ipo->blocktype;
      }

      id_us_min(&cu->ipo->id);
      cu->ipo = nullptr;
    }
  }

  /* --------- Unconverted Animation Data ------------------ */
  /* For Animation data which may not be directly connected (i.e. not linked) to any other
   * data, we need to perform a separate pass to make sure that they are converted to standalone
   * Actions which may then be able to be reused. This does mean that we will be going over data
   * that's already been converted, but there are no problems with that.
   *
   * The most common case for this will be Action Constraints, or IPO's with Fake-Users.
   * We collect all drivers that were found into a temporary collection, and free them in one go,
   * as they're impossible to resolve.
   */

  /* actions */
  for (id = static_cast<ID *>(bmain->actions.first); id; id = static_cast<ID *>(id->next)) {
    bAction *act = (bAction *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting action %s\n", id->name + 2);
    }

    /* if old action, it will be object-only... */
    if (act->chanbase.first) {
      act->idroot = ID_OB;
    }

    /* be careful! some of the actions we encounter will be converted ones... */
    action_to_animato(nullptr, act, &act->groups, &act->curves, &drivers);
  }

  /* ipo's */
  for (id = static_cast<ID *>(bmain->ipo.first); id; id = static_cast<ID *>(id->next)) {
    Ipo *ipo = (Ipo *)id;

    if (G.debug & G_DEBUG) {
      printf("\tconverting ipo %s\n", id->name + 2);
    }

    /* most likely this IPO has already been processed, so check if any curves left to convert */
    if (ipo->curve.first) {
      bAction *new_act;

      /* add a new action for this, and convert all data into that action */
      new_act = BKE_action_add(bmain, id->name + 2);
      ipo_to_animato(nullptr, ipo, nullptr, nullptr, nullptr, nullptr, &new_act->curves, &drivers);
      new_act->idroot = ipo->blocktype;
    }

    /* clear fake-users, and set user-count to zero to make sure it is cleared on file-save */
    ipo->id.us = 0;
    ipo->id.flag &= ~LIB_FAKEUSER;
  }

  /* free unused drivers from actions + ipos */
  BKE_fcurves_free(&drivers);

  if (G.debug & G_DEBUG) {
    printf("INFO: Animato convert done\n");
  }
}
