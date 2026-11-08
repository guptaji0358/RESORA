@echo off
rem Development launcher: venv -> deps -> backend checks -> Qt build -> launch.
rem Usage: run.bat [/nolaunch | /push | /nocommit]
rem   AUTO-COMMIT (user request 2026-11-08): after a successful build + tests (+ launch) run.bat commits by default.
rem   /nocommit = build/launch without committing
rem   /nolaunch = build and check only (also auto-commits; only one switch is read, so /nolaunch cannot be combined with /nocommit)
rem   /push     = push the current branch to its upstream (Git only; no build, no launch, never stages or commits)
rem   (commit step) after build + launch, commit everything (git add -A) using the LAST message block of commit.txt
rem               (exit 67-69, see :gitcommit). Never pushes. Only runs after build, tests and launch succeeded.
rem Overrides (env): QT_DIR, QT_TOOLS
rem Every stage prints PENDING/RUNNING/DONE/FAILED/SKIPPED. Long commands run through scripts\launcher\runstage.ps1, which
rem streams their real output to the console and to logs\run.log line by line (see :runstage).
setlocal EnableExtensions
cd /d "%~dp0"

set "APP_EXE=ImageResolutionAnalyzer.exe"
set "VENV=.venv"
set "BUILD=build"
set "LOGDIR=logs"
set "LOG=%LOGDIR%\run.log"
set "RUNNER=scripts\launcher\runstage.ps1"
if not defined QT_DIR set "QT_DIR=D:\Apps\Qt6\6.12.0\mingw_64"
if not defined QT_TOOLS set "QT_TOOLS=D:\Apps\Qt6\Tools"
set "NOLAUNCH=0"
set "DOCOMMIT=1"
if /i "%~1"=="/nocommit" set "DOCOMMIT=0"
if /i "%~1"=="/nolaunch" set "NOLAUNCH=1"
if /i "%~1"=="/push" goto :gitpush

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
echo [%date% %time%] run.bat started> "%LOG%"
set "PYTHONUNBUFFERED=1"

rem Stage table (index -> name, status). Status starts as PENDING.
set "TOTAL=9"
set "N1=Python environment detection"
set "N2=Virtual environment"
set "N3=Dependencies"
set "N4=Backend self-check"
set "N5=Python automated tests"
set "N6=CMake configuration"
set "N7=Qt compilation"
set "N8=GUI tests (ctest)"
set "N9=Application launch"
set "LASTDONE=none"
for /l %%i in (1,1,%TOTAL%) do set "S%%i=PENDING"
echo.
echo ================ RESORA run.bat ================
call :summary

rem Stop only this project's running app (matched by exact image name).
taskkill /F /IM "%APP_EXE%" >nul 2>&1
rem taskkill exit 128 = not running (fine); anything else non-zero is a real failure.
if errorlevel 129 call :fail 10 "Could not stop running %APP_EXE%"& exit /b 10
if errorlevel 1 if not errorlevel 128 call :fail 10 "Could not stop running %APP_EXE%"& exit /b 10

rem 1. Python (3.11+) -- found on PATH, never downloaded.
call :begin 1
set "PY="
where python >nul 2>&1 && set "PY=python"
if not defined PY where py >nul 2>&1 && set "PY=py -3"
if not defined PY call :stagefail 1 20 "Python not found. Install Python 3.11+ from python.org."& exit /b 20
set "RS_CMD=%PY% -c "import sys; print(sys.executable, sys.version.split()[0]); sys.exit(max(0, 11 - sys.version_info.minor))""
call :runstage
if errorlevel 1 call :stagefail 1 21 "Python 3.11+ required."& exit /b 21
call :done 1

rem 2. Virtual environment: create once, reuse afterwards.
call :begin 2
if not exist "%VENV%\Scripts\python.exe" (
    set "RS_CMD=%PY% -m venv "%VENV%""
    call :runstage
    if errorlevel 1 call :stagefail 2 22 "Failed to create %VENV%"& exit /b 22
    call :done 2 "created %VENV%"
) else (
    call :done 2 "reusing existing %VENV%"
)
set "VPY=%CD%\%VENV%\Scripts\python.exe"

rem 3. Install dependencies only when requirements.txt changed since last install.
call :begin 3
set "STAMP=%VENV%\requirements.installed.txt"
fc /b requirements.txt "%STAMP%" >nul 2>&1
if errorlevel 1 (
    set "RS_CMD="%VPY%" -m pip install -r requirements.txt"
    call :runstage
    if errorlevel 1 call :stagefail 3 30 "pip install failed"& exit /b 30
    copy /y requirements.txt "%STAMP%" >nul
    call :done 3 "installed"
) else (
    call :skip 3 "requirements.txt unchanged since last install"
)

rem 4-5. Engine checks.
call :begin 4
set "RS_CMD="%VPY%" -m scripts.engine --selfcheck"
call :runstage
if errorlevel 1 call :stagefail 4 40 "Backend selfcheck failed"& exit /b 40
call :done 4
call :begin 5
set "RS_CMD="%VPY%" -m unittest discover -v -s tests"
call :runstage
if errorlevel 1 call :stagefail 5 41 "Backend tests failed"& exit /b 41
call :done 5

