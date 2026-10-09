This module contains callback lists.

Handlers are functions that Blender calls when certain events occur,
such as loading a file, changing frame or rendering.

Usage
+++++

Each handler type is a regular Python list,
add a function to have it called and remove it when it's no longer needed.

Since these lists are shared by all scripts:

- Scripts should only add or remove their own handlers,
  never modify handlers added by other scripts.
- A handler may only remove itself (not other handlers) from its list while it's running.
  Removing other handlers from the list being run may cause handlers not to run as expected.
- Add-ons must ensure stale handlers aren't left when the add-on is disabled (on ``unregister``).
