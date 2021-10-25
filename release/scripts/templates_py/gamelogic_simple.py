import bge


def main():

    cont = bge.logic.getCurrentController()
    own = cont.owner

    sens = cont.sensors['mySensor']
    actu = cont.actuators['myActuator']

    if sens.positive:
        cont.activate(actu)
    else:
        cont.deactivate(actu)

main()
