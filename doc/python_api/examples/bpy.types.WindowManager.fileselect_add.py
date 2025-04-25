"""
This method is used from the operators ``invoke`` callback
which must then return ``{'RUNNING_MODAL'}``.

Accepting the file selector will run the operators ``execute`` callback.

The following properties are supported:

``filepath``: ``bpy.props.StringProperty(subtype='FILE_PATH')``
   Represents the absolute path to the file.
``dirpath``: ``bpy.props.StringProperty(subtype='DIR_PATH')``
   Represents the absolute path to the directory.
``dirpath``: ``bpy.props.StringProperty(subtype='DIR_PATH')``
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
"""
