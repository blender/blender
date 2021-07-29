def formatVector(vector):
    return "V({:>7.3f}, {:>7.3f}, {:>7.3f})".format(*vector)

def formatEuler(euler):
    return "E({:>7.3f}, {:>7.3f}, {:>7.3f})".format(*euler)

def formatQuaternion(quaternion):
    return "Q({:>7.3f}, {:>7.3f}, {:>7.3f}, {:>7.3f})".format(*quaternion)

def formatFloat(number):
    return "{:>8.3f}".format(number)
