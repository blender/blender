# gpl: author Daniel Schalla

import bpy
from bpy.types import Operator
from bpy.props import (
        EnumProperty,
        FloatProperty,
        IntProperty,
        )
from math import (
        sin, cos,
        radians,
        sqrt,
        )


class TriLighting(Operator):
    bl_idname = "object.trilighting"
    bl_label = "Tri-Lighting Creator"
    bl_description = ("Add 3 Point Lighting to Selected / Active Object\n"
                      "Needs an existing Active Object")
    bl_options = {'REGISTER', 'UNDO'}

    height = FloatProperty(
            name="Height",
            default=5
            )
    distance = FloatProperty(
            name="Distance",
            default=5,
            min=0.1,
            subtype="DISTANCE"
            )
    energy = IntProperty(
            name="Base Energy",
            default=3,
            min=1
            )
    contrast = IntProperty(
            name="Contrast",
            default=50,
            min=-100, max=100,
            subtype="PERCENTAGE"
            )
    leftangle = IntProperty(
            name="Left Angle",
            default=26,
            min=1, max=90,
            subtype="ANGLE"
            )
    rightangle = IntProperty(
            name="Right Angle",
            default=45,
            min=1, max=90,
            subtype="ANGLE"
            )
    backangle = IntProperty(
            name="Back Angle",
            default=235,
            min=90, max=270,
            subtype="ANGLE"
            )
    Light_Type_List = [
            ('POINT', "Point", "Point Light"),
            ('SUN', "Sun", "Sun Light"),
            ('SPOT', "Spot", "Spot Light"),
            ('HEMI', "Hemi", "Hemi Light"),
            ('AREA', "Area", "Area Light")
            ]
    primarytype = EnumProperty(
            attr='tl_type',
            name="Key Type",
            description="Choose the types of Key Lights you would like",
            items=Light_Type_List,
            default='HEMI'
            )
    secondarytype = EnumProperty(
            attr='tl_type',
            name="Fill + Back Type",
            description="Choose the types of secondary Lights you would like",
            items=Light_Type_List,
            default="POINT"
            )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def draw(self, context):
        layout = self.layout

        layout.label("Position:")
        col = layout.column(align=True)
        col.prop(self, "height")
        col.prop(self, "distance")

        layout.label("Light:")
        col = layout.column(align=True)
        col.prop(self, "energy")
        col.prop(self, "contrast")

        layout.label("Orientation:")
        col = layout.column(align=True)
        col.prop(self, "leftangle")
        col.prop(self, "rightangle")
        col.prop(self, "backangle")

        col = layout.column()
        col.label("Key Light Type:")
        col.prop(self, "primarytype", text="")
        col.label("Fill + Back Type:")
        col.prop(self, "secondarytype", text="")

    def execute(self, context):
        try:
            scene = context.scene
            view = context.space_data
            if view.type == 'VIEW_3D' and not view.lock_camera_and_layers:
                camera = view.camera
            else:
                camera = scene.camera

            if (camera is None):
                cam_data = bpy.data.cameras.new(name='Camera')
                cam_obj = bpy.data.objects.new(name='Camera', object_data=cam_data)
                scene.objects.link(cam_obj)
                scene.camera = cam_obj
                bpy.ops.view3d.camera_to_view()
                camera = cam_obj
                bpy.ops.view3d.viewnumpad(type='TOP')

            obj = bpy.context.scene.objects.active

            # Calculate Energy for each Lamp
            if(self.contrast > 0):
                keyEnergy = self.energy
                backEnergy = (self.energy / 100) * abs(self.contrast)
                fillEnergy = (self.energy / 100) * abs(self.contrast)
            else:
                keyEnergy = (self.energy / 100) * abs(self.contrast)
                backEnergy = self.energy
                fillEnergy = self.energy

            # Calculate Direction for each Lamp

            # Calculate current Distance and get Delta
            obj_position = obj.location
            cam_position = camera.location

            delta_position = cam_position - obj_position
            vector_length = sqrt(
                            (pow(delta_position.x, 2) +
                             pow(delta_position.y, 2) +
                             pow(delta_position.z, 2))
                            )
            if not vector_length:
                # division by zero most likely
                self.report({'WARNING'},
                            "Operation Cancelled. No viable object in the scene")

                return {'CANCELLED'}

            single_vector = (1 / vector_length) * delta_position

            # Calc back position
            singleback_vector = single_vector.copy()
            singleback_vector.x = cos(radians(self.backangle)) * single_vector.x + \
                                  (-sin(radians(self.backangle)) * single_vector.y)

            singleback_vector.y = sin(radians(self.backangle)) * single_vector.x + \
                                 (cos(radians(self.backangle)) * single_vector.y)

            backx = obj_position.x + self.distance * singleback_vector.x
            backy = obj_position.y + self.distance * singleback_vector.y

            backData = bpy.data.lamps.new(name="TriLamp-Back", type=self.secondarytype)
            backData.energy = backEnergy

            backLamp = bpy.data.objects.new(name="TriLamp-Back", object_data=backData)
            scene.objects.link(backLamp)
            backLamp.location = (backx, backy, self.height)

            trackToBack = backLamp.constraints.new(type="TRACK_TO")
            trackToBack.target = obj
            trackToBack.track_axis = "TRACK_NEGATIVE_Z"
            trackToBack.up_axis = "UP_Y"

            # Calc right position
            singleright_vector = single_vector.copy()
            singleright_vector.x = cos(radians(self.rightangle)) * single_vector.x + \
                                  (-sin(radians(self.rightangle)) * single_vector.y)

            singleright_vector.y = sin(radians(self.rightangle)) * single_vector.x + \
                                  (cos(radians(self.rightangle)) * single_vector.y)

            rightx = obj_position.x + self.distance * singleright_vector.x
            righty = obj_position.y + self.distance * singleright_vector.y

            rightData = bpy.data.lamps.new(name="TriLamp-Fill", type=self.secondarytype)
            rightData.energy = fillEnergy
            rightLamp = bpy.data.objects.new(name="TriLamp-Fill", object_data=rightData)
            scene.objects.link(rightLamp)
            rightLamp.location = (rightx, righty, self.height)
            trackToRight = rightLamp.constraints.new(type="TRACK_TO")
            trackToRight.target = obj
            trackToRight.track_axis = "TRACK_NEGATIVE_Z"
            trackToRight.up_axis = "UP_Y"

            # Calc left position
            singleleft_vector = single_vector.copy()
            singleleft_vector.x = cos(radians(-self.leftangle)) * single_vector.x + \
                                (-sin(radians(-self.leftangle)) * single_vector.y)
            singleleft_vector.y = sin(radians(-self.leftangle)) * single_vector.x + \
                                (cos(radians(-self.leftangle)) * single_vector.y)
            leftx = obj_position.x + self.distance * singleleft_vector.x
            lefty = obj_position.y + self.distance * singleleft_vector.y

            leftData = bpy.data.lamps.new(name="TriLamp-Key", type=self.primarytype)
            leftData.energy = keyEnergy

            leftLamp = bpy.data.objects.new(name="TriLamp-Key", object_data=leftData)
            scene.objects.link(leftLamp)
            leftLamp.location = (leftx, lefty, self.height)
            trackToLeft = leftLamp.constraints.new(type="TRACK_TO")
            trackToLeft.target = obj
            trackToLeft.track_axis = "TRACK_NEGATIVE_Z"
            trackToLeft.up_axis = "UP_Y"

        except Exception as e:
            self.report({'WARNING'},
                        "Some operations could not be performed (See Console for more info)")

            print("\n[Add Advanced  Objects]\nOperator: "
                  "object.trilighting\nError: {}".format(e))

            return {'CANCELLED'}

        return {'FINISHED'}
