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

import collections
import time

import bpy
from mathutils import Vector

from sverchok import data_structure
from sverchok.core.socket_data import SvNoDataError, reset_socket_cache
from sverchok.utils.logging import debug, info, warning, error, exception
from sverchok.utils.profile import profile
import sverchok

import traceback
import ast

graphs = []

no_data_color = (1, 0.3, 0)
exception_color = (0.8, 0.0, 0)

def update_error_colors(self, context):
    global no_data_color
    global exception_color
    no_data_color = self.no_data_color[:]
    exception_color = self.exception_color[:]

# cache node group update trees
update_cache = {}
# cache for partial update lists
partial_update_cache = {}


def make_dep_dict(node_tree, down=False):
    """
    Create a dependency dictionary for node group.
    """
    ng = node_tree

    deps = collections.defaultdict(set)

    # create wifi out dependencies, process if needed

    wifi_out_nodes = [(name, node.var_name)
                  for name, node in ng.nodes.items()
                  if node.bl_idname == 'WifiOutNode' and node.outputs]
    if wifi_out_nodes:
        wifi_dict = {node.var_name: name
                     for name, node in ng.nodes.items()
                     if node.bl_idname == 'WifiInNode'}

    for i,link in enumerate(list(ng.links)):
        #  this proctects against a rare occurance where
        #  a link is considered valid without a to_socket
        #  or a from_socket. proctects against a blender crash
        #  see https://github.com/nortikin/sverchok/issues/493
        if not (link.to_socket and link.from_socket):
            ng.links.remove(link)
            raise ValueError("Invalid link found!, please report this file")
        if not link.is_valid:
            return collections.defaultdict(set)  # this happens more often than one might think
        if link.is_hidden:
            continue
        key, value = (link.from_node.name, link.to_node.name) if down else (link.to_node.name, link.from_node.name)
        deps[key].add(value)

    for name, var_name in wifi_out_nodes:
        other = wifi_dict.get(var_name)
        if not other:
            warning("Unsatisifed Wifi dependency: node, %s var,%s", name, var_name)
            return collections.defaultdict(set)
        if down:
            deps[other].add(name)
        else:
            deps[name].add(other)

    return deps


def make_update_list(node_tree, node_set=None, dependencies=None):
    """
    Makes a update list from a node_group
    if a node set is passed only the subtree defined by the node set is used. Otherwise
    the complete node tree is used.
    If dependencies are not passed they are built.
    """

    ng = node_tree
    if not node_set:  # if no node_set, take all
        node_set = set(ng.nodes.keys())
    if len(node_set) == 1:
        return list(node_set)
    if node_set:  # get one name
        name = node_set.pop()
        node_set.add(name)
    else:
        return []
    if not dependencies:
        deps = make_dep_dict(ng)
    else:
        deps = dependencies

    tree_stack = collections.deque([name])
    tree_stack_append = tree_stack.append
    tree_stack_pop = tree_stack.pop
    out = collections.OrderedDict()
    # travel in node graph create one sorted list of nodes based on dependencies
    node_count = len(node_set)
    while node_count > len(out):
        node_dependencies = True
        for dep_name in deps[name]:
            if dep_name in node_set and dep_name not in out:
                tree_stack_append(name)
                name = dep_name
                node_dependencies = False
                break
        if len(tree_stack) > node_count:
            error("Invalid node tree!")
            return []
        # if all dependencies are in out
        if node_dependencies:
            if name not in out:
                out[name] = 1
            if tree_stack:
                name = tree_stack_pop()
            else:
                if node_count == len(out):
                    break
                for node_name in node_set:
                    if node_name not in out:
                        name = node_name
                        break
    return list(out.keys())


