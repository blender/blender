/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Utilities relating to converting VFont's to curves
 * as well as 3D text object layout.
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwctype>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_base_safe.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"

#include "BKE_anim_path.h"
#include "BKE_curve.hh"
#include "BKE_object_types.hh"
#include "BKE_vfont.hh"
#include "BKE_vfontdata.hh"

/**
 * Locking on when manipulating the #VFont because multiple objects may share a VFont.
 * Depsgraph evaluation can evaluate multiple objects in different threads,
 * so any changes to the #VFont (such as glyph cache) must use locking.
 */
static ThreadRWMutex vfont_rwlock = BLI_RWLOCK_INITIALIZER;

/* -------------------------------------------------------------------- */
/** \name Private Utilities
 * \{ */

/**
 * Calculate the mid-point between two points and assign it to both of them.
 */
static void mid_v2v2(float a[2], float b[2])
{
  a[0] = b[0] = (a[0] * 0.5) + (b[0] * 0.5f);
  a[1] = b[1] = (a[1] * 0.5) + (b[1] * 0.5f);
}

static float vfont_metrics_ascent(const VFontData_Metrics *metrics)
{
  return metrics->ascend_ratio * metrics->em_ratio;
}
static float vfont_metrics_descent(const VFontData_Metrics *metrics)
{
  return metrics->em_ratio - vfont_metrics_ascent(metrics);
}

static VFont *vfont_from_charinfo(const Curve &cu, const CharInfo *info)
{
  switch (info->flag & (CU_CHINFO_BOLD | CU_CHINFO_ITALIC)) {
    case CU_CHINFO_BOLD:
      return cu.vfontb ? cu.vfontb : cu.vfont;
    case CU_CHINFO_ITALIC:
      return cu.vfonti ? cu.vfonti : cu.vfont;
    case (CU_CHINFO_BOLD | CU_CHINFO_ITALIC):
      return cu.vfontbi ? cu.vfontbi : cu.vfont;
    default:
      return cu.vfont;
  }
}

static VFontData *vfont_data_ensure_with_lock(VFont *vfont)
{
  if (vfont == nullptr) {
    return nullptr;
  }

  /* Lazily initialize the data. */
  if (!vfont->data) {

    BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_WRITE);

    if (vfont->data) {
      /* Check data again, since it might have been already initialized from other thread
       * (previous check is not accurate or threading,
       * just prevents unneeded lock if all the data is here for sure). */
      BLI_rw_mutex_unlock(&vfont_rwlock);
      return vfont->data;
    }

    BKE_vfont_data_ensure(vfont);

    BLI_rw_mutex_unlock(&vfont_rwlock);
  }

  return vfont->data;
}

static bool vfont_char_find(const VFontData *vfd, char32_t charcode, VChar **r_che)
{
  if (void **che_p = BLI_ghash_lookup_p(vfd->characters, POINTER_FROM_UINT(charcode))) {
    *r_che = static_cast<VChar *>(*che_p);
    return true;
  }
  *r_che = nullptr;
  return false;
}

/**
 * Find the character or lazily initialize it.
 *
 * The intended use-case for this function is that characters are initialized once.
 * Any future access can then use #vfont_char_find or #vfont_char_find_or_placeholder.
 */
