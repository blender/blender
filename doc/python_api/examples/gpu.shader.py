"""
Built-in shaders
++++++++++++++++

All built-in shaders have the ``mat4 ModelViewProjectionMatrix`` uniform.
The value of it can only be modified using the :class:`gpu.matrix` module.

2D_UNIFORM_COLOR:
   attributes: vec3 pos
   uniforms: vec4 color

2D_FLAT_COLOR:
   attributes: vec3 pos, vec4 color
   uniforms: -

2D_SMOOTH_COLOR:
   attributes: vec3 pos, vec4 color
   uniforms: -

2D_IMAGE:
   attributes: vec3 pos, vec2 texCoord
   uniforms: sampler2D image

3D_UNIFORM_COLOR:
   attributes: vec3 pos
   uniforms: vec4 color

3D_FLAT_COLOR:
   attributes: vec3 pos, vec4 color
   uniforms: -

3D_SMOOTH_COLOR:
   attributes: vec3 pos, vec4 color
   uniforms: -

"""
