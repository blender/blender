import win32api, win32con, string, types

def _getDataType( data, coerce = 1 ):
    '''
    Return a tuple of dataType, data for a given object
    automatically converts non-string-or-tuple-data into
    strings by calling pickle.dumps
    '''
    if type( data ) is types.StringType:
        return win32con.REG_SZ, data
    elif type( data ) is types.IntType:
        return win32con.REG_DWORD, data
    # what about attempting to convert Longs, floats, etceteras to ints???
    elif coerce:
        import pickle
        return win32con.REG_SZ, pickle.dumps( data )
    else:
        raise TypeError, '''Unsupported datatype for registry, use getDataType( data, coerce=1) to store types other than string/int.'''

def _getBaseKey( fullPathSpec ):
    '''
    Split a "full path specification" registry key
    into its root and subpath components
    '''
    key = ''
    subkey = fullPathSpec
    # while loop will strip off preceding \\ characters
    while subkey and not key:
        key, subkey = string.split( fullPathSpec, '\\', 1 )
    try:
        return getattr( win32con, key ), subkey
    except AttributeError:
        raise '''Unknown root key %s in registry path %s'''% (key, fullPathSpec)

def RegSetValue( key, valuename='', data='', allowPickling=1 ):
    '''
    Set a registry value by providing a fully-specified
    registry key (and an optional sub-key/value name),
    and a data element.  If allowPickling is true, the
    data element can be any picklable element, otherwise
    data element must be a string or integer.
    '''
    root, subkey = _getBaseKey( key )
    dataType, data = _getDataType( data, allowPickling )
    try:
        hKey = win32api.RegOpenKeyEx( root , subkey, 0, win32con.KEY_ALL_ACCESS) # could we use a lesser access model?
    except:
        hKey = win32api.RegCreateKey( root, subkey )
    try:
        if not valuename: # the default value
            win32api.RegSetValue( hKey, valuename, dataType, data )
        else: # named sub-value
            win32api.RegSetValueEx( hKey, valuename, 0, dataType, data )
    finally:
        win32api.RegCloseKey( hKey)

def RegQueryValue( key, valuename='', pickling=0 ):
    '''
    Get a registry value by providing a fully-specified
    registry key (and an optional sub-key/value name)
    If pickling is true, the data element will be 
    unpickled before being returned.
    '''
    #print 'key', key
    root, subkey = _getBaseKey( key )
    if not valuename: # the default value
        data, type = win32api.RegQueryValue( root , subkey)
    else:
        try:
            #print root, subkey
            hKey = win32api.RegOpenKeyEx( root, subkey, 0, win32con.KEY_READ)
            #print hKey, valuename
            try:
                data, type = win32api.RegQueryValueEx( hKey, valuename )
            except: #
                data, type = None, 0 # value is not available...
                pickling = None
        finally:
            win32api.RegCloseKey( hKey)
    if pickling:
        import pickle
        data = pickle.loads( data )
    return data
    
# following constants seem to reflect where path data is stored on NT machines
# no idea if it'll work on a 95 machine

def AddPathEntry( newEntry, user = 1, prepend=0 ):
    '''
    Add or remove path entry on NT, use prepend == -1 for removal,
    use prepend == 0 for append, prepend= 1 for prepending to the
    current path.
    '''
    if user:
        user = 'USER'
    else:
        user = 'MACHINE'
    key, valuename = COMMON_KEYS[ (user, 'PATH') ]
    _PathManager( key, valuename, newEntry, prepend )

def PyExecutables( user = 1, prepend=0 ):
    '''
    Register/Deregister Python files as executables
    '''
    if user:
        user = 'USER'
    else:
        user = 'MACHINE'
    key, valuename = COMMON_KEYS[ (user, 'PYEXECUTABLES') ]
    # the default executables + Python scripts...
    if prepend < 0: # are to eliminate only .py
        newEntry = '.PY'
    else:
        newEntry = '.PY;.COM;.EXE;.BAT;.CMD'
    _PathManager( key, valuename, newEntry, prepend )

def _PathManager( key, valuename, newEntry, prepend=0, eliminate_duplicates=1 ):
    '''
    Create a new Path entry on NT machines (or kill an old one)
    user determines whether to alter the USER or the Machine's path
    prepend
         1 -> add newEntry to start
         0 -> add newEntry to end
        -1 -> don't add newEntry 
    eliminate_duplicates determines whether to kill equal paths
    
    All values are converted to lower case
    '''
    # get current value...
    curval = RegQueryValue( key, valuename ) or ''
    # split into elements
    curval = string.split( string.lower(curval), ';' )
    if type( newEntry ) not in (types.ListType, types.TupleType):
        newEntry = string.split( string.lower(newEntry), ';' )
    # eliminate duplicates of the newEntry
    curval = filter( None, curval) # strip out null entries
    if eliminate_duplicates:
        newval = []
        for p in curval:
            if p not in newEntry: 
                newval.append( p )
        curval = newval
    if prepend == 1:
        curval = list(newEntry) + curval
    elif prepend == 0:
        curval = curval + list( newEntry )
    elif prepend == -1: # this call is just killing the path entry
        pass
    #now do the recombination
    curval = string.join( curval, ';' )
    RegSetValue( key, valuename, curval )

COMMON_KEYS = {
('USER','PATH') : ('''HKEY_CURRENT_USER\\Environment''', 'path'), 
('MACHINE','PATH') : ('''HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment''', 'path'),
('USER','PYEXECUTABLES') : ('''HKEY_CURRENT_USER\\Environment''', 'pathext'),
('MACHINE','PYEXECUTABLES') : ('''HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment''', 'pathext')
}
