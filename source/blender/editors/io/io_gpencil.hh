/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct ARegion;
struct View3D;
struct bContext;
struct wmOperatorType;

void WM_OT_gpencil_import_svg(wmOperatorType *ot);

#ifdef WITH_PUGIXML
void WM_OT_gpencil_export_svg(wmOperatorType *ot);
#endif
#ifdef WITH_HARU
void WM_OT_gpencil_export_pdf(wmOperatorType *ot);
#endif

ARegion *get_invoke_region(bContext *C);
View3D *get_invoke_view3d(bContext *C);
