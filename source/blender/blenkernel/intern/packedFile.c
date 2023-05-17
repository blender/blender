/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif
#include "MEM_guardedalloc.h"
#include <string.h>

#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_sound_types.h"
#include "DNA_vfont_types.h"
#include "DNA_volume_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_vfont.h"
#include "BKE_volume.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLO_read_write.h"

int BKE_packedfile_seek(PackedFile *pf, int offset, int whence)
{
  int oldseek = -1, seek = 0;

  if (pf) {
    oldseek = pf->seek;
    switch (whence) {
      case SEEK_CUR:
        seek = oldseek + offset;
        break;
      case SEEK_END:
        seek = pf->size + offset;
        break;
      case SEEK_SET:
        seek = offset;
        break;
      default:
        oldseek = -1;
        break;
    }
    if (seek < 0) {
      seek = 0;
    }
    else if (seek > pf->size) {
      seek = pf->size;
    }
    pf->seek = seek;
  }

  return oldseek;
}

void BKE_packedfile_rewind(PackedFile *pf)
{
  BKE_packedfile_seek(pf, 0, SEEK_SET);
}

int BKE_packedfile_read(PackedFile *pf, void *data, int size)
{
  if ((pf != NULL) && (size >= 0) && (data != NULL)) {
    if (size + pf->seek > pf->size) {
      size = pf->size - pf->seek;
    }

    if (size > 0) {
      memcpy(data, ((char *)pf->data) + pf->seek, size);
    }
    else {
      size = 0;
    }

    pf->seek += size;
  }
  else {
    size = -1;
  }

  return size;
}

int BKE_packedfile_count_all(Main *bmain)
{
  Image *ima;
  VFont *vf;
  bSound *sound;
  Volume *volume;
  int count = 0;

  /* let's check if there are packed files... */
  for (ima = bmain->images.first; ima; ima = ima->id.next) {
    if (BKE_image_has_packedfile(ima)) {
      count++;
    }
  }

  for (vf = bmain->fonts.first; vf; vf = vf->id.next) {
    if (vf->packedfile) {
      count++;
    }
  }

  for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
    if (sound->packedfile) {
      count++;
    }
  }

  for (volume = bmain->volumes.first; volume; volume = volume->id.next) {
    if (volume->packedfile) {
      count++;
    }
  }

  return count;
}

void BKE_packedfile_free(PackedFile *pf)
{
  if (pf) {
    BLI_assert(pf->data != NULL);

    MEM_SAFE_FREE(pf->data);
    MEM_freeN(pf);
  }
  else {
    printf("%s: Trying to free a NULL pointer\n", __func__);
  }
}

PackedFile *BKE_packedfile_duplicate(const PackedFile *pf_src)
{
  BLI_assert(pf_src != NULL);
  BLI_assert(pf_src->data != NULL);

  PackedFile *pf_dst;

  pf_dst = MEM_dupallocN(pf_src);
  pf_dst->data = MEM_dupallocN(pf_src->data);

  return pf_dst;
}

PackedFile *BKE_packedfile_new_from_memory(void *mem, int memlen)
{
  BLI_assert(mem != NULL);

  PackedFile *pf = MEM_callocN(sizeof(*pf), "PackedFile");
  pf->data = mem;
  pf->size = memlen;

  return pf;
}

PackedFile *BKE_packedfile_new(ReportList *reports, const char *filepath_rel, const char *basepath)
{
  PackedFile *pf = NULL;
  int file, filelen;
  char filepath[FILE_MAX];
  void *data;

  /* render result has no filepath and can be ignored
   * any other files with no name can be ignored too */
  if (filepath_rel[0] == '\0') {
    return pf;
  }

  // XXX waitcursor(1);

  /* convert relative filenames to absolute filenames */

  STRNCPY(filepath, filepath_rel);
  BLI_path_abs(filepath, basepath);

  /* open the file
   * and create a PackedFile structure */

  file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    BKE_reportf(reports, RPT_ERROR, "Unable to pack file, source path '%s' not found", filepath);
  }
  else {
    filelen = BLI_file_descriptor_size(file);

    if (filelen == 0) {
      /* MEM_mallocN complains about MEM_mallocN(0, "bla");
       * we don't care.... */
      data = MEM_mallocN(1, "packFile");
    }
    else {
      data = MEM_mallocN(filelen, "packFile");
    }
    if (read(file, data, filelen) == filelen) {
      pf = BKE_packedfile_new_from_memory(data, filelen);
    }
    else {
      MEM_freeN(data);
    }

    close(file);
  }

  // XXX waitcursor(0);

  return pf;
}

