Torus
========

Functionality
-------------

Torus generator will create a torus based on its radii sets, number of sections and section phases.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.

- **Major Radius**    [1]
- **Minor Radius**    [1]
- **Exterior Radius** [2]
- **Interior Radius** [2]
- **Revolution Sections**
- **Spin Sections**
- **Revolution Phase**
- **Spin Phase**

Notes:
[1] : Major/Minor radii are available when Major/Minor mode is elected.
[2] : Exterior/Interior radii are available when Exterior/Interior mode is elected.

Parameters
----------

The MODE parameter allows to switch between Major/Minor and Exterior/Interior
radii values. The input socket values for the two radii are interpreted as such
based on the current mode.

All parameters except **mode** and **Separate** can be given by the node or an external input.

+-------------------------+------------+------------+-----------------------------------------------+
| Param                   |  Type      |  Default   |  Description                                  |
+=========================+============+============+===============================================+
| **Major Radius**        |  Float     |  1.00      |  Major radius of the torus [1]                |
+-------------------------+------------+------------+-----------------------------------------------+
| **Minor Radius**        |  Float     |  0.25      |  Minor radius of the torus [1]                |
+-------------------------+------------+------------+-----------------------------------------------+
| **Exterior Radius**     |  Float     |  1.25      |  Exterior radius of the torus [2]             |
+-------------------------+------------+------------+-----------------------------------------------+
| **Interior Radius**     |  Float     |  0.75      |  Interior radius of the torus [2]             |
+-------------------------+------------+------------+-----------------------------------------------+
| **Revolution Sections** |  Int       |  32        |  Number of sections around torus center       |
+-------------------------+------------+------------+-----------------------------------------------+
| **Spin Sections**       |  Int       |  16        |  Number of sections around torus tube         |
+-------------------------+------------+------------+-----------------------------------------------+
| **Revolution Phase**    |  Float     |  0.00      |  Phase revolution sections by a radian amount |
+-------------------------+------------+------------+-----------------------------------------------+
| **Spin Phase**          |  Float     |  0.00      |  Phase spin sections by a radian amount       |
+-------------------------+------------+------------+-----------------------------------------------+
| **Separate**            |  Bolean    |  False     |  Grouping vertices by V direction             |
+-------------------------+------------+------------+-----------------------------------------------+

Notes:
[1] : Major/Minor radii are available when Major/Minor mode is elected.
[2] : Exterior/Interior radii are available when Exterior/Interior mode is elected.

Outputs
-------

**Vertices**, **Edges**, **Polygons** and **Normals**
All outputs will be generated when connected.


Example of usage
----------------

