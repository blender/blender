**********
Contribute
**********

Our workflow:
=============

1. You have the freedom to code how you like, but follow pep8 and use the style of existing nodes. Pep8 using PyLint if your editor has it, search our codebase for "pylint ignore", because there are rules we routinely and consciously ignore. But.. the vast majority of pep8 is solid gold, and part of the path for us to accept your code.

2. Agile software development - look http://en.wikipedia.org/wiki/Agile_software_development here.

3. Implementing ideas from other add-ons is fine, if you have the appropriate licenses (GPL), written consent or rights. We don't accept unattributed code you didn't write.

4. Before you spend a lot of time making something, we recommend you tell us about it beforehand in the Issue Tracker. Sometimes the thing you want to make already exists, or we have plans for it already with placeholder code.

5. Brainstorm and discuss solutions. We use the issue tracker a lot and will give our honest opinion and interpretations of new code. We are artists and technical people trying to solve complicated and interesting issues using code and Blender. We care about optimal code but also accept working and novel solutions.

6. Test your code, Test your proposed changes. Write automated tests for your code whenever it makes sense. Please refer to `this document <testing.html>`_ for details.


What not to do:
===============

Doing these things will break old layouts or have other unintended consequences.

1. Change ``bl_idname`` of a node

2. Remove or rename sockets

3. Adding new socket inbetween existing sockets without updating upgrade.py. We prefer that you add sockets behind the last existing socket for either ``self.inputs`` or ``self.outputs``.

4. the node property ``current_mode`` should be considered a reserved keyword, don't use it.

5. There are other reserved property names see the bpy.types.Node baseclass in Blender docs and Sverchok's custom node mixin class.


Helpful hints
=============

In blender console you can easily inspect the sverchok system by writing:

.. code-block :: python

    import sverchok
    print(dir(sverchok.nodes))


To create a node:
=================

1. Make a scripted node to test the idea.

2. Show your node to us in an issue or create branch or fork of master in github. If it
   is a huge complex job we can make you collaborator. 

3. Copy an existing node that is similar, as a basis to start from.

4. Change class name, class bl_idname, docstring, and class registration section in your file (at the bottom)

5. Add node's ``bl_idname`` to menu.py in an appropriate category.

6. Add file to matching category: ``..sverchok/nodes/whatever_existing_category/your_file.py``

7. Make a pull request.


SOME RULES:
===========

1. All classes that are subclasses of blender classes - have to have prefix Sv, ie SvColors.

2. node_tree.py contains base classes of node tree and, maybe you need to create collection property or new socket (for
   new type of socket tell us first), you make it here.

