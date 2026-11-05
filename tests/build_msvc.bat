@echo off
rem Builds and runs everything with Microsoft's compiler, as C and as C++.
rem Run it from a "Developer Command Prompt", at the root of the project:
rem
rem     tests\build_msvc.bat

setlocal
set FLAGS=/nologo /W3 /utf-8 /I. /Itests /Itutorial\system-monitor /D_CRT_SECURE_NO_WARNINGS
if not exist build\msvc mkdir build\msvc

for %%t in (test_unicode test_buffer test_draw test_render test_parser test_ui test_alloc test_tutorial_stats) do (
    cl %FLAGS% /Fo:build\msvc\ /Fe:build\msvc\%%t.exe tests\%%t.c || exit /b 1
    build\msvc\%%t.exe || exit /b 1

    cl %FLAGS% /TP /EHsc /Fo:build\msvc\ /Fe:build\msvc\%%t_cpp.exe tests\%%t.c || exit /b 1
    build\msvc\%%t_cpp.exe || exit /b 1
)

for %%d in (hello keys dashboard widgets) do (
    cl %FLAGS% /Fo:build\msvc\ /Fe:build\msvc\%%d.exe demo\%%d.c || exit /b 1
)

for %%s in (step1 step2 step3 step4 step5 step6 step7 step8 step9) do (
    cl %FLAGS% /Fo:build\msvc\ /Fe:build\msvc\%%s.exe tutorial\system-monitor\%%s.c || exit /b 1
)

cl %FLAGS% /Fo:build\msvc\ /Fe:build\msvc\e2e_app.exe tests\e2e_app.c || exit /b 1
cl %FLAGS% /Fo:build\msvc\ /Fe:build\msvc\e2e_conpty.exe tests\e2e_conpty.c || exit /b 1
cd build\msvc
e2e_conpty.exe e2e_app.exe || exit /b 1
cd ..\..

echo All MSVC tests passed.
