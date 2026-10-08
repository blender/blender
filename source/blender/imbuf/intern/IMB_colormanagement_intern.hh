/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include <optional>
#include <string>

namespace blender {

namespace ocio {
class ColorSpace;
class CPUProcessor;
}  // namespace ocio

using ColorSpace = ocio::ColorSpace;

struct ImBuf;
struct Main;
enum class ColorManagedFileOutput;

#define MAX_COLORSPACE_NAME 64

/* ** Initialization / De-initialization ** */

void colormanagement_init();
void colormanagement_exit();

void colormanage_environment_setup_for_test(std::optional<std::string> blender_ocio_env,
                                            std::optional<std::string> ocio_env);

bool colormanage_config_reload(Main *bmain);

const ColorSpace *colormanage_colorspace_get_named(const char *name);
const ColorSpace *colormanage_colorspace_get_roled(int role);

void colormanage_imbuf_set_default_spaces(ImBuf *ibuf);
void colormanage_imbuf_make_linear(ImBuf *ibuf,
                                   const char *from_colorspace,
                                   ColorManagedFileOutput output);

}  // namespace blender
