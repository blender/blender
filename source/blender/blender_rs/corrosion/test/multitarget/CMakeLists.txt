corrosion_tests_add_test(multitarget "bin1;bin2;bin3")

# Don't run this test in parallel with others, since the target directory size may cause issues.
set_tests_properties("multitarget_build" PROPERTIES RUN_SERIAL TRUE)

set_tests_properties("multitarget_run_bin1" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello, world!\r?\nHello, bin1! I'm Cpp!"
        RUN_SERIAL
        TRUE
        )

set_tests_properties("multitarget_run_bin2" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello, world!\r?\nHello, bin2! I'm Cpp!"
        RUN_SERIAL
        TRUE
        )

set_tests_properties("multitarget_run_bin3" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello, world!\r?\nHello, bin3! I'm Cpp!"
        RUN_SERIAL
        TRUE
        )
