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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_vfontdata.h"

#include "DNA_packedFile_types.h"
#include "DNA_curve_types.h"
#include "DNA_vfont_types.h"
#include "DNA_object_types.h"

#include "BKE_packedFile.h"
#include "BKE_library.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_anim.h"
#include "BKE_curve.h"

static CLG_LogRef LOG = {"bke.data_transfer"};
static ThreadRWMutex vfont_rwlock = BLI_RWLOCK_INITIALIZER;

/* The vfont code */
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
    freePackedFile(vfont->temp_pf); /* NULL when the font file can't be found on disk */
    vfont->temp_pf = NULL;
  }
}

/** Free (or release) any data used by this font (does not free the font itself). */
void BKE_vfont_free(struct VFont *vf)
{
  BKE_vfont_free_data(vf);

  if (vf->packedfile) {
    freePackedFile(vf->packedfile);
    vf->packedfile = NULL;
  }
}

void BKE_vfont_copy_data(Main *UNUSED(bmain),
                         VFont *vfont_dst,
                         const VFont *UNUSED(vfont_src),
                         const int flag)
{
  /* We never handle usercount here for own data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  /* Just to be sure, should not have any value actually after reading time. */
  vfont_dst->temp_pf = NULL;

  if (vfont_dst->packedfile) {
    vfont_dst->packedfile = dupPackedFile(vfont_dst->packedfile);
  }

  if (vfont_dst->data) {
    vfont_dst->data = BLI_vfontdata_copy(vfont_dst->data, flag_subdata);
  }
}

static void *builtin_font_data = NULL;
static int builtin_font_size = 0;

bool BKE_vfont_is_builtin(struct VFont *vfont)
{
  return STREQ(vfont->name, FO_BUILTIN_NAME);
}

void BKE_vfont_builtin_register(void *mem, int size)
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
  else {
    void *mem = MEM_mallocN(builtin_font_size, "vfd_builtin");

    memcpy(mem, builtin_font_data, builtin_font_size);

    return newPackedFileMemory(mem, builtin_font_size);
  }
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
          vfont->temp_pf = dupPackedFile(pf);
        }
      }
      else {
        pf = newPackedFile(NULL, vfont->name, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));

        if (vfont->temp_pf == NULL) {
          vfont->temp_pf = newPackedFile(NULL, vfont->name, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));
        }
      }
      if (!pf) {
        CLOG_WARN(&LOG, "Font file doesn't exist: %s", vfont->name);

        /* DON'T DO THIS
         * missing file shouldn't modify path! - campbell */
#if 0
        strcpy(vfont->name, FO_BUILTIN_NAME);
#endif
        pf = get_builtin_packedfile();
      }
    }

    if (pf) {
      vfont->data = BLI_vfontdata_from_freetypefont(pf);
      if (pf != vfont->packedfile) {
        freePackedFile(pf);
      }
    }

    BLI_rw_mutex_unlock(&vfont_rwlock);
  }

  return vfont->data;
}

