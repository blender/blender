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

# TODO: find English versions of created object names

bl_info = {
    "name": "Oscurart Chain Maker",
    "author": "Oscurart",
    "version": (1, 1),
    "blender": (2, 56, 0),
    "location": "Add > Mesh > Oscurart Chain",
    "description": "Create chain links from armatures",
    "warning": "",
    "wiki_url": "oscurart.blogspot.com",
    "category": "Object"}


import bpy
from bpy.props import (
        BoolProperty,
        FloatProperty,
        )
from bpy.types import Operator


def makeChain(self, context, mult, curverig):

    if not context.active_object.type == 'ARMATURE':
        self.report({'WARNING'}, "Active Object must be an Armature")
        return False

    bpy.ops.object.mode_set(mode='OBJECT')
    VAR_SWITCH = abs(1)
    ARMATURE = bpy.context.active_object

    def creahuesocero(hueso):
        # create data to link
        mesh = bpy.data.meshes.new("objectData" + str(hueso.name))
        object = bpy.data.objects.new("HardLink" + str(hueso.name), mesh)
        mesh.from_pydata(
            [(-0.04986128956079483, -0.6918092370033264, -0.17846597731113434),
             (-0.04986128956079483, -0.6918091773986816, 0.17846640944480896),
             (-0.049861326813697815, -0.154555082321167, 0.17846627533435822),
             (-0.049861326813697815, -0.15455523133277893, -0.17846614122390747),
             (-0.04986133798956871, -0.03475356101989746, 0.25805795192718506),
             (-0.04986133798956871, -0.03475397825241089, -0.25805795192718506),
             (-0.049861278384923935, -0.8116106986999512, -0.2580576539039612),
             (-0.049861278384923935, -0.8116104602813721, 0.25805822014808655),
             (-0.04986128211021423, -0.7692053318023682, 2.6668965347198537e-07),
             (-0.04986127093434334, -0.923523485660553, 2.7834033744511544e-07),
             (-0.04986133426427841, -0.0771591067314148, 3.5627678585115063e-08),
             (-0.04986134544014931, 0.0771591067314148, -3.5627678585115063e-08),
             (0.04986133798956871, -0.03475397825241089, -0.25805795192718506),
             (0.04986133053898811, 0.0771591067314148, -3.5627678585115063e-08),
             (0.04986133798956871, -0.03475356101989746, 0.25805795192718506),
             (0.04986134544014931, -0.15455523133277893, -0.17846614122390747),
             (0.04986134544014931, -0.0771591067314148, 3.5627678585115063e-08),
             (0.04986134544014931, -0.154555082321167, 0.17846627533435822),
             (0.049861397594213486, -0.8116106986999512, -0.2580576539039612),
             (0.04986140504479408, -0.923523485660553, 2.7834033744511544e-07),
             (0.049861397594213486, -0.8116104602813721, 0.25805822014808655),
             (0.04986139014363289, -0.6918091773986816, 0.17846640944480896),
             (0.04986139014363289, -0.7692053318023682, 2.6668965347198537e-07),
             (0.04986139014363289, -0.6918092370033264, -0.17846597731113434)],
            [(1, 2), (0, 3), (3, 5), (2, 4), (0, 6), (5, 6), (1, 7), (4, 7), (0, 8), (1, 8),
            (7, 9), (6, 9), (8, 9), (2, 10), (3, 10), (4, 11), (5, 11), (10, 11), (5, 12),
            (12, 13), (11, 13), (13, 14), (4, 14), (10, 16), (15, 16), (3, 15), (2, 17),
            (16, 17), (9, 19), (18, 19), (6, 18), (7, 20), (19, 20), (8, 22), (21, 22),
            (1, 21), (0, 23), (22, 23), (14, 20), (12, 18), (15, 23), (17, 21), (12, 15),
            (13, 16), (14, 17), (20, 21), (19, 22), (18, 23)],
            [(6, 0, 3, 5), (1, 7, 4, 2), (0, 6, 9, 8), (8, 9, 7, 1), (2, 4, 11, 10), (10, 11, 5, 3),
            (11, 13, 12, 5), (4, 14, 13, 11), (3, 15, 16, 10), (10, 16, 17, 2), (6, 18, 19, 9),
            (9, 19, 20, 7), (1, 21, 22, 8), (23, 0, 8, 22), (7, 20, 14, 4), (5, 12, 18, 6),
            (0, 23, 15, 3), (2, 17, 21, 1), (16, 15, 12, 13), (17, 16, 13, 14), (22, 21, 20, 19),
            (23, 22, 19, 18), (21, 17, 14, 20), (15, 23, 18, 12)]
            )
        mesh.validate()
        bpy.context.scene.objects.link(object)
        # scale to the bone
        bpy.data.objects["HardLink" + str(hueso.name)].scale = (hueso.length * mult,
                                                                hueso.length * mult,
                                                                hueso.length * mult)
        # Parent Objects
        bpy.data.objects["HardLink" + str(hueso.name)].parent = ARMATURE
        bpy.data.objects["HardLink" + str(hueso.name)].parent_type = 'BONE'
        bpy.data.objects["HardLink" + str(hueso.name)].parent_bone = hueso.name

    def creahuesonoventa(hueso):
        # create data to link
        mesh = bpy.data.meshes.new("objectData" + str(hueso.name))
        object = bpy.data.objects.new("NewLink" + str(hueso.name), mesh)
        mesh.from_pydata(
            [(0.1784660965204239, -0.6918091773986816, -0.049861203879117966),
            (-0.1784662902355194, -0.6918091773986816, -0.04986126348376274),
            (-0.17846627533435822, -0.1545550525188446, -0.04986134544014931),
            (0.17846617102622986, -0.15455520153045654, -0.04986128583550453),
            (-0.25805795192718506, -0.03475359082221985, -0.049861375242471695),
            (0.25805795192718506, -0.034753888845443726, -0.04986129328608513),
            (0.2580578327178955, -0.8116105794906616, -0.04986117407679558),
            (-0.2580580413341522, -0.8116105198860168, -0.049861256033182144),
            (-9.672299938756623e-08, -0.7692052721977234, -0.04986122250556946),
            (-8.99775329799013e-08, -0.923523485660553, -0.04986120015382767),
            (-7.764004550381287e-09, -0.07715904712677002, -0.049861326813697815),
            (4.509517737005808e-08, 0.0771591067314148, -0.049861349165439606),
            (0.25805795192718506, -0.034753888845443726, 0.049861375242471695),
            (-2.2038317837314025e-08, 0.0771591067314148, 0.049861326813697815),
            (-0.25805795192718506, -0.03475359082221985, 0.04986129328608513),
            (0.17846617102622986, -0.15455520153045654, 0.04986138269305229),
            (-1.529285498236277e-08, -0.07715907692909241, 0.049861352890729904),
            (-0.17846627533435822, -0.1545550525188446, 0.049861323088407516),
            (0.2580578029155731, -0.8116105794906616, 0.049861494451761246),
            (-1.5711103173998708e-07, -0.923523485660553, 0.04986147582530975),
            (-0.2580580711364746, -0.8116105198860168, 0.04986141249537468),
            (-0.1784663051366806, -0.6918091773986816, 0.049861419945955276),
            (-1.340541757599567e-07, -0.7692052721977234, 0.049861449748277664),
            (0.1784660816192627, -0.6918091773986816, 0.04986146464943886)],
            [(1, 2), (0, 3), (3, 5), (2, 4), (0, 6), (5, 6), (1, 7), (4, 7), (0, 8),
            (1, 8), (7, 9), (6, 9), (8, 9), (2, 10), (3, 10), (4, 11), (5, 11), (10, 11),
            (5, 12), (12, 13), (11, 13), (13, 14), (4, 14), (10, 16), (15, 16), (3, 15),
            (2, 17), (16, 17), (9, 19), (18, 19), (6, 18), (7, 20), (19, 20), (8, 22),
            (21, 22), (1, 21), (0, 23), (22, 23), (14, 20), (12, 18), (15, 23), (17, 21),
            (12, 15), (13, 16), (14, 17), (20, 21), (19, 22), (18, 23)],
            [(6, 0, 3, 5), (1, 7, 4, 2), (0, 6, 9, 8), (8, 9, 7, 1), (2, 4, 11, 10),
            (10, 11, 5, 3), (11, 13, 12, 5), (4, 14, 13, 11), (3, 15, 16, 10), (10, 16, 17, 2),
            (6, 18, 19, 9), (9, 19, 20, 7), (1, 21, 22, 8), (23, 0, 8, 22), (7, 20, 14, 4),
            (5, 12, 18, 6), (0, 23, 15, 3), (2, 17, 21, 1), (16, 15, 12, 13), (17, 16, 13, 14),
            (22, 21, 20, 19), (23, 22, 19, 18), (21, 17, 14, 20), (15, 23, 18, 12)]
            )
        mesh.validate()
        bpy.context.scene.objects.link(object)
        # scale to the bone
        bpy.data.objects["NewLink" + str(hueso.name)].scale = (hueso.length * mult,
                                                                  hueso.length * mult,
                                                                  hueso.length * mult)
        # Parent objects
        bpy.data.objects["NewLink" + str(hueso.name)].parent = ARMATURE
        bpy.data.objects["NewLink" + str(hueso.name)].parent_type = 'BONE'
        bpy.data.objects["NewLink" + str(hueso.name)].parent_bone = hueso.name

    for hueso in bpy.context.active_object.pose.bones:
        if VAR_SWITCH == 1:
            creahuesocero(hueso)
        else:
            creahuesonoventa(hueso)
        if VAR_SWITCH == 1:
            VAR_SWITCH = 0
        else:
            VAR_SWITCH = 1

    # if curve rig is activated
    if curverig is True:
        # variables
        LISTA_POINTC = []
        ACTARM = bpy.context.active_object

        # create data and link the object to the scene
        crv = bpy.data.curves.new("CurvaCable", "CURVE")
        obCable = bpy.data.objects.new("Cable", crv)
        bpy.context.scene.objects.link(obCable)

        # set the attributes
        crv.dimensions = "3D"
        crv.resolution_u = 10
        crv.resolution_v = 10
        crv.twist_mode = "MINIMUM"

        # create the list of tail and head coordinates
        LISTA_POINTC.append((
                ACTARM.data.bones[0].head_local[0],
                ACTARM.data.bones[0].head_local[1],
                ACTARM.data.bones[0].head_local[2], 1
                ))

        for hueso in ACTARM.data.bones:
            LISTA_POINTC.append((
                    hueso.tail_local[0],
                    hueso.tail_local[1],
                    hueso.tail_local[2], 1
                    ))

        # create the Spline
        spline = crv.splines.new("NURBS")
        lencoord = len(LISTA_POINTC)
        rango = range(lencoord)
        spline.points.add(lencoord - 1)

        for punto in rango:
            spline.points[punto].co = LISTA_POINTC[punto]

        # set the endpoint
        bpy.data.objects['Cable'].data.splines[0].use_endpoint_u = True
        # select the curve
        bpy.ops.object.select_all(action='DESELECT')
        bpy.data.objects['Cable'].select = 1
        bpy.context.scene.objects.active = bpy.data.objects['Cable']
        # switch to Edit mode
        bpy.ops.object.mode_set(mode='EDIT')

        # create hooks
        POINTSTEP = 0
        for POINT in bpy.data.objects['Cable'].data.splines[0].points:
            bpy.ops.curve.select_all(action="DESELECT")
            bpy.data.objects['Cable'].data.splines[0].points[POINTSTEP].select = 1
            bpy.ops.object.hook_add_newob()
            POINTSTEP += 1

        # Objects selection step
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
        ACTARM.select = 1
        bpy.context.scene.objects.active = bpy.data.objects['Armature']
        bpy.ops.object.mode_set(mode='POSE')
        bpy.ops.pose.select_all(action='DESELECT')
        ACTARM.data.bones[-1].select = 1
        ACTARM.data.bones.active = ACTARM.data.bones[-1]

        # set IK Spline
        bpy.ops.pose.constraint_add_with_targets(type='SPLINE_IK')
        ACTARM.pose.bones[-1].constraints['Spline IK'].target = bpy.data.objects['Cable']
        ACTARM.pose.bones[-1].constraints['Spline IK'].chain_count = 100
        bpy.context.active_object.pose.bones[-1].constraints['Spline IK'].use_y_stretch = False
        # return to Object mode
        bpy.ops.object.mode_set(mode='OBJECT')


class MESH_OT_primitive_oscurart_chain_add(Operator):
    bl_idname = "mesh.primitive_oscurart_chain_add"
    bl_label = "Chain to Bones"
    bl_description = ("Add Chain Parented to an Existing Armature\n"
                      "The Active/Last Selected Object must be an Armature")
    bl_options = {'REGISTER', 'UNDO'}

    curverig = BoolProperty(
            name="Curve Rig",
            default=False
            )
    multiplier = FloatProperty(
            name="Scale",
            default=1,
            min=0.01, max=100.0
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj is not None and obj.type == "ARMATURE")

    def execute(self, context):
        try:
            makeChain(self, context, self.multiplier, self.curverig)

        except Exception as e:
            self.report({'WARNING'},
                        "Some operations could not be performed (See Console for more info)")

            print("\n[Add Advanced  Objects]\nOperator: "
                  "mesh.primitive_oscurart_chain_add\nError: {}".format(e))

            return {'CANCELLED'}

        return {'FINISHED'}


def register():
    bpy.utils.register_class(MESH_OT_primitive_oscurart_chain_add)


def unregister():
    bpy.utils.unregister_class(MESH_OT_primitive_oscurart_chain_add)


if __name__ == "__main__":
    register()
