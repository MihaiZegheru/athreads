@echo off
setlocal

set "PYTHON_EXE=C:\msys64\ucrt64\bin\python.exe"

if not exist "%PYTHON_EXE%" (
    echo Could not find %PYTHON_EXE%
    echo Install MSYS2 UCRT Python with Tk support, or edit this launcher.
    exit /b 1
)

"%PYTHON_EXE%" "%~dp0cpu_task_manager.py" %*
