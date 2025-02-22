# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import functools


def cached_by_key(key):
    """
    Decorates functions whose result should be cached. Use it like:
        @cached_by_key(key=...)
        def func(..., export_settings):
            ...
    The decorated function, func, must always take an "export_settings" arg
    (the cache is stored here).
    The key argument to the decorator is a function that computes the key to
    cache on. It is passed all the arguments to func.
    """
    def inner(func):
        @functools.wraps(func)
        def wrapper_cached(*args, **kwargs):
            if kwargs.get("export_settings"):
                export_settings = kwargs["export_settings"]
            else:
                export_settings = args[-1]

            cache_key = key(*args, **kwargs)

            # invalidate cache if export settings have changed
            if not hasattr(func, "__export_settings") or export_settings != func.__export_settings:
                func.__cache = {}
                func.__export_settings = export_settings
            # use or fill cache
            if cache_key in func.__cache:
                return func.__cache[cache_key]
            else:
                result = func(*args, **kwargs)
                func.__cache[cache_key] = result
                return result

        return wrapper_cached

    return inner


def default_key(*args, **kwargs):
    """
    Default cache key for @cached functions.
    Cache on all arguments (except export_settings).
    """
    assert len(args) >= 2 and 0 <= len(kwargs) <= 1, "Wrong signature for cached function"
    cache_key_args = args
    # make a shallow copy of the keyword arguments so that 'export_settings' can be removed
    cache_key_kwargs = dict(kwargs)
    if kwargs.get("export_settings"):
        del cache_key_kwargs["export_settings"]
    else:
        cache_key_args = args[:-1]

    cache_key = ()
    for i in cache_key_args:
        cache_key += (i,)
    for i in cache_key_kwargs.values():
        cache_key += (i,)

    return cache_key


def cached(func):
    return cached_by_key(key=default_key)(func)


def _is_driver_baking(val):
    tab = val.split("_")
    if len(tab) < 2:
        return False

    if tab[0] != tab[1]:
        return False

    return True


def datacache(func):

    def reset_all_cache():
        func.__cache = {}

    func.reset_cache = reset_all_cache

    @functools.wraps(func)
    def wrapper_objectcache(*args, **kwargs):

        # 0 : path
        # 1 : object_uuid
        # 2 : bone (can be, of course, None for path other than 'bone')
        # 3 : action_name
        # 4 : current_frame
        # 5 : step
        # 6 : slot handle
        # 7 : export_settings
        # only_gather_provided : only_gather_provided

        cache_key_args = args
        cache_key_args = args[:-1]

        if not hasattr(func, "__cache"):
            func.reset_cache()

        # object is not cached yet
        if cache_key_args[1] not in func.__cache.keys():
            result = func(*args)
            func.__cache = result
            # Here are the key used: result[obj_uuid][action_name][slot_identifier][path][bone][frame]
            return result[cache_key_args[1]][cache_key_args[3]][cache_key_args[6]][cache_key_args[0]][cache_key_args[2]][cache_key_args[4]]
        # object is in cache, but not this action
        # We need to not erase other actions of this object
        elif cache_key_args[3] not in func.__cache[cache_key_args[1]].keys():
            result = func(*args, only_gather_provided=True)
            # The result can contains multiples animations, in case this is an armature with drivers
            # Need to create all newly retrieved animations
            func.__cache.update(result)
            # Here are the key used: result[obj_uuid][action_name][slot_identifier][path][bone][frame]
            return result[cache_key_args[1]][cache_key_args[3]][cache_key_args[6]][cache_key_args[0]][cache_key_args[2]][cache_key_args[4]]
        # object and action are in cache, but not this slot
        elif cache_key_args[6] not in func.__cache[cache_key_args[1]][cache_key_args[3]].keys():
            if cache_key_args[6] is None and (cache_key_args[3] == cache_key_args[1] or _is_driver_baking(cache_key_args[3])):
                # We are currently baking this animation
                # But maybe we get these data from another object action / with a slot
                # So if there are some data for a slot, use them
                if len(func.__cache[cache_key_args[1]][cache_key_args[3]]) > 0:
                    first_key = list(func.__cache[cache_key_args[1]][cache_key_args[3]].keys())[0]
                    # Here are the key used: result[obj_uuid][action_name][slot_identifier][path][bone][frame]
                    return func.__cache[cache_key_args[1]][cache_key_args[3]][first_key][cache_key_args[0]][cache_key_args[2]][cache_key_args[4]]
            else:
                result = func(*args, only_gather_provided=True)
                # The result can contains multiples animations, in case this is an armature with drivers
                # Need to create all newly retrieved animations
                func.__cache.update(result)
                # Here are the key used: result[obj_uuid][action_name][slot_identifier][path][bone][frame]
                return result[cache_key_args[1]][cache_key_args[3]][cache_key_args[6]][cache_key_args[0]][cache_key_args[2]][cache_key_args[4]]
        # all is already cached
        else:
            # Here are the key used: result[obj_uuid][action_name][slot_identifier][path][bone][frame]
            return func.__cache[cache_key_args[1]][cache_key_args[3]
                                                   ][cache_key_args[6]][cache_key_args[0]][cache_key_args[2]][cache_key_args[4]]
    return wrapper_objectcache


# TODO: replace "cached" with "unique" in all cases where the caching is functional and not only for performance reasons
call_or_fetch = cached
unique = cached


def skdriverdiscovercache(func):

    def reset_cache_skdriverdiscovercache():
        func.__current_armature_uuid = None
        func.__skdriverdiscover = {}

    func.reset_cache = reset_cache_skdriverdiscovercache

    @functools.wraps(func)
    def wrapper_skdriverdiscover(*args, **kwargs):

        # 0 : armature_uuid
        # 1 : export_settings

        cache_key_args = args
        cache_key_args = args[:-1]

        if not hasattr(func, "__current_armature_uuid") or func.__current_armature_uuid is None:
            func.reset_cache()

        if cache_key_args[0] != func.__current_armature_uuid:
            result = func(*args)
            func.__skdriverdiscover[cache_key_args[0]] = result
            func.__current_armature_uuid = cache_key_args[0]
            return result
        else:
            return func.__skdriverdiscover[cache_key_args[0]]
    return wrapper_skdriverdiscover
