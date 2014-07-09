def FindSharedPtr(conf):
    """
    Detect shared_ptr availability
    """

    found = False
    namespace = None
    header = None

    if conf.CheckCXXHeader("memory"):
        # Finding the memory header doesn't mean that shared_ptr is in std
        # namespace.
        #
        # In particular, MSVC 2008 has shared_ptr declared in std::tr1.  In
        # order to support this, we do an extra check to see which namespace
        # should be used.

        if conf.CheckType('std::shared_ptr<int>', language = 'C++', includes="#include <memory>"):
            print("-- Found shared_ptr in std namespace using <memory> header.")
            namespace = 'std'
            header = 'memory'
        elif conf.CheckType('std::tr1::shared_ptr<int>', language = 'C++', includes="#include <memory>"):
            print("-- Found shared_ptr in std::tr1 namespace using <memory> header..")
            namespace = 'std::tr1'
            header = 'memory'

    if not namespace and conf.CheckCXXHeader("tr1/memory"):
        # Further, gcc defines shared_ptr in std::tr1 namespace and
        # <tr1/memory> is to be included for this. And what makes things
        # even more tricky is that gcc does have <memory> header, so
        # all the checks above wouldn't find shared_ptr.
        if conf.CheckType('std::tr1::shared_ptr<int>', language = 'C++', includes="#include <tr1/memory>"):
            print("-- Found shared_ptr in std::tr1 namespace using <tr1/memory> header..")
            namespace = 'std::tr1'
            header = 'tr1/memory'

    if not namespace:
        print("-- Unable to find shared_ptrred_map>.")

    conf.env['WITH_SHARED_PTR_SUPPORT'] = namespace and header
    conf.env['SHARED_PTR_NAMESPACE'] = namespace
    conf.env['SHARED_PTR_HEADER'] = header
