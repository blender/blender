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

#ifndef __BKE_PROBE_H__
#define __BKE_PROBE_H__

/** \file BKE_probe.h
 *  \ingroup bke
 *  \brief General operations for probes.
 */

struct Main;
struct Probe;

void BKE_probe_init(struct Probe *probe);
void *BKE_probe_add(struct Main *bmain, const char *name);
struct Probe *BKE_probe_copy(struct Main *bmain, struct Probe *probe);
void BKE_probe_make_local(struct Main *bmain, struct Probe *probe, const bool lib_local);
void BKE_probe_free(struct Probe *probe);

#endif /* __BKE_PROBE_H__ */
