**********************************
Introduction to modular components
**********************************

**prerequisites**

Same as lesson 01.

Status: **WIP**
---------------

Lesson 03 - A Grid
------------------

Grids are another common geometric primitive. A Grid can be thought of as a Plane subdivided over its *x* and *y* axes. Sverchok's `Plane` generator makes grids (including edges and polygons), but we will combine the elementary nodes to build one from scratch. Doing this will cover several important concepts of parametric design, and practical tips for the construction of dynamic  topology.

**What do we know about Grids?**

For simplicity let's take a subdivided `Plane` as our template. We know it's flat and therefore the 3rd dimension (z) will be constant. We can either have a uniformly subdivided Plane or allow for x and y to be divided separately. A separate XY division is a little bit more interesting, let's go with that. 

.. image:: https://cloud.githubusercontent.com/assets/619340/5506680/c59524c6-879c-11e4-8f64-53e4b83b05a8.png

**Where to start?**

For non trivial objects we often use a notepad (yes, actual paper -- or blender Greasepencil etc) to draw a simplified version of what we want to accomplish. On a drawing we can easily name and point to properties, see relationships, and even solve problems in advance.

I chose a Grid because it has only a few properties: `X Side Length, Y Side Length, X num subdivs, Y num subdivs`. These properies can be exposed in several ways. You could expose the number of divisions (edges) per side, or the amount of vertices per side. 1 geometric property, but two ways of looking at it.

.. image:: https://cloud.githubusercontent.com/assets/619340/5514705/5b6766a8-8847-11e4-8812-76916faece52.png

**Decide which variables you want to expose**

The upside of building generators from scratch is that you can make decisions based on what is most convenient for you. Here I'll pick what I think is the most convenient, it can always be changed later.

- Division distance side X
- Division distance side Y
- Number Vertices on side X
- Number Vertices on side Y

**Think in Whole numbers (ints) if you can**

What I mean by this is, reduce the problem to something that is mathematically uncomplicated. Here's a grid drawn on an xy graph to illustrate the coordinates. The z-dimension could be ignored but it's included for completeness.

.. image:: https://cloud.githubusercontent.com/assets/619340/5505509/ef999dd4-8791-11e4-8892-b46ab9688ad2.png

The reason I pick 4 verts for the X axis and 3 for Y, is because that's the smallest useful set of vertices we can use as a reference. The reason i'm not picking 3*3 or 4*4 is because using different vertex counts makes it clear what that `X` axis might have some relation to 4 and to `Y` to 3.

If you consider the sequence just by looking at the first index of each vertex, it goes ``[0,1,2,3,0,1,2,3,0,1,2,3]``. We can generate sequences like that easily. When we look at the second index of these vertices that sequence is ``[0,0,0,0,1,1,1,1,2,2,2,2]``, this also is easy to generate. 

**Using `modulo` and `integer division` to get grid coordinates**

