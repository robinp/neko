Compilation for x64
===================

Built from the Windows SDK 7.1 command prompt, using with 'SetEnv /Release /x64 /xp'
unless noted otherwise.

NekoVM
------
In the 'vm' directory:
   msbuild nekovm_dll64.vcproj /p:configuration=release
   msbuild nekovm64.vcproj /p:configuration=release

Zlib ndll
---------
In the 'libs\zlib' directory:
   msbuild zlib64.vcproj /p:configuration=release

Std ndll
--------
In the 'libs\std' directory:
   msbuild std64.vcproj /p:configuration=release

(The following are included in binary form, notes are here for reproducibility)

Boehm GC 7.1
------------
Decompress the stable archive and copy the makefile there, then:
   nmake NT_X64_THREADS_RELEASE_MAKEFILE
   
Zlib 1.2.5
----------
In the 'contrib\masmx64' directory:
   bld_ml64.bat
   
In the 'contrib\vstudio\vc9' directory:
   msbuild zlibvc.sln /p:configuration=release,platform=x64