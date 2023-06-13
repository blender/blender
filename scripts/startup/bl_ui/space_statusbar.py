# SPDX-License-Identifier: GPL-2.0-or-later
from bpy.types import Header


class STATUSBAR_HT_header(Header):
    bl_space_type = 'STATUSBAR'

    def draw(self, context):
        layout = self.layout

        # input status
        layout.template_input_status()

        layout.separator_spacer()

        # Messages
        layout.template_reports_banner()

        # Progress Bar
        layout.template_running_jobs()

        layout.separator_spacer()

        row = layout.row()
        row.alignment = 'RIGHT'

        # Stats & Info
        row.label(text=context.screen.statusbar_info(), translate=False)


classes = (
    STATUSBAR_HT_header,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
