@ECHO OFF

pushd %~dp0

REM Command file for Sphinx documentation

if "%SPHINXBUILD%" == "" (
	set SPHINXBUILD=python -msphinx
)
set SOURCEDIR=source
set BUILDDIR=build
set SPHINXPROJ=RadeonDeveloperPanel

if "%1" == "" goto help

%SPHINXBUILD% >NUL 2>NUL
if errorlevel 9009 (
	echo.
	echo.The Sphinx module was not found. Make sure you have Sphinx installed,
	echo.then set the SPHINXBUILD environment variable to point to the full
	echo.path of the 'sphinx-build' executable. Alternatively you may add the
	echo.Sphinx directory to PATH.
	echo.
	echo.If you don't have Sphinx installed, grab it from
	echo.http://sphinx-doc.org/
	exit /b 1
)

if "%2" == "" (
   %SPHINXBUILD% -M %1 %SOURCEDIR% %BUILDDIR% %SPHINXOPTS%
) else (
   %SPHINXBUILD% -M %1 %SOURCEDIR% %BUILDDIR% %SPHINXOPTS% -t %2
)
del %BUILDDIR%\html\ProfilingModes.html
goto end

:help
%SPHINXBUILD% -M help %SOURCEDIR% %BUILDDIR% %SPHINXOPTS%

:end
popd
