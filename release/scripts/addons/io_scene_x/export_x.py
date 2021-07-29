# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation, either version 3
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#  All rights reserved.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

from math import radians, pi

import bpy
from mathutils import *


class DirectXExporter:
    def __init__(self, Config, context):
        self.Config = Config
        self.context = context

        self.Log("Begin verbose logging ----------\n")

        self.File = File(self.Config.filepath)

        self.Log("Setting up coordinate system...")

        # SystemMatrix converts from right-handed, z-up to the target coordinate system
        self.SystemMatrix = Matrix()

        if self.Config.CoordinateSystem == 'LEFT_HANDED':
            self.SystemMatrix *= Matrix.Scale(-1, 4, Vector((0, 0, 1)))

        if self.Config.UpAxis == 'Y':
            self.SystemMatrix *= Matrix.Rotation(radians(-90), 4, 'X')

        self.Log("Done")

        self.Log("Generating object lists for export...")
        if self.Config.SelectedOnly:
            ExportList = list(self.context.selected_objects)
        else:
            ExportList = list(self.context.scene.objects)

        # ExportMap maps Blender objects to ExportObjects
        ExportMap = {}
        for Object in ExportList:
            if Object.type == 'EMPTY':
                ExportMap[Object] = EmptyExportObject(self.Config, self, Object)
            elif Object.type == 'MESH':
                ExportMap[Object] = MeshExportObject(self.Config, self,
                    Object)
            elif Object.type == 'ARMATURE':
                ExportMap[Object] = ArmatureExportObject(self.Config, self,
                    Object)

        # Find the objects who do not have a parent or whose parent we are
        # not exporting
        self.RootExportList = [Object for Object in ExportMap.values()
            if Object.BlenderObject.parent not in ExportList]
        self.RootExportList = Util.SortByNameField(self.RootExportList)

        self.ExportList = Util.SortByNameField(ExportMap.values())

        # Determine each object's children from the pool of ExportObjects
        for Object in ExportMap.values():
            Children = Object.BlenderObject.children
            Object.Children = []
            for Child in Children:
                if Child in ExportMap:
                    Object.Children.append(ExportMap[Child])
        self.Log("Done")

        self.AnimationWriter = None
        if self.Config.ExportAnimation:
            self.Log("Gathering animation data...")

            # Collect all animated object data
            AnimationGenerators = self.__GatherAnimationGenerators()

            # Split the data up into animation sets based on user options
            if self.Config.ExportActionsAsSets:
                self.AnimationWriter = SplitSetAnimationWriter(self.Config,
                    self, AnimationGenerators)
            else:
                self.AnimationWriter = JoinedSetAnimationWriter(self.Config,
                    self, AnimationGenerators)
            self.Log("Done")

    # "Public" Interface

    def Export(self):
        self.Log("Exporting to {}".format(self.File.FilePath),
            MessageVerbose=False)

        # Export everything
        self.Log("Opening file...")
        self.File.Open()
        self.Log("Done")

        self.Log("Writing header...")
        self.__WriteHeader()
        self.Log("Done")

        self.Log("Opening Root frame...")
        self.__OpenRootFrame()
        self.Log("Done")

        self.Log("Writing objects...")
        for Object in self.RootExportList:
            Object.Write()
        self.Log("Done writing objects")

        self.Log("Closing Root frame...")
        self.__CloseRootFrame()
        self.Log("Done")

        if self.AnimationWriter is not None:
            self.Log("Writing animation set(s)...")
            self.AnimationWriter.WriteAnimationSets()
            self.Log("Done writing animation set(s)")

        self.Log("Closing file...")
        self.File.Close()
        self.Log("Done")

    def Log(self, String, MessageVerbose=True):
        if self.Config.Verbose is True or MessageVerbose == False:
            print(String)

    # "Private" Methods

    def __WriteHeader(self):
        self.File.Write("xof 0303txt 0032\n\n")

        # Write the headers that are required by some engines as needed

        if self.Config.IncludeFrameRate:
            self.File.Write("template AnimTicksPerSecond {\n\
  <9E415A43-7BA6-4a73-8743-B73D47E88476>\n\
  DWORD AnimTicksPerSecond;\n\
}\n\n")
        if self.Config.ExportSkinWeights:
            self.File.Write("template XSkinMeshHeader {\n\
  <3cf169ce-ff7c-44ab-93c0-f78f62d172e2>\n\
  WORD nMaxSkinWeightsPerVertex;\n\
  WORD nMaxSkinWeightsPerFace;\n\
  WORD nBones;\n\
}\n\n\
template SkinWeights {\n\
  <6f0d123b-bad2-4167-a0d0-80224f25fabb>\n\
  STRING transformNodeName;\n\
  DWORD nWeights;\n\
  array DWORD vertexIndices[nWeights];\n\
  array float weights[nWeights];\n\
  Matrix4x4 matrixOffset;\n\
}\n\n")

    # Start the Root frame and write its transform matrix
    def __OpenRootFrame(self):
        self.File.Write("Frame Root {\n")
        self.File.Indent()

        self.File.Write("FrameTransformMatrix {\n")
        self.File.Indent()

        # Write the matrix that will convert Blender's coordinate space into
        # DirectX's.
        Util.WriteMatrix(self.File, self.SystemMatrix)

        self.File.Unindent()
        self.File.Write("}\n")

    def __CloseRootFrame(self):
        self.File.Unindent()
        self.File.Write("} // End of Root\n")

    def __GatherAnimationGenerators(self):
        Generators = []

        # If all animation data is to be lumped into one AnimationSet,
        if not self.Config.ExportActionsAsSets:
            # Build the appropriate generators for each object's type
            for Object in self.ExportList:
                if Object.BlenderObject.type == 'ARMATURE':
                    Generators.append(ArmatureAnimationGenerator(self.Config,
                        None, Object))
                else:
                    Generators.append(GenericAnimationGenerator(self.Config,
                        None, Object))
        # Otherwise,
        else:
            # Keep track of which objects have no action.  These will be
            # lumped together in a Default_Action AnimationSet.
            ActionlessObjects = []

            for Object in self.ExportList:
                if Object.BlenderObject.animation_data is None:
                    ActionlessObjects.append(Object)
                    continue
                else:
                    if Object.BlenderObject.animation_data.action is None:
                        ActionlessObjects.append(Object)
                        continue

                # If an object has an action, build its appropriate generator
                if Object.BlenderObject.type == 'ARMATURE':
                    Generators.append(ArmatureAnimationGenerator(self.Config,
                        Util.SafeName(
                            Object.BlenderObject.animation_data.action.name),
                        Object))
                else:
                    Generators.append(GenericAnimationGenerator(self.Config,
                        Util.SafeName(
                            Object.BlenderObject.animation_data.action.name),
                        Object))

            # If we should export unused actions as if the first armature was
            # using them,
            if self.Config.AttachToFirstArmature:
                # Find the first armature
                FirstArmature = None
                for Object in self.ExportList:
                    if Object.BlenderObject.type == 'ARMATURE':
                        FirstArmature = Object
                        break

                if FirstArmature is not None:
                    # Determine which actions are not used
                    UsedActions = [BlenderObject.animation_data.action
                        for BlenderObject in bpy.data.objects
                        if BlenderObject.animation_data is not None]
                    FreeActions = [Action for Action in bpy.data.actions
                        if Action not in UsedActions]

                    # If the first armature has no action, remove it from the
                    # actionless objects so it doesn't end up in Default_Action
                    if FirstArmature in ActionlessObjects and len(FreeActions):
                        ActionlessObjects.remove(FirstArmature)

                    # Keep track of the first armature's animation data so we
                    # can restore it after export
                    OldAction = None
                    NoData = False
                    if FirstArmature.BlenderObject.animation_data is not None:
                        OldAction = \
                            FirstArmature.BlenderObject.animation_data.action
                    else:
                        NoData = True
                        FirstArmature.BlenderObject.animation_data_create()

                    # Build a generator for each unused action
                    for Action in FreeActions:
                        FirstArmature.BlenderObject.animation_data.action = \
                            Action

                        Generators.append(ArmatureAnimationGenerator(
                            self.Config, Util.SafeName(Action.name),
                            FirstArmature))

                    # Restore old animation data
                    FirstArmature.BlenderObject.animation_data.action = \
                        OldAction

                    if NoData:
                        FirstArmature.BlenderObject.animation_data_clear()

            # Build a special generator for all actionless objects
            if len(ActionlessObjects):
                Generators.append(GroupAnimationGenerator(self.Config,
                    "Default_Action", ActionlessObjects))

        return Generators

