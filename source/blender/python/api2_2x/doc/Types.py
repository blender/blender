# The Blender.Types submodule

"""
The Blender.Types submodule
===========================

This module is a dictionary of Blender Python types, for type checking.

Example::
  import Blender
  from Blender import Types, Object, NMesh, Camera, Lamp
  #
  objs = Object.Get() # a list of all objects in the current scene
  for o in objs:
    print
    print o, type(o)
    data = o.getData()
    print type(data)
    if type(data) == Types.NMeshType:
      if len(data.verts):
        print "its vertices are obviously of type:", type(data.verts[0])
      print "and its faces:", Types.NMFaceType
    elif type(data) == Types.CameraType:
      print "It's a Camera."
    elif type(data) == Types.LampType:
      print "Let there be light!"

@var ObjectType: Blender Object. The base object, linked to its specific data
     at its .data member variable.
@var GroupType: Blender Group. A Group that references a list of objects that are a part of this group.
@var NMeshType: Blender NMesh. The mesh structure.
@var NMFaceType: Blender NMFace. A mesh face, with one (a point), two (an edge),
     three (a triangular face) or four (a quad face) vertices.
@var NMVertType: Blender NMVert. A mesh vertex.
@var NMColType: Blender NMCol. A mesh rgba color.
@var MeshType: Blender Mesh. The mesh structure.
@var MFaceType: Blender MFace. A mesh face, with 
     three (a triangular face) or four (a quad face) vertices.
@var MEdgeType: Blender MEdge. A mesh edge, with two vertices
@var MVertType: Blender MVert. A mesh vertex which wraps a Blender mesh vertex
     (typically an object returned from the mesh.verts sequence).
@var PVertType: Blender MVert. A mesh vertex which does not wrap a Blender
     mesh vertex (returned from L{Blender.Mesh.MVert()<Mesh.MVert.__init__>}).
@var MColType: Blender MCol. A mesh rgba color.
@var ArmatureType: Blender Armature. The "skeleton", for animating and deforming
objects.
@var BoneType: Blender Bone. Bones are, obviously, the "pieces" of an Armature.
@var CurveType: Blender Curve.
@var IpoType: Blender Ipo.
@var MetaballType: Blender Metaball.
@var CameraType: Blender Camera.
@var ImageType: Blender Image.
@var LampType: Blender Lamp.
@var TextType: Blender Text.
@var Text3dType: Blender Text3d.
@var MaterialType: Blender Material.
@var SceneType: A Blender Scene. Container of all other objects.
@var ButtonType: Blender Button. One of the Draw widgets.
@var vectorType: Blender vector. Used in NMesh, Mesh and elsewhere.
@var matrix_Type: Blender matrix.
@var quaternionType: Blender quaternion.  Used in armatures.
@var eulerType: Blender euler.
@var bufferType: Blender buffer. A contiguous piece of storage, used in BGL.
@var constantType: Blender constant. A constant dictionary.
@var rgbTupleType: Blender rgbTuple. A (red, green, blue) triplet.
@var TextureType: Blender Texture.
@var MTexType: Blender MTex.  Links materials to a texture.
@var IDGroupType: Blender IDProperty Group type.
@var IDArrayType: Blender IDProperty Array type.
"""
