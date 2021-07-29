Mesh Expression Node
====================

Functionality
-------------

This node generates mesh from description in JSON format. Variables and mathematical expressions are allowed in definitions of vertex coordinates, so exact shape of mesh can be parametrized. All variables used in JSON definition become inputs of node.
It is also possible to generate JSON description from existing mesh.

Usual workflow
--------------

1. Create some mesh object by using usual Blender's modelling techniques. Select that mesh.
2. Press "from selection" button in Mesh Expression node. New text buffer will appear in Blender.
3. Switch to Blender's text editor and select newly created buffer.
4. Edit defintion. You can replace any of vertex coordinates with expression enclosed in double-quotes, such as `"x+1"`. See also syntax description below.
5. Optionally, you can add "defaults" key to definition, with default values of variables.
6. In Mesh Expression node, all variables used in JSON definition will appear as inputs.

JSON syntax
-----------

For generic description of JSON, please refer to https://en.wikipedia.org/wiki/JSON.

Mesh Expression node uses JSON, which should be a dictionary with following keys:

* "vertices". This should be a list, containing 3- or 4- item lists:
  
  * First 3 items of each list are vertex coordinates. Each coordinate should be either integer or floating-point number, or a string with valid expression (see expression syntax below).
  * 4th item, if present, may be either string of list of strings. These strings denote vertex groups, to which this vertex belongs.

  Examples of valid vertex definition are:
  
  * `[0, 0, 0]` 
  * `["X", "Y", 1.0]`
  * `[1, 2, 3, "Selected"]`
  * `[3, 2, 1, ["Top", "Right"]]`
* "edges". This should be a list, containing 2-item lists of integer numbers, which are edges description in Sverchok's native format.
* "faces". This should be a list, containint lists of integer nubmers, which are mesh faces description in Sverchok's native format.
* "defaults". This should be a dictionary. Keys are variable names, and values are default variable values. Values can be:
  
  * integer or floating-point numbers;
  * string expressions (see expression syntax below). Note that expressions in "defaults" section are evaluated in alphabetical order of variable names. So, you can express "Y" in terms of "X", but not vice versa.

See also JSON examples below.

Expression syntax
-----------------

Expressions used in place of vertex coordinates are usual Python's expressions. 

For exact syntax definition, please refer to https://docs.python.org/3/reference/expressions.html.

In short, you can use usual mathematical operations (`+`, `-`, `*`, `/`, `**` for power), numbers, variables, parenthesis, and function call, such as `sin(x)`.

One difference with Python's syntax is that you can call only restricted number of Python's functions. Allowed are:

* sin
* cos
* pi
* sqrt

This restriction is for security reasons. However, Python's ecosystem does not guarantee that noone can call some unsafe operations by using some sort of language-level hacks. So, please be warned that usage of this node with JSON definition obtained from unknown or untrusted source can potentially harm your system or data.

Examples of valid expressions are:

* "1.0"
* "x"
* "x+1"
* "0.75*X + 0.25*Y"
* "R * sin(phi)"

Inputs
------

Set of inputs for this node depends on used JSON definition. Each variable used in JSON becomes one input. If there are no variables used in JSON, then this node will have no inputs.

Parameters
----------

This node has the following parameters:

- **File name**. Its value should be the name of existing Blender's text buffer.
- **Precision**. Number of decimal places used for points coordinates when
  generating mesh definition by **from selection** operator. Default value is
  8. This parameter is only available in the N panel.
- **Example tree**. If checked, then an example set of nodes (demonstrating
  possible usage of Mesh Expression) will be created automatically when you
  press **from selection**. By default this is not checked. This parameter is
  only available in the N panel.

Operators
---------

This node has one button: **from selection**. This button takes currently selected Blender's mesh object and puts it's JSON description into newly created text buffer. Name of created buffer is assigned to **File name** parameter.

For each vertex, if it belongs to some vertex groups in initial mesh object, these group names will be added to vertex definition.

If vertex is selected in edit mode, then special group named "Selected" will be added to vertex definition.

Outputs
-------

This node always has the following outputs:

* **Vertices**
* **Edges**
* **Faces**

Apart from these, a separate output is created for each name of vertex group mentioned in "vertices" section of JSON definition. Each of these outputs contain a mask for **Vertices**, which selects vertices from corresponding group.

Examples of usage
-----------------

Almost trivial, a plane with adjusable size:

::

  {
    "faces": [
      [      0,      1,      3,      2    ]
    ],
    "edges": [
      [      0,      2    ],
      [      0,      1    ],
      [      1,      3    ],
      [      2,      3    ]
    ],
    "vertices": [
      [ "-Size",      "-Size",      0.0    ],
      [ "Size",      "-Size",      0.0    ],
      [ "-Size",      "Size",      0.0    ],
      [ "Size",      "Size",      0.0    ]
    ]
  }

.. image:: https://cloud.githubusercontent.com/assets/284644/24079413/a2757a08-0cb1-11e7-9ef5-155c888b38dd.png

More complex example: `Example JSON definition <https://gist.github.com/portnov/3aae2b0e0f2d21a8da2d61fc28a96790>`_:

.. image:: https://cloud.githubusercontent.com/assets/284644/24079457/a47553ae-0cb2-11e7-9b25-096cdf88a4a1.png

You can find more examples `in the development thread <https://github.com/nortikin/sverchok/issues/1304>`_.

