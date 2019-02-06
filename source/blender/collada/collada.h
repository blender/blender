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
 */

/** \file \ingroup collada
 */

#ifndef __COLLADA_H__
#define __COLLADA_H__

#include <stdlib.h>

#include "ImportSettings.h"
#include "ExportSettings.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "RNA_types.h"

struct Depsgraph;
struct Scene;
struct ViewLayer;
struct bContext;

/*
 * both return 1 on success, 0 on error
 */
int collada_import(struct bContext *C,
				   ImportSettings *import_settings);

int collada_export(struct bContext *C,
                   ExportSettings *export_settings);

#ifdef __cplusplus
}
#endif

#endif
