# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

# <pep8 compliant>
import bpy


class OUTLINER_HT_header(bpy.types.Header):
    bl_space_type = 'OUTLINER'

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        scene = context.scene
        ks = context.scene.active_keying_set

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.itemM("OUTLINER_MT_view")
            if space.display_mode == 'DATABLOCKS':
                sub.itemM("OUTLINER_MT_edit_datablocks")

        layout.itemR(space, "display_mode", text="")

        layout.itemS()

        if space.display_mode == 'DATABLOCKS':
            row = layout.row(align=True)
            row.itemO("outliner.keyingset_add_selected", icon='ICON_ZOOMIN', text="")
            row.itemO("outliner.keyingset_remove_selected", icon='ICON_ZOOMOUT', text="")

            if ks:
                row = layout.row(align=False)
                row.item_pointerR(scene, "active_keying_set", scene, "keying_sets", text="")

                row = layout.row(align=True)
                row.itemO("anim.insert_keyframe", text="", icon='ICON_KEY_HLT')
                row.itemO("anim.delete_keyframe", text="", icon='ICON_KEY_DEHLT')
            else:
                row = layout.row(align=False)
                row.itemL(text="No Keying Set active")


class OUTLINER_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        col = layout.column()
        if space.display_mode not in ('DATABLOCKS', 'USER_PREFERENCES', 'KEYMAPS'):
            col.itemR(space, "show_restriction_columns")
            col.itemS()
            col.itemO("outliner.show_active")

        col.itemO("outliner.show_one_level")
        col.itemO("outliner.show_hierarchy")


class OUTLINER_MT_edit_datablocks(bpy.types.Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        col = layout.column()

        col.itemO("outliner.keyingset_add_selected")
        col.itemO("outliner.keyingset_remove_selected")

        col.itemS()

        col.itemO("outliner.drivers_add_selected")
        col.itemO("outliner.drivers_delete_selected")

bpy.types.register(OUTLINER_HT_header)
bpy.types.register(OUTLINER_MT_view)
bpy.types.register(OUTLINER_MT_edit_datablocks)
