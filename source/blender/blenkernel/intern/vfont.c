/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_base_safe.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_vfont_types.h"

#include "BKE_anim_path.h"
#include "BKE_bpath.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_vfont.h"
#include "BKE_vfontdata.h"

#include "BLO_read_write.h"

static CLG_LogRef LOG = {"bke.data_transfer"};
static ThreadRWMutex vfont_rwlock = BLI_RWLOCK_INITIALIZER;

/**************************** Prototypes **************************/

static PackedFile *get_builtin_packedfile(void);

/****************************** VFont Datablock ************************/

static void vfont_init_data(ID *id)
{
  VFont *vfont = (VFont *)id;
  PackedFile *pf = get_builtin_packedfile();

  if (pf) {
    VFontData *vfd;

    vfd = BKE_vfontdata_from_freetypefont(pf);
    if (vfd) {
      vfont->data = vfd;

      BLI_strncpy(vfont->filepath, FO_BUILTIN_NAME, sizeof(vfont->filepath));
    }

    /* Free the packed file */
    BKE_packedfile_free(pf);
  }
}

static void vfont_copy_data(Main *UNUSED(bmain),
                            ID *id_dst,
                            const ID *UNUSED(id_src),
                            const int flag)
{
  VFont *vfont_dst = (VFont *)id_dst;

  /* We never handle user-count here for own data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  /* Just to be sure, should not have any value actually after reading time. */
  vfont_dst->temp_pf = NULL;

  if (vfont_dst->packedfile) {
    vfont_dst->packedfile = BKE_packedfile_duplicate(vfont_dst->packedfile);
  }

  if (vfont_dst->data) {
    vfont_dst->data = BKE_vfontdata_copy(vfont_dst->data, flag_subdata);
  }
}

/** Free (or release) any data used by this font (does not free the font itself). */
static void vfont_free_data(ID *id)
{
  VFont *vfont = (VFont *)id;
  BKE_vfont_free_data(vfont);

  if (vfont->packedfile) {
    BKE_packedfile_free(vfont->packedfile);
    vfont->packedfile = NULL;
  }
}

static void vfont_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  VFont *vfont = (VFont *)id;

  if (vfont->packedfile != NULL && (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0) {
    return;
  }

  if (BKE_vfont_is_builtin(vfont)) {
    return;
  }

  BKE_bpath_foreach_path_fixed_process(bpath_data, vfont->filepath);
}

static void vfont_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  VFont *vf = (VFont *)id;
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  vf->data = NULL;
  vf->temp_pf = NULL;

  /* Do not store packed files in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(vf) && !is_undo) {
    vf->packedfile = NULL;
  }

  /* write LibData */
  BLO_write_id_struct(writer, VFont, id_address, &vf->id);
  BKE_id_blend_write(writer, &vf->id);

  /* direct data */
  BKE_packedfile_blend_write(writer, vf->packedfile);
}

static void vfont_blend_read_data(BlendDataReader *reader, ID *id)
{
  VFont *vf = (VFont *)id;
  vf->data = NULL;
  vf->temp_pf = NULL;
  BKE_packedfile_blend_read(reader, &vf->packedfile);
}

IDTypeInfo IDType_ID_VF = {
    .id_code = ID_VF,
    .id_filter = FILTER_ID_VF,
    .main_listbase_index = INDEX_ID_VF,
    .struct_size = sizeof(VFont),
    .name = "Font",
    .name_plural = "fonts",
    .translation_context = BLT_I18NCONTEXT_ID_VFONT,
    .flags = IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    .asset_type_info = NULL,

    .init_data = vfont_init_data,
    .copy_data = vfont_copy_data,
    .free_data = vfont_free_data,
    .make_local = NULL,
    .foreach_id = NULL,
    .foreach_cache = NULL,
    .foreach_path = vfont_foreach_path,
    .owner_pointer_get = NULL,

    .blend_write = vfont_blend_write,
    .blend_read_data = vfont_blend_read_data,
    .blend_read_lib = NULL,
    .blend_read_expand = NULL,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/***************************** VFont *******************************/

void BKE_vfont_free_data(struct VFont *vfont)
{
  if (vfont->data) {
    if (vfont->data->characters) {
      GHashIterator gh_iter;
      GHASH_ITER (gh_iter, vfont->data->characters) {
        VChar *che = BLI_ghashIterator_getValue(&gh_iter);

        while (che->nurbsbase.first) {
          Nurb *nu = che->nurbsbase.first;
          if (nu->bezt) {
            MEM_freeN(nu->bezt);
          }
          BLI_freelinkN(&che->nurbsbase, nu);
        }

        MEM_freeN(che);
      }

      BLI_ghash_free(vfont->data->characters, NULL, NULL);
    }

    MEM_freeN(vfont->data);
    vfont->data = NULL;
  }

  if (vfont->temp_pf) {
    BKE_packedfile_free(vfont->temp_pf); /* NULL when the font file can't be found on disk */
    vfont->temp_pf = NULL;
  }
}

static const void *builtin_font_data = NULL;
static int builtin_font_size = 0;

bool BKE_vfont_is_builtin(const struct VFont *vfont)
{
  return STREQ(vfont->filepath, FO_BUILTIN_NAME);
}

void BKE_vfont_builtin_register(const void *mem, int size)
{
  builtin_font_data = mem;
  builtin_font_size = size;
}

static PackedFile *get_builtin_packedfile(void)
{
  if (!builtin_font_data) {
    CLOG_ERROR(&LOG, "Internal error, builtin font not loaded");

    return NULL;
  }

  void *mem = MEM_mallocN(builtin_font_size, "vfd_builtin");

  memcpy(mem, builtin_font_data, builtin_font_size);

  return BKE_packedfile_new_from_memory(mem, builtin_font_size);
}

static VFontData *vfont_get_data(VFont *vfont)
{
  if (vfont == NULL) {
    return NULL;
  }

  /* And then set the data */
  if (!vfont->data) {
    PackedFile *pf;

    BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_WRITE);

    if (vfont->data) {
      /* Check data again, since it might have been already
       * initialized from other thread (previous check is
       * not accurate or threading, just prevents unneeded
       * lock if all the data is here for sure).
       */
      BLI_rw_mutex_unlock(&vfont_rwlock);
      return vfont->data;
    }

    if (BKE_vfont_is_builtin(vfont)) {
      pf = get_builtin_packedfile();
    }
    else {
      if (vfont->packedfile) {
        pf = vfont->packedfile;

        /* We need to copy a tmp font to memory unless it is already there */
        if (vfont->temp_pf == NULL) {
          vfont->temp_pf = BKE_packedfile_duplicate(pf);
        }
      }
      else {
        pf = BKE_packedfile_new(NULL, vfont->filepath, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));

        if (vfont->temp_pf == NULL) {
          vfont->temp_pf = BKE_packedfile_new(
              NULL, vfont->filepath, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));
        }
      }
      if (!pf) {
        CLOG_WARN(&LOG, "Font file doesn't exist: %s", vfont->filepath);

        /* DON'T DO THIS
         * missing file shouldn't modify path! - campbell */
#if 0
        strcpy(vfont->filepath, FO_BUILTIN_NAME);
#endif
        pf = get_builtin_packedfile();
      }
    }

    if (pf) {
      vfont->data = BKE_vfontdata_from_freetypefont(pf);
      if (pf != vfont->packedfile) {
        BKE_packedfile_free(pf);
      }
    }

    BLI_rw_mutex_unlock(&vfont_rwlock);
  }

  return vfont->data;
}