I hope you know Python, or at the very least what `% (modulo)` and `// (int div)` are. The sequences above can be generated using code this way -- If this code doesn't make sense keep reading, it's explained further down::

    # variables
    x = 4
    y = 3
    j = x * y          # 12

    # using for loop
    final_list = []
    for i in range(j):
       x = i % 4       # makes: 0 1 2 3 0 1 2 3 0 1 2 3
       y = i // 4      # makes: 0 0 0 0 1 1 1 1 2 2 2 2
       z = 0
       final_list.append((x, y, z))

    print(final_list)
    '''
    >> [(0, 0, 0), (1, 0, 0), (2, 0, 0), (3, 0, 0), 
    >>  (0, 1, 0), (1, 1, 0), (2, 1, 0), (3, 1, 0), 
    >>  (0, 2, 0), (1, 2, 0), (2, 2, 0), (3, 2, 0)]
    '''

    # using list comprehension
    final_list = [(i%4, i//4, 0) for i in range(j)]

With any luck you aren't lost by all this code, visual programming is very similar except with less typing. The plumbing of an algorithm is still the same whether you are clicking and dragging nodes to create a flow of information or writing code in a text editor.

**Operands**

We introduced the Math node in lesson 01 and 02, the Math node (from the Number menu) has many operations called operands. We'll focus on these to get the vertex components.

+----------------------+---------+--------------------------------------------------------+
| Operand              |  Symbol | Behaviour                                              |  
+======================+=========+========================================================+
| Modulo (mod)         | %       | ``i % 4`` returns the division remainder of ``i / 4``, | 
|                      |         | rounded down to the nearest whole number               |
+----------------------+---------+--------------------------------------------------------+
| Integer Division     | //      | ``i // 4`` returns the result of ``i / 4``,            |
|                      |         | rounded down to the nearest whole number.              |
+----------------------+---------+--------------------------------------------------------+

We can use: 

- ``i % 4`` to turn ``[0,1,2,3,4,5,6,7,8,9,10,11]`` into ``[0,1,2,3,0,1,2,3,0,1,2,3]``
- ``i // 4`` to turn ``[0,1,2,3,4,5,6,7,8,9,10,11]`` into ``[0,0,0,0,1,1,1,1,2,2,2,2]``


**Making vertices**

A recipe which you should be able to hook up yourself by seeing the example image.

- ``Add -> Vector -> Vector In``
- ``Add -> Number -> Math`` (3x) notice I minimized the Multiplication Node.
- ``Add -> Number -> Integer`` (2x)
- ``Add -> Number -> Range Int``

We multiply ``y=3`` by ``x=4`` to get ``12`` this is the number of vertices. This parameter determines the length of the range ``[0,1..11]`` (12 vertices, remember we start counting indices at 0).

.. image:: https://cloud.githubusercontent.com/assets/619340/5477351/e15771f0-862a-11e4-8085-289b88d4cb6a.png

With all nodes hooked up correctly you can hook ``Vector In``'s output to the `vertices` socket of a ViewerDraw node to display the vertices. To test if it works you can use the sliders on the two Integer nodes to see the grid of vertices respond to the two parameters. Remember to put these sliders back to 3 and 4 (as displayed in the image), to continue to the next step.

**Making Polygons**

This might be obvious to some, so this is directed at those who've never done this kind of thing before. This is where we use a notepad to write out the indexlist for the 6 polygons (two rows of 3 polygons, is the result of a x=4, y=3 grid). Viewing the vertices from above, go clockwise. The order in which you populate the the list of polygons is determined by what you find more convenient.

For my example, I think of the X axis as the Columns, and I go from left to right and upwards

.. image:: https://cloud.githubusercontent.com/assets/619340/5514961/5ef77828-8854-11e4-81b4-4bd30a75d177.png

Notice that between polygon index 2 and 3 there is a break in the pattern. The polygon with vertex indices ``[3,7,8,4]`` doesn't exist (for a grid of x=4, y=3), if we did make that polygon it would connect one Row to the next like so:

.. image:: https://cloud.githubusercontent.com/assets/619340/5515010/d58119fc-8856-11e4-837a-44beb57c3fb4.png

We know how many polygons we need (let's call this number ``j``), it is useful to think of an algorithm that produces these index sequences based on a range from ``0 thru j-1`` or ``[0,1,2,3,4,5]``. We can first ignore the fact that we need to remove every n-th polygon, or avoid creating it in the first place. Whatever you decide will be a choice between convenience and efficiency - I will choose convenience here.

**A polygon Algorithm**

  Sverchok lets you create complex geometry without writing a single line of code, but you will not get the most out of the system by avidly avoiding code. Imagine living a lifetime without ever taking a left turn at a corner, you would miss out on faster more convenient ways to reach your destination.


It's easier for me to explain how an algorithm works, and give you something to test it with, by showing the algorithm as a program, a bit of Python. Programming languages allow you to see without ambiguity how something works by running the code.

**WIP - NOT ELEGANT**

this generates faces from a vertex count for x,y::

  ny = 3
  nx = 4

  faces = []
  add_face = faces.append

  total_range = ((ny-1) * (nx))
  for i in range(total_range):
      if not ((i+1) % nx == 0):  # +1 is the shift
          add_face([i, i+nx, i+nx+1, i+1])  # clockwise

  print(faces)

This is that same algorithm using the elementary nodes, can you see the similarity?

.. image:: https://cloud.githubusercontent.com/assets/619340/5515808/31552e1a-887c-11e4-9c74-0f3af2f193e6.png


// -- TODO





