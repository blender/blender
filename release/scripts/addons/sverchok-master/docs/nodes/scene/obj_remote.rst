Object Remote
=============

This node is a convenience node.

 .. image:: https://cloud.githubusercontent.com/assets/619340/11479731/ce5109e2-9793-11e5-861b-70b23705dda7.png

Its features are very limited:

- pick an object with the picker
- add vector sockets to control Location / Scale / Rotation (Euler in Rads)

That's it.

Implementation details
======================

It's a nice short node, one thing it does to avoid setting your Object's scale to 0,0,0 is to detect if 0,0,0 is passed. If 0,0,0 is passed it turns it into 0.00001, 0.00001, 0.00001. The reason for this is that an Object's scale can not be set to 0,0,0 in Blender, it seems to wreck the internal transform matrix.