Text In
========

Functionality
-------------

Import data from text editor in formats csv, json or plain sverchok text.

Properties
----------

**Select** - Select text from blender text editor
**Select input format** - Property to choose between csv, plain sverchok and json data format
  **csv**:
    **Header fields** - to use headers from file
    **Dialect** - to choose dialect of imported table
  **Sverchok**:
    **Data type** - output data socket as selected type
**Load** - Load data from text in blend file

Outputs
-------

**vertices**, **data**, **matrices** - if sverchok plain data selected

**Col** - if csv data selected

**Random** - if json data selected