VFont *BKE_vfont_load(Main *bmain, const char *filepath)
{
  char filename[FILE_MAXFILE];
  VFont *vfont = NULL;
  PackedFile *pf;
  bool is_builtin;

  if (STREQ(filepath, FO_BUILTIN_NAME)) {
    BLI_strncpy(filename, filepath, sizeof(filename));

    pf = get_builtin_packedfile();
    is_builtin = true;
  }
  else {
    BLI_split_file_part(filepath, filename, sizeof(filename));
    pf = BKE_packedfile_new(NULL, filepath, BKE_main_blendfile_path(bmain));

    is_builtin = false;
  }

  if (pf) {
    VFontData *vfd;

    vfd = BKE_vfontdata_from_freetypefont(pf);
    if (vfd) {
      /* If there's a font name, use it for the ID name. */
      vfont = BKE_libblock_alloc(bmain, ID_VF, vfd->name[0] ? vfd->name : filename, 0);
      vfont->data = vfd;
      BLI_strncpy(vfont->filepath, filepath, sizeof(vfont->filepath));

      /* if auto-pack is on store the packed-file in de font structure */
      if (!is_builtin && (G.fileflags & G_FILE_AUTOPACK)) {
        vfont->packedfile = pf;
      }

      /* Do not add #FO_BUILTIN_NAME to temporary list-base. */
      if (!STREQ(filename, FO_BUILTIN_NAME)) {
        vfont->temp_pf = BKE_packedfile_new(NULL, filepath, BKE_main_blendfile_path(bmain));
      }
    }

    /* Free the packed file */
    if (!vfont || vfont->packedfile != pf) {
      BKE_packedfile_free(pf);
    }
  }

  return vfont;
}

VFont *BKE_vfont_load_exists_ex(struct Main *bmain, const char *filepath, bool *r_exists)
{
  VFont *vfont;
  char str[FILE_MAX], strtest[FILE_MAX];

  BLI_strncpy(str, filepath, sizeof(str));
  BLI_path_abs(str, BKE_main_blendfile_path(bmain));

  /* first search an identical filepath */
  for (vfont = bmain->fonts.first; vfont; vfont = vfont->id.next) {
    BLI_strncpy(strtest, vfont->filepath, sizeof(vfont->filepath));
    BLI_path_abs(strtest, ID_BLEND_PATH(bmain, &vfont->id));

    if (BLI_path_cmp(strtest, str) == 0) {
      id_us_plus(&vfont->id); /* officially should not, it doesn't link here! */
      if (r_exists) {
        *r_exists = true;
      }
      return vfont;
    }
  }

  if (r_exists) {
    *r_exists = false;
  }
  return BKE_vfont_load(bmain, filepath);
}

VFont *BKE_vfont_load_exists(struct Main *bmain, const char *filepath)
{
  return BKE_vfont_load_exists_ex(bmain, filepath, NULL);
}

static VFont *which_vfont(Curve *cu, CharInfo *info)
{
  switch (info->flag & (CU_CHINFO_BOLD | CU_CHINFO_ITALIC)) {
    case CU_CHINFO_BOLD:
      return cu->vfontb ? cu->vfontb : cu->vfont;
    case CU_CHINFO_ITALIC:
      return cu->vfonti ? cu->vfonti : cu->vfont;
    case (CU_CHINFO_BOLD | CU_CHINFO_ITALIC):
      return cu->vfontbi ? cu->vfontbi : cu->vfont;
    default:
      return cu->vfont;
  }
}

VFont *BKE_vfont_builtin_get(void)
{
  VFont *vfont;

  for (vfont = G_MAIN->fonts.first; vfont; vfont = vfont->id.next) {
    if (BKE_vfont_is_builtin(vfont)) {
      return vfont;
    }
  }

  return BKE_vfont_load(G_MAIN, FO_BUILTIN_NAME);
}

static VChar *find_vfont_char(VFontData *vfd, uint character)
{
  return BLI_ghash_lookup(vfd->characters, POINTER_FROM_UINT(character));
}

static void build_underline(Curve *cu,
                            ListBase *nubase,
                            const rctf *rect,
                            float yofs,
                            float rot,
                            int charidx,
                            short mat_nr,
                            const float font_size)
{
  Nurb *nu2;
  BPoint *bp;

  nu2 = (Nurb *)MEM_callocN(sizeof(Nurb), "underline_nurb");
  nu2->resolu = cu->resolu;
  nu2->bezt = NULL;
  nu2->knotsu = nu2->knotsv = NULL;
  nu2->charidx = charidx + 1000;
  if (mat_nr > 0) {
    nu2->mat_nr = mat_nr - 1;
  }
  nu2->pntsu = 4;
  nu2->pntsv = 1;
  nu2->orderu = 4;
  nu2->orderv = 1;
  nu2->flagu = CU_NURB_CYCLIC;

  bp = (BPoint *)MEM_calloc_arrayN(4, sizeof(BPoint), "underline_bp");

  copy_v4_fl4(bp[0].vec, rect->xmin, (rect->ymax + yofs), 0.0f, 1.0f);
  copy_v4_fl4(bp[1].vec, rect->xmax, (rect->ymax + yofs), 0.0f, 1.0f);
  copy_v4_fl4(bp[2].vec, rect->xmax, (rect->ymin + yofs), 0.0f, 1.0f);
  copy_v4_fl4(bp[3].vec, rect->xmin, (rect->ymin + yofs), 0.0f, 1.0f);

  /* Used by curve extrusion. */
  bp[0].radius = bp[1].radius = bp[2].radius = bp[3].radius = 1.0f;

  nu2->bp = bp;
  BLI_addtail(nubase, nu2);

  if (rot != 0.0f) {
    float si = sinf(rot);
    float co = cosf(rot);

    for (int i = nu2->pntsu; i > 0; i--) {
      float *fp = bp->vec;

      float x = fp[0] - rect->xmin;
      float y = fp[1] - rect->ymin;

      fp[0] = (+co * x + si * y) + rect->xmin;
      fp[1] = (-si * x + co * y) + rect->ymin;

      bp++;
    }

    bp = nu2->bp;
  }

  mul_v2_fl(bp[0].vec, font_size);
  mul_v2_fl(bp[1].vec, font_size);
  mul_v2_fl(bp[2].vec, font_size);
  mul_v2_fl(bp[3].vec, font_size);
}

