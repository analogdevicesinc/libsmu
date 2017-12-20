rem Run test suite on Windows.
rem
rem Executes all test applications within the current directory in alphabetical
rem order. If a test fails the process waits on a keypress to continue.
rem
rem Note that all test executables must be named "test-testname.exe" in order
rem to be included in the test run.

@echo off
for %%F in ("test-*.exe") do (
	"%%F" || pause
)
pause
