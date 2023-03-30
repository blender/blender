/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct bContext;
struct wmEvent;
struct wmOperator;

/* ed_util_imbuf.c */

void ED_imbuf_sample_draw(const struct bContext *C, struct ARegion *region, void *arg_info);
void ED_imbuf_sample_exit(struct bContext *C, struct wmOperator *op);
int ED_imbuf_sample_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int ED_imbuf_sample_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
void ED_imbuf_sample_cancel(struct bContext *C, struct wmOperator *op);
bool ED_imbuf_sample_poll(struct bContext *C);

#ifdef __cplusplus
}
#endif
