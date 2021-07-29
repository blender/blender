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

bl_info = {
    "name": "Drop to Ground1",
    "author": "Unnikrishnan(kodemax), Florian Meyer(testscreenings)",
    "blender": (2, 71, 0),
    "location": "3D View > Toolshelf > Create > Drop To Ground",
    "description": "Drop selected objects on active object",
    "warning": "",
    "category": "Object"}


import bpy
import bmesh
from mathutils import (
        Vector,
        Matrix,
        )
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import BoolProperty


def test_ground_object(ground):
    if ground.type in {'MESH', 'FONT', 'META', 'CURVE', 'SURFACE'}:
        return True
    return False


def get_align_matrix(location, normal):
    up = Vector((0, 0, 1))
    angle = normal.angle(up)
    axis = up.cross(normal)
    mat_rot = Matrix.Rotation(angle, 4, axis)
    mat_loc = Matrix.Translation(location)
    mat_align = mat_rot * mat_loc
    return mat_align


def transform_ground_to_world(sc, ground):
    tmpMesh = ground.to_mesh(sc, True, 'PREVIEW')
    tmpMesh.transform(ground.matrix_world)
    tmp_ground = bpy.data.objects.new('tmpGround', tmpMesh)
    sc.objects.link(tmp_ground)
    sc.update()

    return tmp_ground


def get_lowest_world_co_from_mesh(ob, mat_parent=None):
    bme = bmesh.new()
    bme.from_mesh(ob.data)
    mat_to_world = ob.matrix_world.copy()
    if mat_parent:
        mat_to_world = mat_parent * mat_to_world
    lowest = None
    for v in bme.verts:
        if not lowest:
            lowest = v
        if (mat_to_world * v.co).z < (mat_to_world * lowest.co).z:
            lowest = v
    lowest_co = mat_to_world * lowest.co
    bme.free()

    return lowest_co


def get_lowest_world_co(context, ob, mat_parent=None):
    if ob.type == 'MESH':
        return get_lowest_world_co_from_mesh(ob)

    elif ob.type == 'EMPTY' and ob.dupli_type == 'GROUP':
        if not ob.dupli_group:
            return None

        else:
            lowest_co = None
            for ob_l in ob.dupli_group.objects:
                if ob_l.type == 'MESH':
                    lowest_ob_l = get_lowest_world_co_from_mesh(ob_l, ob.matrix_world)
                    if not lowest_co:
                        lowest_co = lowest_ob_l
                    if lowest_ob_l.z < lowest_co.z:
                        lowest_co = lowest_ob_l

            return lowest_co


def drop_objectsall(self, context):
    ground = context.active_object
    name = ground.name

    for obs in bpy.context.scene.objects:
        obs.select = True
        if obs.name == name:
            obs.select = False

    obs2 = context.selected_objects

    tmp_ground = transform_ground_to_world(context.scene, ground)
    down = Vector((0, 0, -10000))

    for ob in obs2:
        if self.use_origin:
            lowest_world_co = ob.location
        else:
            lowest_world_co = get_lowest_world_co(context, ob)

        if not lowest_world_co:
            message = "Object {} is of type {} works only with Use Center option " \
                      "checked".format(ob.name, ob.type)
            self.reported.append(message)
            continue
        is_hit, hit_location, hit_normal, hit_index = tmp_ground.ray_cast(lowest_world_co, down)

        if not is_hit:
            message = ob.name + " did not hit the Ground"
            self.reported.append(message)
            continue

        # simple drop down
        to_ground_vec = hit_location - lowest_world_co
        ob.location += to_ground_vec

        # drop with align to hit normal
        if self.align:
            to_center_vec = ob.location - hit_location  # vec: hit_loc to origin
            # rotate object to align with face normal
            mat_normal = get_align_matrix(hit_location, hit_normal)
            rot_euler = mat_normal.to_euler()
            mat_ob_tmp = ob.matrix_world.copy().to_3x3()
            mat_ob_tmp.rotate(rot_euler)
            mat_ob_tmp = mat_ob_tmp.to_4x4()
            ob.matrix_world = mat_ob_tmp
            # move_object to hit_location
            ob.location = hit_location
            # move object above surface again
            to_center_vec.rotate(rot_euler)
            ob.location += to_center_vec

    # cleanup
    bpy.ops.object.select_all(action='DESELECT')
    tmp_ground.select = True
    bpy.ops.object.delete('EXEC_DEFAULT')
    for ob in obs2:
        ob.select = True
    ground.select = True


def drop_objects(self, context):
    ground = context.active_object

    obs = context.selected_objects
    if ground in obs:
        obs.remove(ground)

    tmp_ground = transform_ground_to_world(context.scene, ground)
    down = Vector((0, 0, -10000))

    for ob in obs:
        if self.use_origin:
            lowest_world_co = ob.location
        else:
            lowest_world_co = get_lowest_world_co(context, ob)

        if not lowest_world_co:
            message = "Object {} is of type {} works only with Use Center option " \
                      "checked".format(ob.name, ob.type)
            self.reported.append(message)
            continue

        is_hit, hit_location, hit_normal, hit_index = tmp_ground.ray_cast(lowest_world_co, down)
        if not is_hit:
            message = ob.name + " did not hit the Active Object"
            self.reported.append(message)
            continue

        # simple drop down
        to_ground_vec = hit_location - lowest_world_co
        ob.location += to_ground_vec

        # drop with align to hit normal
        if self.align:
            to_center_vec = ob.location - hit_location  # vec: hit_loc to origin
            # rotate object to align with face normal
            mat_normal = get_align_matrix(hit_location, hit_normal)
            rot_euler = mat_normal.to_euler()
            mat_ob_tmp = ob.matrix_world.copy().to_3x3()
            mat_ob_tmp.rotate(rot_euler)
            mat_ob_tmp = mat_ob_tmp.to_4x4()
            ob.matrix_world = mat_ob_tmp
            # move_object to hit_location
            ob.location = hit_location
            # move object above surface again
            to_center_vec.rotate(rot_euler)
            ob.location += to_center_vec

    # cleanup
    bpy.ops.object.select_all(action='DESELECT')
    tmp_ground.select = True
    bpy.ops.object.delete('EXEC_DEFAULT')
    for ob in obs:
        ob.select = True
    ground.select = True


