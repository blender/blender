# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ..com.material_helpers import get_gltf_node_name, create_settings_group

################ glTF Material Output node ###########################################


def create_gltf_ao_group(operator, group_name):

    # create a new group
    gltf_ao_group = bpy.data.node_groups.new(group_name, "ShaderNodeTree")

    return gltf_ao_group


class NODE_OT_GLTF_SETTINGS(bpy.types.Operator):
    bl_idname = "node.gltf_settings_node_operator"
    bl_label = "glTF Material Output"
    bl_description = "Add a node to the active tree for glTF export"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return (
            space is not None
            and space.type == "NODE_EDITOR"
            and context.object and context.object.active_material
            and bpy.context.preferences.addons['io_scene_gltf2'].preferences.settings_node_ui is True
        )

    def execute(self, context):
        gltf_settings_node_name = get_gltf_node_name()
        if gltf_settings_node_name in bpy.data.node_groups:
            my_group = bpy.data.node_groups[get_gltf_node_name()]
        else:
            my_group = create_settings_group(gltf_settings_node_name)
        node_tree = context.object.active_material.node_tree
        new_node = node_tree.nodes.new("ShaderNodeGroup")
        new_node.node_tree = bpy.data.node_groups[my_group.name]
        return {"FINISHED"}


def add_gltf_settings_to_menu(self, context):
    if bpy.context.preferences.addons['io_scene_gltf2'].preferences.settings_node_ui is True:
        self.layout.operator("node.gltf_settings_node_operator")

################################### KHR_materials_variants ####################

# Global UI panel


class gltf2_KHR_materials_variants_variant(bpy.types.PropertyGroup):
    variant_idx: bpy.props.IntProperty()
    name: bpy.props.StringProperty(name="Variant Name")


