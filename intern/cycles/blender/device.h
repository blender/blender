/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "RNA_blender_cpp.hh"

#include "device/device.h"

CCL_NAMESPACE_BEGIN

/* Get number of threads to use for rendering. */
int blender_device_threads(BL::Scene &b_scene);

/* Convert Blender settings to device specification. In addition, preferences_device contains the
 * device chosen in Cycles global preferences, which is useful for the denoiser device selection.
 */
DeviceInfo blender_device_info(BL::Preferences &b_preferences,
                               BL::Scene &b_scene,
                               bool background,
                               bool preview,
                               DeviceInfo &preferences_device);

CCL_NAMESPACE_END
