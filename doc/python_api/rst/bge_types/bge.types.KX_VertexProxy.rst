KX_VertexProxy(SCA_IObject)
===========================

.. module:: bge.types

base class --- :class:`SCA_IObject`

.. class:: KX_VertexProxy(SCA_IObject)

   A vertex holds position, UV, color and normal information.

   Note:
   The physics simulation is NOT currently updated - physics will not respond
   to changes in the vertex position.

   .. attribute:: XYZ

      The position of the vertex.

      :type: Vector((x, y, z))

   .. attribute:: UV

      The texture coordinates of the vertex.

      :type: Vector((u, v))

   .. attribute:: normal

      The normal of the vertex.

      :type: Vector((nx, ny, nz))

   .. attribute:: color

      The color of the vertex.

      :type: Vector((r, g, b, a))

      Black = [0.0, 0.0, 0.0, 1.0], White = [1.0, 1.0, 1.0, 1.0]

   .. attribute:: x

      The x coordinate of the vertex.

      :type: float

   .. attribute:: y

      The y coordinate of the vertex.

      :type: float

   .. attribute:: z

      The z coordinate of the vertex.

      :type: float

   .. attribute:: u

      The u texture coordinate of the vertex.

      :type: float

   .. attribute:: v

      The v texture coordinate of the vertex.

      :type: float

   .. attribute:: u2

      The second u texture coordinate of the vertex.

      :type: float

   .. attribute:: v2

      The second v texture coordinate of the vertex.

      :type: float

   .. attribute:: r

      The red component of the vertex color. 0.0 <= r <= 1.0.

      :type: float

   .. attribute:: g

      The green component of the vertex color. 0.0 <= g <= 1.0.

      :type: float

   .. attribute:: b

      The blue component of the vertex color. 0.0 <= b <= 1.0.

      :type: float

   .. attribute:: a

      The alpha component of the vertex color. 0.0 <= a <= 1.0.

      :type: float

   .. method:: getXYZ()

      Gets the position of this vertex.

      :return: this vertexes position in local coordinates.
      :rtype: Vector((x, y, z))

   .. method:: setXYZ(pos)

      Sets the position of this vertex.

      :type:  Vector((x, y, z))

      :arg pos: the new position for this vertex in local coordinates.

   .. method:: getUV()

      Gets the UV (texture) coordinates of this vertex.

      :return: this vertexes UV (texture) coordinates.
      :rtype: Vector((u, v))

   .. method:: setUV(uv)

      Sets the UV (texture) coordinates of this vertex.

      :type:  Vector((u, v))

   .. method:: getUV2()

      Gets the 2nd UV (texture) coordinates of this vertex.

      :return: this vertexes UV (texture) coordinates.
      :rtype: Vector((u, v))

   .. method:: setUV2(uv, unit)

      Sets the 2nd UV (texture) coordinates of this vertex.

      :type:  Vector((u, v))

      :arg unit: optional argument, FLAT==1, SECOND_UV==2, defaults to SECOND_UV
      :arg unit:  integer

   .. method:: getRGBA()

      Gets the color of this vertex.

      The color is represented as four bytes packed into an integer value.  The color is
      packed as RGBA.

      Since Python offers no way to get each byte without shifting, you must use the struct module to
      access color in an machine independent way.

      Because of this, it is suggested you use the r, g, b and a attributes or the color attribute instead.

      .. code-block:: python

         import struct;
         col = struct.unpack('4B', struct.pack('I', v.getRGBA()))
         # col = (r, g, b, a)
         # black = (  0, 0, 0, 255)
         # white = (255, 255, 255, 255)

      :return: packed color. 4 byte integer with one byte per color channel in RGBA format.
      :rtype: integer

   .. method:: setRGBA(col)

      Sets the color of this vertex.

      See getRGBA() for the format of col, and its relevant problems.  Use the r, g, b and a attributes
      or the color attribute instead.

      setRGBA() also accepts a four component list as argument col.  The list represents the color as [r, g, b, a]
      with black = [0.0, 0.0, 0.0, 1.0] and white = [1.0, 1.0, 1.0, 1.0]

      .. code-block:: python

         v.setRGBA(0xff0000ff) # Red
         v.setRGBA(0xff00ff00) # Green on little endian, transparent purple on big endian
         v.setRGBA([1.0, 0.0, 0.0, 1.0]) # Red
         v.setRGBA([0.0, 1.0, 0.0, 1.0]) # Green on all platforms.

      :arg col: the new color of this vertex in packed RGBA format.
      :type col: integer or list [r, g, b, a]

   .. method:: getNormal()

      Gets the normal vector of this vertex.

      :return: normalized normal vector.
      :rtype: Vector((nx, ny, nz))

   .. method:: setNormal(normal)

      Sets the normal vector of this vertex.

      :type:  sequence of floats [r, g, b]

      :arg normal: the new normal of this vertex.

