def test_unordered_map(conf):
    """
    Detect unordered_map availability
    
    Returns (True/False, namespace, include prefix)
    """

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
            return True, 'std', ''
        elif conf.CheckType('std::tr1::unordered_map<int, int>', language = 'CXX', includes="#include <unordered_map>"):
            print("-- Found unordered_map/set in std::tr1 namespace.")
            return True, 'std::tr1', ''
        else:
            print("-- Found <unordered_map> but can not find neither std::unordered_map nor std::tr1::unordered_map.")
            return False, '', ''
    elif conf.CheckCXXHeader("tr1/unordered_map"):
        print("-- Found unordered_map/set in std::tr1 namespace.")
        return True, 'std::tr1', 'tr1/'
    else:
        print("-- Unable to find <unordered_map> or <tr1/unordered_map>. ")
        return False, '', ''
