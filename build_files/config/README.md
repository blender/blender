Pipeline Config
===============

The `yaml` configuration file is used by buildbot build pipeline `update-code` step.

The file allows to set branches or specific commits for both git submodules and svn artifacts. Can also define various build package versions for use by build workers. Especially useful in experimental and release branches. 

NOTE:
* The configuration file is ```NOT``` used by the `../utils/make_update.py` script.
* That will implemented in the future.
