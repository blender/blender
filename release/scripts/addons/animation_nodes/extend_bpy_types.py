import bpy
from bpy.props import *
from . operators.callbacks import executeCallback
from . data_structures import (Vector3DList, EdgeIndicesList, PolygonIndicesList,
                               FloatList, DoubleList, UShortList, LongList)

def register():
    bpy.types.Context.getActiveAnimationNodeTree = getActiveAnimationNodeTree
    bpy.types.Operator.an_executeCallback = _executeCallback
    bpy.types.Mesh.an = PointerProperty(type = MeshProperties)
    bpy.types.Object.an = PointerProperty(type = ObjectProperties)
    bpy.types.ID.an_data = PointerProperty(type = IDProperties)

def unregister():
    del bpy.types.Context.getActiveAnimationNodeTree
    del bpy.types.Operator.an_executeCallback
    del bpy.types.Mesh.an
    del bpy.types.Object.an
    del bpy.types.ID.an_data

def getActiveAnimationNodeTree(context):
    if context.area.type == "NODE_EDITOR":
        tree = context.space_data.node_tree
        if getattr(tree, "bl_idname", "") == "an_AnimationNodeTree":
            return tree

def _executeCallback(operator, callback, *args, **kwargs):
    executeCallback(callback, *args, **kwargs)

class MeshProperties(bpy.types.PropertyGroup):
    bl_idname = "an_MeshProperties"

    def getVertices(self):
        vertices = Vector3DList(length = len(self.mesh.vertices))
        self.mesh.vertices.foreach_get("co", vertices.asMemoryView())
        return vertices

    def getEdgeIndices(self):
        edges = EdgeIndicesList(length = len(self.mesh.edges))
        self.mesh.edges.foreach_get("vertices", edges.asMemoryView())
        return edges

    def getPolygonIndices(self):
        polygons = PolygonIndicesList(
                        indicesAmount = len(self.mesh.loops),
                        polygonAmount = len(self.mesh.polygons))
        self.mesh.polygons.foreach_get("vertices", polygons.indices.asMemoryView())
        self.mesh.polygons.foreach_get("loop_total", polygons.polyLengths.asMemoryView())
        self.mesh.polygons.foreach_get("loop_start", polygons.polyStarts.asMemoryView())
        return polygons

    def getVertexNormals(self):
        normals = Vector3DList(length = len(self.mesh.vertices))
        self.mesh.vertices.foreach_get("normal", normals.asMemoryView())
        return normals

    def getPolygonNormals(self):
        normals = Vector3DList(length = len(self.mesh.polygons))
        self.mesh.polygons.foreach_get("normal", normals.asMemoryView())
        return normals

    def getPolygonCenters(self):
        centers = Vector3DList(length = len(self.mesh.polygons))
        self.mesh.polygons.foreach_get("center", centers.asMemoryView())
        return centers

    def getPolygonAreas(self):
        areas = FloatList(length = len(self.mesh.polygons))
        self.mesh.polygons.foreach_get("area", areas.asMemoryView())
        return DoubleList.fromValues(areas)

    def getPolygonMaterialIndices(self):
        indices = UShortList(length = len(self.mesh.polygons))
        self.mesh.polygons.foreach_get("material_index", indices.asMemoryView())
        return LongList.fromValues(indices)

    @property
    def mesh(self):
        return self.id_data

class ObjectProperties(bpy.types.PropertyGroup):
    bl_idname = "an_ObjectProperties"

    def getMesh(self, scene, applyModifiers = False):
        if scene is None: return None
        object = self.id_data

        from . events import isRendering
        settings = "RENDER" if isRendering() else "PREVIEW"

        if not applyModifiers and object.type == "MESH":
            return object.data
        else:
            try: return object.to_mesh(scene, applyModifiers, settings)
            except: return None

class IDProperties(bpy.types.PropertyGroup):
    bl_idname = "an_IDProperties"

    removeOnZeroUsers = BoolProperty(default = False,
        description = "Data block should be removed when it has no users")