void BKE_packedfile_pack_all(Main *bmain, ReportList *reports, bool verbose)
{
  Image *ima;
  VFont *vfont;
  bSound *sound;
  Volume *volume;
  int tot = 0;

  for (ima = bmain->images.first; ima; ima = ima->id.next) {
    if (BKE_image_has_packedfile(ima) == false && !ID_IS_LINKED(ima)) {
      if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_TILED)) {
        BKE_image_packfiles(reports, ima, ID_BLEND_PATH(bmain, &ima->id));
        tot++;
      }
      else if (ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE) && verbose) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Image '%s' skipped, packing movies or image sequences not supported",
                    ima->id.name + 2);
      }
    }
  }

  for (vfont = bmain->fonts.first; vfont; vfont = vfont->id.next) {
    if (vfont->packedfile == NULL && !ID_IS_LINKED(vfont) && BKE_vfont_is_builtin(vfont) == false)
    {
      vfont->packedfile = BKE_packedfile_new(
          reports, vfont->filepath, BKE_main_blendfile_path(bmain));
      tot++;
    }
  }

  for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
    if (sound->packedfile == NULL && !ID_IS_LINKED(sound)) {
      sound->packedfile = BKE_packedfile_new(
          reports, sound->filepath, BKE_main_blendfile_path(bmain));
      tot++;
    }
  }

  for (volume = bmain->volumes.first; volume; volume = volume->id.next) {
    if (volume->packedfile == NULL && !ID_IS_LINKED(volume)) {
      volume->packedfile = BKE_packedfile_new(
          reports, volume->filepath, BKE_main_blendfile_path(bmain));
      tot++;
    }
  }

  if (tot > 0) {
    BKE_reportf(reports, RPT_INFO, "Packed %d file(s)", tot);
  }
  else if (verbose) {
    BKE_report(reports, RPT_INFO, "No new files have been packed");
  }
}

int BKE_packedfile_write_to_file(ReportList *reports,
                                 const char *ref_file_name,
                                 const char *filepath_rel,
                                 PackedFile *pf,
                                 const bool guimode)
{
  int file, number;
  int ret_value = RET_OK;
  bool remove_tmp = false;
  char filepath[FILE_MAX];
  char filepath_temp[FILE_MAX];
  /*      void *data; */

  if (guimode) {
  }  // XXX  waitcursor(1);

  STRNCPY(filepath, filepath_rel);
  BLI_path_abs(filepath, ref_file_name);

  if (BLI_exists(filepath)) {
    for (number = 1; number <= 999; number++) {
      SNPRINTF(filepath_temp, "%s.%03d_", filepath, number);
      if (!BLI_exists(filepath_temp)) {
        if (BLI_copy(filepath, filepath_temp) == RET_OK) {
          remove_tmp = true;
        }
        break;
      }
    }
  }

  BLI_file_ensure_parent_dir_exists(filepath);

  file = BLI_open(filepath, O_BINARY + O_WRONLY + O_CREAT + O_TRUNC, 0666);
  if (file == -1) {
    BKE_reportf(reports, RPT_ERROR, "Error creating file '%s'", filepath);
    ret_value = RET_ERROR;
  }
  else {
    if (write(file, pf->data, pf->size) != pf->size) {
      BKE_reportf(reports, RPT_ERROR, "Error writing file '%s'", filepath);
      ret_value = RET_ERROR;
    }
    else {
      BKE_reportf(reports, RPT_INFO, "Saved packed file to: %s", filepath);
    }

    close(file);
  }

  if (remove_tmp) {
    if (ret_value == RET_ERROR) {
      if (BLI_rename_overwrite(filepath_temp, filepath) != 0) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Error restoring temp file (check files '%s' '%s')",
                    filepath_temp,
                    filepath);
      }
    }
    else {
      if (BLI_delete(filepath_temp, false, false) != 0) {
        BKE_reportf(reports, RPT_ERROR, "Error deleting '%s' (ignored)", filepath_temp);
      }
    }
  }

  if (guimode) {
  }  // XXX waitcursor(0);

  return ret_value;
}

