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

from bpy.props import *

class ExportHelper:
	filepath = StringProperty(name="File Path", description="Filepath used for exporting the file", maxlen= 1024, default= "", subtype='FILE_PATH')
	check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, options={'HIDDEN'})

	def invoke(self, context, event):
		import os
		if not self.properties.is_property_set("filepath"):
			self.properties.filepath = os.path.splitext(context.main.filepath)[0] + self.file_extension

		context.manager.add_fileselect(self)
		return {'RUNNING_MODAL'}


class ImportHelper:
	filepath = StringProperty(name="File Path", description="Filepath used for importing the file", maxlen= 1024, default= "", subtype='FILE_PATH')

	def invoke(self, context, event):
		wm = context.manager
		wm.add_fileselect(self)
		return {'RUNNING_MODAL'}


def unpack_list(list_of_tuples):
    flat_list = []
    flat_list_extend = flat_list.extend # a tich faster
    for t in list_of_tuples:
        flat_list_extend(t)
    return l

# same as above except that it adds 0 for triangle faces
def unpack_face_list(list_of_tuples):
    # allocate the entire list
    flat_ls = [0] * (len(list_of_tuples) * 4)
    i = 0

    for t in list_of_tuples:
        if len(t) == 3:
            if t[2] == 0:
                t = t[1], t[2], t[0]
        else: # assuem quad
            if t[3] == 0 or t[2] == 0:
                t = t[2], t[3], t[0], t[1]

        flat_ls[i:i + len(t)] = t
        i += 4
    return flat_ls