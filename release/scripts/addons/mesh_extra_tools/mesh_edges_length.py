# gpl author: Giuseppe De Marco [BlenderLab] inspired by NirenYang

bl_info = {
    "name": "Set edges length",
    "description": "Edges length",
    "author": "Giuseppe De Marco [BlenderLab] inspired by NirenYang",
    "version": (0, 1, 0),
    "blender": (2, 7, 1),
    "location": "Toolbar > Tools > Mesh Tools: set Length(Shit+Alt+E)",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh",
    }

import bpy
import bmesh
from mathutils import Vector
from bpy.types import Operator
from bpy.props import (
        FloatProperty,
        EnumProperty,
        )

# GLOBALS
edge_length_debug = False
_error_message = "Please select at least one edge to fill select history"
_error_message_2 = "Edges with shared vertices are not allowed. Please, use scale instead"

# Note : Refactor - removed all the operators apart from LengthSet
#        and merged the other ones as options of length (lijenstina)


def get_edge_vector(edge):
    verts = (edge.verts[0].co, edge.verts[1].co)
    vector = verts[1] - verts[0]

    return vector


def get_selected(bmesh_obj, geometry_type):
    # geometry type should be edges, verts or faces
    selected = []

    for i in getattr(bmesh_obj, geometry_type):
        if i.select:
            selected.append(i)
    return tuple(selected)


def get_center_vector(verts):
    # verts = [Vector((x,y,z)), Vector((x,y,z))]

    center_vector = Vector((((verts[1][0] + verts[0][0]) / 2.),
                           ((verts[1][1] + verts[0][1]) / 2.),
                           ((verts[1][2] + verts[0][2]) / 2.)))
    return center_vector


