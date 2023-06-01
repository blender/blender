/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_register.hh"

#include "node_texture_register.hh"

void register_texture_nodes()
{
  register_node_tree_type_tex();

  register_node_type_tex_group();

  register_node_type_tex_at();
  register_node_type_tex_bricks();
  register_node_type_tex_checker();
  register_node_type_tex_combine_color();
  register_node_type_tex_compose();
  register_node_type_tex_coord();
  register_node_type_tex_curve_rgb();
  register_node_type_tex_curve_time();
  register_node_type_tex_decompose();
  register_node_type_tex_distance();
  register_node_type_tex_hue_sat();
  register_node_type_tex_image();
  register_node_type_tex_invert();
  register_node_type_tex_math();
  register_node_type_tex_mix_rgb();
  register_node_type_tex_output();
  register_node_type_tex_proc_blend();
  register_node_type_tex_proc_clouds();
  register_node_type_tex_proc_distnoise();
  register_node_type_tex_proc_magic();
  register_node_type_tex_proc_marble();
  register_node_type_tex_proc_musgrave();
  register_node_type_tex_proc_noise();
  register_node_type_tex_proc_stucci();
  register_node_type_tex_proc_voronoi();
  register_node_type_tex_proc_wood();
  register_node_type_tex_rgbtobw();
  register_node_type_tex_rotate();
  register_node_type_tex_scale();
  register_node_type_tex_separate_color();
  register_node_type_tex_texture();
  register_node_type_tex_translate();
  register_node_type_tex_valtonor();
  register_node_type_tex_valtorgb();
  register_node_type_tex_viewer();
}
