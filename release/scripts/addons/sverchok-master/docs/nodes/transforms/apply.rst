Matrix Apply
============

Functionality
-------------

Applies a Transform Matrix to a list or nested lists of vectors (and therefore vertices)


Inputs
------

+----------+-----------------------------------------------------------------------------+
| Inputs   | Description                                                                 |
+==========+=============================================================================+
| Vectors  | Represents vertices or intermediate vectors used for further vector math    |
+----------+-----------------------------------------------------------------------------+
| Matrices | One or more, never empty                                                    |
+----------+-----------------------------------------------------------------------------+


Outputs
-------

Nested list of vectors / vertices, matching the number nested incoming *matrices*.


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4186410/a3e00666-3760-11e4-9d67-b32345329e9d.PNG
  :alt: MatrixApplyDemo1.PNG

.. image:: https://cloud.githubusercontent.com/assets/619340/4186411/a3e1c14a-3760-11e4-84fe-2acaf1858ad7.PNG
  :alt: MatrixApplyDemo5.PNG


Notes
-------

The ``update`` function is outdated, functionally this is of no relevance to users but we should change it for future compatibility.