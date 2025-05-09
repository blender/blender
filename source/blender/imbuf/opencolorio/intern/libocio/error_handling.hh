/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "BLI_string_ref.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

void report_exception(const OCIO_NAMESPACE::Exception &exception);
void report_error(StringRefNull error);

}  // namespace blender::ocio

#endif
