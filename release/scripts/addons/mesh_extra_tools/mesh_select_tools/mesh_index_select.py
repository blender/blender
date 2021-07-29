# gpl author: liero

bl_info = {
    "name": "Select by index",
    "author": "liero",
    "version": (0, 2),
    "blender": (2, 55, 0),
    "location": "View3D > Tool Shelf",
    "description": "Select mesh data by index / area / length / cursor",
    "category": "Mesh",
    }

import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        FloatProperty,
        EnumProperty,
        )


class SelVertEdgeFace(Operator):
    bl_idname = "mesh.select_vert_edge_face_index"
    bl_label = "Select mesh index"
    bl_description = "Select Vertices, Edges, Faces by their indices"
    bl_options = {"REGISTER", "UNDO"}

    select_type = EnumProperty(
            items=[
                   ('VERT', "Vertices", "Select Vertices by index"),
                   ('EDGE', "Edges", "Select Edges by index"),
                   ('FACE', "Faces", "Select Faces by index"),
                   ],
            name="Selection Mode",
            description="",
            default='VERT',
            )
    indice = FloatProperty(
            name="Selected",
            default=0,
            min=0, max=100,
            description="Percentage of selection",
            precision=2,
            subtype="PERCENTAGE"
            )
    delta = BoolProperty(
            name="Use Parameter",
            default=False,
            description="Select by Index / Parameter"
            )
    flip = BoolProperty(
            name="Reverse Order",
            default=False,
            description="Reverse selecting order"
            )
    start_new = BoolProperty(
            name="Fresh Start",
            default=False,
            description="Start from no previous selection\n"
                        "If unchecked the previous selection is kept"
            )
    delta_text = {'VERT': "Use Cursor",
                  'EDGE': "Use Edges' Length",
                  'FACE': "Use Faces' Area"}

    @classmethod
    def poll(cls, context):
        return (context.object is not None and context.object.type == 'MESH')

    def draw(self, context):
        layout = self.layout

        layout.label("Selection Type:")
        layout.prop(self, "select_type", text="")
        layout.separator()

        layout.label("Selected:")
        layout.prop(self, "indice", text="", slider=True)

        d_text = self.delta_text[self.select_type]
        layout.prop(self, "delta", text=d_text)

        layout.prop(self, "flip")
        layout.prop(self, "start_new")

    def execute(self, context):
        obj = bpy.context.object

        if self.start_new:
            bpy.ops.mesh.select_all(action='DESELECT')

        # Selection mode - Vertex, Edge, Face
        if self.select_type == 'VERT':
            bpy.context.tool_settings.mesh_select_mode = [True, False, False]
            ver = obj.data.vertices
            loc = context.scene.cursor_location
            sel = []
            for v in ver:
                d = v.co - loc
                sel.append((d.length, v.index))
            sel.sort(reverse=self.flip)
            bpy.ops.object.mode_set()
            valor = round(len(sel) / 100 * self.indice)
            if self.delta:
                for i in range(len(sel[:valor])):
                    ver[sel[i][1]].select = True
            else:
                for i in range(len(sel[:valor])):
                    if self.flip:
                        ver[len(sel) - i - 1].select = True
                    else:
                        ver[i].select = True

        elif self.select_type == 'EDGE':
            bpy.context.tool_settings.mesh_select_mode = [False, True, False]
            ver = obj.data.vertices
            edg = obj.data.edges
            sel = []
            for e in edg:
                d = ver[e.vertices[0]].co - ver[e.vertices[1]].co
                sel.append((d.length, e.index))
            sel.sort(reverse=self.flip)
            bpy.ops.object.mode_set()
            valor = round(len(sel) / 100 * self.indice)
            if self.delta:
                for i in range(len(sel[:valor])):
                    edg[sel[i][1]].select = True
            else:
                for i in range(len(sel[:valor])):
                    if self.flip:
                        edg[len(sel) - i - 1].select = True
                    else:
                        edg[i].select = True

        elif self.select_type == 'FACE':
            bpy.context.tool_settings.mesh_select_mode = [False, False, True]
            fac = obj.data.polygons
            sel = []
            for f in fac:
                sel.append((f.area, f.index))
            sel.sort(reverse=self.flip)
            bpy.ops.object.mode_set()
            valor = round(len(sel) / 100 * self.indice)
            if self.delta:
                for i in range(len(sel[:valor])):
                    fac[sel[i][1]].select = True
            else:
                for i in range(len(sel[:valor])):
                    if self.flip:
                        fac[len(sel) - i - 1].select = True
                    else:
                        fac[i].select = True

        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


def register():
    bpy.utils.register_class(SelVertEdgeFace)


def unregister():
    bpy.utils.register_class(SelVertEdgeFace)


if __name__ == '__main__':
    register()
