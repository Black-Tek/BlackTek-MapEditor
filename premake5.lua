workspace "Black-Tek-Mapeditor"
   configurations { "Debug", "Release" }
   platforms { "64" }
   location ""
   editorintegration "On"

   project "Black-Tek-Mapeditor"
      kind "WindowedApp"
      language "C++"
      cppdialect "C++20"
      targetdir "%{wks.location}"
      objdir "build/%{cfg.buildcfg}/obj"
      location ""
      files { "source/**.cpp", "source/**.h" }
      flags { "LinkTimeOptimization", "MultiProcessorCompile" }
      vectorextensions "AVX"

      -- Add wxWidgets, zlib, fmt, OpenGL, GLUT, and wxGL dependencies for Linux
      filter "system:linux"
         includedirs { "/usr/include/wx-3.2" } -- Optional, as wx-config usually handles this
         buildoptions { "`wx-config --cxxflags`" }
         linkoptions { "`wx-config --libs`", "-lwx_gtk3u_aui-3.2", "-lwx_gtk3u_gl-3.2", "-lz", "-lfmt", "-lGL", "-lglut" }
      filter {}

      filter "configurations:Debug"
         defines { "DEBUG" }
         symbols "On"
         optimize "Debug"
      filter {}

      filter "configurations:Release"
         defines { "NDEBUG" }
         symbols "On"
         optimize "Speed"
      filter {}

      filter "platforms:64"
         architecture "amd64"
      filter {}

      filter "system:not windows"
         buildoptions { "-Wall", "-Wextra", "-pedantic", "-pipe", "-fvisibility=hidden", "-Wno-unused-local-typedefs" }
      filter {}

      filter "system:windows"
         openmp "On"
         characterset "MBCS"
         debugformat "c7"
         -- vsprops { VcpkgEnableManifest = "true" } -- Requires premake5-alpha
         buildoptions { "/bigobj", "/utf-8" }
         linkoptions { "/IGNORE:4099" }
      filter {}

      filter "toolset:gcc"
         buildoptions { "-fno-strict-aliasing" }
         buildoptions { "-std=c++20" }
      filter {}

      filter "toolset:clang"
         buildoptions { "-Wimplicit-fallthrough", "-Wmove" }
      filter {}

      filter { "system:macosx", "action:gmake" }
         buildoptions { "-fvisibility=hidden" }   
      filter {}

      intrinsics "On"
