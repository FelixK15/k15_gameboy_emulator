@echo off

set BUILD_CONFIG=%1
if [%1]==[] (
	echo Missing argument, assuming debug build
	set BUILD_CONFIG=debug
)

if not "%BUILD_CONFIG%"=="debug" if not "%BUILD_CONFIG%"=="release" (
	echo Wrong build config "%BUILD_CONFIG%", assuming debug build
	set BUILD_CONFIG="debug"
)

setlocal enableextensions enabledelayedexpansion

set PROJECT_NAME=k15_win32_gb_emulator_%BUILD_CONFIG%
set C_FILE_NAME=k15_win32_gb_emulator.cpp

::FK: Add /Bt to get a compile performance profile
set COMPILER_OPTIONS=-ferror-limit=900 -fstrict-aliasing --output "!PROJECT_NAME!.exe"

if "%BUILD_CONFIG%"=="debug" (
	echo Build config = debug
	set COMPILER_OPTIONS=!COMPILER_OPTIONS! --debug
) else (
	echo Build config = optimized release
	set COMPILER_OPTIONS=!COMPILER_OPTIONS! -O3 -DK15_RELEASE_BUILD
)

::is cl.exe part of PATH?
where /Q cl.exe
if !errorlevel! == 0 (
	echo Found cl.exe in PATH
	goto START_COMPILATION
)

echo Didn't find clang.exe in PATH - searching for Visual Studio installation...

set FOUND_PATH=0
set VS_PATH=

::check whether this is 64bit windows or not
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set OS=32BIT || set OS=64BIT

IF %OS%==64BIT set REG_FOLDER=HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7
IF %OS%==32BIT set REG_FOLDER=HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7

::Go to end if nothing was found
IF %REG_FOLDER%=="" GOTO PATH_FOUND

::try to get get visual studio path from registry for different versions
FOR /l %%G IN (20, -1, 8) DO (
	set REG_COMMAND=reg query !REG_FOLDER! /v %%G.0
	!REG_COMMAND! >nul 2>nul

	::if errorlevel is 0, we found a valid installDir
	if !errorlevel! == 0 (
		::issue reg command again but evaluate output
		FOR /F "skip=2 tokens=*" %%A IN ('!REG_COMMAND!') DO (
			set VS_PATH=%%A
			::truncate stuff we don't want from the output
			set VS_PATH=!VS_PATH:~18!
			set FOUND_PATH=1
			goto PATH_FOUND
		)
	)
)

:PATH_FOUND
::check if a path was found
IF !FOUND_PATH!==0 (
	echo Could not find valid Visual Studio installation.
) ELSE (
	echo Found Visual Studio installation at !VS_PATH!
	echo Searching and executing vsvars64.bat ...
	set VCVARS_PATH="!VS_PATH!VC\vcvars64.bat"

	call !VCVARS_PATH! >nul 2>nul

	if !errorlevel! neq 0 (
		set VCVARS_PATH="!VS_PATH!VC\Auxiliary\Build\vcvars64.bat"
		call !VCVARS_PATH! x86 >nul 2>nul
	)

:START_COMPILATION
	echo Starting build process...
	set CL_PATH="clang.exe"
	set BUILD_COMMAND=!CL_PATH! !C_FILE_NAME! !COMPILER_OPTIONS!
	call !BUILD_COMMAND!
) 