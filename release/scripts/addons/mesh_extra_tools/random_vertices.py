# gpl authors: Oscurart, Greg

bl_info = {
    "name": "Random Vertices",
    "author": "Oscurart, Greg",
    "version": (1, 3),
    "blender": (2, 6, 3),
    "location": "Object > Transform > Random Vertices",
    "description": "Randomize selected components of active object",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
from bpy.types import Operator
import random
import bmesh
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntVectorProperty,
        )


def add_object(self, context, valmin, valmax, factor, vgfilter):
    # select an option with weight map or not
    mode = bpy.context.active_object.mode
    # generate variables
    objact = bpy.context.active_object
    listver = []
    warn_message = False

    # switch to edit mode
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.mode_set(mode='EDIT')

    # bmesh object
    odata = bmesh.from_edit_mesh(objact.data)
    odata.select_flush(False)

    # if the vertex is selected add to the list
    for vertice in odata.verts[:]:
        if vertice.select:
            listver.append(vertice.index)

    # If the minimum value is greater than the maximum,
    # it adds a value to the maximum
    if valmin[0] >= valmax[0]:
        valmax[0] = valmin[0] + 1

    if valmin[1] >= valmax[1]:
        valmax[1] = valmin[1] + 1

    if valmin[2] >= valmax[2]:
        valmax[2] = valmin[2] + 1

    odata.verts.ensure_lookup_table()

    random_factor = factor
    for vertice in listver:
        odata.verts.ensure_lookup_table()
        if odata.verts[vertice].select:
            if vgfilter is True:
                has_group = getattr(objact.data.vertices[vertice], "groups", None)
                vertex_group = has_group[0] if has_group else None
                vertexweight = getattr(vertex_group, "weight", None)
                if vertexweight:
                    random_factor = factor * vertexweight
                else:
                    random_factor = factor
                    warn_message = True

            odata.verts[vertice].co = (
                (((random.randrange(valmin[0], valmax[0], 1)) * random_factor) / 1000) +
                odata.verts[vertice].co[0],
                (((random.randrange(valmin[1], valmax[1], 1)) * random_factor) / 1000) +
                odata.verts[vertice].co[1],
                (((random.randrange(valmin[2], valmax[2], 1)) * random_factor) / 1000) +
                odata.verts[vertice].co[2]
                )

    if warn_message:
        self.report({'WARNING'},
                    "Some of the Selected Vertices don't have a Group with Vertex Weight assigned")
    bpy.ops.object.mode_set(mode=mode)


class MESH_OT_random_vertices(Operator):
    bl_idname = "mesh.random_vertices"
    bl_label = "Random Vertices"
    bl_description = ("Randomize the location of vertices by a specified\n"
                      "Multiplier Factor and random values in the defined range\n"
                      "or a multiplication of them and the Vertex Weights")
    bl_options = {'REGISTER', 'UNDO'}

    vgfilter = BoolProperty(
            name="Vertex Group",
            description="Use Vertex Weight defined in the Active Group",
            default=False
            )
    factor = FloatProperty(
            name="Factor",
            description="Base Multiplier of the randomization effect",
            default=1
            )
    valmin = IntVectorProperty(
            name="Min XYZ",
            description="Define the minimum range of randomization values",
            default=(0, 0, 0)
            )
    valmax = IntVectorProperty(
            name="Max XYZ",
            description="Define the maximum range of randomization values",
            default=(1, 1, 1)
            )

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == "MESH" and
               context.mode == "EDIT_MESH")

    def execute(self, context):
        add_object(self, context, self.valmin, self.valmax, self.factor, self.vgfilter)

        return {'FINISHED'}


# Registration

def register():
    bpy.utils.register_class(MESH_OT_random_vertices)


def unregister():
    bpy.utils.unregister_class(MESH_OT_random_vertices)


if __name__ == '__main__':
    register()
