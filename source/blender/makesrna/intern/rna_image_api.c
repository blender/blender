/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "DNA_packedFile_types.h"

#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_packedFile.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_image.h"
#  include "BKE_image_format.h"
#  include "BKE_image_save.h"
#  include "BKE_main.h"
#  include "BKE_scene.h"
#  include <errno.h>

#  include "IMB_colormanagement.h"
#  include "IMB_imbuf.h"

#  include "DNA_image_types.h"
#  include "DNA_scene_types.h"

#  include "MEM_guardedalloc.h"

static void rna_ImagePackedFile_save(ImagePackedFile *imapf, Main *bmain, ReportList *reports)
{
  if (BKE_packedfile_write_to_file(
          reports, BKE_main_blendfile_path(bmain), imapf->filepath, imapf->packedfile, 0) !=
      RET_OK)
  {
    BKE_reportf(reports, RPT_ERROR, "Could not save packed file to disk as '%s'", imapf->filepath);
  }
}

static void rna_Image_save_render(Image *image,
                                  bContext *C,
                                  ReportList *reports,
                                  const char *path,
                                  Scene *scene,
                                  const int quality)
{
  Main *bmain = CTX_data_main(C);

  if (scene == NULL) {
    scene = CTX_data_scene(C);
  }

  ImageSaveOptions opts;

  if (BKE_image_save_options_init(&opts, bmain, scene, image, NULL, false, true)) {
    opts.save_copy = true;
    STRNCPY(opts.filepath, path);
    if (quality != 0) {
      opts.im_format.quality = clamp_i(quality, 0, 100);
    }

    if (!BKE_image_save(reports, bmain, image, NULL, &opts)) {
      BKE_reportf(
          reports, RPT_ERROR, "Image '%s' could not be saved to '%s'", image->id.name + 2, path);
    }
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
  }

  BKE_image_save_options_free(&opts);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, image);
}

