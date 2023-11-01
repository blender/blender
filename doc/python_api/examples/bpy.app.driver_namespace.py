"""
File Loading & Order of Initialization
   Since drivers may be evaluated immediately after loading a blend-file it is necessary
   to ensure the driver name-space is initialized beforehand.

   This can be done by registering text data-blocks to execute on startup,
   which executes the scripts before drivers are evaluated.
   See *Text -> Register* from Blender's text editor.

   .. hint::

      You may prefer to use external files instead of Blender's text-blocks.
      This can be done using a text-block which executes an external file.

      This example runs ``driver_namespace.py`` located in the same directory as the text-blocks blend-file:

      .. code-block::

         import os
         import bpy
         blend_dir = os.path.normalize(os.path.join(__file__, "..", ".."))
         bpy.utils.execfile(os.path.join(blend_dir, "driver_namespace.py"))

      Using ``__file__`` ensures the text resolves to the expected path even when library-linked from another file.

   Other methods of populating the drivers name-space can be made to work but tend to be error prone:

   Using The ``--python`` command line argument to populate name-space often fails to achieve the desired goal
   because the initial evaluation will lookup a function that doesn't exist yet,
   marking the driver as invalid - preventing further evaluation.

   Populating the driver name-space before the blend-file loads also doesn't work
   since opening a file clears the name-space.

   It is possible to run a script via the ``--python`` command line argument, before the blend file.
   This can register a load-post handler (:mod:`bpy.app.handlers.load_post`) that initialized the name-space.
   While this works for background tasks it has the downside that opening the file from the file selector
   won't setup the name-space.
"""
