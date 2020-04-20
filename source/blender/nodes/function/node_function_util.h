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

#ifndef __NODE_FUNCTION_UTIL_H__
#define __NODE_FUNCTION_UTIL_H__

#include <string.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "NOD_function.h"

#include "node_util.h"

void fn_node_type_base(
    struct bNodeType *ntype, int type, const char *name, short nclass, short flag);
bool fn_node_poll_default(struct bNodeType *ntype, struct bNodeTree *ntree);

#endif /* __NODE_FUNCTION_UTIL_H__ */