void BKE_vfont_build_char(Curve *cu,
                          ListBase *nubase,
                          uint character,
                          CharInfo *info,
                          float ofsx,
                          float ofsy,
                          float rot,
                          int charidx,
                          const float fsize)
{
  VFontData *vfd = vfont_get_data(which_vfont(cu, info));
  if (!vfd) {
    return;
  }

  /* make a copy at distance ofsx, ofsy with shear */
  float shear = cu->shear;
  float si = sinf(rot);
  float co = cosf(rot);

  VChar *che = find_vfont_char(vfd, character);

  /* Select the glyph data */
  Nurb *nu1 = NULL;
  if (che) {
    nu1 = che->nurbsbase.first;
  }

  /* Create the character */
  while (nu1) {
    BezTriple *bezt1 = nu1->bezt;
    if (bezt1) {
      Nurb *nu2 = (Nurb *)MEM_mallocN(sizeof(Nurb), "duplichar_nurb");
      if (nu2 == NULL) {
        break;
      }
      memcpy(nu2, nu1, sizeof(struct Nurb));
      nu2->resolu = cu->resolu;
      nu2->bp = NULL;
      nu2->knotsu = nu2->knotsv = NULL;
      nu2->flag = CU_SMOOTH;
      nu2->charidx = charidx;
      if (info->mat_nr > 0) {
        nu2->mat_nr = info->mat_nr - 1;
      }
      else {
        nu2->mat_nr = 0;
      }
      /* nu2->trim.first = 0; */
      /* nu2->trim.last = 0; */
      int u = nu2->pntsu;

      BezTriple *bezt2 = (BezTriple *)MEM_malloc_arrayN(u, sizeof(BezTriple), "duplichar_bezt2");
      if (bezt2 == NULL) {
        MEM_freeN(nu2);
        break;
      }
      memcpy(bezt2, bezt1, u * sizeof(struct BezTriple));
      nu2->bezt = bezt2;

      if (shear != 0.0f) {
        bezt2 = nu2->bezt;

        for (int i = nu2->pntsu; i > 0; i--) {
          bezt2->vec[0][0] += shear * bezt2->vec[0][1];
          bezt2->vec[1][0] += shear * bezt2->vec[1][1];
          bezt2->vec[2][0] += shear * bezt2->vec[2][1];
          bezt2++;
        }
      }
      if (rot != 0.0f) {
        bezt2 = nu2->bezt;
        for (int i = nu2->pntsu; i > 0; i--) {
          float *fp = bezt2->vec[0];

          float x = fp[0];
          fp[0] = co * x + si * fp[1];
          fp[1] = -si * x + co * fp[1];
          x = fp[3];
          fp[3] = co * x + si * fp[4];
          fp[4] = -si * x + co * fp[4];
          x = fp[6];
          fp[6] = co * x + si * fp[7];
          fp[7] = -si * x + co * fp[7];

          bezt2++;
        }
      }
      bezt2 = nu2->bezt;

      if (info->flag & CU_CHINFO_SMALLCAPS_CHECK) {
        const float sca = cu->smallcaps_scale;
        for (int i = nu2->pntsu; i > 0; i--) {
          float *fp = bezt2->vec[0];
          fp[0] *= sca;
          fp[1] *= sca;
          fp[3] *= sca;
          fp[4] *= sca;
          fp[6] *= sca;
          fp[7] *= sca;
          bezt2++;
        }
      }
      bezt2 = nu2->bezt;

      for (int i = nu2->pntsu; i > 0; i--) {
        float *fp = bezt2->vec[0];
        fp[0] = (fp[0] + ofsx) * fsize;
        fp[1] = (fp[1] + ofsy) * fsize;
        fp[3] = (fp[3] + ofsx) * fsize;
        fp[4] = (fp[4] + ofsy) * fsize;
        fp[6] = (fp[6] + ofsx) * fsize;
        fp[7] = (fp[7] + ofsy) * fsize;
        bezt2++;
      }

      BLI_addtail(nubase, nu2);
    }

    nu1 = nu1->next;
  }
}

int BKE_vfont_select_get(Object *ob, int *r_start, int *r_end)
{
  Curve *cu = ob->data;
  EditFont *ef = cu->editfont;
  int start, end, direction;

  if ((ob->type != OB_FONT) || (ef == NULL)) {
    return 0;
  }

  BLI_assert(ef->len >= 0);
  BLI_assert(ef->selstart >= 0 && ef->selstart <= ef->len + 1);
  BLI_assert(ef->selend >= 0 && ef->selend <= ef->len + 1);
  BLI_assert(ef->pos >= 0 && ef->pos <= ef->len);

  if (ef->selstart == 0) {
    return 0;
  }

  if (ef->selstart <= ef->selend) {
    start = ef->selstart - 1;
    end = ef->selend - 1;
    direction = 1;
  }
  else {
    start = ef->selend;
    end = ef->selstart - 2;
    direction = -1;
  }

  if (start == end + 1) {
    return 0;
  }

  BLI_assert(start < end + 1);
  *r_start = start;
  *r_end = end;
  return direction;
}

void BKE_vfont_select_clamp(Object *ob)
{
  Curve *cu = ob->data;
  EditFont *ef = cu->editfont;

  BLI_assert((ob->type == OB_FONT) && ef);

  CLAMP_MAX(ef->pos, ef->len);
  CLAMP_MAX(ef->selstart, ef->len + 1);
  CLAMP_MAX(ef->selend, ef->len);
}

static float char_width(Curve *cu, VChar *che, CharInfo *info)
{
  /* The character wasn't found, probably ascii = 0, then the width shall be 0 as well */
  if (che == NULL) {
    return 0.0f;
  }
  if (info->flag & CU_CHINFO_SMALLCAPS_CHECK) {
    return che->width * cu->smallcaps_scale;
  }

  return che->width;
}

static void textbox_scale(TextBox *tb_dst, const TextBox *tb_src, float scale)
{
  tb_dst->x = tb_src->x * scale;
  tb_dst->y = tb_src->y * scale;
  tb_dst->w = tb_src->w * scale;
  tb_dst->h = tb_src->h * scale;
}

/**
 * Used for storing per-line data for alignment & wrapping.
 */
struct TempLineInfo {
  float x_min;   /* left margin */
  float x_max;   /* right margin */
  int char_nr;   /* number of characters */
  int wspace_nr; /* number of white-spaces of line */
};

/* -------------------------------------------------------------------- */
/** \name VFont Scale Overflow
 *
 * Scale the font to fit inside #TextBox bounds.
 *
 * - Scale horizontally when #TextBox.h is zero,
 *   otherwise scale vertically, allowing the text to wrap horizontally.
 * - Never increase scale to fit, only ever scale on overflow.
 * \{ */

typedef struct VFontToCurveIter {
  int iteraction;
  float scale_to_fit;
  struct {
    float min;
    float max;
  } bisect;
  bool ok;
  /**
   * Wrap words that extends beyond the text-box width (enabled by default).
   *
   * Currently only disabled when scale-to-fit is enabled,
   * so floating-point error doesn't cause unexpected wrapping, see #89241.
   *
   * \note This should only be set once, in the #VFONT_TO_CURVE_INIT pass
   * otherwise iterations wont behave predictably, see #91401.
   */
  bool word_wrap;
  int status;
} VFontToCurveIter;

enum {
  VFONT_TO_CURVE_INIT = 0,
  VFONT_TO_CURVE_BISECT,
  VFONT_TO_CURVE_SCALE_ONCE,
  VFONT_TO_CURVE_DONE,
};

#define FONT_TO_CURVE_SCALE_ITERATIONS 20
#define FONT_TO_CURVE_SCALE_THRESHOLD 0.0001f

/** \} */

/**
 * Font metric values explained:
 *
 * Baseline: Line where the text "rests", used as the origin vertical position for the glyphs.
 * Em height: Space most glyphs should fit within.
 * Ascent: the recommended distance above the baseline to fit most characters.
 * Descent: the recommended distance below the baseline to fit most characters.
 *
 * We obtain ascent and descent from the font itself (FT_Face->ascender / face->height).
 * And in some cases it is even the same value as FT_Face->bbox.yMax/yMin
 * (font top and bottom respectively).
 *
 * The em_height here is relative to FT_Face->bbox.
 */

