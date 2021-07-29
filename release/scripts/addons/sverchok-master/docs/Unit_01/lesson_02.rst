**********************************
Introduction to modular components
**********************************

**prerequisites**

Same as lesson 01.


Lesson 02 - A Circle
--------------------

This lesson will introduce the following nodes: ``List Length, Int Range, List Shift, List Zip``

This will continue from the previous lesson where we made a plane from 4 vectors. We can reuse some of these nodes in order to make a Circle. If you saved it as suggested load it up, or download from **here**. You can also create it from scratch by cross referencing this image.

.. image:: https://cloud.githubusercontent.com/assets/619340/5428874/fac67fd4-83d5-11e4-9601-1399248dddd6.png

**A Circle**

Just like Blender has a Circle primitive, Sverchok also has a built in Circle primitive called the `Circle Generator`. We will avoid using the primtives until we've covered more of the fundamental nodes and how they interact.

**Dynamic Polygons**

In the collection of nodes we have in the Node View at the moment, the sequence used for linking up vertices to form a `polygon` is input manually. As mentioned earlier, as soon as you need to link many vertices instead of the current 4, you will want to make this `list creation` automatic. You will probably also want to make it dynamic to add new segments automatically if the vertex count is changeable. 

Because this is a common task, there's a dedicated node for it called ``UV Connect`` (link) , but just like the `Circle` generator nodes we will avoid using that for the same reason. Learning how to build these things yourself is the best way to learn Visual Programming with nodes.


**Generating an index list for the polygon**

In order to make the list automatically, we should know how many vertices there are at any given moment.

- ``Add -> List Main -> List Length``

The `List Length` node lets you output the length of incoming data, it also lets you pick what level of the data you want to inspect. It's worth reading the **reference** of this node for a comprehensive tour of its capabilities.

1) hook the `Vector In` output into the `Data` input of `List Length`
2) hook a new `Stethoscope` up to the output of the `List Length` node.
3) notice the `Level` slider is set to 1 by default, you should see Stethoscope shows output.

.. image:: https://cloud.githubusercontent.com/assets/619340/5436323/15ea171e-8465-11e4-8356-ec18ae8ea19d.png

Notice that, besides all the square brackets, you see the length of the incoming data is `4`, as expected. We want to generate a sequence (list) that goes something like ``[0,1,2,...n]`` where n is the index of that last vertex. In Python you might write something like this this::

  n = 4
  range(start=0, end=n, step=1)
  >> [0,1,2,...n]

To generate the index list for the polygon we need a node that outputs a sequential list of integers, Sverchok has exactly such a node and it accepts values for `start`, `step` and `count` as parameters. This is what the `Range Integer (count mode)` node does.

- ``Add -> Numbers -> Range Int``

1) Set the mode of List Range int to `Count`
2) Make sure `start` is 0 and `step` is 1
3) Hook the output of `List Length` into the `count` socket of Range Int
4) Disconnect the Formula node from the EdgPol socket
5) Connect the output of Range Int into EdgPol instead
6) Optionally you can connect a Stethoscope also to the output of Range Int in order to see the generated list for yourself

.. image:: https://cloud.githubusercontent.com/assets/619340/5436483/10651094-8467-11e4-9dc3-e4ade958531a.png

**Generating the set of circular verts**

The 4 verts we've had from the very beginning are already points on a circular path, we can make a simple change to finally see this Circle emerge.

1) Set the `mode` of the Float series node to `Range`
2) Set the `stop` parameter to 2.0
3) Set the `step` to 0.2 for example.

``2.0 / 0.2 = 10``, this means the Float Series node will now output ``[0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8]``. Notice that it does not output 2.0 at the end, because this mode excludes the terminating value. (called non inclusive)

.. image:: https://cloud.githubusercontent.com/assets/619340/5436796/a37a7092-846a-11e4-8f81-512e910b3a0b.png

You can see the beginnings of a circle.

**Forcing an even spread of Vertices**

Above we have the step set to 0.2, this manually sets the distance but calculation of this step value soon gets cumbersome. We will add nodes to do the calculation for us. Think about how you might do that.

I would want to have something like ``1 / number_vertices``, this calls for a Math node and an `Int` to represent the whole number of vertices. 

- ``Add -> Numbers -> Math``
- ``Add -> Numbers -> Int``

