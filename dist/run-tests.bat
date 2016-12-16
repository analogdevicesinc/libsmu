rem Run test suite on Windows.
rem
rem Executes all test applications within the current directory.
rem
rem Note that all test executables must be named "testname-test.exe" in order
rem to be run.

for /r "." %%a in (*-test.exe) do start /b "" "%%~fa"
