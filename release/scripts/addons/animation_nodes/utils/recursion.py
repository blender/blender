import functools

activeFunctions = set()

def noRecursion(function):
    """The decorated function should not return any values"""
    @functools.wraps(function)
    def wrapper(*args, **kwargs):
        identifier = id(function)
        if identifier not in activeFunctions:
            activeFunctions.add(identifier)
            result = function(*args, **kwargs)
            activeFunctions.remove(identifier)
            return result
    return wrapper

def noCallbackRecursion(function):
    """The decorated function should not return any values"""
    @functools.wraps(function)
    def wrapper(self, context):
        identifier = id(function)
        if identifier not in activeFunctions:
            activeFunctions.add(identifier)
            result = function(self, context)
            activeFunctions.remove(identifier)
            return result
    return wrapper
