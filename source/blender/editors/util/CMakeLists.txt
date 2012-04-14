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
	../../blenloader
	../../bmesh
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
	ed_util.c
	editmode_undo.c
	numinput.c
	undo.c
	crazyspace.c

	util_intern.h
	# general includes
	../include/BIF_gl.h
	../include/BIF_glutil.h
	../include/ED_anim_api.h
	../include/ED_armature.h
	../include/ED_clip.h
	../include/ED_curve.h
	../include/ED_datafiles.h
	../include/ED_fileselect.h
	../include/ED_fluidsim.h
	../include/ED_gpencil.h
	../include/ED_image.h
	../include/ED_info.h
	../include/ED_keyframes_draw.h
	../include/ED_keyframes_edit.h
	../include/ED_keyframing.h
	../include/ED_lattice.h
	../include/ED_logic.h
	../include/ED_markers.h
	../include/ED_mball.h
	../include/ED_mesh.h
	../include/ED_node.h
	../include/ED_numinput.h
	../include/ED_object.h
	../include/ED_particle.h
	../include/ED_physics.h
	../include/ED_render.h
	../include/ED_screen.h
	../include/ED_screen_types.h
	../include/ED_sculpt.h
	../include/ED_sequencer.h
	../include/ED_sound.h
	../include/ED_space_api.h
	../include/ED_text.h
	../include/ED_transform.h
	../include/ED_types.h
	../include/ED_util.h
	../include/ED_uvedit.h
	../include/ED_view3d.h
	../include/UI_icons.h
	../include/UI_interface.h
	../include/UI_interface_icons.h
	../include/UI_resources.h
	../include/UI_view2d.h
)

blender_add_lib(bf_editor_util "${SRC}" "${INC}" "${INC_SYS}")
