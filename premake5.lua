workspace "psychobox"
  configurations { "Debug", "Release" }
  location "bin"
  system "Windows"
  architecture "x86_64"

project "psychobox"
  kind "WindowedApp"
  language "C++"
  debugdir "."
  includedirs { "src", "third_party/include" }
  files { "src/*.hpp", "src/*.cpp" }
  libdirs {"third_party/lib"}
  links { "user32.lib", "gdi32.lib", "d3d11.lib", "dxgi.lib", "dxguid.lib", "freetype.lib", "assimp-vc143-mt.lib", "Xinput.lib"}
  buildoptions { "/W4", "/sdl", "/MP", "/std:c++20", "/EHsc", "/wd4100" }
  linkoptions { "/SUBSYSTEM:WINDOWS" }

  filter "configurations:Debug"
    targetdir "bin/Debug"
    links { "imguid.obj", "D3DCompiler.lib" }
    buildoptions { "/MDd" }
    -- "/fsanitize=address" for addr sanitizer.
    defines { "_DEBUG" }
    symbols "On"

  filter "configurations:Release"
    targetdir "bin/Release"
    linkoptions { "../icon.res" }
    buildoptions { "/O2", "/MD" }
    optimize "On"