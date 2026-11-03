"""
UIList Example - Active Item Stored in the User Interface
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++

A ``UIList`` for a collection without an "active" property (timeline markers).

When ``None`` and ``""`` are passed for ``active_dataptr`` and ``active_propname``, the list stores
its active item in the region instead of the data. Read it back with
``Region.active_item_index_get``, using the identifiers passed to ``template_list``.
"""
import bpy


class SCENE_UL_timeline_markers(bpy.types.UIList):
    # For lists that store their active item in the region, `active_data` is None and `active_propname` empty.
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        is_current = (item.frame == context.scene.frame_current)
        row = layout.row(align=True)
        row.prop(item, "name", text="", emboss=False, icon='MARKER_HLT' if is_current else 'MARKER')
        row.label(text=str(item.frame))


# The identifiers of the list, used to draw it and to read back its active item.
LISTTYPE_NAME = SCENE_UL_timeline_markers.__name__
LIST_ID = ""


class SCENE_OT_timeline_marker_jump_example(bpy.types.Operator):
    """Set the current frame to the frame of the marker"""
    bl_idname = "scene.timeline_marker_jump_example"
    bl_label = "Jump to Marker"
    bl_options = {'REGISTER', 'UNDO_GROUPED'}

    # The panel passes the active item to the operator, so the operator doesn't depend on the UI.
    index: bpy.props.IntProperty(
        name="Index",
        description="Index of the marker in the scene's timeline markers",
        default=-1,
    )

    def execute(self, context):
        scene = context.scene
        markers = scene.timeline_markers
        if not (0 <= self.index < len(markers)):
            return {'CANCELLED'}
        scene.frame_current = markers[self.index].frame
        return {'FINISHED'}


class SCENE_PT_timeline_markers_example(bpy.types.Panel):
    """Creates a Panel in the Scene properties window"""
    bl_label = "Timeline Markers Example"
    bl_idname = "SCENE_PT_timeline_markers_example"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # No active property, the active item is stored in the region.
        layout.template_list(
            LISTTYPE_NAME, LIST_ID,
            scene, "timeline_markers",
            None, "",
        )

        # Also when this panel is shown as a popover, the list stores its active item
        # in the region the popover was opened from.
        region = context.region
        index = region.active_item_index_get(LISTTYPE_NAME, LIST_ID)
        markers = scene.timeline_markers
        if index >= len(markers):
            return
        marker = markers[index]

        layout.use_property_split = True
        layout.prop(marker, "frame")
        layout.prop(marker, "camera")
        layout.operator("scene.timeline_marker_jump_example").index = index


classes = (
    SCENE_UL_timeline_markers,
    SCENE_OT_timeline_marker_jump_example,
    SCENE_PT_timeline_markers_example,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
