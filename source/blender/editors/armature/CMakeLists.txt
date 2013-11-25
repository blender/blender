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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenfont
	../../blenkernel
	../../blenlib
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	armature_add.c
	armature_edit.c
	armature_naming.c
	armature_ops.c
	armature_relations.c
	armature_select.c
	armature_skinning.c
	armature_utils.c
	editarmature_generate.c
	editarmature_retarget.c
	editarmature_sketch.c
	meshlaplacian.c
	pose_edit.c
	pose_lib.c
	pose_group.c
	pose_select.c
	pose_slide.c
	pose_transform.c
	pose_utils.c
	reeb.c

	BIF_generate.h
	BIF_retarget.h
	armature_intern.h
	meshlaplacian.h
	reeb.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENNL)
	add_definitions(-DWITH_OPENNL)
	list(APPEND INC_SYS
		../../../../intern/opennl/extern
	)
endif()

blender_add_lib(bf_editor_armature "${SRC}" "${INC}" "${INC_SYS}")
