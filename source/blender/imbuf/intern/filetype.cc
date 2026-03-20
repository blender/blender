/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstddef>

#include "BLI_utildefines.h"

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "oiio/openimageio_api.h"

#ifdef WITH_IMAGE_OPENEXR
#  include "openexr/openexr_api.h"
#endif

namespace blender {

const ImFileType IMB_FILE_TYPES[] = {
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_jpeg,
        /*load*/ imb_load_jpeg,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ imb_thumbnail_jpeg,
        /*save*/ imb_savejpeg,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ eImFileTypeCapability::File,
        /*filetype*/ IMB_FTYPE_JPG,
        /*filetype_id*/ "JPEG",
        /*file_extensions*/ imb_file_extensions_jpeg,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_png,
        /*load*/ imb_load_png,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_png,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_PNG,
        /*filetype_id*/ "PNG",
        /*file_extensions*/ imb_file_extensions_png,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_bmp,
        /*load*/ imb_load_bmp,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_bmp,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_BMP,
        /*filetype_id*/ "BMP",
        /*file_extensions*/ imb_file_extensions_bmp,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_tga,
        /*load*/ imb_load_tga,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_tga,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_TGA,
        /*filetype_id*/ "TGA",
        /*file_extensions*/ imb_file_extensions_tga,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_iris,
        /*load*/ imb_loadiris,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_saveiris,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ eImFileTypeCapability::File,
        /*filetype*/ IMB_FTYPE_IRIS,
        /*filetype_id*/ "IRIS",
        /*file_extensions*/ imb_file_extensions_iris,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
#ifdef WITH_IMAGE_CINEON
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_dpx,
        /*load*/ imb_load_dpx,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_dpx,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_DPX,
        /*filetype_id*/ "DPX",
        /*file_extensions*/ imb_file_extensions_dpx,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_FLOAT,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_cineon,
        /*load*/ imb_load_cineon,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_cineon,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ eImFileTypeCapability::File,
        /*filetype*/ IMB_FTYPE_CINEON,
        /*filetype_id*/ "CINEON",
        /*file_extensions*/ imb_file_extensions_cineon,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_FLOAT,
    },
#endif
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_tiff,
        /*load*/ imb_load_tiff,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_tiff,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_TIF,
        /*filetype_id*/ "TIFF",
        /*file_extensions*/ imb_file_extensions_tiff,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_hdr,
        /*load*/ imb_load_hdr,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_hdr,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_RADHDR,
        /*filetype_id*/ "HDR",
        /*file_extensions*/ imb_file_extensions_hdr,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_FLOAT,
    },
#ifdef WITH_IMAGE_OPENEXR
    {
        /*init*/ imb_initopenexr,
        /*exit*/ imb_exitopenexr,
        /*is_a*/ imb_is_a_openexr,
        /*load*/ imb_load_openexr,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ imb_load_filepath_thumbnail_openexr,
        /*save*/ imb_save_openexr,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_OPENEXR,
        /*filetype_id*/ "OPEN_EXR",
        /*file_extensions*/ imb_file_extensions_openexr,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_FLOAT,
    },
#endif
#ifdef WITH_IMAGE_OPENJPEG
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_jp2,
        /*load*/ imb_load_jp2,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_jp2,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ eImFileTypeCapability::File,
        /*filetype*/ IMB_FTYPE_JP2,
        /*filetype_id*/ "JPEG2000",
        /*file_extensions*/ imb_file_extensions_jp2,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
#endif
    {
        /*init*/ imb_init_dds,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_dds,
        /*load*/ imb_load_dds,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ nullptr,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ eImFileTypeCapability::Zero,
        /*filetype*/ IMB_FTYPE_DDS,
        /*filetype_id*/ "DDS",
        /*file_extensions*/ imb_file_extensions_dds,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_psd,
        /*load*/ imb_load_psd,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ nullptr,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ eImFileTypeCapability::Zero,
        /*filetype*/ IMB_FTYPE_PSD,
        /*filetype_id*/ "PSD",
        /*file_extensions*/ imb_file_extensions_psd,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_FLOAT,
    },
