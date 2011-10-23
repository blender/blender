# This script defines functions to be used directly in drivers expressions to
# extend the builtin set of python functions.
#
# This can be executed on manually or set to 'Register' to
# initialize thefunctions on file load.


# two sample functions
def invert(f):
    """ Simple function call:

            invert(val)
    """
    return 1.0 - f


uuid_store = {}


def slow_value(value, fac, uuid):
    """ Delay the value by a factor, use a unique string to allow
        use in multiple drivers without conflict:

            slow_value(val, 0.5, "my_value")
    """
    value_prev = uuid_store.get(uuid, value)
    uuid_store[uuid] = value_new = (value_prev * fac) + (value * (1.0 - fac))
    return value_new


import bpy

# Add variable defined in this script into the drivers namespace.
bpy.app.driver_namespace["invert"] = invert
bpy.app.driver_namespace["slow_value"] = slow_value