3. data_structure.py has magic of:

    a. update definitions, that works pretty well with sockets' and nodes' updates
    b. some bmesh functiones
    c. cache -- operates with handles -- we use it to operate with nodes' data, it is dictionary,
       with node'name, node-tree'name and it's data
    d. list levels definitions - you must yse them:
        1. data correct makes list from chaotic to 4th levels list, seems like [[[floats/integers]]]
           and not [[f/i]] or [[[[f/i]]]].

           usage:
               dataCorrect(data, nominal_dept=2), where data - is your list, nominal_depth -
               optioal, normal for your' case depth of list

        2. data spoil - make list more nasty for value of depth

           usage:
               dataSpoil(data, dept), where data - your list, depth - level of nestynessing.
               Gives list from [] to [[]] etc

        3. Levels of list - find level of nestiness.

           usage:
               levelsOflist(list) - returns level of list nestiness [[]] - is 2, [] - 1, [[[]]] - 3

    e. matrix definitions - to make something with matrices/vertices

        1. Matrix_generate(prop) - make from simple list real mathutils.Matrix(()()()())
        2. Matrix_listing(prop) - make from mathutils.Matrix() simple list like [[][][][]] with
           container is [[[][][][]]]
        3. Matrix_location(prop, list=False) - return mathutils.Vector() of matrix' location
        4. Matrix_scale(prop, list=False) - the same, but returns matrix' scale vectors
        5. Matrix_rotation(prop, list=False) - return rotation axis and rotation angle in radians
           value as tuple (Vector((x,y,z)),angle)
        6. Vector_generate(prop) - makes from simple list real mathutils.Vector(), as Matrix generate def
        7. Vector_degenerate(prop) - as matrix listing, it degenerate Vectors to simple list
        8. Edg_pol_generate(prop) - define wether it is edges or polygons in list, and terurns tuple as
           (type,list)
        9. matrixdef(orig, loc, scale, rot, angle, vec_angle=[[]]) - defines matrix

    f. list definitions:

        1. fullList(l, count) - makes list full till some count. last item multiplied to rest of needed
           length, ie [1,2,3,4] for count 6 will be [1,2,3,4,4,4]

        2. match_short(lsts) Shortest list decides output length
           [[1,2,3,4,5], [10,11]] -> [[1,2], [10, 11]]

        3. match_cross2(lsts) cross rference
           [[1,2], [5,6,7]] ->[[1, 2, 1, 2, 1, 2], [5, 5, 6, 6, 7, 7]]

        4. match_long_repeat(lsts) repeat last of shorter list
           [[1,2,3,4,5] ,[10,11]] -> [[1,2,3,4,5] ,[10,11,10,11,10]]

        5. match_long_cycle(lsts) cycle shorts lists
           [[1,2,3,4,5] ,[10,11]] -> [[1,2,3,4,5] ,[10,11,10,11,10]]

        6. repeat_last(lst) creates an infinite iterator that repeats last item of list,
           for cycle see itertools.cycle

        7. some others to operate with exact nodes

    g. update sockets - definitions to operate with update

    h. changable type of socket - makes possible to use changable socket in your node - it calling

       usage:
            1. node has to have self veriables:
            2. and in update:
                * inputsocketname = 'data' # 'data' - name of your input socket, that defines type
                * outputsocketname = ['dataTrue','dataFalse'] # 'data...' - are names of your
                  sockets to be changed
                * changable_sockets(self, inputsocketname, outputsocketname)

    i. multi-socket multi_socket(node,min=1) - as used by ListJoin, List Zip, Connect UV

        * multi_socket(node,min=1)

        * base_name = 'data'

        * multi_socket_type = 'StringsSocket'

        * setup the fixed number of socket you need, the last of them is the first multi socket.
          minimum of one.

        * then in update(self):

            - multi_socket(self, min=1, start=0, breck=False)  - [where min - minimum count of
              input sockets;

            - start - starting of numeration, could be -1, -2 to start as in formula2 node; breck -
              to make breckets, as used in formula2 node]

        * for more details see files mentioned above

4. **Utils** folder has:

       a. CADmodule - to provide lines intersection

       b. IndexViewerDraw - to provide OpenGL drawing of INDXview node in basics

       c. sv_bmeshutils - self say name

       d. sv_tools - it is toolbox in node area for update button, upgrade button and for layers
          visibility buttons, also update node and upgrade functional to automate this process.

       e. text_editor_plugins - for sandbox node scripted node (SN) to implement Ctrl+I auto complete function

       f. text_editor_submenu - templates of SN

       g. upgrade - to avoid breaking old layouts. Defines new simplified interface override. if you change some property in def draw_buttons()
          than just bring new properties here to avoid break old layout

       h. viewer_draw - for draw and handle OpenGL of Viewer Draw node (have to be remaked)

       i. voronoi - for delaunai and voronoi functions of correspond nodes

5. **Node scripts** folder for every template for SN (see utils-e.)

6. **Nodes** folder for categorized nodes. not forget to write your nodes to init.py there

7. To use enumerate property you have to assign index to items, never change the index of items added,
     it will break if you add more functions.

8. Not make many nodes if you can do less multifunctional.

9. Use levels, findout how it works and use level IntProperty in draw to define what level is to operate.
   We operate with 1,2,3 - standart and additional 4... infinity. make sure, that your levels limited,
   dropped down by levelsOflist as maximum value

10. Keep order in node' update definition as if output: if input. To count input only if you have output socket
    assembled.

11. Look at todo list to know what is happening on and what you can do.
    use your nodes and test them.

12. There is no reason to auto wrap or make less levels of wrapping, than needed to proceed in other nodes.
    So, for now canonical will be [[0,1,2,3]] for simple data and [[[0,1,2,3]]] for real data as edge,
    vertex, matrix other cases may be more nasty, but not less nesty and wrapping need to be grounded on
    some reasons to be provided.

13. Do not use is_linked to test if socket is linked in ``def update(self)``, check links. In ``def process(self)``
    use ``.is_linked`` 

14. to **CHANGE** some node, please, follow next:

    a. Put old node file to ../old_nodes add the corresponding bl_idname in __init__.py in the table. (there is README file also);
    b. Make new changed node as mk2(3,4...n) and place to where old node was placed with all changes as new node, change name and bl_idname (look 'To create a node:' in current instructions).
