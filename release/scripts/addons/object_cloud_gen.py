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

bl_info = {
    "name": "Cloud Generator",
    "author": "Nick Keeline(nrk)",
    "version": (1, 0, 2),
    "blender": (2, 78, 5),
    "location": "Tool Shelf > Create Tab",
    "description": "Creates Volumetric Clouds",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Object/Cloud_Gen",
    "category": "Object",
}

import bpy
from bpy.props import (
        BoolProperty,
        EnumProperty,
        )
from bpy.types import (
        Operator,
        Panel,
        )


# For Cycles Render we create node groups or if it already exists we return it.
def CreateNodeGroup(Type):

    # Look for NodeTree if it already exists return it
    CreateGroup = True
    for Group in bpy.data.node_groups:
        if Group.name == Type:
            CreateGroup = False
            NodeGroup = Group

    if CreateGroup is True:
        NodeGroup = bpy.data.node_groups.new(name=Type, type="ShaderNodeTree")
        NodeGroup.name = Type
        NodeGroup.bl_label = Type
        NodeGroup.nodes.clear()

        # Create a bunch of nodes and group them based on input to the def
        # Function type
        if Type == 'CloudGen_VolumeProperties':
            AddAddAndEmission = NodeGroup.nodes.new('ShaderNodeAddShader')
            AddAddAndEmission.location = [300, 395]
            AddAbsorptionAndScatter = NodeGroup.nodes.new('ShaderNodeAddShader')
            AddAbsorptionAndScatter.location = [0, 395]
            VolumeAbsorption = NodeGroup.nodes.new('ShaderNodeVolumeAbsorption')
            VolumeAbsorption.location = [-300, 395]
            VolumeScatter = NodeGroup.nodes.new('ShaderNodeVolumeScatter')
            VolumeScatter.location = [-300, 0]
            VolumeEmission = NodeGroup.nodes.new('ShaderNodeEmission')
            VolumeEmission.location = [-300, -300]
            MathAbsorptionMultiply = NodeGroup.nodes.new('ShaderNodeMath')
            MathAbsorptionMultiply.location = [-750, 395]
            MathAbsorptionMultiply.operation = 'MULTIPLY'
            MathScatterMultiply = NodeGroup.nodes.new('ShaderNodeMath')
            MathScatterMultiply.location = [-750, 0]
            MathScatterMultiply.operation = 'MULTIPLY'
            MathEmissionMultiply = NodeGroup.nodes.new('ShaderNodeMath')
            MathEmissionMultiply.location = [-750, -300]
            MathEmissionMultiply.operation = 'MULTIPLY'
            MathBrightnessMultiply = NodeGroup.nodes.new('ShaderNodeMath')
            MathBrightnessMultiply.location = [-1200, 0]
            MathBrightnessMultiply.operation = 'MULTIPLY'
            MathGreaterThan = NodeGroup.nodes.new('ShaderNodeMath')
            MathGreaterThan.location = [-1200, 600]
            MathGreaterThan.operation = 'GREATER_THAN'
            MathGreaterThan.inputs[1].default_value = 0

            NodeGroup.links.new(AddAddAndEmission.inputs[0], AddAbsorptionAndScatter.outputs[0])
            NodeGroup.links.new(AddAddAndEmission.inputs[1], VolumeEmission.outputs[0])
            NodeGroup.links.new(AddAbsorptionAndScatter.inputs[0], VolumeAbsorption.outputs[0])
            NodeGroup.links.new(AddAbsorptionAndScatter.inputs[1], VolumeScatter.outputs[0])
            NodeGroup.links.new(VolumeAbsorption.inputs[1], MathAbsorptionMultiply.outputs[0])
            NodeGroup.links.new(VolumeScatter.inputs[1], MathScatterMultiply.outputs[0])
            NodeGroup.links.new(VolumeEmission.inputs[1], MathEmissionMultiply.outputs[0])
            NodeGroup.links.new(MathAbsorptionMultiply.inputs[0], MathGreaterThan.outputs[0])
            NodeGroup.links.new(MathScatterMultiply.inputs[0], MathGreaterThan.outputs[0])
            NodeGroup.links.new(MathEmissionMultiply.inputs[0], MathGreaterThan.outputs[0])
            NodeGroup.links.new(VolumeAbsorption.inputs[0], MathBrightnessMultiply.outputs[0])

            # Create and Link In/Out to Group Node
            # Outputs
            group_outputs = NodeGroup.nodes.new('NodeGroupOutput')
            group_outputs.location = (600, 395)
            NodeGroup.outputs.new('NodeSocketShader', 'shader_out')
            NodeGroup.links.new(AddAddAndEmission.outputs[0], group_outputs.inputs['shader_out'])

            # Inputs
            group_inputs = NodeGroup.nodes.new('NodeGroupInput')
            group_inputs.location = (-1500, -300)
            NodeGroup.inputs.new('NodeSocketFloat', 'Density')
            NodeGroup.inputs.new('NodeSocketFloat', 'Absorption Multiply')
            NodeGroup.inputs.new('NodeSocketColor', 'Absorption Color')
            NodeGroup.inputs.new('NodeSocketFloat', 'Scatter Multiply')
            NodeGroup.inputs.new('NodeSocketColor', 'Scatter Color')
            NodeGroup.inputs.new('NodeSocketFloat', 'Emission Amount')
            NodeGroup.inputs.new('NodeSocketFloat', 'Cloud Brightness')

            NodeGroup.links.new(group_inputs.outputs['Density'], MathGreaterThan.inputs[0])
            NodeGroup.links.new(group_inputs.outputs['Absorption Multiply'], MathAbsorptionMultiply.inputs[1])
            NodeGroup.links.new(group_inputs.outputs['Absorption Color'], MathBrightnessMultiply.inputs[0])
            NodeGroup.links.new(group_inputs.outputs['Scatter Multiply'], MathScatterMultiply.inputs[1])
            NodeGroup.links.new(group_inputs.outputs['Scatter Color'], VolumeScatter.inputs[0])
            NodeGroup.links.new(group_inputs.outputs['Emission Amount'], MathEmissionMultiply.inputs[1])
            NodeGroup.links.new(group_inputs.outputs['Cloud Brightness'], MathBrightnessMultiply.inputs[1])

        if Type == 'CloudGen_TextureProperties':
            MathAdd = NodeGroup.nodes.new('ShaderNodeMath')
            MathAdd.location = [-200, 0]
            MathAdd.operation = 'ADD'
            MathDensityMultiply = NodeGroup.nodes.new('ShaderNodeMath')
            MathDensityMultiply.location = [-390, 0]
            MathDensityMultiply.operation = 'MULTIPLY'
            PointDensityRamp = NodeGroup.nodes.new('ShaderNodeValToRGB')
            PointDensityRamp.location = [-675, -250]
            PointRamp = PointDensityRamp.color_ramp
            PElements = PointRamp.elements
            PElements[0].position = 0.418
            PElements[0].color = 0, 0, 0, 1
            PElements[1].position = 0.773
            PElements[1].color = 1, 1, 1, 1
            CloudRamp = NodeGroup.nodes.new('ShaderNodeValToRGB')
            CloudRamp.location = [-675, 0]
            CRamp = CloudRamp.color_ramp
            CElements = CRamp.elements
            CElements[0].position = 0.527
            CElements[0].color = 0, 0, 0, 1
            CElements[1].position = 0.759
            CElements[1].color = 1, 1, 1, 1
            NoiseTex = NodeGroup.nodes.new('ShaderNodeTexNoise')
            NoiseTex.location = [-940, 0]
            NoiseTex.inputs['Detail'].default_value = 4
            TexCoord = NodeGroup.nodes.new('ShaderNodeTexCoord')
            TexCoord.location = [-1250, 0]

            NodeGroup.links.new(MathAdd.inputs[0], MathDensityMultiply.outputs[0])
            NodeGroup.links.new(MathAdd.inputs[1], PointDensityRamp.outputs[0])
            NodeGroup.links.new(MathDensityMultiply.inputs[0], CloudRamp.outputs[0])
            NodeGroup.links.new(CloudRamp.inputs[0], NoiseTex.outputs[0])
            NodeGroup.links.new(NoiseTex.inputs[0], TexCoord.outputs[3])

            # Create and Link In/Out to Group Nodes
            # Outputs
            group_outputs = NodeGroup.nodes.new('NodeGroupOutput')
            group_outputs.location = (0, 0)
            NodeGroup.outputs.new('NodeSocketFloat', 'Density W_CloudTex')
            NodeGroup.links.new(MathAdd.outputs[0], group_outputs.inputs['Density W_CloudTex'])

            # Inputs
            group_inputs = NodeGroup.nodes.new('NodeGroupInput')
            group_inputs.location = (-1250, -300)
            NodeGroup.inputs.new('NodeSocketFloat', 'Scale')
            NodeGroup.inputs.new('NodeSocketFloat', 'Point Density In')
            NodeGroup.links.new(group_inputs.outputs['Scale'], NoiseTex.inputs['Scale'])
            NodeGroup.links.new(group_inputs.outputs['Point Density In'], MathDensityMultiply.inputs[1])
            NodeGroup.links.new(group_inputs.outputs['Point Density In'], PointDensityRamp.inputs[0])

    return NodeGroup