enum ePF_FileCompare BKE_packedfile_compare_to_file(const char *ref_file_name,
                                                    const char *filepath_rel,
                                                    PackedFile *pf)
{
  BLI_stat_t st;
  enum ePF_FileCompare ret_val;
  char buf[4096];
  char filepath[FILE_MAX];

  STRNCPY(filepath, filepath_rel);
  BLI_path_abs(filepath, ref_file_name);

  if (BLI_stat(filepath, &st) == -1) {
    ret_val = PF_CMP_NOFILE;
  }
  else if (st.st_size != pf->size) {
    ret_val = PF_CMP_DIFFERS;
  }
  else {
    /* we'll have to compare the two... */

    const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
    if (file == -1) {
      ret_val = PF_CMP_NOFILE;
    }
    else {
      ret_val = PF_CMP_EQUAL;

      for (int i = 0; i < pf->size; i += sizeof(buf)) {
        int len = pf->size - i;
        if (len > sizeof(buf)) {
          len = sizeof(buf);
        }

        if (read(file, buf, len) != len) {
          /* read error ... */
          ret_val = PF_CMP_DIFFERS;
          break;
        }

        if (memcmp(buf, ((char *)pf->data) + i, len) != 0) {
          ret_val = PF_CMP_DIFFERS;
          break;
        }
      }

      close(file);
    }
  }

  return ret_val;
}

char *BKE_packedfile_unpack_to_file(ReportList *reports,
                                    const char *ref_file_name,
                                    const char *abs_name,
                                    const char *local_name,
                                    PackedFile *pf,
                                    enum ePF_FileStatus how)
{
  char *newname = NULL;
  const char *temp = NULL;

  if (pf != NULL) {
    switch (how) {
      case PF_KEEP:
        break;
      case PF_REMOVE:
        temp = abs_name;
        break;
      case PF_USE_LOCAL: {
        char temp_abs[FILE_MAX];

        STRNCPY(temp_abs, local_name);
        BLI_path_abs(temp_abs, ref_file_name);

        /* if file exists use it */
        if (BLI_exists(temp_abs)) {
          temp = local_name;
          break;
        }
        /* else create it */
        ATTR_FALLTHROUGH;
      }
      case PF_WRITE_LOCAL:
        if (BKE_packedfile_write_to_file(reports, ref_file_name, local_name, pf, 1) == RET_OK) {
          temp = local_name;
        }
        break;
      case PF_USE_ORIGINAL: {
        char temp_abs[FILE_MAX];

        STRNCPY(temp_abs, abs_name);
        BLI_path_abs(temp_abs, ref_file_name);

        /* if file exists use it */
        if (BLI_exists(temp_abs)) {
          BKE_reportf(reports, RPT_INFO, "Use existing file (instead of packed): %s", abs_name);
          temp = abs_name;
          break;
        }
        /* else create it */
        ATTR_FALLTHROUGH;
      }
      case PF_WRITE_ORIGINAL:
        if (BKE_packedfile_write_to_file(reports, ref_file_name, abs_name, pf, 1) == RET_OK) {
          temp = abs_name;
        }
        break;
      default:
        printf("%s: unknown return_value %u\n", __func__, how);
        break;
    }

    if (temp) {
      newname = BLI_strdup(temp);
    }
  }

  return newname;
}