# This class wraps a Blender object and writes its data to the file
class ExportObject: # Base class, do not use
    def __init__(self, Config, Exporter, BlenderObject):
        self.Config = Config
        self.Exporter = Exporter
        self.BlenderObject = BlenderObject

        self.name = self.BlenderObject.name # Simple alias
        self.SafeName = Util.SafeName(self.BlenderObject.name)
        self.Children = []

    def __repr__(self):
        return "[ExportObject: {}]".format(self.BlenderObject.name)

    # "Public" Interface

    def Write(self):
        self.Exporter.Log("Opening frame for {}".format(self))
        self._OpenFrame()

        self.Exporter.Log("Writing children of {}".format(self))
        self._WriteChildren()

        self._CloseFrame()
        self.Exporter.Log("Closed frame of {}".format(self))

    # "Protected" Interface

    def _OpenFrame(self):
        self.Exporter.File.Write("Frame {} {{\n".format(self.SafeName))
        self.Exporter.File.Indent()

        self.Exporter.File.Write("FrameTransformMatrix {\n")
        self.Exporter.File.Indent()
        Util.WriteMatrix(self.Exporter.File, self.BlenderObject.matrix_local)
        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}\n")

    def _CloseFrame(self):
        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {}\n".format(self.SafeName))

    def _WriteChildren(self):
        for Child in Util.SortByNameField(self.Children):
            Child.Write()

# Simple decorator implemenation for ExportObject.  Used by empty objects
class EmptyExportObject(ExportObject):
    def __init__(self, Config, Exporter, BlenderObject):
        ExportObject.__init__(self, Config, Exporter, BlenderObject)

    def __repr__(self):
        return "[EmptyExportObject: {}]".format(self.name)

