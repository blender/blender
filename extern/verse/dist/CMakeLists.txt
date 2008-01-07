# $Id$
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

SUBDIRS(mkprot_cmd)

SET(SRC_MKPROT_OUT
  v_gen_pack_init.c
  v_gen_pack_a_node.c
  v_gen_pack_b_node.c
  v_gen_pack_c_node.c
  v_gen_pack_g_node.c
  v_gen_pack_m_node.c
  v_gen_pack_o_node.c
  v_gen_pack_s_node.c
  v_gen_pack_t_node.c
)

SET(INC .)

SET(SRC
  v_cmd_buf.c
  v_connect.c
  v_connection.c
  v_encryption.c
  v_func_storage.c
  v_man_pack_node.c
  v_network.c
  v_network_in_que.c
  v_network_out_que.c
  v_pack.c
  v_pack_method.c
  v_prime.c
  v_randgen.c
  v_util.c
  v_bignum.c
  verse_ms.c
  ${SRC_MKPROT_OUT}
)

BLENDERLIB(verse "${SRC}" "${INC}")
ADD_DEPENDENCIES(verse mkprot)
#verselib = env.BlenderLib(libname='verse', sources=lib_source_files, includes=[], defines=defines, libtype=['core', 'intern'], priority = [5, 5])

SET(SRC_VERSE
  vs_connection.c
  vs_main.c
  vs_node_audio.c
  vs_node_bitmap.c
  vs_node_curve.c
  vs_node_geometry.c
  vs_node_head.c
  vs_node_material.c
  vs_node_object.c
  vs_node_particle.c
  vs_node_storage.c
  vs_node_text.c
  vs_master.c  
)

ADD_EXECUTABLE(verse_server ${SRC_VERSE})
IF(WIN32)
TARGET_LINK_LIBRARIES(verse_server verse ws2_32)
ELSE(WIN32)
TARGET_LINK_LIBRARIES(verse_server verse)
ENDIF(WIN32)
ADD_DEPENDENCIES(verse_server mkprot)
MESSAGE(STATUS "Configuring verse_server")