static void rna_Image_save(Image *image,
                           Main *bmain,
                           bContext *C,
                           ReportList *reports,
                           const char *path,
                           const int quality)
{
  Scene *scene = CTX_data_scene(C);
  ImageSaveOptions opts;

  if (BKE_image_save_options_init(&opts, bmain, scene, image, NULL, false, false)) {
    if (path && path[0]) {
      STRNCPY(opts.filepath, path);
    }
    if (quality != 0) {
      opts.im_format.quality = clamp_i(quality, 0, 100);
    }
    if (!BKE_image_save(reports, bmain, image, NULL, &opts)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Image '%s' could not be saved to '%s'",
                  image->id.name + 2,
                  image->filepath);
    }
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
  }

  BKE_image_save_options_free(&opts);

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
  else if (ELEM(image->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
    BKE_report(reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
    return;
  }
  else {
    /* reports its own error on failure */
    BKE_packedfile_unpack_image(bmain, reports, image, method);
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

  if (ibuf->byte_buffer.data) {
    IMB_rect_from_float(ibuf);
  }

  ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  BKE_image_partial_update_mark_full_update(image);

  BKE_image_release_ibuf(image, ibuf, NULL);
}

static void rna_Image_scale(Image *image, ReportList *reports, int width, int height)
{
  if (!BKE_image_scale(image, width, height)) {
    BKE_reportf(reports, RPT_ERROR, "Image '%s' does not have any image data", image->id.name + 2);
    return;
  }
  BKE_image_partial_update_mark_full_update(image);
  WM_main_add_notifier(NC_IMAGE | NA_EDITED, image);
}

static int rna_Image_gl_load(
    Image *image, ReportList *reports, int frame, int layer_index, int pass_index)
{
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  iuser.framenr = frame;
  iuser.layer = layer_index;
  iuser.pass = pass_index;
  if (image->rr != NULL) {
    BKE_image_multilayer_index(image->rr, &iuser);
  }

  GPUTexture *tex = BKE_image_get_gpu_texture(image, &iuser, NULL);

  if (tex == NULL) {
    BKE_reportf(reports, RPT_ERROR, "Failed to load image texture '%s'", image->id.name + 2);
    /* TODO(fclem): this error code makes no sense for vulkan. */
    return 0x0502; /* GL_INVALID_OPERATION */
  }

  return 0; /* GL_NO_ERROR */
}

static int rna_Image_gl_touch(
    Image *image, ReportList *reports, int frame, int layer_index, int pass_index)
{
  int error = 0; /* GL_NO_ERROR */

  BKE_image_tag_time(image);

  if (image->gputexture[TEXTARGET_2D][0] == NULL) {
    error = rna_Image_gl_load(image, reports, frame, layer_index, pass_index);
  }

  return error;
}

static void rna_Image_gl_free(Image *image)
{
  BKE_image_free_gputextures(image);

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
  parm = RNA_def_string_file_path(func, "filepath", NULL, 0, "", "Output path");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_pointer(func, "scene", "Scene", "", "Scene to take image parameters from");
  RNA_def_int(func,
              "quality",
              0,
              0,
              100,
              "Quality",
              "Quality for image formats that support lossy compression, uses default quality if "
              "not specified",
              0,
              100);

  func = RNA_def_function(srna, "save", "rna_Image_save");
  RNA_def_function_ui_description(func, "Save image");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_string_file_path(func,
                           "filepath",
                           NULL,
                           0,
                           "",
                           "Output path, uses image data-block filepath if not specified");
  RNA_def_int(func,
              "quality",
              0,
              0,
              100,
              "Quality",
              "Quality for image formats that support lossy compression, uses default quality if "
              "not specified",
              0,
              100);

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
  RNA_def_function_ui_description(func, "Update the display image from the floating-point buffer");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);

  func = RNA_def_function(srna, "scale", "rna_Image_scale");
  RNA_def_function_ui_description(func, "Scale the buffer of the image, in pixels");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "width", 1, 1, INT_MAX, "", "Width", 1, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "height", 1, 1, INT_MAX, "", "Height", 1, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "gl_touch", "rna_Image_gl_touch");
  RNA_def_function_ui_description(
      func, "Delay the image from being cleaned from the cache due inactivity");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(
      func, "frame", 0, 0, INT_MAX, "Frame", "Frame of image sequence or movie", 0, INT_MAX);
  RNA_def_int(func,
              "layer_index",
              0,
              0,
              INT_MAX,
              "Layer",
              "Index of layer that should be loaded",
              0,
              INT_MAX);
  RNA_def_int(func,
              "pass_index",
              0,
              0,
              INT_MAX,
              "Pass",
              "Index of pass that should be loaded",
              0,
              INT_MAX);
  /* return value */
  parm = RNA_def_int(
      func, "error", 0, -INT_MAX, INT_MAX, "Error", "OpenGL error value", -INT_MAX, INT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "gl_load", "rna_Image_gl_load");
  RNA_def_function_ui_description(
      func,
      "Load the image into an OpenGL texture. On success, image.bindcode will contain the "
      "OpenGL texture bindcode. Colors read from the texture will be in scene linear color space "
      "and have premultiplied or straight alpha matching the image alpha mode");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_int(
      func, "frame", 0, 0, INT_MAX, "Frame", "Frame of image sequence or movie", 0, INT_MAX);
  RNA_def_int(func,
              "layer_index",
              0,
              0,
              INT_MAX,
              "Layer",
              "Index of layer that should be loaded",
              0,
              INT_MAX);
  RNA_def_int(func,
              "pass_index",
              0,
              0,
              INT_MAX,
              "Pass",
              "Index of pass that should be loaded",
              0,
              INT_MAX);
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
                                  "The resulting filepath from the image and its user");
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0); /* needed for string return value */
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "buffers_free", "rna_Image_buffers_free");
  RNA_def_function_ui_description(func, "Free the image buffers from memory");

  /* TODO: pack/unpack, maybe should be generic functions? */
}

#endif
