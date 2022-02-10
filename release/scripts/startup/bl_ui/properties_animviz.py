# SPDX-License-Identifier: GPL-2.0-or-later

# <pep8 compliant>

# Generic Panels (Independent of DataType)

# NOTE:
# The specialized panel types are derived in their respective UI modules
# don't register these classes since they are only helpers.


class MotionPathButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_label = "Motion Paths"
    # bl_options = {'DEFAULT_CLOSED'}

    def draw_settings(self, _context, avs, mpath, bones=False):
        layout = self.layout

        mps = avs.motion_path

        # Display Range
        layout.use_property_split = True
        layout.use_property_decorate = False

        row = layout.row(align=True)
        row.prop(mps, "type")
        if mps.type == 'RANGE':
            if bones:
                row.operator("pose.paths_range_update", text="", icon='TIME')
            else:
                row.operator("object.paths_range_update", text="", icon='TIME')

        if mps.type == 'CURRENT_FRAME':
            col = layout.column(align=True)
            col.prop(mps, "frame_before", text="Frame Range Before")
            col.prop(mps, "frame_after", text="After")
            col.prop(mps, "frame_step", text="Step")
        elif mps.type == 'RANGE':
            col = layout.column(align=True)
            col.prop(mps, "frame_start", text="Frame Range Start")
            col.prop(mps, "frame_end", text="End")
            col.prop(mps, "frame_step", text="Step")

        if mpath:
            col = layout.column(align=True)
            col.enabled = False
            if bones:
                col.prop(mpath, "frame_start", text="Bone Cache From")
            else:
                col.prop(mpath, "frame_start", text="Cache From")
            col.prop(mpath, "frame_end", text="To")

            col = layout.column(align=True)

            if bones:
                col.operator("pose.paths_update", text="Update Paths", icon='BONE_DATA')
            else:
                col.operator("object.paths_update", text="Update Paths", icon='OBJECT_DATA')
        else:
            col = layout.column(align=True)
            col.label(text="Nothing to show yet...", icon='ERROR')

            if bones:
                col.operator("pose.paths_calculate", text="Calculate...", icon='BONE_DATA')
            else:
                col.operator("object.paths_calculate", text="Calculate...", icon='OBJECT_DATA')

        row = col.row(align=True)
        row.operator("object.paths_update_visible", text="Update All Paths", icon='WORLD')
        if bones:
            row.operator("pose.paths_clear", text="", icon='X')
        else:
            row.operator("object.paths_clear", text="", icon='X')


class MotionPathButtonsPanel_display:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_label = "Display"

    def draw_settings(self, _context, avs, mpath, bones=False):
        layout = self.layout

        mps = avs.motion_path

        layout.use_property_split = True
        layout.use_property_decorate = False

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=True)

        flow.prop(mps, "show_frame_numbers", text="Frame Numbers")
        flow.prop(mps, "show_keyframe_highlight", text="Keyframes")
        sub = flow.column()
        sub.enabled = mps.show_keyframe_highlight
        if bones:
            sub.prop(mps, "show_keyframe_action_all", text="+ Non-Grouped Keyframes")
        sub.prop(mps, "show_keyframe_numbers", text="Keyframe Numbers")

        # Customize path
        if mpath is not None:
            flow.prop(mpath, "lines", text="Lines")

            col = layout.column()
            col.prop(mpath, "line_thickness", text="Thickness")

            split = col.split(factor=0.6)

            split.prop(mpath, "use_custom_color", text="Custom Color")
            sub = split.column()
            sub.enabled = mpath.use_custom_color
            sub.prop(mpath, "color", text="")


classes = (
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
