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

/** \file
 * \ingroup edfile
 */
#pragma once

#include "ED_file_indexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Default indexer to use when listing files. The implementation is a no-operation indexing. When
 * set it won't use indexing. It is added to increase the code clarity.
 */
extern const FileIndexerType file_indexer_noop;

#ifdef __cplusplus
}
#endif