static float vfont_ascent(const VFontData *vfd)
{
  return vfd->ascender * vfd->em_height;
}
static float vfont_descent(const VFontData *vfd)
{
  return vfd->em_height - vfont_ascent(vfd);
}

static bool vfont_to_curve(Object *ob,
                           Curve *cu,
                           int mode,
                           VFontToCurveIter *iter_data,
                           ListBase *r_nubase,
                           const char32_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           struct CharTrans **r_chartransdata)
{
  EditFont *ef = cu->editfont;
  EditFontSelBox *selboxes = NULL;
  VFont *vfont, *oldvfont;
  VFontData *vfd = NULL;
  CharInfo *info = NULL, *custrinfo;
  TextBox tb_scale;
  bool use_textbox;
  VChar *che;
  struct CharTrans *chartransdata = NULL, *ct;
  struct TempLineInfo *lineinfo;
  float *f, xof, yof, xtrax, linedist;
  float twidth = 0, maxlen = 0;
  int i, slen, j;
  int curbox;
  int selstart = 0, selend = 0;
  int cnr = 0, lnr = 0, wsnr = 0;
  const char32_t *mem = NULL;
  char32_t ascii;
  bool ok = false;
  const float font_size = cu->fsize * iter_data->scale_to_fit;
  const bool word_wrap = iter_data->word_wrap;
  const float xof_scale = safe_divide(cu->xof, font_size);
  const float yof_scale = safe_divide(cu->yof, font_size);
  int last_line = -1;
  /* Length of the text disregarding \n breaks. */
  float current_line_length = 0.0f;
  float longest_line_length = 0.0f;

  /* Text at the beginning of the last used text-box (use for y-axis alignment).
   * We over-allocate by one to simplify logic of getting last char. */
  int *i_textbox_array = MEM_callocN(sizeof(*i_textbox_array) * (cu->totbox + 1),
                                     "TextBox initial char index");

#define MARGIN_X_MIN (xof_scale + tb_scale.x)
#define MARGIN_Y_MIN (yof_scale + tb_scale.y)

  /* NOTE: do calculations including the trailing '\0' of a string
   * because the cursor can be at that location. */

  BLI_assert(ob == NULL || ob->type == OB_FONT);

  /* Set font data */
  vfont = cu->vfont;

  if (cu->str == NULL) {
    return ok;
  }
  if (vfont == NULL) {
    return ok;
  }

  vfd = vfont_get_data(vfont);

  /* The VFont Data can not be found */
  if (!vfd) {
    return ok;
  }

  if (ef) {
    slen = ef->len;
    mem = ef->textbuf;
    custrinfo = ef->textbufinfo;
  }
  else {
    char32_t *mem_tmp;
    slen = cu->len_char32;

    /* Create unicode string */
    mem_tmp = MEM_malloc_arrayN((slen + 1), sizeof(*mem_tmp), "convertedmem");
    if (!mem_tmp) {
      return ok;
    }

    BLI_str_utf8_as_utf32(mem_tmp, cu->str, slen + 1);

    if (cu->strinfo == NULL) { /* old file */
      cu->strinfo = MEM_calloc_arrayN((slen + 4), sizeof(CharInfo), "strinfo compat");
    }
    custrinfo = cu->strinfo;
    if (!custrinfo) {
      return ok;
    }

    mem = mem_tmp;
  }

  if (cu->tb == NULL) {
    cu->tb = MEM_calloc_arrayN(MAXTEXTBOX, sizeof(TextBox), "TextBox compat");
  }

  if (ef != NULL && ob != NULL) {
    if (ef->selboxes) {
      MEM_freeN(ef->selboxes);
    }

    if (BKE_vfont_select_get(ob, &selstart, &selend)) {
      ef->selboxes_len = (selend - selstart) + 1;
      ef->selboxes = MEM_calloc_arrayN(ef->selboxes_len, sizeof(EditFontSelBox), "font selboxes");
    }
    else {
      ef->selboxes_len = 0;
      ef->selboxes = NULL;
    }

    selboxes = ef->selboxes;
  }

  /* calc offset and rotation of each char */
  ct = chartransdata = MEM_calloc_arrayN((slen + 1), sizeof(struct CharTrans), "buildtext");

  /* We assume the worst case: 1 character per line (is freed at end anyway) */
  lineinfo = MEM_malloc_arrayN((slen * 2 + 1), sizeof(*lineinfo), "lineinfo");

  linedist = cu->linedist;

  curbox = 0;
  textbox_scale(&tb_scale, &cu->tb[curbox], safe_divide(1.0f, font_size));
  use_textbox = (tb_scale.w != 0.0f);

  xof = MARGIN_X_MIN;
  yof = MARGIN_Y_MIN;

  xtrax = 0.5f * cu->spacing - 0.5f;

  oldvfont = NULL;

  for (i = 0; i < slen; i++) {
    custrinfo[i].flag &= ~(CU_CHINFO_WRAP | CU_CHINFO_SMALLCAPS_CHECK | CU_CHINFO_OVERFLOW);
  }

  i = 0;
  while (i <= slen) {
    /* Characters in the list */
    info = &custrinfo[i];
    ascii = mem[i];
    if (info->flag & CU_CHINFO_SMALLCAPS) {
      ascii = towupper(ascii);
      if (mem[i] != ascii) {
        info->flag |= CU_CHINFO_SMALLCAPS_CHECK;
      }
    }

    vfont = which_vfont(cu, info);

    if (vfont == NULL) {
      break;
    }

    if (vfont != oldvfont) {
      vfd = vfont_get_data(vfont);
      oldvfont = vfont;
    }

    /* VFont Data for VFont couldn't be found */
    if (!vfd) {
      MEM_freeN(chartransdata);
      chartransdata = NULL;
      MEM_freeN(lineinfo);
      goto finally;
    }

    if (!ELEM(ascii, '\n', '\0')) {
      BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_READ);
      che = find_vfont_char(vfd, ascii);
      BLI_rw_mutex_unlock(&vfont_rwlock);

      /* The character wasn't in the current curve base so load it.
       * But if the font is built-in then do not try loading since
       * whole font is in the memory already. */
      if (che == NULL && BKE_vfont_is_builtin(vfont) == false) {
        BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_WRITE);
        /* Check it once again, char might have been already load
         * between previous #BLI_rw_mutex_unlock() and this #BLI_rw_mutex_lock().
         *
         * Such a check should not be a bottleneck since it wouldn't
         * happen often once all the chars are load. */
        if ((che = find_vfont_char(vfd, ascii)) == NULL) {
          che = BKE_vfontdata_char_from_freetypefont(vfont, ascii);
        }
        BLI_rw_mutex_unlock(&vfont_rwlock);
      }
    }
    else {
      che = NULL;
    }

    twidth = char_width(cu, che, info);

    /* Calculate positions. */

    if ((tb_scale.w != 0.0f) && (ct->dobreak == 0)) { /* May need wrapping. */
      const float x_available = xof_scale + tb_scale.w;
      const float x_used = (xof - tb_scale.x) + twidth;

      if (word_wrap == false) {
        /* When scale to fit is used, don't do any wrapping.
         *
         * Floating precision error can cause the text to be slightly larger.
         * Assert this is a small value as large values indicate incorrect
         * calculations with scale-to-fit which shouldn't be ignored. See #89241. */
        if (x_used > x_available) {
          BLI_assert_msg(compare_ff_relative(x_used, x_available, FLT_EPSILON, 64),
                         "VFontToCurveIter.scale_to_fit not set correctly!");
        }
      }
      else if (x_used > x_available) {
        // CLOG_WARN(&LOG, "linewidth exceeded: %c%c%c...", mem[i], mem[i+1], mem[i+2]);
        bool dobreak = false;
        for (j = i; (mem[j] != '\n') && (chartransdata[j].dobreak == 0); j--) {

          /* Special case when there are no breaks possible. */
          if (UNLIKELY(j == 0)) {
            if (i == slen) {
              /* Use the behavior of zero a height text-box when a break cannot be inserted.
               *
               * Typically when a text-box has any height and overflow is set to scale
               * the text will wrap to fit the width as necessary. When wrapping isn't
               * possible it's important to use the same code-path as zero-height lines.
               * Without this exception a single word will not scale-to-fit (see: #95116). */
              tb_scale.h = 0.0f;
            }
            break;
          }

          if (ELEM(mem[j], ' ', '-')) {
            ct -= (i - (j - 1));
            cnr -= (i - (j - 1));
            if (mem[j] == ' ') {
              wsnr--;
            }
            if (mem[j] == '-') {
              wsnr++;
            }
            i = j - 1;
            xof = ct->xof;
            ct[1].dobreak = 1;
            custrinfo[i + 1].flag |= CU_CHINFO_WRAP;
            dobreak = true;
            break;
          }
          BLI_assert(chartransdata[j].dobreak == 0);
        }

        if (dobreak) {
          if (tb_scale.h == 0.0f) {
            /* NOTE: If underlined text is truncated away, the extra space is also truncated. */
            custrinfo[i + 1].flag |= CU_CHINFO_OVERFLOW;
          }
          /* Since a break was added, re-run this loop with `i` at it's new value. */
          continue;
        }
      }
    }

    if (ascii == '\n' || ascii == 0 || ct->dobreak) {
      ct->xof = xof;
      ct->yof = yof;
      ct->linenr = lnr;
      ct->charnr = cnr;

      yof -= linedist;

      lineinfo[lnr].x_min = (xof - xtrax) - tb_scale.x;
      lineinfo[lnr].x_max = tb_scale.w;
      lineinfo[lnr].char_nr = cnr;
      lineinfo[lnr].wspace_nr = wsnr;

      CLAMP_MIN(maxlen, lineinfo[lnr].x_min);

      if ((tb_scale.h != 0.0f) && (-(yof - tb_scale.y) > (tb_scale.h - linedist) - yof_scale)) {
        if (cu->totbox > (curbox + 1)) {
          maxlen = 0;
          curbox++;
          i_textbox_array[curbox] = i + 1;

          textbox_scale(&tb_scale, &cu->tb[curbox], 1.0f / font_size);

          yof = MARGIN_Y_MIN;
        }
        else if (last_line == -1) {
          last_line = lnr + 1;
          info->flag |= CU_CHINFO_OVERFLOW;
        }
      }

      current_line_length += xof - MARGIN_X_MIN;
      if (ct->dobreak) {
        current_line_length += twidth;
      }
      else {
        longest_line_length = MAX2(current_line_length, longest_line_length);
        current_line_length = 0.0f;
      }

      xof = MARGIN_X_MIN;
      lnr++;
      cnr = 0;
      wsnr = 0;
    }
    else if (ascii == '\t') { /* Tab character. */
      float tabfac;

      ct->xof = xof;
      ct->yof = yof;
      ct->linenr = lnr;
      ct->charnr = cnr++;

      tabfac = (xof - MARGIN_X_MIN + 0.01f);
      tabfac = 2.0f * ceilf(tabfac / 2.0f);
      xof = MARGIN_X_MIN + tabfac;
    }
    else {
      EditFontSelBox *sb = NULL;
      float wsfac;

      ct->xof = xof;
      ct->yof = yof;
      ct->linenr = lnr;
      ct->charnr = cnr++;

      if (selboxes && (i >= selstart) && (i <= selend)) {
        sb = &selboxes[i - selstart];
        sb->y = yof * font_size - linedist * font_size * 0.1f;
        sb->h = linedist * font_size;
        sb->w = xof * font_size;
      }

      if (ascii == ' ') { /* Space character. */
        wsfac = cu->wordspace;
        wsnr++;
      }
      else {
        wsfac = 1.0f;
      }

      /* Set the width of the character. */
      twidth = char_width(cu, che, info);

      xof += (twidth * wsfac * (1.0f + (info->kern / 40.0f))) + xtrax;

      if (sb) {
        sb->w = (xof * font_size) - sb->w;
      }
    }
    ct++;
    i++;
  }

  current_line_length += xof + twidth - MARGIN_X_MIN;
  longest_line_length = MAX2(current_line_length, longest_line_length);

  cu->lines = 1;
  for (i = 0; i <= slen; i++) {
    ascii = mem[i];
    ct = &chartransdata[i];
    if (ascii == '\n' || ct->dobreak) {
      cu->lines++;
    }
  }

  /* Line-data is now: width of line. */

  if (cu->spacemode != CU_ALIGN_X_LEFT) {
    ct = chartransdata;

    if (cu->spacemode == CU_ALIGN_X_RIGHT) {
      struct TempLineInfo *li;

      for (i = 0, li = lineinfo; i < lnr; i++, li++) {
        li->x_min = (li->x_max - li->x_min) + xof_scale;
      }

      for (i = 0; i <= slen; i++) {
        ct->xof += lineinfo[ct->linenr].x_min;
        ct++;
      }
    }
    else if (cu->spacemode == CU_ALIGN_X_MIDDLE) {
      struct TempLineInfo *li;

      for (i = 0, li = lineinfo; i < lnr; i++, li++) {
        li->x_min = ((li->x_max - li->x_min) + xof_scale) / 2.0f;
      }

      for (i = 0; i <= slen; i++) {
        ct->xof += lineinfo[ct->linenr].x_min;
        ct++;
      }
    }
    else if ((cu->spacemode == CU_ALIGN_X_FLUSH) && use_textbox) {
      struct TempLineInfo *li;

      for (i = 0, li = lineinfo; i < lnr; i++, li++) {
        li->x_min = ((li->x_max - li->x_min) + xof_scale);

        if (li->char_nr > 1) {
          li->x_min /= (float)(li->char_nr - 1);
        }
      }
      for (i = 0; i <= slen; i++) {
        for (j = i; !ELEM(mem[j], '\0', '\n') && (chartransdata[j].dobreak == 0) && (j < slen);
             j++) {
          /* do nothing */
        }

        //              if ((mem[j] != '\n') && (mem[j])) {
        ct->xof += ct->charnr * lineinfo[ct->linenr].x_min;
        //              }
        ct++;
      }
    }
    else if ((cu->spacemode == CU_ALIGN_X_JUSTIFY) && use_textbox) {
      float curofs = 0.0f;
      for (i = 0; i <= slen; i++) {
        for (j = i; (mem[j]) && (mem[j] != '\n') && (chartransdata[j].dobreak == 0) && (j < slen);
             j++) {
          /* pass */
        }

        if ((mem[j] != '\n') && (chartransdata[j].dobreak != 0)) {
          if (mem[i] == ' ') {
            struct TempLineInfo *li;

            li = &lineinfo[ct->linenr];
            curofs += ((li->x_max - li->x_min) + xof_scale) / (float)li->wspace_nr;
          }
          ct->xof += curofs;
        }
        if (mem[i] == '\n' || chartransdata[i].dobreak) {
          curofs = 0;
        }
        ct++;
      }
    }
  }

  /* Top-baseline is default, in this case, do nothing. */
  if (cu->align_y != CU_ALIGN_Y_TOP_BASELINE) {
    if (tb_scale.h != 0.0f) {
      /* We need to loop all the text-boxes even the "full" ones.
       * This way they all get the same vertical padding. */
      for (int tb_index = 0; tb_index < cu->totbox; tb_index++) {
        struct CharTrans *ct_first, *ct_last;
        const int i_textbox = i_textbox_array[tb_index];
        const int i_textbox_next = i_textbox_array[tb_index + 1];
        const bool is_last_filled_textbox = ELEM(i_textbox_next, 0, slen + 1);
        int lines;

        ct_first = chartransdata + i_textbox;
        ct_last = chartransdata + (is_last_filled_textbox ? slen : i_textbox_next - 1);
        lines = ct_last->linenr - ct_first->linenr + 1;

        if (cu->overflow == CU_OVERFLOW_TRUNCATE) {
          /* Ensure overflow doesn't truncate text, before centering vertically
           * giving odd/buggy results, see: #66614. */
          if ((tb_index == cu->totbox - 1) && (last_line != -1)) {
            lines = last_line - ct_first->linenr;
          }
        }

        textbox_scale(&tb_scale, &cu->tb[tb_index], 1.0f / font_size);
        /* The initial Y origin of the text-box is hard-coded to 1.0f * text scale. */
        const float textbox_y_origin = 1.0f;
        float yoff = 0.0f;

        switch (cu->align_y) {
          case CU_ALIGN_Y_TOP_BASELINE:
            break;
          case CU_ALIGN_Y_TOP:
            yoff = textbox_y_origin - vfont_ascent(vfd);
            break;
          case CU_ALIGN_Y_CENTER:
            yoff = ((((vfd->em_height + (lines - 1) * linedist) * 0.5f) - vfont_ascent(vfd)) -
                    (tb_scale.h * 0.5f) + textbox_y_origin);
            break;
          case CU_ALIGN_Y_BOTTOM_BASELINE:
            yoff = textbox_y_origin + ((lines - 1) * linedist) - tb_scale.h;
            break;
          case CU_ALIGN_Y_BOTTOM:
            yoff = textbox_y_origin + ((lines - 1) * linedist) - tb_scale.h + vfont_descent(vfd);
            break;
        }

        for (ct = ct_first; ct <= ct_last; ct++) {
          ct->yof += yoff;
        }

        if (is_last_filled_textbox) {
          break;
        }
      }
    }
    else {
      /* Non text-box case handled separately. */
      float yoff = 0.0f;

      switch (cu->align_y) {
        case CU_ALIGN_Y_TOP_BASELINE:
          break;
        case CU_ALIGN_Y_TOP:
          yoff = -vfont_ascent(vfd);
          break;
        case CU_ALIGN_Y_CENTER:
          yoff = ((vfd->em_height + (lnr - 1) * linedist) * 0.5f) - vfont_ascent(vfd);
          break;
        case CU_ALIGN_Y_BOTTOM_BASELINE:
          yoff = (lnr - 1) * linedist;
          break;
        case CU_ALIGN_Y_BOTTOM:
          yoff = (lnr - 1) * linedist + vfont_descent(vfd);
          break;
      }

      ct = chartransdata;
      for (i = 0; i <= slen; i++) {
        ct->yof += yoff;
        ct++;
      }
    }
  }

  MEM_freeN(lineinfo);
  MEM_freeN(i_textbox_array);

  /* TEXT ON CURVE */
  /* NOTE: Only #OB_CURVES_LEGACY objects could have a path. */
  if (cu->textoncurve && cu->textoncurve->type == OB_CURVES_LEGACY) {
    BLI_assert(cu->textoncurve->runtime.curve_cache != NULL);
    if (cu->textoncurve->runtime.curve_cache != NULL &&
        cu->textoncurve->runtime.curve_cache->anim_path_accum_length != NULL) {
      float distfac, imat[4][4], imat3[3][3], cmat[3][3];
      float minx, maxx;
      float timeofs, sizefac;

      if (ob != NULL) {
        invert_m4_m4(imat, ob->object_to_world);
      }
      else {
        unit_m4(imat);
      }
      copy_m3_m4(imat3, imat);

      copy_m3_m4(cmat, cu->textoncurve->object_to_world);
      mul_m3_m3m3(cmat, cmat, imat3);
      sizefac = normalize_v3(cmat[0]) / font_size;

      ct = chartransdata;
      minx = maxx = ct->xof;
      ct++;
      for (i = 1; i <= slen; i++, ct++) {
        if (minx > ct->xof) {
          minx = ct->xof;
        }
        if (maxx < ct->xof) {
          maxx = ct->xof;
        }
      }

      /* We put the x-coordinate exact at the curve, the y is rotated. */

      /* length correction */
      const float chartrans_size_x = maxx - minx;
      if (chartrans_size_x != 0.0f) {
        const CurveCache *cc = cu->textoncurve->runtime.curve_cache;
        const float totdist = BKE_anim_path_get_length(cc);
        distfac = (sizefac * totdist) / chartrans_size_x;
        distfac = (distfac > 1.0f) ? (1.0f / distfac) : 1.0f;
      }
      else {
        /* Happens when there are no characters, set this value to place the text cursor. */
        distfac = 0.0f;
      }

      timeofs = 0.0f;

      if (distfac < 1.0f) {
        /* Path longer than text: space-mode is involved. */

        if (cu->spacemode == CU_ALIGN_X_RIGHT) {
          timeofs = 1.0f - distfac;
        }
        else if (cu->spacemode == CU_ALIGN_X_MIDDLE) {
          timeofs = (1.0f - distfac) / 2.0f;
        }
        else if (cu->spacemode == CU_ALIGN_X_FLUSH) {
          distfac = 1.0f;
        }
      }

      if (chartrans_size_x != 0.0f) {
        distfac /= chartrans_size_x;
      }

      timeofs += distfac * cu->xof; /* not cyclic */

      ct = chartransdata;
      for (i = 0; i <= slen; i++, ct++) {
        float ctime, dtime, vec[4], rotvec[3];
        float si, co;

        /* Rotate around center character. */
        info = &custrinfo[i];
        ascii = mem[i];
        if (info->flag & CU_CHINFO_SMALLCAPS_CHECK) {
          ascii = towupper(ascii);
        }

        che = find_vfont_char(vfd, ascii);

        twidth = char_width(cu, che, info);

        dtime = distfac * 0.5f * twidth;

        ctime = timeofs + distfac * (ct->xof - minx);
        CLAMP(ctime, 0.0f, 1.0f);

        /* Calculate the right loc AND the right rot separately. */
        /* `vec` needs 4 items. */
        BKE_where_on_path(cu->textoncurve, ctime, vec, NULL, NULL, NULL, NULL);
        BKE_where_on_path(cu->textoncurve, ctime + dtime, NULL, rotvec, NULL, NULL, NULL);

        mul_v3_fl(vec, sizefac);

        ct->rot = (float)M_PI - atan2f(rotvec[1], rotvec[0]);

        si = sinf(ct->rot);
        co = cosf(ct->rot);

        yof = ct->yof;

        ct->xof = vec[0] + si * yof;
        ct->yof = vec[1] + co * yof;

        if (selboxes && (i >= selstart) && (i <= selend)) {
          EditFontSelBox *sb;
          sb = &selboxes[i - selstart];
          sb->rot = -ct->rot;
        }
      }
    }
  }

  if (selboxes) {
    ct = chartransdata;
    for (i = 0; i <= selend; i++, ct++) {
      if (i >= selstart) {
        selboxes[i - selstart].x = ct->xof * font_size;
        selboxes[i - selstart].y = (ct->yof - 0.25f) * font_size;
        selboxes[i - selstart].h = font_size;
      }
    }
  }

  if (ELEM(mode, FO_CURSUP, FO_CURSDOWN, FO_PAGEUP, FO_PAGEDOWN) &&
      iter_data->status == VFONT_TO_CURVE_INIT) {
    ct = &chartransdata[ef->pos];

    if (ELEM(mode, FO_CURSUP, FO_PAGEUP) && ct->linenr == 0) {
      /* pass */
    }
    else if (ELEM(mode, FO_CURSDOWN, FO_PAGEDOWN) && ct->linenr == lnr) {
      /* pass */
    }
    else {
      switch (mode) {
        case FO_CURSUP:
          lnr = ct->linenr - 1;
          break;
        case FO_CURSDOWN:
          lnr = ct->linenr + 1;
          break;
        case FO_PAGEUP:
          lnr = ct->linenr - 10;
          break;
        case FO_PAGEDOWN:
          lnr = ct->linenr + 10;
          break;
      }
      cnr = ct->charnr;
      /* Seek for char with `lnr` & `cnr`. */
      ef->pos = 0;
      ct = chartransdata;
      for (i = 0; i < slen; i++) {
        if (ct->linenr == lnr) {
          if ((ct->charnr == cnr) || ((ct + 1)->charnr == 0)) {
            break;
          }
        }
        else if (ct->linenr > lnr) {
          break;
        }
        ef->pos++;
        ct++;
      }
    }
  }

  /* Cursor first. */
  if (ef) {
    float si, co;

    ct = &chartransdata[ef->pos];
    si = sinf(ct->rot);
    co = cosf(ct->rot);

    f = ef->textcurs[0];

    f[0] = font_size * (-0.02f * co + ct->xof);
    f[1] = font_size * (0.1f * si - (0.25f * co) + ct->yof);

    f[2] = font_size * (0.02f * co + ct->xof);
    f[3] = font_size * (-0.1f * si - (0.25f * co) + ct->yof);

    f[4] = font_size * (0.02f * co + 0.8f * si + ct->xof);
    f[5] = font_size * (-0.1f * si + 0.75f * co + ct->yof);

    f[6] = font_size * (-0.02f * co + 0.8f * si + ct->xof);
    f[7] = font_size * (0.1f * si + 0.75f * co + ct->yof);
  }

  if (mode == FO_SELCHANGE) {
    MEM_freeN(chartransdata);
    chartransdata = NULL;
  }
  else if (mode == FO_EDIT) {
    /* Make NURBS-data. */
    BKE_nurbList_free(r_nubase);

    ct = chartransdata;
    for (i = 0; i < slen; i++) {
      uint cha = (uint)mem[i];
      info = &(custrinfo[i]);

      if ((cu->overflow == CU_OVERFLOW_TRUNCATE) && (ob && ob->mode != OB_MODE_EDIT) &&
          (info->flag & CU_CHINFO_OVERFLOW)) {
        break;
      }

      if (info->flag & CU_CHINFO_SMALLCAPS_CHECK) {
        cha = towupper(cha);
      }

      /* Only do that check in case we do have an object, otherwise all materials get erased every
       * time that code is called without an object. */
      if (ob != NULL && (info->mat_nr > (ob->totcol))) {
        // CLOG_ERROR(
        //     &LOG, "Illegal material index (%d) in text object, setting to 0", info->mat_nr);
        info->mat_nr = 0;
      }
      /* We don't want to see any character for '\n'. */
      if (cha != '\n') {
        BKE_vfont_build_char(cu, r_nubase, cha, info, ct->xof, ct->yof, ct->rot, i, font_size);
      }

      if ((info->flag & CU_CHINFO_UNDERLINE) && (cha != '\n')) {
        float ulwidth, uloverlap = 0.0f;
        rctf rect;

        if ((i < (slen - 1)) && (mem[i + 1] != '\n') &&
            ((mem[i + 1] != ' ') || (custrinfo[i + 1].flag & CU_CHINFO_UNDERLINE)) &&
            ((custrinfo[i + 1].flag & CU_CHINFO_WRAP) == 0)) {
          uloverlap = xtrax + 0.1f;
        }
        /* Find the character, the characters has to be in the memory already
         * since character checking has been done earlier already. */
        che = find_vfont_char(vfd, cha);

        twidth = char_width(cu, che, info);
        ulwidth = (twidth * (1.0f + (info->kern / 40.0f))) + uloverlap;

        rect.xmin = ct->xof;
        rect.xmax = rect.xmin + ulwidth;

        rect.ymin = ct->yof;
        rect.ymax = rect.ymin - cu->ulheight;

        build_underline(
            cu, r_nubase, &rect, cu->ulpos - 0.05f, ct->rot, i, info->mat_nr, font_size);
      }
      ct++;
    }
  }

  if (iter_data->status == VFONT_TO_CURVE_SCALE_ONCE) {
    /* That means we were in a final run, just exit. */
    BLI_assert(cu->overflow == CU_OVERFLOW_SCALE);
    iter_data->status = VFONT_TO_CURVE_DONE;
  }
  else if (cu->overflow == CU_OVERFLOW_NONE) {
    /* Do nothing. */
  }
  else if ((tb_scale.h == 0.0f) && (tb_scale.w == 0.0f)) {
    /* Do nothing. */
  }
  else if (cu->overflow == CU_OVERFLOW_SCALE) {
    if ((cu->totbox == 1) && ((tb_scale.w == 0.0f) || (tb_scale.h == 0.0f))) {
      /* These are special cases, simpler to deal with. */
      if (tb_scale.w == 0.0f) {
        /* This is a potential vertical overflow.
         * Since there is no width limit, all the new lines are from line breaks. */
        if ((last_line != -1) && (lnr > last_line)) {
          const float total_text_height = lnr * linedist;
          iter_data->scale_to_fit = tb_scale.h / total_text_height;
          iter_data->status = VFONT_TO_CURVE_SCALE_ONCE;
          iter_data->word_wrap = false;
        }
      }
      else if (tb_scale.h == 0.0f) {
        /* This is a horizontal overflow. */
        if (longest_line_length > tb_scale.w) {
          /* We make sure longest line before it broke can fit here. */
          float scale_to_fit = tb_scale.w / longest_line_length;

          iter_data->scale_to_fit = scale_to_fit;
          iter_data->status = VFONT_TO_CURVE_SCALE_ONCE;
          iter_data->word_wrap = false;
        }
      }
    }
    else {
      /* This is the really complicated case, the best we can do is to iterate over
       * this function a few times until we get an acceptable result.
       *
       * Keep in mind that there is no single number that will make all fit to the end.
       * In a way, our ultimate goal is to get the highest scale that still leads to the
       * number of extra lines to zero. */
      if (iter_data->status == VFONT_TO_CURVE_INIT) {
        bool valid = true;

        for (int tb_index = 0; tb_index <= curbox; tb_index++) {
          TextBox *tb = &cu->tb[tb_index];
          if ((tb->w == 0.0f) || (tb->h == 0.0f)) {
            valid = false;
            break;
          }
        }

        if (valid && (last_line != -1) && (lnr > last_line)) {
          const float total_text_height = lnr * linedist;
          float scale_to_fit = tb_scale.h / total_text_height;

          iter_data->bisect.max = 1.0f;
          iter_data->bisect.min = scale_to_fit;

          iter_data->status = VFONT_TO_CURVE_BISECT;
        }
      }
      else {
        BLI_assert(iter_data->status == VFONT_TO_CURVE_BISECT);
        /* Try to get the highest scale that gives us the exactly
         * number of lines we need. */
        bool valid = false;

        if ((last_line != -1) && (lnr > last_line)) {
          /* It is overflowing, scale it down. */
          iter_data->bisect.max = iter_data->scale_to_fit;
        }
        else {
          /* It fits inside the text-box, scale it up. */
          iter_data->bisect.min = iter_data->scale_to_fit;
          valid = true;
        }

        /* Bisecting to try to find the best fit. */
        iter_data->scale_to_fit = (iter_data->bisect.max + iter_data->bisect.min) * 0.5f;

        /* We iterated enough or got a good enough result. */
        if ((!iter_data->iteraction--) || ((iter_data->bisect.max - iter_data->bisect.min) <
                                           (cu->fsize * FONT_TO_CURVE_SCALE_THRESHOLD))) {
          if (valid) {
            iter_data->status = VFONT_TO_CURVE_DONE;
          }
          else {
            iter_data->scale_to_fit = iter_data->bisect.min;
            iter_data->status = VFONT_TO_CURVE_SCALE_ONCE;
          }
        }
      }
    }
  }

  /* Scale to fit only works for single text box layouts. */
  if (ELEM(iter_data->status, VFONT_TO_CURVE_SCALE_ONCE, VFONT_TO_CURVE_BISECT)) {
    /* Always cleanup before going to the scale-to-fit repetition. */
    if (r_nubase != NULL) {
      BKE_nurbList_free(r_nubase);
    }

    if (chartransdata != NULL) {
      MEM_freeN(chartransdata);
    }

    if (ef == NULL) {
      MEM_freeN((void *)mem);
    }
    return true;
  }

  ok = true;
