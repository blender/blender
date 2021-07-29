Mesh Join
=========

Functionality
-------------

Analogue to ``Ctrl+J`` in the 3dview of Blender. Separate nested lists of *vertices* and *polygons/edges* are merged. The keys in the Edge and Polygon lists are incremented to coincide with the newly created vertex list.

The inner workings go something like::

    vertices_obj_1 = [
        (0.2, 1.5, 0.1), (1.2, 0.5, 0.1), (1.2, 1.5, 0.1),
        (0.2, 2.5, 5.1), (0.2, 0.5, 2.1), (0.2, 2.5, 0.1)]

    vertices_obj_2 = [
        (0.2, 1.4, 0.1), (1.2, 0.2, 0.3), (1.2, 4.5, 4.1),
        (0.2, 1.5, 3.4), (5.2, 6.5, 2.1), (0.2, 5.5, 2.1)]

    key_list_1 = [[0,1,2],[3,4,5]]
    key_list_2 = [[0,1,2],[3,4,5]]

    verts_nested = [vertices_obj_1, vertices_obj_2]
    keys_nested = [key_list_1, key_list_2]

    def mesh_join(verts_nested, keys_nested):

        mega_vertex_list = []
        mega_key_list = []

        def adjust_indices(klist, offset):
            return [[i+offset for i in keys] for keys in klist]
            # for every key in klist, add offset
            # return result

        for vert_list, key_list in zip(verts_nested, keys_nested):
            adjusted_key_list = adjust_indices(key_list, len(mega_vertex_list))
            mega_vertex_list.extend(vert_list)
            mega_key_list.extend(adjusted_key_list)

        return mega_vertex_list, mega_key_list

    print(mesh_join(verts_nested, keys_nested))

    # result
    [(0.2, 1.5, 0.1), (1.2, 0.5, 0.1), (1.2, 1.5, 0.1), 
    (0.2, 2.5, 5.1), (0.2, 0.5, 2.1), (0.2, 2.5, 0.1), 
    (0.2, 1.4, 0.1), (1.2, 0.2, 0.3), (1.2, 4.5, 4.1), 
    (0.2, 1.5, 3.4), (5.2, 6.5, 2.1), (0.2, 5.5, 2.1)] 

    [[0, 1, 2], [3, 4, 5], [6, 7, 8], [9, 10, 11]]




Inputs & Outputs
----------------

The inputs and outputs are *vertices* and *polygons / edges*.

Expects a nested collection of vertex lists. Each nested list represents an object which can itself have many vertices and key lists.


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4186165/8ea02bde-375e-11e4-96d8-175959c26505.PNG
  :alt: MeshJoinDemo1.PNG

Notes
-----