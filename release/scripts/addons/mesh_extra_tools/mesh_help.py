# gpl authors: lijenstina, meta-androcto

# Note:  this script contains the Help Operator used by the various functions
# Usage: add a key string to the dictionary in this file with the list of strings to pass to labels
#        and call the operator from the add-on UI draw function by passing the help_ids parameter
#        If the size of the pop-up if needed, define popup_size in the call by using varibles
#        Example (with using the variable props):
#        props = layout.row("mesh.extra_tools_help")
#        props.help_ids = "default"
#        props.popup_size = 400


import bpy
from bpy.types import Operator
from bpy.props import (
        StringProperty,
        IntProperty,
        )


class MESH_OT_extra_tools_help(Operator):
    bl_idname = "mesh.extra_tools_help"
    bl_label = ""
    bl_description = "Tool Help - click to read some basic information"
    bl_options = {'REGISTER'}

    help_ids = StringProperty(
            name="ID of the Operator to display",
            options={'HIDDEN'},
            default="default"
            )
    popup_size = IntProperty(
            name="Size of the Help Pop-up Menu",
            default=350,
            min=100,
            max=600,
            )

    def draw(self, context):
        layout = self.layout
        pick_help = help_custom_draw(self.help_ids)

        for line_text in pick_help:
            layout.label(line_text)

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_popup(self, width=self.popup_size)


