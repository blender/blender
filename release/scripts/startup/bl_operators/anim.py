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

# <pep8-80 compliant>

if "bpy" in locals():
    from importlib import reload
    if "anim_utils" in locals():
        reload(anim_utils)
    del reload


import bpy
from bpy.types import Operator
from bpy.props import (
    IntProperty,
    BoolProperty,
    EnumProperty,
    StringProperty,
)


class ANIM_OT_keying_set_export(Operator):
    """Export Keying Set to a python script"""
    bl_idname = "anim.keying_set_export"
    bl_label = "Export Keying Set..."

    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_text: BoolProperty(
        name="Filter text",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter python",
        default=True,
        options={'HIDDEN'},
    )

    def execute(self, context):
        if not self.filepath:
            raise Exception("Filepath not set")

        f = open(self.filepath, "w")
        if not f:
            raise Exception("Could not open file")

        scene = context.scene
        ks = scene.keying_sets.active

        f.write("# Keying Set: %s\n" % ks.bl_idname)

        f.write("import bpy\n\n")
        f.write("scene = bpy.context.scene\n\n")

        # Add KeyingSet and set general settings
        f.write("# Keying Set Level declarations\n")
        f.write("ks = scene.keying_sets.new(idname=\"%s\", name=\"%s\")\n"
                "" % (ks.bl_idname, ks.bl_label))
        f.write("ks.bl_description = %r\n" % ks.bl_description)

        if not ks.is_path_absolute:
            f.write("ks.is_path_absolute = False\n")
        f.write("\n")

        f.write("ks.use_insertkey_needed = %s\n" % ks.use_insertkey_needed)
        f.write("ks.use_insertkey_visual = %s\n" % ks.use_insertkey_visual)
        f.write("ks.use_insertkey_xyz_to_rgb = %s\n" % ks.use_insertkey_xyz_to_rgb)
        f.write("\n")

        # --------------------------------------------------------
        # generate and write set of lookups for id's used in paths

        # cache for syncing ID-blocks to bpy paths + shorthand's
        id_to_paths_cache = {}

        for ksp in ks.paths:
            if ksp.id is None:
                continue
            if ksp.id in id_to_paths_cache:
                continue

            # - idtype_list is used to get the list of id-datablocks from
            #   bpy.data.* since this info isn't available elsewhere
            # - id.bl_rna.name gives a name suitable for UI,
            #   with a capitalised first letter, but we need
            #   the plural form that's all lower case
            # - special handling is needed for "nested" ID-blocks
            #   (e.g. nodetree in Material)
            if ksp.id.bl_rna.identifier.startswith("ShaderNodeTree"):
                # Find material or light using this node tree...
                id_bpy_path = "bpy.data.nodes[\"%s\"]"
                found = False

                for mat in bpy.data.materials:
                    if mat.node_tree == ksp.id:
                        id_bpy_path = "bpy.data.materials[\"%s\"].node_tree" % (mat.name)
                        found = True
                        break

                if not found:
                    for light in bpy.data.lights:
                        if light.node_tree == ksp.id:
                            id_bpy_path = "bpy.data.lights[\"%s\"].node_tree" % (light.name)
                            found = True
                            break

                if not found:
                    self.report({'WARN'}, "Could not find material or light using Shader Node Tree - %s" % (ksp.id))
            elif ksp.id.bl_rna.identifier.startswith("CompositorNodeTree"):
                # Find compositor nodetree using this node tree...
                for scene in bpy.data.scenes:
                    if scene.node_tree == ksp.id:
                        id_bpy_path = "bpy.data.scenes[\"%s\"].node_tree" % (scene.name)
                        break
                else:
                    self.report({'WARN'}, "Could not find scene using Compositor Node Tree - %s" % (ksp.id))
            elif ksp.id.bl_rna.name == "Key":
                # "keys" conflicts with a Python keyword, hence the simple solution won't work
                id_bpy_path = "bpy.data.shape_keys[\"%s\"]" % (ksp.id.name)
            else:
                idtype_list = ksp.id.bl_rna.name.lower() + "s"
                id_bpy_path = "bpy.data.%s[\"%s\"]" % (idtype_list, ksp.id.name)

            # shorthand ID for the ID-block (as used in the script)
            short_id = "id_%d" % len(id_to_paths_cache)

            # store this in the cache now
            id_to_paths_cache[ksp.id] = [short_id, id_bpy_path]

        f.write("# ID's that are commonly used\n")
        for id_pair in id_to_paths_cache.values():
            f.write("%s = %s\n" % (id_pair[0], id_pair[1]))
        f.write("\n")

        # write paths
        f.write("# Path Definitions\n")
        for ksp in ks.paths:
            f.write("ksp = ks.paths.add(")

            # id-block + data_path
            if ksp.id:
                # find the relevant shorthand from the cache
                id_bpy_path = id_to_paths_cache[ksp.id][0]
            else:
                id_bpy_path = "None"  # XXX...
            f.write("%s, '%s'" % (id_bpy_path, ksp.data_path))

            # array index settings (if applicable)
            if ksp.use_entire_array:
                f.write(", index=-1")
            else:
                f.write(", index=%d" % ksp.array_index)

            # grouping settings (if applicable)
            # NOTE: the current default is KEYINGSET, but if this changes,
            # change this code too
            if ksp.group_method == 'NAMED':
                f.write(", group_method='%s', group_name=\"%s\"" %
                        (ksp.group_method, ksp.group))
            elif ksp.group_method != 'KEYINGSET':
                f.write(", group_method='%s'" % ksp.group_method)

            # finish off
            f.write(")\n")

        f.write("\n")
        f.close()

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class NLA_OT_bake(Operator):
    """Bake all selected objects loc/scale/rotation animation to an action"""
    bl_idname = "nla.bake"
    bl_label = "Bake Action"
    bl_options = {'REGISTER', 'UNDO'}

    frame_start: IntProperty(
        name="Start Frame",
        description="Start frame for baking",
        min=0, max=300000,
        default=1,
    )
    frame_end: IntProperty(
        name="End Frame",
        description="End frame for baking",
        min=1, max=300000,
        default=250,
    )
    step: IntProperty(
        name="Frame Step",
        description="Frame Step",
        min=1, max=120,
        default=1,
    )
    only_selected: BoolProperty(
        name="Only Selected Bones",
        description="Only key selected bones (Pose baking only)",
        default=True,
    )
    visual_keying: BoolProperty(
        name="Visual Keying",
        description="Keyframe from the final transformations (with constraints applied)",
        default=False,
    )
    clear_constraints: BoolProperty(
        name="Clear Constraints",
        description="Remove all constraints from keyed object/bones, and do 'visual' keying",
        default=False,
    )
    clear_parents: BoolProperty(
        name="Clear Parents",
        description="Bake animation onto the object then clear parents (objects only)",
        default=False,
    )
    use_current_action: BoolProperty(
        name="Overwrite Current Action",
        description="Bake animation into current action, instead of creating a new one "
        "(useful for baking only part of bones in an armature)",
        default=False,
    )
    bake_types: EnumProperty(
        name="Bake Data",
        description="Which data's transformations to bake",
        options={'ENUM_FLAG'},
        items=(
             ('POSE', "Pose", "Bake bones transformations"),
             ('OBJECT', "Object", "Bake object transformations"),
        ),
        default={'POSE'},
    )

    def execute(self, context):
        from bpy_extras import anim_utils
        objects = context.selected_editable_objects
        object_action_pairs = (
            [(obj, getattr(obj.animation_data, "action", None)) for obj in objects]
            if self.use_current_action else
            [(obj, None) for obj in objects]
        )

        actions = anim_utils.bake_action_objects(
            object_action_pairs,
            frames=range(self.frame_start, self.frame_end + 1, self.step),
            only_selected=self.only_selected,
            do_pose='POSE' in self.bake_types,
            do_object='OBJECT' in self.bake_types,
            do_visual_keying=self.visual_keying,
            do_constraint_clear=self.clear_constraints,
            do_parents_clear=self.clear_parents,
            do_clean=True,
        )

        if not any(actions):
            self.report({'INFO'}, "Nothing to bake")
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, _event):
        scene = context.scene
        self.frame_start = scene.frame_start
        self.frame_end = scene.frame_end
        self.bake_types = {'POSE'} if context.mode == 'POSE' else {'OBJECT'}

        wm = context.window_manager
        return wm.invoke_props_dialog(self)


