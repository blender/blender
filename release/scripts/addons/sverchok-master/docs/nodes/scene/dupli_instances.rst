Dupli Instances
===============

Functionality
-------------

This node exposes the functionality of the Duplication types ``VERTS`` and ``FACES`` to the Sverchok node tree. The Node works in two ways. One mode accepts just Locations, the other mode accepts just Matrices.

+-----------------+--------------------------------------------------------------------------+
| Features        | Description                                                              |
+=================+==========================================================================+
| Locations       | the node generates a proper blender mesh internally, based on vertices.  |
|                 | The duplication is set to VERTS.                                         | 
+-----------------+--------------------------------------------------------------------------+
| Matrices        | the node generates a vertex+face mesh using the transforms contained in  |
|                 | individual matrices. First it makes a unit 1 triangle, then multiplies   |
|                 | the vertex coordinates with a matrix. This is done for each of the       |
|                 | passed matrices. Passing 4 matrices, makes 4 triangles : a total of 12   |
|                 | verts and 4 faces. The duplication is set to FACES.                      |
+-----------------+--------------------------------------------------------------------------+
| Child Object    | You pick the child Object from the UI.                                   |
+-----------------+--------------------------------------------------------------------------+
| Parent Object   | (not exposed to the UI) , this is generated internally from the          |
|                 | Locations or Matrices socket data                                        |
+-----------------+--------------------------------------------------------------------------+


The name of the internal parent object in this example is 'booom' , but this can be changed and should probably be node specific.

.. image:: https://cloud.githubusercontent.com/assets/619340/11168229/2178fba8-8b86-11e5-9f71-ef05e7e14156.png


Inputs
------

+-----------------+--------------------------------------------------------------------------+
| Input           | Description                                                              |
+=================+==========================================================================+
| Locations       | Vertices, coordinates, vectors, 3tuples, 3lists                          | 
+-----------------+--------------------------------------------------------------------------+
| Matrices        | full on 4*4 transform matrices (but scale is converted to uniform)       |
+-----------------+--------------------------------------------------------------------------+

Parameters
----------

The only parameter is the Object selection, it needs to duplicate something


Limitations
-----------

It's worth mentioning that because the faces duplication relies on the area of the triangle to determin the scale, that the scale is a scalar, and therefor uniform (x,y,z are scaled equally).



Examples
--------

More info: https://github.com/nortikin/sverchok/issues/740  


.. image:: https://cloud.githubusercontent.com/assets/619340/11168561/25c27244-8b94-11e5-832f-a763f4362670.png

.. image:: https://cloud.githubusercontent.com/assets/619340/11168569/869f0b7c-8b94-11e5-831b-48acb70ee00d.png
