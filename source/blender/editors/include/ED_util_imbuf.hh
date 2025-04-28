/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ARegion;
struct bContext;
struct wmEvent;
struct wmOperator;

/* `ed_util_imbuf.cc` */

void ED_imbuf_sample_draw(const bContext *C, ARegion *region, void *arg_info);
void ED_imbuf_sample_exit(bContext *C, wmOperator *op);
wmOperatorStatus ED_imbuf_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event);
wmOperatorStatus ED_imbuf_sample_modal(bContext *C, wmOperator *op, const wmEvent *event);
void ED_imbuf_sample_cancel(bContext *C, wmOperator *op);
bool ED_imbuf_sample_poll(bContext *C);
