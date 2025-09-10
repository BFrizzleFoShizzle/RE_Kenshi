# RE_Kenshi
This project uses Git submodules. To automatically clone submodules:  
`git clone --recursive https://github.com/BFrizzleFoShizzle/RE_Kenshi.git`  

# Compiling
Some instructions are excerpt from: https://github.com/allenk/WinSDK71_VisualStudio2019  
You will likely not need all the steps, some are there to troubleshoot if you run into something, I marked them with **(T)** *and put it in intalic*

Clone the repo (make sure submodules are cloned too)  

## Install Visual Studio 2019/2022

## Download the Windows SDK 7.1. [Microsoft Windows SDK for Windows 7 and .NET Framework 4 (ISO)](https://www.microsoft.com/en-us/download/details.aspx?id=8442)

In our case we need the **GRMSDKX_EN_DVD.iso. (64bit one)**

**Don't run the default installer. setup.exe in ISO folder, because it will ask for some obsolete version of .NET, in stead Run \setup\SdkSetup.exe.**

**Unselect Visual C++ Compilers, and unselect redistributable Packages.**

the corresponding env vars are:

**WindowsSDK_Include** c:\Program Files\Microsoft SDKs\Windows\v7.1\Include\

**WindowsSDKDir** c:\Program Files\Microsoft SDKs\Windows\v7.1\

## Install [Windows VC 2010 SP1 Update for SDK 7.1 Microsoft Visual C++ 2010 Service Pack 1 Compiler Update for the Windows SDK 7.1 (VC-Compiler-KB2519277.exe).](https://www.microsoft.com/en-US/download/details.aspx?id=4422)

the corresponding env vars are:

add to **PATH**: C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE

**VS100COMNTOOLS** C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\Tools


## Install [Microsoft Visual C++ 2010 Service Pack 1 Redistributable Package MFC Security Update](https://www.microsoft.com/en-us/download/details.aspx?id=26999)

## Install the DirectX June 2010 SDK https://www.microsoft.com/en-us/download/details.aspx?id=6812  

the corresponding env vars are: (these should be added to system env vars by the installer, but if for some reason its missing)

**DXSDK_DIR** C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\

## Registry entries
Make sure the registry is set up correctly (you can even make a new text file rename it something.reg put these in and just run it)
```
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\10.0]
"Source Directories"="C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\crt\\src\\;;;"

[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\10.0\\Setup]

[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\10.0\\Setup\\VC]
"ProductDir"="C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\"

[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\10.0\\Setup\\VS]
"ProductDir"="C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\"
```

**(T)** *if for any reason you get an error like: files missing "",
then it is likely you have some other windows skd installed as well, and somehow it messed up the registry.
The problem is likely that the version has the wrong folder (x86), which has no sdk in such location, so make sure to set it to normal Program Files folder (remove x86 from the path) like below*
```
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows]
"CurrentVersion"="7.1.7600.0.30514"
"CurrentInstallFolder"="C:\\Program Files\\Microsoft SDKs\\Windows\\v7.1"
```

## Install Boost 1.60.0 https://www.boost.org/users/history/version_1_60_0.html  
If you are following this readme (means you don't have a visual studio 2010 professional or ultimate installed), you are likely to have a few extra steps compiling boost.

First you have to have the correct setup for the 64 bit install script, you have to make the bootstrap.bat use the files that is in the amd64 folder instead of the 32 bit ones:
c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64\

There is no vcvars64.bat file which sets up the 64 bit env vars for us by default, so we have to make one.
There is a vcvars32.bat file in the c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\ copy it into the c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64\ and rename it to vcvars64.bat

Change the following lines (you have to add the \amd64 for some paths):

```
@if exist "%VCINSTALLDIR%BIN" set PATH=%VCINSTALLDIR%BIN;%PATH%
TO
@if exist "%VCINSTALLDIR%BIN" set PATH=%VCINSTALLDIR%BIN\amd64;%PATH%

BOTH PLACES
 @if exist "%VCINSTALLDIR%LIB" set LIB=%VCINSTALLDIR%LIB;%LIB%
TO
 @if exist "%VCINSTALLDIR%LIB" set LIB=%VCINSTALLDIR%LIB\amd64;%LIB%
``` 
You will have to use this file from the command line to start bootstrap.bat in the boost folder

To make life easier i made an install.bat file that does that and sets some libs up

```
set include=%include%;C:\Program Files\Microsoft SDKs\Windows\v7.1\Include
set lib=%lib%;C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib\x64
set path=%path%;C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin

call "c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64\vcvars64.bat"
set lib=c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64;%lib%
call "bootstrap.bat"
:: call b2.exe address-model=64
```

**(T)** *it is useful to echo out these env vars if something is not working like echo %lib% from console.
For example for me something set the 32 bit libs to the libs folder, thats why i included the 64bit ones again in my setup bat in the beginning.
If you get linker errors saying it is conflicting with architexture x86, it is likely the same in your case, so check that the 64 bit ones are first in the lib var.*

run b2.exe **address-model=64** from console

add **BOOST_INCLUDE_PATH** to environment vars should point to the boost_1_60_0\ folder

## OgreDeps
**most of the ogredeps are included in the repo as binaries, but if you by any reason want to compile them(same win7.1 sdk, vs10, 64bit) here is the repo:**
https://github.com/OGRECave/ogre-next-deps **at commit: 391e992**
pull and checkout that commit above, and configure cmake to use the above mentioned tools

**At the moment for compress tools you need Freeimage form this repo, the other deps are included, but might be included in the future too.**

### At this point kenshilib should compile YAY!!!
**This might be enough for modding in the future if a c++ modding api based on kenshilib ends up being made.**

## RE-Kenshi
For the newer branch to proceed, you need the **ATL/MFC** header/libs from the **visual studio 2010 Ptofesisonal vs ultimate**, there is no way around this to my knowledge.
put them into the VS10 include dir: c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\Include

At this point you can open the project and compile.

# Manual installation
Copy RE_Kenshi.dll to your kenshi install dir  
Open "Plugins_x64.cfg" in your kenshi install directory and add the line:  
`Plugin=RE_Kenshi`  
after:  
`Plugin=Plugin_Terrain_x64`  
Run Kenshi and the mod will be loaded automatically.  

# License
This project is currently GPLv3, but I may downgrade it to LGPL in the future. No promises though.
