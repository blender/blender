# BEGIN GPL LICENSE BLOCK #####
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
# END GPL LICENSE BLOCK #####
# support classes for SvScript node MK2
# some utility functions

import abc
# basic class for Script Node MK2   
from .sv_itertools import sv_zip_longest

import itertools

'''
TEMPORARY DOCUMENTATION

Every SvScript needs a self.process() function.
The node can be access via self.node
 
Procsess won't be called unless all sockets without a default are conneted
 
inputs = (socket_type, socket_name, default, ... )
outputs = (socket_type, socket_name, ... )
the ... can be additional parameters for specific node script.

if the function provides a draw_buttons it will be called
the same with update, but then the node is also responsible for calling process 

if the .name parameter is set it will used as a label otherwise the class will be used
'''

# base method for all scripts
class SvScript(metaclass=abc.ABCMeta):
    
    @abc.abstractmethod
    def process(self):
        return

    
class SvScriptAuto(SvScript, metaclass=abc.ABCMeta):
    """ 
    f(x,y,z,...n) -> t
    with unlimited depth, match longest
    """

    @staticmethod
    @abc.abstractmethod
    def function(*args):
        return
        
    def process(self):
        inputs = self.node.inputs
        tmp = [s.sv_get() for s in inputs]
        res = atomic_map(self.function, tmp)
        self.node.outputs[0].sv_set(res)
        
class SvScriptSimpleGenerator(SvScript, metaclass=abc.ABCMeta):
    """
    Simple generator script template
    outputs must be in the following format
    (socket_type, socket_name, socket_function)
    where socket_function will be called for linked socket production
    for each set of input parameters
    """
    def process(self):
        inputs = self.node.inputs
        outputs = self.node.outputs
        
        data = [s.sv_get()[0] for s in inputs]

        for socket, ref in zip(outputs, self.outputs):
            if socket.links:
                func = getattr(self, ref[2])
                out = tuple(itertools.starmap(func, sv_zip_longest(*data)))
                socket.sv_set(out)

class SvScriptSimpleFunction(SvScript, metaclass=abc.ABCMeta):
    """
    Simple f(x0, x1, ... xN) -> y0, y1, ... ,yM
    
    """
    @abc.abstractmethod
    def function(*args, depth=None):
        return 
        
    def process(self):
        inputs = self.node.inputs
        outputs = self.node.outputs
        
        data = [s.sv_get() for s in inputs]
        # this is not used yet, I don't think flat depth is the right long
        # term approach, but the data tree should be easily parseable, which it isn't right now
        depth = tuple(map(recursive_depth, data))
        links = [s.links for s in outputs]
        result = [[] for d in data]
        for d in zip(*data):
            res = self.function(*d, depth=depth)
            for i, r in enumerate(res):
                result[i].append(r)
        for link, res, socket in zip(links, result, outputs):
            if link:
                socket.sv_set(res)

class SvMultiInput(SvScript):
    """
    Multi input base file. Many sockets to one socket.
    
    """
    def update(self):
        if isinstance(self.inputs, tuple):
            self.inputs = list(self.inputs) 
        inputs = self.node.inputs
        if not inputs:
            print(len(inputs))
            return
        if inputs[-1].links:
            print("multi inputs")
            length = len(inputs)
            name = self.base_name.format(str(length))
            socket_types = {
                'v': 'VerticesSocket',
                's': 'StringsSocket',
                'm': 'MatrixSocket'
            }
            s_type = socket_types[self.multi_socket_type]    
            inputs.new(s_type, name)
            self.inputs.append((self.multi_socket_type, name))
        else:
            while len(inputs) > 1 and not inputs[-2].links:
                inputs.remove(inputs[-1])
    
    def process(self):
        in_data = [s.sv_get() for s in self.node.inputs if s.links]
        if in_data and self.node.outputs[0].links:
            out_data = self.function(in_data)
            self.node.outputs[0].sv_set(out_data)

class SvScriptFunction(SvScript, metaclass=abc.ABCMeta):
    """
    Complex f(x0, x1, ... xN) -> y0, y1, ... ,yM
    NOT READY
    """
    @abc.abstractmethod
    def function(*args):
        return 
        
    def process(self):
        
        inputs = self.node.inputs
        outputs = self.node.outputs
        
        data = [s.sv_get() for s in inputs]

        depth = tuple(map(recursive_depth, data))
        work_depth = [i[-1] for i in inputs]
        diff = [d-wd for d,wd in zip(depth, work_depth)]
        if any(diff):
            if any((x < 0 for x in diff)):
                print("not enough depth")
            else:
                def wrap(data, n):
                    if n > 0:
                        return wrap([data], n-1)
                    else:
                        return data
                        
                for i in range(len(data)):
                    if diff[i] > 0:
                        data[i] = wrap(data[i], diff[i])
                
            
        links = [s.links for s in outputs]
        result = [[] for d in data]
        
        for d in zip(*data):
            res = self.function(*d, depth=depth)
            for i, r in enumerate(res):
                result[i].append(r)
        for link, res, socket in zip(links, result, outputs):
            if link:
                socket.sv_set(res)
                    

class SvMultiInput(SvScript):
    
    def update(self):
        if isinstance(self.inputs, tuple):
            self.inputs = list(self.inputs) 
        inputs = self.node.inputs
        if not inputs:
            print(len(inputs))
            return
        if inputs[-1].links:
            print("multi inputs")
            length = len(inputs)
            name = self.base_name.format(str(length))
            socket_types = {
                'v': 'VerticesSocket',
                's': 'StringsSocket',
                'm': 'MatrixSocket'
            }
            s_type = socket_types[self.multi_socket_type]    
            inputs.new(s_type, name)
            self.inputs.append((self.multi_socket_type, name))
        else:
            while len(inputs) > 1 and not inputs[-2].links:
                inputs.remove(inputs[-1])
    
    def process(self):
        in_data = [s.sv_get() for s in self.node.inputs if s.links]
        if in_data and self.node.outputs[0].links:
            out_data = self.function(in_data)
            self.node.outputs[0].sv_set(out_data)

# below are helper functions

def recursive_depth(l):
    if isinstance(l, (list, tuple)) and l:
        return 1 + recursive_depth(l[0])
    elif isinstance(l, (int, float, str)):
        return 0
    else:
        return None
        


        
# this method will be renamed and moved
        
def atomic_map(f, args):
    # this should support different methods for finding depth
    types = tuple(isinstance(a, (int, float)) for a in args)
    
    if all(types):
        return f(*args)
    elif any(types):
        tmp = [] 
        tmp_app = tmp.append
        for t,a in zip(types, args):
            if t:
                tmp_app((a,))
            else:
                tmp_app(a)
        return atomic_map(f, tmp)
    else:
        res = []
        res_app = res.append
        for z_arg in sv_zip_longest(*args):
            res_app(atomic_map(f, z_arg))
        return res


# not ready at all.
def v_map(f,*args, kwargs):
    def vector_map(f, *args):
        # this should support different methods for finding depth   
        types = tuple(isinstance(a, (int, float)) for a in args)
        if all(types):
            return f(*args)
        elif any(types):
            tmp = [] 
            tmp_app
            for t,a in zip(types, args):
                if t:
                    tmp_app([a])
                else:
                    tmp_app(a)
            return atomic_map(f, *tmp)
        else:
            res = []
            res_app = res.append
            for z_arg in sv_zip_longest(*args):
                res_app(atomic_map(f,*z_arg))
            return res
