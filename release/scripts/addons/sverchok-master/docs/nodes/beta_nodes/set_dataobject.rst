Set_dataobject
==============

*destination after Beta:*

Functionality
-------------

*It works with a list of objects and a list of Values*

*multiple lists can be combined into one with the help of ListJoin node*

MK2 version can work with multiple nested lists

When there is only one socket Objects- performs **(Object.str)**

If the second socket is connected Values- performs **(Object.str=Value)**

If the second socket is connected OutValues- performs **(OutValue=Object.str)**

*Do not connect both the Values sockets at the same time*

*First word in str must be property or metod of object*

*Use i. prefix in str to bring second property of same object*

Inputs
------

This node has the following inputs:

- **Objects** - only one list of python objects, like **bpy.data.objects** or **mathutils.Vector**
- **values** - only one list of any type values like **tupple** or **float** or **bpy.data.objects**


Outputs
-------

This node has the following outputs:

- **outvalues** - the list of values returned by the **str** expression for each object

Examples of usage
-----------------

.. image:: https://cloud.githubusercontent.com/assets/7894950/7807360/92f00446-0393-11e5-80e5-21f2679a602a.png

.. image:: https://cloud.githubusercontent.com/assets/7894950/7807387/d2ddd2d6-0393-11e5-9c6b-1a6880fd3c65.png

.. image:: https://cloud.githubusercontent.com/assets/7894950/7807409/0b8ad44e-0394-11e5-930f-2416debba804.png

.. image:: https://cloud.githubusercontent.com/assets/7894950/7901468/a50b74ba-0791-11e5-8fb7-296e866e7db4.png

.. image:: https://cloud.githubusercontent.com/assets/22656834/21178380/56ea8b88-c200-11e6-8c38-be16d880f105.png
