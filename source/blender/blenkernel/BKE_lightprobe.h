/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_LIGHTPROBE_H__
#define __BKE_LIGHTPROBE_H__

/** \file BKE_lightprobe.h
 *  \ingroup bke
 *  \brief General operations for probes.
 */

struct Main;
struct LightProbe;

void BKE_lightprobe_init(struct LightProbe *probe);
void *BKE_lightprobe_add(struct Main *bmain, const char *name);
void BKE_lightprobe_copy_data(struct Main *bmain, struct LightProbe *probe_dst, const struct LightProbe *probe_src, const int flag);
struct LightProbe *BKE_lightprobe_copy(struct Main *bmain, const struct LightProbe *probe);
void BKE_lightprobe_make_local(struct Main *bmain, struct LightProbe *probe, const bool lib_local);
void BKE_lightprobe_free(struct LightProbe *probe);

#endif /* __BKE_LIGHTPROBE_H__ */