# Mesh object implementation of ExportObject
class MeshExportObject(ExportObject):
    def __init__(self, Config, Exporter, BlenderObject):
        ExportObject.__init__(self, Config, Exporter, BlenderObject)

    def __repr__(self):
        return "[MeshExportObject: {}]".format(self.name)

    # "Public" Interface

    def Write(self):
        self.Exporter.Log("Opening frame for {}".format(self))
        self._OpenFrame()

        if self.Config.ExportMeshes:
            self.Exporter.Log("Generating mesh for export...")
            # Generate the export mesh
            Mesh = None
            if self.Config.ApplyModifiers:
                # Certain modifiers shouldn't be applied in some cases
                # Deactivate them until after mesh generation is complete

                DeactivatedModifierList = []

                # If we're exporting armature data, we shouldn't apply
                # armature modifiers to the mesh
                if self.Config.ExportSkinWeights:
                    DeactivatedModifierList = [Modifier
                        for Modifier in self.BlenderObject.modifiers
                        if Modifier.type == 'ARMATURE' and \
                        Modifier.show_viewport]

                for Modifier in DeactivatedModifierList:
                    Modifier.show_viewport = False

                Mesh = self.BlenderObject.to_mesh(self.Exporter.context.scene,
                    True, 'PREVIEW')

                # Restore the deactivated modifiers
                for Modifier in DeactivatedModifierList:
                    Modifier.show_viewport = True
            else:
                Mesh = self.BlenderObject.to_mesh(self.Exporter.context.scene,
                    False, 'PREVIEW')
            self.Exporter.Log("Done")

            self.__WriteMesh(Mesh)

            # Cleanup
            bpy.data.meshes.remove(Mesh)

        self.Exporter.Log("Writing children of {}".format(self))
        self._WriteChildren()

        self._CloseFrame()
        self.Exporter.Log("Closed frame of {}".format(self))

    # "Protected"

    # This class provides a general system for indexing a mesh, depending on
    # exporter needs.  For instance, some options require us to duplicate each
    # vertex of each face, some can reuse vertex data.  For those we'd use
    # _UnrolledFacesMeshEnumerator and _OneToOneMeshEnumerator respectively.
    class _MeshEnumerator:
        def __init__(self, Mesh):
            self.Mesh = Mesh

            # self.vertices and self.PolygonVertexIndices relate to the
            # original mesh in the following way:

            # Mesh.vertices[Mesh.polygons[x].vertices[y]] ==
            # self.vertices[self.PolygonVertexIndices[x][y]]

            self.vertices = None
            self.PolygonVertexIndices = None

    # Represents the mesh as it is inside Blender
    class _OneToOneMeshEnumerator(_MeshEnumerator):
        def __init__(self, Mesh):
            MeshExportObject._MeshEnumerator.__init__(self, Mesh)

            self.vertices = Mesh.vertices

            self.PolygonVertexIndices = tuple(tuple(Polygon.vertices)
                for Polygon in Mesh.polygons)

    # Duplicates each vertex for each face
    class _UnrolledFacesMeshEnumerator(_MeshEnumerator):
        def __init__(self, Mesh):
            MeshExportObject._MeshEnumerator.__init__(self, Mesh)

            self.vertices = []
            for Polygon in Mesh.polygons:
                self.vertices += [Mesh.vertices[VertexIndex]
                    for VertexIndex in Polygon.vertices]
            self.vertices = tuple(self.vertices)

            self.PolygonVertexIndices = []
            Index = 0
            for Polygon in Mesh.polygons:
                self.PolygonVertexIndices.append(tuple(range(Index,
                    Index + len(Polygon.vertices))))
                Index += len(Polygon.vertices)

    # "Private" Methods

    def __WriteMesh(self, Mesh):
        self.Exporter.Log("Writing mesh vertices...")
        self.Exporter.File.Write("Mesh {{ // {} mesh\n".format(self.SafeName))
        self.Exporter.File.Indent()

        # Create the mesh enumerator based on options
        MeshEnumerator = None
        if (self.Config.ExportUVCoordinates and Mesh.uv_textures) or \
            (self.Config.ExportVertexColors and Mesh.vertex_colors) or \
            (self.Config.ExportSkinWeights):
            MeshEnumerator = MeshExportObject._UnrolledFacesMeshEnumerator(Mesh)
        else:
            MeshEnumerator = MeshExportObject._OneToOneMeshEnumerator(Mesh)

        # Write vertex positions
        VertexCount = len(MeshEnumerator.vertices)
        self.Exporter.File.Write("{};\n".format(VertexCount))
        for Index, Vertex in enumerate(MeshEnumerator.vertices):
            Position = Vertex.co
            self.Exporter.File.Write("{:9f};{:9f};{:9f};".format(
                        Position[0], Position[1], Position[2]))

            if Index == VertexCount - 1:
                self.Exporter.File.Write(";\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)

        # Write face definitions
        PolygonCount = len(MeshEnumerator.PolygonVertexIndices)
        self.Exporter.File.Write("{};\n".format(PolygonCount))
        for Index, PolygonVertexIndices in \
            enumerate(MeshEnumerator.PolygonVertexIndices):

            self.Exporter.File.Write("{};".format(len(PolygonVertexIndices)))

            if self.Config.CoordinateSystem == 'LEFT_HANDED':
                PolygonVertexIndices = PolygonVertexIndices[::-1]

            for VertexCountIndex, VertexIndex in \
                enumerate(PolygonVertexIndices):

                if VertexCountIndex == len(PolygonVertexIndices) - 1:
                    self.Exporter.File.Write("{};".format(VertexIndex),
                        Indent=False)
                else:
                    self.Exporter.File.Write("{},".format(VertexIndex),
                        Indent=False)

            if Index == PolygonCount - 1:
                self.Exporter.File.Write(";\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)
        self.Exporter.Log("Done")

        # Write the other mesh components

        if self.Config.ExportNormals:
            self.Exporter.Log("Writing mesh normals...")
            self.__WriteMeshNormals(Mesh)
            self.Exporter.Log("Done")

        if self.Config.ExportUVCoordinates:
            self.Exporter.Log("Writing mesh UV coordinates...")
            self.__WriteMeshUVCoordinates(Mesh)
            self.Exporter.Log("Done")

        if self.Config.ExportMaterials:
            self.Exporter.Log("Writing mesh materials...")
            if self.Config.ExportActiveImageMaterials:
                self.Exporter.Log("Referencing active images instead of "\
                    "material image textures.")
                self.__WriteMeshActiveImageMaterials(Mesh)
            else:
                self.__WriteMeshMaterials(Mesh)
            self.Exporter.Log("Done")

        if self.Config.ExportVertexColors:
            self.Exporter.Log("Writing mesh vertex colors...")
            self.__WriteMeshVertexColors(Mesh, MeshEnumerator=MeshEnumerator)
            self.Exporter.Log("Done")

        if self.Config.ExportSkinWeights:
            self.Exporter.Log("Writing mesh skin weights...")
            self.__WriteMeshSkinWeights(Mesh, MeshEnumerator=MeshEnumerator)
            self.Exporter.Log("Done")

        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {} mesh\n".format(self.SafeName))

    def __WriteMeshNormals(self, Mesh, MeshEnumerator=None):
        # Since mesh normals only need their face counts and vertices per face
        # to match up with the other mesh data, we can optimize export with
        # this enumerator.  Exports each vertex's normal when a face is shaded
        # smooth, and exports the face normal only once when a face is shaded
        # flat.
        class _NormalsMeshEnumerator(MeshExportObject._MeshEnumerator):
            def __init__(self, Mesh):
                MeshExportObject._MeshEnumerator(Mesh)

                self.vertices = []
                self.PolygonVertexIndices = []

                Index = 0
                for Polygon in Mesh.polygons:
                    if not Polygon.use_smooth:
                        self.vertices.append(Polygon)
                        self.PolygonVertexIndices.append(
                            tuple(len(Polygon.vertices) * [Index]))
                        Index += 1
                    else:
                        for VertexIndex in Polygon.vertices:
                            self.vertices.append(Mesh.vertices[VertexIndex])
                        self.PolygonVertexIndices.append(
                            tuple(range(Index, Index + len(Polygon.vertices))))
                        Index += len(Polygon.vertices)

        if MeshEnumerator is None:
            MeshEnumerator = _NormalsMeshEnumerator(Mesh)

        self.Exporter.File.Write("MeshNormals {{ // {} normals\n".format(
            self.SafeName))
        self.Exporter.File.Indent()

        NormalCount = len(MeshEnumerator.vertices)
        self.Exporter.File.Write("{};\n".format(NormalCount))

        # Write mesh normals.
        for Index, Vertex in enumerate(MeshEnumerator.vertices):
            Normal = Vertex.normal
            if self.Config.FlipNormals:
                Normal = -1.0 * Vertex.normal

            self.Exporter.File.Write("{:9f};{:9f};{:9f};".format(Normal[0],
                Normal[1], Normal[2]))

            if Index == NormalCount - 1:
                self.Exporter.File.Write(";\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)

        # Write face definitions.
        FaceCount = len(MeshEnumerator.PolygonVertexIndices)
        self.Exporter.File.Write("{};\n".format(FaceCount))
        for Index, Polygon in enumerate(MeshEnumerator.PolygonVertexIndices):
            self.Exporter.File.Write("{};".format(len(Polygon)))

            if self.Config.CoordinateSystem == 'LEFT_HANDED':
                Polygon = Polygon[::-1]

            for VertexCountIndex, VertexIndex in enumerate(Polygon):
                if VertexCountIndex == len(Polygon) - 1:
                    self.Exporter.File.Write("{};".format(VertexIndex),
                        Indent=False)
                else:
                    self.Exporter.File.Write("{},".format(VertexIndex),
                        Indent=False)

            if Index == FaceCount - 1:
                self.Exporter.File.Write(";\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)

        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {} normals\n".format(
            self.SafeName))

    def __WriteMeshUVCoordinates(self, Mesh):
        if not Mesh.uv_textures:
            return

        self.Exporter.File.Write("MeshTextureCoords {{ // {} UV coordinates\n" \
            .format(self.SafeName))
        self.Exporter.File.Indent()

        UVCoordinates = Mesh.uv_layers.active.data

        VertexCount = 0
        for Polygon in Mesh.polygons:
            VertexCount += len(Polygon.vertices)

        # Gather and write UV coordinates
        Index = 0
        self.Exporter.File.Write("{};\n".format(VertexCount))
        for Polygon in Mesh.polygons:
            Vertices = []
            for Vertex in [UVCoordinates[Vertex] for Vertex in
                Polygon.loop_indices]:
                Vertices.append(tuple(Vertex.uv))
            for Vertex in Vertices:
                self.Exporter.File.Write("{:9f};{:9f};".format(Vertex[0],
                    1.0 - Vertex[1]))
                Index += 1
                if Index == VertexCount:
                    self.Exporter.File.Write(";\n", Indent=False)
                else:
                    self.Exporter.File.Write(",\n", Indent=False)

        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {} UV coordinates\n".format(
            self.SafeName))

    def __WriteMeshMaterials(self, Mesh):
        def WriteMaterial(Exporter, Material):
            def GetMaterialTextureFileName(Material):
                if Material:
                    # Create a list of Textures that have type 'IMAGE'
                    ImageTextures = [Material.texture_slots[TextureSlot].texture
                        for TextureSlot in Material.texture_slots.keys()
                        if Material.texture_slots[TextureSlot].texture.type ==
                        'IMAGE']
                    # Refine to only image file names if applicable
                    ImageFiles = [bpy.path.basename(Texture.image.filepath)
                        for Texture in ImageTextures
                        if getattr(Texture.image, "source", "") == 'FILE']
                    if ImageFiles:
                        return ImageFiles[0]
                return None

            Exporter.File.Write("Material {} {{\n".format(
                Util.SafeName(Material.name)))
            Exporter.File.Indent()

            Diffuse = list(Vector(Material.diffuse_color) *
                Material.diffuse_intensity)
            Diffuse.append(Material.alpha)
            # Map Blender's range of 1 - 511 to 0 - 1000
            Specularity = 1000 * (Material.specular_hardness - 1.0) / 510.0
            Specular = list(Vector(Material.specular_color) *
                Material.specular_intensity)

            Exporter.File.Write("{:9f};{:9f};{:9f};{:9f};;\n".format(Diffuse[0],
                Diffuse[1], Diffuse[2], Diffuse[3]))
            Exporter.File.Write(" {:9f};\n".format(Specularity))
            Exporter.File.Write("{:9f};{:9f};{:9f};;\n".format(Specular[0],
                Specular[1], Specular[2]))
            Exporter.File.Write(" 0.000000; 0.000000; 0.000000;;\n")

            TextureFileName = GetMaterialTextureFileName(Material)
            if TextureFileName:
                Exporter.File.Write("TextureFilename {{\"{}\";}}\n".format(
                    TextureFileName))

            Exporter.File.Unindent()
            Exporter.File.Write("}\n");

        Materials = Mesh.materials
        # Do not write materials if there are none
        if not Materials.keys():
            return

        self.Exporter.File.Write("MeshMaterialList {{ // {} material list\n".
            format(self.SafeName))
        self.Exporter.File.Indent()

        self.Exporter.File.Write("{};\n".format(len(Materials)))
        self.Exporter.File.Write("{};\n".format(len(Mesh.polygons)))
        # Write a material index for each face
        for Index, Polygon in enumerate(Mesh.polygons):
            self.Exporter.File.Write("{}".format(Polygon.material_index))
            if Index == len(Mesh.polygons) - 1:
                self.Exporter.File.Write(";\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)

        for Material in Materials:
            WriteMaterial(self.Exporter, Material)

        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {} material list\n".format(
            self.SafeName))

    def __WriteMeshActiveImageMaterials(self, Mesh):
        def WriteMaterial(Exporter, MaterialKey):
            #Unpack key
            Material, Image = MaterialKey

            Exporter.File.Write("Material {} {{\n".format(
                Util.SafeName(Material.name)))
            Exporter.File.Indent()

            Diffuse = list(Vector(Material.diffuse_color) *
                Material.diffuse_intensity)
            Diffuse.append(Material.alpha)
            # Map Blender's range of 1 - 511 to 0 - 1000
            Specularity = 1000 * (Material.specular_hardness - 1.0) / 510.0
            Specular = list(Vector(Material.specular_color) *
                Material.specular_intensity)

            Exporter.File.Write("{:9f};{:9f};{:9f};{:9f};;\n".format(Diffuse[0],
                Diffuse[1], Diffuse[2], Diffuse[3]))
            Exporter.File.Write(" {:9f};\n".format(Specularity))
            Exporter.File.Write("{:9f};{:9f};{:9f};;\n".format(Specular[0],
                Specular[1], Specular[2]))
            Exporter.File.Write(" 0.000000; 0.000000; 0.000000;;\n")

            if Image is not None:
                Exporter.File.Write("TextureFilename {{\"{}\";}}\n".format(
                    bpy.path.basename(Image.filepath)))

            self.Exporter.File.Unindent()
            self.Exporter.File.Write("}\n")

        def GetMaterialKey(Material, UVTexture, Index):
            Image = None
            if UVTexture is not None and UVTexture.data[Index].image is not None:
                Image = UVTexture.data[Index].image if \
                    UVTexture.data[Index].image.source == 'FILE' else None

            return (Material, Image)

        Materials = Mesh.materials
        # Do not write materials if there are none
        if not Materials.keys():
            return

        self.Exporter.File.Write("MeshMaterialList {{ // {} material list\n".
            format(self.SafeName))
        self.Exporter.File.Indent()

        from array import array
        MaterialIndices = array("I", [0]) * len(Mesh.polygons) # Fast allocate
        MaterialIndexMap = {}

        for Index, Polygon in enumerate(Mesh.polygons):
            MaterialKey = GetMaterialKey(Materials[Polygon.material_index],
                Mesh.uv_textures.active, Index)

            if MaterialKey in MaterialIndexMap:
                MaterialIndices[Index] = MaterialIndexMap[MaterialKey]
            else:
                MaterialIndex = len(MaterialIndexMap)
                MaterialIndexMap[MaterialKey] = MaterialIndex
                MaterialIndices[Index] = MaterialIndex

        self.Exporter.File.Write("{};\n".format(len(MaterialIndexMap)))
        self.Exporter.File.Write("{};\n".format(len(Mesh.polygons)))

        for Index in range(len(Mesh.polygons)):
            self.Exporter.File.Write("{}".format(MaterialIndices[Index]))
            if Index == len(Mesh.polygons) - 1:
                self.Exporter.File.Write(";;\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)

        for Material in MaterialIndexMap.keys():
            WriteMaterial(self.Exporter, Material)

        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {} material list\n".format(
            self.SafeName))

    def __WriteMeshVertexColors(self, Mesh, MeshEnumerator=None):
        # If there are no vertex colors, don't write anything
        if len(Mesh.vertex_colors) == 0:
            return

        # Blender stores vertex color information per vertex per face, so we
        # need to pass in an _UnrolledFacesMeshEnumerator.  Otherwise,
        if MeshEnumerator is None:
            MeshEnumerator = _UnrolledFacesMeshEnumerator(Mesh)

        # Gather the colors of each vertex
        VertexColorLayer = Mesh.vertex_colors.active
        VertexColors = [VertexColorLayer.data[Index].color for Index in
            range(0,len(MeshEnumerator.vertices))]
        VertexColorCount = len(VertexColors)

        self.Exporter.File.Write("MeshVertexColors {{ // {} vertex colors\n" \
            .format(self.SafeName))
        self.Exporter.File.Indent()
        self.Exporter.File.Write("{};\n".format(VertexColorCount))

        # Write the vertex colors for each vertex index.
        for Index, Color in enumerate(VertexColors):
            self.Exporter.File.Write("{};{:9f};{:9f};{:9f};{:9f};;".format(
                Index, Color[0], Color[1], Color[2], 1.0))

            if Index == VertexColorCount - 1:
                self.Exporter.File.Write(";\n", Indent=False)
            else:
                self.Exporter.File.Write(",\n", Indent=False)

        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {} vertex colors\n".format(
            self.SafeName))

    def __WriteMeshSkinWeights(self, Mesh, MeshEnumerator=None):
        # This contains vertex indices and weights for the vertices that belong
        # to this bone's group.  Also calculates the bone skin matrix.
        class _BoneVertexGroup:
                def __init__(self, BlenderObject, ArmatureObject, BoneName):
                    self.BoneName = BoneName
                    self.SafeName = Util.SafeName(ArmatureObject.name) + "_" + \
                        Util.SafeName(BoneName)

                    self.Indices = []
                    self.Weights = []

                    # BoneMatrix transforms mesh vertices into the
                    # space of the bone.
                    # Here are the final transformations in order:
                    #  - Object Space to World Space
                    #  - World Space to Armature Space
                    #  - Armature Space to Bone Space
                    # This way, when BoneMatrix is transformed by the bone's
                    # Frame matrix, the vertices will be in their final world
                    # position.

                    self.BoneMatrix = ArmatureObject.data.bones[BoneName] \
                        .matrix_local.inverted()
                    self.BoneMatrix *= ArmatureObject.matrix_world.inverted()
                    self.BoneMatrix *= BlenderObject.matrix_world

                def AddVertex(self, Index, Weight):
                    self.Indices.append(Index)
                    self.Weights.append(Weight)

        # Skin weights work well with vertex reuse per face.  Use a
        # _OneToOneMeshEnumerator if possible.
        if MeshEnumerator is None:
            MeshEnumerator = MeshExportObject._OneToOneMeshEnumerator(Mesh)

        ArmatureModifierList = [Modifier
            for Modifier in self.BlenderObject.modifiers
            if Modifier.type == 'ARMATURE' and Modifier.show_viewport]

        if not ArmatureModifierList:
            return

        # Although multiple armature objects are gathered, support for
        # multiple armatures per mesh is not complete
        ArmatureObjects = [Modifier.object for Modifier in ArmatureModifierList]

        for ArmatureObject in ArmatureObjects:
            # Determine the names of the bone vertex groups
            PoseBoneNames = [Bone.name for Bone in ArmatureObject.pose.bones]
            VertexGroupNames = [Group.name for Group
                in self.BlenderObject.vertex_groups]
            UsedBoneNames = set(PoseBoneNames).intersection(VertexGroupNames)

            # Create a _BoneVertexGroup for each group name
            BoneVertexGroups = [_BoneVertexGroup(self.BlenderObject,
                ArmatureObject, BoneName) for BoneName in UsedBoneNames]

            # Maps Blender's internal group indexing to our _BoneVertexGroups
            GroupIndexToBoneVertexGroups = {Group.index : BoneVertexGroup
                for Group in self.BlenderObject.vertex_groups
                for BoneVertexGroup in BoneVertexGroups
                if Group.name == BoneVertexGroup.BoneName}

            MaximumInfluences = 0

            for Index, Vertex in enumerate(MeshEnumerator.vertices):
                VertexWeightTotal = 0.0
                VertexInfluences = 0

                # Sum up the weights of groups that correspond
                # to armature bones.
                for VertexGroup in Vertex.groups:
                    BoneVertexGroup = GroupIndexToBoneVertexGroups.get(
                        VertexGroup.group)
                    if BoneVertexGroup is not None:
                        VertexWeightTotal += VertexGroup.weight
                        VertexInfluences += 1

                if VertexInfluences > MaximumInfluences:
                    MaximumInfluences = VertexInfluences

                # Add the vertex to the bone vertex groups it belongs to,
                # normalizing each bone's weight.
                for VertexGroup in Vertex.groups:
                    BoneVertexGroup = GroupIndexToBoneVertexGroups.get(
                        VertexGroup.group)
                    if BoneVertexGroup is not None:
                        Weight = VertexGroup.weight / VertexWeightTotal
                        BoneVertexGroup.AddVertex(Index, Weight)

            self.Exporter.File.Write("XSkinMeshHeader {\n")
            self.Exporter.File.Indent()
            self.Exporter.File.Write("{};\n".format(MaximumInfluences))
            self.Exporter.File.Write("{};\n".format(3 * MaximumInfluences))
            self.Exporter.File.Write("{};\n".format(len(BoneVertexGroups)))
            self.Exporter.File.Unindent()
            self.Exporter.File.Write("}\n")

            for BoneVertexGroup in BoneVertexGroups:
                self.Exporter.File.Write("SkinWeights {\n")
                self.Exporter.File.Indent()
                self.Exporter.File.Write("\"{}\";\n".format(
                    BoneVertexGroup.SafeName))

                GroupVertexCount = len(BoneVertexGroup.Indices)
                self.Exporter.File.Write("{};\n".format(GroupVertexCount))

                # Write the indices of the vertices this bone affects.
                for Index, VertexIndex in enumerate(BoneVertexGroup.Indices):
                    self.Exporter.File.Write("{}".format(VertexIndex))

                    if Index == GroupVertexCount - 1:
                        self.Exporter.File.Write(";\n", Indent=False)
                    else:
                        self.Exporter.File.Write(",\n", Indent=False)

                # Write the weights of the affected vertices.
                for Index, VertexWeight in enumerate(BoneVertexGroup.Weights):
                    self.Exporter.File.Write("{:9f}".format(VertexWeight))

                    if Index == GroupVertexCount - 1:
                        self.Exporter.File.Write(";\n", Indent=False)
                    else:
                        self.Exporter.File.Write(",\n", Indent=False)

                # Write the bone's matrix.
                Util.WriteMatrix(self.Exporter.File, BoneVertexGroup.BoneMatrix)

                self.Exporter.File.Unindent()
                self.Exporter.File.Write("}} // End of {} skin weights\n" \
                    .format(BoneVertexGroup.SafeName))

