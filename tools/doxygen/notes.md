DOXYGEN_NOTES {#doxygen-notes}
=============

*.CONF FILES LOCATION
---------------------
To compile a *.conf _doxygen_ file (e.g. _doxygen_rtf_source_code.conf_) move it to the root folder 
first. That's required because all the paths are relative to that location.

We could change the path settings of the _conf_ file (e.g. INPUT = "../../" instead of INPUT = ".",
and also for EXAMPLE_PAHT and IMAGE_PATH) but there are still references in source code that assume
'root' location. Therefore, the build with these modified paths will produce some 'unresolved
symbol' errors.


OPTIONS
-------

If you want to include all the copyright notes from files add COPYRIGHT_NOTES to the
ENABLED_SECTIONS in 'doxygen.conf' (or to _Build_ > _Enabled Sections_ in the _Expert_ tab if
using _Doxywizard_)

