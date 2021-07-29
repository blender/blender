Viewer BMesh
============

Functionality
-------------

*aliases: BMeshViewer, BMView*

Similar to ViewerDraw but instead of using OpenGL calls to display geometry this Node *writes* or *updates* Blender Meshes on every geometry update. The bonus is that this geometry is renderable without an extra bake step. We can use Blender's Modifier stack to affect the mesh. The only exception to the modifiers is the Skin Modifier but we aren't entirely sure why, maybe because BMview invalidates the BMesh between updates.

Inputs
------

- Verts
- Edges
- Faces
- Matrix

Parameters & Features
---------------------

Features we rarely need or want to interact with are placed in the N-Panel / Properties Panel. 

+----------+-------------------+---------------------------------------------------------------------------------------+
| Location | Param             | Description                                                                           |
+==========+===================+=======================================================================================+ 
| Node UI  | Update            | Processing only happens if *update* is ticked                                         | 
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Group             | On by default, auto groups all meshes produced by incoming geometry                   | 
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Hide View         | Hides current meshes from view                                                        |
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Hide Select       | Disables the ability to select these meshes                                           | 
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Hide Render       | Disables the renderability of these meshes                                            |
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Base Name         | Base name for Objects and Meshes made by this node                                    |
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Select / Deselect | Select every object in 3dview that was created by this Node using Base Name           | 
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Material Select   | Assign materials to Objects made by this node                                         |
+----------+-------------------+---------------------------------------------------------------------------------------+
| N Panel  | Random Name       | In the case of multiple BMview nodes, this button makes it easier to generate a new   |
|          |                   | random name to prevent interference with existing Meshes. It will never produce and   |
|          |                   | use the name of an existing Object, it will always append names with indices.         |
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Fixed Vert count  | If you know the only change to the mesh is in vertex locations, then this toggle      |
|          |                   | will use the foreach construct to overwrite the locations only,                       | 
|          |                   | leaving existing edges and faces unchanged. Use it if you can.                        | 
+----------+-------------------+---------------------------------------------------------------------------------------+
|          | Smooth shade      | Automatically sets *shade* type to smooth when ticked.                                |
+----------+-------------------+---------------------------------------------------------------------------------------+

Outputs
-------

Outputs directly to Blender ``bpy.data.meshes`` and ``bpy.data.objects``


Examples
--------
