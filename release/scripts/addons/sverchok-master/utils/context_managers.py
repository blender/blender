from contextlib import contextmanager

import bpy
import sverchok

@contextmanager
def hard_freeze(self):
    '''
    Use this when you don't want modifications to node properties 
    to trigger the node's `process()` function. 

    usage  (when self is a reference to a node)

        from sverchok.utils.context_managers import hard_freeze

        ...
        ...

        with hard_freeze(self) as node:
            node.some_prop = 'some_value_change'
            node.some_other_prop = 'some_other_value_change'

    '''
    self.id_data.freeze(hard=True)
    yield self
    self.id_data.unfreeze(hard=True)


@contextmanager
def sv_preferences():
    '''
    use this whenever you need set or get content of the user_preferences class
    usage
        from sverchok.utils.context_managers import sv_preferences
        ...
        with sv_preferences() as prefs:
            print(prefs.<some attr>)
    '''
    # by using svercok.__name__ we increase likelyhood that the addon preferences will correspond
    addon = bpy.context.user_preferences.addons.get(sverchok.__name__)
    if addon and hasattr(addon, "preferences"):
        yield addon.preferences


@contextmanager
def new_input(node, ident, name):
    '''
    use this to contextualize additional props on a socket. f.ex:

        c1 = inew('StringsSocket', 'stroke color')
        c1.prop_name = 'unit_1_color'
        c1.nodule_color = nodule_color

        becomes

        with new_input(self, 'StringsSocket', 'stroke color') as c1:
            c1.prop_name = 'unit_1_color'
            c1.nodule_color = nodule_color        


    '''
    yield node.inputs.new(ident, name)