static VChar *vfont_char_ensure_with_lock(VFont *vfont, char32_t charcode)
{
  VChar *che;
  if (vfont && vfont->data) {
    VFontData *vfd = vfont->data;
    BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_READ);
    bool che_found = vfont_char_find(vfd, charcode, &che);
    BLI_rw_mutex_unlock(&vfont_rwlock);

    /* The character wasn't in the current curve base so load it. */
    if (che_found == false) {
      BLI_rw_mutex_lock(&vfont_rwlock, THREAD_LOCK_WRITE);
      /* Check it once again, char might have been already load
       * between previous #BLI_rw_mutex_unlock() and this #BLI_rw_mutex_lock().
       *
       * Such a check should not be a bottleneck since it wouldn't
       * happen often once all the chars are load. */
      che_found = vfont_char_find(vfd, charcode, &che);
      if (che_found == false) {
        che = BKE_vfontdata_char_from_freetypefont(vfont, charcode);
      }
      BLI_rw_mutex_unlock(&vfont_rwlock);
    }
  }
  else {
    che = nullptr;
  }
  return che;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont to Curve: Character Placeholder
 *
 * Simple utility to create a dummy #VChar on demand which can be used
 * when the character's glyph isn't available.
 * \{ */

struct VCharPlaceHolder {
  /** Keep first, used for initializing. */
  const VFontData_Metrics *metrics = nullptr;

  bool initialized = false;

  /** Zeroed on initialization. */
  struct {
    /** The placeholder (blank & space). */
    VChar che[2] = {};

    /** Data for #VChar::nurbsbase. */
    Nurb nu[2] = {};
    /** Data for #Nurb::bezt. */
    BezTriple bezt[2][4] = {};
  } data;
};

/**
 * Return a "placeholder" character, used for the glyph not found symbol,
 * used when the font can't be loaded or it doesn't contain the requested glyph.
 */
static VChar *vfont_placeholder_ensure(VCharPlaceHolder &che_placeholder, char32_t charcode)
{
  const int che_index = (charcode == ' ') ? 0 : 1;

  if (!che_placeholder.initialized) {
    const VFontData_Metrics *metrics = che_placeholder.metrics;

    const float ascent = vfont_metrics_ascent(metrics);

    const float line_width = 0.05 * metrics->em_ratio;

    /* The rectangle size within the available bounds. */
    const blender::float2 size_factor = {
        0.9f,
        0.9f - (line_width * 2),
    };
    const float size_factor_margin_y = ((1.0 - size_factor.x) / 2.0f);

    /* Always initialize all placeholders, only if one is used. */
    VChar *che;

    /* Space - approximately en width. */
    che = &che_placeholder.data.che[0];
    che->width = 0.5f * metrics->em_ratio;

    /* Hollow rectangle - approximately en width. */
    che = &che_placeholder.data.che[1];
    che->width = 0.5f * metrics->em_ratio;

    for (int nu_index = 0; nu_index < ARRAY_SIZE(che_placeholder.data.nu); nu_index++) {
      Nurb *nu = &che_placeholder.data.nu[nu_index];
      BLI_addtail(&che->nurbsbase, nu);

      /* In this case poly makes more sense, follow the convention for others. */
      nu->type = CU_BEZIER;
      nu->resolu = 8;

      nu->bezt = &che_placeholder.data.bezt[nu_index][0];
      nu->pntsu = 4;
      nu->pntsv = 1;
      nu->flagu |= CU_NURB_CYCLIC;

      rctf bounds;
      bounds.xmin = (che->width * size_factor_margin_y);
      bounds.xmax = (che->width * 1.0 - size_factor_margin_y);
      bounds.ymin = 0.0f;
      bounds.ymax = ascent * size_factor.y;

      if (nu_index == 1) {
        bounds.xmin += line_width;
        bounds.xmax -= line_width;
        bounds.ymin += line_width;
        bounds.ymax -= line_width;
      }

      if (nu_index == 0) {
        ARRAY_SET_ITEMS(nu->bezt[0].vec[1], bounds.xmin, bounds.ymin);
        ARRAY_SET_ITEMS(nu->bezt[1].vec[1], bounds.xmin, bounds.ymax);
        ARRAY_SET_ITEMS(nu->bezt[2].vec[1], bounds.xmax, bounds.ymax);
        ARRAY_SET_ITEMS(nu->bezt[3].vec[1], bounds.xmax, bounds.ymin);
      }
      else {
        /* Holes are meant to use reverse winding, while not essential for Blender.
         * Do this for the sake of correctness. */
        ARRAY_SET_ITEMS(nu->bezt[3].vec[1], bounds.xmin, bounds.ymin);
        ARRAY_SET_ITEMS(nu->bezt[2].vec[1], bounds.xmin, bounds.ymax);
        ARRAY_SET_ITEMS(nu->bezt[1].vec[1], bounds.xmax, bounds.ymax);
        ARRAY_SET_ITEMS(nu->bezt[0].vec[1], bounds.xmax, bounds.ymin);
      }

      for (int bezt_index = 0; bezt_index < 4; bezt_index++) {
        BezTriple *bezt = &nu->bezt[bezt_index];
        bezt->radius = 1.0;
        bezt->h1 = HD_VECT;
        bezt->h2 = HD_VECT;
      }
    }
    che_placeholder.initialized = true;
  }
  return &che_placeholder.data.che[che_index];
}

/**
 * A version of #vfont_char_find that returns a place-holder if the glyph cannot be found.
 */
static VChar *vfont_char_find_or_placeholder(const VFontData *vfd,
                                             char32_t charcode,
                                             VCharPlaceHolder &che_placeholder)
{
  VChar *che = nullptr;
  if (vfd) {
    vfont_char_find(vfd, charcode, &che);
  }
  if (UNLIKELY(che == nullptr)) {
    che = vfont_placeholder_ensure(che_placeholder, charcode);
  }
  return che;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont Build Character
 * \{ */

/**
 * \param ul_prev_nu: The previous adjacent underline
 * which has it's right edge welded with this underlines left edge
 * to prevent gaps or overlapping geometry which can cause Z-fighting.
 *
 * \return The shape used for the underline which may be passed in
 * as the `ul_prev_nu` in future calls to this function.
 */
static Nurb *build_underline(const Curve &cu,
                             ListBase *nubase,
                             const rctf *rect,
                             const float yofs,
                             const float rotate,
                             const int charidx,
                             const short mat_nr,
                             const float font_size,
                             Nurb *ul_prev_nu)
{
  Nurb *nu;
  BPoint *bp;

  nu = MEM_callocN<Nurb>("underline_nurb");
  nu->resolu = cu.resolu;
  nu->bezt = nullptr;
  nu->knotsu = nu->knotsv = nullptr;
  nu->charidx = charidx + 1000;
  if (mat_nr >= 0) {
    nu->mat_nr = mat_nr;
  }
  nu->pntsu = 4;
  nu->pntsv = 1;
  nu->orderu = 4;
  nu->orderv = 1;
  nu->flagu = CU_NURB_CYCLIC;

  bp = MEM_calloc_arrayN<BPoint>(4, "underline_bp");

  copy_v4_fl4(bp[0].vec, rect->xmin, (rect->ymax + yofs), 0.0f, 1.0f);
  copy_v4_fl4(bp[1].vec, rect->xmax, (rect->ymax + yofs), 0.0f, 1.0f);
  copy_v4_fl4(bp[2].vec, rect->xmax, (rect->ymin + yofs), 0.0f, 1.0f);
  copy_v4_fl4(bp[3].vec, rect->xmin, (rect->ymin + yofs), 0.0f, 1.0f);

  /* Used by curve extrusion. */
  bp[0].radius = bp[1].radius = bp[2].radius = bp[3].radius = 1.0f;

  nu->bp = bp;
  BLI_addtail(nubase, nu);

  if (rotate != 0.0f) {
    float si = sinf(rotate);
    float co = cosf(rotate);

    for (int i = nu->pntsu; i > 0; i--) {
      float *fp = bp->vec;

      float x = fp[0] - rect->xmin;
      float y = fp[1] - rect->ymin;

      fp[0] = (+co * x + si * y) + rect->xmin;
      fp[1] = (-si * x + co * y) + rect->ymin;

      bp++;
    }

    bp = nu->bp;
  }

  mul_v2_fl(bp[0].vec, font_size);
  mul_v2_fl(bp[1].vec, font_size);
  mul_v2_fl(bp[2].vec, font_size);
  mul_v2_fl(bp[3].vec, font_size);

  if (ul_prev_nu) {
    /* Weld locations with the previous, adjacent underline. */
    BPoint *bp_prev = ul_prev_nu->bp;
    mid_v2v2(bp_prev[1].vec, bp[0].vec); /* Lower line. */
    mid_v2v2(bp_prev[2].vec, bp[3].vec); /* Upper line. */
  }

  return nu;
}

static void vfont_char_build_impl(const Curve &cu,
                                  ListBase *nubase,
                                  const VChar *che,
                                  const CharInfo *info,
                                  const bool is_smallcaps,
                                  const blender::float2 &offset,
                                  const float rotate,
                                  const int charidx,
                                  const float fsize)
{
  /* Make a copy at distance `offset` with shear. */
  float shear = cu.shear;
  float si = sinf(rotate);
  float co = cosf(rotate);

  /* Select the glyph data */
  const Nurb *nu_from_vchar = nullptr;
  if (che) {
    nu_from_vchar = static_cast<Nurb *>(che->nurbsbase.first);
  }

  /* Create the character. */
  while (nu_from_vchar) {
    const BezTriple *bezt_from_vchar = nu_from_vchar->bezt;
    if (bezt_from_vchar) {
      Nurb *nu = MEM_mallocN<Nurb>("duplichar_nurb");
      if (nu == nullptr) {
        break;
      }
      *nu = blender::dna::shallow_copy(*nu_from_vchar);
      nu->resolu = cu.resolu;
      nu->bp = nullptr;
      nu->knotsu = nu->knotsv = nullptr;
      nu->flag = CU_SMOOTH;
      nu->charidx = charidx;
      if (info->mat_nr > 0) {
        nu->mat_nr = info->mat_nr;
      }
      else {
        nu->mat_nr = 0;
      }
      int u = nu->pntsu;

      BezTriple *bezt = MEM_malloc_arrayN<BezTriple>(size_t(u), "duplichar_bezt2");
      if (bezt == nullptr) {
        MEM_freeN(nu);
        break;
      }
      memcpy(bezt, bezt_from_vchar, u * sizeof(BezTriple));
      nu->bezt = bezt;

      if (shear != 0.0f) {
        bezt = nu->bezt;

        for (int i = nu->pntsu; i > 0; i--) {
          bezt->vec[0][0] += shear * bezt->vec[0][1];
          bezt->vec[1][0] += shear * bezt->vec[1][1];
          bezt->vec[2][0] += shear * bezt->vec[2][1];
          bezt++;
        }
      }
      if (rotate != 0.0f) {
        bezt = nu->bezt;
        for (int i = nu->pntsu; i > 0; i--) {
          float *fp = bezt->vec[0];

          float x = fp[0];
          fp[0] = co * x + si * fp[1];
          fp[1] = -si * x + co * fp[1];
          x = fp[3];
          fp[3] = co * x + si * fp[4];
          fp[4] = -si * x + co * fp[4];
          x = fp[6];
          fp[6] = co * x + si * fp[7];
          fp[7] = -si * x + co * fp[7];

          bezt++;
        }
      }
      bezt = nu->bezt;

      if (is_smallcaps) {
        const float sca = cu.smallcaps_scale;
        for (int i = nu->pntsu; i > 0; i--) {
          float *fp = bezt->vec[0];
          fp[0] *= sca;
          fp[1] *= sca;
          fp[3] *= sca;
          fp[4] *= sca;
          fp[6] *= sca;
          fp[7] *= sca;
          bezt++;
        }
      }
      bezt = nu->bezt;

      for (int i = nu->pntsu; i > 0; i--) {
        float *fp = bezt->vec[0];
        fp[0] = (fp[0] + offset.x) * fsize;
        fp[1] = (fp[1] + offset.y) * fsize;
        fp[3] = (fp[3] + offset.x) * fsize;
        fp[4] = (fp[4] + offset.y) * fsize;
        fp[6] = (fp[6] + offset.x) * fsize;
        fp[7] = (fp[7] + offset.y) * fsize;
        bezt++;
      }

      BLI_addtail(nubase, nu);
    }

    nu_from_vchar = nu_from_vchar->next;
  }
}

void BKE_vfont_char_build(const Curve &cu,
                          ListBase *nubase,
                          uint charcode,
                          const CharInfo *info,
                          const bool is_smallcaps,
                          const blender::float2 &offset,
                          float rotate,
                          int charidx,
                          const float fsize)
{
  VFontData *vfd = vfont_data_ensure_with_lock(vfont_from_charinfo(cu, info));
  if (!vfd) {
    return;
  }
  VChar *che;
  vfont_char_find(vfd, charcode, &che);
  vfont_char_build_impl(cu, nubase, che, info, is_smallcaps, offset, rotate, charidx, fsize);
}

static float vfont_char_width(const Curve &cu, VChar *che, const bool is_smallcaps)
{
  /* The character wasn't found, probably `charcode = 0`, then the width shall be 0 as well. */
  if (che == nullptr) {
    return 0.0f;
  }
  if (is_smallcaps) {
    return che->width * cu.smallcaps_scale;
  }

  return che->width;
}

static char32_t vfont_char_apply_smallcaps(char32_t charcode, const bool is_smallcaps)
{
  if (UNLIKELY(is_smallcaps)) {
    return toupper(charcode);
  }
  return charcode;
}

static void textbox_scale(TextBox *tb_dst, const TextBox *tb_src, float scale)
{
  tb_dst->x = tb_src->x * scale;
  tb_dst->y = tb_src->y * scale;
  tb_dst->w = tb_src->w * scale;
  tb_dst->h = tb_src->h * scale;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont to Curve: Scale Overflow
 *
 * Scale the font to fit inside #TextBox bounds.
 *
 * - Scale horizontally when #TextBox.h is zero,
 *   otherwise scale vertically, allowing the text to wrap horizontally.
 * - Never increase scale to fit, only ever scale on overflow.
 * \{ */

struct VFontToCurveIter {
  int iteration;
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
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont to Curve: Mouse Cursor to Text Offset
 *
 * This is an optional argument to `vfont_to_curve` for getting the text
 * offset into the string at a mouse cursor location. Used for getting
 * text cursor (caret) position or selection range.
 * \{ */

/** Used when translating a mouse cursor location to a position within the string. */
struct VFontCursor_Params {
  /** Mouse cursor location in Object coordinate space as input. */
  blender::float2 cursor_location;
  /** Character position within #EditFont::textbuf as output. */
  int r_string_offset;
};

enum {
  VFONT_TO_CURVE_INIT = 0,
  VFONT_TO_CURVE_BISECT,
  VFONT_TO_CURVE_SCALE_ONCE,
  VFONT_TO_CURVE_DONE,
};

#define FONT_TO_CURVE_SCALE_ITERATIONS 20
#define FONT_TO_CURVE_SCALE_THRESHOLD 0.0001f

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont to Curve: Info Context
 * \{ */

struct VFontInfoContext {
  VFont *vfont;
  VFontData *vfd;
};

static void vfont_info_context_init(VFontInfoContext *vfinfo_ctx, const Curve &cu)
{
  BLI_assert(!vfinfo_ctx->vfont);
  BLI_assert(!vfinfo_ctx->vfd);

  vfinfo_ctx->vfont = cu.vfont;
  vfinfo_ctx->vfd = vfont_data_ensure_with_lock(vfinfo_ctx->vfont);
}

static void vfont_info_context_update(VFontInfoContext *vfinfo_ctx,
                                      const Curve &cu,
                                      const CharInfo *info)
{
  VFont *vfont = vfont_from_charinfo(cu, info);
  if (vfinfo_ctx->vfont != vfont) {
    vfinfo_ctx->vfont = vfont;
    vfinfo_ctx->vfd = vfont_data_ensure_with_lock(vfont);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont to Curve: 3D Text Layout Implementation
 * \{ */

/**
 * Track additional information when using the cursor to select with multiple text boxes.
 * This gives a more predictable result when the user moves the cursor outside the text-box.
 */
struct TextBoxBounds_ForCursor {
  /**
   * Describes the minimum rectangle that contains all characters in a text-box,
   * values are compatible with #TextBox.
   */
  rctf bounds;
  /**
   * The last character in this text box or -1 when unfilled.
   */
  int char_index_last;
};

/**
 * Used for storing per-line data for alignment & wrapping.
 */
struct TempLineInfo {
  /** Left margin. */
  float x_min;
  /** Right margin. */
  float x_max;
  /** Number of characters. */
  int char_nr;
  /** Number of white-spaces of line. */
  int wspace_nr;
};

/**
 * This function implements text layout & formatting
 * with font styles, text boxes as well as text cursor placement.
 */
static bool vfont_to_curve(Object *ob,
                           const Curve &cu,
                           const eEditFontMode mode,
                           VFontToCurveIter &iter_data,
                           VFontCursor_Params *cursor_params,
                           ListBase *r_nubase,
                           const char32_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           CharTrans **r_chartransdata,
                           float *r_font_size_eval)
{
  EditFont *ef = cu.editfont;
  EditFontSelBox *selboxes = nullptr;
  const CharInfo *info = nullptr, *custrinfo;
  TextBox tb_scale;
  VChar *che;
  CharTrans *chartransdata = nullptr, *ct;
  TempLineInfo *lineinfo;
  float xtrax, linedist;
  float twidth = 0;
  int i, slen, j;
  int curbox;
  /* These values are only set to the selection range when `selboxes` is non-null. */
  int selstart = 0, selend = 0;
  int cnr = 0, lnr = 0, wsnr = 0;
  const char32_t *mem = nullptr;
  bool mem_alloc = false;
  const float font_size = cu.fsize * iter_data.scale_to_fit;
  /* Shift down vertically to be 25% below & 75% above baseline (before font scale is applied). */
  const float font_select_y_offset = 0.25;
  const bool word_wrap = iter_data.word_wrap;
  const blender::float2 cu_offset_scale = {
      safe_divide(cu.xof, font_size),
      safe_divide(cu.yof, font_size),
  };
  int last_line = -1;
  /* Length of the text disregarding \n breaks. */
  float current_line_length = 0.0f;
  float longest_line_length = 0.0f;

  /* Text at the beginning of the last used text-box (use for y-axis alignment).
   * We over-allocate by one to simplify logic of getting last char. */
  blender::Array<int> i_textbox_array(cu.totbox + 1, 0);

#define MARGIN_X_MIN (cu_offset_scale.x + tb_scale.x)
#define MARGIN_Y_MIN (cu_offset_scale.y + tb_scale.y)

  /* NOTE: do calculations including the trailing `\0` of a string
   * because the cursor can be at that location. */

  BLI_assert(ob == nullptr || ob->type == OB_FONT);

  /* Read-file ensures non-null, must have become null at run-time, this is a bug! */
  if (UNLIKELY(!(cu.str && cu.tb && (ef ? ef->textbufinfo : cu.strinfo)))) {
    BLI_assert(0);
    return false;
  }

  /* Set font data */
  VFontInfoContext vfinfo_ctx = {nullptr};
  vfont_info_context_init(&vfinfo_ctx, cu);

  /* This must only be used for calculations which apply to all text,
   * for character level queries, values from `vfinfo_ctx` must be updated & used.
   * Note that this can be null. */
  VFontData_Metrics _vfont_metrics_default_buf;
  const VFontData_Metrics *metrics;
  if (vfinfo_ctx.vfd) {
    metrics = &vfinfo_ctx.vfd->metrics;
  }
  else {
    BKE_vfontdata_metrics_get_defaults(&_vfont_metrics_default_buf);
    metrics = &_vfont_metrics_default_buf;
  }

  VCharPlaceHolder che_placeholder = {
      /*metrics*/ metrics,
  };

  if (ef) {
    slen = ef->len;
    mem = ef->textbuf;
    custrinfo = ef->textbufinfo;
  }
  else {
    char32_t *mem_tmp;
    slen = cu.len_char32;

    /* Create unicode string. */
    mem_tmp = MEM_malloc_arrayN<char32_t>(size_t(slen) + 1, "convertedmem");
    if (!mem_tmp) {
      return false;
    }

    BLI_str_utf8_as_utf32(mem_tmp, cu.str, slen + 1);

    mem = mem_tmp;
    mem_alloc = true;
    custrinfo = cu.strinfo;
  }

  /* Only manipulate the edit-font if this object is in edit-mode, otherwise it's unnecessary
   * as well as crashing since manipulating the #EditFont here isn't thread-safe, see: #144970.
   *
   * NOTE(@ideasman42): Relying on the objects mode here isn't as fool-proof as I'd like,
   * however, even in cases where object data is shared between two different objects,
   * both active in different windows - it's not possible to enter edit on both at the same time.
   * If problems are found with this method, other checks could be investigated. */
  if (ef) {
    if (ob && (ob->mode & OB_MODE_EDIT)) {
      /* Pass. */
    }
    else {
      /* Other modes manipulate `ef->pos` which must only be done when this object is in edit-mode.
       * Not when a curve that happens to have edit-mode data is evaluated
       * (typically a linked duplicate). */
      BLI_assert(!FO_CURS_IS_MOTION(mode));

      /* Since all data has been accessed that's needed, set as null since it's
       * important never to manipulate this data from multiple threads at once. */
      ef = nullptr;
    }
  }

  if (ef != nullptr) {
    if (ef->selboxes) {
      MEM_freeN(ef->selboxes);
    }

    if (BKE_vfont_select_get(&cu, &selstart, &selend)) {
      ef->selboxes_len = (selend - selstart) + 1;
      ef->selboxes = MEM_calloc_arrayN<EditFontSelBox>(ef->selboxes_len, "font selboxes");
    }
    else {
      ef->selboxes_len = 0;
      ef->selboxes = nullptr;
    }

    selboxes = ef->selboxes;
  }

  /* Calculate the offset and rotation of each char. */
  ct = chartransdata = MEM_calloc_arrayN<CharTrans>(size_t(slen) + 1, "buildtext");

  /* We assume the worst case: 1 character per line (is freed at end anyway). */
  lineinfo = MEM_malloc_arrayN<TempLineInfo>(size_t(slen) * 2 + 1, "lineinfo");

  linedist = cu.linedist;

  curbox = 0;
  textbox_scale(&tb_scale, &cu.tb[curbox], safe_divide(1.0f, font_size));
  const bool use_textbox = (tb_scale.w != 0.0f);

  blender::float2 offset{
      MARGIN_X_MIN,
      MARGIN_Y_MIN,
  };
  xtrax = 0.5f * cu.spacing - 0.5f;

  TextBoxBounds_ForCursor *tb_bounds_for_cursor = nullptr;
  if (cursor_params != nullptr) {
    if (cu.textoncurve == nullptr && (cu.totbox > 1) && (slen > 0)) {
      tb_bounds_for_cursor = MEM_malloc_arrayN<TextBoxBounds_ForCursor>(size_t(cu.totbox),
                                                                        "TextboxBounds_Cursor");
      for (curbox = 0; curbox < cu.totbox; curbox++) {
        TextBoxBounds_ForCursor *tb_bounds = &tb_bounds_for_cursor[curbox];
        tb_bounds->char_index_last = -1;
        tb_bounds->bounds.xmin = FLT_MAX;
        tb_bounds->bounds.xmax = -FLT_MAX;
        tb_bounds->bounds.ymin = FLT_MAX;
        tb_bounds->bounds.ymax = -FLT_MAX;
      }
    }
    curbox = 0;
  }

  i = 0;
  while (i <= slen) {
    /* Characters in the list. */
    info = &custrinfo[i];
    char32_t charcode = mem[i];
    if (info->flag & CU_CHINFO_SMALLCAPS) {
      charcode = towupper(charcode);
      if (mem[i] != charcode) {
        BLI_assert(ct == &chartransdata[i]);
        ct->is_smallcaps = true;
      }
    }
    /* The #vfont_char_apply_smallcaps function can be used from now on. */

    vfont_info_context_update(&vfinfo_ctx, cu, info);

    if (!ELEM(charcode, '\n', '\0')) {
      che = vfont_char_ensure_with_lock(vfinfo_ctx.vfont, charcode);
      if (che == nullptr) {
        che = vfont_placeholder_ensure(che_placeholder, charcode);
      }
    }
    else {
      che = nullptr;
    }

    twidth = vfont_char_width(cu, che, ct->is_smallcaps);

    /* Calculate positions. */

    if ((tb_scale.w != 0.0f) && (ct->do_break == 0)) { /* May need wrapping. */
      const float x_available = cu_offset_scale.x + tb_scale.w;
      const float x_used = (offset.x - tb_scale.x) + twidth;

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
        bool do_break = false;
        for (j = i; (mem[j] != '\n') && (chartransdata[j].do_break == 0); j--) {

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
            offset.x = ct->offset.x;
            BLI_assert(&ct[1] == &chartransdata[i + 1]);
            ct[1].do_break = 1;
            ct[1].is_wrap = 1;
            do_break = true;
            break;
          }
          BLI_assert(chartransdata[j].do_break == 0);
        }

        if (do_break) {
          if (tb_scale.h == 0.0f) {
            /* NOTE: If underlined text is truncated away, the extra space is also truncated. */
            BLI_assert(&chartransdata[i + 1] == &ct[1]);
            ct[1].is_overflow = 1;
          }
          /* Since a break was added, re-run this loop with `i` at it's new value. */
          continue;
        }
      }
    }

    if (charcode == '\n' || charcode == 0 || ct->do_break) {
      ct->offset = offset;
      ct->linenr = lnr;
      ct->charnr = cnr;

      offset.y -= linedist;

      lineinfo[lnr].x_min = (offset.x - xtrax) - tb_scale.x;
      lineinfo[lnr].x_max = tb_scale.w;
      lineinfo[lnr].char_nr = cnr;
      lineinfo[lnr].wspace_nr = wsnr;

      if (tb_bounds_for_cursor != nullptr) {
        tb_bounds_for_cursor[curbox].char_index_last = i;
      }

      if ((tb_scale.h != 0.0f) &&
          (-(offset.y - tb_scale.y) > (tb_scale.h - linedist) - cu_offset_scale.y))
      {
        if (cu.totbox > (curbox + 1)) {
          curbox++;
          i_textbox_array[curbox] = i + 1;

          textbox_scale(&tb_scale, &cu.tb[curbox], 1.0f / font_size);

          offset.y = MARGIN_Y_MIN;
        }
        else if (last_line == -1) {
          last_line = lnr + 1;
          ct->is_overflow = 1;
        }
      }

      current_line_length += offset.x - MARGIN_X_MIN;
      if (ct->do_break) {
        current_line_length += twidth;
      }
      else {
        longest_line_length = std::max(current_line_length, longest_line_length);
        current_line_length = 0.0f;
      }

      offset.x = MARGIN_X_MIN;
      lnr++;
      cnr = 0;
      wsnr = 0;
    }
    else if (charcode == '\t') { /* Tab character. */
      float tabfac;

      ct->offset = offset;
      ct->linenr = lnr;
      ct->charnr = cnr++;

      tabfac = (offset.x - MARGIN_X_MIN + 0.01f);
      tabfac = 2.0f * ceilf(tabfac / 2.0f);
      offset.x = MARGIN_X_MIN + tabfac;
    }
    else {
      EditFontSelBox *sb = nullptr;
      float wsfac;

      ct->offset = offset;
      ct->linenr = lnr;
      ct->charnr = cnr++;

      if (selboxes && (i >= selstart) && (i <= selend)) {
        sb = &selboxes[i - selstart];
        sb->y = (offset.y - font_select_y_offset) * font_size - linedist * font_size * 0.1f;
        sb->h = linedist * font_size;
        sb->w = offset.x * font_size;
      }

      if (charcode == ' ') { /* Space character. */
        wsfac = cu.wordspace;
        wsnr++;
      }
      else {
        wsfac = 1.0f;
      }

      /* Set the width of the character. */
      twidth = vfont_char_width(cu, che, ct->is_smallcaps);

      offset.x += (twidth * wsfac * (1.0f + (info->kern / 40.0f))) + xtrax;

      if (sb) {
        sb->w = (offset.x * font_size) - sb->w;
      }
    }
    ct++;
    i++;
  }

  current_line_length += offset.x + twidth - MARGIN_X_MIN;
  longest_line_length = std::max(current_line_length, longest_line_length);

  if (ef && selboxes) {
    /* Set combined style flags for the selected string. Start with all styles then
     * remove one if ANY characters do not have it. Break out if we've removed them all. */
    ef->select_char_info_flag = CU_CHINFO_STYLE_ALL;
    for (int k = selstart; k <= selend && ef->select_char_info_flag; k++) {
      info = &custrinfo[k];
      ef->select_char_info_flag &= info->flag;
    }
  }

  if (cu.spacemode != CU_ALIGN_X_LEFT) {
    ct = chartransdata;

    if (cu.spacemode == CU_ALIGN_X_RIGHT) {
      TempLineInfo *li;

      for (i = 0, li = lineinfo; i < lnr; i++, li++) {
        li->x_min = (li->x_max - li->x_min) + cu_offset_scale.x;
      }

      for (i = 0; i <= slen; i++) {
        ct->offset.x += lineinfo[ct->linenr].x_min;
        ct++;
      }
    }
    else if (cu.spacemode == CU_ALIGN_X_MIDDLE) {
      TempLineInfo *li;

      for (i = 0, li = lineinfo; i < lnr; i++, li++) {
        li->x_min = ((li->x_max - li->x_min) + cu_offset_scale.x) / 2.0f;
      }

      for (i = 0; i <= slen; i++) {
        ct->offset.x += lineinfo[ct->linenr].x_min;
        ct++;
      }
    }
    else if ((cu.spacemode == CU_ALIGN_X_FLUSH) && use_textbox) {
      TempLineInfo *li;

      for (i = 0, li = lineinfo; i < lnr; i++, li++) {
        li->x_min = ((li->x_max - li->x_min) + cu_offset_scale.x);

        if (li->char_nr > 1) {
          li->x_min /= float(li->char_nr - 1);
        }
      }
      for (i = 0; i <= slen; i++) {
        for (j = i; !ELEM(mem[j], '\0', '\n') && (chartransdata[j].do_break == 0) && (j < slen);
             j++)
        {
          /* Pass. */
        }

        // if ((mem[j] != '\n') && (mem[j])) {
        ct->offset.x += ct->charnr * lineinfo[ct->linenr].x_min;
        // }
        ct++;
      }
    }
    else if ((cu.spacemode == CU_ALIGN_X_JUSTIFY) && use_textbox) {
      float curofs = 0.0f;
      for (i = 0; i <= slen; i++) {
        for (j = i; (mem[j]) && (mem[j] != '\n') && (chartransdata[j].do_break == 0) && (j < slen);
             j++)
        {
          /* Pass. */
        }

        if ((mem[j] != '\n') && (chartransdata[j].do_break != 0)) {
          if (mem[i] == ' ') {
            TempLineInfo *li;

            li = &lineinfo[ct->linenr];
            curofs += ((li->x_max - li->x_min) + cu_offset_scale.x) / float(li->wspace_nr);
          }
          ct->offset.x += curofs;
        }
        if (mem[i] == '\n' || chartransdata[i].do_break) {
          curofs = 0;
        }
        ct++;
      }
    }
  }

  /* Top-baseline is default, in this case, do nothing. */
  if (cu.align_y != CU_ALIGN_Y_TOP_BASELINE) {
    if (tb_scale.h != 0.0f) {
      /* We need to loop all the text-boxes even the "full" ones.
       * This way they all get the same vertical padding. */
      for (int tb_index = 0; tb_index < cu.totbox; tb_index++) {
        CharTrans *ct_first, *ct_last;
        const int i_textbox = i_textbox_array[tb_index];
        const int i_textbox_next = i_textbox_array[tb_index + 1];
        const bool is_last_filled_textbox = ELEM(i_textbox_next, 0, slen + 1);
        int lines;

        ct_first = chartransdata + i_textbox;
        ct_last = chartransdata + (is_last_filled_textbox ? slen : i_textbox_next - 1);
        lines = ct_last->linenr - ct_first->linenr + 1;

        if (cu.overflow == CU_OVERFLOW_TRUNCATE) {
          /* Ensure overflow doesn't truncate text, before centering vertically
           * giving odd/buggy results, see: #66614. */
          if ((tb_index == cu.totbox - 1) && (last_line != -1)) {
            lines = last_line - ct_first->linenr;
          }
        }

        textbox_scale(&tb_scale, &cu.tb[tb_index], 1.0f / font_size);
        /* The initial Y origin of the text-box is hard-coded to 1.0f * text scale. */
        const float textbox_y_origin = 1.0f;
        float yoff = 0.0f;

        switch (cu.align_y) {
          case CU_ALIGN_Y_TOP_BASELINE:
            break;
          case CU_ALIGN_Y_TOP:
            yoff = textbox_y_origin - vfont_metrics_ascent(metrics);
            break;
          case CU_ALIGN_Y_CENTER:
            yoff = ((((metrics->em_ratio + (lines - 1) * linedist) * 0.5f) -
                     vfont_metrics_ascent(metrics)) -
                    (tb_scale.h * 0.5f) + textbox_y_origin);
            break;
          case CU_ALIGN_Y_BOTTOM_BASELINE:
            yoff = textbox_y_origin + ((lines - 1) * linedist) - tb_scale.h;
            break;
          case CU_ALIGN_Y_BOTTOM:
            yoff = textbox_y_origin + ((lines - 1) * linedist) - tb_scale.h +
                   vfont_metrics_descent(metrics);
            break;
        }

        for (ct = ct_first; ct <= ct_last; ct++) {
          ct->offset.y += yoff;
        }

        if (is_last_filled_textbox) {
          break;
        }
      }
    }
    else {
      /* Non text-box case handled separately. */
      float yoff = 0.0f;

      switch (cu.align_y) {
        case CU_ALIGN_Y_TOP_BASELINE:
          break;
        case CU_ALIGN_Y_TOP:
          yoff = -vfont_metrics_ascent(metrics);
          break;
        case CU_ALIGN_Y_CENTER:
          yoff = ((metrics->em_ratio + (lnr - 1) * linedist) * 0.5f) -
                 vfont_metrics_ascent(metrics);
          break;
        case CU_ALIGN_Y_BOTTOM_BASELINE:
          yoff = (lnr - 1) * linedist;
          break;
        case CU_ALIGN_Y_BOTTOM:
          yoff = (lnr - 1) * linedist + vfont_metrics_descent(metrics);
          break;
      }

      ct = chartransdata;
      for (i = 0; i <= slen; i++) {
        ct->offset.y += yoff;
        ct++;
      }
    }
  }
  if (tb_bounds_for_cursor != nullptr) {
    int char_beg_next = 0;
    for (curbox = 0; curbox < cu.totbox; curbox++) {
      TextBoxBounds_ForCursor *tb_bounds = &tb_bounds_for_cursor[curbox];
      if (tb_bounds->char_index_last == -1) {
        continue;
      }
      const int char_beg = char_beg_next;
      const int char_end = tb_bounds->char_index_last;

      TempLineInfo *line_beg = &lineinfo[chartransdata[char_beg].linenr];
      TempLineInfo *line_end = &lineinfo[chartransdata[char_end].linenr];

      int char_idx_offset = char_beg;

      rctf *bounds = &tb_bounds->bounds;
      /* In a text-box with no curves, `offset.y` only decrements over lines, `ymax` and `ymin`
       * can be obtained from any character in the first and last line of the text-box. */
      bounds->ymax = chartransdata[char_beg].offset.y;
      bounds->ymin = chartransdata[char_end].offset.y;

      for (TempLineInfo *line = line_beg; line <= line_end; line++) {
        const CharTrans *first_char_line = &chartransdata[char_idx_offset];
        const CharTrans *last_char_line = &chartransdata[char_idx_offset + line->char_nr];

        bounds->xmin = min_ff(bounds->xmin, first_char_line->offset.x);
        bounds->xmax = max_ff(bounds->xmax, last_char_line->offset.x);
        char_idx_offset += line->char_nr + 1;
      }
      /* Move the bounds into a space compatible with `cursor_location`. */
      BLI_rctf_mul(bounds, font_size);

      char_beg_next = tb_bounds->char_index_last + 1;
    }
  }

  MEM_freeN(lineinfo);

  /* TEXT ON CURVE */
  /* NOTE: Only #OB_CURVES_LEGACY objects could have a path. */
  if (cu.textoncurve && cu.textoncurve->type == OB_CURVES_LEGACY) {
    BLI_assert(cu.textoncurve->runtime->curve_cache != nullptr);
    if (cu.textoncurve->runtime->curve_cache != nullptr &&
        cu.textoncurve->runtime->curve_cache->anim_path_accum_length != nullptr)
    {
      float distfac, imat[4][4], imat3[3][3], cmat[3][3];
      float minx, maxx;
      float timeofs, sizefac;

      if (ob != nullptr) {
        invert_m4_m4(imat, ob->object_to_world().ptr());
      }
      else {
        unit_m4(imat);
      }
      copy_m3_m4(imat3, imat);

      copy_m3_m4(cmat, cu.textoncurve->object_to_world().ptr());
      mul_m3_m3m3(cmat, cmat, imat3);
      sizefac = normalize_v3(cmat[0]) / font_size;

      ct = chartransdata;
      minx = maxx = ct->offset.x;
      ct++;
      for (i = 1; i <= slen; i++, ct++) {
        minx = std::min(minx, ct->offset.x);
        maxx = std::max(maxx, ct->offset.x);
      }

      /* We put the x-coordinate exact at the curve, the y is rotated. */

      /* Length correction. */
      const float chartrans_size_x = maxx - minx;
      if (chartrans_size_x != 0.0f) {
        const CurveCache *cc = cu.textoncurve->runtime->curve_cache;
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

        if (cu.spacemode == CU_ALIGN_X_RIGHT) {
          timeofs = 1.0f - distfac;
        }
        else if (cu.spacemode == CU_ALIGN_X_MIDDLE) {
          timeofs = (1.0f - distfac) / 2.0f;
        }
        else if (cu.spacemode == CU_ALIGN_X_FLUSH) {
          distfac = 1.0f;
        }
      }

      if (chartrans_size_x != 0.0f) {
        distfac /= chartrans_size_x;
      }

      timeofs += distfac * cu.xof; /* Not cyclic. */

      ct = chartransdata;
      for (i = 0; i <= slen; i++, ct++) {
        float ctime, dtime, vec[4], rotvec[3];
        float si, co;

        /* Rotate around center character. */
        info = &custrinfo[i];
        BLI_assert(ct == &chartransdata[i]);
        const char32_t charcode = vfont_char_apply_smallcaps(mem[i], ct->is_smallcaps);

        vfont_info_context_update(&vfinfo_ctx, cu, info);
        che = vfont_char_find_or_placeholder(vfinfo_ctx.vfd, charcode, che_placeholder);

        twidth = vfont_char_width(cu, che, ct->is_smallcaps);

        dtime = distfac * 0.5f * twidth;

        ctime = timeofs + distfac * (ct->offset.x - minx);
        CLAMP(ctime, 0.0f, 1.0f);

        /* Calculate the right loc AND the right rot separately. */
        BKE_where_on_path(cu.textoncurve, ctime, vec, nullptr, nullptr, nullptr, nullptr);
        BKE_where_on_path(
            cu.textoncurve, ctime + dtime, nullptr, rotvec, nullptr, nullptr, nullptr);

        mul_v3_fl(vec, sizefac);

        ct->rotate = float(M_PI) - atan2f(rotvec[1], rotvec[0]);

        si = sinf(ct->rotate);
        co = cosf(ct->rotate);

        offset.y = ct->offset.y;

        ct->offset = {
            vec[0] + si * offset.y,
            vec[1] + co * offset.y,
        };

        if (selboxes && (i >= selstart) && (i <= selend)) {
          EditFontSelBox *sb;
          sb = &selboxes[i - selstart];
          sb->rotate = -ct->rotate;
        }
      }
    }
  }

  if (selboxes) {
    ct = chartransdata;
    for (i = 0; i <= selend; i++, ct++) {
      if (i >= selstart) {
        EditFontSelBox *sb = &selboxes[i - selstart];
        sb->x = ct->offset.x;
        sb->y = ct->offset.y;
        if (ct->rotate != 0.0f) {
          sb->x -= sinf(ct->rotate) * font_select_y_offset;
          sb->y -= cosf(ct->rotate) * font_select_y_offset;
        }
        else {
          /* Simple downward shift below baseline when not rotated. */
          sb->y -= font_select_y_offset;
        }
        sb->x *= font_size;
        sb->y *= font_size;
        selboxes[i - selstart].h = font_size;
      }
    }
  }

  if (ELEM(mode, FO_CURSUP, FO_CURSDOWN, FO_PAGEUP, FO_PAGEDOWN, FO_LINE_BEGIN, FO_LINE_END) &&
      iter_data.status == VFONT_TO_CURVE_INIT)
  {
    ct = &chartransdata[ef->pos];

    if (ELEM(mode, FO_CURSUP, FO_PAGEUP) && ct->linenr == 0) {
      /* Pass. */
    }
    else if (ELEM(mode, FO_CURSDOWN, FO_PAGEDOWN) && ct->linenr == lnr) {
      /* Pass. */
    }
    else if (mode == FO_LINE_BEGIN) {
      /* Line wrap aware line beginning. */
      while ((ef->pos > 0) && (chartransdata[ef->pos - 1].linenr == ct->linenr)) {
        ef->pos -= 1;
      }
    }
    else if (mode == FO_LINE_END) {
      /* Line wrap aware line end. */
      while ((ef->pos < slen) && (chartransdata[ef->pos + 1].linenr == ct->linenr)) {
        ef->pos += 1;
      }
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
          /* Ignored. */
        case FO_EDIT:
        case FO_CURS:
        case FO_DUPLI:
        case FO_SELCHANGE:
        case FO_LINE_BEGIN:
        case FO_LINE_END:
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
    ct = &chartransdata[ef->pos];
    const float cursor_width = 0.04f;
    const float cursor_half = 0.02f;

    /* By default the cursor is exactly between the characters
     * and matches the rotation of the character to the right. */
    float cursor_left = 0.0f - cursor_half;
    float cursor_rotate = ct->rotate;

    if (ef->selboxes) {
      if (ef->selend >= ef->selstart) {
        /* Cursor at right edge of a text selection. Match rotation to the character at the
         * end of selection. Cursor is further right to show the selected characters better. */
        cursor_rotate = chartransdata[max_ii(0, ef->selend - 1)].rotate;
        cursor_left = 0.0f;
      }
      else {
        /* Cursor at the left edge of a text selection. Cursor
         * is further left to show the selected characters better. */
        cursor_left = 0.0f - cursor_width;
      }
    }
    else if ((ef->pos == ef->len) && (ef->len > 0)) {
      /* Nothing selected, but at the end of the string. Match rotation to previous character. */
      cursor_rotate = chartransdata[ef->len - 1].rotate;
    }

    /* We need the rotation to be around the bottom-left corner. So we make
     * that the zero point before rotation, rotate, then apply offsets afterward. */

    /* Bottom left. */
    ef->textcurs[0] = blender::float2(cursor_left, 0.0f - font_select_y_offset);
    /* Bottom right. */
    ef->textcurs[1] = blender::float2(cursor_left + cursor_width, 0.0f - font_select_y_offset);
    /* Top left. */
    ef->textcurs[3] = blender::float2(cursor_left, 1.0f - font_select_y_offset);
    /* Top right. */
    ef->textcurs[2] = blender::float2(cursor_left + cursor_width, 1.0f - font_select_y_offset);

    for (int vert = 0; vert < 4; vert++) {
      blender::float2 temp_fl;
      /* Rotate around the cursor's bottom-left corner. */
      rotate_v2_v2fl(temp_fl, &ef->textcurs[vert][0], -cursor_rotate);
      ef->textcurs[vert] = font_size * (ct->offset + temp_fl);
    }
  }

  if (mode == FO_SELCHANGE) {
    MEM_freeN(chartransdata);
    chartransdata = nullptr;
  }
  else if (mode == FO_EDIT) {
    /* Make NURBS-data. */
    BKE_nurbList_free(r_nubase);

    /* Track the previous underline so contiguous underlines can be welded together.
     * This is done to prevent overlapping geometry, see: #122540. */
    int ul_prev_i = -1;
    Nurb *ul_prev_nu = nullptr;

    ct = chartransdata;
    for (i = 0; i < slen; i++) {

      if ((cu.overflow == CU_OVERFLOW_TRUNCATE) && (ob && ob->mode != OB_MODE_EDIT) &&
          ct->is_overflow)
      {
        break;
      }

      info = &(custrinfo[i]);
      const char32_t charcode = vfont_char_apply_smallcaps(mem[i], ct->is_smallcaps);
      /* We don't want to see any character for `\n`. */
      if (charcode != '\n') {

        vfont_info_context_update(&vfinfo_ctx, cu, info);
        /* Find the character, the characters has to be in the memory already
         * since character checking has been done earlier already. */
        che = vfont_char_find_or_placeholder(vfinfo_ctx.vfd, charcode, che_placeholder);
        vfont_char_build_impl(
            cu, r_nubase, che, info, ct->is_smallcaps, ct->offset, ct->rotate, i, font_size);

        if (info->flag & CU_CHINFO_UNDERLINE) {
          float ulwidth, uloverlap = 0.0f;
          rctf rect;

          BLI_assert(&ct[1] == &chartransdata[i + 1]);
          if ((i < (slen - 1)) && (mem[i + 1] != '\n') &&
              ((mem[i + 1] != ' ') || (custrinfo[i + 1].flag & CU_CHINFO_UNDERLINE)) &&
              ((ct[1].is_wrap) == 0))
          {
            uloverlap = xtrax;
          }

          twidth = vfont_char_width(cu, che, ct->is_smallcaps);
          ulwidth = (twidth * (1.0f + (info->kern / 40.0f))) + uloverlap;

          rect.xmin = ct->offset.x;
          rect.xmax = rect.xmin + ulwidth;

          rect.ymin = ct->offset.y;
          rect.ymax = rect.ymin - cu.ulheight;

          if ((ul_prev_i != -1) &&
              /* Skip welding underlines when there are gaps. */
              ((ul_prev_i + 1 != i) ||
               /* Skip welding on new lines. */
               (chartransdata[ul_prev_i].linenr != ct->linenr)))
          {
            ul_prev_nu = nullptr;
          }

          ul_prev_nu = build_underline(cu,
                                       r_nubase,
                                       &rect,
                                       cu.ulpos - 0.05f,
                                       ct->rotate,
                                       i,
                                       info->mat_nr,
                                       font_size,
                                       ul_prev_nu);
          ul_prev_i = ul_prev_nu ? i : -1;
        }
      }
      ct++;
    }
  }

  if (iter_data.status == VFONT_TO_CURVE_SCALE_ONCE) {
    /* That means we were in a final run, just exit. */
    BLI_assert(cu.overflow == CU_OVERFLOW_SCALE);
    iter_data.status = VFONT_TO_CURVE_DONE;
  }
  else if (cu.overflow == CU_OVERFLOW_NONE) {
    /* Pass. */
  }
  else if ((tb_scale.h == 0.0f) && (tb_scale.w == 0.0f)) {
    /* Pass. */
  }
  else if (cu.overflow == CU_OVERFLOW_SCALE) {
    if ((cu.totbox == 1) && ((tb_scale.w == 0.0f) || (tb_scale.h == 0.0f))) {
      /* These are special cases, simpler to deal with. */
      if (tb_scale.w == 0.0f) {
        /* This is a potential vertical overflow.
         * Since there is no width limit, all the new lines are from line breaks. */
        if ((last_line != -1) && (lnr > last_line)) {
          const float total_text_height = lnr * linedist;
          iter_data.scale_to_fit = tb_scale.h / total_text_height;
          iter_data.status = VFONT_TO_CURVE_SCALE_ONCE;
          iter_data.word_wrap = false;
        }
      }
      else if (tb_scale.h == 0.0f) {
        /* This is a horizontal overflow. */
        if (longest_line_length > tb_scale.w) {
          /* We make sure longest line before it broke can fit here. */
          float scale_to_fit = tb_scale.w / longest_line_length;

          iter_data.scale_to_fit = scale_to_fit;
          iter_data.status = VFONT_TO_CURVE_SCALE_ONCE;
          iter_data.word_wrap = false;
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
      if (iter_data.status == VFONT_TO_CURVE_INIT) {
        bool valid = true;

        for (int tb_index = 0; tb_index <= curbox; tb_index++) {
          TextBox *tb = &cu.tb[tb_index];
          if ((tb->w == 0.0f) || (tb->h == 0.0f)) {
            valid = false;
            break;
          }
        }

        if (valid && (last_line != -1) && (lnr > last_line)) {
          const float total_text_height = lnr * linedist;
          float scale_to_fit = tb_scale.h / total_text_height;

          iter_data.bisect.max = 1.0f;
          iter_data.bisect.min = scale_to_fit;

          iter_data.status = VFONT_TO_CURVE_BISECT;
        }
      }
      else {
        BLI_assert(iter_data.status == VFONT_TO_CURVE_BISECT);
        /* Try to get the highest scale that gives us the exactly
         * number of lines we need. */
        bool valid = false;

        if ((last_line != -1) && (lnr > last_line)) {
          /* It is overflowing, scale it down. */
          iter_data.bisect.max = iter_data.scale_to_fit;
        }
        else {
          /* It fits inside the text-box, scale it up. */
          iter_data.bisect.min = iter_data.scale_to_fit;
          valid = true;
        }

        /* Bisecting to try to find the best fit. */
        iter_data.scale_to_fit = (iter_data.bisect.max + iter_data.bisect.min) * 0.5f;

        /* We iterated enough or got a good enough result. */
        if ((!iter_data.iteration--) || ((iter_data.bisect.max - iter_data.bisect.min) <
                                         (cu.fsize * FONT_TO_CURVE_SCALE_THRESHOLD)))
        {
          if (valid) {
            iter_data.status = VFONT_TO_CURVE_DONE;
          }
          else {
            iter_data.scale_to_fit = iter_data.bisect.min;
            iter_data.status = VFONT_TO_CURVE_SCALE_ONCE;
          }
        }
      }
    }
  }

  if (cursor_params) {
    const blender::float2 &cursor_location = cursor_params->cursor_location;
    /* Erasing all text could give `slen = 0`. */
    if (slen == 0) {
      cursor_params->r_string_offset = -1;
    }
    else if (cu.textoncurve != nullptr) {

      int closest_char = -1;
      float closest_dist_sq = FLT_MAX;

      for (i = 0; i <= slen; i++) {
        const blender::float2 char_location = chartransdata[i].offset * font_size;
        const float test_dist_sq = blender::math::distance_squared(cursor_location, char_location);
        if (closest_dist_sq > test_dist_sq) {
          closest_char = i;
          closest_dist_sq = test_dist_sq;
        }
      }

      cursor_params->r_string_offset = closest_char;
    }
    else {
      /* Find the first box closest to `cursor_location`. */
      int char_beg = 0;
      int char_end = slen;

      if (tb_bounds_for_cursor != nullptr) {
        /* Search for the closest box. */
        int closest_box = -1;
        float closest_dist_sq = FLT_MAX;
        for (curbox = 0; curbox < cu.totbox; curbox++) {
          const TextBoxBounds_ForCursor *tb_bounds = &tb_bounds_for_cursor[curbox];
          if (tb_bounds->char_index_last == -1) {
            continue;
          }
          /* The closest point in the box to the `cursor_location`
           * by clamping it to the bounding box. */
          const blender::float2 cursor_location_clamped = {
              clamp_f(cursor_location.x, tb_bounds->bounds.xmin, tb_bounds->bounds.xmax),
              clamp_f(cursor_location.y, tb_bounds->bounds.ymin, tb_bounds->bounds.ymax),
          };

          const float test_dist_sq = blender::math::distance_squared(cursor_location,
                                                                     cursor_location_clamped);
          if (test_dist_sq < closest_dist_sq) {
            closest_dist_sq = test_dist_sq;
            closest_box = curbox;
          }
        }
        if (closest_box != -1) {
          if (closest_box != 0) {
            char_beg = tb_bounds_for_cursor[closest_box - 1].char_index_last + 1;
          }
          char_end = tb_bounds_for_cursor[closest_box].char_index_last;
        }
        MEM_freeN(tb_bounds_for_cursor);
        tb_bounds_for_cursor = nullptr; /* Safety only. */
      }
      const float interline_offset = ((linedist - 0.5f) / 2.0f) * font_size;
      /* Loop until find the line where `cursor_location` is over. */
      for (i = char_beg; i <= char_end; i++) {
        if (cursor_location.y >= ((chartransdata[i].offset.y * font_size) - interline_offset)) {
          break;
        }
      }

      i = min_ii(i, char_end);
      const float char_yof = chartransdata[i].offset.y;

      /* Loop back until find the first character of the line, this because `cursor_location` can
       * be positioned further below the text, so #i can be the last character of the last line. */
      for (; i >= char_beg + 1 && chartransdata[i - 1].offset.y == char_yof; i--) {
        /* Pass. */
      }
      /* Loop until find the first character to the right of `cursor_location`
       * (using the character midpoint on the x-axis as a reference). */
      for (; i <= char_end && char_yof == chartransdata[i].offset.y; i++) {
        info = &custrinfo[i];
        const char32_t charcode = vfont_char_apply_smallcaps(mem[i], info);

        vfont_info_context_update(&vfinfo_ctx, cu, info);
        che = vfont_char_find_or_placeholder(vfinfo_ctx.vfd, charcode, che_placeholder);

        const float charwidth = vfont_char_width(cu, che, info);
        const float charhalf = (charwidth / 2.0f);
        if (cursor_location.x <= ((chartransdata[i].offset.x + charhalf) * font_size)) {
          break;
        }
      }
      i = min_ii(i, char_end);

      /* If there is no character to the right of the cursor we are on the next line, go back to
       * the last character of the previous line. */
      if (i > char_beg && chartransdata[i].offset.y != char_yof) {
        i -= 1;
      }
      cursor_params->r_string_offset = i;
    }
    /* Must be cleared & freed. */
    BLI_assert(tb_bounds_for_cursor == nullptr);
  }

  /* Scale to fit only works for single text box layouts. */
  if (ELEM(iter_data.status, VFONT_TO_CURVE_SCALE_ONCE, VFONT_TO_CURVE_BISECT)) {
    /* Always cleanup before going to the scale-to-fit repetition. */
    if (r_nubase != nullptr) {
      BKE_nurbList_free(r_nubase);
    }

    if (chartransdata != nullptr) {
      MEM_freeN(chartransdata);
    }

    if (mem_alloc) {
      MEM_freeN(mem);
    }
    return true;
  }

  if (r_text) {
    *r_text = mem;
    *r_text_len = slen;
    *r_text_free = mem_alloc;
  }
  else {
    if (mem_alloc) {
      MEM_freeN(mem);
    }
  }

  if (chartransdata) {
    if (r_chartransdata) {
      *r_chartransdata = chartransdata;
    }
    else {
      MEM_freeN(chartransdata);
    }
  }

  /* Store the effective scale, to use for the text-box lines. */
  if (ef != nullptr) {
    ef->font_size_eval = font_size;
  }
  if (r_font_size_eval) {
    *r_font_size_eval = font_size;
  }
  return true;

#undef MARGIN_X_MIN
#undef MARGIN_Y_MIN
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont to Curve Public API
 *
 * Access to 3D text layout.
 * \{ */

bool BKE_vfont_to_curve_ex(Object *ob,
                           const Curve &cu,
                           const eEditFontMode mode,
                           ListBase *r_nubase,
                           const char32_t **r_text,
                           int *r_text_len,
                           bool *r_text_free,
                           CharTrans **r_chartransdata,
                           float *r_font_size_eval)
{
  VFontToCurveIter data = {};
  data.iteration = cu.totbox * FONT_TO_CURVE_SCALE_ITERATIONS;
  data.scale_to_fit = 1.0f;
  data.word_wrap = true;
  data.ok = true;
  data.status = VFONT_TO_CURVE_INIT;

  do {
    data.ok &= vfont_to_curve(ob,
                              cu,
                              mode,
                              data,
                              nullptr,
                              r_nubase,
                              r_text,
                              r_text_len,
                              r_text_free,
                              r_chartransdata,
                              r_font_size_eval);
  } while (data.ok && ELEM(data.status, VFONT_TO_CURVE_SCALE_ONCE, VFONT_TO_CURVE_BISECT));

  return data.ok;
}

int BKE_vfont_cursor_to_text_index(Object *ob, const blender::float2 &cursor_location)
{
  Curve &cu = *(Curve *)ob->data;
  ListBase *r_nubase = &cu.nurb;

  /* TODO: iterating to calculate the scale can be avoided. */
  VFontToCurveIter data = {};
  data.iteration = cu.totbox * FONT_TO_CURVE_SCALE_ITERATIONS;
  data.scale_to_fit = 1.0f;
  data.word_wrap = true;
  data.ok = true;
  data.status = VFONT_TO_CURVE_INIT;

  VFontCursor_Params cursor_params = {};
  cursor_params.cursor_location = cursor_location;
  cursor_params.r_string_offset = -1;

  do {
    data.ok &= vfont_to_curve(ob,
                              cu,
                              FO_CURS,
                              data,
                              &cursor_params,
                              r_nubase,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr);
  } while (data.ok && ELEM(data.status, VFONT_TO_CURVE_SCALE_ONCE, VFONT_TO_CURVE_BISECT));

  return cursor_params.r_string_offset;
}

#undef FONT_TO_CURVE_SCALE_ITERATIONS
#undef FONT_TO_CURVE_SCALE_THRESHOLD

bool BKE_vfont_to_curve_nubase(Object *ob, const eEditFontMode mode, ListBase *r_nubase)
{
  BLI_assert(ob->type == OB_FONT);
  const Curve &cu = *static_cast<const Curve *>(ob->data);
  return BKE_vfont_to_curve_ex(
      ob, cu, mode, r_nubase, nullptr, nullptr, nullptr, nullptr, nullptr);
}

bool BKE_vfont_to_curve(Object *ob, const eEditFontMode mode)
{
  Curve &cu = *static_cast<Curve *>(ob->data);
  return BKE_vfont_to_curve_ex(
      ob, cu, mode, &cu.nurb, nullptr, nullptr, nullptr, nullptr, nullptr);
}

/** \} */