finally:
  if (r_text) {
    *r_text = mem;
    *r_text_len = slen;
    *r_text_free = (ef == NULL);
  }
  else {
    if (ef == NULL) {
      MEM_freeN((void *)mem);
    }
  }

  if (chartransdata) {
    if (ok && r_chartransdata) {
      *r_chartransdata = chartransdata;
    }
    else {
      MEM_freeN(chartransdata);
    }
  }

  /* Store the effective scale, to use for the text-box lines. */
  cu->fsize_realtime = font_size;

  return ok;

#undef MARGIN_X_MIN
#undef MARGIN_Y_MIN
}

#undef DESCENT
#undef ASCENT

bool BKE_vfont_to_curve_ex(Object *ob,
                           Curve *cu,
                           int mode,
                           ListBase *r_nubase,
                           const char32_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           struct CharTrans **r_chartransdata)
{
  VFontToCurveIter data = {
      .iteraction = cu->totbox * FONT_TO_CURVE_SCALE_ITERATIONS,
      .scale_to_fit = 1.0f,
      .word_wrap = true,
      .ok = true,
      .status = VFONT_TO_CURVE_INIT,
  };

  do {
    data.ok &= vfont_to_curve(
        ob, cu, mode, &data, r_nubase, r_text, r_text_len, r_text_free, r_chartransdata);
  } while (data.ok && ELEM(data.status, VFONT_TO_CURVE_SCALE_ONCE, VFONT_TO_CURVE_BISECT));

  return data.ok;
}

