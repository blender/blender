Scripted Node 2(Generator)
=========================

aka Script Node MK2

- Introduction
- Features
- Structure
- Templates
- Conveniences
- Examples
- Techniques to improve Python performance
- Limitations

Introduction
------------

When you want to express an idea in written form and the concept is suitable
for a one line Python expression then often you can use a Formula node. If you
need access to imports, classes, temporary variables, and functions then you can 
write a script to load into Script Node 2. 

Script Node MK2 differs from Script Node iteratrion 1 in that offers more control.
It also has a prototype system where you could for example reuse the behavior of 
a generator and the template takes care of all the details leaving you to focus 
on the function. Scripts using the templates automatically becomes more powerful.

It's a prototype so bug reports, questions and feature request are very welcome.

Features
--------

allows:

- Loading/Reloading scripts currently in TextEditor
- imports and aliasing, ie anything you can import from console works in SN2
- nested functions and lambdas
- named inputs and outputs
- named operators (buttons to action something upon button press)

Structure
---------

At present all scripts for SN2 must:

- be subclasses SvScript
- include a function called process in the class
- have member attributes called ``inputs`` and ``outputs``
- have one Script class per file, if more than one, last one found will be used

**process(self)**


``process(self)`` is the main flow control function. It is called when all sockets
without defaults are connected. Usually the template provides a ``process`` function
for you.

**inputs**

Default can be a float or integer value, not other types are usable yet::

    inputs = [
        [type, 'socket name on ui', default],
        [type, 'socket name on ui2', default],
        # ...
    ]


**outputs**

::

    outputs = [
        [type, 'socket name on ui'],
        [type, 'socket name on ui 2'],
        # ...
    ]

**inputs and outputs**

- Each *socket name on ui* string shall be unique.

- **type** are currently limited to
   
   +---------+-------------------------------------+
   | type id | type data                           | 
   +=========+=====================================+
   | 's'     | floats, ints, edges, faces, strings |
   +---------+-------------------------------------+
   | 'v'     | vertices, vectors, 3-tuples         | 
   +---------+-------------------------------------+
   | 'm'     | matrices, 4 x 4 nested lists        |
   +---------+-------------------------------------+

There are a series of names that have special meaning that scripts should 
avoid as class attributes or only used for the intended meaning. To be described:
 ``node`` ``draw_buttons`` ``update`` ``process`` ``enum_func`` ``inputs``
``outputs``


Templates
---------

Sverchok includes a series of examples for the different templates.  


Conveniences
------------

We value our time, we are sure you do too, so features have been added to help speed up the 
script creation process.

**Text Editor**

- can refresh the Script Node which currently loads that script by hitting ``Ctrl+Enter``

Main classes for your subclasses are:

 - ``SvScript``
 - ``SvScriptSimpleGenerator``
 - ``SvScriptSimpleFunction``

Limitations
-----------

Using ``SvScriptSimpleGenerator`` and ``SvScriptSimpleFunction`` you limit inputs to deal with one object. 
For plane, for example, you'll get next data:

 [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (1.0, 1.0, 0.0)] [(0, 1, 3, 2)]

If you need Full support of Sverchok data - you'd better use ``SvScript`` 
class and ``self.node.inputs[0].sv_get()`` function.

Examples
--------

The best way to get familiarity with Script Node 2 is to go through the templates folder.
They are intended to be lightweight and educational, but some of them will show
advanced use cases. The images and animations on this `thread on github 
<https://github.com/nortikin/sverchok/issues/439>`_. 
may also provide some insight into what's possible.

A typical nodescript using the ``SvScriptSimpleGenerator`` may look like this, note that
the third argument for outputs is specific to this template::

    import numpy 
    import itertools

    class GridGen(SvScriptSimpleGenerator):
        inputs = [("s", "Size", 10.0),
                  ("s", "Subdivs", 10)]
        outputs = [("v", "verts", "make_verts"),
                   ("s", "edges", "make_edges")]
        
        @staticmethod
        def make_verts(size, sub):
            side = numpy.linspace(-size / 2, size / 2, sub)
            return tuple((x, y, 0) for x, y in itertools.product(side, side))
            
        @staticmethod
        def make_edges(size, sub):
            edges = []       
            for i in range(sub):
                for j in range(sub - 1):
                    edges.append((sub * i + j, sub * i + j + 1))
                    edges.append((sub * j + i, sub * j + i + sub))
            return edges


