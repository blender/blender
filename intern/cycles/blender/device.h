/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BLENDER_DEVICE_H__
#define __BLENDER_DEVICE_H__

#include "MEM_guardedalloc.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"
#include "RNA_types.h"

#include "device/device.h"

CCL_NAMESPACE_BEGIN

/* Get number of threads to use for rendering. */
int blender_device_threads(BL::Scene &b_scene);

/* Convert Blender settings to device specification. */
DeviceInfo blender_device_info(BL::Preferences &b_preferences,
                               BL::Scene &b_scene,
                               bool background);

CCL_NAMESPACE_END

#endif /* __BLENDER_DEVICE_H__ */
