# This script must be assigned to a python controller
# where it can access the object that owns it and the sensors/actuators that it connects to.

import bge

# support for Vector(), Matrix() types and advanced functions like Matrix.Scale(...) and Matrix.Rotation(...)
# import mathutils

# for functions like getWindowWidth(), getWindowHeight()
# import Rasterizer


def main():
    cont = bge.logic.getCurrentController()

    # The KX_GameObject that owns this controller.
    own = cont.owner

    # for scripts that deal with spacial logic
    own_pos = own.worldPosition

    # Some example functions, remove to write your own script.
    # check for a positive sensor, will run on any object without errors.
    print("Logic info for KX_GameObject", own.name)
    input = False

    for sens in cont.sensors:
        # The sensor can be on another object, we may want to use it
        own_sens = sens.owner
        print("    sensor:", sens.name, end=" ")
        if sens.positive:
            print("(true)")
            input = True
        else:
            print("(false)")

    for actu in cont.actuators:
        # The actuator can be on another object, we may want to use it
        own_actu = actu.owner
        print("    actuator:", actu.name)

        # This runs the actuator or turns it off
        # note that actuators will continue to run unless explicitly turned off.
        if input:
            cont.activate(actu)
        else:
            cont.deactivate(actu)

    # Its also good practice to get sensors and actuators by name
    # rather then index so any changes to their order wont break the script.

    # sens_key = cont.sensors["key_sensor"]
    # actu_motion = cont.actuators["motion"]

    # Loop through all other objects in the scene
    sce = bge.logic.getCurrentScene()
    print("Scene Objects:", sce.name)
    for ob in sce.objects:
        print("   ", ob.name, ob.worldPosition)

    # Example where collision objects are checked for their properties
    # adding to our objects "life" property
    """
    actu_collide = cont.sensors["collision_sens"]
    for ob in actu_collide.objectHitList:
        # Check to see the object has this property
        if "life" in ob:
            own["life"] += ob["life"]
            ob["life"] = 0
    print(own["life"])
    """

main()