static void unpack_generate_paths(const char *filepath,
                                  ID *id,
                                  char *r_abspath,
                                  size_t abspath_maxncpy,
                                  char *r_relpath,
                                  size_t relpath_maxncpy)
{
  const short id_type = GS(id->name);
  char temp_filename[FILE_MAX];
  char temp_dirname[FILE_MAXDIR];

  BLI_path_split_dir_file(
      filepath, temp_dirname, sizeof(temp_dirname), temp_filename, sizeof(temp_filename));

  if (temp_filename[0] == '\0') {
    /* NOTE: we generally do not have any real way to re-create extension out of data. */
    const size_t len = STRNCPY_RLEN(temp_filename, id->name + 2);
    printf("%s\n", temp_filename);

    /* For images ensure that the temporary filename contains tile number information as well as
     * a file extension based on the file magic. */
    if (id_type == ID_IM) {
      Image *ima = (Image *)id;
      ImagePackedFile *imapf = ima->packedfiles.last;
      if (imapf != NULL && imapf->packedfile != NULL) {
        const PackedFile *pf = imapf->packedfile;
        enum eImbFileType ftype = IMB_ispic_type_from_memory((const uchar *)pf->data, pf->size);
        if (ima->source == IMA_SRC_TILED) {
          char tile_number[6];
          SNPRINTF(tile_number, ".%d", imapf->tile_number);
          BLI_strncpy(temp_filename + len, tile_number, sizeof(temp_filename) - len);
        }
        if (ftype != IMB_FTYPE_NONE) {
          const int imtype = BKE_ftype_to_imtype(ftype, NULL);
          BKE_image_path_ext_from_imtype_ensure(temp_filename, sizeof(temp_filename), imtype);
        }
      }
    }

    BLI_path_make_safe_filename(temp_filename);
    printf("%s\n", temp_filename);
  }

  if (temp_dirname[0] == '\0') {
    /* Fallback to relative dir. */
    STRNCPY(temp_dirname, "//");
  }

  {
    const char *dir_name = NULL;
    switch (id_type) {
      case ID_VF:
        dir_name = "fonts";
        break;
      case ID_SO:
        dir_name = "sounds";
        break;
      case ID_IM:
        dir_name = "textures";
        break;
      case ID_VO:
        dir_name = "volumes";
        break;
      default:
        break;
    }
    if (dir_name) {
      BLI_path_join(r_relpath, relpath_maxncpy, "//", dir_name, temp_filename);
    }
  }

  {
    size_t len = BLI_strncpy_rlen(r_abspath, temp_dirname, abspath_maxncpy);
    BLI_strncpy(r_abspath + len, temp_filename, abspath_maxncpy - len);
  }
}

char *BKE_packedfile_unpack(Main *bmain,
                            ReportList *reports,
                            ID *id,
                            const char *orig_file_path,
                            PackedFile *pf,
                            enum ePF_FileStatus how)
{
  char localname[FILE_MAX], absname[FILE_MAX];
  char *new_name = NULL;

  if (id != NULL) {
    unpack_generate_paths(
        orig_file_path, id, absname, sizeof(absname), localname, sizeof(localname));
    new_name = BKE_packedfile_unpack_to_file(
        reports, BKE_main_blendfile_path(bmain), absname, localname, pf, how);
  }

  return new_name;
}

int BKE_packedfile_unpack_vfont(Main *bmain,
                                ReportList *reports,
                                VFont *vfont,
                                enum ePF_FileStatus how)
{
  int ret_value = RET_ERROR;
  if (vfont) {
    char *new_file_path = BKE_packedfile_unpack(
        bmain, reports, (ID *)vfont, vfont->filepath, vfont->packedfile, how);

    if (new_file_path != NULL) {
      ret_value = RET_OK;
      BKE_packedfile_free(vfont->packedfile);
      vfont->packedfile = NULL;
      STRNCPY(vfont->filepath, new_file_path);
      MEM_freeN(new_file_path);
    }
  }

  return ret_value;
}

int BKE_packedfile_unpack_sound(Main *bmain,
                                ReportList *reports,
                                bSound *sound,
                                enum ePF_FileStatus how)
{
  int ret_value = RET_ERROR;

