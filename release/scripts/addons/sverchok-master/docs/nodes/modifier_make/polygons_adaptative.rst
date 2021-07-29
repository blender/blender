Adaptative Polygons
===================

Functionality
-------------

Share one object's **verts+faces** to another object's **verts+faces**. Donor spreads itself onto recipient polygons, every polygon recieves a copy of the donor object and deforms according to the recipients face **normals**. 

*Limitations:* This node was created primarily with Quads (quadrilateral polygons) in mind, and will output unusual meshes if you feed it Tris or Ngons in the recipient Mesh. Original code taken with permission from https://sketchesofcode.wordpress.com/2013/11/11/ by Alessandro Zomparelli (sketchesofcode).

Inputs
------

- **VersR** and **PolsR** is Recipient object's data. 
- **VersD** and **PolsD** is donor's object data. 
- **Z_Coef** is coefficient of height, can be vectorized.

Parameters
----------

table

+------------------+---------------+-------------------------------------------------------------------+
| Param            | Type          | Description                                                       |  
+==================+===============+===================================================================+
| **Donor width**  | Float         | Donor's spread width is part from recipient's polygons width      | 
+------------------+---------------+-------------------------------------------------------------------+

Outputs
-------

**Vertices** and **Polygons** are data for created object.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4222738/25e20e00-3916-11e4-9aca-5127f2edaa95.jpg
  :alt: Adaptive_Polygons.jpg