#ifdef WITH_IMAGE_WEBP
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_webp,
        /*load*/ imb_loadwebp,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ imb_load_filepath_thumbnail_webp,
        /*save*/ imb_savewebp,
        /*flag*/ 0,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_WEBP,
        /*filetype_id*/ "WEBP",
        /*file_extensions*/ imb_file_extensions_webp,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
#endif
    {
        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ imb_is_a_avif,
        /*load*/ imb_load_avif,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ nullptr,
        /*save*/ imb_save_avif,
        /*flag*/ IM_FTYPE_FLOAT,
        /*capability_read*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*capability_write*/ (eImFileTypeCapability::File | eImFileTypeCapability::Memory),
        /*filetype*/ IMB_FTYPE_AVIF,
        /*filetype_id*/ "AVIF",
        /*file_extensions*/ imb_file_extensions_avif,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        /* Only implementing thumbnailing for SVG file type to support specialized importers.
         * General file loading, if wanted, would require a better library and would have to
         * support features like user-specified resolution. */

        /*init*/ nullptr,
        /*exit*/ nullptr,
        /*is_a*/ nullptr,
        /*load*/ nullptr,
        /*load_filepath*/ nullptr,
        /*load_filepath_thumbnail*/ imb_load_filepath_thumbnail_svg,
        /*save*/ nullptr,
        /*flag*/ 0,
        /*capability_read*/ eImFileTypeCapability::Zero,
        /*capability_write*/ eImFileTypeCapability::Zero,
        /*filetype*/ IMB_FTYPE_NONE,
        /*filetype_id*/ nullptr,
        /*file_extensions*/ nullptr,
        /*default_save_role*/ COLOR_ROLE_DEFAULT_BYTE,
    },
    {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0,
        eImFileTypeCapability::Zero,
        eImFileTypeCapability::Zero,
        0,
        nullptr,
        nullptr,
        0,
    },
};

const ImFileType *IMB_FILE_TYPES_LAST = &IMB_FILE_TYPES[ARRAY_SIZE(IMB_FILE_TYPES) - 1];

const ImFileType *IMB_file_type_from_ftype(int ftype)
{
  for (const ImFileType *type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (ftype == type->filetype) {
      return type;
    }
  }
  return nullptr;
}

const ImFileType *IMB_file_type_from_ibuf(const ImBuf *ibuf)
{
  return IMB_file_type_from_ftype(ibuf->ftype);
}

bool IMB_ftype_is_supported(int ftype)
{
  return IMB_file_type_from_ftype(ftype) != nullptr;
}

const char *IMB_ftype_to_id(int ftype)
{
  const ImFileType *type = IMB_file_type_from_ftype(ftype);
  return type ? type->filetype_id : nullptr;
}

int IMB_ftype_from_id(const char *id)
{
  for (const ImFileType *type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (type->filetype_id && STREQ(id, type->filetype_id)) {
      return type->filetype;
    }
  }
  return IMB_FTYPE_NONE;
}

const char **IMB_ftype_file_extensions(int ftype)
{
  const ImFileType *type = IMB_file_type_from_ftype(ftype);
  return type ? type->file_extensions : nullptr;
}

eImFileTypeCapability IMB_ftype_capability_read(int ftype)
{
  const ImFileType *type = IMB_file_type_from_ftype(ftype);
  return type ? type->capability_read : eImFileTypeCapability::Zero;
}

eImFileTypeCapability IMB_ftype_capability_write(int ftype)
{
  const ImFileType *type = IMB_file_type_from_ftype(ftype);
  return type ? type->capability_write : eImFileTypeCapability::Zero;
}

void imb_filetypes_init()
{
  const ImFileType *type;

  OIIO_init();

  for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (type->init) {
      type->init();
    }
  }
}

void imb_filetypes_exit()
{
  const ImFileType *type;

  for (type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (type->exit) {
      type->exit();
    }
  }
}

}  // namespace blender
