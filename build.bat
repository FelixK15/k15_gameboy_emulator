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

echo Searching for Visual Studio installation...
setlocal enableextensions enabledelayedexpansion

set PROJECT_NAME=k15_win32_gb_emulator_%BUILD_CONFIG%
set C_FILE_NAME=k15_win32_gb_emulator.cpp

set COMPILER_OPTIONS=/nologo /TP /MTd /W3 /Fe!PROJECT_NAME!.exe
set LINKER_OPTIONS=/PDB:!PROJECT_NAME!.pdb

if "%BUILD_CONFIG%"=="debug" (
	echo Build config = debug
	set COMPILER_OPTIONS=!COMPILER_OPTIONS! /Od /Z7
) else (
	echo Build config = optimized release
	set COMPILER_OPTIONS=!COMPILER_OPTIONS! /O2 /Z7
)

set CL_OPTIONS=!COMPILER_OPTIONS! /link !LINKER_OPTIONS!

set FOUND_PATH=0
set VS_PATH=

::check whether this is 64bit windows or not
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set OS=32BIT || set OS=64BIT

IF %OS%==64BIT set REG_FOLDER=HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7
IF %OS%==32BIT set REG_FOLDER=HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7

::Go to end if nothing was found
IF %REG_FOLDER%=="" GOTO DECISION

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
			goto DECISION
		)
	)
)

:DECISION
::check if a path was found
IF !FOUND_PATH!==0 (
	echo Could not find valid Visual Studio installation.
) ELSE (
	echo Starting build process...
	set VCVARS_PATH="!VS_PATH!VC\vcvarsall.bat"

	call !VCVARS_PATH! >nul 2>nul

	if !errorlevel! neq 0 (
		set VCVARS_PATH="!VS_PATH!VC\Auxiliary\Build\vcvarsall.bat"
		call !VCVARS_PATH! x86 >nul 2>nul
	)

	set CL_PATH="cl.exe"
	set BUILD_COMMAND=!CL_PATH! !C_FILE_NAME! !CL_OPTIONS!
	call !BUILD_COMMAND!
) 