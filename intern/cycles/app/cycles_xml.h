/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __CYCLES_XML_H__
#define __CYCLES_XML_H__

CCL_NAMESPACE_BEGIN

class Scene;

void xml_read_file(Scene *scene, const char *filepath);

/* macros for importing */
#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))
#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))

CCL_NAMESPACE_END

#endif /* __CYCLES_XML_H__ */
