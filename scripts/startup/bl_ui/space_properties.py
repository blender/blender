# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Header, Panel
from rna_prop_ui import PropertyPanel
from bl_ui import anim
from bpy.app.translations import (
    pgettext_iface as iface_,
)


tabs_attr_infos = (
    ("show_properties_tool", "Tool", 'TOOL_SETTINGS'),
    ("show_properties_render", "Render", 'SCENE'),
    ("show_properties_output", "Output", 'OUTPUT'),
    ("show_properties_view_layer", "View Layer", 'RENDERLAYERS'),
    ("show_properties_scene", "Scene", 'SCENE_DATA'),
    ("show_properties_world", "World", 'WORLD'),
    ("show_properties_collection", "Collection", 'GROUP'),
    ("show_properties_object", "Object", 'OBJECT_DATA'),
    ("show_properties_modifiers", "Modifiers", 'MODIFIER'),
    ("show_properties_effects", "Effects", 'SHADERFX'),
    ("show_properties_particles", "Particles", 'PARTICLES'),
    ("show_properties_physics", "Physics", 'PHYSICS'),
    ("show_properties_constraints", "Constraints", 'CONSTRAINT'),
    ("show_properties_data", "Data", 'MESH_DATA'),
    ("show_properties_bone", "Bone", 'BONE_DATA'),
    ("show_properties_bone_constraints", "Bone Constraints", 'CONSTRAINT_BONE'),
    ("show_properties_material", "Material", 'MATERIAL'),
    ("show_properties_texture", "Texture", 'TEXTURE'),
    ("show_properties_strip", "Strip", 'SEQ_SEQUENCER'),
    ("show_properties_strip_modifier", "Strip Modifiers", 'SEQ_STRIP_MODIFIER'),
)


class PROPERTIES_HT_header(Header):
    bl_space_type = 'PROPERTIES'

    @staticmethod
    def _search_poll(space):
        return any(getattr(space, tab_info[0]) for tab_info in tabs_attr_infos)

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        region = context.region
        ui_scale = context.preferences.system.ui_scale

        layout.template_header()

        layout.separator_spacer()

        if self._search_poll(context.space_data):
            # The following is an ugly attempt to make the search button center-align better visually.
            # A dummy icon is inserted that has to be scaled as the available width changes.
            content_size_est = 160 * ui_scale
            layout_scale = min(1, max(0, (region.width / content_size_est) - 1))
            if layout_scale > 0:
                row = layout.row()
                row.scale_x = layout_scale
                row.label(icon='BLANK1')

            layout.prop(view, "search_filter", icon='VIEWZOOM', text="")

        layout.separator_spacer()

        layout.popover(panel="PROPERTIES_PT_options", text="")


def has_hidden_tabs(space):
    return not all(getattr(space, tab_info[0]) for tab_info in tabs_attr_infos)


class PROPERTIES_PT_navigation_bar(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'NAVIGATION_BAR'
    bl_label = "Navigation Bar"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        layout.scale_x = 1.4
        layout.scale_y = 1.4
        if view.search_filter:
            layout.prop_tabs_enum(
                view, "context", data_highlight=view,
                property_highlight="tab_search_results", icon_only=True,
            )
        else:
            layout.prop_tabs_enum(view, "context", icon_only=True)

        # Scale sub layout to make the popover button smaller and use separator to
        # offset it, such that it is centered.
        sub = layout.row(align=True)
        sub.alignment = 'CENTER'
        sub.emboss = 'NONE'
        sub.scale_x = 0.8
        sub.scale_y = 0.8
        sub.separator(factor=0.7)
        sub.popover(panel="PROPERTIES_PT_visibility", text="")
        sub.active = has_hidden_tabs(view)


class PROPERTIES_PT_options(Panel):
    """Show options for the properties editor"""
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'HEADER'
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        col = layout.column()
        col.label(text="Sync with Outliner")
        col.row().prop(space, "outliner_sync", expand=True)


class PropertiesAnimationMixin:
    """Mix-in class for Animation panels.

    This class can be used to show a generic 'Animation' panel for IDs shown in
    the properties editor. Specific ID types need specific subclasses.

    For an example, see DATA_PT_camera_animation in properties_data_camera.py
    """
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Animation"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = PropertyPanel.bl_order - 1  # Order just above the Custom Properties.

    _animated_id_context_property = ""
    """context.{_animatable_id_context_property} is used to find the animated ID."""

    @classmethod
    def _animated_id(cls, context):
        assert cls._animated_id_context_property, "set _animated_id_context_property on {!r}".format(cls)

        # If the pinned ID is of a different type, there could still be an ID
        # for which to show this panel. For example, a camera object can be
        # pinned, and then this panel can be shown for its camera data.
        return getattr(context, cls._animated_id_context_property, None)

    @classmethod
    def poll(cls, context):
        animated_id = cls._animated_id(context)
        return animated_id is not None

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.use_property_split = True
        col.use_property_decorate = False
        self.draw_action_and_slot_selector(context, col, self._animated_id(context))

    @classmethod
    def draw_action_and_slot_selector(cls, context, layout, animated_id):
        if not animated_id:
            class_list = [c.__name__ for c in cls.mro()]
            print("PropertiesAnimationMixin: no animatable data-block, this is a bug "
                  "in one of these classes: {!r}".format(class_list))
            layout.label(text="No animatable data-block, please report as bug", icon='ERROR')
            return

        anim.draw_action_and_slot_selector_for_id(layout, animated_id)


class PROPERTIES_PT_visibility(Panel):
    """Choose visibility of tabs in the properties editor"""
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'HEADER'
    bl_label = "Visibility"

    def draw(self, context):
        space = context.space_data
        layout = self.layout
        layout.use_property_decorate = False

        col = layout.column(align=True)
        col.label(text="Visible Tabs")
        for prop, name, icon in tabs_attr_infos:
            row = col.row(align=True)
            row.label(text=iface_(name), icon=icon)
            row.prop(space, prop, text="")


classes = (
    PROPERTIES_HT_header,
    PROPERTIES_PT_navigation_bar,
    PROPERTIES_PT_options,
    PROPERTIES_PT_visibility,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
