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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_UTIL_IMBUF_H__
#define __ED_UTIL_IMBUF_H__

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct Main;
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

#endif /* __ED_UTIL_IMBUF_H__ */
