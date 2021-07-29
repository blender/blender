Ring
====

Functionality
-------------

Ring generator will create a 2D ring based on its radii sets, number of sections and phase.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.

- **Major Radius**    [1]
- **Minor Radius**    [1]
- **Exterior Radius** [2]
- **Interior Radius** [2]
- **Radial Sections**
- **Circular Sections**
- **Phase**

Notes:
[1] : Major/Minor radii are available when Major/Minor mode is elected.
[2] : Exterior/Interior radii are available when Exterior/Interior mode is elected.

Parameters
----------

The MODE parameter allows to switch between Major/Minor and Exterior/Interior
radii values. The input socket values for the two radii are interpreted as such
based on the current mode.

All parameters except **Mode** and **Separate** can be given by the node or an external input.

+------------------------+-----------+-----------+---------------------------------------------+
| Param                  |  Type     |  Default  |  Description                                |
+========================+===========+===========+=============================================+
| **Major Radius**       |  Float    |  1.00     |  Major radius of the ring [1]               |
+------------------------+-----------+-----------+---------------------------------------------+
| **Minor Radius**       |  Float    |  0.25     |  Minor radius of the ring [1]               |
+------------------------+-----------+-----------+---------------------------------------------+
| **Exterior Radius**    |  Float    |  1.25     |  Exterior radius of the ring [2]            |
+------------------------+-----------+-----------+---------------------------------------------+
| **Interior Radius**    |  Float    |  0.75     |  Interior radius of the ring [2]            |
+------------------------+-----------+-----------+---------------------------------------------+
| **Radial Sections**    |  Int      |  32       |  Number of sections around the ring center  |
+------------------------+-----------+-----------+---------------------------------------------+
| **Circular Sections**  |  Int      |  3        |  Number of sections accross the ring band   |
+------------------------+-----------+-----------+---------------------------------------------+
| **Phase**              |  Float    |  0.00     |  Phase of the radial sections (in radians)  |
+------------------------+-----------+-----------+---------------------------------------------+
| **Separate**           |  Bolean   |  False    |  Grouping vertices by V direction           |
+------------------------+-----------+-----------+---------------------------------------------+

Notes:
[1] : Major/Minor radii are available when Major/Minor mode is elected.
[2] : Exterior/Interior radii are available when Exterior/Interior mode is elected.

Outputs
-------

**Vertices**, **Edges** and **Polygons**.
All outputs will be generated when connected.


Example of usage
----------------

