import os
from os.path import dirname
from os.path import basename
from collections import defaultdict

directory = dirname(__file__)

# you may supply a list of `directory/node_name.py` to ignore
# this is about the only manual thing in this file.
ignore_list = {}
ignore_list['analyzer'] = ['bvh_raycast', 'bvh_nearest']
ignore_list['scene'] = ['create_bvh_tree']

nodes_dict = defaultdict(list)

def automatic_collection():
    for subdir, dirs, files in os.walk(directory):
        current_dir = basename(subdir)
        if current_dir == '__pycache__':
            continue
        for file in files:
            if file == '__init__.py':
                continue
            if not file.endswith('.py'):
                continue
            nodes_dict[current_dir].append(file[:-3])

    # remove items found in ignore_list
    for k, v in ignore_list.items():
        items = nodes_dict.get(k)
        if items:
            for filename in v:
                try:
                    items.remove(filename)
                except:
                    print('failed to remove', filename, 'from', k, ' : check your spelling')

    # may not be used, but can be.
    return nodes_dict

automatic_collection()
