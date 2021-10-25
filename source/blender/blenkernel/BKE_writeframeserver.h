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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_WRITEFRAMESERVER_H__
#define __BKE_WRITEFRAMESERVER_H__

/** \file BKE_writeframeserver.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct RenderData;	
struct ReportList;
struct Scene;

int BKE_frameserver_start(
        void *context_v, struct Scene *scene, struct RenderData *rd, int rectx, int recty,
        struct ReportList *reports, bool preview, const char *suffix);
void BKE_frameserver_end(void *context_v);
int BKE_frameserver_append(
        void *context_v, struct RenderData *rd, int start_frame, int frame, int *pixels,
        int rectx, int recty, const char *suffix, struct ReportList *reports);
int BKE_frameserver_loop(void *context_v, struct RenderData *rd, struct ReportList *reports);
void *BKE_frameserver_context_create(void);
void BKE_frameserver_context_free(void *context_v);

#ifdef __cplusplus
}
#endif

#endif

