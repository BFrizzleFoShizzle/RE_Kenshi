# RE_Kenshi
This project uses Git submodules. To automatically clone submodules:  
`git clone --recursive https://github.com/BFrizzleFoShizzle/RE_Kenshi.git`  

# Compiling
Install Visual Studio 2019  
Install the Windows SDK v7.1 - The official installer no longer works on Windows 10+, but the instructions here may work: https://github.com/allenk/WinSDK71_VisualStudio2019  
Install the DirectX June 2010 SDK https://www.microsoft.com/en-us/download/details.aspx?id=6812  
Install Boost 1.60.0 https://www.boost.org/users/history/version_1_60_0.html  
Clone the repo (make sure submodules are cloned too)  
Open + compile with VC++  

# Manual installation
Copy RE_Kenshi.dll to your kenshi install dir  
Open "Plugins_x64.cfg" in your kenshi install directory and add the line:  
`Plugin=RE_Kenshi`  
after:  
`Plugin=Plugin_Terrain_x64`  
Run Kenshi and the mod will be loaded automatically.  

# License
This project is currently GPLv3, but I may downgrade it to LGPL in the future. No promises though.
