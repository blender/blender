# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
from . import presets


class RENDER_UL_copy_settings(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        #assert(isinstance(item, (bpy.types.RenderCopySettingsScene, bpy.types.RenderCopySettingsDataSetting)))
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if isinstance(item, bpy.types.RenderCopySettingsDataSetting):
                layout.label(item.name, icon_value=icon)
                layout.prop(item, "copy", text="")
            else: #elif isinstance(item, bpy.types.RenderCopySettingsDataScene):
                layout.prop(item, "allowed", text=item.name, toggle=True)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            if isinstance(item, bpy.types.RenderCopySettingsDataSetting):
                layout.label(item.name, icon_value=icon)
                layout.prop(item, "copy", text="")
            else: #elif isinstance(item, bpy.types.RenderCopySettingsDataScene):
                layout.prop(item, "allowed", text=item.name, toggle=True)


class RENDER_PT_copy_settings(bpy.types.Panel):
    bl_label = "Copy Settings"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        cp_sett = context.scene.render_copy_settings

        layout.operator("scene.render_copy_settings", text="Copy Render Settings")

        # This will update affected_settings/allowed_scenes (as this seems
        # to be impossible to do it from here…).
        if bpy.ops.scene.render_copy_settings_prepare.poll():
            bpy.ops.scene.render_copy_settings_prepare()

        split = layout.split(0.75)
        split.template_list("RENDER_UL_copy_settings", "settings", cp_sett, "affected_settings",
                            cp_sett, "affected_settings_idx", rows=6)

        col = split.column()
        all_set = {sett.strid for sett in cp_sett.affected_settings if sett.copy}
        for p in presets.presets:
            label = ""
            if p.elements & all_set == p.elements:
                label = "Clear {}".format(p.ui_name)
            else:
                label = "Set {}".format(p.ui_name)
            col.operator("scene.render_copy_settings_preset", text=label).presets = {p.rna_enum[0]}

        layout.prop(cp_sett, "filter_scene")
        if len(cp_sett.allowed_scenes):
            layout.label("Affected Scenes:")
            layout.template_list("RENDER_UL_copy_settings", "scenes", cp_sett, "allowed_scenes",
#                                 cp_sett, "allowed_scenes_idx", rows=6, type='GRID')
                                 cp_sett, "allowed_scenes_idx", rows=6) # XXX Grid is not nice currently...
        else:
            layout.label(text="No Affectable Scenes!", icon="ERROR")


classes = (
    RENDER_UL_copy_settings,
    RENDER_PT_copy_settings,
)
