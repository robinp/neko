Compilation for x64
===================

Built from the Windows SDK 7.1 command prompt, using with 'SetEnv /Release /x64 /xp'
unless noted otherwise.

Boehm GC 7.1
------------
Decompress the stable archive and copy the makefile there, then:
   nmake NT_X64_THREADS_RELEASE_MAKEFILE

NekoVM
------
In the 'vm' directory:
   msbuild nekovm_dll64.vcproj /p:configuration=release
   msbuild nekovm64.vcproj /p:configuration=release