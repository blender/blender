
Test Utilities
==============

These tests are not intended to run as part of automated unit testing,
rather they can be used to expose issues though stress testing or other less predictable
actions that aren't practical to include in unit tests.

Examples include:

- Loading many blend files from a directory, which can expose issues in file reading.
- Running operators in various contexts which can expose crashes.
- Simulating user input for so ``git bisect`` can be performed on bugs that require user interaction.
- Fuzz testing file importers & file format support.

Note that we could make reduced versions of these tests into unit tests at some point.
