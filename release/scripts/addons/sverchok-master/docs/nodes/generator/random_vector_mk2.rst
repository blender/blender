Random Vector
=============

Functionality
-------------

Produces a list of random unit vectors from a seed value.


Inputs & Parameters
-------------------

+------------+-------------------------------------------------------------------------+
| Parameters | Description                                                             |
+============+=========================================================================+
| Count      | Number of random vectors numbers to spit out                            |
+------------+-------------------------------------------------------------------------+
| Seed       | Accepts float values, they are hashed into *Integers* internally.       |
+------------+-------------------------------------------------------------------------+
| Scale      | Scales vertices on some value *Floats*.                                 |
+------------+-------------------------------------------------------------------------+

Outputs
-------

A list of random unit vectors, or nested lists.

Examples
--------

Notes
-----

Seed is applied per output, not for the whole operation (Should this be changed?)
A unit vector has length of 1, a convex hull of random unit vectors will approximate a sphere with radius off 1.

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/5783432/19576172/09c7d264-9723-11e6-86fc-3b6acd0b5d53.png
  :alt: randomvector1.PNG
.. image:: https://cloud.githubusercontent.com/assets/5783432/19576267/666a5ad2-9723-11e6-93df-7f0fbfb712e2.png
  :alt: randomvector2.PNG