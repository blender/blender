KX_NavMeshObject(KX_GameObject)
===============================

.. module:: bge.types

base class --- :class:`KX_GameObject`

.. class:: KX_NavMeshObject(KX_GameObject)

   Python interface for using and controlling navigation meshes. 

   .. method:: findPath(start, goal)

      Finds the path from start to goal points.

      :arg start: the start point
      :arg start: 3D Vector
      :arg goal: the goal point
      :arg start: 3D Vector
      :return: a path as a list of points
      :rtype: list of points

   .. method:: raycast(start, goal)

      Raycast from start to goal points.

      :arg start: the start point
      :arg start: 3D Vector
      :arg goal: the goal point
      :arg start: 3D Vector
      :return: the hit factor
      :rtype: float

   .. method:: draw(mode)

      Draws a debug mesh for the navigation mesh.

      :arg mode: the drawing mode (one of :ref:`these constants <navmesh-draw-mode>`)
      :arg mode: integer
      :return: None

   .. method:: rebuild()

      Rebuild the navigation mesh.

      :return: None