# Armature object implementation of ExportObject
class ArmatureExportObject(ExportObject):
    def __init__(self, Config, Exporter, BlenderObject):
        ExportObject.__init__(self, Config, Exporter, BlenderObject)

    def __repr__(self):
        return "[ArmatureExportObject: {}]".format(self.name)

    # "Public" Interface

    def Write(self):
        self.Exporter.Log("Opening frame for {}".format(self))
        self._OpenFrame()

        if self.Config.ExportArmatureBones:
            Armature = self.BlenderObject.data
            RootBones = [Bone for Bone in Armature.bones if Bone.parent is None]
            self.Exporter.Log("Writing frames for armature bones...")
            self.__WriteBones(RootBones)
            self.Exporter.Log("Done")

        self.Exporter.Log("Writing children of {}".format(self))
        self._WriteChildren()

        self._CloseFrame()
        self.Exporter.Log("Closed frame of {}".format(self))

    # "Private" Methods

    def __WriteBones(self, Bones):
        # Simply export the frames for each bone.  Export in rest position or
        # posed position depending on options.
        for Bone in Bones:
            BoneMatrix = Matrix()

            if self.Config.ExportRestBone:
                if Bone.parent:
                    BoneMatrix = Bone.parent.matrix_local.inverted()
                BoneMatrix *= Bone.matrix_local
            else:
                PoseBone = self.BlenderObject.pose.bones[Bone.name]
                if Bone.parent:
                    BoneMatrix = PoseBone.parent.matrix.inverted()
                BoneMatrix *= PoseBone.matrix

            BoneSafeName = self.SafeName + "_" + \
                Util.SafeName(Bone.name)
            self.__OpenBoneFrame(BoneSafeName, BoneMatrix)

            self.__WriteBoneChildren(Bone)

            self.__CloseBoneFrame(BoneSafeName)


    def __OpenBoneFrame(self, BoneSafeName, BoneMatrix):
        self.Exporter.File.Write("Frame {} {{\n".format(BoneSafeName))
        self.Exporter.File.Indent()

        self.Exporter.File.Write("FrameTransformMatrix {\n")
        self.Exporter.File.Indent()
        Util.WriteMatrix(self.Exporter.File, BoneMatrix)
        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}\n")

    def __CloseBoneFrame(self, BoneSafeName):
        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}} // End of {}\n".format(BoneSafeName))

    def __WriteBoneChildren(self, Bone):
        self.__WriteBones(Util.SortByNameField(Bone.children))


