Bounding Box
============

Functionality
-------------

Generates a special ordered *bounding box* from incoming Vertices. 

Inputs
------

**Vertices**, or a nested list of vertices that represent separate objects.

Outputs
-------

+----------+-----------+----------------------------------------------------------------------------+
| Output   | Type      | Description                                                                |
+==========+===========+============================================================================+
| Vertices | Vectors   | One or more sets of Bounding Box vertices.                                 |
+----------+-----------+----------------------------------------------------------------------------+
| Edges    | Key Lists | One or more sets of Edges corresponding to the Vertices of the same index. |
+----------+-----------+----------------------------------------------------------------------------+
| Mean     | Vectors   | Arithmetic averages of the incoming sets of vertices                       |
+----------+-----------+----------------------------------------------------------------------------+
| Center   | Matrix    | Represents the *Center* of the bounding box; the average of its vertices   |
+----------+-----------+----------------------------------------------------------------------------+



Examples
--------

*Mean: Average of incoming set of Vertices*

.. image:: https://cloud.githubusercontent.com/assets/619340/4186539/def83614-3761-11e4-9cb4-4f7d8a8608bb.PNG
  :alt: BBox_Mean1.PNG

*Center: Average of the Bounding Box*

.. image:: https://cloud.githubusercontent.com/assets/619340/4186538/def29d62-3761-11e4-8069-b9544e2ad62a.PNG
  :alt: BBox_Center3.PNG

Notes
-----

GitHub issue tracker `discussion about this node <https://github.com/nortikin/sverchok/issues/161>`_
