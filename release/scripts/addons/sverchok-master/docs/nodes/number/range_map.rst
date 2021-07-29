Map Range
=========

This node map all the incoming values in the desired range.

Input and Output
^^^^^^^^^^^^^^^^
All the values except clamp, may be floats or int.

+-------------------+-----------------------+
| socket            | description           |
+===================+=======================+
| **inputs**        |                       |
+-------------------+-----------------------+
| value             | incoming float values |
+-------------------+-----------------------+
| Old Min           | old minimun value     |
+-------------------+-----------------------+
| Old Max           | old maximum value     |
+-------------------+-----------------------+
| New Min           | new minimum value     |
+-------------------+-----------------------+
| New Max           | new maximum value     |
+-------------------+-----------------------------+
| clamp             | clamp the values if         |
|                   | they are outside the range  |
+-------------------+-----------------------------+
| **outputs**       |                       |
+-------------------+-----------------------+
| value             | outcoming values      |
+-------------------+-----------------------+

Examples
--------

basic example:

..image:: https://cloud.githubusercontent.com/assets/1275858/24461336/0a4204d4-14a1-11e7-9e72-907a627c1cd0.png

basic example with clamping:

..image:: https://cloud.githubusercontent.com/assets/1275858/24461347/1224300a-14a1-11e7-85da-5376a858c7bb.png

example with variable lacunarity node:

..image:: https://cloud.githubusercontent.com/assets/1275858/24462196/aa61cfd8-14a3-11e7-8ef9-9f1f3da264d5.png

In this example we need to map in the range (0.0, 1.0) because otherwise the image
will be understaurated. See the incoming range (-0.769, 0.892)