# Container for animation data
class Animation:
    def __init__(self, SafeName):
        self.SafeName = SafeName

        self.RotationKeys = []
        self.ScaleKeys = []
        self.PositionKeys = []

    # "Public" Interface

    def GetKeyCount(self):
        return len(self.RotationKeys)


# Creates a list of Animation objects based on the animation needs of the
# ExportObject passed to it
class AnimationGenerator: # Base class, do not use
    def __init__(self, Config, SafeName, ExportObject):
        self.Config = Config
        self.SafeName = SafeName
        self.ExportObject = ExportObject

        self.Animations = []


# Creates one Animation object that contains the rotation, scale, and position
# of the ExportObject
class GenericAnimationGenerator(AnimationGenerator):
    def __init__(self, Config, SafeName, ExportObject):
        AnimationGenerator.__init__(self, Config, SafeName, ExportObject)

        self._GenerateKeys()

    # "Protected" Interface

    def _GenerateKeys(self):
        Scene = bpy.context.scene # Convenience alias
        BlenderCurrentFrame = Scene.frame_current

        CurrentAnimation = Animation(self.ExportObject.SafeName)
        BlenderObject = self.ExportObject.BlenderObject

        for Frame in range(Scene.frame_start, Scene.frame_end + 1):
            Scene.frame_set(Frame)

            Rotation = BlenderObject.rotation_euler.to_quaternion()
            Scale = BlenderObject.matrix_local.to_scale()
            Position = BlenderObject.matrix_local.to_translation()

            CurrentAnimation.RotationKeys.append(Rotation)
            CurrentAnimation.ScaleKeys.append(Scale)
            CurrentAnimation.PositionKeys.append(Position)

        self.Animations.append(CurrentAnimation)
        Scene.frame_set(BlenderCurrentFrame)


