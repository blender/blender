# Corrosion Tests

Corrosions tests are run via ctest. The tests themselves utilize CMake script mode
to configure and build a test project, which allows for great flexibility.
Using ctest properties such as `PASS_REGULAR_EXPRESSION` or `FAIL_REGULAR_EXPRESSION`
can be used to confirm that built executable targets run as expected, but can also
be used to fail tests if Corrosion warnings appear in the configure output.