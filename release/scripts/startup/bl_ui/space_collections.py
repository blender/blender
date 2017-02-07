# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Header, Menu


class COLLECTIONS_HT_header(Header):
    bl_space_type = 'COLLECTION_MANAGER'

    def draw(self, context):
        layout = self.layout

        layout.template_header()

        row = layout.row(align=True)
        row.operator("collections.collection_new", text="", icon='NEW')
        row.operator("collections.override_new", text="", icon='LINK_AREA')
        row.operator("collections.collection_link", text="", icon='LINKED')
        row.operator("collections.collection_unlink", text="", icon='UNLINKED')
        row.operator("collections.delete", text="", icon='X')


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
