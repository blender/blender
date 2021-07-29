Iterate
=======


Functionality
-------------

This node iteratively applies affine transformation (specified by matrix) to
input vertices, edges and polygons. So, given matrix ``M`` and vertex ``V``, it
will produce vertices ``V``, ``M*V``, ``M*M*V``, ``M*M*M*V`` and so on. 

If several matrices are presented on input, then on each iteration this node
will apply *all* these matrices to input vertices. So, if 1 set of vertices and
N matrices are passed, then on first iteration it will produce N sets of
vertices, on second iteration - N*N more, and so on.

**Note 1**. Source set of vertices (edges, and faces) is always passed to
output as-is. With minimal number of iterations, which is zero, this node will
just copy input to output.

**Note 2**. Due to recursive nature of this node, with bigger iterations number
and a several input matrices it can produce *a lot of* data. For example, if
you pass 100 vertices, 10 matrices and specify number of iterations = 4, then
it will produce 100 + 10*100 + 10*10*100 + 10*10*10*100 + 10*10*10*10*100 =
1111100 vertices.

**Note 3**. This node always produce one mesh. To split it to parts, use
Separate Loose Parts node.

Inputs
------

This node has the following inputs:

- **Matrix**
- **Verices**
- **Edges**. Must be either empty (or not connected) or presenting number of edges sets,
  which is equal to number of vertices sets in ``Vertices`` input.
- **Polygons**. Must be either empty (or not connected) or presenting number of polygons sets,
  which is equal to number of vertices sets in ``Vertices`` input.
- **Iterations**. Can be used to pass value of Iterations parameter. If series
  of values is passed, then first value will be used for first set of vertices,
  second for second set of vertices, and so on.

Parameters
----------

This node has one parameter: **Iterations**. This parameter can also be defined
via corresponding input slot. This parameter defines a number of iterations to
perform. Minimal value of zero means do not any iterations and just pass input
to output as is. 

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Polygons**
- **Matrices**. Matrices that are applied to generated copies of source mesh.

If ``Edges`` or ``Polygons`` input is not connected, then corresponding output will be empty.

Examples
--------

Circle as input, Iterations = 3; one matrix specifies scale by 0.65 (along all axis) and translation along X axis by 0.3:

.. image:: https://cloud.githubusercontent.com/assets/284644/5691511/05057312-98ec-11e4-97b9-cd24206970d5.png

One object as input, Iterations = 4; one matrix specifies scale by 0.6 along X and Y axis, and translation along Z by 1:

.. image:: https://cloud.githubusercontent.com/assets/284644/5691510/05039af6-98ec-11e4-9984-9cb85ec580bb.png

One Box as input, Iteration = 3, two matrices:

.. image:: https://cloud.githubusercontent.com/assets/284644/5691509/04d79802-98ec-11e4-9560-e6346c6ab7c8.png

Iterate cubes along with pentagons:

.. image:: https://cloud.githubusercontent.com/assets/284644/6249157/241a50c2-b7a7-11e4-9937-adb096f1d694.png

