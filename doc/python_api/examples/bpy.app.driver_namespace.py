"""
File Loading & Order of Initialization
   Since drivers are evaluated immediately after loading a blend-file, using the ``--python`` command line argument
   to populate name-space often fails to achieve the desired goal because the initial evaluation will lookup a function
   that doesn't exist yet, marking the driver as invalid - preventing further evaluation.

   Populating the driver name-space before the blend-file loads also doesn't work
   since opening a file clears the name-space.

   The driver name-space should be populated for newly loaded files using text-block registration.

   Text blocks may be marked to execute on startup from the text editor *Text -> Register*.
   Scripts that are registered will execute after a blend-file loads & before driver evaluation.

   It's also possible to use run a script via the ``--python`` command line argument, before the blend file.
   This can register a load-post handler (:mod:`bpy.app.handlers.load_post`) that initialized the name-space.
   While this works for background tasks it has the downside that opening the file from the file selector
   won't setup the name-space.
"""
