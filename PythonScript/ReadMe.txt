Boost::Python build commands
Debug variant (with python debugging (python26_d.dll)
bjam variant=debug link=static runtime-link=static threading=multi python-debugging=on

Debug variant (without python debugging (python26.dll)
bjam variant=debug link=static runtime-link=static threading=multi


Release variant 
bjam variant=release link=static runtime-link=static threading=multi


========================================================================
    DYNAMIC LINK LIBRARY : PythonScript2010 Project Overview
========================================================================

AppWizard has created this PythonScript2010 DLL for you.

This file contains a summary of what you will find in each of the files that
make up your PythonScript2010 application.


PythonScript2010.vcxproj
    This is the main project file for VC++ projects generated using an Application Wizard.
    It contains information about the version of Visual C++ that generated the file, and
    information about the platforms, configurations, and project features selected with the
    Application Wizard.

PythonScript2010.vcxproj.filters
    This is the filters file for VC++ projects generated using an Application Wizard. 
    It contains information about the association between the files in your project 
    and the filters. This association is used in the IDE to show grouping of files with
    similar extensions under a specific node (for e.g. ".cpp" files are associated with the
    "Source Files" filter).

PythonScript2010.cpp
    This is the main DLL source file.

	When created, this DLL does not export any symbols. As a result, it
	will not produce a .lib file when it is built. If you wish this project
	to be a project dependency of some other project, you will either need to
	add code to export some symbols from the DLL so that an export library
	will be produced, or you can set the Ignore Input Library property to Yes
	on the General propert page of the Linker folder in the project's Property
	Pages dialog box.

/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named PythonScript2010.pch and a precompiled types file named StdAfx.obj.

/////////////////////////////////////////////////////////////////////////////
Other notes:

AppWizard uses "TODO:" comments to indicate parts of the source code you
should add to or customize.

/////////////////////////////////////////////////////////////////////////////
