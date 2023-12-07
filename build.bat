@echo off

set DefaultCompilerFlags= -MTd -nologo -GR- -EHa- -Od -Oi -WX -W4 -wd4505 -wd4189 -wd4201 -wd4100 -D UNOPTIMIZED=1 -D INTERNAL=1 -D WIN32=1 -FC -Zi 
set DefaultLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib ole32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 64-bit executable
del *.pdb > NUL 2> NUL
cl %DefaultCompilerFlags% ..\keepmovingforward\src\keepmovingforward.cpp -Fmkeepmovingforward.map -LD /link -incremental:no -opt:ref -PDB:keepmovingforward_%random%.pdb -EXPORT:GameUpdateAndRender
cl %DefaultCompilerFlags% ..\keepmovingforward\src\win32_keepmovingforward.cpp -Fmwin32_keepmovingforward.map /link %DefaultLinkerFlags%
popd