# define base dummy class for inheritance
class DropBaseAtributes:
    align = BoolProperty(
            name="Align to ground",
            description="Aligns the objects' rotation to the ground",
            default=True)
    use_origin = BoolProperty(
            name="Use Origins",
            description="Drop to objects' origins\n"
                        "Use this option for dropping all types of Objects",
            default=False)


class OBJECT_OT_drop_to_ground(Operator, DropBaseAtributes):
    bl_idname = "object.drop_on_active"
    bl_label = "Drop to Ground"
    bl_description = ("Drop selected objects on the Active object\n"
                      "Active Object has to be of following the types:\n"
                      "Mesh, Font, Metaball, Curve, Surface")
    bl_options = {'REGISTER', 'UNDO'}

    reported = []

    @classmethod
    def poll(cls, context):
        act_obj = context.active_object
        return (len(context.selected_objects) >= 2 and
                act_obj and test_ground_object(act_obj))

    def execute(self, context):
        drop_objects(self, context)

        if self.reported:
            self.report({"INFO"},
                        "Some objects could not be dropped (See the Console for more Info)")
            report_items = "  \n".join(self.reported)
            print("\n[Drop to Ground Report]\n{}\n".format(report_items))

        self.reported[:] = []

        return {'FINISHED'}


class OBJECT_OT_drop_all_ground(Operator, DropBaseAtributes):
    bl_idname = "object.drop_all_active"
    bl_label = "Drop All to Ground (Active Object)"
    bl_description = ("Drop all other objects onto Active Object\n"
                      "Active Object has to be of following the types:\n"
                      "Mesh, Font, Metaball, Curve, Surface")
    bl_options = {'REGISTER', 'UNDO'}

    reported = []

    @classmethod
    def poll(cls, context):
        act_obj = context.active_object
        return act_obj and test_ground_object(act_obj)

    def execute(self, context):
        drop_objectsall(self, context)

        if self.reported:
            self.report({"INFO"},
                        "Some objects could not be dropped (See the Console for more Info)")
            report_items = "  \n".join(self.reported)
            print("\n[Drop All to Ground Report]\n{}\n".format(report_items))

        self.reported[:] = []

        return {'FINISHED'}


class Drop_help(Operator):
    bl_idname = "help.drop"
    bl_label = "Drop to Ground Help"
    bl_description = "Clik for some information about Drop to Ground"
    bl_options = {"REGISTER", "INTERNAL"}

    is_all = BoolProperty(
            default=True,
            options={"HIDDEN"}
            )

    def draw(self, context):
        layout = self.layout

        layout.label("General Info:")
        layout.label("The Active Object has to be of a Mesh, Font,")
        layout.label("Metaball, Curve or Surface type and")
        layout.label("be at the lowest Z location")
        layout.label("The option Use Origins must be enabled to drop")
        layout.label("objects that are not of a Mesh or DupliGroup type")
        layout.label("The Active Object has to be big enough to catch them")
        layout.label("To check that, use the Orthographic Top View")
        layout.separator()

        layout.label("To use:")

        if self.is_all is False:
            layout.label("Select objects to drop")
            layout.label("Then Shift Select the object to be the ground")
            layout.label("Drops Selected Object to the Active one")
        else:
            layout.label("Select the ground Mesh and press Drop all")
            layout.label("The unselected Objects will be moved straight")
            layout.label("down the Z axis, so they have to be above")
            layout.label("the Selected / Active one to fall")

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_popup(self, width=300)


class Drop_Operator_Panel(Panel):
    bl_label = "Drop To Ground"
    bl_region_type = "TOOLS"
    bl_space_type = "VIEW_3D"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "objectmode"
    bl_category = "Create"

    def draw(self, context):
        layout = self.layout

        row = layout.split(percentage=0.8, align=True)
        row.operator(OBJECT_OT_drop_to_ground.bl_idname,
                     text="Drop Selected")
        row.operator("help.drop", text="", icon="LAYER_USED").is_all = False

        row = layout.split(percentage=0.8, align=True)
        row.operator(OBJECT_OT_drop_all_ground.bl_idname,
                     text="Drop All")
        row.operator("help.drop", text="", icon="LAYER_USED").is_all = True


# Register
def register():
    bpy.utils.register_class(OBJECT_OT_drop_all_ground)
    bpy.utils.register_class(OBJECT_OT_drop_to_ground)
    bpy.utils.register_class(Drop_Operator_Panel)
    bpy.utils.register_class(Drop_help)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_drop_all_ground)
    bpy.utils.unregister_class(OBJECT_OT_drop_to_ground)
    bpy.utils.unregister_class(Drop_Operator_Panel)
    bpy.utils.unregister_class(Drop_help)


if __name__ == "__main__":
    register()
