@echo off

:: Set PYTHONBUILDDIR to the root of your python directory
:: Or, if you just want to build the installer for PythonScript with an existing python27.dll,
:: set PYTHONBUILDDIR to a path containing a PCBuild directory, which contains the python27.dll


SET ORIGINALDIR=%CD%

CD /d %~dp0

IF NOT EXIST "buildPaths.bat" (
	echo You need to copy/rename buildPaths.bat.orig to buildPaths.bat, and edit it to your local configuration
	goto error
	)

CALL buildPaths.bat

SET PYTHONSCRIPTVERSION=1.0.0.0


echo Generating WiX information for ..\pythonlib\full
heat dir ..\pythonlib\full -ag -cg CG_PythonLib -dr D_PythonScript -var var.pylibSource -t changeDirLib.xsl -o temp\fullLib.wxs 
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

echo Compiling python lib WiX source
candle temp\fullLib.wxs -o temp\fullLib.wixobj -dpylibSource=..\pythonlib\full
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

echo Generating WiX information for ..\pythonlib\extra
heat dir ..\pythonlib\extra -ag -cg CG_PythonExtraLib -dr D_PythonScript -var var.pylibSource -t changeDirLib.xsl -o temp\extra.wxs
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

echo Compiling extra lib WiX source
candle temp\extra.wxs -o temp\extra.wixobj -dpylibSource=..\pythonlib\extra
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

echo Generating WiX information for ..\pythonlib\tcl
heat dir ..\pythonlib\tcl -ag -cg CG_PythonTclTkLib -dr D_PythonScript -var var.pylibSource -t changeDirLib.xsl -o temp\tcl.wxs
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

echo Compiling tcl lib WiX source
candle temp\tcl.wxs -o temp\tcl.wixobj -dpylibSource=..\pythonlib\tcl
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)



echo Generating WiX information for ..\PythonScript\python_tests
heat dir ..\PythonScript\python_tests -ag -cg CG_UnitTests -dr D_PythonScript -var var.unittestSource -t changeDirTests.xsl -o temp\unittests.wxs
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

echo Compiling Unit test WiX source
candle temp\unittests.wxs -o temp\unittests.wixobj -dunittestSource=..\pythonscript\python_tests
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)



echo Compiling main PythonScript installer
candle pythonscript.wxs -o temp\pythonscript.wixobj -dversion=%PYTHONSCRIPTVERSION% -dbaseDir=.. -dpythonDir=l:\code\cpython
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)


echo Linking installer - generating MSI
light temp\pythonscript.wixobj temp\fullLib.wixobj temp\extra.wixobj temp\unittests.wixobj temp\tcl.wixobj -o build\PythonScript_%PYTHONSCRIPTVERSION%.msi -ext WixUIExtension
if NOT [%ERRORLEVEL%]==[0] (
 goto error
)

goto end

:error
echo Error!

:end

CD /d %ORIGINALDIR%