# This routine takes an object and deletes all of the geometry in it
# and adds a bounding box to it.
# It will add or subtract the bound box size by the variable sizeDifference.

def getMeshandPutinEditMode(scene, object):

    # Go into Object Mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Deselect All
    bpy.ops.object.select_all(action='DESELECT')

    # Select the object
    object.select = True
    scene.objects.active = object

    # Go into Edit Mode
    bpy.ops.object.mode_set(mode='EDIT')

    return object.data


def maxAndMinVerts(scene, object):

    mesh = getMeshandPutinEditMode(scene, object)
    verts = mesh.vertices

    # Set the max and min verts to the first vertex on the list
    maxVert = [verts[0].co[0], verts[0].co[1], verts[0].co[2]]
    minVert = [verts[0].co[0], verts[0].co[1], verts[0].co[2]]

    # Create Max and Min Vertex array for the outer corners of the box
    for vert in verts:
        # Max vertex
        if vert.co[0] > maxVert[0]:
            maxVert[0] = vert.co[0]
        if vert.co[1] > maxVert[1]:
            maxVert[1] = vert.co[1]
        if vert.co[2] > maxVert[2]:
            maxVert[2] = vert.co[2]

        # Min Vertex
        if vert.co[0] < minVert[0]:
            minVert[0] = vert.co[0]
        if vert.co[1] < minVert[1]:
            minVert[1] = vert.co[1]
        if vert.co[2] < minVert[2]:
            minVert[2] = vert.co[2]

    return [maxVert, minVert]