# Creates one Animation object for each of the ExportObjects it gets passed.
# Essentially a bunch of GenericAnimationGenerators lumped into one.
class GroupAnimationGenerator(AnimationGenerator):
    def __init__(self, Config, SafeName, ExportObjects):
        AnimationGenerator.__init__(self, Config, SafeName, None)
        self.ExportObjects = ExportObjects

        self._GenerateKeys()

    # "Protected" Interface

    def _GenerateKeys(self):
        for Object in self.ExportObjects:
            if Object.BlenderObject.type == 'ARMATURE':
                TemporaryGenerator = ArmatureAnimationGenerator(self.Config,
                    None, Object)
                self.Animations += TemporaryGenerator.Animations
            else:
                TemporaryGenerator = GenericAnimationGenerator(self.Config,
                    None, Object)
                self.Animations += TemporaryGenerator.Animations


# Creates an Animation object for the ArmatureExportObject it gets passed and
# an Animation object for each bone in the armature (if options allow)
class ArmatureAnimationGenerator(GenericAnimationGenerator):
    def __init__(self, Config, SafeName, ArmatureExportObject):
        GenericAnimationGenerator.__init__(self, Config, SafeName,
            ArmatureExportObject)

        if self.Config.ExportArmatureBones:
            self._GenerateBoneKeys()

    # "Protected" Interface

    def _GenerateBoneKeys(self):
        from itertools import zip_longest as zip

        Scene = bpy.context.scene # Convenience alias
        BlenderCurrentFrame = Scene.frame_current

        ArmatureObject = self.ExportObject.BlenderObject
        ArmatureSafeName = self.ExportObject.SafeName

        # Create Animation objects for each bone
        BoneAnimations = [Animation(ArmatureSafeName + "_" + \
            Util.SafeName(Bone.name)) for Bone in ArmatureObject.pose.bones]

        for Frame in range(Scene.frame_start, Scene.frame_end + 1):
            Scene.frame_set(Frame)

            for Bone, BoneAnimation in \
                zip(ArmatureObject.pose.bones, BoneAnimations):

                PoseMatrix = Matrix()
                if Bone.parent:
                    PoseMatrix = Bone.parent.matrix.inverted()
                PoseMatrix *= Bone.matrix

                Rotation = PoseMatrix.to_quaternion().normalized()
                OldRotation = BoneAnimation.RotationKeys[-1] if \
                    len(BoneAnimation.RotationKeys) else Rotation

                Scale = PoseMatrix.to_scale()

                Position = PoseMatrix.to_translation()

                BoneAnimation.RotationKeys.append(Util.CompatibleQuaternion(
                    Rotation, OldRotation))
                BoneAnimation.ScaleKeys.append(Scale)
                BoneAnimation.PositionKeys.append(Position)

        self.Animations += BoneAnimations
        Scene.frame_set(BlenderCurrentFrame)


