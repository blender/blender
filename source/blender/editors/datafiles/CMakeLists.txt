# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public LicenseS
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

)

set(INC_SYS

)

# blender and player
set(SRC

)

data_to_c_simple(../../../../release/datafiles/bfont.pfb SRC)
data_to_c_simple(../../../../release/datafiles/bfont.ttf SRC)

if(WITH_BLENDER)
	# blender only (not player)
	data_to_c_simple(../../../../release/datafiles/bmonofont.ttf SRC)

	if(NOT WITH_HEADLESS)
		# blender UI only

		# blends
		data_to_c_simple(../../../../release/datafiles/preview.blend SRC)
		data_to_c_simple(../../../../release/datafiles/preview_cycles.blend SRC)

		# images
		data_to_c_simple(../../../../release/datafiles/splash.png SRC)
		data_to_c_simple(../../../../release/datafiles/splash_2x.png SRC)
		# XXX These are handy, but give nasty "false changes" in svn :/
		#svg_to_png(../../../../release/datafiles/blender_icons.svg
		           #../../../../release/datafiles/blender_icons16.png
		           #90 SRC)

		data_to_c_simple_icons(../../../../release/datafiles/blender_icons16 SRC)
		#data_to_c_simple(../../../../release/datafiles/blender_icons16.png SRC)
		#svg_to_png(../../../../release/datafiles/blender_icons.svg
		           #../../../../release/datafiles/blender_icons32.png
		           #180 SRC)
		data_to_c_simple_icons(../../../../release/datafiles/blender_icons32 SRC)
		#data_to_c_simple(../../../../release/datafiles/blender_icons32.png SRC)
		#svg_to_png(../../../../release/datafiles/prvicons.svg
		           #../../../../release/datafiles/prvicons.png
		           #90 SRC)
		data_to_c_simple(../../../../release/datafiles/prvicons.png SRC)

		# brushes
		data_to_c_simple(../../../../release/datafiles/brushicons/add.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/blob.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/blur.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/clay.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/claystrips.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/clone.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/crease.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/darken.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/draw.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/fill.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/flatten.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/grab.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/inflate.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/layer.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/lighten.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/mask.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/mix.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/multiply.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/nudge.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/pinch.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/scrape.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/smear.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/smooth.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/snake_hook.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/soften.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/subtract.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/texdraw.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/thumb.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/twist.png SRC)
		data_to_c_simple(../../../../release/datafiles/brushicons/vertexdraw.png SRC)

		# matcap
		data_to_c_simple(../../../../release/datafiles/matcaps/mc01.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc02.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc03.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc04.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc05.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc06.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc07.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc08.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc09.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc10.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc11.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc12.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc13.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc14.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc15.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc16.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc17.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc18.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc19.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc20.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc21.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc22.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc23.jpg SRC)
		data_to_c_simple(../../../../release/datafiles/matcaps/mc24.jpg SRC)

	endif()

	data_to_c_simple(../../../../release/datafiles/startup.blend SRC)
endif()

blender_add_lib(bf_editor_datafiles "${SRC}" "${INC}" "${INC_SYS}")