def help_custom_draw(identifier="default"):
    # A table of lists containing the help text under an index key that is the script name
    # If several returns are needed per file, add some suffix after the script name
    # and call them separately
    # In case nothing is passed from the UI call, the returned list is default
    # If undefined one is passed, it will return a warning message
    help_text = {
        "default": [
                "This is a placeholder text",
                "Please fill up the entries in the " + __name__ + " script",
                ],
        "random_vertices": [
                "To use:",
                "Make a selection or selection of Vertices",
                "Randomize displaced positions",
                "Note:",
                "There is an option to use Vertex Weights for displacement",
                "Prior to use, don't forget to assign after updating the Group Weight",
                ],
        "mesh_vertex_chamfer": [
                "To use:",
                "Make a selection or selection of vertices",
                "Result is a triangle Chamfer, works on a single vertex",
                "Note:",
                "The difference to the vertex Bevel is that original geometry",
                "(selected vertices) can optionally be kept and displaced",
                "Limitation:",
                "In some cases, may need to press F to fill the result",
                ],
        "mesh_filletplus": [
                "To use:",
                "Select two adjacent edges and press Fillet button",
                "Limitation:",
                "Works on a mesh with all faces sharing the same normal",
                "(Flat Surface - faces have the same direction)",
                "Planes with already round corners can produce unsatisfactory results",
                "Only boundary edges will be evaluated",
                ],
        "mesh_offset_edges": [
                "To use:",
                "Make a selection or selection of Edges",
                "Extrude, rotate extrusions and more",
                "Limitation:",
                "Operates only on separate Edge loops selections",
                "(i.e. Edge loops that are not connected by a selected edge)",
                ],
        "mesh_edge_roundifier": [
                "To use:",
                "Select a single or multiple Edges",
                "Make Arcs with various parameters",
                "Reference, Rotation, Scaling, Connection and Offset",
                "Note:",
                "The Mode - Reset button restores the default values",
                ],
        "mesh_edges_length": [
                "To use:",
                "Select a single or multiple Edges",
                "Change length with various parameters",
                "Limitation:",
                "Does not operate on edges that share a vertex",
                "If the selection wasn't done in Edge Selection mode,",
                "the option Active will not work (due to Blender's limitation)",
                ],
        "mesh_edges_floor_plan": [
                "To use:",
                "Starting edges will be flat extruded forming faces strips",
                "on the inside. Similar to using Face fill inset select outer",
                "Methods:",
                "Edge Net: Fills the edge grid with faces then Inset",
                "Single Face: Single Face fill (all Edges) then Inset",
                "Solidify: Extrude along defined axis, apply a Solidify modifier",
                "Note:",
                "Grid Fill and Single Face sometimes need tweaking with the options",
                "Limitation:",
                "Depending on the input geometry, Keep Ngons sometimes needs to be",
                "enabled to produce any results",
                "Edge Net and Single Face depend on bmesh face fill and inset",
                "that sometimes can fail to produce good results",
                "Avoid using Single Face Method on Edges that define a Volume - like Suzanne",
                "Solidify method works best for flat surfaces and complex geometry",
                ],
        "mesh_mextrude_plus": [
                "To use:",
                "Make a selection of Faces",
                "Extrude with Rotation, Scaling, Variation,",
                "Randomization and Offset parameters",
                "Limitation:",
                "Works only with selections that enclose Faces",
                "(i.e. all Edges or Vertices of a Face selected)",
                ],
        "mesh_extrude_and_reshape": [
                "To use:",
                "Extrude Face and merge Edge intersections,",
                "between the mesh and the new Edges",
                "Note:",
                "If selected Vertices don't form Face they will be",
                "still extruded in the same direction",
                "Limitation:",
                "Works only with the last selected face",
                "(or all Edges or Vertices of a Face selected)",
                ],
        "face_inset_fillet": [
                "To use:",
                "Select one or multiple faces and inset",
                "Inset square, circle or outside",
                "Note:",
                "Radius: use remove doubles to tidy joins",
                "Out: select and use normals flip before extruding",
                "Limitation:",
                "Using the Out option, sometimes can lead to unsatisfactory results",
                ],
        "mesh_cut_faces": [
                "To use:",
                "Make a selection or selection of Faces",
                "Some Functions work on a plane only",
                "Limitation:",
                "The selection must include at least two Faces with adjacent edges",
                "(Selections not sharing edges will not work)",
                ],
        "split_solidify": [
                "To use:",
                "Make a selection or selection of Faces",
                "Split Faces and Extrude results",
                "Similar to a shatter/explode effect",
                ],
        "mesh_fastloop": [
                "To use:",
                "Activate the tool and hover over the mesh in the general area",
                "for the loop and left click once to confirm the loop placement",
                "Slide using the mouse to fine tune its position, left click to confirm",
                "Repeat the operations if needed for new loops",
                "Press Esc. twice to exit the tool",
                "Limitation:",
                "The tool has the same limitations as Loop Cut and Slide",
                "In the Operator Panel, only the last loop can be tweaked",
                ],
        "mesh_pen_tool": [
                "To use:",
                "Press Ctrl + D key or click Draw button",
                "To draw along x use SHIFT + MOUSEMOVE",
                "To draw along y use ALT + MOUSEMOVE",
                "Press Ctrl to toggle Extrude at Cursor tool",
                "Right click to finish drawing or",
                "Press Esc to cancel",
                ],
        "pkhg_faces": [
                "To use:",
                "Needs a Face Selection in Edit Mode",
                "Select an option from Face Types drop down list",
                "Extrude, rotate extrusions and more",
                "Toggle Edit Mode after use",
                "Note:",
                "After using the operator, normals could need repair,",
                "or Removing Doubles",
                ],
        "vertex_align": [
                "To use:",
                "Select vertices that you want to align and click Align button",
                "Options include aligning to defined Custom coordinates or",
                "Stored vertex - (a single selected one with Store Selected Vertex)",
                "Note:",
                "Use Stored Coordinates - allows to save a set of coordinates",
                "as a starting point that can be tweaked on during operation",
                ],
        "mesh_check": [
                "To use:",
                "Tris and Ngons will select Faces by corensponding type",
                "Display faces will color the faces depending on the",
                "defined Colors, Edges' width and Face Opacity",
                "Note:",
                "The Faces' type count is already included elsewhere:",
                "In the Properties Editor > Data > Face / Info Select Panel",
                ],
        }

    if identifier in help_text:
        return help_text[identifier]

    return ["ERROR:", "Help Operator", "Undefined call to the Dictionary"]


# register
def register():
    bpy.utils.register_class(MESH_OT_extra_tools_help)


def unregister():
    bpy.utils.unregister_class(MESH_OT_extra_tools_help)


if __name__ == "__main__":
    register()
