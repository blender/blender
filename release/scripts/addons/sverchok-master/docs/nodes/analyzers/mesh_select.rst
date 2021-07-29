Select mesh elements by location
================================

Functionality
-------------

This node allows to select mesh elements (vertices, edges and faces) by their geometrical location, by one of supported criteria.

You can combine different criteria by applying several instances of this node and combining masks with Logic node.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Faces**
- **Direction**. Direction vector. Used in modes: **By side**, **By normal**, **By plane**, **By cylinder**. Exact meaning depends on selected mode.
- **Center**. Center or base point. Used in modes: **By sphere**, **By plane**, **By cylinder**, **By bounding box**.
- **Percent**. How many vertices to select. Used in modes: **By side**, **By normal**.
- **Radius**. Allowed distance from center, or line, or plane, to selected vertices. Used in modes: **By sphere**, **By plane**, **By cylinder**, **By bounding box**.

Parameters
----------

This node has the following parameters:

- **Mode**. Criteria type to apply. Supported criterias are:

  * **By side**. Selects vertices that are located at one side of mesh. The side is specified by **Direction** input. So you can select "rightmost" vertices by passing (0, 0, 1) as Direction. Number of vertices to select is controlled by **Percent** input: 1% means select only "most rightmost" vertices, 99% means select "all but most leftmost". More exactly, this mode selects vertex V if `(Direction, V) >= max - Percent * (max - min)`, where `max` and `min` are maximum and minimum values of that scalar product amongst all vertices.
  * **By normal direction**. Selects faces, that have normal vectors pointing in specified **Direction**. So you can select "faces looking to right". Number of faces to select is controlled by **Percent** input, similar to **By side** mode. More exactly, this mode selects face F if `(Direction, Normal(F)) >= max - Percent * (max - min)`, where `max` and `min` are maximum and minimum values of that scalar product amongst all vertices.
  * **By center and radius**. Selects vertices, which are within **Radius** from specified **Center**; in other words, it selects vertices that are located inside given sphere. More exactly, this mode selects vertex V if `Distance(V, Center) <= Radius`. This mode also supports passing many points to **Center** input; in this case, "Distance" is distance from vertex to the nearest "Center".
  * **By plane**. Selects vertices, which are within **Radius** from specified plane. Plane is specified by providing normal vector (**Direction** input) and a point, belonging to that plane (**Center** input). For example, if you specify Direction = (0, 0, 1) and Center = (0, 0, 0), the plane will by OXY. More exactly, this mode selects vertex V if `Distance(V, Plane) <= Radius`.
  * **By cylinder**. Selects vertices, which are within **Radius** from specified straight line. Line is specified by providing directing vector (**Direction** input) and a point, belonging to that line (**Center** input). For example, if you specify Direction = (0, 0, 1) and Center = (0, 0, 0), the line will by Z axis. More exactly, this mode selects vertex V if `Distance(V, Line) <= Radius`.
  * **By edge direction**. Selects edges, which are nearly parallel to specified **Direction** vector. Note that this mode considers edges as non-directed; as a result, you can change sign of all coordinates of **Direction** and it will not affect output. More exactly, this mode selects edge E if `Abs(Cos(Angle(E, Direction))) >= max - Percent * (max - min)`, where max and min are maximum and minimum values of that cosine.
  * **Normal pointing outside**. Selects faces, that have normal vectors pointing outside from specified **Center**. So you can select "faces looking outside". Number of faces to select is controlled by **Percent** input. More exactly, this mode selects face F if `Angle(Center(F) - Center, Normal(F)) >= max - Percent * (max - min)`, where max and min are maximum and minimum values of that angle.
  * **By bounding box**. Selects vertices, that are within bounding box defined by points passed into **Center** input. **Radius** is interpreted as tolerance limit. For examples:

    - If one point `(0, 0, 0)` is passed, and Radius = 1, then the node will select all vertices that have `-1 <= X <= 1`, `-1 <= Y <= 1`, `-1 <= Z <= 1`.
    - If points `(0, 0, 0)`, `(1, 2, 3)` are passed, and Radius = 0.5, then the node will select all vertices that have `-0.5 <= X <= 1.5`, `-0.5 <= Y <= 2.5`, `-0.5 <= Z <= 3.5`.
- **Include partial selection**. Not available in **By normal** mode. All other modes select vertices first. This parameter controls either we need to select edges and faces that have **any** of vertices selected (Include partial = True), or only edges and faces that have **all** vertices selected (Include partial = False).

Outputs
-------

This node has the following outputs:

- **VerticesMask**. Mask for selected vertices.
- **EdgesMask**. Mask for selected edges. Please note that this mask relates to list of vertices provided at node input, not list of vertices selected by this node.
- **FacesMask**. Mask for selected faces. Please note that this mask relates to list of vertices provided at node input, not list of vertices selected by this node.


Examples of usage
-----------------

Select rightmost vertices:

.. image:: https://cloud.githubusercontent.com/assets/284644/23761326/aa0cacf6-051c-11e7-8dae-1848bc0e81cd.png

Select faces looking to right:

.. image:: https://cloud.githubusercontent.com/assets/284644/23761372/cc0950b6-051c-11e7-9c57-4b76a91c2e5d.png

Select vertices within sphere:

.. image:: https://cloud.githubusercontent.com/assets/284644/23761537/5106db9e-051d-11e7-81e8-2fca30c02b18.png

Using multiple centers:

.. image:: https://cloud.githubusercontent.com/assets/284644/24580675/b5206da8-172d-11e7-9aa3-2c345712c899.png

Select vertices near OYZ plane:

.. image:: https://cloud.githubusercontent.com/assets/284644/23756618/7036cf88-050e-11e7-9619-b0d748d03d20.png

Select vertices near vertical line:

.. image:: https://cloud.githubusercontent.com/assets/284644/23756638/81324d3a-050e-11e7-89c2-e2016557aa47.png

Bevel only edges that are parallel to Z axis:

.. image:: https://cloud.githubusercontent.com/assets/284644/23831501/fcebffee-074c-11e7-8e15-de759d67588c.png

Select faces that are looking outside:

.. image:: https://cloud.githubusercontent.com/assets/284644/23831280/62e48816-0748-11e7-887f-b9223dbbf939.png

Select faces by bounding box:

.. image:: https://cloud.githubusercontent.com/assets/284644/24332028/248a1026-1261-11e7-8886-f7a0f88ecb60.png

