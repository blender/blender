"""
Overriding the context can be used to temporarily activate another ``window`` / ``area`` & ``region``,
as well as other members such as the ``active_object`` or ``bone``.

Notes:

- When overriding window, area and regions: the arguments must be consistent,
  so any region argument that's passed in must be contained by the current area or the area passed in.
  The same goes for the area needing to be contained in the current window.

- Temporary context overrides may be nested, when this is done, members will be added to the existing overrides.

- Context members are restored outside the scope of the context-manager.
  The only exception to this is when the data is no longer available.

  In the event windowing data was removed (for example), the state of the context is left as-is.
  While this isn't likely to happen, explicit window operation such as closing windows or loading a new file
  remove the windowing data that was set before the temporary context was created.
"""
