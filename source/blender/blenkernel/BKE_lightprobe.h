/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */

#ifndef __BKE_LIGHTPROBE_H__
#define __BKE_LIGHTPROBE_H__

/** \file
 * \ingroup bke
 * \brief General operations for probes.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct LightProbe;
struct Main;

void BKE_lightprobe_type_set(struct LightProbe *probe, const short lightprobe_type);
void *BKE_lightprobe_add(struct Main *bmain, const char *name);
struct LightProbe *BKE_lightprobe_copy(struct Main *bmain, const struct LightProbe *probe);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_LIGHTPROBE_H__ */