def separate_nodes(ng, links=None):
    '''
    Separate a node group (layout) into unconnected parts
    Arguments: Node group
    Returns: A list of sets with separate node groups
    '''
    nodes = set(ng.nodes.keys())
    if not nodes:
        return []
    node_links = make_dep_dict(ng)
    down = make_dep_dict(ng, down=True)
    for name, links in down.items():
        node_links[name].update(links)
    n = nodes.pop()
    node_set_list = [set([n])]
    node_stack = collections.deque()
    # find separate sets
    node_stack_append = node_stack.append
    node_stack_pop = node_stack.pop

    while nodes:
        for node in node_links[n]:
            if node not in node_set_list[-1]:
                node_stack_append(node)
        if not node_stack:  # new part
            n = nodes.pop()
            node_set_list.append(set([n]))
        else:
            while n in node_set_list[-1] and node_stack:
                n = node_stack_pop()
            nodes.discard(n)
            node_set_list[-1].add(n)
    """
    if ng.bl_idname == "SverchCustomTreeType":
        skip_types = {"SvGroupInputsNode", "SvGroupOutputsNode"}
        skip_nodes = {n.name for n in ng.nodes if n.bl_idname in skip_types}
        if skip_nodes:
            node_set_list = filter(lambda ns:ns.isdisjoint(skip_nodes), node_set_list)
    """
    return [ns for ns in node_set_list if len(ns) > 1]

def make_tree_from_nodes(node_names, tree, down=True):
    """
    Create a partial update list from a sub-tree, node_names is a list of nodes that
    drives change for the tree
    """
    ng = tree
    nodes = ng.nodes
    if not node_names:
        warning("No nodes!")
        return make_update_list(ng)

    out_set = set(node_names)

    out_stack = collections.deque(node_names)
    current_node = out_stack.pop()

    # build downwards links, this should be cached perhaps
    node_links = make_dep_dict(ng, down)
    while current_node:
        for node in node_links[current_node]:
            if node not in out_set:
                out_set.add(node)
                out_stack.append(node)
        if out_stack:
            current_node = out_stack.pop()
        else:
            current_node = ''

    if len(out_set) == 1:
        return list(out_set)
    else:
        return make_update_list(ng, out_set)


# to make update tree based on node types and node names bases
# no used yet
# should add a check do find animated or driven nodes.
# needs some updates

def make_animation_tree(node_types, node_list, tree_name):
    """
    Create update list for specific purposes depending on which nodes are dynamic
    node_types
    """
    global update_cache
    ng = bpy.data.node_groups[tree_name]
    node_set = set(node_list)
    for n_t in node_types:
        node_set = node_set | {name for name, node in ng.nodes.items() if node.bl_idname == n_t}
    a_tree = make_tree_from_nodes(list(node_set), tree_name)
    return a_tree


def do_update_heat_map(node_list, nodes):
    """
    Create a heat map for the node tree,
    Needs development.
    """
    if not nodes.id_data.sv_user_colors:
        color_data = {node.name: (node.color[:], node.use_custom_color) for node in nodes}
        nodes.id_data.sv_user_colors = str(color_data)

    times = do_update_general(node_list, nodes)
    if not times:
        return
    t_max = max(times)
    addon_name = data_structure.SVERCHOK_NAME
    addon = bpy.context.user_preferences.addons.get(addon_name)
    if addon:
        # to use Vector.lerp
        cold = Vector(addon.preferences.heat_map_cold)
        hot = addon.preferences.heat_map_hot
    else:
        error("Cannot find preferences")
        cold = Vector((1, 1, 1))
        hot = (.8, 0, 0)
    for name, t in zip(node_list, times):
        nodes[name].use_custom_color = True
        # linear scale.
        nodes[name].color = cold.lerp(hot, t / t_max)

def update_error_nodes(ng, name, err=Exception):
    if ng.bl_idname == "SverchGroupTreeType":
        return # ignore error color inside of monad
    if "error nodes" in ng:
        error_nodes = ast.literal_eval(ng["error nodes"])
    else:
        error_nodes = {}
    if ng.bl_idname == "SverchGroupTreeType":
        return
    node = ng.nodes.get(name)
    if not node:
        return
    error_nodes[name] = (node.use_custom_color, node.color[:])
    ng["error nodes"] = str(error_nodes)

    if isinstance(err, SvNoDataError):
        node.color = no_data_color
    else:
        node.color = exception_color
    node.use_custom_color=True

def reset_error_nodes(ng):
    if "error nodes" in ng:
        error_nodes = ast.literal_eval(ng["error nodes"])
        for name, data in error_nodes.items():
            node = ng.nodes.get(name)
            if node:
                node.use_custom_color = data[0]
                node.color = data[1]
        del ng["error nodes"]


