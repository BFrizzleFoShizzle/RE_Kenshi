# RE_Kenshi
This project uses Git submodules. To automatically clone submodules:  
`git clone --recursive https://github.com/BFrizzleFoShizzle/RE_Kenshi.git`

# Plugin development
If you're looking to develop a RE_Kenshi/KenshiLib plugin, you <ins>**do not have to compile RE_Kenshi or KenshiLib**</ins>, there are precompiled versions of both that have less dependencies to work with.  
Information on plugin development can be found on the [KenshiLib](https://github.com/KenshiReclaimer/KenshiLib/) repo.

# Compiling
## Installing dependencies
#### 

#### Install Visual Studio 2019 or newer and the Visual C++ 2010 x64 compilers
RE_Kenshi/KenshiLib MUST be compiled using the Visual Studio 2010 compiler. Copies of Visual Studio 2010 can be found on the [Wayback Machine](https://archive.org/search?query=visual+studio+2010).

![Image](https://github.com/user-attachments/assets/fd4db477-d0dc-4449-99c9-8b343c95a5a1)

#### Install the DirectX June 2010 SDK https://www.microsoft.com/en-us/download/details.aspx?id=6812
After installing, set up the following environment variables: (these are supposed to be added to system env vars by the installer, but will likely be missing)
**DXSDK_DIR** `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\`

### Install Boost 1.60.0 https://www.boost.org/users/history/version_1_60_0.html  
A precompiled version can be found in the [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps) repo  
add **BOOST_INCLUDE_PATH** to environment vars, pointing to the `boost_1_60_0\` folder

Clone the repo (make sure submodules are cloned too)  

## Ogre
Most of the `ogre` dependencies are included in the repo as binaries, but if for any reason you want to compile them from source (same vs2010, 64bit) here is the repo:
https://github.com/OGRECave/ogre-next-deps **at commit: 391e992**
pull and checkout that commit above, and configure cmake to use the above mentioned tools

**At the moment for compress tools you need Freeimage from this repo, the other deps are included, but might be included in the future too.**

---

### At this point KenshiLib should compile, YAY!!! 🥳 🎉
Note: compile in Release, Debug will probably be broken.

---

## Compiling RE_Kenshi
The latest version of the codebase requires the **ATL/MFC** header/libs from **Visual Studio 2010 Professional** or **Ultimate**, I am not aware of any other public source for these headers.
If you have a different version of Visual Studio 2010 othe WIn7.1 SDK installed, you need to put these headers into the VS10 include dir: `C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\Include`

At this point you can open the project and compile.

# Manual installation (outdated) (more info: https://github.com/BFrizzleFoShizzle/RE_Kenshi/issues/4)
Copy RE_Kenshi.dll to your kenshi install dir  
Open "Plugins_x64.cfg" in your kenshi install directory and add the line:  
`Plugin=RE_Kenshi`  
after:  
`Plugin=Plugin_Terrain_x64`  
Run Kenshi and the mod will be loaded automatically.  

# License
The core codebase is GPLv3, check the dependencies for their respective licenses.
