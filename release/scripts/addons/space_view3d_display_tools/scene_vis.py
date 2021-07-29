# space_view_3d_display_tools.py Copyright (C) 2014, Jordi Vall-llovera
# Multiple display tools for fast navigate/interact with the viewport

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
# ***** END GPL LICENCE BLOCK *****

"""
Additional links:
    Author Site: http://www.jordiart.com
"""

import bpy


# Set Render Settings
def set_render_settings(context):
    scene = context.scene
    render = scene.render
    render.simplify_subdivision = 0
    render.simplify_shadow_samples = 0
    render.simplify_child_particles = 0
    render.simplify_ao_sss = 0


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
