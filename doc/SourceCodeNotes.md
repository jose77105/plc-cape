SOURCE CODE NOTES {#source_code_notes}
=================

## DEBUGGING

Tracing points added by means of TRACE-series macros. For more information go to the @ref TRACE
declaration.

Examples:

	// Trace an unexpected error
	TRACE(1, "Unexpected error")
	
	// Trace normal helper messages
	TRACE(2, "File not found! Recreated")

	// Trace normal helper messages
	TRACE(3, "Controller started")
	
	
How the tracing is routed is component-specific, relying on the declaration of a _plc_trace_
function. For example we can just logs the tracing messages to normal output, to stderr, to a
specific file, etc.

TRACEs are only included in DEBUG builds (when DEBUG defined).

... TO BE CONTINUED ...

## DOCUMENTATION

The source code has been documented following DOXYGEN rules and patterns.
There are two types of information:
* generic done in parallel *.md files in Doxygen-specific MarkDown format
* embedded in source code (*.c and *.h) to describe the most important functions, mainly the ones 
  that are public and that can be used on plugin development


## ORGANIZATION

For simpler maintenance source code has been based on logical base paths:

In Windows Operating Systems we have used logical units:
* X: Project source root code
* Y: Project generable output (binaries, Doxygen documentation, etc.)

In Linux Operating Systems we have used soft symbolic link:
* /media/x: equivalent to X:
* /media/y: equivalent to Y:

This indirection can be found in:
* in some documentation (as in the main _PlcCape.pdf_) because in some pages we have copied the
  content from Doxygen. In the Copy/Paste process LibreOffice has converted the relative Hyperlinks
  to absolute ones, based on _Y:_. If you want these links to become active you need to map the 
  _Y:_ drive and put there the corresponding Doxygen-generated documentation

In the project source code (*.c, *.h) you won't see references to these units as the _include_
paths are always relative paths to the project root folder.

In the _makefile_ files the X: and Y: indirection is managed by the environment variables
$DEV_SRC_DIR and $DEV_BIN_DIR.

If you use some IDE (as Visual Studio or Eclipse) it's quite probable they use absolute paths
for the references to the project files. In these cases you can see both indirection ways (X and Y
units and $DEV_SRC_DIR and $DEV_BIN_DIR environment variables) depending on the context.
