/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

void register_nodes();

void register_node_type_frame();
void register_node_type_reroute();

void register_node_type_group_input();
void register_node_type_group_output();

void register_composite_nodes();
void register_function_nodes();
void register_geometry_nodes();
void register_shader_nodes();
void register_texture_nodes();

/**
 * This macro has three purposes:
 * - It serves as marker in source code that `discover_nodes.py` can search for to find nodes that
 *   need to be registered. This script generates code that calls the register functions of all
 *   nodes.
 * - It creates a non-static wrapper function for the registration function that is then called by
 *   the generated code. This wrapper is necessary because the normal registration is static and
 *   can't be called from somewhere else. It could be made non-static, but then it would require
 *   a declaration to avoid warnings.
 * - It reduces the amount of "magic" with how node registration works. The script could also
 *   search for `node_register` functions directly, but then it would not be apparent in the code
 *   that anything unusual is going on.
 */
#define NOD_REGISTER_NODE(REGISTER_FUNC) \
  void REGISTER_FUNC##_discover(); \
  void REGISTER_FUNC##_discover() \
  { \
    REGISTER_FUNC(); \
  }
