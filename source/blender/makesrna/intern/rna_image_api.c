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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "DNA_packedFile_types.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "BIF_gl.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include <errno.h>
#  include "BKE_image.h"
#  include "BKE_packedFile.h"
#  include "BKE_main.h"

#  include "IMB_imbuf.h"
#  include "IMB_colormanagement.h"

#  include "DNA_image_types.h"
#  include "DNA_scene_types.h"

#  include "MEM_guardedalloc.h"

static void rna_ImagePackedFile_save(ImagePackedFile *imapf, Main *bmain, ReportList *reports)
{
  if (writePackedFile(
          reports, BKE_main_blendfile_path(bmain), imapf->filepath, imapf->packedfile, 0) !=
      RET_OK) {
    BKE_reportf(reports, RPT_ERROR, "Could not save packed file to disk as '%s'", imapf->filepath);
  }
}

static void rna_Image_save_render(
    Image *image, bContext *C, ReportList *reports, const char *path, Scene *scene)
{
  ImBuf *ibuf;

  if (scene == NULL) {
    scene = CTX_data_scene(C);
  }

  if (scene) {
    ImageUser iuser = {NULL};
    void *lock;

    iuser.scene = scene;
    iuser.ok = 1;

    ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

    if (ibuf == NULL) {
      BKE_report(reports, RPT_ERROR, "Could not acquire buffer from image");
    }
    else {
      ImBuf *write_ibuf;

      write_ibuf = IMB_colormanagement_imbuf_for_write(
          ibuf, true, true, &scene->view_settings, &scene->display_settings, &scene->r.im_format);

      write_ibuf->planes = scene->r.im_format.planes;
      write_ibuf->dither = scene->r.dither_intensity;

      if (!BKE_imbuf_write(write_ibuf, path, &scene->r.im_format)) {
        BKE_reportf(reports, RPT_ERROR, "Could not write image: %s, '%s'", strerror(errno), path);
      }

      if (write_ibuf != ibuf)
        IMB_freeImBuf(write_ibuf);
    }

    BKE_image_release_ibuf(image, ibuf, lock);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Scene not in context, could not get save parameters");
  }
}

static void rna_Image_save(Image *image, Main *bmain, bContext *C, ReportList *reports)
{
  void *lock;

  ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);
  if (ibuf) {
    char filename[FILE_MAX];
    BLI_strncpy(filename, image->name, sizeof(filename));
    BLI_path_abs(filename, ID_BLEND_PATH(bmain, &image->id));

    /* note, we purposefully ignore packed files here,
     * developers need to explicitly write them via 'packed_files' */

    if (IMB_saveiff(ibuf, filename, ibuf->flags)) {
      image->type = IMA_TYPE_IMAGE;

      if (image->source == IMA_SRC_GENERATED)
        image->source = IMA_SRC_FILE;

      IMB_colormanagement_colorspace_from_ibuf_ftype(&image->colorspace_settings, ibuf);

      ibuf->userflags &= ~IB_BITMAPDIRTY;
    }
    else {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Image '%s' could not be saved to '%s'",
                  image->id.name + 2,
                  image->name);
    }
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
  }

  BKE_image_release_ibuf(image, ibuf, lock);
  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, image);
}

static void rna_Image_pack(
    Image *image, Main *bmain, bContext *C, ReportList *reports, const char *data, int data_len)
{
  BKE_image_free_packedfiles(image);

  if (data) {
    char *data_dup = MEM_mallocN(sizeof(*data_dup) * (size_t)data_len, __func__);
    memcpy(data_dup, data, (size_t)data_len);
    BKE_image_packfiles_from_mem(reports, image, data_dup, (size_t)data_len);
  }
  else if (BKE_image_is_dirty(image)) {
    BKE_image_memorypack(image);
  }
  else {
    BKE_image_packfiles(reports, image, ID_BLEND_PATH(bmain, &image->id));
  }

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, image);
}

static void rna_Image_unpack(Image *image, Main *bmain, ReportList *reports, int method)
{
  if (!BKE_image_has_packedfile(image)) {
    BKE_report(reports, RPT_ERROR, "Image not packed");
  }
  else if (BKE_image_is_animated(image)) {
    BKE_report(reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
    return;
  }
  else {
    /* reports its own error on failure */
    unpackImage(bmain, reports, image, method);
  }
}

static void rna_Image_reload(Image *image, Main *bmain)
{
  BKE_image_signal(bmain, image, NULL, IMA_SIGNAL_RELOAD);
  WM_main_add_notifier(NC_IMAGE | NA_EDITED, image);
}

static void rna_Image_update(Image *image, ReportList *reports)
{
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);

  if (ibuf == NULL) {
    BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
    return;
  }

  if (ibuf->rect)
    IMB_rect_from_float(ibuf);

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

  BKE_image_release_ibuf(image, ibuf, NULL);
}

static void rna_Image_scale(Image *image, ReportList *reports, int width, int height)
{
  if (!BKE_image_scale(image, width, height)) {
    BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
  }
}

