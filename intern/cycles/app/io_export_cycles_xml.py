# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation

# XML exporter for generating test files, not intended for end users

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
            description="Cycles XML export settings",
        )
        cls.filepath = StringProperty(
            name='Filepath',
            description='Filepath for the .xml file',
            maxlen=256,
            default='',
            subtype='FILE_PATH',
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles_xml


# User Interface Drawing Code.


class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"

    @classmethod
    def poll(cls, context):
        return context.engine == 'CYCLES'


class PHYSICS_PT_fluid_export(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Cycles XML Exporter"

    def draw(self, context):
        layout = self.layout

        cycles = context.scene.cycles_xml

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
        uvs = ""
        P = ""

        for v in mesh.vertices:
            P += "%f %f %f  " % (v.co[0], v.co[1], v.co[2])

        verts_and_uvs = zip(mesh.tessfaces, mesh.tessface_uv_textures.active.data)

        for f, uvf in verts_and_uvs:
            vcount = len(f.vertices)
            nverts += str(vcount) + " "

            for v in f.vertices:
                verts += str(v) + " "

            uvs += str(uvf.uv1[0]) + " " + str(uvf.uv1[1]) + " "
            uvs += str(uvf.uv2[0]) + " " + str(uvf.uv2[1]) + " "
            uvs += str(uvf.uv3[0]) + " " + str(uvf.uv3[1]) + " "
            if vcount == 4:
                uvs += " " + str(uvf.uv4[0]) + " " + str(uvf.uv4[1]) + " "

        node = etree.Element(
            'mesh',
            attrib={
                'nverts': nverts.strip(),
                'verts': verts.strip(),
                'P': P,
                'UV': uvs.strip(),
            })

        # write to file
        write(node, filepath)

        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
