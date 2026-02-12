/* SPDX-FileCopyrightText: 2025 OpenImageIO project
 * SPDX-FileCopyrightText: 2026 Blender Authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * This is a modified version of maketexture.cpp from OpenImageIO, to add a few
 * features missing in the native implementation. */

#pragma once

#include <cstdlib>
#include <string>

#include "util/param.h"
#include "util/string.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

#define TX_FILE_FORMAT_VERSION 0

bool resolve_tx(const string &filepath,
                const string &texture_cache_path,
                ustring colorspace,
                const ImageAlphaType alpha_type,
                const ImageFormatType format_type,
                std::string &out_filepath);

bool make_tx(const string &filepath,
             const string &out_filepath,
             ustring colorspace,
             const ImageAlphaType alpha_type,
             const ImageFormatType format_type);

CCL_NAMESPACE_END
