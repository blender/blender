# gpl author: Mano-Wii

bl_info = {
    "name": "Arrange on Curve",
    "author": "Mano-Wii",
    "version": (6, 3, 0),
    "blender": (2, 7, 7),
    "location": "3D View > Toolshelf > Create > Arrange on Curve",
    "description": "Arrange objects along a curve",
    "warning": "Select curve",
    "wiki_url": "",
    "category": "3D View"
    }

# Note: scene properties are moved into __init__
# search for patterns advanced_objects and adv_obj

import bpy
import mathutils
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import (
        EnumProperty,
        FloatProperty,
        IntProperty,
        )

FLT_MIN = 0.004


class PanelDupliCurve(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"
    bl_category = "Create"
    bl_label = "Arrange on Curve"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.object and context.mode == 'OBJECT' and context.object.type == 'CURVE'

    def draw(self, context):
        layout = self.layout
        adv_obj = context.scene.advanced_objects

        layout.prop(adv_obj, "arrange_c_use_selected")

        if not adv_obj.arrange_c_use_selected:
            layout.prop(adv_obj, "arrange_c_select_type", expand=True)
            if adv_obj.arrange_c_select_type == 'O':
                layout.column(align=True).prop_search(
                              adv_obj, "arrange_c_obj_arranjar",
                              bpy.data, "objects"
                              )
            elif adv_obj.arrange_c_select_type == 'G':
                layout.column(align=True).prop_search(
                              adv_obj, "arrange_c_obj_arranjar",
                              bpy.data, "groups"
                              )
        if context.object.type == 'CURVE':
            layout.operator("object.arranjar_numa_curva", text="Arrange Objects")


class DupliCurve(Operator):
    bl_idname = "object.arranjar_numa_curva"
    bl_label = "Arrange Objects along a Curve"
    bl_description = "Arange chosen / selected objects along the Active Curve"
    bl_options = {'REGISTER', 'UNDO'}

    use_distance = EnumProperty(
            name="Arrangement",
            items=[
                ("D", "Distance", "Objects are arranged depending on the distance", 0),
                ("Q", "Quantity", "Objects are arranged depending on the quantity", 1),
                ("R", "Range", "Objects are arranged uniformly between the corners", 2)
                ]
            )
    distance = FloatProperty(
            name="Distance",
            description="Distance between Objects",
            default=1.0,
            min=FLT_MIN,
            soft_min=0.1,
            unit='LENGTH',
            )
    object_qt = IntProperty(
            name="Quantity",
            description="Object amount",
            default=2,
            min=0,
            )
    scale = FloatProperty(
            name="Scale",
            description="Object Scale",
            default=1.0,
            min=FLT_MIN,
            unit='LENGTH',
                )
    Yaw = FloatProperty(
            name="X",
            description="Rotate around the X axis (Yaw)",
            default=0.0,
            unit='ROTATION'
            )
    Pitch = FloatProperty(
            default=0.0,
            description="Rotate around the Y axis (Pitch)",
            name="Y",
            unit='ROTATION'
            )
    Roll = FloatProperty(
            default=0.0,
            description="Rotate around the Z axis (Roll)",
            name="Z",
            unit='ROTATION'
            )
    max_angle = FloatProperty(
            default=1.57079,
            max=3.141592,
            name="Angle",
            unit='ROTATION'
            )
    offset = FloatProperty(
            default=0.0,
            name="Offset",
            unit='LENGTH'
            )

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.prop(self, "use_distance", text="")
        col = layout.column(align=True)
        if self.use_distance == "D":
            col.prop(self, "distance")
        elif self.use_distance == "Q":
            col.prop(self, "object_qt")
        else:
            col.prop(self, "distance")
            col.prop(self, "max_angle")
            col.prop(self, "offset")

        col = layout.column(align=True)
        col.prop(self, "scale")
        col.prop(self, "Yaw")
        col.prop(self, "Pitch")
        col.prop(self, "Roll")

    def Glpoints(self, curve):
        Gpoints = []
        for i, spline in enumerate(curve.data.splines):
            segments = len(spline.bezier_points)
            if segments >= 2:
                r = spline.resolution_u + 1

                points = []
                for j in range(segments):
                    bp1 = spline.bezier_points[j]
                    inext = (j + 1)
                    if inext == segments:
                        if not spline.use_cyclic_u:
                            break
                        inext = 0
                    bp2 = spline.bezier_points[inext]
                    if bp1.handle_right_type == bp2.handle_left_type == 'VECTOR':
                        _points = (bp1.co, bp2.co) if j == 0 else (bp2.co,)
                    else:
                        knot1 = bp1.co
                        handle1 = bp1.handle_right
                        handle2 = bp2.handle_left
                        knot2 = bp2.co
                        _points = mathutils.geometry.interpolate_bezier(knot1, handle1, handle2, knot2, r)
                    points.extend(_points)
                Gpoints.append(tuple((curve.matrix_world * p for p in points)))
            elif len(spline.points) >= 2:
                l = [curve.matrix_world * p.co.xyz for p in spline.points]
                if spline.use_cyclic_u:
                    l.append(l[0])
                Gpoints.append(tuple(l))

            if self.use_distance == "R":
                max_angle = self.max_angle
                tmp_Gpoints = []
                sp = Gpoints[i]
                sp2 = [sp[0], sp[1]]
                lp = sp[1]
                v1 = lp - sp[0]
                for p in sp[2:]:
                    v2 = p - lp
                    try:
                        if (3.14158 - v1.angle(v2)) < max_angle:
                            tmp_Gpoints.append(tuple(sp2))
                            sp2 = [lp]
                    except Exception as e:
                        print("\n[Add Advanced  Objects]\nOperator: "
                              "object.arranjar_numa_curva\nError: {}".format(e))
                        pass
                    sp2.append(p)
                    v1 = v2
                    lp = p
                tmp_Gpoints.append(tuple(sp2))
                Gpoints = Gpoints[:i] + tmp_Gpoints

        lengths = []
        if self.use_distance != "D":
            for sp in Gpoints:
                lp = sp[1]
                leng = (lp - sp[0]).length
                for p in sp[2:]:
                    leng += (p - lp).length
                    lp = p
                lengths.append(leng)
        return Gpoints, lengths

    def execute(self, context):
        if context.object.type != 'CURVE':
            return {'CANCELLED'}

        curve = context.active_object
        Gpoints, lengs = self.Glpoints(curve)
        adv_obj = context.scene.advanced_objects

        if adv_obj.arrange_c_use_selected:
            G_Objeto = context.selected_objects
            G_Objeto.remove(curve)

            if not G_Objeto:
                return {'CANCELLED'}

        elif adv_obj.arrange_c_select_type == 'O':
            G_Objeto = bpy.data.objects[adv_obj.arrange_c_obj_arranjar],
        elif adv_obj.arrange_c_select_type == 'G':
            G_Objeto = bpy.data.groups[adv_obj.arrange_c_obj_arranjar].objects

        yawMatrix = mathutils.Matrix.Rotation(self.Yaw, 4, 'X')
        pitchMatrix = mathutils.Matrix.Rotation(self.Pitch, 4, 'Y')
        rollMatrix = mathutils.Matrix.Rotation(self.Roll, 4, 'Z')

        max_angle = self.max_angle  # max_angle is called in Glpoints

        if self.use_distance == "D":
            dist = self.distance
            for sp_points in Gpoints:
                dx = 0.0  # Length of initial calculation of section
                last_point = sp_points[0]
                j = 0
                for point in sp_points[1:]:
                    vetorx = point - last_point  # Vector spline section
                    quat = mathutils.Vector.to_track_quat(vetorx, 'X', 'Z')  # Tracking the selected objects
                    quat = quat.to_matrix().to_4x4()

                    v_len = vetorx.length
                    if v_len > 0.0:
                        dx += v_len  # Defined length calculation equal total length of the spline section
                        v_norm = vetorx / v_len
                        while dx > dist:
                            object = G_Objeto[j % len(G_Objeto)]
                            j += 1
                            dx -= dist  # Calculating the remaining length of the section
                            obj = object.copy()
                            context.scene.objects.link(obj)
                            obj.matrix_world = quat * yawMatrix * pitchMatrix * rollMatrix
                            # Placing in the correct position
                            obj.matrix_world.translation = point - v_norm * dx
                            obj.scale *= self.scale
                        last_point = point

        elif self.use_distance == "Q":
            object_qt = self.object_qt + 1
            for i, sp_points in enumerate(Gpoints):
                dx = 0.0  # Length of initial calculation of section
                dist = lengs[i] / object_qt
                last_point = sp_points[0]
                j = 0
                for point in sp_points[1:]:
                    vetorx = point - last_point  # Vector spline section
                    # Tracking the selected objects
                    quat = mathutils.Vector.to_track_quat(vetorx, 'X', 'Z')
                    quat = quat.to_matrix().to_4x4()

                    v_len = vetorx.length
                    if v_len > 0.0:
                        # Defined length calculation equal total length of the spline section
                        dx += v_len
                        v_norm = vetorx / v_len
                        while dx > dist:
                            object = G_Objeto[j % len(G_Objeto)]
                            j += 1
                            dx -= dist  # Calculating the remaining length of the section
                            obj = object.copy()
                            context.scene.objects.link(obj)
                            obj.matrix_world = quat * yawMatrix * pitchMatrix * rollMatrix
                            # Placing in the correct position
                            obj.matrix_world.translation = point - v_norm * dx
                            obj.scale *= self.scale
                        last_point = point

        else:
            dist = self.distance
            offset2 = 2 * self.offset
            for i, sp_points in enumerate(Gpoints):
                leng = lengs[i] - offset2
                rest = leng % dist
                offset = offset2 + rest
                leng -= rest
                offset /= 2
                last_point = sp_points[0]

                dx = dist - offset  # Length of initial calculation of section
                j = 0
                for point in sp_points[1:]:
                    vetorx = point - last_point  # Vector spline section
                    # Tracking the selected objects
                    quat = mathutils.Vector.to_track_quat(vetorx, 'X', 'Z')
                    quat = quat.to_matrix().to_4x4()

                    v_len = vetorx.length
                    if v_len > 0.0:
                        dx += v_len
                        v_norm = vetorx / v_len
                        while dx >= dist and leng >= 0.0:
                            leng -= dist
                            dx -= dist  # Calculating the remaining length of the section
                            object = G_Objeto[j % len(G_Objeto)]
                            j += 1
                            obj = object.copy()
                            context.scene.objects.link(obj)
                            obj.matrix_world = quat * yawMatrix * pitchMatrix * rollMatrix
                            # Placing in the correct position
                            obj.matrix_world.translation = point - v_norm * dx
                            obj.scale *= self.scale
                        last_point = point

        return {"FINISHED"}


def register():
    bpy.utils.register_class(PanelDupliCurve)
    bpy.utils.register_class(DupliCurve)


def unregister():
    bpy.utils.unregister_class(PanelDupliCurve)
    bpy.utils.unregister_class(DupliCurve)


if __name__ == "__main__":
    register()