class LengthSet(Operator):
    bl_idname = "object.mesh_edge_length_set"
    bl_label = "Set edge length"
    bl_description = ("Change one selected edge length by a specified target,\n"
                      "existing lenght and different modes\n"
                      "Note: works only with Edges that not share a vertex")
    bl_options = {'REGISTER', 'UNDO'}

    old_length = FloatProperty(
            name="Original length",
            options={'HIDDEN'},
            )
    set_lenght_type = EnumProperty(
            items=[
                ('manual', "Manual",
                 "Input manually the desired Target Lenght"),
                ('existing', "Existing Lenght",
                 "Use existing geometry Edges' characteristics"),
            ],
            name="Set Type of Input",
            )
    target_length = FloatProperty(
            name="Target Length",
            description="Input a value for an Edges Lenght target",
            default=1.00,
            unit='LENGTH',
            precision=5
            )
    existing_lenght = EnumProperty(
            items=[
                ('min', "Shortest",
                 "Set all to shortest Edge of selection"),
                ('max', "Longest",
                 "Set all to the longest Edge of selection"),
                ('average', "Average",
                 "Set all to the average Edge lenght of selection"),
                ('active', "Active",
                 "Set all to the active Edge's one\n"
                 "Needs a selection to be done in Edge Select mode"),
            ],
            name="Existing lenght"
            )
    mode = EnumProperty(
            items=[
                ('fixed', "Fixed", "Fixed"),
                ('increment', "Increment", "Increment"),
                ('decrement', "Decrement", "Decrement"),
            ],
            name="Mode"
            )
    behaviour = EnumProperty(
            items=[
                ('proportional', "Proportional",
                 "Move vertex locations proportionally to the center of the Edge"),
                ('clockwise', "Clockwise",
                "Compute the Edges' vertex locations in a clockwise fashion"),
                ('unclockwise', "Counterclockwise",
                "Compute the Edges' vertex locations in a counterclockwise fashion"),
            ],
            name="Resize behavior"
            )

    originary_edge_length_dict = {}
    edge_lenghts = []
    selected_edges = ()

    @classmethod
    def poll(cls, context):
        return (context.edit_object and context.object.type == 'MESH')

    def check(self, context):
        return True

    def draw(self, context):
        layout = self.layout

        layout.label("Original Active lenght is: {:.3f}".format(self.old_length))

        layout.label("Input Mode:")
        layout.prop(self, "set_lenght_type", expand=True)
        if self.set_lenght_type == 'manual':
            layout.prop(self, "target_length")
        else:
            layout.prop(self, "existing_lenght", text="")

        layout.label("Mode:")
        layout.prop(self, "mode", text="")

        layout.label("Resize Behavior:")
        layout.prop(self, "behaviour", text="")

    def get_existing_edge_lenght(self, bm):
        if self.existing_lenght != "active":
            if self.existing_lenght == "min":
                return min(self.edge_lenghts)
            if self.existing_lenght == "max":
                return max(self.edge_lenghts)
            elif self.existing_lenght == "average":
                return sum(self.edge_lenghts) / float(len(self.selected_edges))
        else:
            bm.edges.ensure_lookup_table()
            active_edge_length = None

            for elem in reversed(bm.select_history):
                if isinstance(elem, bmesh.types.BMEdge):
                    active_edge_length = elem.calc_length()
                    break
            return active_edge_length

        return 0.0

    def invoke(self, context, event):
        wm = context.window_manager

        obj = context.edit_object
        bm = bmesh.from_edit_mesh(obj.data)

        bpy.ops.mesh.select_mode(type="EDGE")
        self.selected_edges = get_selected(bm, 'edges')

        if self.selected_edges:
            vertex_set = []

            for edge in self.selected_edges:
                vector = get_edge_vector(edge)

                if edge.verts[0].index not in vertex_set:
                    vertex_set.append(edge.verts[0].index)
                else:
                    self.report({'ERROR_INVALID_INPUT'}, _error_message_2)
                    return {'CANCELLED'}

                if edge.verts[1].index not in vertex_set:
                    vertex_set.append(edge.verts[1].index)
                else:
                    self.report({'ERROR_INVALID_INPUT'}, _error_message_2)
                    return {'CANCELLED'}

                # warning, it's a constant !
                verts_index = ''.join((str(edge.verts[0].index), str(edge.verts[1].index)))
                self.originary_edge_length_dict[verts_index] = vector
                self.edge_lenghts.append(vector.length)
                self.old_length = vector.length
        else:
            self.report({'ERROR'}, _error_message)
            return {'CANCELLED'}

        if edge_length_debug:
            self.report({'INFO'}, str(self.originary_edge_length_dict))

        if bpy.context.scene.unit_settings.system == 'IMPERIAL':
            # imperial to metric conversion
            vector.length = (0.9144 * vector.length) / 3

        self.target_length = vector.length

        return wm.invoke_props_dialog(self)

    def execute(self, context):

        bpy.ops.mesh.select_mode(type="EDGE")
        self.context = context

        obj = context.edit_object
        bm = bmesh.from_edit_mesh(obj.data)

        self.selected_edges = get_selected(bm, 'edges')

        if not self.selected_edges:
            self.report({'ERROR'}, _error_message)
            return {'CANCELLED'}

        for edge in self.selected_edges:
            vector = get_edge_vector(edge)
            # what we should see in original length dialog field
            self.old_length = vector.length

            if self.set_lenght_type == 'manual':
                vector.length = abs(self.target_length)
            else:
                get_lenghts = self.get_existing_edge_lenght(bm)
                # check for edit mode
                if not get_lenghts:
                    self.report({'WARNING'},
                                "Operation Cancelled. "
                                "Active Edge could not be determined (needs selection in Edit Mode)")
                    return {'CANCELLED'}

                vector.length = get_lenghts

            if vector.length == 0.0:
                self.report({'ERROR'}, "Operation cancelled. Target lenght is set to zero")
                return {'CANCELLED'}

            center_vector = get_center_vector((edge.verts[0].co, edge.verts[1].co))

            verts_index = ''.join((str(edge.verts[0].index), str(edge.verts[1].index)))

            if edge_length_debug:
                self.report({'INFO'},
                            ' - '.join(('vector ' + str(vector),
                                        'originary_vector ' +
                                        str(self.originary_edge_length_dict[verts_index])
                                        )))
            verts = (edge.verts[0].co, edge.verts[1].co)

            if edge_length_debug:
                self.report({'INFO'},
                            '\n edge.verts[0].co ' + str(verts[0]) +
                            '\n edge.verts[1].co ' + str(verts[1]) +
                            '\n vector.length' + str(vector.length))

            # the clockwise direction have v1 -> v0, unclockwise v0 -> v1
            if self.target_length >= 0:
                if self.behaviour == 'proportional':
                    edge.verts[1].co = center_vector + vector / 2
                    edge.verts[0].co = center_vector - vector / 2

                    if self.mode == 'decrement':
                        edge.verts[0].co = (center_vector + vector / 2) - \
                                            (self.originary_edge_length_dict[verts_index] / 2)
                        edge.verts[1].co = (center_vector - vector / 2) + \
                                            (self.originary_edge_length_dict[verts_index] / 2)

                    elif self.mode == 'increment':
                        edge.verts[1].co = (center_vector + vector / 2) + \
                                            self.originary_edge_length_dict[verts_index] / 2
                        edge.verts[0].co = (center_vector - vector / 2) - \
                                            self.originary_edge_length_dict[verts_index] / 2

                elif self.behaviour == 'unclockwise':
                    if self.mode == 'increment':
                        edge.verts[1].co = \
                                verts[0] + (self.originary_edge_length_dict[verts_index] + vector)
                    elif self.mode == 'decrement':
                        edge.verts[0].co = \
                                verts[1] - (self.originary_edge_length_dict[verts_index] - vector)
                    else:
                        edge.verts[1].co = verts[0] + vector

                else:
                    # clockwise
                    if self.mode == 'increment':
                        edge.verts[0].co = \
                                verts[1] - (self.originary_edge_length_dict[verts_index] + vector)
                    elif self.mode == 'decrement':
                        edge.verts[1].co = \
                                verts[0] + (self.originary_edge_length_dict[verts_index] - vector)
                    else:
                        edge.verts[0].co = verts[1] - vector

            if bpy.context.scene.unit_settings.system == 'IMPERIAL':
                """
                # yards to metric conversion
                vector.length = ( 3. * vector.length ) / 0.9144
                # metric to yards conversion
                vector.length = ( 0.9144 * vector.length ) / 3.
                """
                for mvert in edge.verts:
                    # school time: 0.9144 : 3 = X : mvert
                    mvert.co = (0.9144 * mvert.co) / 3

            if edge_length_debug:
                self.report({'INFO'},
                            '\n edge.verts[0].co' + str(verts[0]) +
                            '\n edge.verts[1].co' + str(verts[1]) +
                            '\n vector' + str(vector) + '\n v1 > v0:' + str((verts[1] >= verts[0]))
                            )
            bmesh.update_edit_mesh(obj.data, True)

        return {'FINISHED'}


def register():
    bpy.utils.register_class(LengthSet)


def unregister():
    bpy.utils.unregister_class(LengthSet)


if __name__ == "__main__":
    register()
