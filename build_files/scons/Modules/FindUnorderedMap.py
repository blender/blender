def FindUnorderedMap(conf):
    """
    Detect unordered_map availability
    """

    namespace = None
    header = None

    if conf.CheckCXXHeader("unordered_map"):
        # Even so we've found unordered_map header file it doesn't
        # mean unordered_map and unordered_set will be declared in
        # std namespace.
        #
        # Namely, MSVC 2008 have unordered_map header which declares
        # unordered_map class in std::tr1 namespace. In order to support
        # this, we do extra check to see which exactly namespace is
        # to be used.

        if conf.CheckType('std::unordered_map<int, int>', language = 'CXX', includes="#include <unordered_map>"):
            print("-- Found unordered_map/set in std namespace.")
            namespace = 'std'
            header = 'unordered_map'
        elif conf.CheckType('std::tr1::unordered_map<int, int>', language = 'CXX', includes="#include <unordered_map>"):
            print("-- Found unordered_map/set in std::tr1 namespace.")
            namespace = 'std::tr1'
            header = 'unordered_map'
        else:
            print("-- Found <unordered_map> but can not find neither std::unordered_map nor std::tr1::unordered_map.")
    elif conf.CheckCXXHeader("tr1/unordered_map"):
        print("-- Found unordered_map/set in std::tr1 namespace.")
        namespace = 'std::tr1'
        header = 'tr1/unordered_map'
    else:
        print("-- Unable to find <unordered_map> or <tr1/unordered_map>. ")

    conf.env['WITH_UNORDERED_MAP_SUPPORT'] = namespace and header
    conf.env['UNORDERED_MAP_NAMESPACE'] = namespace
    conf.env['UNORDERED_MAP_HEADER'] = header
