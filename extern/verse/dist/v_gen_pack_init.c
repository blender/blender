/*
** This is automatically generated source code -- do not edit.
** Changes are affected either by editing the corresponding protocol
** definition file (v_cmd_def_X.c where X=node type), or by editing
** the code generator itself, in v_cmd_gen.c.
*/

#include <stdlib.h>
#include <stdio.h>

#include "v_cmd_gen.h"
#if !defined(V_GENERATE_FUNC_MODE)
#include "verse.h"
#include "v_cmd_buf.h"
#include "v_network_out_que.h"
#include "v_network.h"
#include "v_connection.h"
#include "v_util.h"

#include "v_gen_unpack_func.h"

#include "verse.h"


extern void verse_send_packet_ack(uint32 packet_id);
extern void verse_send_packet_nak(uint32 packet_id);

void init_pack_and_unpack(void)
{
	v_fs_add_func(0, v_unpack_connect, verse_send_connect, NULL);
	v_fs_add_func(1, v_unpack_connect_accept, verse_send_connect_accept, NULL);
	v_fs_add_func(2, v_unpack_connect_terminate, verse_send_connect_terminate, NULL);
	v_fs_add_func(5, v_unpack_ping, verse_send_ping, NULL);
	v_fs_add_func(7, v_unpack_packet_ack, verse_send_packet_ack, NULL);
	v_fs_add_func(8, v_unpack_packet_nak, verse_send_packet_nak, NULL);
	v_fs_add_func(9, v_unpack_node_index_subscribe, verse_send_node_index_subscribe, NULL);
	v_fs_add_func(10, v_unpack_node_create, verse_send_node_create, verse_send_node_destroy);
	v_fs_add_func(11, v_unpack_node_subscribe, verse_send_node_subscribe, verse_send_node_unsubscribe);
	v_fs_add_func(16, v_unpack_tag_group_create, verse_send_tag_group_create, verse_send_tag_group_destroy);
	v_fs_add_func(17, v_unpack_tag_group_subscribe, verse_send_tag_group_subscribe, verse_send_tag_group_unsubscribe);
	v_fs_add_func(18, v_unpack_tag_create, verse_send_tag_create, verse_send_tag_destroy);
	v_fs_add_func(19, v_unpack_node_name_set, verse_send_node_name_set, NULL);
	v_fs_add_func(32, v_unpack_o_transform_pos_real32, verse_send_o_transform_pos_real32, NULL);
	v_fs_add_func(33, v_unpack_o_transform_rot_real32, verse_send_o_transform_rot_real32, NULL);
	v_fs_add_func(34, v_unpack_o_transform_scale_real32, verse_send_o_transform_scale_real32, NULL);
	v_fs_add_func(35, v_unpack_o_transform_pos_real64, verse_send_o_transform_pos_real64, NULL);
	v_fs_add_func(36, v_unpack_o_transform_rot_real64, verse_send_o_transform_rot_real64, NULL);
	v_fs_add_func(37, v_unpack_o_transform_scale_real64, verse_send_o_transform_scale_real64, NULL);
	v_fs_add_func(38, v_unpack_o_transform_subscribe, verse_send_o_transform_subscribe, verse_send_o_transform_unsubscribe);
	v_fs_add_func(39, v_unpack_o_light_set, verse_send_o_light_set, NULL);
	v_fs_add_func(40, v_unpack_o_link_set, verse_send_o_link_set, verse_send_o_link_destroy);
	v_fs_add_func(41, v_unpack_o_method_group_create, verse_send_o_method_group_create, verse_send_o_method_group_destroy);
	v_fs_add_func(42, v_unpack_o_method_group_subscribe, verse_send_o_method_group_subscribe, verse_send_o_method_group_unsubscribe);
	v_fs_add_func(43, v_unpack_o_method_create, verse_send_o_method_create, verse_send_o_method_destroy);
	v_fs_add_func(44, v_unpack_o_method_call, verse_send_o_method_call, NULL);
	v_fs_add_func(45, v_unpack_o_anim_run, verse_send_o_anim_run, NULL);
	v_fs_add_func(46, v_unpack_o_hide, verse_send_o_hide, NULL);
	v_fs_add_func(48, v_unpack_g_layer_create, verse_send_g_layer_create, verse_send_g_layer_destroy);
	v_fs_add_func(49, v_unpack_g_layer_subscribe, verse_send_g_layer_subscribe, verse_send_g_layer_unsubscribe);
	v_fs_add_func(50, v_unpack_g_vertex_set_xyz_real32, verse_send_g_vertex_set_xyz_real32, verse_send_g_vertex_delete_real32);
	v_fs_add_func(51, v_unpack_g_vertex_set_xyz_real64, verse_send_g_vertex_set_xyz_real64, verse_send_g_vertex_delete_real64);
	v_fs_add_func(52, v_unpack_g_vertex_set_uint32, verse_send_g_vertex_set_uint32, NULL);
	v_fs_add_func(53, v_unpack_g_vertex_set_real64, verse_send_g_vertex_set_real64, NULL);
	v_fs_add_func(54, v_unpack_g_vertex_set_real32, verse_send_g_vertex_set_real32, NULL);
	v_fs_add_func(55, v_unpack_g_polygon_set_corner_uint32, verse_send_g_polygon_set_corner_uint32, verse_send_g_polygon_delete);
	v_fs_add_func(56, v_unpack_g_polygon_set_corner_real64, verse_send_g_polygon_set_corner_real64, NULL);
	v_fs_add_func(57, v_unpack_g_polygon_set_corner_real32, verse_send_g_polygon_set_corner_real32, NULL);
	v_fs_add_func(58, v_unpack_g_polygon_set_face_uint8, verse_send_g_polygon_set_face_uint8, NULL);
	v_fs_add_func(59, v_unpack_g_polygon_set_face_uint32, verse_send_g_polygon_set_face_uint32, NULL);
	v_fs_add_func(60, v_unpack_g_polygon_set_face_real64, verse_send_g_polygon_set_face_real64, NULL);
	v_fs_add_func(61, v_unpack_g_polygon_set_face_real32, verse_send_g_polygon_set_face_real32, NULL);
	v_fs_add_func(62, v_unpack_g_crease_set_vertex, verse_send_g_crease_set_vertex, NULL);
	v_fs_add_func(63, v_unpack_g_crease_set_edge, verse_send_g_crease_set_edge, NULL);
	v_fs_add_func(64, v_unpack_g_bone_create, verse_send_g_bone_create, verse_send_g_bone_destroy);
	v_fs_add_func(68, v_unpack_m_fragment_create, verse_send_m_fragment_create, verse_send_m_fragment_destroy);
	v_fs_add_func(80, v_unpack_b_dimensions_set, verse_send_b_dimensions_set, NULL);
	v_fs_add_func(81, v_unpack_b_layer_create, verse_send_b_layer_create, verse_send_b_layer_destroy);
	v_fs_add_func(82, v_unpack_b_layer_subscribe, verse_send_b_layer_subscribe, verse_send_b_layer_unsubscribe);
	v_fs_add_func(83, v_unpack_b_tile_set, verse_send_b_tile_set, NULL);
	v_fs_add_func(96, v_unpack_t_language_set, verse_send_t_language_set, NULL);
	v_fs_add_func(97, v_unpack_t_buffer_create, verse_send_t_buffer_create, verse_send_t_buffer_destroy);
	v_fs_add_func(98, v_unpack_t_buffer_subscribe, verse_send_t_buffer_subscribe, verse_send_t_buffer_unsubscribe);
	v_fs_add_func(99, v_unpack_t_text_set, verse_send_t_text_set, NULL);
	v_fs_add_func(128, v_unpack_c_curve_create, verse_send_c_curve_create, verse_send_c_curve_destroy);
	v_fs_add_func(129, v_unpack_c_curve_subscribe, verse_send_c_curve_subscribe, verse_send_c_curve_unsubscribe);
	v_fs_add_func(130, v_unpack_c_key_set, verse_send_c_key_set, verse_send_c_key_destroy);
	v_fs_add_func(160, v_unpack_a_buffer_create, verse_send_a_buffer_create, verse_send_a_buffer_destroy);
	v_fs_add_func(161, v_unpack_a_buffer_subscribe, verse_send_a_buffer_subscribe, verse_send_a_buffer_unsubscribe);
	v_fs_add_func(162, v_unpack_a_block_set, verse_send_a_block_set, verse_send_a_block_clear);
	v_fs_add_func(163, v_unpack_a_stream_create, verse_send_a_stream_create, verse_send_a_stream_destroy);
	v_fs_add_func(164, v_unpack_a_stream_subscribe, verse_send_a_stream_subscribe, verse_send_a_stream_unsubscribe);
	v_fs_add_func(165, v_unpack_a_stream, verse_send_a_stream, NULL);
}
#endif