  if (sound != NULL) {
    char *new_file_path = BKE_packedfile_unpack(
        bmain, reports, (ID *)sound, sound->filepath, sound->packedfile, how);
    if (new_file_path != NULL) {
      STRNCPY(sound->filepath, new_file_path);
      MEM_freeN(new_file_path);

      BKE_packedfile_free(sound->packedfile);
      sound->packedfile = NULL;

      BKE_sound_load(bmain, sound);

      ret_value = RET_OK;
    }
  }

  return ret_value;
}

int BKE_packedfile_unpack_image(Main *bmain,
                                ReportList *reports,
                                Image *ima,
                                enum ePF_FileStatus how)
{
  int ret_value = RET_ERROR;

  if (ima != NULL) {
    while (ima->packedfiles.last) {
      ImagePackedFile *imapf = ima->packedfiles.last;
      char *new_file_path = BKE_packedfile_unpack(
          bmain, reports, (ID *)ima, imapf->filepath, imapf->packedfile, how);

      if (new_file_path != NULL) {
        ImageView *iv;

        ret_value = ret_value == RET_ERROR ? RET_ERROR : RET_OK;
        BKE_packedfile_free(imapf->packedfile);
        imapf->packedfile = NULL;

        /* update the new corresponding view filepath */
        iv = BLI_findstring(&ima->views, imapf->filepath, offsetof(ImageView, filepath));
        if (iv) {
          STRNCPY(iv->filepath, new_file_path);
        }

        /* keep the new name in the image for non-pack specific reasons */
        if (how != PF_REMOVE) {
          STRNCPY(ima->filepath, new_file_path);
          if (ima->source == IMA_SRC_TILED) {
            /* Ensure that the Image filepath is kept in a tokenized format. */
            BKE_image_ensure_tile_token(ima->filepath, sizeof(ima->filepath));
          }
        }
        MEM_freeN(new_file_path);
      }
      else {
        ret_value = RET_ERROR;
      }

      BLI_remlink(&ima->packedfiles, imapf);
      MEM_freeN(imapf);
    }
  }

  if (ret_value == RET_OK) {
    BKE_image_signal(bmain, ima, NULL, IMA_SIGNAL_RELOAD);
  }

  return ret_value;
}

int BKE_packedfile_unpack_volume(Main *bmain,
                                 ReportList *reports,
                                 Volume *volume,
                                 enum ePF_FileStatus how)
{
  int ret_value = RET_ERROR;

  if (volume != NULL) {
    char *new_file_path = BKE_packedfile_unpack(
        bmain, reports, (ID *)volume, volume->filepath, volume->packedfile, how);
    if (new_file_path != NULL) {
      STRNCPY(volume->filepath, new_file_path);
      MEM_freeN(new_file_path);

      BKE_packedfile_free(volume->packedfile);
      volume->packedfile = NULL;

      BKE_volume_unload(volume);

      ret_value = RET_OK;
    }
  }

  return ret_value;
}

int BKE_packedfile_unpack_all_libraries(Main *bmain, ReportList *reports)
{
  Library *lib;
  char *newname;
  int ret_value = RET_ERROR;

  for (lib = bmain->libraries.first; lib; lib = lib->id.next) {
    if (lib->packedfile && lib->filepath[0]) {

      newname = BKE_packedfile_unpack_to_file(reports,
                                              BKE_main_blendfile_path(bmain),
                                              lib->filepath_abs,
                                              lib->filepath_abs,
                                              lib->packedfile,
                                              PF_WRITE_ORIGINAL);
      if (newname != NULL) {
        ret_value = RET_OK;

        printf("Unpacked .blend library: %s\n", newname);

        BKE_packedfile_free(lib->packedfile);
        lib->packedfile = NULL;

        MEM_freeN(newname);
      }
    }
  }

  return ret_value;
}

void BKE_packedfile_pack_all_libraries(Main *bmain, ReportList *reports)
{
  Library *lib;

  /* Test for relativeness. */
  for (lib = bmain->libraries.first; lib; lib = lib->id.next) {
    if (!BLI_path_is_rel(lib->filepath)) {
      break;
    }
  }

  if (lib) {
    BKE_reportf(reports, RPT_ERROR, "Cannot pack absolute file: '%s'", lib->filepath);
    return;
  }

  for (lib = bmain->libraries.first; lib; lib = lib->id.next) {
    if (lib->packedfile == NULL) {
      lib->packedfile = BKE_packedfile_new(reports, lib->filepath, BKE_main_blendfile_path(bmain));
    }
  }
}

