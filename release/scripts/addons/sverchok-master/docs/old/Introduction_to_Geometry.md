### basics

If you've ever created a mesh and geometry programatically then you can skip this section. If you are uncertain what any of the following terms mean then use it as a reference for further study. 

> Vertex, Vector, Index, Edge, Face, Polygon, Normal, Transformation, and Matrix.

#### 3d geometry  
  
The most fundamental element you need to know about is the `vertex` (plural is vertices). A vertex is a point in 3d space described by 3 or 4 values which represent its X, Y and Z location. Optionally a 4th value can represent a property of the vertex, usually _influence_ or _weight_ and is denoted by W.  
  
Vertices are the special, limited, case of `vectors`. Understanding vector math is an integral part of parametric modeling and generative design. The various ways in which vectors can be manipulated will be covered in subsequent parts. If you want to do cool stuff with Sverchok spend time getting to understand vector based math, it will be time well spent.  
  
In computer graphics each vertex in an object has an `index`. The index of the first vertex is 0 and the last has index (number_of_vertices - 1). Conceptually each vertex is identified uniquely by its order of appearance in the list of vertices of the object.

A quick Python example should clarify this. The following would make 3 vertices. In this case each vertex has 3 components.

    v0 = (1.0, 1.0, 0.0)
    v1 = (0.5, 0.6, 1.0)
    v2 = (0.0, 1.0, 0.0)

Mesh objects in Blender contain geometric data stored in `lists`. In Python and Sverchok an empty list looks like `[]`. Vertices are stored in lists too, a list with 3 vertices might look like:

    vertices = [
        (1.0, 1.0, 0.0),
        (0.5, 0.6, 1.0),
        (0.0, 1.0, 0.0)
    ]

The first vertex has index 0. The second vertex has index 1 etc,.

`Edges` - generally speaking - form a bond between 2 vertices. Edges are also stored in a list associated with the mesh object. If we want to declare edges, we reference the vertices by their index. For example: `edges = []` would set up an empty list to hold the edges, but doing `edges = [[0, 1], [1, 2], [2, 0]]` forms 3 edges. Here you see we are using lists inside lists to help separate the edges. 

> Lists are ordered storage.

Polygons - also called Faces - are built using the same convention. The main difference is that polygons include at least 3 unique vertex indices. For the purposes of this introduction we'll only cover polygons made from 3 or 4 vertices, these are called Tris and Quads respectively. Let's add another 3 vertices to make 2 distinct polygons. `polygons = [[0, 1, 2], [3, 4, 5]]` 

In Blender you might mix Tris and Quads in one polygon list during the modelling process, but for Sverchok geometry you'll find it more convenient to create separate lists for each and combine them at the end.  
  
An example that sets us up for the first Sverchok example is the cube. Conceptually in python this looks like:

    v0 = Vector((x,y,z))
    v1
    v2
    v3
    v4
    v5
    v6
    v7
    
    vertices = [v0, v1, v2, v3, v4, v5, v6, v7]
    
    edges = []   # empty list for now.
    
    polygons = [[],[],[],[],[],[]]
    

Once you define polygons then you are also defining edges implicitely. If a polygon has 4 vertices, then it also has 4 edges. Two adjacent polygons may share edges. I think this broadly covers the things you should be comfortable with before Sverchok will make sense.

###Sverchok

This section will introduce you to a selection of nodes that can be combined to create renderable geometry. Starting with the simple Plane generator