/* Bad naming actually in this case... */
void BKE_vfont_init(VFont *vfont)
{
  PackedFile *pf = get_builtin_packedfile();

  if (pf) {
    VFontData *vfd;

    vfd = BLI_vfontdata_from_freetypefont(pf);
    if (vfd) {
      vfont->data = vfd;

      BLI_strncpy(vfont->name, FO_BUILTIN_NAME, sizeof(vfont->name));
    }

    /* Free the packed file */
    freePackedFile(pf);
  }
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
    pf = newPackedFile(NULL, filepath, BKE_main_blendfile_path(bmain));

    is_builtin = false;
  }

  if (pf) {
    VFontData *vfd;

    vfd = BLI_vfontdata_from_freetypefont(pf);
    if (vfd) {
      vfont = BKE_libblock_alloc(bmain, ID_VF, filename, 0);
      vfont->data = vfd;

      /* if there's a font name, use it for the ID name */
      if (vfd->name[0] != '\0') {
        BLI_strncpy(vfont->id.name + 2, vfd->name, sizeof(vfont->id.name) - 2);
      }
      BLI_strncpy(vfont->name, filepath, sizeof(vfont->name));

      /* if autopack is on store the packedfile in de font structure */
      if (!is_builtin && (G.fileflags & G_FILE_AUTOPACK)) {
        vfont->packedfile = pf;
      }

      /* Do not add FO_BUILTIN_NAME to temporary listbase */
      if (!STREQ(filename, FO_BUILTIN_NAME)) {
        vfont->temp_pf = newPackedFile(NULL, filepath, BKE_main_blendfile_path(bmain));
      }
    }

    /* Free the packed file */
    if (!vfont || vfont->packedfile != pf) {
      freePackedFile(pf);
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
    BLI_strncpy(strtest, vfont->name, sizeof(vfont->name));
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

void BKE_vfont_make_local(Main *bmain, VFont *vfont, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &vfont->id, true, lib_local);
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

static VChar *find_vfont_char(VFontData *vfd, unsigned int character)
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
  nu2->flag = CU_2D;
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

  nu2->bp = bp;
  BLI_addtail(nubase, nu2);

  if (rot != 0.0f) {
    float si, co;
    int i;

    si = sinf(rot);
    co = cosf(rot);

    for (i = nu2->pntsu; i > 0; i--) {
      float *fp;
      float x, y;

      fp = bp->vec;

      x = fp[0] - rect->xmin;
      y = fp[1] - rect->ymin;

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

static void buildchar(Curve *cu,
                      ListBase *nubase,
                      unsigned int character,
                      CharInfo *info,
                      float ofsx,
                      float ofsy,
                      float rot,
                      int charidx,
                      const float fsize)
{
  BezTriple *bezt1, *bezt2;
  Nurb *nu1 = NULL, *nu2 = NULL;
  float *fp, shear, x, si, co;
  VFontData *vfd = NULL;
  VChar *che = NULL;
  int i;

  vfd = vfont_get_data(which_vfont(cu, info));
  if (!vfd) {
    return;
  }

  /* make a copy at distance ofsx, ofsy with shear */
  shear = cu->shear;
  si = sinf(rot);
  co = cosf(rot);

  che = find_vfont_char(vfd, character);

  /* Select the glyph data */
  if (che) {
    nu1 = che->nurbsbase.first;
  }

  /* Create the character */
  while (nu1) {
    bezt1 = nu1->bezt;
    if (bezt1) {
      nu2 = (Nurb *)MEM_mallocN(sizeof(Nurb), "duplichar_nurb");
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
      i = nu2->pntsu;

      bezt2 = (BezTriple *)MEM_malloc_arrayN(i, sizeof(BezTriple), "duplichar_bezt2");
      if (bezt2 == NULL) {
        MEM_freeN(nu2);
        break;
      }
      memcpy(bezt2, bezt1, i * sizeof(struct BezTriple));
      nu2->bezt = bezt2;

      if (shear != 0.0f) {
        bezt2 = nu2->bezt;

        for (i = nu2->pntsu; i > 0; i--) {
          bezt2->vec[0][0] += shear * bezt2->vec[0][1];
          bezt2->vec[1][0] += shear * bezt2->vec[1][1];
          bezt2->vec[2][0] += shear * bezt2->vec[2][1];
          bezt2++;
        }
      }
      if (rot != 0.0f) {
        bezt2 = nu2->bezt;
        for (i = nu2->pntsu; i > 0; i--) {
          fp = bezt2->vec[0];

          x = fp[0];
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
        for (i = nu2->pntsu; i > 0; i--) {
          fp = bezt2->vec[0];
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

      for (i = nu2->pntsu; i > 0; i--) {
        fp = bezt2->vec[0];
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
  else {
    BLI_assert(start < end + 1);
    *r_start = start;
    *r_end = end;
    return direction;
  }
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
  else if (info->flag & CU_CHINFO_SMALLCAPS_CHECK) {
    return che->width * cu->smallcaps_scale;
  }
  else {
    return che->width;
  }
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
  int wspace_nr; /* number of whitespaces of line */
};

typedef struct VFontToCurveIter {
  int iteraction;
  float scale_to_fit;
  struct {
    float min;
    float max;
  } bisect;
  bool ok;
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
#define ASCENT(vfd) ((vfd)->ascender * (vfd)->em_height)
#define DESCENT(vfd) ((vfd)->em_height - ASCENT(vfd))

static bool vfont_to_curve(Object *ob,
                           Curve *cu,
                           int mode,
                           VFontToCurveIter *iter_data,
                           ListBase *r_nubase,
                           const wchar_t **r_text,
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
  int selstart, selend;
  int cnr = 0, lnr = 0, wsnr = 0;
  const wchar_t *mem = NULL;
  wchar_t ascii;
  bool ok = false;
  const float font_size = cu->fsize * iter_data->scale_to_fit;
  const float xof_scale = cu->xof / font_size;
  const float yof_scale = cu->yof / font_size;
  int last_line = -1;
  /* Length of the text disregarding \n breaks. */
  float current_line_length = 0.0f;
  float longest_line_length = 0.0f;

  /* Text at the beginning of the last used text-box (use for y-axis alignment).
   * We overallocate by one to simplify logic of getting last char. */
  int *i_textbox_array = MEM_callocN(sizeof(*i_textbox_array) * (cu->totbox + 1),
                                     "TextBox initial char index");

#define MARGIN_X_MIN (xof_scale + tb_scale.x)
#define MARGIN_Y_MIN (yof_scale + tb_scale.y)

  /* remark: do calculations including the trailing '\0' of a string
   * because the cursor can be at that location */

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
    wchar_t *mem_tmp;
    slen = cu->len_wchar;

    /* Create unicode string */
    mem_tmp = MEM_malloc_arrayN((slen + 1), sizeof(wchar_t), "convertedmem");
    if (!mem_tmp) {
      return ok;
    }

    BLI_strncpy_wchar_from_utf8(mem_tmp, cu->str, slen + 1);

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
  textbox_scale(&tb_scale, &cu->tb[curbox], 1.0f / font_size);
  use_textbox = (tb_scale.w != 0.0f);

  xof = MARGIN_X_MIN;
  yof = MARGIN_Y_MIN;

  xtrax = 0.5f * cu->spacing - 0.5f;

  oldvfont = NULL;

  for (i = 0; i < slen; i++) {
    custrinfo[i].flag &= ~(CU_CHINFO_WRAP | CU_CHINFO_SMALLCAPS_CHECK | CU_CHINFO_OVERFLOW);
  }

  for (i = 0; i <= slen; i++) {
  makebreak:
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

      /*
       * The character wasn't in the current curve base so load it
       * But if the font is built-in then do not try loading since
       * whole font is in the memory already
       */
      if (che == NULL && BKE_vfont_is_builtin(vfont) == false) {
        BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_WRITE);
        /* Check it once again, char might have been already load
         * between previous BLI_rw_mutex_unlock() and this BLI_rw_mutex_lock().
         *
         * Such a check should not be a bottleneck since it wouldn't
         * happen often once all the chars are load.
         */
        if ((che = find_vfont_char(vfd, ascii)) == NULL) {
          che = BLI_vfontchar_from_freetypefont(vfont, ascii);
        }
        BLI_rw_mutex_unlock(&vfont_rwlock);
      }
    }
    else {
      che = NULL;
    }

    twidth = char_width(cu, che, info);

    /* Calculate positions */
    if ((tb_scale.w != 0.0f) && (ct->dobreak == 0) &&
        (((xof - tb_scale.x) + twidth) > xof_scale + tb_scale.w)) {
      //      CLOG_WARN(&LOG, "linewidth exceeded: %c%c%c...", mem[i], mem[i+1], mem[i+2]);
      for (j = i; j && (mem[j] != '\n') && (chartransdata[j].dobreak == 0); j--) {
        bool dobreak = false;
        if (mem[j] == ' ' || mem[j] == '-') {
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
        }
        else if (chartransdata[j].dobreak) {
          //              CLOG_WARN(&LOG, "word too long: %c%c%c...", mem[j], mem[j+1], mem[j+2]);
          ct->dobreak = 1;
          custrinfo[i + 1].flag |= CU_CHINFO_WRAP;
          ct -= 1;
          cnr -= 1;
          i--;
          xof = ct->xof;
          dobreak = true;
        }
        if (dobreak) {
          if (tb_scale.h == 0.0f) {
            /* Note: If underlined text is truncated away, the extra space is also truncated. */
            custrinfo[i + 1].flag |= CU_CHINFO_OVERFLOW;
          }
          goto makebreak;
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

      if ((tb_scale.h != 0.0f) && ((-(yof - tb_scale.y)) > (tb_scale.h - linedist) - yof_scale)) {
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

      current_line_length += xof;
      if (ct->dobreak) {
        current_line_length += twidth;
      }
      else {
        longest_line_length = MAX2(current_line_length, longest_line_length);
        current_line_length = 0.0f;
      }

      /* XXX, has been unused for years, need to check if this is useful, r4613 r5282 - campbell */
#if 0
      if (ascii == '\n') {
        xof = xof_scale;
      }
      else {
        xof = MARGIN_X_MIN;
      }
#else
      xof = MARGIN_X_MIN;
#endif
      lnr++;
      cnr = 0;
      wsnr = 0;
    }
    else if (ascii == 9) { /* TAB */
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

      if (ascii == 32) {
        wsfac = cu->wordspace;
        wsnr++;
      }
      else {
        wsfac = 1.0f;
      }

      /* Set the width of the character */
      twidth = char_width(cu, che, info);

      xof += (twidth * wsfac * (1.0f + (info->kern / 40.0f))) + xtrax;

      if (sb) {
        sb->w = (xof * font_size) - sb->w;
      }
    }
    ct++;
  }
  current_line_length += xof + twidth;
  longest_line_length = MAX2(current_line_length, longest_line_length);

  cu->lines = 1;
  for (i = 0; i <= slen; i++) {
    ascii = mem[i];
    ct = &chartransdata[i];
    if (ascii == '\n' || ct->dobreak) {
      cu->lines++;
    }
  }

  /* linedata is now: width of line */

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
        for (j = i; (!ELEM(mem[j], '\0', '\n')) && (chartransdata[j].dobreak == 0) && (j < slen);
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

        if ((mem[j] != '\n') && ((chartransdata[j].dobreak != 0))) {
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

  /* top-baseline is default, in this case, do nothing */
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

        textbox_scale(&tb_scale, &cu->tb[tb_index], 1.0f / font_size);
        /* The initial Y origin of the textbox is hardcoded to 1.0f * text scale. */
        const float textbox_y_origin = 1.0f;
        float yoff = 0.0f;

        switch (cu->align_y) {
          case CU_ALIGN_Y_TOP_BASELINE:
            break;
          case CU_ALIGN_Y_TOP:
            yoff = textbox_y_origin - ASCENT(vfd);
            break;
          case CU_ALIGN_Y_CENTER:
            yoff = ((((vfd->em_height + (lines - 1) * linedist) * 0.5f) - ASCENT(vfd)) -
                    (tb_scale.h * 0.5f) + textbox_y_origin);
            break;
          case CU_ALIGN_Y_BOTTOM_BASELINE:
            yoff = textbox_y_origin + ((lines - 1) * linedist) - tb_scale.h;
            break;
          case CU_ALIGN_Y_BOTTOM:
            yoff = textbox_y_origin + ((lines - 1) * linedist) - tb_scale.h + DESCENT(vfd);
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
          yoff = -ASCENT(vfd);
          break;
        case CU_ALIGN_Y_CENTER:
          yoff = ((vfd->em_height + (lnr - 1) * linedist) * 0.5f) - ASCENT(vfd);
          break;
        case CU_ALIGN_Y_BOTTOM_BASELINE:
          yoff = (lnr - 1) * linedist;
          break;
        case CU_ALIGN_Y_BOTTOM:
          yoff = (lnr - 1) * linedist + DESCENT(vfd);
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
  /* Note: Only OB_CURVE objects could have a path  */
  if (cu->textoncurve && cu->textoncurve->type == OB_CURVE) {
    BLI_assert(cu->textoncurve->runtime.curve_cache != NULL);
    if (cu->textoncurve->runtime.curve_cache != NULL &&
        cu->textoncurve->runtime.curve_cache->path != NULL) {
      float distfac, imat[4][4], imat3[3][3], cmat[3][3];
      float minx, maxx, miny, maxy;
      float timeofs, sizefac;

      if (ob != NULL) {
        invert_m4_m4(imat, ob->obmat);
      }
      else {
        unit_m4(imat);
      }
      copy_m3_m4(imat3, imat);

      copy_m3_m4(cmat, cu->textoncurve->obmat);
      mul_m3_m3m3(cmat, cmat, imat3);
      sizefac = normalize_v3(cmat[0]) / font_size;

      minx = miny = 1.0e20f;
      maxx = maxy = -1.0e20f;
      ct = chartransdata;
      for (i = 0; i <= slen; i++, ct++) {
        if (minx > ct->xof) {
          minx = ct->xof;
        }
        if (maxx < ct->xof) {
          maxx = ct->xof;
        }
        if (miny > ct->yof) {
          miny = ct->yof;
        }
        if (maxy < ct->yof) {
          maxy = ct->yof;
        }
      }

      /* we put the x-coordinaat exact at the curve, the y is rotated */

      /* length correction */
      distfac = sizefac * cu->textoncurve->runtime.curve_cache->path->totdist / (maxx - minx);
      timeofs = 0.0f;

      if (distfac > 1.0f) {
        /* path longer than text: spacemode involves */
        distfac = 1.0f / distfac;

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
      else {
        distfac = 1.0;
      }

      distfac /= (maxx - minx);

      timeofs += distfac * cu->xof; /* not cyclic */

      ct = chartransdata;
      for (i = 0; i <= slen; i++, ct++) {
        float ctime, dtime, vec[4], tvec[4], rotvec[3];
        float si, co;

        /* rotate around center character */
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

        /* calc the right loc AND the right rot separately */
        /* vec, tvec need 4 items */
        where_on_path(cu->textoncurve, ctime, vec, tvec, NULL, NULL, NULL);
        where_on_path(cu->textoncurve, ctime + dtime, tvec, rotvec, NULL, NULL, NULL);

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
        selboxes[i - selstart].y = ct->yof * font_size;
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
      /* seek for char with lnr en cnr */
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

  /* cursor first */
  if (ef) {
    float si, co;

    ct = &chartransdata[ef->pos];
    si = sinf(ct->rot);
    co = cosf(ct->rot);

    f = ef->textcurs[0];

    f[0] = font_size * (-0.1f * co + ct->xof);
    f[1] = font_size * (0.1f * si + ct->yof);

    f[2] = font_size * (0.1f * co + ct->xof);
    f[3] = font_size * (-0.1f * si + ct->yof);

    f[4] = font_size * (0.1f * co + 0.8f * si + ct->xof);
    f[5] = font_size * (-0.1f * si + 0.8f * co + ct->yof);

    f[6] = font_size * (-0.1f * co + 0.8f * si + ct->xof);
    f[7] = font_size * (0.1f * si + 0.8f * co + ct->yof);
  }

  if (mode == FO_SELCHANGE) {
    MEM_freeN(chartransdata);
    chartransdata = NULL;
  }
  else if (mode == FO_EDIT) {
    /* make nurbdata */
    BKE_nurbList_free(r_nubase);

    ct = chartransdata;
    for (i = 0; i < slen; i++) {
      unsigned int cha = (unsigned int)mem[i];
      info = &(custrinfo[i]);

      if ((cu->overflow == CU_OVERFLOW_TRUNCATE) && (ob && ob->mode != OB_MODE_EDIT) &&
          (info->flag & CU_CHINFO_OVERFLOW)) {
        break;
      }

      if (info->flag & CU_CHINFO_SMALLCAPS_CHECK) {
        cha = towupper(cha);
      }

      if (ob == NULL || info->mat_nr > (ob->totcol)) {
        // CLOG_ERROR(
        //     &LOG, "Illegal material index (%d) in text object, setting to 0", info->mat_nr);
        info->mat_nr = 0;
      }
      /* We do not want to see any character for \n or \r */
      if (cha != '\n') {
        buildchar(cu, r_nubase, cha, info, ct->xof, ct->yof, ct->rot, i, font_size);
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
        }
      }
      else if (tb_scale.h == 0.0f) {
        /* This is a horizontal overflow. */
        if (lnr > 1) {
          /* We make sure longest line before it broke can fit here. */
          float scale_to_fit = tb_scale.w / (longest_line_length);
          scale_to_fit -= FLT_EPSILON;

          iter_data->scale_to_fit = scale_to_fit;
          iter_data->status = VFONT_TO_CURVE_SCALE_ONCE;
        }
      }
    }
    else {
      /* This is the really complicated case, the best we can do is to iterate over
       * this function a few times until we get an acceptable result.
       *
       * Keep in mind that there is no single number that will make all fit to the end.
       * In a way, our ultimate goal is to get the highest scale that still leads to the
       * number of extra lines to zero.
       */
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
          /* It fits inside the textbox, scale it up. */
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
  else {
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

    /* Store the effective scale, to use for the textbox lines. */
    cu->fsize_realtime = font_size;
  }
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
                           const wchar_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           struct CharTrans **r_chartransdata)
{
  VFontToCurveIter data = {
      .iteraction = cu->totbox * FONT_TO_CURVE_SCALE_ITERATIONS,
      .scale_to_fit = 1.0f,
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

/** Warning: expects to have access to evaluated data
 * (i.e. passed object should be evaluated one...). */
bool BKE_vfont_to_curve(Object *ob, int mode)
{
  Curve *cu = ob->data;

  return BKE_vfont_to_curve_ex(ob, ob->data, mode, &cu->nurb, NULL, NULL, NULL, NULL);
}

/* -------------------------------------------------------------------- */
/** \name VFont Clipboard
 * \{ */

static struct {
  wchar_t *text_buffer;
  CharInfo *info_buffer;
  size_t len_wchar;
  size_t len_utf8;
} g_vfont_clipboard = {NULL};

void BKE_vfont_clipboard_free(void)
{
  MEM_SAFE_FREE(g_vfont_clipboard.text_buffer);
  MEM_SAFE_FREE(g_vfont_clipboard.info_buffer);
  g_vfont_clipboard.len_wchar = 0;
  g_vfont_clipboard.len_utf8 = 0;
}

void BKE_vfont_clipboard_set(const wchar_t *text_buf, const CharInfo *info_buf, const size_t len)
{
  wchar_t *text;
  CharInfo *info;

  /* clean previous buffers*/
  BKE_vfont_clipboard_free();

  text = MEM_malloc_arrayN((len + 1), sizeof(wchar_t), __func__);
  if (text == NULL) {
    return;
  }

  info = MEM_malloc_arrayN(len, sizeof(CharInfo), __func__);
  if (info == NULL) {
    MEM_freeN(text);
    return;
  }

  memcpy(text, text_buf, len * sizeof(wchar_t));
  text[len] = '\0';
  memcpy(info, info_buf, len * sizeof(CharInfo));

  /* store new buffers */
  g_vfont_clipboard.text_buffer = text;
  g_vfont_clipboard.info_buffer = info;
  g_vfont_clipboard.len_utf8 = BLI_wstrlen_utf8(text);
  g_vfont_clipboard.len_wchar = len;
}

void BKE_vfont_clipboard_get(wchar_t **r_text_buf,
                             CharInfo **r_info_buf,
                             size_t *r_len_utf8,
                             size_t *r_len_wchar)
{
  if (r_text_buf) {
    *r_text_buf = g_vfont_clipboard.text_buffer;
  }

  if (r_info_buf) {
    *r_info_buf = g_vfont_clipboard.info_buffer;
  }

  if (r_len_wchar) {
    *r_len_wchar = g_vfont_clipboard.len_wchar;
  }

  if (r_len_utf8) {
    *r_len_utf8 = g_vfont_clipboard.len_utf8;
  }
}

/** \} */
