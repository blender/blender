# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

from sverchok.data_structure import get_other_socket

from mathutils import Matrix, Quaternion
from sverchok.data_structure import Matrix_listing, Matrix_generate


# conversion tests, to be used in sv_get!

def cross_test_socket(self, A, B):
    """ A is origin type, B is destination type """
    other = get_other_socket(self)
    get_type = {'v': 'VerticesSocket', 'm': 'MatrixSocket', 'q': "SvQuaternionSocket"}
    return other.bl_idname == get_type[A] and self.bl_idname == get_type[B]


def is_vector_to_matrix(self):
    return cross_test_socket(self, 'v', 'm')


def is_matrix_to_vector(self):
    return cross_test_socket(self, 'm', 'v')


def is_matrix_to_quaternion(self):
    return cross_test_socket(self, 'm', 'q')


def is_quaternion_to_matrix(self):
    return cross_test_socket(self, 'q', 'm')

# ---


def get_matrices_from_locs(data):
    location_matrices = []
    collect_matrix = location_matrices.append

    def get_all(data):
        for item in data:
            if isinstance(item, (tuple, list)) and len(item) == 3 and isinstance(item[0], (float, int)):
                # generate location matrix from location
                x, y, z = item
                collect_matrix([(1., .0, .0, x), (.0, 1., .0, y), (.0, .0, 1., z), (.0, .0, .0, 1.)])
            else:
                get_all(item)

    get_all(data)
    return location_matrices


def get_matrices_from_quaternions(data):
    matrices = []
    collect_matrix = matrices.append

    def get_all(data):
        for item in data:
            if isinstance(item, (tuple, list)) and len(item) == 4 and isinstance(item[0], (float, int)):
                mat = Quaternion(item).to_matrix().to_4x4()
                collect_matrix(Matrix_listing([mat])[0])
            else:
                get_all(item)

    get_all(data)
    return matrices


def get_quaternions_from_matrices(data):
    quaternions = []
    collect_quaternion = quaternions.append

    def get_all(data):
        for sublist in data:
            if is_matrix(sublist):
                mat = Matrix(sublist)
                q = tuple(mat.to_quaternion())
                collect_quaternion(q)
            else:
                get_all(sublist)

    get_all(data)
    return [quaternions]


def is_matrix(mat):
    ''' expensive function call? '''
    if not isinstance(mat, (tuple, list)) or not len(mat) == 4:
        return

    for i in range(4):
        if isinstance(mat[i], (tuple, list)):
            if not (len(mat[i]) == 4 and all([isinstance(j, (float, int)) for j in mat[i]])):
                return
        else:
            return
    return True


def get_locs_from_matrices(data):
    locations = []
    collect_vector = locations.append

    def get_all(data):

        for sublist in data:
            if is_matrix(sublist):
                collect_vector((sublist[0][3], sublist[1][3], sublist[2][3]))
            else:
                get_all(sublist)

    get_all(data)
    return locations

class ImplicitConversionProhibited(Exception):
    def __init__(self, socket):
        super().__init__()
        self.socket = socket
        self.node = socket.node
        self.from_socket_type = socket.other.bl_idname
        self.to_socket_type = socket.bl_idname
        self.message = "Implicit conversion from socket type {} to socket type {} is not supported for socket {} of node {}. Please use explicit conversion nodes.".format(self.from_socket_type, self.to_socket_type, socket.name, socket.node.name)

    def __str__(self):
        return self.message

class NoImplicitConversionPolicy(object):
    """
    Base (empty) implicit conversion policy.
    This prohibits any implicit conversions.
    """
    @classmethod
    def convert(cls, socket, source_data):
        raise ImplicitConversionProhibited(socket)

class LenientImplicitConversionPolicy(object):
    """
    Lenient implicit conversion policy.
    Does not actually convert anything, but passes any
    type of data as-is.
    To be used for sockets that do not care about the
    nature of data they process (such as most List processing
    nodes).
    """
    @classmethod
    def convert(cls, socket, source_data):
        return source_data

class DefaultImplicitConversionPolicy(NoImplicitConversionPolicy):
    """
    Default implicit conversion policy.
    """
    @classmethod
    def convert(cls, socket, source_data):
        if is_vector_to_matrix(socket):
            return cls.vectors_to_matrices(socket, source_data)
        elif is_matrix_to_vector(socket):
            return cls.matrices_to_vectors(socket, source_data)
        elif is_quaternion_to_matrix(socket):
            return cls.quaternions_to_matrices(socket, source_data)
        elif is_matrix_to_quaternion(socket):
            return cls.matrices_to_quaternions(socket, source_data)
        elif socket.bl_idname in cls.get_lenient_socket_types():
            return source_data
        else:
            super().convert(socket, source_data)

    @classmethod
    def get_lenient_socket_types(cls):
        """
        Return collection of bl_idnames of socket classes
        that are allowed to consume arbitrary data type.
        """
        return ['StringsSocket', 'SvObjectSocket', 'SvColorSocket']

    @classmethod
    def vectors_to_matrices(cls, socket, source_data):
        # this means we're going to get a flat list of the incoming
        # locations and convert those into matrices proper.
        out = get_matrices_from_locs(source_data)
        socket.num_matrices = len(out)
        return out
    
    @classmethod
    def matrices_to_vectors(cls, socket, source_data):
        return get_locs_from_matrices(source_data)

    @classmethod
    def quaternions_to_matrices(cls, socket, source_data):
        out = get_matrices_from_quaternions(source_data)
        socket.num_matrices = len(out)
        return out

    @classmethod
    def matrices_to_quaternions(cls, socket, source_data):
        return get_quaternions_from_matrices(source_data)