1) Set the Math node `mode` to ``/ (division)`` , and put 1.0 in the numerator
2) Connect the Int node into the bottom socket of the division Math node
3) Adjust the integer value on the Int node to 18 for example
4) In the image below I've connected a Stethoscope to the output of the Math Node to see the value of this computation
5) Finally, hook up the output of the division Math node into the `step` socket of Float series

You should see something like this, if not you can by now probably figure out what to do.

.. image:: https://cloud.githubusercontent.com/assets/619340/5437240/f7f80fa4-846e-11e4-8229-97a4c62c6368.png

**Notice this is starting to get crowded, let's minimize nodes**

Before going any further I would like to draw attention to the fact that you can make nodes smaller. This minimizing feature is called `hide`, we can argue about how good or badly that option is named. With Any node selected press H, to 'minimize/hide'.

.. image:: https://cloud.githubusercontent.com/assets/619340/5438258/29b11056-8477-11e4-877d-499553dcfe0c.png

In Sverchok we added special functionality to certain nodes to draw information about themselves into their header area. This allows you to see what the node is supposed to be doing even when the UI is minimized. Currently the `Int, Float, Math, Vector Math` nodes have this behaviour because they are essential nodes and used very often.

In future lessons you will often see minimized/hidden nodes

**Polygon is easy, what about Edges?**

Remember, there are nodes that can take an incoming set of vertices and generate the required Edges index lists. But we're trying to explore the modular features of Sverchok -- we'll build our own Edges generator this time.

The edge index list of the square looked like ``[[0,1],[1,2],[2,3],[3,0]]``. For the Circle of a variable number of vertices that list will look like ``[[0,1],[1,2],...,[n-1,n],[n,0]]``. Notice i'm just showing the start of the list and the end, to indicate that there might be a formula for it based purely on how many verts you want to link.

In python you might express this using a for loop or a list comprehension::

    # for loop
    n = 5
    for i in range(n):
       print(i, (i+1) % n)

    >> 0 1
    >> 1 2
    >> 2 3
    >> 3 4
    >> 4 0

    # list comprehension
    n = 5
    edges = [[i, (i+1) % n] for i in range(n)]
    print(edges)
    >> [[0, 1], [1, 2], [2, 3], [3, 4], [4, 0]]

In Sverchok the end result will be the same, but we'll arrive at the result in a different way.

The second index of each edge is one higher than the first index, except for the last edge. The last edge closes the ring of edges and meets back up with the first vertex. In essenence this is a wrap-around. Or, you can think of it as two lists, one of which is shifted by one with respect the other list.

Sverchok has a node for this called `List Shift`. We'll zip the two lists together using `List Zip` node.

- ``add -> List Struct -> List Shift``
- ``add -> List Main -> List Zip``

1) Hook the output of `List Range Int` into the first Data socket of the `List Zip` node.
2) Hook the output of `List Range Int` also into the `List Shift` node.
3) To make the wrap-around, simply set the `Shift slider` to 1.
4) connect the output of `List Shift` to the second Data input of `List Zip`.
5) Make sure the level parameter on `List Zip` is set to 1.
6) Hook up a Stethoscope to the output of `List Zip` to verify

Notice in this image I have minimized/hidden (shortcut H) a few nodes to keep the node view from getting claustrophobic. 

.. image:: https://cloud.githubusercontent.com/assets/619340/5440504/6f4ddf60-8489-11e4-81f4-ead627fe710c.png

7) Or hook up the output of `List Zip` straight into the EdgPol socket of`Viewer Draw`.

.. image:: https://cloud.githubusercontent.com/assets/619340/5440916/bee96a1e-848c-11e4-8799-060c7f458c3e.png

**End of lesson 02**

Save this .blend you’ve been working on as Sverchok_Unit_01_Lesson_02 for future tutorials or as reference if you want to look something up later if you like.

You now know how to create basic shapes programmatically using Sverchok nodes. In Lesson 03 a dynamic grid will be generated, but first relax and reiterate what has been learned so far.

**Addendum**

``Viewer Draw`` automatically generates Edges when you pass one or more Vertices and Polygons. This means in practice when you already have the Polygons for an object then you don't need to also pass in the Edges, they are inferred purely from the indices of the incoming Polygons.