class ClearUselessActions(Operator):
    """Mark actions with no F-Curves for deletion after save & reload of """ \
        """file preserving \"action libraries\""""
    bl_idname = "anim.clear_useless_actions"
    bl_label = "Clear Useless Actions"
    bl_options = {'REGISTER', 'UNDO'}

    only_unused: BoolProperty(
        name="Only Unused",
        description="Only unused (Fake User only) actions get considered",
        default=True,
    )

    @classmethod
    def poll(cls, _context):
        return bool(bpy.data.actions)

    def execute(self, _context):
        removed = 0

        for action in bpy.data.actions:
            # if only user is "fake" user...
            if (
                (self.only_unused is False) or
                (action.use_fake_user and action.users == 1)
            ):

                # if it has F-Curves, then it's a "action library"
                # (i.e. walk, wave, jump, etc.)
                # and should be left alone as that's what fake users are for!
                if not action.fcurves:
                    # mark action for deletion
                    action.user_clear()
                    removed += 1

        self.report({'INFO'}, "Removed %d empty and/or fake-user only Actions"
                              % removed)
        return {'FINISHED'}


class UpdateAnimatedTransformConstraint(Operator):
    """Update fcurves/drivers affecting Transform constraints (use it with files from 2.70 and earlier)"""
    bl_idname = "anim.update_animated_transform_constraints"
    bl_label = "Update Animated Transform Constraints"
    bl_options = {'REGISTER', 'UNDO'}

    use_convert_to_radians: BoolProperty(
        name="Convert To Radians",
        description="Convert fcurves/drivers affecting rotations to radians (Warning: use this only once!)",
        default=True,
    )

    def execute(self, context):
        import animsys_refactor
        from math import radians
        import io

        from_paths = {"from_max_x", "from_max_y", "from_max_z", "from_min_x", "from_min_y", "from_min_z"}
        to_paths = {"to_max_x", "to_max_y", "to_max_z", "to_min_x", "to_min_y", "to_min_z"}
        paths = from_paths | to_paths

        def update_cb(base, class_name, old_path, fcurve, options):
            # print(options)

            def handle_deg2rad(fcurve):
                if fcurve is not None:
                    if hasattr(fcurve, "keyframes"):
                        for k in fcurve.keyframes:
                            k.co.y = radians(k.co.y)
                    for mod in fcurve.modifiers:
                        if mod.type == 'GENERATOR':
                            if mod.mode == 'POLYNOMIAL':
                                mod.coefficients[:] = [radians(c) for c in mod.coefficients]
                            else:  # if mod.type == 'POLYNOMIAL_FACTORISED':
                                mod.coefficients[:2] = [radians(c) for c in mod.coefficients[:2]]
                        elif mod.type == 'FNGENERATOR':
                            mod.amplitude = radians(mod.amplitude)
                    fcurve.update()

            data = ...
            try:
                data = eval("base." + old_path)
            except:
                pass
            ret = (data, old_path)
            if isinstance(base, bpy.types.TransformConstraint) and data is not ...:
                new_path = None
                map_info = base.map_from if old_path in from_paths else base.map_to
                if map_info == 'ROTATION':
                    new_path = old_path + "_rot"
                    if options is not None and options["use_convert_to_radians"]:
                        handle_deg2rad(fcurve)
                elif map_info == 'SCALE':
                    new_path = old_path + "_scale"

                if new_path is not None:
                    data = ...
                    try:
                        data = eval("base." + new_path)
                    except:
                        pass
                    ret = (data, new_path)
                    # print(ret)

            return ret

        options = {"use_convert_to_radians": self.use_convert_to_radians}
        replace_ls = [("TransformConstraint", p, update_cb, options) for p in paths]
        log = io.StringIO()

        animsys_refactor.update_data_paths(replace_ls, log)

        context.scene.frame_set(context.scene.frame_current)

        log = log.getvalue()
        if log:
            print(log)
            text = bpy.data.texts.new("UpdateAnimatedTransformConstraint Report")
            text.from_string(log)
            self.report({'INFO'}, "Complete report available on '%s' text datablock" % text.name)
        return {'FINISHED'}


classes = (
    ANIM_OT_keying_set_export,
    NLA_OT_bake,
    ClearUselessActions,
    UpdateAnimatedTransformConstraint,
)
