@echo off
rem Development launcher: venv -> deps -> backend checks -> Qt build -> launch.
rem Usage: run.bat [/nolaunch | /push]
rem   /nolaunch = build and check only
rem   /push     = push the current branch to its upstream (Git only; no build, no launch, never stages or commits)
rem Overrides (env): QT_DIR, QT_TOOLS
setlocal EnableExtensions
cd /d "%~dp0"

set "APP_EXE=ImageResolutionAnalyzer.exe"
set "VENV=.venv"
set "BUILD=build"
set "LOGDIR=logs"
set "LOG=%LOGDIR%\run.log"
if not defined QT_DIR set "QT_DIR=D:\Apps\Qt6\6.12.0\mingw_64"
if not defined QT_TOOLS set "QT_TOOLS=D:\Apps\Qt6\Tools"
set "NOLAUNCH=0"
if /i "%~1"=="/nolaunch" set "NOLAUNCH=1"
if /i "%~1"=="/push" goto :gitpush

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
echo [%date% %time%] run.bat started> "%LOG%"

rem 1-2. Stop only this project's running app (matched by exact image name).
taskkill /F /IM "%APP_EXE%" >nul 2>&1
rem taskkill exit 128 = not running (fine); anything else non-zero is a real failure.
if errorlevel 129 call :fail 10 "Could not stop running %APP_EXE%"& exit /b 10
if errorlevel 1 if not errorlevel 128 call :fail 10 "Could not stop running %APP_EXE%"& exit /b 10

rem 3. Python (3.11+) -- found on PATH, never downloaded.
set "PY="
where python >nul 2>&1 && set "PY=python"
if not defined PY where py >nul 2>&1 && set "PY=py -3"
if not defined PY call :fail 20 "Python not found. Install Python 3.11+ from python.org."& exit /b 20
%PY% -c "import sys; sys.exit(0 if sys.version_info >= (3,11) else 1)" >>"%LOG%" 2>&1
if errorlevel 1 call :fail 21 "Python 3.11+ required."& exit /b 21

rem 4. Virtual environment: create once, reuse afterwards.
if not exist "%VENV%\Scripts\python.exe" (
    echo Creating virtual environment...
    %PY% -m venv "%VENV%" >>"%LOG%" 2>&1
    if errorlevel 1 call :fail 22 "Failed to create %VENV%"& exit /b 22
)
set "VPY=%CD%\%VENV%\Scripts\python.exe"

rem 5. Install dependencies only when requirements.txt changed since last install.
set "STAMP=%VENV%\requirements.installed.txt"
fc /b requirements.txt "%STAMP%" >nul 2>&1
if errorlevel 1 (
    echo Installing dependencies...
    "%VPY%" -m pip install -r requirements.txt >>"%LOG%" 2>&1
    if errorlevel 1 call :fail 30 "pip install failed"& exit /b 30
    copy /y requirements.txt "%STAMP%" >nul
)

rem 6. Engine checks.
"%VPY%" -m scripts.engine --selfcheck >>"%LOG%" 2>&1
if errorlevel 1 call :fail 40 "Backend selfcheck failed"& exit /b 40
"%VPY%" -m unittest discover -s tests >>"%LOG%" 2>&1
if errorlevel 1 call :fail 41 "Backend tests failed"& exit /b 41

rem 7. Build Qt GUI (configure once, incremental build afterwards).
set "PATH=%QT_TOOLS%\mingw1310_64\bin;%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%QT_DIR%\bin;%PATH%"
if not exist "%QT_DIR%\lib\cmake\Qt6\Qt6Config.cmake" call :fail 50 "Qt not found at %QT_DIR% (set QT_DIR)"& exit /b 50
if not exist "%BUILD%\CMakeCache.txt" (
    cmake -S gui -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%QT_DIR%" >>"%LOG%" 2>&1
    if errorlevel 1 call :fail 51 "CMake configure failed"& exit /b 51
)
cmake --build "%BUILD%" >>"%LOG%" 2>&1
if errorlevel 1 call :fail 52 "Qt build failed"& exit /b 52
ctest --test-dir "%BUILD%" --output-on-failure >>"%LOG%" 2>&1
if errorlevel 1 call :fail 53 "GUI tests failed"& exit /b 53

rem 8. Launch (backend location is passed to the GUI through the environment).
if "%NOLAUNCH%"=="1" (
    echo Build OK. Skipping launch.
    exit /b 0
)
set "IMGAN_PYTHON=%VPY%"
set "IMGAN_BACKEND_DIR=%CD%"
start "" "%BUILD%\%APP_EXE%"
exit /b 0

:fail
rem %1 = code, %2 = message. Prints the message and the tail of the log.
echo [%date% %time%] ERROR %~1: %~2>> "%LOG%"
echo.
echo ERROR %~1: %~2
echo --- last log lines (full log: %LOG%) ---
powershell -NoProfile -Command "Get-Content -Tail 25 '%LOG%'"
exit /b %~1

:gitpush
rem Explicit, separate Git operation. Exit codes: 0 pushed / nothing to push, 60 git missing, 61 not a repository,
rem 62 no branch (detached HEAD), 63 uncommitted changes, 64 no upstream, 65 git query failed, 66 push failed.
rem Never stages, commits, force-pushes, or touches the build. Normal launching does not need Git.
where git >nul 2>&1
if errorlevel 1 echo ERROR 60: Git is not installed or not on PATH.& exit /b 60
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 echo ERROR 61: this folder is not a Git repository.& exit /b 61
set "BRANCH="
for /f "delims=" %%b in ('git symbolic-ref --short -q HEAD 2^>nul') do set "BRANCH=%%b"
if not defined BRANCH echo ERROR 62: detached HEAD - check out a branch before pushing.& exit /b 62
echo Branch: %BRANCH%
set "DIRTY="
for /f "delims=" %%l in ('git status --porcelain 2^>nul') do set "DIRTY=1"
if defined DIRTY (
    echo ERROR 63: the working tree has uncommitted changes. Review and commit them first, then run: run.bat /push
    echo Nothing was staged, committed or pushed.
    git status --short
    exit /b 63
)
set "UPSTREAM="
for /f "delims=" %%u in ('git rev-parse --abbrev-ref --symbolic-full-name "@{u}" 2^>nul') do set "UPSTREAM=%%u"
if not defined UPSTREAM (
    echo ERROR 64: branch %BRANCH% has no upstream. Set it once yourself, e.g.: git push -u origin %BRANCH%
    exit /b 64
)
set "AHEAD="
for /f "delims=" %%n in ('git rev-list --count "@{u}..HEAD" 2^>nul') do set "AHEAD=%%n"
if not defined AHEAD echo ERROR 65: could not compare %BRANCH% with %UPSTREAM%.& exit /b 65
if "%AHEAD%"=="0" echo Nothing to push: %BRANCH% is up to date with %UPSTREAM% ^(as of the last fetch^).& exit /b 0
echo Pushing %AHEAD% commit^(s^) from %BRANCH% to %UPSTREAM% ...
git push
if errorlevel 1 echo ERROR 66: git push failed ^(see the messages above; nothing was forced^).& exit /b 66
echo Push OK.
exit /b 0
