Extrude Separate Faces
======================

*destination after Beta: Modifier Change*

Functionality
-------------

This node applies Extrude operator to each of input faces separately. After that, resulting faces can be scaled up or down by specified factor.
It is possible to provide specific extrude and scaling factors for each face.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Polygons**
- **Mask**. List of boolean or integer flags. Zero means do not process face with corresponding index. If this input is not connected, then by default all faces will be processed.
- **Height**. Extrude factor.
- **Scale**. Scaling factor.

Parameters
----------

This node has the following parameters:

+----------------+---------------+-------------+------------------------------------------------------+
| Parameter      | Type          | Default     | Description                                          |  
+================+===============+=============+======================================================+
| **Height**     | Float         | 0.0         | Extrude factor as a portion of face normal length.   |
|                |               |             | Default value of zero means do not extrude.          |
|                |               |             | Negative value means extrude to the opposite         |
|                |               |             | direction. This parameter can be also provided via   |
|                |               |             | corresponding input.                                 |
+----------------+---------------+-------------+------------------------------------------------------+
| **Scale**      | Float         | 1.0         | Scale factor. Default value of 1 means do not scale. |
+----------------+---------------+-------------+------------------------------------------------------+

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Polygons**. All faces of resulting mesh.
- **ExtrudedPolys**. Only extruded faces of resulting mesh.
- **OtherPolys**. All other faces of resulting mesh.

Example of usage
----------------

Extruded faces of sphere, extruding factor depending on Z coordinate of face:

.. image:: https://cloud.githubusercontent.com/assets/284644/5888213/f8c4c4b8-a417-11e4-9a6d-4ee891744587.png

Sort of cage:

.. image:: https://cloud.githubusercontent.com/assets/284644/5888237/978cdc66-a418-11e4-89d4-a17d325426c0.png

