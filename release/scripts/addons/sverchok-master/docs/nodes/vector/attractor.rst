Attraction Vectors
==================

Functionality
-------------

This node calculates vectors directed from input vertices to specified attractor. Vector lengths are calculated by one of physics-like falloff laws (like 1/R^2), so it looks like attractor attracts vertices, similar to gravity force, for example.
Output vectors can be used to move vertices along them, for example.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Center**. Center of used attractor. Exact meaning depends on selected attractor type.
- **Direction**. Direction of used attractor. Exact meaning depends on selected attractor type. Not available if attractor type is **Point**.
- **Amplitude**. Coefficient of attractor power. Zero means that all output vectors will be zero. If many values are provided, each value will be matched to one vertex.
- **Coefficient**. Scale coefficient for falloff law. Exact meaning depends on selected falloff type. Available only for falloff types **Inverse exponent** and **Gauss**. If many values are provided, each value will be matched to one vertex.

Parameters
----------

This node has the following parameters:

- **Attractor type**. Selects form of used attractor. Available values are:
  - **Point**. Default value. In simple case, attractor is just one point specified in **Center** input. Several points can be passed in that input; in this case, attracting force for each vertex will be calculated as average of attracting forces towards each attractor point.
  - **Line**. Attractor is a straight line, defined by a point belonging to this line (**Center** input) and directing vector (**Direction** input).
  - **Plane**. Attractor is a plane, defined by a point belonging to this line (**Center** input) and normal vector (**Direction** input).
- **Falloff type**. Used falloff law. Avalable values are:
  - **Inverse**. Falloff law is 1/R, where R is distance from vertex to attractor.
  - **Inverse square**. Falloff law is 1/R^2. This law is most common in physics (gravity and electromagnetizm), so this is the default value.
  - **Inverse cubic**. Falloff law is 1/R^2.
  - **Inverse exponent**. Falloff law is `exp(- C * R)`, where R is distance from vertex to attractor, and C is value from **Coefficient** input.
  - **Gauss**. Falloff law is `exp(- C * R^2 / 2)`, where R is distance from vertex to attractor, and C is value from **Coefficient** input.
- **Clamp**. Whether to restrict output vector length with distance from vertex to attractor. If not checked, then attraction vector length can be very big for vertices close to attractor, depending on selected falloff type. Default value is True.

Outputs
-------

This node has the following outputs:

- **Vectors**. Calculated attraction force vectors. 
- **Directions**. Unit vectors in the same directions as attracting force.
- **Coeffs**. Lengths of calculated attraction force vectors.

Examples of usage
-----------------

Most obvious case, just a plane attracted by single point:

.. image:: https://cloud.githubusercontent.com/assets/284644/23908413/28c3d12a-08fe-11e7-9995-58fef78910b3.png

Plane attracted by single point, with Clamp unchecked:

.. image:: https://cloud.githubusercontent.com/assets/284644/24082405/3b84b186-0cef-11e7-94d6-c58d4fcd62e9.png

Not so obvious, plane attracted by circle (red points):

.. image:: https://cloud.githubusercontent.com/assets/284644/23908410/283bd220-08fe-11e7-96e3-7b895a801e8e.png

Coefficients can be used without directions:

.. image:: https://cloud.githubusercontent.com/assets/284644/23908414/28d14b02-08fe-11e7-8bb5-585a6226c4f1.png

Torus attracted by a line along X axis:

.. image:: https://cloud.githubusercontent.com/assets/284644/23908411/28ab67ac-08fe-11e7-8659-5ebb90771864.png

Sphere attracted by a plane:

.. image:: https://cloud.githubusercontent.com/assets/284644/23908415/28e0d950-08fe-11e7-8695-3aa3bd249710.png