static int rna_Image_gl_load(Image *image, ReportList *reports, int frame)
{
  ImageUser iuser = {NULL};
  iuser.framenr = frame;
  iuser.ok = true;

  GPUTexture *tex = GPU_texture_from_blender(image, &iuser, GL_TEXTURE_2D);

  if (tex == NULL) {
    BKE_reportf(reports, RPT_ERROR, "Failed to load image texture '%s'", image->id.name + 2);
    return (int)GL_INVALID_OPERATION;
  }

  return GL_NO_ERROR;
}

static int rna_Image_gl_touch(Image *image, ReportList *reports, int frame)
{
  int error = GL_NO_ERROR;

  BKE_image_tag_time(image);

  if (image->gputexture[TEXTARGET_TEXTURE_2D] == NULL)
    error = rna_Image_gl_load(image, reports, frame);

  return error;
}

static void rna_Image_gl_free(Image *image)
{
  GPU_free_image(image);

  /* remove the nocollect flag, image is available for garbage collection again */
  image->flag &= ~IMA_NOCOLLECT;
}

static void rna_Image_filepath_from_user(Image *image, ImageUser *image_user, char *filepath)
{
  BKE_image_user_file_path(image_user, image, filepath);
}

static void rna_Image_buffers_free(Image *image)
{
  BKE_image_free_buffers_ex(image, true);
}

#else

void RNA_api_image_packed_file(StructRNA *srna)
{
  FunctionRNA *func;

  func = RNA_def_function(srna, "save", "rna_ImagePackedFile_save");
  RNA_def_function_ui_description(func, "Save the packed file to its filepath");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
}

void RNA_api_image(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "save_render", "rna_Image_save_render");
  RNA_def_function_ui_description(func,
                                  "Save image to a specific path using a scenes render settings");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_string_file_path(func, "filepath", NULL, 0, "", "Save path");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_pointer(func, "scene", "Scene", "", "Scene to take image parameters from");

  func = RNA_def_function(srna, "save", "rna_Image_save");
  RNA_def_function_ui_description(func, "Save image to its source path");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);

  func = RNA_def_function(srna, "pack", "rna_Image_pack");
  RNA_def_function_ui_description(func, "Pack an image as embedded data into the .blend file");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_property(func, "data", PROP_STRING, PROP_BYTESTRING);
  RNA_def_property_ui_text(parm, "data", "Raw data (bytes, exact content of the embedded file)");
  RNA_def_int(func,
              "data_len",
              0,
              0,
              INT_MAX,
              "data_len",
              "length of given data (mandatory if data is provided)",
              0,
              INT_MAX);

  func = RNA_def_function(srna, "unpack", "rna_Image_unpack");
  RNA_def_function_ui_description(func, "Save an image packed in the .blend file to disk");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_enum(
      func, "method", rna_enum_unpack_method_items, PF_USE_LOCAL, "method", "How to unpack");

  func = RNA_def_function(srna, "reload", "rna_Image_reload");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Reload the image from its source path");

  func = RNA_def_function(srna, "update", "rna_Image_update");
  RNA_def_function_ui_description(func, "Update the display image from the floating point buffer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);

  func = RNA_def_function(srna, "scale", "rna_Image_scale");
  RNA_def_function_ui_description(func, "Scale the image in pixels");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "width", 1, 1, 10000, "", "Width", 1, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "height", 1, 1, 10000, "", "Height", 1, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "gl_touch", "rna_Image_gl_touch");
  RNA_def_function_ui_description(
      func, "Delay the image from being cleaned from the cache due inactivity");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(
      func, "frame", 0, 0, INT_MAX, "Frame", "Frame of image sequence or movie", 0, INT_MAX);
  /* return value */
  parm = RNA_def_int(
      func, "error", 0, -INT_MAX, INT_MAX, "Error", "OpenGL error value", -INT_MAX, INT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "gl_load", "rna_Image_gl_load");
  RNA_def_function_ui_description(
      func,
      "Load the image into an OpenGL texture. On success, image.bindcode will contain the "
      "OpenGL texture bindcode. Colors read from the texture will be in scene linear color space "
      "and have premultiplied alpha.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(
      func, "frame", 0, 0, INT_MAX, "Frame", "Frame of image sequence or movie", 0, INT_MAX);
  /* return value */
  parm = RNA_def_int(
      func, "error", 0, -INT_MAX, INT_MAX, "Error", "OpenGL error value", -INT_MAX, INT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "gl_free", "rna_Image_gl_free");
  RNA_def_function_ui_description(func, "Free the image from OpenGL graphics memory");

  /* path to an frame specified by image user */
  func = RNA_def_function(srna, "filepath_from_user", "rna_Image_filepath_from_user");
  RNA_def_function_ui_description(
      func,
      "Return the absolute path to the filepath of an image frame specified by the image user");
  RNA_def_pointer(
      func, "image_user", "ImageUser", "", "Image user of the image to get filepath for");
  parm = RNA_def_string_file_path(func,
                                  "filepath",
                                  NULL,
                                  FILE_MAX,
                                  "File Path",
                                  "The resulting filepath from the image and it's user");
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0); /* needed for string return value */
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "buffers_free", "rna_Image_buffers_free");
  RNA_def_function_ui_description(func, "Free the image buffers from memory");

  /* TODO, pack/unpack, maybe should be generic functions? */
}

#endif
