/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_base.h"

CCL_NAMESPACE_BEGIN

class Scene;

void xml_read_file(Scene *scene, const char *filepath);

/* macros for importing */
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0f / M_PI_F))
#define DEG2RADF(_deg) ((_deg) * (float)(M_PI_F / 180.0f))

CCL_NAMESPACE_END
