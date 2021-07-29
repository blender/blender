Contour 2D
==========

Functionality
-------------

.. image:: https://user-images.githubusercontent.com/10011941/34644861-37e4c430-f33f-11e7-92e0-d89080effc4b.png

This node creates one or many contours at specified distance. 

- It is feeded by sets of vertices and edges.
- Every set of vertices needs to share the Z coordinate in order to create a valid contour.


Inputs
------

This node has the following inputs, all of them can accept one or many different values:

- **Distance** - distance to vertex.
- **Nº Vertices** - Number of vectices per vertex
- **Verts_in** - origin vertices.
- **Edgs_in** - edges (pairs of integers).


Parameters
----------


+------------------+---------------+-------------+-------------------------------------------------------------+
| Parameter        | Type          | Default     | Description                                                 |
+==================+===============+=============+=============================================================+
|**Mode**          | Menu          | Constant    |**Constant**: Constant distances on each perimeter.          |
|                  |               |             |**Weighted**: Different distances per vertex.                |
+------------------+---------------+-------------+-------------------------------------------------------------+
|**Remove Doubles**| Float         | 1.0         | Remove doubled vertices under this distance.                |
+------------------+---------------+-------------+-------------------------------------------------------------+
|**Distance**      | Float         | 1.0         | Distance to vertex.                                         |
+------------------+---------------+-------------+-------------------------------------------------------------+
|**Nº Vertices**   | Float         | 1.0         | Number of vectices per vertex.                              |
+------------------+---------------+-------------+-------------------------------------------------------------+
| **Verts_in**     | Vector        |(0.0,0.0,0.0)| Origin vectors.                                             |
+------------------+---------------+-------------+-------------------------------------------------------------+
| **Edges_in**     | Int tuples    | []          | Connexion between vectices                                  |
+------------------+---------------+-------------+-------------------------------------------------------------+
|In the N-Panel                                                                                                |
+------------------+---------------+-------------+-------------------------------------------------------------+
|**Mask Tolerance**| Float         | 1.0e-5      | Tolerance on masking (for low Nº Vertices or small values)  |
+------------------+---------------+-------------+-------------------------------------------------------------+
|**Inters. Mode**  | Menu          | Circular    |**Circular**: Intersecction based on circles (Slower).       |
|                  |               |             |**Poligonal**: Intersecction based on poligons (Faster).     |
+------------------+---------------+-------------+-------------------------------------------------------------+
|**List Match**    | Menu          | Long Cycle  |**Long Repeat**: After shortest list repeat last value.      |
|                  |               |             |**Long Cycle**: After shortest list got to first last value. |
+------------------+---------------+-------------+-------------------------------------------------------------+


Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**

Examples of usage
-----------------

- When you input different distances:
In constant mode independent contours will be created (one per distance)
In Weighted mode will apply each distance to a vertex creating independent contours when there are more distances than vertices

.. image:: https://user-images.githubusercontent.com/10011941/34644863-41eabfde-f33f-11e7-8ed6-6e8fa7a1e6df.png   
 
- When you input different objects independent contours will be created:

.. image:: https://user-images.githubusercontent.com/10011941/34644864-46463d24-f33f-11e7-80c1-bb0718d9966b.png
  

- With the intersection mode on "Circular" the intersection points will be placed as if we were using perfect circles. This will change the edges angles, but the distance between the intersection point and the original points will be maintained. On "Poligonal" the edges angles are preserved but the distance to original vertex will depend on the number of vertices.

.. image:: https://user-images.githubusercontent.com/10011941/35116834-027e2f8c-fc8d-11e7-9cff-35465e3e5e17.png
 
- Integrated list match function can lead to different results:

.. image:: https://user-images.githubusercontent.com/10011941/34644870-5935b1ee-f33f-11e7-99ba-0c536bf67f91.png

- Different ranges can be used to create a complex contour. 

.. image:: https://user-images.githubusercontent.com/10011941/35116835-029ea8de-fc8d-11e7-9df0-f044677c059a.png

- When using text meshes it can get very slow but also interesting

.. image:: https://user-images.githubusercontent.com/10011941/35116836-02b9cc36-fc8d-11e7-9526-259c18c8556f.png


Notes
-----

- This implementation can get very slow when working with hundreds of inputs and different distances, handle it with patience.

- If the node does not create a closed contour try increasing the vertices number or rising the mask tolerance slowly 

- This is the pull request where this node was added https://github.com/nortikin/sverchok/pull/2001

