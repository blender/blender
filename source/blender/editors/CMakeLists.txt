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

if(WITH_BLENDER)
	add_subdirectory(animation)
	add_subdirectory(armature)
	add_subdirectory(curve)
	add_subdirectory(gpencil)
	add_subdirectory(interface)
	add_subdirectory(mask)
	add_subdirectory(mesh)
	add_subdirectory(metaball)
	add_subdirectory(object)
	add_subdirectory(physics)
	add_subdirectory(render)
	add_subdirectory(screen)
	add_subdirectory(sculpt_paint)
	add_subdirectory(sound)
	add_subdirectory(space_action)
	add_subdirectory(space_api)
	add_subdirectory(space_buttons)
	add_subdirectory(space_clip)
	add_subdirectory(space_console)
	add_subdirectory(space_file)
	add_subdirectory(space_graph)
	add_subdirectory(space_image)
	add_subdirectory(space_info)
	add_subdirectory(space_logic)
	add_subdirectory(space_nla)
	add_subdirectory(space_node)
	add_subdirectory(space_outliner)
	add_subdirectory(space_script)
	add_subdirectory(space_sequencer)
	add_subdirectory(space_text)
	add_subdirectory(space_time)
	add_subdirectory(space_userpref)
	add_subdirectory(space_view3d)
	add_subdirectory(transform)
	add_subdirectory(util)
	add_subdirectory(uvedit)
endif()

add_subdirectory(datafiles)
