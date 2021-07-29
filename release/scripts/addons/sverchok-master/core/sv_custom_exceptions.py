# This file is part of project Sverchok. It's copyrighted by the contributors
# recorded in the version control history of the file, available from
# its original location https://github.com/nortikin/sverchok/commit/master
#  
# SPDX-License-Identifier: GPL3
# License-Filename: LICENSE



class SvProcessingError(Exception):
    pass


class SvNotFullyConnected(SvProcessingError):

    def __init__(self, node, sockets):
        self.node = node
        self.sockets = sockets
        socket_names = ", ".join(sockets)
        self.message = "The following inputs are required for node to perform correctly: " + socket_names

    def __str__(self):
        return self.message
