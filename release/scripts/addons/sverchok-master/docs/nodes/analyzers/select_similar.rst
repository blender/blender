Select similar
==============

Functionality
-------------

This node allows to select mesh elements (vertices, edges and faces), which are similar to elements already selected in some aspect.
This is the implementation of Blender's own "Select similar" (Shift+G) feature. So fo additional information, please refer to the Blender's documentation on this feature.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Faces**
- **Mask**. This indicates elements selected initially. This mask can be applied to vertices, edges or faces, depending on selected mode.
- **Threshold**. Similarity threshold.

Parameters
----------

This node has the following parameters:

- **Select**. This parameter defines what elements are you selecting: Vertices, Edges or Faces.
- **Select by**. Similarity criteria type to apply. Supported criteria set depends on selection mode:

  * For **Vertices** supported criteria are:

    * **Normal**. Vertices with similar normal vector.
    * **Adjacent edges**. Vertices with similar number of adjacent edges.
    * **Adjacent faces**. Vertices with similar number of adjacent faces.
  * For **Edges**, supported criteria are:

    * **Length**. Edges with similar length.
    * **Direction**. Edges with similar direction.
    * **Adjacent faces**. Edges with similar number of adjacent faces.
    * **Face Angle**. Edges which have similar angle between adjacent faces.
  * For **Faces**, supported criteria are:

    * **Area**. Faces with similar area.
    * **Sides**. Faces with similar number of sides.
    * **Perimeter**. Faces with similar perimeter.
    * **Normal**. Faces with similar normal vector.
    * **CoPlanar**. Faces nearly coplanar to selected.
- **Compare by**. Comparasion operator to use. Available values are **=**, **>=**, **<=**.
- **Threshold**. Similarity threshold. This parameter can be also provided as input.

Outputs
-------

This node has the following outputs:

- **Mask**. This indicates elements selected by the node. This mask is to be applied to vertices, edges or faces, depending on selected mode.
- **Vertices**. Selected vertices. This output is only available in **Vertices** mode.
- **Edges**. Selected edges. This output is only available in **Edges** mode.
- **Faces**. Selected faces. This output is only available in **Faces** mode.

Examples of usage
-----------------

Select faces with similar normal vector. Originally selected faces are marked with red color.

.. image:: https://cloud.githubusercontent.com/assets/284644/25073036/6cabd4da-22ff-11e7-9880-143d8af4b8c9.png

Select faces with similar area. Originally selected faces are marked with red color.

.. image:: https://cloud.githubusercontent.com/assets/284644/25073037/6ce11f50-22ff-11e7-8744-f5aefb616f23.png

Select edges with direction similar to selected edges. Originally selected edges are marked with orange color.

.. image:: https://cloud.githubusercontent.com/assets/284644/25073037/6ce11f50-22ff-11e7-8744-f5aefb616f23.png

