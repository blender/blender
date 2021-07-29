Get Property / Set Property
===========================

Functionality
-------------

These nodes can be used to control almost anything in Blender once you know the python path. For instance if you want to ``Set`` the location of a scene object called ``Cube``, the python path is ```bpy.data.objects['Cube'].location```.

By default these nodes don't expose their input / output sockets, but they will appear if the node was able to find a compatible property in the path you provided. The Socket types that the node generates depend on the kind of data path you gave it. It knows about Matrices and Vectors and Numbers etc..

There are also convenience aliases. Instead of writing ```bpy.data.objects['Cube'].location``` you can write ```objs['Cube'].location``` . The aliases are as follows::

    aliases = {
        "c": "bpy.context",
        "C" : "bpy.context",
        "scene": "bpy.context.scene",
        "data": "bpy.data",
        "D": "bpy.data",
        "objs": "bpy.data.objects",
        "mats": "bpy.data.materials",
        "meshes": "bpy.data.meshes",
        "texts": "bpy.data.texts"
    }  


Input
-----

In ``Set`` mode

+-----------------+--------------------------------------------------------------------------+
| Input           | Description                                                              |
+=================+==========================================================================+
| Dynamic         | Any of the Sverchok socket types that make sense                         | 
+-----------------+--------------------------------------------------------------------------+

Output
------

In ``Get`` mode

+-----------------+--------------------------------------------------------------------------+
| Output          | Description                                                              |
+=================+==========================================================================+
| Dynamic         | Any of the Sverchok socket types that make sense                         | 
+-----------------+--------------------------------------------------------------------------+



Parameters
----------

The only parameter is the python path to the property you want to set or get. Usually we search this manually using Blender's Python console.


Limitations
-----------

(todo?)



Examples
--------


.. image:: https://cloud.githubusercontent.com/assets/619340/11468741/2a1aa3c4-9752-11e5-85d9-13cd8478c0d2.png

.. image:: https://cloud.githubusercontent.com/assets/619340/11468834/a9eaa342-9752-11e5-8ea9-76a0b678c2a4.png

Using aliases ``objs`` instead of ``bpy.data.objects``

.. image:: https://cloud.githubusercontent.com/assets/619340/11468901/1af0f73a-9753-11e5-8fb3-55b8975178bb.png
