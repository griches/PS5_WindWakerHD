# Wind Waker HD for the PlayStation 5 - what the console adds to the recompilation's own build.
#
# tools/build.sh configures vendor/wwhd itself (its CMakeLists.txt expects to be
# the top level) with the payload SDK's toolchain, and names this file as
# CMAKE_PROJECT_wwhd_recomp_INCLUDE: CMake includes it right after the project()
# call, before the build looks for its dependencies. It provides:
#
#   SDL3     .deps/SDL, patched (tools/setup-sdl.sh), with the drivers in ps5/sdl
#   Vulkan   RADV's headers; the driver itself is linked by tools/link-title.sh
#   zlib     the headers of the zlib inside the RADV archive
#   glslang, LZ4   from the pinned sources upstream's own WWHD_BUNDLED_DEPS names
#
# and makes the executable's link PS5_Vulkan's (tools/link-title.sh, with RADV).
#
# SPDX-License-Identifier: MIT

foreach(required PS5_PORT_DIR PS5_VULKAN_DIR SDL_DIR)
	if(NOT ${required})
		message(FATAL_ERROR "ps5-game.cmake: ${required} is not set (use ps5/tools/build.sh)")
	endif()
endforeach()
set(ps5 ${PS5_PORT_DIR}/ps5)

# -fno-omit-frame-pointer: the console's crash backtraces are frame-pointer walks.
add_compile_options(-g1 -ffunction-sections -fdata-sections -fno-omit-frame-pointer)
# The console's CPU is a Zen 2: the toolchain's default is the first x86-64 (SSE2 and nothing
# since). The recompiled code keeps its exact arithmetic: upstream builds it with
# -ffp-contract=off, so no multiply-add is fused whatever the CPU offers.
add_compile_options(-march=znver2)
# An init array, in the order the source has: tools/cc-init-array.sh has the reason
set(CMAKE_C_COMPILER_LAUNCHER sh ${ps5}/tools/cc-init-array.sh)
set(CMAKE_CXX_COMPILER_LAUNCHER sh ${ps5}/tools/cc-init-array.sh)
# The SDK's libc++ is LLVM 18's, where std::jthread and std::stop_token are still
# experimental: tools/link-title.sh links libc++experimental.a
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>)

# The console: klog, the pad, the title's exit, and the libc names a title cannot import
add_library(ps5_platform STATIC ${ps5}/src/platform.c ${ps5}/src/shims.c ${ps5}/src/startup.c)
target_include_directories(ps5_platform PUBLIC ${ps5}/src)
target_include_directories(ps5_platform PRIVATE ${SDL_DIR}/include)
file(READ ${ps5}/sce_sys/param.json param_json)
string(JSON PS5_TITLE_NAME GET "${param_json}" localizedParameters en-US titleName)
target_compile_definitions(ps5_platform PRIVATE "PS5_TITLE_NAME=\"${PS5_TITLE_NAME}\"")
set_target_properties(ps5_platform PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# By its file, not its target: it is linked whole into the title (below)
set(ps5_platform_archive ${CMAKE_BINARY_DIR}/libps5_platform.a)

# SDL3: threads, time and files as a FreeBSD has them; video, audio and the pad from ps5/sdl
set(SDL_PS5_PORT_DIR ${ps5}/sdl CACHE INTERNAL "")
set(SDL_PS5_PLATFORM_DIR ${ps5}/src CACHE INTERNAL "")
set(SDL_STATIC ON CACHE BOOL "" FORCE)
# no X11 and no Wayland, on purpose
set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
foreach(off SDL_SHARED SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL
		SDL_X11 SDL_WAYLAND SDL_KMSDRM SDL_RPI SDL_ROCKCHIP SDL_VIVANTE SDL_OFFSCREEN SDL_DUMMYVIDEO
		SDL_OPENGL SDL_OPENGLES SDL_VULKAN SDL_RENDER_VULKAN SDL_GPU SDL_RENDER_GPU SDL_CAMERA SDL_OPENVR
		SDL_OSS SDL_ALSA SDL_JACK SDL_PIPEWIRE SDL_PULSEAUDIO SDL_SNDIO SDL_DISKAUDIO SDL_DUMMYAUDIO
		SDL_DBUS SDL_IBUS SDL_LIBURING SDL_LIBUDEV SDL_HIDAPI SDL_HIDAPI_LIBUSB SDL_VIRTUAL_JOYSTICK
		SDL_DLOPEN_NOTES SDL_DEPS_SHARED SDL_ASSEMBLY_UNUSED)
	set(${off} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory(${SDL_DIR} ${CMAKE_BINARY_DIR}/sdl EXCLUDE_FROM_ALL)

# Vulkan: headers only (the renderer loads every function at run time, from SDL's
# vkGetInstanceProcAddr, which is RADV's)
set(Vulkan_INCLUDE_DIR ${PS5_VULKAN_DIR}/.deps/native/radv-release/include CACHE PATH "" FORCE)
set(Vulkan_LIBRARY ${ps5_platform_archive} CACHE FILEPATH "" FORCE)

# zlib is in the RADV archive already (Mesa's own): its headers, and nothing to link
add_library(ps5_zlib INTERFACE)
target_include_directories(ps5_zlib INTERFACE ${PS5_VULKAN_DIR}/.deps/native/zlib/root/usr/include)
add_library(ZLIB::ZLIB ALIAS ps5_zlib)

# glslang and LZ4 from upstream's pinned sources
set(WWHD_BUNDLED_DEPS ON CACHE BOOL "" FORCE)
set(WWHD_RENDERER VULKAN CACHE STRING "" FORCE)

# The link: every object and archive CMake would hand the linker goes to
# PS5_Vulkan's recipe instead (RADV, the platform layer, the CRT, the fake SELF)
set(CMAKE_CXX_LINK_EXECUTABLE
	"bash ${ps5}/tools/link-title.sh ${CMAKE_BINARY_DIR} ${PS5_VULKAN_DIR} ${PS5_PAYLOAD_SDK} --whole-archive ${ps5_platform_archive} --no-whole-archive <OBJECTS> <LINK_LIBRARIES>")
# (the start-up constructor in the platform archive has no caller: hence whole. And first: the
# initialisers run in link order, and the runtime reads options from the environment in its own,
# which /app0/env.txt must have filled by then)

# The platform archive is named by its file in the link, so the game does not
# depend on it by itself (the game's target exists once its CMakeLists.txt has run)
function(ps5_late_fixes)
	add_dependencies(wwhd ps5_platform)
	set_property(TARGET wwhd APPEND PROPERTY LINK_DEPENDS ${ps5_platform_archive})
endfunction()
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL ps5_late_fixes)
