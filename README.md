# Psycho Box

3D sokoban-like puzzle game rendered using DirectX 11.

[Play it on itch.io!](https://lucypero.itch.io/psycho-box)

## Build

Visual Studio and [Premake](https://premake.github.io/) are required to compile the project.

```
premake5 vs2022
```

You can now open the `.sln` file found in `bin` with Visual Studio and compile the project within the GUI.

The command above will generate a solution file for Visual Studio 2022. To generate solutions on other versions of Visual Studio, you can target them. Run `premake5 --help` for a full list of available targets.

`ninja` also works as a target and really well. [Ninja](https://ninja-build.org/) and [premake-ninja](https://github.com/jimon/premake-ninja) are required. Doing it this way does not require having the full Visual Studio program installed, only the Microsoft Command Line [Build Tools](https://visualstudio.microsoft.com/downloads/?q=build+tools#build-tools-for-visual-studio-2022).

## Third party libraries used

- imgui
- freetype
- miniaudio
- assimp
- stb_image