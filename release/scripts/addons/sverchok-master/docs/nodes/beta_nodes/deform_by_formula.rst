Deform by formula
========

Functionality
-------------

available variables: **x**, **y**, **z** for access to initial (xyz) coordinates.
And **i** for access to index of current vertex to be evaluated. It is also possible
to get index of current object list evaluated as **I** variable.
So **i** for index of vertex, and **I** for index of object.
Internally imported everything from Python **math** module.
Blender Py API also accessible (like **bpy.context.scene.frame_current**)

Inputs
------

- **Verts**

Outputs
-------

**Verts**.
resulted vertices to X,Y,Z elements of which was applied expression.

Example of usage
----------------
.. image:: https://user-images.githubusercontent.com/22656834/34645578-6feb7138-f373-11e7-87e7-54d4307c9b0a.png
