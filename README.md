# RE_Kenshi
This project uses Git submodules. To automatically clone submodules:
git clone --recursive https://github.com/BFrizzleFoShizzle/RE_Kenshi.git

# Compiling
Install Visual Studio 2019
Install the Windows SDK v7.1
Clone the repo (make sure submodules are cloned too)
Open + compile with VC++

# Manual installation
Copy RE_Kenshi.dll to your kenshi install dir
Open "Plugins_x64.cfg" in your kenshi install directory and add the line:
`Plugin=RE_Kenshi`
after:
`Plugin=Plugin_Terrain_x64`
Run Kenshi and the mod will be loaded automatically. 