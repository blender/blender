/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void register_node_type_fn_align_euler_to_vector(void);
void register_node_type_fn_boolean_math(void);
void register_node_type_fn_compare(void);
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
