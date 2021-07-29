Centers Polygons
================

Functionality
-------------

Analizing geometry and finding centers of polygons, normals (from global zero), normals from local centers of polygons and matrices that find polygons rotation. Not works with edges.

Inputs
------

**Vertices** and **Polygons** from object that we analizing.

Outputs
-------

**Normals** is normals from global zero coordinates, vector. **Norm_abs** is normals shifted to centers of polygons. **Origins** centers of polygons. **Centers** matrices that has rotation and location of polygons.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4222939/b86a1d3e-3917-11e4-8e03-c24980672404.jpg
  :alt: Centers_of_polygons_normals.jpg

.. image:: https://cloud.githubusercontent.com/assets/5783432/4222936/b863cb46-3917-11e4-9cfe-0d863c4850b6.jpg
  :alt: Centers_of_polygons_normalsabs.jpg

.. image:: https://cloud.githubusercontent.com/assets/5783432/4222937/b864c8fc-3917-11e4-9368-b5260703e4c5.jpg
  :alt: Centers_of_polygons_locations.jpg

.. image:: https://cloud.githubusercontent.com/assets/5783432/4222949/c5874906-3917-11e4-9c9c-94c016560f98.jpg
  :alt: Centers_of_polygons_matrices.jpg

Problems
--------

The code of matrix rotation based on Euler rotation, so when you rotate to plane X-oriented, it makes wrong. We need spherical coordinates and quaternion rotation here, needed help or something