rem 6-8. Build Qt GUI (configure once, incremental build afterwards).
set "PATH=%QT_TOOLS%\mingw1310_64\bin;%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%QT_DIR%\bin;%PATH%"
if not exist "%QT_DIR%\lib\cmake\Qt6\Qt6Config.cmake" call :stagefail 6 50 "Qt not found at %QT_DIR% (set QT_DIR)"& exit /b 50
call :begin 6
if not exist "%BUILD%\CMakeCache.txt" (
    set "RS_CMD=cmake -S gui -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%QT_DIR%""
    call :runstage
    if errorlevel 1 call :stagefail 6 51 "CMake configure failed"& exit /b 51
    call :done 6 "configured"
) else (
    call :skip 6 "build directory already configured"
)
call :begin 7
set "RS_CMD=cmake --build "%BUILD%""
call :runstage
if errorlevel 1 call :stagefail 7 52 "Qt build failed"& exit /b 52
call :done 7
call :begin 8
set "RS_CMD=ctest --test-dir "%BUILD%" --output-on-failure"
call :runstage
if errorlevel 1 call :stagefail 8 53 "GUI tests failed"& exit /b 53
call :done 8

rem 9. Launch (backend location is passed to the GUI through the environment).
if "%NOLAUNCH%"=="1" (
    call :skip 9 "/nolaunch"
    call :summary
    echo Build OK. Skipping launch.
    echo [%date% %time%] Build OK, launch skipped>> "%LOG%"
    if "%DOCOMMIT%"=="1" goto :gitcommit
    exit /b 0
)
call :begin 9
set "IMGAN_PYTHON=%VPY%"
set "IMGAN_BACKEND_DIR=%CD%"
start "" "%BUILD%\%APP_EXE%"
call :done 9 "started %BUILD%\%APP_EXE%"
call :summary
echo [%date% %time%] Launched>> "%LOG%"
if "%DOCOMMIT%"=="1" goto :gitcommit
exit /b 0

:begin
rem %1 = stage index. Marks RUNNING and prints a heading with a timestamp.
set "S%~1=RUNNING"
call set "CURN=%%N%~1%%"
set "CURIDX=%~1"
echo.
echo [%time:~0,8%] [%~1/%TOTAL%] RUNNING  %CURN%
echo [%date% %time%] [%~1/%TOTAL%] RUNNING %CURN%>> "%LOG%"
exit /b 0

:done
rem %1 = index, %2 = optional note.
set "S%~1=DONE"
set "LASTDONE=%~1/%TOTAL% %CURN%"
echo [%time:~0,8%] [%~1/%TOTAL%] DONE     %CURN% %~2
echo [%date% %time%] [%~1/%TOTAL%] DONE %CURN% %~2>> "%LOG%"
exit /b 0

:skip
set "S%~1=SKIPPED"
call set "CURN=%%N%~1%%"
echo [%time:~0,8%] [%~1/%TOTAL%] SKIPPED  %CURN% (%~2)
echo [%date% %time%] [%~1/%TOTAL%] SKIPPED %CURN% (%~2)>> "%LOG%"
exit /b 0

:stagefail
rem %1 = stage index, %2 = exit code, %3 = message.
set "S%~1=FAILED"
call set "CURN=%%N%~1%%"
echo [%time:~0,8%] [%~1/%TOTAL%] FAILED   %CURN%
call :fail %~2 "%~3"
exit /b %~2

:runstage
rem Runs %RS_CMD% live (console + log). Returns the command's exit code in ERRORLEVEL.
powershell -NoProfile -ExecutionPolicy Bypass -File "%RUNNER%" -Log "%LOG%"
exit /b %errorlevel%

:summary
echo.
echo Stages:
for /l %%i in (1,1,%TOTAL%) do call :sumline %%i
echo.
exit /b 0

:sumline
call set "SN=%%N%~1%%"
call set "SS=%%S%~1%%"
echo   [%~1/%TOTAL%] %SS%  %SN%
exit /b 0

:fail
rem %1 = code, %2 = message. Prints the failure, the last completed stage and the tail of the log (the live output is above).
echo [%date% %time%] ERROR %~1: %~2>> "%LOG%"
echo.
echo ERROR %~1: %~2
echo Last completed stage: %LASTDONE%
call :summary
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

:gitcommit
rem Explicit opt-in (run.bat /commit). Reached only after build, tests and launch succeeded.
rem Exit codes: 0 committed / nothing to commit, 67 git missing or not a repository, 68 commit.txt missing/empty, 69 commit failed.
rem Stages with `git add -A` (ignored folders .venv/build/logs stay out), message = last block of commit.txt. Never pushes.
where git >nul 2>&1
if errorlevel 1 echo ERROR 67: Git is not installed or not on PATH.& exit /b 67
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 echo ERROR 67: this folder is not a Git repository.& exit /b 67
set "DIRTY="
for /f "delims=" %%l in ('git status --porcelain 2^>nul') do set "DIRTY=1"
if not defined DIRTY echo Nothing to commit.& exit /b 0
set "MSGFILE=%LOGDIR%\commit-message.txt"
powershell -NoProfile -Command "$t=Get-Content -Raw -Encoding UTF8 'commit.txt'; if(-not $t){exit 1}; $b=[regex]::Split($t.Trim(),'\r?\n\r?\n(?=(?:feat|fix|refactor|docs|test|chore|perf|build|style)(?:\([^)]*\))?!?:)'); [IO.File]::WriteAllText('%MSGFILE%',($b[-1].Trim()+[Environment]::NewLine),(New-Object Text.UTF8Encoding $false))" >nul 2>&1
if errorlevel 1 echo ERROR 68: commit.txt is missing or empty.& exit /b 68
git add -A
git commit -F "%MSGFILE%"
if errorlevel 1 echo ERROR 69: git commit failed ^(see messages above^).& exit /b 69
echo Commit OK ^(not pushed; use run.bat /push^).
exit /b 0