def makeObjectIntoBoundBox(scene, objects, sizeDifference, takeFromObject):
    # Let's find the max and min of the reference object,
    # it can be the same as the destination object
    [maxVert, minVert] = maxAndMinVerts(scene, takeFromObject)

    # get objects mesh
    mesh = getMeshandPutinEditMode(scene, objects)

    # Add the size difference to the max size of the box
    maxVert[0] = maxVert[0] + sizeDifference
    maxVert[1] = maxVert[1] + sizeDifference
    maxVert[2] = maxVert[2] + sizeDifference

    # subtract the size difference to the min size of the box
    minVert[0] = minVert[0] - sizeDifference
    minVert[1] = minVert[1] - sizeDifference
    minVert[2] = minVert[2] - sizeDifference

    # Create arrays of verts and faces to be added to the mesh
    addVerts = []

    # X high loop
    addVerts.append([maxVert[0], maxVert[1], maxVert[2]])
    addVerts.append([maxVert[0], maxVert[1], minVert[2]])
    addVerts.append([maxVert[0], minVert[1], minVert[2]])
    addVerts.append([maxVert[0], minVert[1], maxVert[2]])

    # X low loop
    addVerts.append([minVert[0], maxVert[1], maxVert[2]])
    addVerts.append([minVert[0], maxVert[1], minVert[2]])
    addVerts.append([minVert[0], minVert[1], minVert[2]])
    addVerts.append([minVert[0], minVert[1], maxVert[2]])

    # Make the faces of the bounding box.
    addFaces = []

    # Draw a box on paper and number the vertices.
    # Use right hand rule to come up with number orders for faces on
    # the box (with normals pointing out).
    addFaces.append([0, 3, 2, 1])
    addFaces.append([4, 5, 6, 7])
    addFaces.append([0, 1, 5, 4])
    addFaces.append([1, 2, 6, 5])
    addFaces.append([2, 3, 7, 6])
    addFaces.append([0, 4, 7, 3])

    # Delete all geometry from the object.
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.delete(type='VERT')

    # Must be in object mode for from_pydata to work
    bpy.ops.object.mode_set(mode='OBJECT')

    # Add the mesh data.
    mesh.from_pydata(addVerts, [], addFaces)
    mesh.validate()

    # Update the mesh
    mesh.update()


def applyScaleRotLoc(scene, obj):
    # Deselect All
    bpy.ops.object.select_all(action='DESELECT')

    # Select the object
    obj.select = True
    scene.objects.active = obj

    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def totallyDeleteObject(scene, obj):
    bpy.data.objects.remove(obj, do_unlink=True)


def makeParent(parentobj, childobj, scene):
    applyScaleRotLoc(scene, parentobj)
    applyScaleRotLoc(scene, childobj)
    childobj.parent = parentobj


def addNewObject(scene, name, copyobj):
    # avoid creating not needed meshes pro forme
    # Create a new object
    tempme = copyobj.data
    ob_new_data = tempme.copy()
    ob_new = bpy.data.objects.new(name, ob_new_data)
    ob_new.scale = copyobj.scale
    ob_new.location = copyobj.location

    # Link new object to the given scene and select it
    scene.objects.link(ob_new)
    ob_new.select = True

    return ob_new


