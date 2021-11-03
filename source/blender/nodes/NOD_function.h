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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void register_node_type_fn_legacy_random_float(void);

void register_node_type_fn_align_euler_to_vector(void);
void register_node_type_fn_boolean_math(void);
void register_node_type_fn_float_compare(void);
void register_node_type_fn_float_to_int(void);
void register_node_type_fn_input_bool(void);
void register_node_type_fn_input_color(void);
void register_node_type_fn_input_int(void);
void register_node_type_fn_input_special_characters(void);
void register_node_type_fn_input_string(void);
void register_node_type_fn_input_vector(void);
void register_node_type_fn_random_value(void);
void register_node_type_fn_replace_string(void);
void register_node_type_fn_rotate_euler(void);
void register_node_type_fn_slice_string(void);
void register_node_type_fn_string_length(void);
void register_node_type_fn_value_to_string(void);

#ifdef __cplusplus
}
#endif