#undef FONT_TO_CURVE_SCALE_ITERATIONS
#undef FONT_TO_CURVE_SCALE_THRESHOLD

bool BKE_vfont_to_curve_nubase(Object *ob, int mode, ListBase *r_nubase)
{
  BLI_assert(ob->type == OB_FONT);

  return BKE_vfont_to_curve_ex(ob, ob->data, mode, r_nubase, NULL, NULL, NULL, NULL);
}

bool BKE_vfont_to_curve(Object *ob, int mode)
{
  Curve *cu = ob->data;

  return BKE_vfont_to_curve_ex(ob, ob->data, mode, &cu->nurb, NULL, NULL, NULL, NULL);
}

/* -------------------------------------------------------------------- */
/** \name VFont Clipboard
 * \{ */

static struct {
  char32_t *text_buffer;
  CharInfo *info_buffer;
  size_t len_utf32;
  size_t len_utf8;
} g_vfont_clipboard = {NULL};

void BKE_vfont_clipboard_free(void)
{
  MEM_SAFE_FREE(g_vfont_clipboard.text_buffer);
  MEM_SAFE_FREE(g_vfont_clipboard.info_buffer);
  g_vfont_clipboard.len_utf32 = 0;
  g_vfont_clipboard.len_utf8 = 0;
}

