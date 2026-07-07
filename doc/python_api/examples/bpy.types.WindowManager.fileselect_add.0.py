"""
This method is used from the operators ``invoke`` callback
which must then return ``{'RUNNING_MODAL'}``.

Accepting the file selector will run the operators ``execute`` callback.

The following properties are supported:

``filepath``: ``bpy.props.StringProperty(subtype='FILE_PATH')``
   Represents the absolute path to the file.
``dirpath``: ``bpy.props.StringProperty(subtype='DIR_PATH')``
   Represents the absolute path to the directory.
``filename``: ``bpy.props.StringProperty(subtype='FILE_NAME')``
   Represents the filename without the leading directory.
``files``: ``bpy.props.CollectionProperty(type=bpy.types.OperatorFileListElement)``
   When present in the operator this collection includes all selected files.
``filter_glob``: ``bpy.props.StringProperty(default="*.ext")``
   When present in the operator and it's not empty,
   it will be used as a file filter (example value: ``*.zip;*.py;*.exe``).
``check_existing``: ``bpy.props.BoolProperty()``
   If this property is present and set to ``True``,
   the operator will warn if the provided file-path already exists
   by highlighting the filename input field in red.


.. warning::

   After opening the file-browser the user may continue to use Blender,
   this means it is possible for the user to change the context in ways
   that would cause the operators ``poll`` function to fail.

   Unless the operator reads all necessary data from the context before the file-selector is opened,
   it is recommended for operators to check the ``poll`` function from ``execute``
   to ensure the context is still valid.

   Example from the body of an operators ``execute`` function:

   .. code-block:: python

      if self.options.is_invoke:
          # The context may have changed since invoking the file selector.
          if not self.poll(context):
              self.report({'ERROR'}, "Invalid context")
              return {'CANCELLED'}
"""