Note that here the name of the method that should be called for producing data 
for each socket in the final last arguments to ``outputs`` but we are not forced 
to have all code inside the class, we can also do
::

    def lorenz(N, verts, h, a, b, c):
        add_vert = verts.append

        x0 = 0.1
        y0 = 0
        z0 = 0
        for i in range(N):
            x1 = x0 + h * a * (y0 - x0)
            y1 = y0 + h * (x0 * (b - z0) - y0)
            z1 = z0 + h * (x0 * y0 - c * z0)
            x0, y0, z0 = x1, y1, z1

            add_vert((x1,y1,z1))
            
    class LorenzAttractor(SvScriptSimpleGenerator):

        inputs = [
            ['s', 'N', 1000],
            ['s', 'h', 0.01],
            ['s', 'a', 10.0],
            ['s', 'b', 28.0],
            ['s', 'c', 8.0/3.0]
        ]

        @staticmethod
        def make_verts(N, h, a, b, c):
            verts = []
            lorenz(N, verts, h, a, b, c)
            return verts

        @staticmethod
        def make_edges(N, h a, b, c:
            edges = [(i, i+1) for i in range(N-1)]
            return edges

        outputs = [
            ['v','verts', "make_verts"],
            ['s','edges', "make_edges"]
        ]


Here is a simple script for deleting loose vertices from mesh data, it also serves as an 
illustration for a type of script that uses the ```SvScriptSimpleFunction``` template that
has one main function that decomposes into separate sockets. The methods don't have be static
but in general it is good practice to keep them free from side effects.
::

    from itertools import chain

    class DeleteLooseVerts(SvScriptSimpleFunction):
        inputs = [
            ('v', 'verts'),
            ('s', 'pol')
            ]
        outputs = [
            ('v', 'verts'),
            ('s', 'pol')
            ]
        
        # delete loose verts 
        @staticmethod
        def function(*args, **kwargs):
            ve, pe = args       
            # find used indexes
            v_index = sorted(set(chain.from_iterable(pe)))
            # remap the vertices
            v_out = [ve[i] for i in v_index]
            # create a mapping from old to new vertices index
            mapping = dict(((j, i) for i, j in enumerate(v_index)))
            # apply mapping to input polygon index
            p_out = [tuple(map(mapping.get, p)) for p in pe]
            return v_out, p_out


Breakout Scripts
----------------
Scripts that needs to access the node can do so via the ```self.node``` variable
that is automatically set.
::

    class Breakout(SvScript):
        def process(self):
            pass
            
        def update(self):
            node = self.node
            node_group = self.node.id_data
            # here you can do anything to the node or node group
            # that real a real node could do including multisocket
            # adaptive sockets etc. templates and examples for this are
            # coming


Admit, you can call sockets data directly when using ```SvScript``` as ```self.node.inputs[0].sv_get()```.
And other ```self.node.``` operations possible from this class.


Techniques to improve Python performance
----------------------------------------

There are many ways to speed up python code. Some slowness will be down to
innefficient algorithm design, other slowness is caused purely by how much
processing is minimally required to solve a problem. A decent read regarding
general methods to improve python code performance can be found
on `python.org <https://wiki.python.org/moin/PythonSpeed/PerformanceTips>`_.
If you don't know where the cycles are being consumed, then you don't know
if your efforts to optimize will have any significant impact.

Read these 5 rules by Rob Pike before any optimization.
http://users.ece.utexas.edu/~adnan/pike.html

Limitations
-----------

Most limitations are voided by increasing your Python and ``bpy`` skills. But
one should also realize what is approriate for a node script to do.


That's it for now.
