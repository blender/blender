/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct ARegion;
struct View3D;
struct bContext;
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
