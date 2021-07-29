# gpl: authors Liero, Atom

bl_info = {
    "name": "Unfold transition",
    "author": "Liero, Atom",
    "location": "3D View > Toolshelf > Create > Unfold Transition",
    "description": "Simple unfold transition / animation, will "
                   "separate faces and set up an armature",
    "category": "Animation"}

# Note the properties are moved to __init__
# search for patterns advanced_objects, adv_obj

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from random import (
        randint,
        uniform,
        )
from mathutils import Vector
from mathutils.geometry import intersect_point_line


class Set_Up_Fold(Operator):
    bl_idname = "object.set_up_fold"
    bl_label = "Set Up Unfold"
    bl_description = ("Set up Faces and Bones for animation\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.type == "MESH")

    def execute(self, context):
        bpy.ops.object.mode_set()
        scn = bpy.context.scene
        adv_obj = scn.advanced_objects1
        obj = bpy.context.object
        dat = obj.data
        fac = dat.polygons
        ver = dat.vertices

        # try to cleanup traces of previous actions
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.remove_doubles(threshold=0.0001, use_unselected=True)
        bpy.ops.object.mode_set()
        old_vg = [vg for vg in obj.vertex_groups if vg.name.startswith("bone.")]
        for vg in old_vg:
            obj.vertex_groups.remove(vg)

        if "UnFold" in obj.modifiers:
            arm = obj.modifiers["UnFold"].object
            rig = arm.data
            try:
                scn.objects.unlink(arm)
                bpy.data.objects.remove(arm)
                bpy.data.armatures.remove(rig)
            except:
                pass
            obj.modifiers.remove(obj.modifiers["UnFold"])

        # try to obtain the face sequence from the vertex weights
        if adv_obj.unfold_modo == "weight":
            if len(obj.vertex_groups):
                i = obj.vertex_groups.active.index
                W = []
                for f in fac:
                    v_data = []
                    for v in f.vertices:
                        try:
                            w = ver[v].groups[i].weight
                            v_data.append((w, v))
                        except:
                            v_data.append((0, v))
                    v_data.sort(reverse=True)
                    v1 = ver[v_data[0][1]].co
                    v2 = ver[v_data[1][1]].co
                    cen = Vector(f.center)
                    its = intersect_point_line(cen, v2, v1)
                    head = v2.lerp(v1, its[1])
                    peso = sum([x[0] for x in v_data])
                    W.append((peso, f.index, cen, head))
                W.sort(reverse=True)
                S = [x[1:] for x in W]
            else:
                self.report({"INFO"}, "First paint a Weight Map for this object")

                return {"FINISHED"}

        # separate the faces and sort them
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.edge_split()
        bpy.ops.mesh.select_all(action="SELECT")

        if adv_obj.unfold_modo == "cursor":
            bpy.context.tool_settings.mesh_select_mode = [True, True, True]
            bpy.ops.mesh.sort_elements(
                    type="CURSOR_DISTANCE", elements={"VERT", "EDGE", "FACE"}
                    )
        bpy.context.tool_settings.mesh_select_mode = [False, False, True]
        bpy.ops.object.mode_set()

        # Get sequence of faces and edges from the face / vertex indices
        if adv_obj.unfold_modo != "weight":
            S = []
            for f in fac:
                E = list(f.edge_keys)
                E.sort()
                v1 = ver[E[0][0]].co
                v2 = ver[E[0][1]].co
                cen = Vector(f.center)
                its = intersect_point_line(cen, v2, v1)
                head = v2.lerp(v1, its[1])
                S.append((f.index, f.center, head))

        # create the armature and the modifier
        arm = bpy.data.armatures.new("arm")
        rig = bpy.data.objects.new("rig_" + obj.name, arm)

        # store the name for checking the right rig
        adv_obj.unfold_arm_name = rig.name
        rig.matrix_world = obj.matrix_world
        scn.objects.link(rig)
        scn.objects.active = rig
        bpy.ops.object.mode_set(mode="EDIT")
        arm.draw_type = "WIRE"
        rig.show_x_ray = True
        mod = obj.modifiers.new("UnFold", "ARMATURE")
        mod.show_in_editmode = True
        mod.object = rig

        # create bones and vertex groups
        root = arm.edit_bones.new("bone.000")
        root.tail = (0, 0, 0)
        root.head = (0, 0, 1)
        root.select = True
        vis = [False, True] + [False] * 30

        for fb in S:
            f = fac[fb[0]]
            b = arm.edit_bones.new("bone.000")
            if adv_obj.unfold_flip:
                b.tail, b.head = fb[2], fb[1]
            else:
                b.tail, b.head = fb[1], fb[2]

            b.align_roll(f.normal)
            b.select = False
            b.layers = vis
            b.parent = root
            vg = obj.vertex_groups.new(b.name)
            vg.add(f.vertices, 1, "ADD")

        bpy.ops.object.mode_set()

        if adv_obj.unfold_modo == "weight":
            obj.vertex_groups.active_index = 0
        scn.objects.active = rig
        obj.select = False

        return {"FINISHED"}


class Animate_Fold(Operator):
    bl_idname = "object.animate_fold"
    bl_label = "Animate Unfold"
    bl_description = ("Animate bones to simulate unfold. Starts on current frame\n"
                      "Needs an existing Active Armature Object created in the previous step")
    bl_options = {"REGISTER", "UNDO"}

    is_not_undo = False

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.type == "ARMATURE" and obj.is_visible(bpy.context.scene))

    def draw(self, context):
        layout = self.layout
        adv_obj = context.scene.advanced_objects1

        if self.is_not_undo is True:
            layout.label(text="Warning:", icon="INFO")
            layout.label(text="The generated Armature was not selected or it was renamed")
            layout.label(text="The animation can fail if it is not generated by the previous step")
            layout.separator()
            layout.label(text="Expected Armature name:", icon="BONE_DATA")
            layout.label(text=str(adv_obj.unfold_arm_name), icon="TRIA_RIGHT")
            layout.label(text="To Continue press OK, to Cancel click Outside the Pop-up")
            layout.separator()
        else:
            return

    def invoke(self, context, event):
        obj = bpy.context.object
        scn = bpy.context.scene
        adv_obj = scn.advanced_objects1

        if obj.name != adv_obj.unfold_arm_name:
            self.is_not_undo = True
            return context.window_manager.invoke_props_dialog(self, width=400)
        else:
            return self.execute(context)

    def execute(self, context):
        obj = bpy.context.object
        scn = bpy.context.scene
        adv_obj = scn.advanced_objects1
        fra = scn.frame_current
        if obj.name != adv_obj.unfold_arm_name:
            self.report({"INFO"},
                        "The generated rig was not selected or renamed. The animation can fail")
        # clear the animation and get the list of bones
        if obj.animation_data:
            obj.animation_data_clear()
        bpy.ops.object.mode_set(mode="POSE")
        bones = obj.pose.bones[0].children_recursive

        if adv_obj.unfold_flip:
            rot = -3.141592
        else:
            rot = adv_obj.unfold_rot_max / 57.3

        extra = adv_obj.unfold_rot_time * adv_obj.unfold_bounce
        ruido = max(adv_obj.unfold_rot_time + extra,
                    adv_obj.unfold_sca_time) + adv_obj.unfold_fold_noise

        len_bones = len(bones) if len(bones) != 0 else 1  # possible division by zero
        vel = (adv_obj.unfold_fold_duration - ruido) / len_bones

        # introduce scale and rotation keyframes
        for a, b in enumerate(bones):
            t = fra + a * vel + randint(0, adv_obj.unfold_fold_noise)

            if adv_obj.unfold_flip:
                b.scale = (1, 1, 1)
            elif adv_obj.unfold_from_point:
                b.scale = (0, 0, 0)
            else:
                b.scale = (1, 0, 0)

            if not adv_obj.unfold_flip:
                b.keyframe_insert("scale", frame=t)
                b.scale = (1, 1, 1)
                b.keyframe_insert("scale", frame=t + adv_obj.unfold_sca_time)

            if adv_obj.unfold_rot_max:
                b.rotation_mode = "XYZ"
                if adv_obj.unfold_wiggle_rot:
                    euler = (uniform(-rot, rot), uniform(-rot, rot), uniform(-rot, rot))
                else:
                    euler = (rot, 0, 0)

                b.rotation_euler = euler
                b.keyframe_insert("rotation_euler", frame=t)

            if adv_obj.unfold_bounce:
                val = adv_obj.unfold_bounce * -.10
                b.rotation_euler = (val * euler[0], val * euler[1], val * euler[2])
                b.keyframe_insert(
                        "rotation_euler", frame=t + adv_obj.unfold_rot_time + .25 * extra
                        )

                val = adv_obj.unfold_bounce * .05
                b.rotation_euler = (val * euler[0], val * euler[1], val * euler[2])
                b.keyframe_insert(
                        "rotation_euler", frame=t + adv_obj.unfold_rot_time + .50 * extra
                        )

                val = adv_obj.unfold_bounce * -.025
                b.rotation_euler = (val * euler[0], val * euler[1], val * euler[2])
                b.keyframe_insert(
                        "rotation_euler", frame=t + adv_obj.unfold_rot_time + .75 * extra
                        )

            b.rotation_euler = (0, 0, 0)
            b.keyframe_insert(
                        "rotation_euler", frame=t + adv_obj.unfold_rot_time + extra
                        )
        self.is_not_undo = False

        return {"FINISHED"}


class PanelFOLD(Panel):
    bl_label = "Unfold Transition"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Create"
    bl_context = "objectmode"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        adv_obj = context.scene.advanced_objects1

        box = layout.box()
        col = box.column()
        col.operator("object.set_up_fold", text="1. Set Up Unfold")
        col.separator()
        col.label("Unfold Mode:")
        col.prop(adv_obj, "unfold_modo")
        col.prop(adv_obj, "unfold_flip")

        box = layout.box()
        col = box.column(align=True)
        col.operator("object.animate_fold", text="2. Animate Unfold")
        col.separator()
        col.prop(adv_obj, "unfold_fold_duration")
        col.prop(adv_obj, "unfold_sca_time")
        col.prop(adv_obj, "unfold_rot_time")
        col.prop(adv_obj, "unfold_rot_max")

        row = col.row(align=True)
        row.prop(adv_obj, "unfold_fold_noise")
        row.prop(adv_obj, "unfold_bounce")
        row = col.row(align=True)
        row.prop(adv_obj, "unfold_wiggle_rot")

        if not adv_obj.unfold_flip:
            row.prop(adv_obj, "unfold_from_point")


classes = (
    Set_Up_Fold,
    Animate_Fold,
    PanelFOLD,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