# Container for all AnimationGenerators that belong in a single AnimationSet
class AnimationSet:
    def __init__(self, SafeName, AnimationGenerators):
        self.SafeName = SafeName
        self.AnimationGenerators = AnimationGenerators


# Writes all animation data to file.  Implementations will control the
# separation of AnimationGenerators into distinct AnimationSets.
class AnimationWriter:
    def __init__(self, Config, Exporter, AnimationGenerators):
        self.Config = Config
        self.Exporter = Exporter
        self.AnimationGenerators = AnimationGenerators

        self.AnimationSets = []

    # "Public" Interface

    # Writes all AnimationSets.  Implementations probably won't have to override
    # this method.
    def WriteAnimationSets(self):
        if self.Config.IncludeFrameRate:
            self.Exporter.Log("Writing frame rate...")
            self.__WriteFrameRate()
            self.Exporter.Log("Done")

        for Set in self.AnimationSets:
            self.Exporter.Log("Writing animation set {}".format(Set.SafeName))
            self.Exporter.File.Write("AnimationSet {} {{\n".format(
                Set.SafeName))
            self.Exporter.File.Indent()

            # Write each animation of each generator
            for Generator in Set.AnimationGenerators:
                for CurrentAnimation in Generator.Animations:
                    self.Exporter.Log("Writing animation of {}".format(
                        CurrentAnimation.SafeName))
                    self.Exporter.File.Write("Animation {\n")
                    self.Exporter.File.Indent()
                    self.Exporter.File.Write("{{{}}}\n".format(
                        CurrentAnimation.SafeName))

                    KeyCount = CurrentAnimation.GetKeyCount()

                    # Write rotation keys
                    self.Exporter.File.Write("AnimationKey { // Rotation\n");
                    self.Exporter.File.Indent()
                    self.Exporter.File.Write("0;\n")
                    self.Exporter.File.Write("{};\n".format(KeyCount))

                    for Frame, Key in enumerate(CurrentAnimation.RotationKeys):
                        self.Exporter.File.Write(
                            "{};4;{:9f},{:9f},{:9f},{:9f};;".format(
                            Frame, -Key[0], Key[1], Key[2], Key[3]))

                        if Frame == KeyCount - 1:
                            self.Exporter.File.Write(";\n", Indent=False)
                        else:
                            self.Exporter.File.Write(",\n", Indent=False)

                    self.Exporter.File.Unindent()
                    self.Exporter.File.Write("}\n")

                    # Write scale keys
                    self.Exporter.File.Write("AnimationKey { // Scale\n");
                    self.Exporter.File.Indent()
                    self.Exporter.File.Write("1;\n")
                    self.Exporter.File.Write("{};\n".format(KeyCount))

                    for Frame, Key in enumerate(CurrentAnimation.ScaleKeys):
                        self.Exporter.File.Write(
                            "{};3;{:9f},{:9f},{:9f};;".format(
                            Frame, Key[0], Key[1], Key[2]))

                        if Frame == KeyCount - 1:
                            self.Exporter.File.Write(";\n", Indent=False)
                        else:
                            self.Exporter.File.Write(",\n", Indent=False)

                    self.Exporter.File.Unindent()
                    self.Exporter.File.Write("}\n")

                    # Write position keys
                    self.Exporter.File.Write("AnimationKey { // Position\n");
                    self.Exporter.File.Indent()
                    self.Exporter.File.Write("2;\n")
                    self.Exporter.File.Write("{};\n".format(KeyCount))

                    for Frame, Key in enumerate(CurrentAnimation.PositionKeys):
                        self.Exporter.File.Write(
                            "{};3;{:9f},{:9f},{:9f};;".format(
                            Frame, Key[0], Key[1], Key[2]))

                        if Frame == KeyCount - 1:
                            self.Exporter.File.Write(";\n", Indent=False)
                        else:
                            self.Exporter.File.Write(",\n", Indent=False)

                    self.Exporter.File.Unindent()
                    self.Exporter.File.Write("}\n")

                    self.Exporter.File.Unindent()
                    self.Exporter.File.Write("}\n")
                    self.Exporter.Log("Done")

            self.Exporter.File.Unindent()
            self.Exporter.File.Write("}} // End of AnimationSet {}\n".format(
                Set.SafeName))
            self.Exporter.Log("Done writing animation set {}".format(
                Set.SafeName))

    # "Private" Methods

    def __WriteFrameRate(self):
        Scene = bpy.context.scene # Convenience alias

        # Calculate the integer frame rate
        FrameRate = int(Scene.render.fps / Scene.render.fps_base)

        self.Exporter.File.Write("AnimTicksPerSecond {\n");
        self.Exporter.File.Indent()
        self.Exporter.File.Write("{};\n".format(FrameRate))
        self.Exporter.File.Unindent()
        self.Exporter.File.Write("}\n")