@profile(section="UPDATE")
def do_update_general(node_list, nodes, procesed_nodes=set()):
    """
    General update function for node set
    """
    global graphs
    timings = []
    graph = []
    total_time = 0
    done_nodes = set(procesed_nodes)

    for node_name in node_list:
        if node_name in done_nodes:
            continue
        try:
            node = nodes[node_name]
            start = time.perf_counter()
            if hasattr(node, "process"):
                node.process()
            delta = time.perf_counter() - start
            total_time += delta
            if data_structure.DEBUG_MODE:
                debug("Processed  %s in: %.4f", node_name, delta)
            timings.append(delta)
            graph.append({"name" : node_name,
                           "bl_idname": node.bl_idname,
                           "start": start,
                           "duration": delta})

        except Exception as err:
            ng = nodes.id_data
            update_error_nodes(ng, node_name, err)
            #traceback.print_tb(err.__traceback__)
            exception("Node %s had exception: %s", node_name, err)
            return None
    graphs.append(graph)
    if data_structure.DEBUG_MODE:
        debug("Node set updated in: %.4f seconds", total_time)
    return timings


def do_update(node_list, nodes):
    if data_structure.HEAT_MAP:
        do_update_heat_map(node_list, nodes)
    else:
        do_update_general(node_list, nodes)

def build_update_list(ng=None):
    """
    Makes a complete update list for the tree,
    If tree is not passed, all sverchok custom tree
    are processced
    """
    global update_cache
    global partial_update_cache
    global graphs
    graphs = []
    if not ng:
        for ng in sverchok_trees():
            build_update_list(ng)
    else:
        node_sets = separate_nodes(ng)
        deps = make_dep_dict(ng)
        out = [make_update_list(ng, s, deps) for s in node_sets]
        update_cache[ng.name] = out
        partial_update_cache[ng.name] = {}
        reset_socket_cache(ng)


def process_to_node(node):
    """
    Process nodes upstream until node
    """
    global graphs
    graphs = []

    ng = node.id_data
    reset_error_nodes(ng)

    if data_structure.RELOAD_EVENT:
        reload_sverchok()
        return

    update_list = make_tree_from_nodes([node.name], ng, down=False)
    do_update(update_list, ng.nodes)


def process_from_nodes(nodes):
    node_names = [node.name for node in nodes]
    ng = nodes[0].id_data
    update_list = make_tree_from_nodes(node_names, ng)
    do_update(update_list, ng.nodes)


def process_from_node(node):
    """
    Process downstream from a given node
    """
    global update_cache
    global partial_update_cache
    global graphs
    graphs = []
    ng = node.id_data
    reset_error_nodes(ng)

    if data_structure.RELOAD_EVENT:
        reload_sverchok()
        return
    if update_cache.get(ng.name):
        p_u_c = partial_update_cache.get(ng.name)
        update_list = None
        if p_u_c:
            update_list = p_u_c.get(node.name)
        if not update_list:
            update_list = make_tree_from_nodes([node.name], ng)
            partial_update_cache[ng.name][node.name] = update_list
        nodes = ng.nodes
        if not ng.sv_process:
            return
        do_update(update_list, nodes)
    else:
        process_tree(ng)

def sverchok_trees():
    for ng in bpy.data.node_groups:
        if ng.bl_idname == "SverchCustomTreeType":
            yield ng

def process_tree(ng=None):
    global update_cache
    global partial_update_cache
    global graphs
    graphs = []

    if data_structure.RELOAD_EVENT:
        reload_sverchok()
        return
    if not ng:
        for ng in sverchok_trees():
            process_tree(ng)
    elif ng.bl_idname == "SverchCustomTreeType" and ng.sv_process:
        update_list = update_cache.get(ng.name)
        reset_error_nodes(ng)
        if not update_list:
            build_update_list(ng)
            update_list = update_cache.get(ng.name)
        for l in update_list:
            do_update(l, ng.nodes)
    else:
        pass


def reload_sverchok():
    data_structure.RELOAD_EVENT = False
    from sverchok.core import handlers
    handlers.sv_post_load([])

def get_update_lists(ng):
    """
    Make update list available in blender console.
    See the function with the same name in node_tree.py
    """
    global update_cache
    global partial_update_cache
    if not ng.name in update_cache:
        build_update_list(ng)
    return (update_cache.get(ng.name), partial_update_cache.get(ng.name))

def register():
    addon_name = sverchok.__name__
    addon = bpy.context.user_preferences.addons.get(addon_name)
    if addon:
        update_error_colors(addon.preferences, [])
