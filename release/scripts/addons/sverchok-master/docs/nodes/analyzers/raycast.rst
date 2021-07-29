Raycast
=======

Functionality
-------------

Functionality is almost completely analogous to the two built-in blender operators 
``bpy.context.scene.ray_cast`` and ``object.ray_cast``. 
Ray is casted from "start" vector to "end" vector and can hit polygons of mesh objects.

see docs: 
`bpy.types.Object.ray_cast <http://www.blender.org/documentation/blender_python_api_2_71_0/bpy.types.Object.html#bpy.types.Object.ray_cast>`_ and 
`bpy.types.Scene.ray_cast <http://www.blender.org/documentation/blender_python_api_2_71_0/bpy.types.Scene.html#bpy.types.Scene.ray_cast>`_


Input sockets
-------------

**Start** - "start" vectors

**End** - "end" vectors

Parameters
----------

+-----------------+--------------------------------------------------------------------------------------------------+
| parameter       | description                                                                                      | 
+=================+==================================================================================================+
| object name     | Name of object to analize. (For **object_space** mode only)                                      |
+-----------------+--------------------------------------------------------------------------------------------------+
| raycast modes   | In **object_space** mode: node works like ``bpy.types.Object.ray_cast``                          |
|                 | (origin of object- center of coordinate for Start & End).                                        | 
|                 |                                                                                                  |
|                 | In **world_space** mode: node works like ``bpy.types.Scene.ray_cast``.                           |
+-----------------+--------------------------------------------------------------------------------------------------+


Output sockets
--------------

+------------------------+----------------------------------------------------------------------------------------+
| socket name            | description                                                                            |
+========================+========================================================================================+
| Hitp                   | Hit location for every raycast                                                         |
+------------------------+----------------------------------------------------------------------------------------+
| Hitnorm                | Normal of hit polygon (in "object_space" mode-local coordinates,                       |
|                        | in "world_space"- global                                                               |
+------------------------+----------------------------------------------------------------------------------------+
| Index/succes           | For **object_space** mode: index of hit polygon.                                       |
|                        | For **world_space** mode: ``True`` if ray hit mesh object, otherwise ``False``.        |
+------------------------+----------------------------------------------------------------------------------------+
| data object            | ``bpy.data.objects[hit object]`` or ``None`` type if ray doesn't hit a mesh object.    |
|                        | (only in "world_space" mode)                                                           |
+------------------------+----------------------------------------------------------------------------------------+
| hit object matrix      | Matrix of hit/struck object. (only in "world_space" mode)                              |
+------------------------+----------------------------------------------------------------------------------------+


Usage
-----

.. image:: https://cloud.githubusercontent.com/assets/7894950/4437227/4ac2cc4a-4790-11e4-8359-040da4398213.png
.. image:: https://cloud.githubusercontent.com/assets/7894950/4536920/7e47f270-4dd0-11e4-97fd-7d34d56229a0.png
