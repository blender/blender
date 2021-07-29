.. _scripted-tutorial:

An introduction and tutorial for the Scripted Nodes
===================================================

> Dealga Mcardle | 2014 | October

In my opinion new users should avoid the Script Nodes until they understand a majority of the existing nodes and the Sverchok `Eco-system` as a concept. This suggestion applies to everyone, even competent coders.

Script Nodes are great when you want to encapsulate a behaviour which may not be easy to achieve with existing nodes alone. They are my prefered way to either 1) prototype code, or 2) write custom nodes that are too specific to be submitted as regular nodes. 

At the moment Sverchok has 2 Scripted Node implementations: SN and SN2. Exactly how they differ from eachother will be shown later. Both offer `practical shorthand` ways to define what a node does, which sliders and sane defaults it might have, and what socket types can connect to it. These scripts have a minimal interface, are stored inside the `.blend` file as plain text python, and can be shared easily. 

If you've ever written code for a Blender addon or script, you will be familiar with registration of classes. Nodes normally also need to be registered so Blender can find them, but Script Nodes don't because they are in essence a shell for your code -- and the shell is already registered, all you have to do is write code to process input into output.

Scripted Node 1 -- an informal introduction
-------------------------------------------

Here is a classic 'Hello World' style example used to demonstrate graphics coding. It's called a Lorenz Attractor. ::


    def lorenz(N, verts):
        add_vert = verts.append
        h = 0.01
        a = 10.0
        b = 28.0
        c = 8.0 / 3.0

        x0 = 0.1
        y0 = 0
        z0 = 0
        for i in range(N):
            x1 = x0 + h * a * (y0 - x0)
            y1 = y0 + h * (x0 * (b - z0) - y0)
            z1 = z0 + h * (x0 * y0 - c * z0)
            x0, y0, z0 = x1, y1, z1

            add_vert((x1,y1,z1))

    def sv_main(N=1000):

        verts = []
        in_sockets = [['s', 'N', N]]
        out_sockets = [['v','verts', [verts]]]

        lorenz(N, verts)
        return in_sockets, out_sockets


Here's what this code produces.

.. image:: https://cloud.githubusercontent.com/assets/619340/5219883/5d3e1252-765d-11e4-87e8-56e1eef2d5ae.png

Infact, here's the Node Interface that the script produces too

.. image:: https://cloud.githubusercontent.com/assets/619340/5219902/a310c824-765d-11e4-9836-c34cb0d8a7b4.png

Compare the code with the image of the node and you might get a fair idea where the sockets are defined and where the default comes from. Look carefully at 
``in_sockets`` and ``out_sockets``, two of the elements are strings (socket type and socket name), and the third element is the Python variable that we automatically bind to those sockets.

Brief Guided Explanation
-------------------------

You've probably got a fair idea already from the example script. SN1 has a few conventions which let you quickly define sockets and defaults. What follows are short remarks about the elements that make up these scripts, aimed at someone who is about to write their first script for SN1.

Sockets
-------

Sverchok at present has 3 main socket types: VerticesSocket, StringsSocket and MatrixSocket. Script Nodes refer to these socket types with only their first letter in lowercase. 's','v','m'::

    's' to hold: floats, ints, edges, faces, strings
    'v' to hold: vertices, vectors, 3-tuples
    'm' to hold: matrices, 4 x 4 nested lists


Socket Names
------------

Each socket has a name. Take a minute to think about a good descriptive name for each. Socket names can always be changed later, but my advice is to use clear names from the very beginning.

Variable names
--------------

Variable names are used to expose the values of the associated socket to your script. If the socket is unconnected then the value of the variable will be taken from the specified default.

node function `(sv_main)`
-------------------------

The main function for SN1 is ``sv_main``, in the body of this function is where we declare socket types, socket names, and variable names for input and output sockets. These are declared in two arrays ``in_sockets`` and ``out_sockets``.

The argument list of ``sv_main`` is where you provide defaults values or the nestedness of an incoming datatype. (don't worry if this makes no sense, read it again later).

That's great, show me!
----------------------

The easiest way to get started is to first load an existing script. Here are some steps:

- Go to `Generators / Scripted Node` and add it to the NodeView.
- Open a Blender TextEditor window so you can see the TextEditor and the NodeView at the same time.
- Paste the Lorenz Attractor script (from above) into the TextEditor and call it 'attractor.py'
- In NodeView look at the field on the second row of the Scripted Node. This is a file selector which shows all Text files in blender. When you click on it you will see "attractor.py"
- Select "attractor.py" press the button the right, the one that looks like a powersocket.
- This changes the way the Node appears. The node will now have 1 input socket and one output socket. It might even have changed to a light blue.

That's pretty much all there is to loading a script. All you do now is hook the output Verts to a Viewer Node and you'll see a classic Lorenz Attractor point set.

Study the sv_main
-----------------

If you look carefully in ``sv_main`` there's not a lot to the whole process. ``sv_main`` has two **required** lists; ``in_sockets`` and ``out_sockets``. sv_main also has a argument list which you must fill with defaults, here the only variable is N so the argument list was ``sv_main(N=1000)``.

The lorenz function takes 2 arguments: 

- **N**, to set the number of vertices. 
- **verts**, a list-variable to store the vertices generated by the algorithm.

In this example the ``verts`` variable is also what will be sent to the output socket, because it says so in ``out_sockets``. Notice that the lorenz function doesn't return the verts variable. All the lorenz function does is fill that list with values. Just to be clear about this example. At the time ``sv_main`` ends, the content of ``verts`` is full, but before ``lorenz()`` is called, ``verts`` is an empty list.

Here is the same lorenz attractor with more parameters exposed, see can you load it? 
https://github.com/nortikin/sverchok/blob/master/node_scripts/templates/zeffii/LorenzAttractor2.py

Lastly
------

If none of this makes sense, spend time learning about Python and dig through the ``node_scripts/templates`` directory. 

