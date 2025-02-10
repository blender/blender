# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Panel,
    UIList,
)

from rna_prop_ui import PropertyPanel
from .space_properties import PropertiesAnimationMixin

from bl_ui.properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
)

from bpy.app.translations import pgettext_iface as iface_


class SCENE_UL_keying_set_paths(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.KeyingSetPath)
        kspath = item
        icon = layout.enum_item_icon(kspath, "id_type", kspath.id_type)
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            # Do not make this one editable in uiList for now...
            layout.label(text=kspath.data_path, translate=False, icon_value=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class SceneButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"


class SCENE_PT_scene(SceneButtonsPanel, Panel):
    bl_label = "Scene"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene

        layout.prop(scene, "camera")
        layout.prop(scene, "background_set")
        layout.prop(scene, "active_clip", text="Active Clip")


class SCENE_PT_unit(SceneButtonsPanel, Panel):
    bl_label = "Units"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        unit = context.scene.unit_settings

        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(unit, "system")

        col = layout.column()
        col.enabled = unit.system != 'NONE'
        col.prop(unit, "scale_length")
        col.prop(unit, "use_separate")

        col = layout.column()
        col.prop(unit, "system_rotation", text="Rotation")
        subcol = col.column()
        subcol.enabled = unit.system != 'NONE'
        subcol.prop(unit, "length_unit", text="Length")
        subcol.prop(unit, "mass_unit", text="Mass")
        subcol.prop(unit, "time_unit", text="Time")
        subcol.prop(unit, "temperature_unit", text="Temperature")


class SceneKeyingSetsPanel:

    @staticmethod
    def draw_keyframing_settings(context, layout, ks, ksp):
        SceneKeyingSetsPanel._draw_keyframing_setting(
            context, layout, ks, ksp, iface_("Needed"),
            "use_insertkey_override_needed", "use_insertkey_needed",
            userpref_fallback="use_auto_keyframe_insert_needed",
        )
        SceneKeyingSetsPanel._draw_keyframing_setting(
            context, layout, ks, ksp, iface_("Visual"),
            "use_insertkey_override_visual", "use_insertkey_visual",
            userpref_fallback="use_visual_keying",
        )

    @staticmethod
    def _draw_keyframing_setting(context, layout, ks, ksp, label, toggle_prop, prop, userpref_fallback=None):
        if ksp:
            item = ksp

            if getattr(ks, toggle_prop):
                owner = ks
                propname = prop
            else:
                owner = context.preferences.edit
                if userpref_fallback:
                    propname = userpref_fallback
                else:
                    propname = prop
        else:
            item = ks

            owner = context.preferences.edit
            if userpref_fallback:
                propname = userpref_fallback
            else:
                propname = prop

        row = layout.row(align=True)

        subrow = row.row(align=True)
        subrow.active = getattr(item, toggle_prop)

        if subrow.active:
            subrow.prop(item, prop, text=label)
        else:
            subrow.prop(owner, propname, text=label)

        row.prop(item, toggle_prop, text="", icon='STYLUS_PRESSURE', toggle=True)  # XXX: needs dedicated icon


class SCENE_PT_keying_sets(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Keying Sets"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        row = layout.row()

        col = row.column()
        col.template_list("UI_UL_list", "keying_sets", scene, "keying_sets", scene.keying_sets, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("anim.keying_set_add", icon='ADD', text="")
        col.operator("anim.keying_set_remove", icon='REMOVE', text="")

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=False)

        ks = scene.keying_sets.active
        if ks and ks.is_path_absolute:
            col = flow.column()
            col.prop(ks, "bl_description")

            subcol = flow.column()
            subcol.operator_context = 'INVOKE_DEFAULT'
            subcol.operator("anim.keying_set_export", text="Export to File").filepath = "keyingset.py"


class SCENE_PT_keyframing_settings(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Keyframing Settings"
    bl_parent_id = "SCENE_PT_keying_sets"

    @classmethod
    def poll(cls, context):
        ks = context.scene.keying_sets.active
        return (ks and ks.is_path_absolute)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        ks = scene.keying_sets.active

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)

        col = flow.column(align=True)
        col.alignment = 'RIGHT'
        col.label(text="General Override")

        self.draw_keyframing_settings(context, col, ks, None)

        ksp = ks.paths.active
        if ksp:
            col.separator()

            col = flow.column(align=True)
            col.alignment = 'RIGHT'
            col.label(text="Active Set Override")

            self.draw_keyframing_settings(context, col, ks, ksp)


class SCENE_PT_keying_set_paths(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Active Keying Set"
    bl_parent_id = "SCENE_PT_keying_sets"

    @classmethod
    def poll(cls, context):
        ks = context.scene.keying_sets.active
        return (ks and ks.is_path_absolute)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        ks = scene.keying_sets.active

        row = layout.row()
        row.label(text="Paths:")

        row = layout.row()

        col = row.column()
        col.template_list("SCENE_UL_keying_set_paths", "", ks, "paths", ks.paths, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("anim.keying_set_path_add", icon='ADD', text="")
        col.operator("anim.keying_set_path_remove", icon='REMOVE', text="")

        # TODO: 1) the template_any_ID needs to be fixed for the text alignment.
        #       2) use_property_decorate has to properly skip the non animatable properties.
        #          Properties affected with needless draw:
        #          group_method, template_any_ID drop-down, use_entire_array.

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation (remove this later on).

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=True)

        ksp = ks.paths.active
        if ksp:
            col = flow.column(align=True)
            col.alignment = 'RIGHT'

            col.template_any_ID(ksp, "id", "id_type", text="Target ID-Block")

            col.separator()

            col.template_path_builder(ksp, "data_path", ksp.id, text="Data Path")

            col = flow.column()

            col.prop(ksp, "use_entire_array", text="Array All Items")

            if not ksp.use_entire_array:
                col.prop(ksp, "array_index", text="Index")

            col.separator()

            col.prop(ksp, "group_method", text="F-Curve Grouping")
            if ksp.group_method == 'NAMED':
                col.prop(ksp, "group")


class SCENE_PT_audio(SceneButtonsPanel, Panel):
    bl_label = "Audio"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(scene, "audio_volume")

        col.separator()

        col = col.column(align=True)
        col.prop(scene, "audio_distance_model")
        col.prop(scene, "audio_doppler_speed", text="Doppler Speed")
        col.prop(scene, "audio_doppler_factor", text="Doppler Factor")

        col.separator()

        layout.operator("sound.bake_animation")


class SCENE_PT_physics(SceneButtonsPanel, Panel):
    bl_label = "Gravity"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        self.layout.prop(context.scene, "use_gravity", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene

        layout.active = scene.use_gravity

        layout.prop(scene, "gravity")


class SCENE_PT_simulation(SceneButtonsPanel, Panel):
    bl_label = "Simulation"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene

        col = layout.column()
        col.prop(scene, "use_custom_simulation_range", text="Simulation Range")
        subcol = col.column(align=True)
        subcol.active = scene.use_custom_simulation_range
        subcol.prop(scene, "simulation_frame_start", text="Start")
        subcol.prop(scene, "simulation_frame_end", text="End")


class SCENE_PT_rigid_body_world(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body World"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world
        if rbw is not None:
            self.layout.prop(rbw, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rbw = scene.rigidbody_world

        if rbw is None:
            layout.operator("rigidbody.world_add")
        else:
            layout.operator("rigidbody.world_remove")


class RigidBodySubPanel(SceneButtonsPanel):
    bl_parent_id = "SCENE_PT_rigid_body_world"

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and scene.rigidbody_world


class SCENE_PT_rigid_body_world_settings(RigidBodySubPanel, Panel):
    bl_label = "Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rbw = scene.rigidbody_world

        if rbw:
            flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

            col = flow.column()
            col.active = rbw.enabled

            col = col.column()
            col.prop(rbw, "collection")
            col.prop(rbw, "constraints")

            col = col.column()
            col.prop(rbw, "time_scale", text="Speed")

            col = flow.column()
            col.active = rbw.enabled
            col.prop(rbw, "use_split_impulse")

            col = col.column()
            col.prop(rbw, "substeps_per_frame")
            col.prop(rbw, "solver_iterations")


class SCENE_PT_rigid_body_cache(RigidBodySubPanel, Panel):
    bl_label = "Cache"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        point_cache_ui(self, rbw.point_cache, rbw.point_cache.is_baked is False and rbw.enabled, 'RIGID_BODY')


class SCENE_PT_rigid_body_field_weights(RigidBodySubPanel, Panel):
    bl_label = "Field Weights"
    bl_parent_id = "SCENE_PT_rigid_body_world"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        effector_weights_ui(self, rbw.effector_weights, 'RIGID_BODY')


class SCENE_PT_eevee_next_light_probes(SceneButtonsPanel, Panel):
    bl_label = "Light Probes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.eevee

        col = layout.column()

        # TODO(fclem): Move to probe
        col.prop(props, "gi_cubemap_resolution", text="Spheres Resolution")

        row = col.row(align=True)
        row.operator("object.lightprobe_cache_bake", text="Bake All Light Probe Volumes").subset = 'ALL'
        row.operator("object.lightprobe_cache_free", text="", icon='TRASH').subset = 'ALL'


class SCENE_PT_animation(SceneButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    _animated_id_context_property = "scene"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene

        col = layout.column(align=True)
        col.label(text="Scene")
        self.draw_action_and_slot_selector(context, col, scene)

        if node_tree := scene.node_tree:
            col = layout.column(align=True)
            col.label(text="Compositing Node Tree")
            self.draw_action_and_slot_selector(context, col, node_tree)


class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel, Panel):
    _context_path = "scene"
    _property_type = bpy.types.Scene


classes = (
    SCENE_UL_keying_set_paths,
    SCENE_PT_scene,
    SCENE_PT_unit,
    SCENE_PT_physics,
    SCENE_PT_simulation,
    SCENE_PT_keying_sets,
    SCENE_PT_keying_set_paths,
    SCENE_PT_keyframing_settings,
    SCENE_PT_audio,
    SCENE_PT_rigid_body_world,
    SCENE_PT_rigid_body_world_settings,
    SCENE_PT_rigid_body_cache,
    SCENE_PT_rigid_body_field_weights,
    SCENE_PT_eevee_next_light_probes,
    SCENE_PT_animation,
    SCENE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
