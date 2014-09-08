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
 * limitations under the License
 */

#ifndef __CCL_API_H__
#define __CCL_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/* returns a list of devices for selection, array is empty identifier
 * terminated and must not be freed */

typedef struct CCLDeviceInfo {
	char identifier[128];
	char name[512];
	int value;
} CCLDeviceInfo;

CCLDeviceInfo *CCL_compute_device_list(int device_type);

/* create python module _cycles used by addon */

void *CCL_python_module_init(void);

void CCL_init_logging(const char *argv0);
void CCL_start_debug_logging(void);
void CCL_logging_verbosity_set(int verbosity);

#ifdef __cplusplus
}
#endif

#endif /* __CCL_API_H__ */
