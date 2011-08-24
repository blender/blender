#
# Copyright 2011, Blender Foundation.
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

# XML exporter for generating test files, not intended for end users

import os
import bpy
from bpy_extras.io_utils import ExportHelper
import xml.etree.ElementTree as etree
import xml.dom.minidom as dom

def strip(root):
	root.text = None
	root.tail = None

	for elem in root:
		strip(elem)

def write(node, fname):
	strip(node)

	s = etree.tostring(node)
	s = dom.parseString(s).toprettyxml()

	f = open(fname, "w")
	f.write(s)

class ExportCyclesXML(bpy.types.Operator, ExportHelper):
	''''''
	bl_idname = "export_mesh.cycles_xml"
	bl_label = "Export Cycles XML"

	filename_ext = ".xml"

	@classmethod
	def poll(cls, context):
		return context.active_object != None

	def execute(self, context):
		filepath = bpy.path.ensure_ext(self.filepath, ".xml")

		# get mesh
		scene = context.scene
		object = context.object

		if not object:
			raise Exception("No active object")

		mesh = object.to_mesh(scene, True, 'PREVIEW')

		if not mesh:
			raise Exception("No mesh data in active object")

		# generate mesh node
		nverts = ""
		verts = ""
		P = ""

		for v in mesh.vertices:
			P += "%f %f %f  " % (v.co[0], v.co[1], v.co[2])

		for i, f in enumerate(mesh.faces):
			nverts += str(len(f.vertices)) + " "

			for v in f.vertices:
				verts += str(v) + " "
			verts += " "

		node = etree.Element('mesh', attrib={'nverts': nverts, 'verts': verts, 'P': P})
		
		# write to file
		write(node, filepath)

		return {'FINISHED'}

def register():
	pass

def unregister():
	pass

if __name__ == "__main__":
	register()

