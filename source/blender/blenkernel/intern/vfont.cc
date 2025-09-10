/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <optional>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_vfont_types.h"

#include "BKE_bpath.hh"
#include "BKE_curve.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.hh"
#include "BKE_vfont.hh"
#include "BKE_vfontdata.hh"

#include "BLO_read_write.hh"

static CLG_LogRef LOG = {"geom.vfont"};

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static PackedFile *packedfile_new_from_builtin();

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont Data-block
 * \{ */

const void *builtin_font_data = nullptr;
int builtin_font_size = 0;

static void vfont_init_data(ID *id)
{
  VFont *vfont = (VFont *)id;
  PackedFile *pf = packedfile_new_from_builtin();

  if (pf) {
    VFontData *vfd;

    vfd = BKE_vfontdata_from_freetypefont(pf);
    if (vfd) {
      vfont->data = vfd;

      STRNCPY(vfont->filepath, FO_BUILTIN_NAME);
    }

    /* Free the packed file */
    BKE_packedfile_free(pf);
  }
}

static void vfont_copy_data(Main * /*bmain*/,
                            std::optional<Library *> /*owner_library*/,
                            ID *id_dst,
                            const ID * /*id_src*/,
                            const int flag)
{
  VFont *vfont_dst = (VFont *)id_dst;

  /* We never handle user-count here for own data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  /* Just to be sure, should not have any value actually after reading time. */
  vfont_dst->temp_pf = nullptr;

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
  BKE_vfont_data_free(vfont);

  if (vfont->packedfile) {
    BKE_packedfile_free(vfont->packedfile);
    vfont->packedfile = nullptr;
  }
}

static void vfont_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  VFont *vfont = (VFont *)id;

  if ((vfont->packedfile != nullptr) &&
      (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0)
  {
    return;
  }

  if (BKE_vfont_is_builtin(vfont)) {
    return;
  }

  BKE_bpath_foreach_path_fixed_process(bpath_data, vfont->filepath, sizeof(vfont->filepath));
}

static void vfont_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  VFont *vf = (VFont *)id;
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  vf->data = nullptr;
  vf->temp_pf = nullptr;

  /* Do not store packed files in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(vf) && !is_undo) {
    vf->packedfile = nullptr;
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
  vf->data = nullptr;
  vf->temp_pf = nullptr;
  BKE_packedfile_blend_read(reader, &vf->packedfile, vf->filepath);
}

IDTypeInfo IDType_ID_VF = {
    /*id_code*/ VFont::id_type,
    /*id_filter*/ FILTER_ID_VF,
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_VF,
    /*struct_size*/ sizeof(VFont),
    /*name*/ "Font",
    /*name_plural*/ N_("fonts"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_VFONT,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ vfont_init_data,
    /*copy_data*/ vfont_copy_data,
    /*free_data*/ vfont_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ vfont_foreach_path,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ vfont_blend_write,
    /*blend_read_data*/ vfont_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont
 * \{ */

void BKE_vfont_data_ensure(VFont *vfont)
{
  PackedFile *pf;
  if (BKE_vfont_is_builtin(vfont)) {
    pf = packedfile_new_from_builtin();
  }
  else {
    if (vfont->packedfile) {
      pf = vfont->packedfile;

      /* We need to copy a temporary font to memory unless it is already there. */
      if (vfont->temp_pf == nullptr) {
        vfont->temp_pf = BKE_packedfile_duplicate(pf);
      }
    }
    else {
      pf = BKE_packedfile_new(nullptr, vfont->filepath, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));

      if (vfont->temp_pf == nullptr) {
        vfont->temp_pf = BKE_packedfile_new(
            nullptr, vfont->filepath, ID_BLEND_PATH_FROM_GLOBAL(&vfont->id));
      }
    }
    if (!pf) {
      CLOG_WARN(&LOG, "Font file doesn't exist: %s", vfont->filepath);

      /* NOTE(@ideasman42): Don't attempt to find a fallback.
       * If the font requested by the user doesn't load, font rendering will display
       * placeholder characters instead. */
    }
  }

  if (pf) {
    vfont->data = BKE_vfontdata_from_freetypefont(pf);
    if (pf != vfont->packedfile) {
      BKE_packedfile_free(pf);
    }
  }
}

void BKE_vfont_data_free(VFont *vfont)
{
  if (vfont->data) {
    if (vfont->data->characters) {
      GHashIterator gh_iter;
      GHASH_ITER (gh_iter, vfont->data->characters) {
        VChar *che = static_cast<VChar *>(BLI_ghashIterator_getValue(&gh_iter));
        if (che == nullptr) {
          continue;
        }

        while (che->nurbsbase.first) {
          Nurb *nu = static_cast<Nurb *>(che->nurbsbase.first);
          if (nu->bezt) {
            MEM_freeN(nu->bezt);
          }
          BLI_freelinkN(&che->nurbsbase, nu);
        }

        MEM_freeN(che);
      }

      BLI_ghash_free(vfont->data->characters, nullptr, nullptr);
    }

    MEM_freeN(vfont->data);
    vfont->data = nullptr;
  }

  if (vfont->temp_pf) {
    BKE_packedfile_free(vfont->temp_pf); /* Null when the font file can't be found on disk. */
    vfont->temp_pf = nullptr;
  }
}

bool BKE_vfont_is_builtin(const VFont *vfont)
{
  return STREQ(vfont->filepath, FO_BUILTIN_NAME);
}

void BKE_vfont_builtin_register(const void *mem, int size)
{
  builtin_font_data = mem;
  builtin_font_size = size;
}

static PackedFile *packedfile_new_from_builtin()
{
  if (!builtin_font_data) {
    CLOG_ERROR(&LOG, "Internal error, builtin font not loaded");

    return nullptr;
  }

  void *mem = MEM_mallocN(builtin_font_size, "vfd_builtin");

  memcpy(mem, builtin_font_data, builtin_font_size);

  return BKE_packedfile_new_from_memory(mem, builtin_font_size);
}

VFont *BKE_vfont_load(Main *bmain, const char *filepath)
{
  char filename[FILE_MAXFILE];
  VFont *vfont = nullptr;
  PackedFile *pf;
  bool is_builtin;

  if (STREQ(filepath, FO_BUILTIN_NAME)) {
    STRNCPY(filename, filepath);

    pf = packedfile_new_from_builtin();
    is_builtin = true;
  }
  else {
    BLI_path_split_file_part(filepath, filename, sizeof(filename));
    pf = BKE_packedfile_new(nullptr, filepath, BKE_main_blendfile_path(bmain));

    is_builtin = false;
  }

  if (pf) {
    VFontData *vfd;

    vfd = BKE_vfontdata_from_freetypefont(pf);
    if (vfd) {
      /* If there's a font name, use it for the ID name. */
      vfont = static_cast<VFont *>(
          BKE_libblock_alloc(bmain, ID_VF, vfd->name[0] ? vfd->name : filename, 0));
      vfont->data = vfd;
      STRNCPY(vfont->filepath, filepath);

      /* if auto-pack is on store the packed-file in de font structure */
      if (!is_builtin && (G.fileflags & G_FILE_AUTOPACK)) {
        vfont->packedfile = pf;
      }

      /* Do not add #FO_BUILTIN_NAME to temporary list-base. */
      if (!STREQ(filename, FO_BUILTIN_NAME)) {
        vfont->temp_pf = BKE_packedfile_new(nullptr, filepath, BKE_main_blendfile_path(bmain));
      }
    }

    /* Free the packed file */
    if (!vfont || vfont->packedfile != pf) {
      BKE_packedfile_free(pf);
    }
  }

  return vfont;
}

VFont *BKE_vfont_load_exists_ex(Main *bmain, const char *filepath, bool *r_exists)
{
  char filepath_abs[FILE_MAX], filepath_test[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));

  /* first search an identical filepath */
  LISTBASE_FOREACH (VFont *, vfont, &bmain->fonts) {
    STRNCPY(filepath_test, vfont->filepath);
    BLI_path_abs(filepath_test, ID_BLEND_PATH(bmain, &vfont->id));

    if (BLI_path_cmp(filepath_test, filepath_abs) == 0) {
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

VFont *BKE_vfont_load_exists(Main *bmain, const char *filepath)
{
  return BKE_vfont_load_exists_ex(bmain, filepath, nullptr);
}

VFont *BKE_vfont_builtin_ensure()
{
  LISTBASE_FOREACH (VFont *, vfont, &G_MAIN->fonts) {
    if (BKE_vfont_is_builtin(vfont)) {
      return vfont;
    }
  }

  /* Newly loaded ID's have a user by default, in this case the caller is responsible
   * for assigning a user, otherwise an additional user would be added, see: #100819. */
  VFont *vfont = BKE_vfont_load(G_MAIN, FO_BUILTIN_NAME);
  id_us_min(&vfont->id);
  BLI_assert(vfont->id.us == 0);
  return vfont;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont Selection
 * \{ */

int BKE_vfont_select_get(const Curve *cu, int *r_start, int *r_end)
{
  EditFont *ef = cu->editfont;
  int start, end, direction;

  if (ef == nullptr || (cu->ob_type != OB_FONT)) {
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

void BKE_vfont_select_clamp(Curve *cu)
{
  EditFont *ef = cu->editfont;

  BLI_assert((cu->ob_type == OB_FONT) && ef);

  CLAMP_MAX(ef->pos, ef->len);
  CLAMP_MAX(ef->selstart, ef->len + 1);
  CLAMP_MAX(ef->selend, ef->len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VFont Clipboard
 * \{ */

static struct {
  char32_t *text_buffer;
  CharInfo *info_buffer;
  size_t len_utf32;
  size_t len_utf8;
} g_vfont_clipboard = {nullptr};

void BKE_vfont_clipboard_free()
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

  text = MEM_malloc_arrayN<char32_t>((len + 1), __func__);
  if (text == nullptr) {
    return;
  }

  info = MEM_malloc_arrayN<CharInfo>(len, __func__);
  if (info == nullptr) {
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
