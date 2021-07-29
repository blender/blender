Object ID Selector
==================

Functionality
-------------

Has the ability to select items from **bpy.data.**. Some data types have more options than others. Currently the extended options are available for Images, Objects and Grease Pencil.

- Images : lets you pick a name, and get the flattened pixels (by ticking **pass pixels**)  
- Objects : lets you pick by name, or leave the name blank and you'll get the list of objects for the selected type
- Grease Pencil : you can also pick a layer by name or leave blank, if you pick by name you'll get the option to pick **active_frame** for that layer or available frames. Ticking **pass_points** will pass


Example of usage
----------------

See the development thread:
https://github.com/nortikin/sverchok/issues/1379#issuecomment-287331274