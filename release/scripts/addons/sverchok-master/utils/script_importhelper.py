import inspect
from sverchok.utils.sv_script import SvScript

def load_script(source, file_name):
    from_file = '<{}>'.format(file_name)
    code = compile(source, from_file, 'exec', optimize=0)
    # insert classes that we can inherit from
    local_space = {cls.__name__:cls for cls in SvScript.__subclasses__()}
    base_classes = set(cls.__name__ for cls in SvScript.__subclasses__())
    local_space["SvScript"] = SvScript
    
    exec(code, globals(),local_space)

    script = None
    
    for name in code.co_names:
        # filter out inherited
        if not name in local_space:
            continue
        # skip base classes
        if name in base_classes:
            continue        
            
        try: 
            script_class = local_space.get(name)
            if inspect.isclass(script_class):
                script = script_class()
                if isinstance(script, SvScript):
                    print("Script Node found script {}".format(name))
                    script = script
                    globals().update(local_space)
                    
        except Exception as Err:
            print("Script Node couldn't load {0}".format(name))
            print(Err) 
            
    if not script:
        raise ImportWarning("Couldn't find script in {}".format(name))  
    return script
    
