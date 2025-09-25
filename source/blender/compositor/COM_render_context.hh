/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <string>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

struct RenderResult;

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * File Output
 *
 * A FileOutput represents an image that will be saved to a file output. The image is internally
 * stored as a RenderResult and saved at the path according to the image format. The image can
 * either be saved as an EXR image or a non-EXR image, specified by the format. This is important
 * because EXR images needs to constructed differently from other image types as will be explained
 * in the following sections.
 *
 * For EXR images, the render result needs to be composed of passes for each layer, so the add_pass
 * method should be called to add each of the passes. Additionally, an empty view should be added
 * for each of the views referenced by the passes, using the single-argument overload of the
 * add_view method. Those views are merely empty structure and does not hold any data aside from
 * the view name. An exception to this rule is stereo EXR images, which needs to have the same
 * structure as non-EXR images as explained in the following section.
 *
 * For non-EXR images, the render result needs to composed of views, so the multi-argument overload
 * of the method add_view should be used to add each view.
 *
 * Color management will be applied on the images if save_as_render_ is true.
 *
 * Meta data can be added using the add_meta_data function. */
class FileOutput {
 private:
  std::string path_;
  ImageFormatData format_;
  RenderResult *render_result_;
  bool save_as_render_;
  Map<std::string, std::string> meta_data_;

 public:
  /* Allocate and initialize the internal render result of the file output using the give
   * parameters. See the implementation for more information. */
  FileOutput(const std::string &path,
             const ImageFormatData &format,
             int2 size,
             bool save_as_render);

  /* Free the internal render result. */
  ~FileOutput();

  /* Add an empty view with the given name. An empty view is just structure and does not hold any
   * data aside from the view name. This should be called for each view referenced by passes. This
   * should only be called for EXR images. */
  void add_view(const char *view_name);

  /* Add a view of the given name that stores the given pixel buffer composed of the given number
   * of channels. */
  void add_view(const char *view_name, int channels, float *buffer);

  /* Add a pass of the given name in the given view that stores the given pixel buffer composed of
   * each of the channels given by the channels string. The channels string should contain a
   * character for each channel in the pixel buffer representing the channel ID. This should only
   * be called for EXR images. The given view name should be the name of an added view using the
   * add_view method. */
  void add_pass(const char *pass_name, const char *view_name, const char *channels, float *buffer);

  /* Add meta data that will eventually be saved to the file if the format supports it. */
  void add_meta_data(std::string key, std::string value);

  /* Save the file to the path along with its meta data, reporting any reports to the standard
   * output. */
  void save(Scene *scene);
};

/* ------------------------------------------------------------------------------------------------
 * Render Context
 *
 * A render context is created by the render pipeline and passed to the compositor to stores data
 * that is specifically related to the rendering process. In particular, since the compositor is
 * executed for each view separately and consecutively, it can be used to store and accumulate
 * data from each of the evaluations of each view, for instance, to save all views in a single file
 * for the File Output node, see the file_outputs_ member for more information. */
class RenderContext {
 public:
  /* True if the render context represents an animation render. */
  bool is_animation_render = false;

 private:
  /* A mapping between file outputs and their image file paths. Those are constructed in the
   * get_file_output method and saved in the save_file_outputs method. See those methods for more
   * information. */
  Map<std::string, std::unique_ptr<FileOutput>> file_outputs_;

 public:
  /* Check if there is an available file output with the given path in the context, if one exists,
   * return it, otherwise, return a newly created one from the given parameters and add it to the
   * context. The arguments are ignored if the file output already exist. This method is typically
   * called in the File Output nodes in the compositor.
   *
   * Since the compositor gets executed multiple times for each view, for single view renders, the
   * file output will be constructed and fully initialized in the same compositor evaluation. For
   * multi-view renders, the file output will be constructed in the evaluation of the first view,
   * and each view will subsequently add its data until the file output is fully initialized in the
   * last view. The render pipeline code will then call the save_file_outputs method after all
   * views were evaluated to write the file outputs. */
  FileOutput &get_file_output(std::string path,
                              ImageFormatData format,
                              int2 size,
                              bool save_as_render);

  /* Write the file outputs that were added to the context. The render pipeline code should call
   * this method after all views were evaluated to write the file outputs. See the get_file_output
   * method for more information. */
  void save_file_outputs(Scene *scene);
};

}  // namespace blender::compositor