void BKE_packedfile_unpack_all(Main *bmain, ReportList *reports, enum ePF_FileStatus how)
{
  Image *ima;
  VFont *vf;
  bSound *sound;
  Volume *volume;

  for (ima = bmain->images.first; ima; ima = ima->id.next) {
    if (BKE_image_has_packedfile(ima)) {
      BKE_packedfile_unpack_image(bmain, reports, ima, how);
    }
  }

  for (vf = bmain->fonts.first; vf; vf = vf->id.next) {
    if (vf->packedfile) {
      BKE_packedfile_unpack_vfont(bmain, reports, vf, how);
    }
  }

  for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
    if (sound->packedfile) {
      BKE_packedfile_unpack_sound(bmain, reports, sound, how);
    }
  }

  for (volume = bmain->volumes.first; volume; volume = volume->id.next) {
    if (volume->packedfile) {
      BKE_packedfile_unpack_volume(bmain, reports, volume, how);
    }
  }
}

bool BKE_packedfile_id_check(const ID *id)
{
  switch (GS(id->name)) {
    case ID_IM: {
      const Image *ima = (const Image *)id;
      return BKE_image_has_packedfile(ima);
    }
    case ID_VF: {
      const VFont *vf = (const VFont *)id;
      return vf->packedfile != NULL;
    }
    case ID_SO: {
      const bSound *snd = (const bSound *)id;
      return snd->packedfile != NULL;
    }
    case ID_VO: {
      const Volume *volume = (const Volume *)id;
      return volume->packedfile != NULL;
    }
    case ID_LI: {
      const Library *li = (const Library *)id;
      return li->packedfile != NULL;
    }
    default:
      break;
  }
  return false;
}

void BKE_packedfile_id_unpack(Main *bmain, ID *id, ReportList *reports, enum ePF_FileStatus how)
{
  switch (GS(id->name)) {
    case ID_IM: {
      Image *ima = (Image *)id;
      if (BKE_image_has_packedfile(ima)) {
        BKE_packedfile_unpack_image(bmain, reports, ima, how);
      }
      break;
    }
    case ID_VF: {
      VFont *vf = (VFont *)id;
      if (vf->packedfile) {
        BKE_packedfile_unpack_vfont(bmain, reports, vf, how);
      }
      break;
    }
    case ID_SO: {
      bSound *snd = (bSound *)id;
      if (snd->packedfile) {
        BKE_packedfile_unpack_sound(bmain, reports, snd, how);
      }
      break;
    }
    case ID_VO: {
      Volume *volume = (Volume *)id;
      if (volume->packedfile) {
        BKE_packedfile_unpack_volume(bmain, reports, volume, how);
      }
      break;
    }
    case ID_LI: {
      Library *li = (Library *)id;
      BKE_reportf(reports, RPT_ERROR, "Cannot unpack individual Library file, '%s'", li->filepath);
      break;
    }
    default:
      break;
  }
}

void BKE_packedfile_blend_write(BlendWriter *writer, PackedFile *pf)
{
  if (pf == NULL) {
    return;
  }
  BLO_write_struct(writer, PackedFile, pf);
  BLO_write_raw(writer, pf->size, pf->data);
}

void BKE_packedfile_blend_read(BlendDataReader *reader, PackedFile **pf_p)
{
  BLO_read_packed_address(reader, pf_p);
  PackedFile *pf = *pf_p;
  if (pf == NULL) {
    return;
  }

  BLO_read_packed_address(reader, &pf->data);
  if (pf->data == NULL) {
    /* We cannot allow a PackedFile with a NULL data field,
     * the whole code assumes this is not possible. See #70315. */
    printf("%s: NULL packedfile data, cleaning up...\n", __func__);
    MEM_SAFE_FREE(pf);
  }
}
