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

/**
 * \file
 * \ingroup pygen
 */

#pragma once

#include "BLI_sys_types.h"

struct EnumPropertyItem;

char *BPy_enum_as_string(const struct EnumPropertyItem *item);

int pyrna_enum_value_from_id(const struct EnumPropertyItem *item,
                             const char *identifier,
                             int *value,
                             const char *error_prefix);

/**
 * Data for #pyrna_enum_value_parse_string & #pyrna_enum_bitfield_parse_set parsing utilities.
 * Use with #PyArg_ParseTuple's `O&` formatting.
 */
struct BPy_EnumProperty_Parse {
  const struct EnumPropertyItem *items;
  /**
   * Set when the value was successfully parsed.
   * Useful if the input ever needs to be included in an error message.
   * (if the value is not supported under certain conditions).
   */
  PyObject *value_orig;

  int value;
  bool is_set;
};
int pyrna_enum_value_parse_string(PyObject *o, void *p);
int pyrna_enum_bitfield_parse_set(PyObject *o, void *p);

unsigned int *pyrna_set_to_enum_bitmap(const struct EnumPropertyItem *items,
                                       PyObject *value,
                                       int type_size,
                                       bool type_convert_sign,
                                       int bitmap_size,
                                       const char *error_prefix);

int pyrna_set_to_enum_bitfield(const struct EnumPropertyItem *items,
                               PyObject *value,
                               int *r_value,
                               const char *error_prefix);

PyObject *pyrna_enum_bitfield_to_py(const struct EnumPropertyItem *items, int value);