# Implementation of AnimationWriter that sticks all generators into a
# single AnimationSet
class JoinedSetAnimationWriter(AnimationWriter):
    def __init__(self, Config, Exporter, AnimationGenerators):
        AnimationWriter.__init__(self, Config, Exporter, AnimationGenerators)

        self.AnimationSets = [AnimationSet("Global", self.AnimationGenerators)]

# Implementation of AnimationWriter that puts each generator into its
# own AnimationSet
class SplitSetAnimationWriter(AnimationWriter):
    def __init__(self, Config, Exporter, AnimationGenerators):
        AnimationWriter.__init__(self, Config, Exporter, AnimationGenerators)

        self.AnimationSets = [AnimationSet(Generator.SafeName, [Generator])
            for Generator in AnimationGenerators]


# Interface to the file.  Supports automatic whitespace indenting.
class File:
    def __init__(self, FilePath):
        self.FilePath = FilePath
        self.File = None
        self.__Whitespace = 0

    def Open(self):
        if not self.File:
            self.File = open(self.FilePath, 'w')

    def Close(self):
        self.File.close()
        self.File = None

    def Write(self, String, Indent=True):
        if Indent:
            # Escape any formatting braces
            String = String.replace("{", "{{")
            String = String.replace("}", "}}")
            self.File.write(("{}" + String).format("  " * self.__Whitespace))
        else:
            self.File.write(String)

    def Indent(self, Levels=1):
        self.__Whitespace += Levels

    def Unindent(self, Levels=1):
        self.__Whitespace -= Levels
        if self.__Whitespace < 0:
            self.__Whitespace = 0


# Static utilities
class Util:
    @staticmethod
    def SafeName(Name):
        # Replaces each character in OldSet with NewChar
        def ReplaceSet(String, OldSet, NewChar):
            for OldChar in OldSet:
                String = String.replace(OldChar, NewChar)
            return String

        import string

        NewName = ReplaceSet(Name, string.punctuation + " ", "_")
        if NewName[0].isdigit() or NewName in ["ARRAY", "DWORD", "UCHAR",
            "FLOAT", "ULONGLONG", "BINARY_RESOURCE", "SDWORD", "UNICODE",
            "CHAR", "STRING", "WORD", "CSTRING", "SWORD", "DOUBLE", "TEMPLATE"]:
            NewName = "_" + NewName
        return NewName

    @staticmethod
    def WriteMatrix(File, Matrix):
        File.Write("{:9f},{:9f},{:9f},{:9f},\n".format(Matrix[0][0],
            Matrix[1][0], Matrix[2][0], Matrix[3][0]))
        File.Write("{:9f},{:9f},{:9f},{:9f},\n".format(Matrix[0][1],
            Matrix[1][1], Matrix[2][1], Matrix[3][1]))
        File.Write("{:9f},{:9f},{:9f},{:9f},\n".format(Matrix[0][2],
            Matrix[1][2], Matrix[2][2], Matrix[3][2]))
        File.Write("{:9f},{:9f},{:9f},{:9f};;\n".format(Matrix[0][3],
            Matrix[1][3], Matrix[2][3], Matrix[3][3]))

    # Used on lists of blender objects and lists of ExportObjects, both of
    # which have a name field
    @staticmethod
    def SortByNameField(List):
        def SortKey(x):
            return x.name

        return sorted(List, key=SortKey)

    # Make A compatible with B
    @staticmethod
    def CompatibleQuaternion(A, B):
        if (A.normalized().conjugated() * B.normalized()).angle > pi:
            return -A
        else:
            return A