class SCENE_UL_gltf2_variants(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        layout.prop(item, "name", text="", emboss=False)


class SCENE_PT_gltf2_variants(bpy.types.Panel):
    bl_label = "glTF Material Variants"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "glTF Variants"

    @classmethod
    def poll(self, context):
        return bpy.context.preferences.addons['io_scene_gltf2'].preferences.KHR_materials_variants_ui is True

    def draw(self, context):
        layout = self.layout
        row = layout.row()

        if bpy.data.scenes[0].gltf2_KHR_materials_variants_variants and len(
                bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) > 0:

            row.template_list(
                "SCENE_UL_gltf2_variants",
                "",
                bpy.data.scenes[0],
                "gltf2_KHR_materials_variants_variants",
                bpy.data.scenes[0],
                "gltf2_active_variant")
            col = row.column()
            row = col.column(align=True)
            row.operator("scene.gltf2_variant_add", icon="ADD", text="")
            row.operator("scene.gltf2_variant_remove", icon="REMOVE", text="")

            row = layout.row()
            row.operator("scene.gltf2_display_variant", text="Display Variant")
            row = layout.row()
            row.operator("scene.gltf2_assign_to_variant", text="Assign To Variant")
            row = layout.row()
            row.operator("scene.gltf2_reset_to_original", text="Reset To Original")
            row.operator("scene.gltf2_assign_as_original", text="Assign as Original")
        else:
            row.operator("scene.gltf2_variant_add", text="Add Material Variant")


class SCENE_OT_gltf2_variant_add(bpy.types.Operator):
    """Add a new Material Variant"""
    bl_idname = "scene.gltf2_variant_add"
    bl_label = "Add Material Variant"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return True

    def execute(self, context):
        var = bpy.data.scenes[0].gltf2_KHR_materials_variants_variants.add()
        var.variant_idx = len(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) - 1
        var.name = "VariantName"
        bpy.data.scenes[0].gltf2_active_variant = len(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) - 1
        return {'FINISHED'}


class SCENE_OT_gltf2_variant_remove(bpy.types.Operator):
    """Add a new Material Variant"""
    bl_idname = "scene.gltf2_variant_remove"
    bl_label = "Remove Variant"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return len(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) > 0

    def execute(self, context):
        bpy.data.scenes[0].gltf2_KHR_materials_variants_variants.remove(bpy.data.scenes[0].gltf2_active_variant)

        # loop on all mesh
        for obj in [o for o in bpy.data.objects if o.type == "MESH"]:
            mesh = obj.data
            remove_idx_data = []
            for idx, i in enumerate(mesh.gltf2_variant_mesh_data):
                remove_idx_variants = []
                for idx_var, v in enumerate(i.variants):
                    if v.variant.variant_idx == bpy.data.scenes[0].gltf2_active_variant:
                        remove_idx_variants.append(idx_var)
                    elif v.variant.variant_idx > bpy.data.scenes[0].gltf2_active_variant:
                        v.variant.variant_idx -= 1

                if len(remove_idx_variants) > 0:
                    for idx_var in remove_idx_variants:
                        i.variants.remove(idx_var)

                if len(i.variants) == 0:
                    remove_idx_data.append(idx)

            if len(remove_idx_data) > 0:
                for idx_data in remove_idx_data:
                    mesh.gltf2_variant_mesh_data.remove(idx_data)

        return {'FINISHED'}


# Operator to display a variant
class SCENE_OT_gltf2_display_variant(bpy.types.Operator):
    bl_idname = "scene.gltf2_display_variant"
    bl_label = "Display Variant"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return len(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) > 0

    def execute(self, context):

        gltf2_active_variant = bpy.data.scenes[0].gltf2_active_variant

        # loop on all mesh
        for obj in [o for o in bpy.data.objects if o.type == "MESH"]:
            mesh = obj.data
            for i in mesh.gltf2_variant_mesh_data:
                if i.variants and gltf2_active_variant in [v.variant.variant_idx for v in i.variants]:
                    mat = i.material
                    slot = i.material_slot_index
                    if slot < len(obj.material_slots):  # Seems user remove some slots...
                        obj.material_slots[slot].material = mat

        return {'FINISHED'}

# Operator to assign current mesh materials to a variant


class SCENE_OT_gltf2_assign_to_variant(bpy.types.Operator):
    bl_idname = "scene.gltf2_assign_to_variant"
    bl_label = "Assign To Variant"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return len(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) > 0 \
            and bpy.context.object and bpy.context.object.type == "MESH"

    def execute(self, context):
        gltf2_active_variant = bpy.data.scenes[0].gltf2_active_variant
        obj = bpy.context.object

        # loop on material slots ( primitives )
        for mat_slot_idx, s in enumerate(obj.material_slots):
            # Check if there is already data for this slot
            found = False
            variant_found = False
            for i in obj.data.gltf2_variant_mesh_data:
                if i.material_slot_index == mat_slot_idx and i.material == s.material:
                    found = True
                    variant_primitive = i
                elif i.material_slot_index == mat_slot_idx and bpy.data.scenes[0].gltf2_active_variant in [
                        v.variant.variant_idx for v in i.variants]:
                    # User changed the material, so store the new one (replace instead of add)
                    found = True
                    variant_found = True
                    variant_primitive = i
                    i.material = s.material

            if found is False:
                variant_primitive = obj.data.gltf2_variant_mesh_data.add()
                variant_primitive.material_slot_index = mat_slot_idx
                variant_primitive.material = s.material

            if variant_found is False:
                vari = variant_primitive.variants.add()
                vari.variant.variant_idx = bpy.data.scenes[0].gltf2_active_variant

        return {'FINISHED'}

# Operator to reset mesh to original (using default material when exists)


class SCENE_OT_gltf2_reset_to_original(bpy.types.Operator):
    bl_idname = "scene.gltf2_reset_to_original"
    bl_label = "Reset to Original"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return bpy.context.object and bpy.context.object.type == "MESH" and len(
            context.object.data.gltf2_variant_default_materials) > 0

    def execute(self, context):
        obj = bpy.context.object

        # loop on material slots ( primitives )
        for mat_slot_idx, s in enumerate(obj.material_slots):
            # Check if there is a default material for this slot
            found = False
            for i in obj.data.gltf2_variant_default_materials:
                if i.material_slot_index == mat_slot_idx:
                    s.material = i.default_material
                    break

        return {'FINISHED'}

# Operator to assign current materials as default materials


class SCENE_OT_gltf2_assign_as_original(bpy.types.Operator):
    bl_idname = "scene.gltf2_assign_as_original"
    bl_label = "Assign as Original"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return bpy.context.object and bpy.context.object.type == "MESH"

    def execute(self, context):
        obj = bpy.context.object

        # loop on material slots ( primitives )
        for mat_slot_idx, s in enumerate(obj.material_slots):
            # Check if there is a default material for this slot
            found = False
            for i in obj.data.gltf2_variant_default_materials:
                if i.material_slot_index == mat_slot_idx:
                    found = True
                    # Update if needed
                    i.default_material = s.material
                    break

            if found is False:
                default_mat = obj.data.gltf2_variant_default_materials.add()
                default_mat.material_slot_index = mat_slot_idx
                default_mat.default_material = s.material

        return {'FINISHED'}

# Mesh Panel


class gltf2_KHR_materials_variant_pointer(bpy.types.PropertyGroup):
    variant: bpy.props.PointerProperty(type=gltf2_KHR_materials_variants_variant)


class gltf2_KHR_materials_variants_default_material(bpy.types.PropertyGroup):
    material_slot_index: bpy.props.IntProperty(name="Material Slot Index")
    default_material: bpy.props.PointerProperty(type=bpy.types.Material)


class gltf2_KHR_materials_variants_primitive(bpy.types.PropertyGroup):
    material_slot_index: bpy.props.IntProperty(name="Material Slot Index")
    material: bpy.props.PointerProperty(type=bpy.types.Material)
    variants: bpy.props.CollectionProperty(type=gltf2_KHR_materials_variant_pointer)
    active_variant_idx: bpy.props.IntProperty()


class MESH_UL_gltf2_mesh_variants(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):

        vari = item.variant
        layout.context_pointer_set("id", vari)
        layout.prop(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants[vari.variant_idx],
                    "name", text="", emboss=False)


class MESH_PT_gltf2_mesh_variants(bpy.types.Panel):
    bl_label = "glTF Material Variants"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"

    @classmethod
    def poll(self, context):
        if not bpy.context.object:
            return False
        return bpy.context.preferences.addons['io_scene_gltf2'].preferences.KHR_materials_variants_ui is True \
            and len(bpy.context.object.material_slots) > 0

    def draw(self, context):
        layout = self.layout

        active_material_slots = bpy.context.object.active_material_index

        found = False
        for idx, prim in enumerate(bpy.context.object.data.gltf2_variant_mesh_data):
            if prim.material_slot_index == active_material_slots and id(prim.material) == id(
                    bpy.context.object.material_slots[active_material_slots].material):
                found = True
                break

        row = layout.row()
        if found is True:
            row.template_list("MESH_UL_gltf2_mesh_variants", "", prim, "variants", prim, "active_variant_idx")
            col = row.column()
            row = col.column(align=True)
            row.operator("scene.gltf2_variants_slot_add", icon="ADD", text="")
            row.operator("scene.gltf2_remove_material_variant", icon="REMOVE", text="")

            row = layout.row()
            if bpy.data.scenes[0].gltf2_KHR_materials_variants_variants:
                row.prop_search(
                    context.object.data,
                    "gltf2_variant_pointer",
                    bpy.data.scenes[0],
                    "gltf2_KHR_materials_variants_variants",
                    text="Variant")
                row = layout.row()
                row.operator("scene.gltf2_material_to_variant", text="Assign To Variant")
            else:
                row.label(text="Please Create a Variant First")
        else:
            if bpy.data.scenes[0].gltf2_KHR_materials_variants_variants:
                row.operator("scene.gltf2_variants_slot_add", text="Add a new Variant Slot")
            else:
                row.label(text="Please Create a Variant First")


class SCENE_OT_gltf2_variant_slot_add(bpy.types.Operator):
    """Add a new Slot"""
    bl_idname = "scene.gltf2_variants_slot_add"
    bl_label = "Add new Slot"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        if not bpy.context.object:
            return False
        return len(bpy.context.object.material_slots) > 0

    def execute(self, context):
        mesh = context.object.data
        # Check if there is already a data for this slot_idx + material

        found = False
        for i in mesh.gltf2_variant_mesh_data:
            if i.material_slot_index == context.object.active_material_index and i.material == context.object.material_slots[
                    context.object.active_material_index].material:
                found = True
                variant_primitive = i

        if found is False:
            variant_primitive = mesh.gltf2_variant_mesh_data.add()
            variant_primitive.material_slot_index = context.object.active_material_index
            variant_primitive.material = context.object.material_slots[context.object.active_material_index].material

        vari = variant_primitive.variants.add()
        vari.variant.variant_idx = bpy.data.scenes[0].gltf2_active_variant

        return {'FINISHED'}


class SCENE_OT_gltf2_material_to_variant(bpy.types.Operator):
    """Assign Variant to Slot"""
    bl_idname = "scene.gltf2_material_to_variant"
    bl_label = "Assign Material To Variant"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        if not bpy.context.object:
            return False
        return len(bpy.context.object.material_slots) > 0 and context.object.data.gltf2_variant_pointer != ""

    def execute(self, context):
        mesh = context.object.data

        found = False
        for i in mesh.gltf2_variant_mesh_data:
            if i.material_slot_index == context.object.active_material_index and i.material == context.object.material_slots[
                    context.object.active_material_index].material:
                found = True
                variant_primitive = i

        if found is False:
            return {'CANCELLED'}

        vari = variant_primitive.variants[variant_primitive.active_variant_idx]

        # Retrieve variant idx
        found = False
        for v in bpy.data.scenes[0].gltf2_KHR_materials_variants_variants:
            if v.name == context.object.data.gltf2_variant_pointer:
                found = True
                break

        if found is False:
            return {'CANCELLED'}

        vari.variant.variant_idx = v.variant_idx

        return {'FINISHED'}


class SCENE_OT_gltf2_remove_material_variant(bpy.types.Operator):
    """Remove a variant Slot"""
    bl_idname = "scene.gltf2_remove_material_variant"
    bl_label = "Remove a variant Slot"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        if not bpy.context.object:
            return False
        return len(bpy.context.object.material_slots) > 0 and len(bpy.context.object.data.gltf2_variant_mesh_data) > 0

    def execute(self, context):
        mesh = context.object.data

        found = False
        found_idx = -1
        for idx, i in enumerate(mesh.gltf2_variant_mesh_data):
            if i.material_slot_index == context.object.active_material_index and i.material == context.object.material_slots[
                    context.object.active_material_index].material:
                found = True
                variant_primitive = i
                found_idx = idx

        if found is False:
            return {'CANCELLED'}

        variant_primitive.variants.remove(variant_primitive.active_variant_idx)

        if len(variant_primitive.variants) == 0:
            mesh.gltf2_variant_mesh_data.remove(found_idx)

        return {'FINISHED'}


################ glTF Animation ###########################################

class gltf2_animation_NLATrackNames(bpy.types.PropertyGroup):
    name: bpy.props.StringProperty(name="NLA Track Name")


class SCENE_UL_gltf2_animation_track(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        row = layout.row()
        icon = 'SOLO_ON' if index == bpy.data.scenes[0].gltf2_animation_applied else 'SOLO_OFF'
        row.prop(item, "name", text="", emboss=False)
        op = row.operator("scene.gltf2_animation_apply", text='', icon=icon)
        op.index = index


class SCENE_OT_gltf2_animation_apply(bpy.types.Operator):
    """Apply glTF animations"""
    bl_idname = "scene.gltf2_animation_apply"
    bl_label = "Apply glTF animation"
    bl_options = {'REGISTER'}

    index: bpy.props.IntProperty()

    @classmethod
    def poll(self, context):
        return True

    def execute(self, context):

        track_name = bpy.data.scenes[0].gltf2_animation_tracks[self.index].name

        # remove all actions from objects
        for obj in bpy.context.scene.objects:
            if obj.animation_data:
                if obj.animation_data.action is not None:
                    obj.animation_data.action_slot = None
                obj.animation_data.action = None
                obj.matrix_world = obj.gltf2_animation_rest

                for track in [track for track in obj.animation_data.nla_tracks if track.name ==
                              track_name and len(track.strips) > 0 and track.strips[0].action is not None]:
                    obj.animation_data.action = track.strips[0].action

            if obj.type == "MESH" and obj.data and obj.data.shape_keys and obj.data.shape_keys.animation_data:
                if obj.data.shape_keys.animation_data.action is not None:
                    obj.data.shape_keys.animation_data.action_slot = None
                obj.data.shape_keys.animation_data.action = None
                for idx, data in enumerate(obj.gltf2_animation_weight_rest):
                    obj.data.shape_keys.key_blocks[idx + 1].value = data.val

                for track in [track for track in obj.data.shape_keys.animation_data.nla_tracks if track.name ==
                              track_name and len(track.strips) > 0 and track.strips[0].action is not None]:
                    obj.data.shape_keys.animation_data.action = track.strips[0].action
                    obj.data.shape_keys.animation_data.action_slot = track.strips[0].action_slot

            if obj.type in ["LIGHT", "CAMERA"] and obj.data and obj.data.animation_data:
                if obj.data.animation_data.action is not None:
                    obj.data.animation_data.action_slot = None
                obj.data.animation_data.action = None
                for track in [track for track in obj.data.animation_data.nla_tracks if track.name ==
                              track_name and len(track.strips) > 0 and track.strips[0].action is not None]:
                    obj.data.animation_data.action = track.strips[0].action
                    obj.data.animation_data.action_slot = track.strips[0].action_slot

        for mat in bpy.data.materials:
            if not mat.node_tree:
                continue
            if mat.node_tree.animation_data:
                if mat.node_tree.animation_data.action is not None:
                    mat.node_tree.animation_data.action_slot = None
                mat.node_tree.animation_data.action = None
                for track in [track for track in mat.node_tree.animation_data.nla_tracks if track.name ==
                              track_name and len(track.strips) > 0 and track.strips[0].action is not None]:
                    mat.node_tree.animation_data.action = track.strips[0].action
                    mat.node_tree.animation_data.action_slot = track.strips[0].action_slot

        bpy.data.scenes[0].gltf2_animation_applied = self.index
        return {'FINISHED'}


class SCENE_PT_gltf2_animation(bpy.types.Panel):
    bl_label = "glTF Animations"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "glTF"

    @classmethod
    def poll(self, context):
        return bpy.context.preferences.addons['io_scene_gltf2'].preferences.animation_ui is True

    def draw(self, context):
        layout = self.layout
        row = layout.row()

        if len(bpy.data.scenes[0].gltf2_animation_tracks) > 0:
            row.template_list(
                "SCENE_UL_gltf2_animation_track",
                "",
                bpy.data.scenes[0],
                "gltf2_animation_tracks",
                bpy.data.scenes[0],
                "gltf2_animation_active")
        else:
            row.label(text="No glTF Animation")


class GLTF2_weight(bpy.types.PropertyGroup):
    val: bpy.props.FloatProperty(name="weight")

################################### Filtering animation ####################


class SCENE_OT_gltf2_action_filter_refresh(bpy.types.Operator):
    """Refresh list of actions"""
    bl_idname = "scene.gltf2_action_filter_refresh"
    bl_label = "Refresh action list"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return True

    def execute(self, context):
        # Remove no more existing actions
        for idx, i in enumerate(bpy.data.scenes[0].gltf_action_filter):
            if i.action is None:
                bpy.data.scenes[0].gltf_action_filter.remove(idx)

        for action in bpy.data.actions:
            if id(action) in [id(i.action) for i in bpy.data.scenes[0].gltf_action_filter]:
                continue
            item = bpy.data.scenes[0].gltf_action_filter.add()
            item.action = action
            item.keep = True

        return {'FINISHED'}


class SCENE_UL_gltf2_filter_action(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):

        action = item.action
        layout.context_pointer_set("id", action)
        layout.split().prop(item.action, "name", text="", emboss=False)
        layout.split().prop(item, "keep", text="", emboss=True)


def export_panel_animation_action_filter(layout, operator):
    if operator.export_animation_mode not in ["ACTIONS", "ACTIVE_ACTIONS", "BROADCAST"]:
        return

    header, body = layout.panel("GLTF_export_action_filter", default_closed=True)
    header.use_property_split = False
    header.prop(operator, "export_action_filter", text="")
    header.label(text="Action Filter")
    if body and operator.export_action_filter:
        body.active = operator.export_animations and operator.export_action_filter

        row = body.row()

        # Collection Export does not handle correctly property declaration
        # So use this tweak to avoid spaming the console, waiting for a better solution
        is_file_browser = bpy.context.space_data.type == 'FILE_BROWSER'
        if not is_file_browser and not hasattr(bpy.data.scenes[0], "gltf_action_filter"):
            row.label(text="Please disable/enable 'action filter' to refresh the list")
            return

        if len(bpy.data.actions) > 0:
            row.template_list(
                "SCENE_UL_gltf2_filter_action",
                "",
                bpy.data.scenes[0],
                "gltf_action_filter",
                bpy.data.scenes[0],
                "gltf_action_filter_active")
            col = row.column()
            row = col.column(align=True)
            row.operator("scene.gltf2_action_filter_refresh", icon="FILE_REFRESH", text="")
        else:
            row.label(text="No Actions in .blend file")

###############################################################################


def register():
    bpy.utils.register_class(NODE_OT_GLTF_SETTINGS)
    bpy.types.NODE_MT_category_shader_output.append(add_gltf_settings_to_menu)
    bpy.utils.register_class(SCENE_OT_gltf2_action_filter_refresh)
    bpy.utils.register_class(SCENE_UL_gltf2_filter_action)


def variant_register():
    bpy.utils.register_class(SCENE_OT_gltf2_display_variant)
    bpy.utils.register_class(SCENE_OT_gltf2_assign_to_variant)
    bpy.utils.register_class(SCENE_OT_gltf2_reset_to_original)
    bpy.utils.register_class(SCENE_OT_gltf2_assign_as_original)
    bpy.utils.register_class(SCENE_OT_gltf2_remove_material_variant)
    bpy.utils.register_class(gltf2_KHR_materials_variants_variant)
    bpy.utils.register_class(gltf2_KHR_materials_variant_pointer)
    bpy.utils.register_class(gltf2_KHR_materials_variants_primitive)
    bpy.utils.register_class(gltf2_KHR_materials_variants_default_material)
    bpy.utils.register_class(SCENE_UL_gltf2_variants)
    bpy.utils.register_class(SCENE_PT_gltf2_variants)
    bpy.utils.register_class(MESH_UL_gltf2_mesh_variants)
    bpy.utils.register_class(MESH_PT_gltf2_mesh_variants)
    bpy.utils.register_class(SCENE_OT_gltf2_variant_add)
    bpy.utils.register_class(SCENE_OT_gltf2_variant_remove)
    bpy.utils.register_class(SCENE_OT_gltf2_material_to_variant)
    bpy.utils.register_class(SCENE_OT_gltf2_variant_slot_add)
    bpy.types.Mesh.gltf2_variant_mesh_data = bpy.props.CollectionProperty(type=gltf2_KHR_materials_variants_primitive)
    bpy.types.Mesh.gltf2_variant_default_materials = bpy.props.CollectionProperty(
        type=gltf2_KHR_materials_variants_default_material)
    bpy.types.Mesh.gltf2_variant_pointer = bpy.props.StringProperty()
    bpy.types.Scene.gltf2_KHR_materials_variants_variants = bpy.props.CollectionProperty(
        type=gltf2_KHR_materials_variants_variant)
    bpy.types.Scene.gltf2_active_variant = bpy.props.IntProperty()


def unregister():
    bpy.utils.unregister_class(NODE_OT_GLTF_SETTINGS)
    bpy.utils.unregister_class(SCENE_UL_gltf2_filter_action)
    bpy.utils.unregister_class(SCENE_OT_gltf2_action_filter_refresh)


def variant_unregister():
    bpy.utils.unregister_class(SCENE_OT_gltf2_variant_add)
    bpy.utils.unregister_class(SCENE_OT_gltf2_variant_remove)
    bpy.utils.unregister_class(SCENE_OT_gltf2_material_to_variant)
    bpy.utils.unregister_class(SCENE_OT_gltf2_variant_slot_add)
    bpy.utils.unregister_class(SCENE_OT_gltf2_display_variant)
    bpy.utils.unregister_class(SCENE_OT_gltf2_assign_to_variant)
    bpy.utils.unregister_class(SCENE_OT_gltf2_reset_to_original)
    bpy.utils.unregister_class(SCENE_OT_gltf2_assign_as_original)
    bpy.utils.unregister_class(SCENE_OT_gltf2_remove_material_variant)
    bpy.utils.unregister_class(SCENE_PT_gltf2_variants)
    bpy.utils.unregister_class(SCENE_UL_gltf2_variants)
    bpy.utils.unregister_class(MESH_PT_gltf2_mesh_variants)
    bpy.utils.unregister_class(MESH_UL_gltf2_mesh_variants)
    bpy.utils.unregister_class(gltf2_KHR_materials_variants_default_material)
    bpy.utils.unregister_class(gltf2_KHR_materials_variants_primitive)
    bpy.utils.unregister_class(gltf2_KHR_materials_variants_variant)
    bpy.utils.unregister_class(gltf2_KHR_materials_variant_pointer)


def anim_ui_register():
    bpy.utils.register_class(GLTF2_weight)
    bpy.utils.register_class(SCENE_OT_gltf2_animation_apply)
    bpy.utils.register_class(gltf2_animation_NLATrackNames)
    bpy.utils.register_class(SCENE_UL_gltf2_animation_track)
    bpy.types.Scene.gltf2_animation_tracks = bpy.props.CollectionProperty(type=gltf2_animation_NLATrackNames)
    bpy.types.Scene.gltf2_animation_active = bpy.props.IntProperty()
    bpy.types.Scene.gltf2_animation_applied = bpy.props.IntProperty()
    bpy.types.Object.gltf2_animation_rest = bpy.props.FloatVectorProperty(name="Rest", size=[4, 4], subtype="MATRIX")
    bpy.types.Object.gltf2_animation_weight_rest = bpy.props.CollectionProperty(type=GLTF2_weight)
    bpy.utils.register_class(SCENE_PT_gltf2_animation)


def anim_ui_unregister():
    bpy.utils.unregister_class(SCENE_PT_gltf2_animation)
    del bpy.types.Scene.gltf2_animation_active
    del bpy.types.Scene.gltf2_animation_tracks
    del bpy.types.Scene.gltf2_animation_applied
    del bpy.types.Object.gltf2_animation_rest
    del bpy.types.Object.gltf2_animation_weight_rest
    bpy.utils.unregister_class(SCENE_UL_gltf2_animation_track)
    bpy.utils.unregister_class(gltf2_animation_NLATrackNames)
    bpy.utils.unregister_class(SCENE_OT_gltf2_animation_apply)
    bpy.utils.unregister_class(GLTF2_weight)