void BKE_vfont_clipboard_set(const char32_t *text_buf, const CharInfo *info_buf, const size_t len)
{
  char32_t *text;
  CharInfo *info;

  /* Clean previous buffers. */
  BKE_vfont_clipboard_free();

  text = MEM_malloc_arrayN((len + 1), sizeof(*text), __func__);
  if (text == NULL) {
    return;
  }

  info = MEM_malloc_arrayN(len, sizeof(CharInfo), __func__);
  if (info == NULL) {
    MEM_freeN(text);
    return;
  }

  memcpy(text, text_buf, len * sizeof(*text));
  text[len] = '\0';
  memcpy(info, info_buf, len * sizeof(CharInfo));

  /* store new buffers */
  g_vfont_clipboard.text_buffer = text;
  g_vfont_clipboard.info_buffer = info;
  g_vfont_clipboard.len_utf8 = BLI_str_utf32_as_utf8_len(text);
  g_vfont_clipboard.len_utf32 = len;
}

void BKE_vfont_clipboard_get(char32_t **r_text_buf,
                             CharInfo **r_info_buf,
                             size_t *r_len_utf8,
                             size_t *r_len_utf32)
{
  if (r_text_buf) {
    *r_text_buf = g_vfont_clipboard.text_buffer;
  }

  if (r_info_buf) {
    *r_info_buf = g_vfont_clipboard.info_buffer;
  }

  if (r_len_utf32) {
    *r_len_utf32 = g_vfont_clipboard.len_utf32;
  }

  if (r_len_utf8) {
    *r_len_utf8 = g_vfont_clipboard.len_utf8;
  }
}

/** \} */
