import importlib
import sverchok

reload_event = False

root_modules = [
    "menu", "node_tree", "data_structure", "core",
    "utils", "ui", "nodes", "old_nodes", "sockets",
]

core_modules = [
    "monad_properties", "sv_custom_exceptions",
    "handlers", "update_system", "upgrade_nodes", "upgrade_group",
    "monad", "node_defaults"
]

def sv_register_modules(modules):
    for m in modules:
        if m.__name__ != "sverchok.menu":
            if hasattr(m, "register"):
                # print("Registering module: {}".format(m.__name__))
                m.register()

def sv_unregister_modules(modules):
    for m in reversed(modules):
        if hasattr(m, "unregister"):
            # print("Unregistering module: {}".format(m.__name__))
            m.unregister()

def sv_registration_utils():
    """ this is a faux module for syntactic sugar on the imports in __init__ """
    pass


sv_registration_utils.register_all = sv_register_modules 
sv_registration_utils.unregister_all = sv_unregister_modules


def reload_all(imported_modules, node_list, old_nodes):
    # reload base modules
    _ = [importlib.reload(im) for im in imported_modules]

    # reload nodes
    _ = [importlib.reload(node) for node in node_list]

    old_nodes.reload_old()


def make_node_list(nodes):
    node_list = []
    base_name = "sverchok.nodes"
    for category, names in nodes.nodes_dict.items():
        importlib.import_module('.{}'.format(category), base_name)
        import_modules(names, '{}.{}'.format(base_name, category), node_list)
    return node_list


def import_modules(modules, base, im_list):
    for m in modules:
        im = importlib.import_module('.{}'.format(m), base)
        im_list.append(im)


def handle_reload_event(nodes, imported_modules, old_nodes):
    node_list = make_node_list(nodes)
    reload_all(imported_modules, node_list, old_nodes)
    return node_list


def import_settings(imported_modules, sv_dir_name):
    # "settings" treated separately incase the sverchok dir not named "sverchok"
    settings = importlib.import_module(".settings", sv_dir_name)
    imported_modules.append(settings)


def import_all_modules(imported_modules, mods_bases):
    for mods, base in mods_bases:
        import_modules(mods, base, imported_modules)


def init_architecture(sv_name, utils_modules, ui_modules):

    imported_modules = []
    mods_bases = [
        (root_modules, "sverchok"),
        (core_modules, "sverchok.core"),
        (utils_modules, "sverchok.utils"),
        (ui_modules, "sverchok.ui")
    ]

    import_settings(imported_modules, sv_name)
    import_all_modules(imported_modules, mods_bases)
    return imported_modules


def init_bookkeeping(sv_name):

    from sverchok.core import node_defaults
    from sverchok.utils import ascii_print, auto_gather_node_classes

    sverchok.data_structure.SVERCHOK_NAME = sv_name
    ascii_print.show_welcome()
    node_defaults.register_defaults()
    auto_gather_node_classes()    
