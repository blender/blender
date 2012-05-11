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
	Bfont.c
	bfont.ttf.c
)

if(WITH_BLENDER)
	# blender only
	list(APPEND SRC
		startup.blend.c
		bmonofont.ttf.c
	)

	if(NOT WITH_HEADLESS)
		# blender UI only
		list(APPEND SRC
			# blends
			preview.blend.c

			# images
			splash.png.c
			blender_icons.png.c

			# brushes
			add.png.c
			blob.png.c
			blur.png.c
			clay.png.c
			clone.png.c
			crease.png.c
			darken.png.c
			draw.png.c
			fill.png.c
			flatten.png.c
			grab.png.c
			inflate.png.c
			layer.png.c
			lighten.png.c
			mask.png.c
			mix.png.c
			multiply.png.c
			nudge.png.c
			pinch.png.c
			prvicons.c
			scrape.png.c
			smear.png.c
			smooth.png.c
			snake_hook.png.c
			soften.png.c
			claystrips.png.c
			subtract.png.c
			texdraw.png.c
			thumb.png.c
			twist.png.c
			vertexdraw.png.c
		)
	endif()
endif()

blender_add_lib(bf_editor_datafiles "${SRC}" "${INC}" "${INC_SYS}")
