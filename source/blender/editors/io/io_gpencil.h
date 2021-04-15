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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

#ifndef __IO_GPENCIL_H__
#define __IO_GPENCIL_H__

/** \file
 * \ingroup editor/io
 */

struct ARegion;
struct bContext;
struct View3D;
struct wmOperatorType;

void WM_OT_gpencil_import_svg(struct wmOperatorType *ot);

#ifdef WITH_PUGIXML
void WM_OT_gpencil_export_svg(struct wmOperatorType *ot);
#endif
#ifdef WITH_HARU
void WM_OT_gpencil_export_pdf(struct wmOperatorType *ot);
#endif

struct ARegion *get_invoke_region(struct bContext *C);
struct View3D *get_invoke_view3d(struct bContext *C);

#endif /* __IO_GPENCIL_H__ */
