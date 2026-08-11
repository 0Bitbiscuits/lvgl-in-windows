cmake_minimum_required(VERSION 3.16)
project(lvgl_playground LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ============================================================
# 运行时输出目录：所有可执行文件 + 所需 DLL 统一放到 runtime/
# 这样 build/ 完全是中间产物，随时可删不影响运行
# ============================================================
set(RUNTIME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/runtime")
file(MAKE_DIRECTORY "${RUNTIME_OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}")
foreach(CFG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} "${RUNTIME_OUTPUT_DIR}")
endforeach()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

message(STATUS "SDL2 include: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 libs:    ${SDL2_LIBRARIES}")
message(STATUS "Runtime dir:  ${RUNTIME_OUTPUT_DIR}")

add_executable(lvgl_playground
    main.c
)

target_include_directories(lvgl_playground PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

target_compile_options(lvgl_playground PRIVATE
    ${SDL2_CFLAGS_OTHER}
)

add_subdirectory(lvgl)

target_link_libraries(lvgl_playground PRIVATE
    lvgl
    ${SDL2_LIBRARIES}
)

if(WIN32)
    target_link_libraries(lvgl_playground PRIVATE
        mingw32
        m
    )

    # ============================================================
    # Post-build：自动把运行时 DLL 从 MSYS2 / MinGW bin 目录拷贝到 runtime/
    # 用到的几个 DLL：
    #   1) SDL2.dll           (图形后端)
    #   2) libgcc_s_seh-1.dll (GCC 运行时, 异常处理)
    #   3) libstdc++-6.dll    (lvgl 内部有 C++ 代码时用)
    #   4) libwinpthread-1.dll(MinGW pthread)
    #   5) zlib1.dll          (freetype/png 解码依赖)
    #   6) libpng16-16.dll    (libpng)
    #   7) libfreetype-6.dll  (freetype 字体引擎)
    #   8) libbrotlidec.dll / libbrotlicommon.dll (freetype -> woff2/woff 字依赖)
    #   9) libharfbuzz-0.dll  (freetype 复杂文本布局依赖)
    #  10) libgraphite2.dll   (harfbuzz 依赖)
    #  11) libglib-2.0-0.dll / libpcre2-8-0.dll (harfbuzz -> glib 依赖)
    # 找不到的 DLL 会自动跳过不报错
    # ============================================================
    if(MINGW)
        # 根据 SDL2_LIBDIR 反推 msys2/mingw 的 bin 目录
        # SDL2_LIBDIR 通常形如 xxx/lib，bin 就是 xxx/bin
        cmake_path(SET _SDL_LIB_DIR NORMALIZE "${SDL2_LIBDIR}")
        cmake_path(GET _SDL_LIB_DIR PARENT_PATH _MINGW_ROOT)
        set(MINGW_BIN_DIR "${_MINGW_ROOT}/bin")

        message(STATUS "Detected MinGW bin dir: ${MINGW_BIN_DIR}")

        set(_RUNTIME_DLLS
            "SDL2.dll"
            "libgcc_s_seh-1.dll"
            "libstdc++-6.dll"
            "libwinpthread-1.dll"
            "zlib1.dll"
            "libpng16-16.dll"
            "libfreetype-6.dll"
            "libbrotlidec.dll"
            "libbrotlicommon.dll"
            "libharfbuzz-0.dll"
            "libgraphite2.dll"
            "libglib-2.0-0.dll"
            "libpcre2-8-0.dll"
            "libintl-8.dll"
            "libiconv-2.dll"
        )

        foreach(_DLL ${_RUNTIME_DLLS})
            set(_SRC "${MINGW_BIN_DIR}/${_DLL}")
            if(EXISTS "${_SRC}")
                add_custom_command(TARGET lvgl_playground POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_SRC}"
                        "${RUNTIME_OUTPUT_DIR}/"
                    COMMENT "Copy runtime DLL: ${_DLL}"
                )
            else()
                message(STATUS "Skip missing DLL: ${_DLL}")
            endif()
        endforeach()
    endif()
endif()