def getpdensitytexture(object):

    for mslot in object.material_slots:
        # Material slot can be empty
        mat = getattr(mslot, "material", None)
        if mat:
            for tslot in mat.texture_slots:
                if tslot != 'NoneType':
                    tex = tslot.texture
                    if tex.type == 'POINT_DENSITY':
                        if tex.point_density.point_source == 'PARTICLE_SYSTEM':
                            return tex


def removeParticleSystemFromObj(scene, obj):
    # Deselect All
    bpy.ops.object.select_all(action='DESELECT')

    # Select the object
    obj.select = True
    scene.objects.active = obj

    bpy.ops.object.particle_system_remove()

    # Deselect All
    bpy.ops.object.select_all(action='DESELECT')


def convertParticlesToMesh(scene, particlesobj, destobj, replacemesh):
    # Select the Destination object
    destobj.select = True
    scene.objects.active = destobj

    # Go to Edit Mode
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    # Delete everything in mesh if replace is true
    if replacemesh:
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.delete(type='VERT')

    meshPnts = destobj.data

    listCloudParticles = particlesobj.particles

    listMeshPnts = []
    for pTicle in listCloudParticles:
        listMeshPnts.append(pTicle.location)

    # Must be in object mode for from_pydata to work
    bpy.ops.object.mode_set(mode='OBJECT')

    # Add in the mesh data
    meshPnts.from_pydata(listMeshPnts, [], [])

    # Update and Validate the mesh
    meshPnts.validate()
    meshPnts.update()


def combineObjects(scene, combined, listobjs):
    # scene is the current scene
    # combined is the object we want to combine everything into
    # listobjs is the list of objects to stick into combined

    # Deselect All
    bpy.ops.object.select_all(action='DESELECT')

    # Select the new object.
    combined.select = True
    scene.objects.active = combined

    # Add data
    if len(listobjs) > 0:
        for i in listobjs:
            # Add a modifier
            bpy.ops.object.modifier_add(type='BOOLEAN')

            union = combined.modifiers
            union[0].name = "AddEmUp"
            union[0].object = i
            union[0].operation = 'UNION'

            # Apply modifier
            bpy.ops.object.modifier_apply(apply_as='DATA', modifier=union[0].name)


# Returns the action we want to take
def getActionToDo(obj):

    if not obj or obj.type != 'MESH':
        return 'NOT_OBJ_DO_NOTHING'

    elif obj is None:
        return 'NO_SELECTION_DO_NOTHING'

    elif "CloudMember" in obj:
        if obj["CloudMember"] is not None:
            if obj["CloudMember"] == "MainObj":
                return 'DEGENERATE'
            elif obj["CloudMember"] == "CreatedObj" and len(obj.particle_systems) > 0:
                return 'CLOUD_CONVERT_TO_MESH'
            else:
                return 'CLOUD_DO_NOTHING'

    elif obj.type == 'MESH':
        return 'GENERATE'

    else:
        return 'DO_NOTHING'


