from mathutils import Matrix, Euler

def composeMatrix(location, rotation, scale):
    # Scale
    scaleMatrix = Matrix.Identity(3)
    scaleMatrix[0][0] = scale[0]
    scaleMatrix[1][1] = scale[1]
    scaleMatrix[2][2] = scale[2]

    # Rotation
    matrix = (rotation.to_matrix() * scaleMatrix).to_4x4()

    # Translation
    matrix[0][3] = location[0]
    matrix[1][3] = location[1]
    matrix[2][3] = location[2]

    return matrix

def extractRotation(matrix):
    return rotationMatrix(matrix.to_euler())

def rotationMatrix(rotation):
    matrix = Matrix.Rotation(rotation[2], 4, 'Z')
    matrix *= Matrix.Rotation(rotation[1], 4, 'Y')
    matrix *= Matrix.Rotation(rotation[0], 4, 'X')
    return matrix

def scaleMatrix(scale):
    scaleMatrix = Matrix.Identity(4)
    scaleMatrix[0][0] = scale[0]
    scaleMatrix[1][1] = scale[1]
    scaleMatrix[2][2] = scale[2]
    return scaleMatrix

def mixEulers(a, b, factor):
    x = a.x * (1 - factor) + b.x * factor
    y = a.y * (1 - factor) + b.y * factor
    z = a.z * (1 - factor) + b.z * factor
    return Euler((x, y, z), a.order)
