************************
Introduction to geometry
************************

Basics
======

If you've ever created a mesh and geometry programatically then you can skip this section.
If you are uncertain what any of the following terms mean then use it as a reference for 
further study::

    List, Index, Vector, Vertex, Edge, Polygon, Normal, Transformation, and Matrix.


List
----

As a visual programming language Sverchok borrows many terms from text-based programming languages, specifically from ``Python``. Sverchok uses *Lists* to store geometry, *Lists* offer fast and ordered storage. The items stored in Lists are called *Elements*. Each element of a List is assigned a unique sequential index. 


Index
-----

*plural: Indices*

Indices allow us to quickly reference a specific element of a List. The index of the first element is 0 and the index of the last element is equal to the number of total elements minus 1. 


3D Geometry
===========

Vector
------

The most fundamental element you need to know about is the *Vector*. Think of Vectors as things that have a multitude of properties (also called components). For example *House prices* are calculated depending on maybe 20 or more different properties: floor space, neighbourhood, age, any renovations, rooms, bathrooms, garage... The point is, a house can be seen as a Vector datapoint::

    House_one = Vector((floor_space, neighbourhood, age, renovations, rooms, ...))

    # or simply
    House_one = (floor_space, neighbourhood, age, renovations, rooms, ...)

3D Geometry concentrates mostly on a small number of components. ``X, Y, Z, and maybe W``. If you've ever scaled or moved a model in 3d space you have performed Vector Math on the locations of those 3d points. The concept of *3d location* or *point in 3d space* is so important that the Vector used to describe the idea has a special name; *Vertex*, and it is a special, limited, case of a *Vector*. More about this later.

Understanding Vectors and Vector math is an integral part of parametric modeling and generative design, and it's a lot easier than it might appear at first. You won't have to do the calculations yourself, but you will need to feed Sverchok meaningful input. The good news is that figuring out what Vector math operations produce which results can be learned through observation and understood by experimenting interactively.

The various ways in which Vectors can be manipulated will be covered in subsequent parts. If you want to do cool stuff with Sverchok spend time getting to understand Vector based math, it will be time well spent. 

Vertex
------

*plural: Vertices*

A vertex is a point in 3d space described by 3 or 4 values which represent its X, Y and Z location. Optionally a 4th value can represent a property of the vertex, usually *influence* or *weight* and is denoted by **W**.

A quick Python example should clarify this. The following would make 3 vertices.
In this case each vertex has 3 components.::

    v0 = (1.0, 1.0, 0.0)
    v1 = (0.5, 0.6, 1.0)
    v2 = (0.0, 1.0, 0.0)

Mesh objects in Blender contain geometric data stored in *lists*. In Python and
Sverchok an empty list looks like ``[ ]`` (open and closed square brackets). Vertices are stored in lists too,
a list with 3 vertices might look like::

    vertices = [
        (1.0, 1.0, 0.0),
        (0.5, 0.6, 1.0),
        (0.0, 1.0, 0.0)
    ]


Edges
-----

*Edges* form a bond between 2 vertices. Edges are also stored in a list associated 
with the mesh object. For example the following sets up an empty list to hold the edges::

    edges = []

If we want to declare edges, we reference the vertices by index. Below is an example of
how 3 edges are created::

    edges = [[0, 1], [1, 2], [2, 0]]

Here you see we are using lists inside lists to help separate the edges. This is called *Nesting*


Polygons
--------

*also called Faces or Polys*

Polygons are built using the same convention as Edges. The main difference is that polygons include at least 3 unique vertex indices. For the purposes of this introduction we'll only cover polygons made from 3 or 4 vertices, these are called *Tris and Quads* respectively. 

Now imagine we have a total of 6 vertices, the last vertex index is 5. If we want
to create 2 polygons, each built from 3 vertices, we do::

    polygons = [[0, 1, 2], [3, 4, 5]]

In Blender you might mix Tris and Quads in one object during the modelling process, 
but for Sverchok geometry you'll find it more convenient to create separate lists for each and combine them at the end.

An example that sets us up for the first Sverchok example is the following pyhon code::

    # this code can be run from Blender Text Editor and it will generate a Cube.
    
    import bpy
    
    verts = [
        ( 1.0, 1.0,-1.0),
        ( 1.0,-1.0,-1.0),
        (-1.0,-1.0,-1.0),
        (-1.0, 1.0,-1.0),
        ( 1.0, 1.0, 1.0),
        ( 1.0,-1.0, 1.0),
        (-1.0,-1.0, 1.0),
        (-1.0, 1.0, 1.0)
    ]

    edges = []  # empty list for now.
    
    faces = [
        (0, 1, 2, 3),
        (4, 7, 6, 5),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (4, 0, 3, 7)
    ]
    
    mesh_data = bpy.data.meshes.new("cube_mesh_data")
    mesh_data.from_pydata(verts, edges, faces)
    mesh_data.update()
    
    cube_object = bpy.data.objects.new("Cube_Object", mesh_data)
    
    scene = bpy.context.scene  
    scene.objects.link(cube_object)  
    cube_object.select = True  

If we extract from that the geometry only we are left with::

    v0 = (1.0, 1.0, -1.0)
    v1 = (1.0, -1.0, -1.0)
    v2 = (-1.0, -1.0, -1.0)
    v3 = (-1.0, 1.0, -1.0)
    v4 = (1.0, 1.0, 1.0)
    v5 = (1.0, -1.0, 1.0)
    v6 = (-1.0, -1.0, 1.0)
    v7 = (-1.0, 1.0, 1.0)

    vertices = [v0, v1, v2, v3, v4, v5, v6, v7]

    polygons = [
        (0, 1, 2, 3),
        (4, 7, 6, 5),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (4, 0, 3, 7)
    ]


Side Effect of Defining Polygons
--------------------------------

A chain of Vertex indices defines a polygon and each polygon has edges that make up its boundary. If a polygon has 4 vertices, then it also has 4 edges (or sides..if you prefer). 

**example 1**  

If we take the above polygons list as example and look at the first polygon (index=0), it reads ``(0, 1, 2, 3)``. That polygon therefor defines the following edges ``(0,1),(1,2),(2,3),(3,0)``. The last edge ``(3,0)`` is the edge that closes the polygon. 

**example 2**  

The polygon with index 3 reads ``(1, 5, 6, 2)``, it implies the following edges ``(1,5) (5,6) (6,2) (2,1)``. 


Ready?
------

I think this broadly covers the things you should be
comfortable with before Sverchok will make sense.


Sverchok
--------

This section will introduce you to a selection of nodes that can be combined
to create renderable geometry. Starting with the simple Plane generator
