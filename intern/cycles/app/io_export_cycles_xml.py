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
import xml.etree.ElementTree as etree
import xml.dom.minidom as dom

import bpy
from bpy_extras.io_utils import ExportHelper
from bpy.props import PointerProperty, StringProperty

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
    
class CyclesXMLSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Scene.cycles_xml = PointerProperty(
                                        type=cls,
                                        name="Cycles XML export Settings",
                                        description="Cycles XML export settings")
        cls.filepath = StringProperty(
                        name='Filepath',
                        description='Filepath for the .xml file',
                        maxlen=256,
                        default='',
                        subtype='FILE_PATH')
                        
    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles_xml
        
# User Interface Drawing Code
class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"

    @classmethod
    def poll(self, context):
        rd = context.scene.render
        return rd.engine == 'CYCLES'


class PHYSICS_PT_fluid_export(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Cycles XML Exporter"

    def draw(self, context):
        layout = self.layout
        
        cycles = context.scene.cycles_xml
        
        #layout.prop(cycles, "filepath")
        layout.operator("export_mesh.cycles_xml")

        
# Export Operator
class ExportCyclesXML(bpy.types.Operator, ExportHelper):
    bl_idname = "export_mesh.cycles_xml"
    bl_label = "Export Cycles XML"

    filename_ext = ".xml"

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None)

    def execute(self, context):
        filepath = bpy.path.ensure_ext(self.filepath, ".xml")

        # get mesh
        scene = context.scene
        object = context.active_object

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
    bpy.utils.register_module(__name__)

def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()

