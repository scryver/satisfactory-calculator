@echo off

set opts=-FC -GR- -EHa- -nologo -Zi -fp:fast -fp:except-
set curDir=%cd%
set code=%curDir%\src
pushd gebouw
cl %opts% %code%\main.cpp -Festatisfactory-calc.exe /link -incremental:no -opt:ref winmm.lib
popd