class VIEW3D_PT_tools_cloud(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = 'Create'
    bl_label = "Cloud Generator"
    bl_context = "objectmode"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        active_obj = context.active_object
        layout = self.layout
        col = layout.column(align=True)

        WhatToDo = getActionToDo(active_obj)

        if WhatToDo == 'DEGENERATE':
            col.operator("cloud.generate_cloud", text="DeGenerate")

        elif WhatToDo == 'CLOUD_CONVERT_TO_MESH':
            col.operator("cloud.generate_cloud", text="Convert to Mesh")

        elif WhatToDo == 'NO_SELECTION_DO_NOTHING':
            col.label(text="Select one or more")
            col.label(text="objects to generate")
            col.label(text="a cloud")

        elif WhatToDo == 'CLOUD_DO_NOTHING':
            col.label(text="Must select")
            col.label(text="bound box")

        elif WhatToDo == 'GENERATE':
            col.operator("cloud.generate_cloud", text="Generate Cloud")

            col.prop(context.scene, "cloud_type")
            col.prop(context.scene, "cloudsmoothing")
        else:
            col.label(text="Select one or more", icon="INFO")
            col.label(text="objects to generate", icon="BLANK1")
            col.label(text="a cloud", icon="BLANK1")


class GenerateCloud(Operator):
    bl_idname = "cloud.generate_cloud"
    bl_label = "Generate Cloud"
    bl_description = ("Create a Cloud, Undo a Cloud, or convert to "
                      "Mesh Cloud depending on selection\n"
                      "Needs an Active Mesh Object")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        # Prevent unsupported Execution in Local View modes
        space_data = bpy.context.space_data

        if True in space_data.layers_local_view:
            self.report({'INFO'},
                        "Works with Global Perspective modes only. Operation Cancelled")
            return {'CANCELLED'}

        # Make variable that is the active object selected by user
        active_object = context.active_object

        # Make variable scene that is current scene
        scene = context.scene

        # Parameters the user may want to change:
        # Number of points this number is multiplied by the volume to get
        # the number of points the scripts will put in the volume.

        if bpy.context.scene.render.engine == 'BLENDER_RENDER':
            numOfPoints = 1.0
            maxNumOfPoints = 100000
            maxPointDensityRadius = 1.5
            scattering = 2.5
            pointDensityRadiusFactor = 1.0
            densityScale = 1.5
        elif bpy.context.scene.render.engine == 'CYCLES':
            numOfPoints = .80
            maxNumOfPoints = 100000
            maxPointDensityRadius = 1.0
            scattering = 2.5
            pointDensityRadiusFactor = .37
            densityScale = 1.5

        # What should we do?
        WhatToDo = getActionToDo(active_object)

        if WhatToDo == 'DEGENERATE':
            # Degenerate Cloud
            mainObj = active_object

            bpy.ops.object.hide_view_clear()

            cloudMembers = active_object.children
            createdObjects = []
            definitionObjects = []

            for member in cloudMembers:
                applyScaleRotLoc(scene, member)
                if member["CloudMember"] == "CreatedObj":
                    createdObjects.append(member)
                else:
                    definitionObjects.append(member)

            for defObj in definitionObjects:
                # Delete cloudmember data from objects
                if "CloudMember" in defObj:
                    del(defObj["CloudMember"])

            for createdObj in createdObjects:
                totallyDeleteObject(scene, createdObj)

            # Delete the blend_data object
            totallyDeleteObject(scene, mainObj)

            # Select all of the left over boxes so people can immediately
            # press generate again if they want
            for eachMember in definitionObjects:
                eachMember.draw_type = 'SOLID'
                eachMember.select = True
                eachMember.hide_render = False

        elif WhatToDo == 'CLOUD_CONVERT_TO_MESH':
            cloudParticles = active_object.particle_systems.active

            bounds = active_object.parent

            # Create CloudPnts for putting points in #
            # Create a new object cloudPnts
            cloudPnts = addNewObject(scene, "CloudPoints", bounds)
            cloudPnts["CloudMember"] = "CreatedObj"
            cloudPnts.draw_type = 'WIRE'
            cloudPnts.hide_render = True

            makeParent(bounds, cloudPnts, scene)
            convertParticlesToMesh(scene, cloudParticles, cloudPnts, True)
            removeParticleSystemFromObj(scene, active_object)

            pDensity = getpdensitytexture(bounds)
            pDensity.point_density.point_source = 'OBJECT'
            pDensity.point_density.object = cloudPnts

            # Let's resize the bound box to be more accurate
            how_much_bigger = pDensity.point_density.radius
            makeObjectIntoBoundBox(scene, bounds, how_much_bigger, cloudPnts)

        else:
            # Generate Cloud

            # Create Combined Object bounds #
            # Make a list of all Selected objects
            selectedObjects = bpy.context.selected_objects
            if not selectedObjects:
                selectedObjects = [bpy.context.active_object]

            # Create a new object bounds
            bounds = addNewObject(
                            scene, "CloudBounds",
                            selectedObjects[0]
                            )

            bounds.draw_type = 'BOUNDS'
            bounds.hide_render = False

            # Just add a Definition Property designating this
            # as the blend_data object
            bounds["CloudMember"] = "MainObj"

            # Since we used iteration 0 to copy with object we
            # delete it off the list.
            firstObject = selectedObjects[0]
            del selectedObjects[0]

            # Apply location Rotation and Scale to all objects involved
            applyScaleRotLoc(scene, bounds)
            for each in selectedObjects:
                applyScaleRotLoc(scene, each)

            # Let's combine all of them together.
            combineObjects(scene, bounds, selectedObjects)

            # Let's add some property info to the objects
            for selObj in selectedObjects:
                selObj["CloudMember"] = "DefinitionObj"
                selObj.name = "DefinitionObj"
                selObj.draw_type = 'WIRE'
                selObj.hide_render = True
                selObj.hide = True
                makeParent(bounds, selObj, scene)

            # Do the same to the 1. object since it is no longer in list.
            firstObject["CloudMember"] = "DefinitionObj"
            firstObject.name = "DefinitionObj"
            firstObject.draw_type = 'WIRE'
            firstObject.hide_render = True
            makeParent(bounds, firstObject, scene)

            # Create Cloud for putting Cloud Mesh #
            # Create a new object cloud.
            cloud = addNewObject(scene, "CloudMesh", bounds)
            cloud["CloudMember"] = "CreatedObj"
            cloud.draw_type = 'WIRE'
            cloud.hide_render = True

            makeParent(bounds, cloud, scene)

            bpy.ops.object.editmode_toggle()
            bpy.ops.mesh.select_all(action='SELECT')

            # Don't subdivide object or smooth if smoothing box not checked.
            if scene.cloudsmoothing:
                bpy.ops.mesh.subdivide(number_cuts=2, fractal=0, smoothness=1)
                bpy.ops.mesh.vertices_smooth(repeat=20)
            bpy.ops.mesh.tris_convert_to_quads()
            bpy.ops.mesh.faces_shade_smooth()
            bpy.ops.object.editmode_toggle()

            # Create Particles in cloud obj #

            # Set time to 0
            scene.frame_current = 0

            # Add a new particle system
            bpy.ops.object.particle_system_add()

            # Particle settings setting it up!
            cloudParticles = cloud.particle_systems.active
            cloudParticles.name = "CloudParticles"
            cloudParticles.settings.frame_start = 0
            cloudParticles.settings.frame_end = 0
            cloudParticles.settings.emit_from = 'VOLUME'
            cloudParticles.settings.lifetime = scene.frame_end
            cloudParticles.settings.draw_method = 'DOT'
            cloudParticles.settings.render_type = 'NONE'
            cloudParticles.settings.distribution = 'RAND'
            cloudParticles.settings.physics_type = 'NEWTON'
            cloudParticles.settings.normal_factor = 0

            # Gravity does not affect the particle system
            eWeights = cloudParticles.settings.effector_weights
            eWeights.gravity = 0

            # Create Volume Material #
            # Deselect All
            bpy.ops.object.select_all(action='DESELECT')

            # Select the object.
            bounds.select = True
            scene.objects.active = bounds

            # Turn bounds object into a box. Use itself as a reference
            makeObjectIntoBoundBox(scene, bounds, 1.0, bounds)

            # Delete all material slots in bounds object
            for i in range(len(bounds.material_slots)):
                bounds.active_material_index = i - 1
                bpy.ops.object.material_slot_remove()

            # Add a new material
            cloudMaterial = bpy.data.materials.new("CloudMaterial")
            bpy.ops.object.material_slot_add()
            bounds.material_slots[0].material = cloudMaterial

            # Set time
            scene.frame_current = 1

            # Set Up Material for Blender Internal
            if bpy.context.scene.render.engine == 'BLENDER_RENDER':
                # Set Up the Cloud Material
                cloudMaterial.name = "CloudMaterial"
                cloudMaterial.type = 'VOLUME'
                mVolume = cloudMaterial.volume
                mVolume.scattering = scattering
                mVolume.density = 0
                mVolume.density_scale = densityScale
                mVolume.transmission_color = 3.0, 3.0, 3.0
                mVolume.step_size = 0.1
                mVolume.use_light_cache = True
                mVolume.cache_resolution = 45

                # Add a texture
                # vMaterialTextureSlots = cloudMaterial.texture_slots  # UNUSED
                cloudtex = bpy.data.textures.new("CloudTex", type='CLOUDS')
                cloudtex.noise_type = 'HARD_NOISE'
                cloudtex.noise_scale = 2
                mtex = cloudMaterial.texture_slots.add()
                mtex.texture = cloudtex
                mtex.texture_coords = 'ORCO'
                mtex.use_map_color_diffuse = True

                # Set time
                scene.frame_current = 1

                # Add a Point Density texture
                pDensity = bpy.data.textures.new("CloudPointDensity", 'POINT_DENSITY')

                mtex = cloudMaterial.texture_slots.add()
                mtex.texture = pDensity
                mtex.texture_coords = 'GLOBAL'
                mtex.use_map_density = True
                mtex.use_rgb_to_intensity = True
                mtex.texture_coords = 'GLOBAL'

                pDensity.point_density.vertex_cache_space = 'WORLD_SPACE'
                pDensity.point_density.use_turbulence = True
                pDensity.point_density.noise_basis = 'VORONOI_F2'
                pDensity.point_density.turbulence_depth = 3

                pDensity.use_color_ramp = True
                pRamp = pDensity.color_ramp
                # pRamp.use_interpolation = 'LINEAR'
                pRampElements = pRamp.elements
                # pRampElements[1].position = .9
                # pRampElements[1].color = 0.18, 0.18, 0.18, 0.8
                bpy.ops.texture.slot_move(type='UP')

            # Set Up Material for Cycles Engine
            elif bpy.context.scene.render.engine == 'CYCLES':
                VolumePropertiesGroup = CreateNodeGroup('CloudGen_VolumeProperties')
                CloudTexPropertiesGroup = CreateNodeGroup('CloudGen_TextureProperties')

                cloudMaterial.name = "CloudMaterial"
                # Add a texture
                cloudtex = bpy.data.textures.new("CloudTex", type='CLOUDS')
                cloudtex.noise_type = 'HARD_NOISE'
                cloudtex.noise_scale = 2

                cloudMaterial.use_nodes = True
                cloudTree = cloudMaterial.node_tree
                cloudMatNodes = cloudTree.nodes
                cloudMatNodes.clear()

                outputNode = cloudMatNodes.new('ShaderNodeOutputMaterial')
                outputNode.location = (200, 300)

                tranparentNode = cloudMatNodes.new('ShaderNodeBsdfTransparent')
                tranparentNode.location = (0, 300)

                volumeGroup = cloudMatNodes.new("ShaderNodeGroup")
                volumeGroup.node_tree = VolumePropertiesGroup
                volumeGroup.location = (0, 150)

                cloudTexGroup = cloudMatNodes.new("ShaderNodeGroup")
                cloudTexGroup.node_tree = CloudTexPropertiesGroup
                cloudTexGroup.location = (-200, 150)

                PointDensityNode = cloudMatNodes.new("ShaderNodeTexPointDensity")
                PointDensityNode.location = (-400, 150)
                PointDensityNode.resolution = 100
                PointDensityNode.space = 'OBJECT'
                PointDensityNode.interpolation = 'Linear'
                # PointDensityNode.color_source = 'CONSTANT'

                cloudTree.links.new(outputNode.inputs[0], tranparentNode.outputs[0])
                cloudTree.links.new(outputNode.inputs[1], volumeGroup.outputs[0])
                cloudTree.links.new(volumeGroup.inputs[0], cloudTexGroup.outputs[0])
                cloudTree.links.new(cloudTexGroup.inputs[1], PointDensityNode.outputs[1])

            # Estimate the number of particles for the size of bounds.
            volumeBoundBox = (bounds.dimensions[0] * bounds.dimensions[1] * bounds.dimensions[2])
            numParticles = int((2.4462 * volumeBoundBox + 430.4) * numOfPoints)
            if numParticles > maxNumOfPoints:
                numParticles = maxNumOfPoints
            if numParticles < 10000:
                numParticles = int(numParticles + 15 * volumeBoundBox)

            # Set the number of particles according to the volume of bounds
            cloudParticles.settings.count = numParticles

            PDensityRadius = (.00013764 * volumeBoundBox + .3989) * pointDensityRadiusFactor

            if bpy.context.scene.render.engine == 'BLENDER_RENDER':
                pDensity.point_density.radius = PDensityRadius

                if pDensity.point_density.radius > maxPointDensityRadius:
                    pDensity.point_density.radius = maxPointDensityRadius

            elif bpy.context.scene.render.engine == 'CYCLES':
                PointDensityNode.radius = PDensityRadius

                if PDensityRadius > maxPointDensityRadius:
                    PointDensityNode.radius = maxPointDensityRadius

            # Set time to 1.
            scene.frame_current = 1

            if not scene.cloudparticles:
                # Create CloudPnts for putting points in #
                # Create a new object cloudPnts
                cloudPnts = addNewObject(scene, "CloudPoints", bounds)
                cloudPnts["CloudMember"] = "CreatedObj"
                cloudPnts.draw_type = 'WIRE'
                cloudPnts.hide_render = True

                makeParent(bounds, cloudPnts, scene)
                convertParticlesToMesh(scene, cloudParticles, cloudPnts, True)

                # Add a modifier.
                bpy.ops.object.modifier_add(type='DISPLACE')

                cldPntsModifiers = cloudPnts.modifiers
                cldPntsModifiers[0].name = "CloudPnts"
                cldPntsModifiers[0].texture = cloudtex
                cldPntsModifiers[0].texture_coords = 'OBJECT'
                cldPntsModifiers[0].texture_coords_object = cloud
                cldPntsModifiers[0].strength = -1.4

                # Apply modifier
                bpy.ops.object.modifier_apply(apply_as='DATA', modifier=cldPntsModifiers[0].name)

                if bpy.context.scene.render.engine == 'BLENDER_RENDER':
                    pDensity.point_density.point_source = 'OBJECT'
                    pDensity.point_density.object = cloudPnts

                elif bpy.context.scene.render.engine == 'CYCLES':
                    PointDensityNode.point_source = 'OBJECT'
                    PointDensityNode.object = cloudPnts

                removeParticleSystemFromObj(scene, cloud)

            else:
                if bpy.context.scene.render.engine == 'BLENDER_RENDER':
                    pDensity.point_density.point_source = 'PARTICLE_SYSTEM'
                    pDensity.point_density.object = cloud
                    pDensity.point_density.particle_system = cloudParticles

                elif bpy.context.scene.render.engine == 'CYCLES':
                    PointDensityNode.point_source = 'PARTICLE_SYSTEM'
                    PointDensityNode.particle_system = cloudPnts

            if bpy.context.scene.render.engine == 'BLENDER_RENDER':
                if scene.cloud_type == '1':  # Cumulous
                    mVolume.density_scale = 2.22
                    pDensity.point_density.turbulence_depth = 10
                    pDensity.point_density.turbulence_strength = 6.3
                    pDensity.point_density.turbulence_scale = 2.9
                    pRampElements[1].position = .606
                    pDensity.point_density.radius = pDensity.point_density.radius + 0.1

                elif scene.cloud_type == '2':  # Cirrus
                    pDensity.point_density.turbulence_strength = 22
                    mVolume.transmission_color = 3.5, 3.5, 3.5
                    mVolume.scattering = 0.13

                elif scene.cloud_type == '3':  # Explosion
                    mVolume.emission = 1.42
                    mtex.use_rgb_to_intensity = False
                    pRampElements[0].position = 0.825
                    pRampElements[0].color = 0.119, 0.119, 0.119, 1
                    pRampElements[1].position = .049
                    pRampElements[1].color = 1.0, 1.0, 1.0, 0
                    pDensity.point_density.turbulence_strength = 1.5
                    pRampElement1 = pRampElements.new(.452)
                    pRampElement1.color = 0.814, 0.112, 0, 1
                    pRampElement2 = pRampElements.new(.234)
                    pRampElement2.color = 0.814, 0.310, 0.002, 1
                    pRampElement3 = pRampElements.new(0.669)
                    pRampElement3.color = 0.0, 0.0, 0.040, 1

            elif bpy.context.scene.render.engine == 'CYCLES':

                volumeGroup.inputs['Absorption Multiply'].default_value = 50
                volumeGroup.inputs['Absorption Color'].default_value = (1.0, 1.0, 1.0, 1.0)
                volumeGroup.inputs['Scatter Multiply'].default_value = 30
                volumeGroup.inputs['Scatter Color'].default_value = (.58, .58, .58, 1.0)
                volumeGroup.inputs['Emission Amount'].default_value = .1
                volumeGroup.inputs['Cloud Brightness'].default_value = 1.3
                noiseCloudScale = volumeBoundBox * (-.001973) + 5.1216
                if noiseCloudScale < .05:
                    noiseCloudScale = .05
                cloudTexGroup.inputs['Scale'].default_value = noiseCloudScale

                # to cloud to view in cycles in render mode we need to hide geometry meshes...
                firstObject.hide = True
                cloud.hide = True

            # Select the object.
            bounds.select = True
            scene.objects.active = bounds

            # Let's resize the bound box to be more accurate.
            how_much_bigger = PDensityRadius + 0.1

            # If it's a particle cloud use cloud mesh if otherwise use point mesh
            if not scene.cloudparticles:
                makeObjectIntoBoundBox(scene, bounds, how_much_bigger, cloudPnts)
            else:
                makeObjectIntoBoundBox(scene, bounds, how_much_bigger, cloud)

            cloud_string = "Cumulous" if scene.cloud_type == '1' else "Cirrus" if \
                           scene.cloud_type == '2' else "Stratus" if \
                           scene.cloud_type == '0' else "Explosion"

            self.report({'INFO'},
                         "Created the cloud of type {}".format(cloud_string))

        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.cloudparticles = BoolProperty(
            name="Particles",
            description="Generate Cloud as Particle System",
            default=False
            )
    bpy.types.Scene.cloudsmoothing = BoolProperty(
            name="Smoothing",
            description="Smooth Resultant Geometry From Gen Cloud Operation",
            default=True
            )
    bpy.types.Scene.cloud_type = EnumProperty(
            name="Type",
            description="Select the type of cloud to create with material settings",
            items=[("0", "Stratus", "Generate Stratus (foggy) Cloud"),
                   ("1", "Cumulous", "Generate Cumulous (puffy) Cloud"),
                   ("2", "Cirrus", "Generate Cirrus (wispy) Cloud"),
                   ("3", "Explosion", "Generate Explosion"),
                  ],
            default='0'
            )


def unregister():
    bpy.utils.unregister_module(__name__)

    del bpy.types.Scene.cloudparticles
    del bpy.types.Scene.cloudsmoothing
    del bpy.types.Scene.cloud_type


if __name__ == "__main__":
    register()
