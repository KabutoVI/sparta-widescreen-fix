call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
cl /nologo /O1 /LD /GS- d3d9log.c /link /DEF:d3d9log.def /NODEFAULTLIB:msvcrt kernel32.lib user32.lib /OUT:d3d9.dll /ENTRY:DllMain
