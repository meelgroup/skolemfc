# Copyright (c) 2017, Mate Soos
# Copyright (C) 2024, Arijit Shaw, Brendan Juba, and Kuldeep S. Meel
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required (VERSION 3.24 FATAL_ERROR)

enable_language( CXX )

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

message(STATUS "LIB directory is '${CMAKE_INSTALL_LIBDIR}'")
message(STATUS "BIN directory is '${CMAKE_INSTALL_BINDIR}'")

if(POLICY CMP0048)
    #policy for VERSION in cmake 3.0
    cmake_policy(SET CMP0048 NEW)
endif()


if(POLICY CMP0022)
    cmake_policy(SET CMP0022 NEW)
endif()

if(POLICY CMP0046)
    cmake_policy(SET CMP0046 NEW)
endif()

if(POLICY CMP0026)
    cmake_policy(SET CMP0026 NEW)
endif()

if(POLICY CMP0104)
    cmake_policy(SET CMP0104 NEW)
endif()


project (skolemfc)

# -----------------------------------------------------------------------------
# Make RelWithDebInfo the default build type if otherwise not set
# -----------------------------------------------------------------------------
set(build_types Debug Release RelWithDebInfo MinSizeRel)
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "You can choose the type of build, options are:${build_types}")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Options are ${build_types}"
        FORCE
    )

    # Provide drop down menu options in cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${build_types})
endif()
message(STATUS "Doing a ${CMAKE_BUILD_TYPE} build")

# contains some library search cmake scripts
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

option(BUILD_SHARED_LIBS "Build the shared library" ON)
option(STATICCOMPILE "Compile to static executable" OFF)
if (STATICCOMPILE)
    set(BUILD_SHARED_LIBS OFF)
    set(Boost_USE_STATIC_LIBS ON)
endif()

if ((${CMAKE_SYSTEM_NAME} MATCHES "Linux") OR (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
    if(NOT BUILD_SHARED_LIBS)
        MESSAGE(STATUS "Compiling for static library use")
        if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -Wl,--whole-archive -ldl -Wl,--no-whole-archive -static ")
            #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        endif()

        SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

        # removing -rdynamic that's automatically added
        foreach (language CXX C)
            set(VAR_TO_MODIFY "CMAKE_SHARED_LIBRARY_LINK_${language}_FLAGS")
            string(REGEX REPLACE "(^| )-rdynamic($| )"
                                 " "
                                 replacement
                                 "${${VAR_TO_MODIFY}}")
            #message("Original (${VAR_TO_MODIFY}) is ${${VAR_TO_MODIFY}} replacement is ${replacement}")
            set(${VAR_TO_MODIFY} "${replacement}" CACHE STRING "Default flags for ${build_config} configuration" FORCE)
        endforeach()
    else()
        add_definitions(-DBOOST_TEST_DYN_LINK)
        MESSAGE(STATUS "Compiling for dynamic library use")
    endif()
endif()

include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)
macro(add_cxx_flag_if_supported flagname)
  check_cxx_compiler_flag("${flagname}" HAVE_FLAG_${flagname})

  if(HAVE_FLAG_${flagname})
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flagname}" )
  endif()
endmacro()
macro(add_c_flag_if_supported flagname)
  check_c_compiler_flag("${flagname}" HAVE_FLAG_${flagname})

  if(HAVE_FLAG_${flagname})
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flagname}" )
  endif()
endmacro()

# Note: O3 gives slight speed increase, 1 more solved from SAT Comp'14 @ 3600s
if (NOT MSVC)
    add_compile_options( -g)
    add_compile_options( -pthread )

    add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:-O3>")
    add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:-mtune=native>")

    add_compile_options("$<$<CONFIG:RELEASE>:-O3>")
    add_compile_options("$<$<CONFIG:RELEASE>:-g0>")
    add_compile_options("$<$<CONFIG:RELEASE>:-DNDEBUG>")
    add_compile_options("$<$<CONFIG:RELEASE>:-mtune=native>")

    add_compile_options("$<$<CONFIG:DEBUG>:-O0>")

else()
    # see https://msdn.microsoft.com/en-us/library/fwkeyyhe.aspx for details
    # /ZI = include debug info
    # /Wall = all warnings

    add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:/Ox>")
    add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:/ZI>")

    add_compile_options("$<$<CONFIG:RELEASE>:/Ox>")
    add_compile_options("$<$<CONFIG:RELEASE>:/D>")
    add_compile_options("$<$<CONFIG:RELEASE>:/NDEBUG>")
    add_compile_options("$<$<CONFIG:RELEASE>:/ZI>")

    add_compile_options("$<$<CONFIG:DEBUG>:/Od>")

    if (STATICCOMPILE)
        # We statically link to reduce dependencies
        foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            # /MD -- Causes the application to use the multithread-specific and DLL-specific version of the run-time library.
            #        Defines _MT and _DLL and causes the compiler to place the library name MSVCRT.lib into the .obj file.
            if(${flag_var} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MD")

            # /MDd	-- Defines _DEBUG, _MT, and _DLL and causes the application to use the debug multithread-specific and DLL-specific version of the run-time library.
            #          It also causes the compiler to place the library name MSVCRTD.lib into the .obj file.
            if(${flag_var} MATCHES "/MDd")
                string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
            endif(${flag_var} MATCHES "/MDd")
        endforeach(flag_var)

        # Creates a multithreaded executable (static) file using LIBCMT.lib.
        add_compile_options(/MT)
    endif()

    # buffers security check
    add_compile_options(/GS)

    # Proper warning level
    add_compile_options(/W1)

    # Disable STL used in DLL-boundary warning
    add_compile_options(/wd4251)
    add_compile_options(/D_CRT_SECURE_NO_WARNINGS)

    # Wall is MSVC's Weverything, so annoying unless used from the start
    # and with judiciously used warning disables
    # add_compile_options(/Wall)

    # /Za = only ansi C98 & C++11
    # /Za is not recommended for use, not tested, etc.
    # see: http://stackoverflow.com/questions/5489326/za-compiler-directive-does-not-compile-system-headers-in-vs2010
    # add_compile_options(/Za)

    add_compile_options(/fp:precise)

    # exception handling. s = The exception-handling model that catches C++ exceptions only and tells the compiler to assume that functions declared as extern "C" may throw an exception.
    # exception handling. c = If used with s (/EHsc), catches C++ exceptions only and tells the compiler to assume that functions declared as extern "C" never throw a C++ exception.
    add_compile_options(/EHsc)


    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /PDBCOMPRESS")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:1572864")

    #what does this do?
    set(DEF_INSTALL_CMAKE_DIR CMake)
endif()

# -----------------------------------------------------------------------------
# Option to enable/disable assertions
# -----------------------------------------------------------------------------
# Filter out definition of NDEBUG from the default build configuration flags.
# We will add this ourselves if we want to disable assertions
foreach (build_config ${build_types})
    string(TOUPPER ${build_config} upper_case_build_config)
    foreach (language CXX C)
        set(VAR_TO_MODIFY "CMAKE_${language}_FLAGS_${upper_case_build_config}")
        string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )"
                             " "
                             replacement
                             "${${VAR_TO_MODIFY}}")
        #message("Original (${VAR_TO_MODIFY}) is ${${VAR_TO_MODIFY}} replacement is ${replacement}")
        set(${VAR_TO_MODIFY} "${replacement}" CACHE STRING "Default flags for ${build_config} configuration" FORCE)
    endforeach()
endforeach()

option(ENABLE_ASSERTIONS "Build with assertions enabled" ON)
message(STATUS "build type is ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ENABLE_ASSERTIONS OFF)
endif()

if (ENABLE_ASSERTIONS)
    # NDEBUG was already removed.
else()
    # Note this definition doesn't appear in the cache variables.
    add_definitions(-DNDEBUG)
    add_cxx_flag_if_supported("-fno-stack-protector")
    add_definitions(-D_FORTIFY_SOURCE=0)
endif()

# -----------------------------------------------------------------------------
# Flags
# -----------------------------------------------------------------------------
if (NOT WIN32)
    if(NOT ENABLE_TESTING AND ${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND NOT COVERAGE)
        #dd_cxx_flag_if_supported("-fvisibility=hidden")
    endif()
    add_compile_options("-fPIC")

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        #add_cxx_flag_if_supported("-flto")
    else()
        add_cxx_flag_if_supported("-mtune=native")
        add_cxx_flag_if_supported("-Wall")
        # add_cxx_flag_if_supported("-Wconversion") # enable for implicit conversion show
        add_cxx_flag_if_supported("-Wextra")
        add_cxx_flag_if_supported("-Wunused")
        add_cxx_flag_if_supported("-Wsign-compare")
        if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
            add_cxx_flag_if_supported("-fno-omit-frame-pointer")
        endif()
        add_cxx_flag_if_supported("-Wtype-limits")
        add_cxx_flag_if_supported("-Wuninitialized")
        add_cxx_flag_if_supported("-Wno-deprecated")
        add_cxx_flag_if_supported("-Wstrict-aliasing")
        add_cxx_flag_if_supported("-Wpointer-arith")
        add_cxx_flag_if_supported("-Wheader-guard")
        add_cxx_flag_if_supported("-Wpointer-arith")
        add_cxx_flag_if_supported("-Wformat-nonliteral")
        add_cxx_flag_if_supported("-Winit-self")
        add_cxx_flag_if_supported("-Wparentheses")
        add_cxx_flag_if_supported("-Wunreachable-code")
        add_cxx_flag_if_supported("-ggdb3")
    endif()
endif()

if (NOT WIN32)
    MESSAGE(STATUS "${CMAKE_C_FLAGS}")
    add_c_flag_if_supported("-fPIC")
    add_c_flag_if_supported("-std=c99")
    add_c_flag_if_supported("-Wall")
    add_c_flag_if_supported("-msse4.1")
    add_c_flag_if_supported("-mpclmul")
endif()

# generate JSON file of compile commands -- useful for code extension
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(SANITIZE "Use Clang sanitizers. You MUST use clang++ as the compiler for this to work" OFF)
if (SANITIZE)
    MESSAGE(WARNING " --Using clang sanitizers -- you MUST use clang++ or the compile WILL fail")
    SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}  -lasan -lubsan " )
    SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lasan -lubsan ")
    add_compile_options("-fsanitize=address")
    # add_compile_options("-fsanitize=integer")
    # add_compile_options("-fsanitize=undefined")

    add_compile_options("-fsanitize=null")
    add_compile_options("-fsanitize=alignment")
    add_compile_options("-fno-sanitize-recover")

    add_compile_options("-fsanitize=return")
    add_compile_options("-fsanitize=bounds")
    add_compile_options("-fsanitize=float-divide-by-zero")
    add_compile_options("-fsanitize=integer-divide-by-zero")
    # add_compile_options("-fsanitize=unsigned-integer-overflow")
    # add_compile_options("-fsanitize=signed-integer-overflow")
    add_compile_options("-fsanitize=bool")
    add_compile_options("-fsanitize=enum")
    add_compile_options("-fsanitize=float-cast-overflow")

    #add_compile_options("-Weverything")
    #add_compile_options("-Wshorten-64-to-32")
    #add_compile_options("-Wweak-vtables")
    #add_compile_options("-Wsign-conversion")
    #add_compile_options("-Wconversion")
endif()


# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
set (boost_components "")
set(boost_components ${boost_components} program_options serialization)
find_package(Boost 1.46 COMPONENTS ${boost_components} REQUIRED)

option(NOZLIB "Don't use zlib" OFF)
if (NOT NOZLIB AND NOT (STATICCOMPILE AND WIN32))
    find_package(ZLIB)
    IF (ZLIB_FOUND)
        MESSAGE(STATUS "OK, Found ZLIB!")
        include_directories(${ZLIB_INCLUDE_DIR})
        link_directories(${ZLIB_LIB_DIR})
        add_definitions( -DUSE_ZLIB )
    ELSE (ZLIB_FOUND)
        MESSAGE(STATUS "WARNING: Did not find ZLIB, gzipped file support will be disabled")
    ENDIF (ZLIB_FOUND)
endif()

find_library(GMP_LIB gmp REQUIRED)
find_library(GMPXX_LIB gmpxx REQUIRED)

option(NOUNIGEN "Don't try to use UniSamp" OFF)

# -----------------------------------------------------------------------------
# Meelgroup dependency chain: louvain -> cryptominisat5 -> arjun -> approxmc -> unigen
#
# AUTO_DOWNLOAD=ON  : clone each dep from GitHub into build/deps/<name> at
#                     cmake configure time, build and install to build/deps_install/
# AUTO_DOWNLOAD=OFF : use local deps/<name> submodules if present, same build/install
#                     falls back to system find_package if submodules are absent
#
# Use ./configure.sh -D to pass AUTO_DOWNLOAD=ON automatically.
# Each dep is built in its own isolated cmake process to avoid target-name
# collisions between the projects.  A stamp file prevents redundant rebuilds.
# -----------------------------------------------------------------------------
option(AUTO_DOWNLOAD "Clone and build all dependencies into build/deps/ (requires internet)" OFF)

set(DEPS_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps_install" CACHE PATH
    "Installation prefix for bundled dependencies")

include(ProcessorCount)
ProcessorCount(_nproc)
if(_nproc EQUAL 0)
    set(_nproc 4)
endif()

# _build_dep(name src_dir [extra cmake args...])
# Configures, builds, and installs a dependency into DEPS_INSTALL_DIR.
# A stamp file prevents re-running unless deps_install is wiped.
function(_build_dep _name _src_dir)
    set(_build_dir "${CMAKE_BINARY_DIR}/deps_build/${_name}")
    set(_stamp     "${DEPS_INSTALL_DIR}/.stamp_${_name}")

    if(EXISTS "${_stamp}")
        message(STATUS "[deps] ${_name}: already built (delete ${_stamp} to rebuild)")
        return()
    endif()

    message(STATUS "[deps] Configuring ${_name} from ${_src_dir} ...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -S "${_src_dir}"
            -B "${_build_dir}"
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
            -DCMAKE_PREFIX_PATH=${DEPS_INSTALL_DIR}
            # Make the linker find previously-installed dep shared libs.
            # (The Config.cmake.in files in these projects do not call
            # find_dependency(), so transitive IMPORTED targets may not be
            # available.  Providing -L lets the linker resolve them.)
            "-DCMAKE_SHARED_LINKER_FLAGS=-L${DEPS_INSTALL_DIR}/lib"
            "-DCMAKE_EXE_LINKER_FLAGS=-L${DEPS_INSTALL_DIR}/lib"
            -DCMAKE_INSTALL_RPATH=${DEPS_INSTALL_DIR}/lib
            -DCMAKE_BUILD_RPATH=${DEPS_INSTALL_DIR}/lib
            -DENABLE_TESTING=OFF
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            # Suppress man-page generation: FALSE skips find_program re-search
            # and cmake's if() treats "FALSE" as false (unlike "-NOTFOUND" values
            # which trigger a re-search in some cmake versions).
            -DHELP2MAN_FOUND:STRING=FALSE
            ${ARGN}
        RESULT_VARIABLE _ret
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE  _err
    )
    if(_ret)
        message(FATAL_ERROR
            "[deps] cmake configure of '${_name}' failed (exit ${_ret}).\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()

    message(STATUS "[deps] Building and installing ${_name} ...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            --build "${_build_dir}"
            --target install
            --parallel ${_nproc}
        RESULT_VARIABLE _ret
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE  _err
    )
    if(_ret)
        message(FATAL_ERROR
            "[deps] cmake build of '${_name}' failed (exit ${_ret}).\n"
            "stdout:\n${_out}\nstderr:\n${_err}")
    endif()

    file(TOUCH "${_stamp}")
    message(STATUS "[deps] ${_name}: done")
endfunction()

# _clone_dep(name url tag dest_dir)
# Shallow-clones a git repository if the destination does not already exist.
function(_clone_dep _name _url _tag _dest)
    if(NOT EXISTS "${_dest}/.git")
        message(STATUS "[deps] Cloning ${_name} from ${_url} (${_tag}) ...")
        find_package(Git QUIET REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch "${_tag}" "${_url}" "${_dest}"
            RESULT_VARIABLE _ret
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE  _err
        )
        if(_ret)
            message(FATAL_ERROR
                "[deps] git clone of '${_name}' failed (exit ${_ret}).\n${_err}")
        endif()
    else()
        message(STATUS "[deps] ${_name}: source already present at ${_dest}")
    endif()
endfunction()

if(AUTO_DOWNLOAD OR EXISTS "${CMAKE_SOURCE_DIR}/deps/louvain-community/CMakeLists.txt")

    if(AUTO_DOWNLOAD)
        set(_DEPS_SRC "${CMAKE_BINARY_DIR}/deps")
        message(STATUS "AUTO_DOWNLOAD: deps source → ${_DEPS_SRC}, install → ${DEPS_INSTALL_DIR}")
        _clone_dep(louvain-community
            "https://github.com/meelgroup/louvain-community.git" "master"
            "${_DEPS_SRC}/louvain-community")
        _clone_dep(cryptominisat
            "https://github.com/msoos/cryptominisat.git" "master"
            "${_DEPS_SRC}/cryptominisat")
        _clone_dep(arjun
            "https://github.com/meelgroup/arjun.git" "master"
            "${_DEPS_SRC}/arjun")
        _clone_dep(approxmc
            "https://github.com/meelgroup/approxmc.git" "master"
            "${_DEPS_SRC}/approxmc")
        _clone_dep(unigen
            "https://github.com/meelgroup/unigen.git" "master"
            "${_DEPS_SRC}/unigen")
    else()
        set(_DEPS_SRC "${CMAKE_SOURCE_DIR}/deps")
        message(STATUS "Using local deps/ submodules from ${_DEPS_SRC}/")
    endif()

    # Build in strict dependency order; each dep can find previously installed ones
    # via CMAKE_PREFIX_PATH=${DEPS_INSTALL_DIR}.
    _build_dep(louvain-community "${_DEPS_SRC}/louvain-community")
    _build_dep(cryptominisat     "${_DEPS_SRC}/cryptominisat")
    _build_dep(arjun             "${_DEPS_SRC}/arjun")
    _build_dep(approxmc         "${_DEPS_SRC}/approxmc")
    if(NOT NOUNIGEN)
        _build_dep(unigen        "${_DEPS_SRC}/unigen")
    endif()

    # Point find_package to the freshly installed deps
    list(PREPEND CMAKE_PREFIX_PATH "${DEPS_INSTALL_DIR}")

endif()

# Now find the packages (from DEPS_INSTALL_DIR or system, whichever comes first)
find_package(louvain_communities CONFIG)
find_package(cryptominisat5      CONFIG REQUIRED)
find_package(arjun               CONFIG REQUIRED)
find_package(approxmc            CONFIG REQUIRED)
if(NOT NOUNIGEN)
    find_package(unigen CONFIG)
endif()

if(NOT cryptominisat5_FOUND)
    message(FATAL_ERROR
        "CryptoMiniSat5 not found and deps/ submodules are not initialised.\n"
        "Fix with one of:\n"
        "  git submodule update --init && cmake ..\n"
        "  ./configure.sh -D   (auto-downloads everything)\n")
endif()
if(NOT approxmc_FOUND)
    message(FATAL_ERROR
        "ApproxMC not found.\n"
        "Fix with: git submodule update --init && cmake ..\n"
        "  or:     ./configure.sh -D\n")
endif()

message(STATUS "CryptoMiniSat5 libs:     ${CRYPTOMINISAT5_LIBRARIES}")
message(STATUS "CryptoMiniSat5 includes: ${CRYPTOMINISAT5_INCLUDE_DIRS}")
message(STATUS "Arjun libs:              ${ARJUN_LIBRARIES}")
message(STATUS "ApproxMC libs:           ${APPROXMC_LIBRARIES}")
if(unigen_FOUND)
    message(STATUS "UniGen libs:             ${UNIGEN_LIBRARIES}")
    message(STATUS "UniGen includes:         ${UNIGEN_INCLUDE_DIRS}")
    include_directories(SYSTEM ${UNIGEN_INCLUDE_DIRS})
endif()

# add compile defines
set(COMPILE_DEFINES)
foreach( d ${DirDefs} )
    # message( STATUS "Found Define: " ${d} )
    set(COMPILE_DEFINES "${COMPILE_DEFINES} -D${d}")
endforeach()
message(STATUS "All defines at startup: ${COMPILE_DEFINES}")

# -----------------------------------------------------------------------------
# Add GIT version
# -----------------------------------------------------------------------------
function(SetVersionNumber PREFIX VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
  set(${PREFIX}_VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
  set(${PREFIX}_VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
  set(${PREFIX}_VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
  set(${PREFIX}_VERSION
        "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
        PARENT_SCOPE)
endfunction()

find_program (GIT_EXECUTABLE git)
if (GIT_EXECUTABLE)
  include(GetGitRevisionDescription)
  get_git_head_revision(GIT_REFSPEC GIT_SHA1)
  MESSAGE(STATUS "GIT hash found: ${GIT_SHA1}")
else()
  set(GIT_SHA "GIT-hash-notfound")
endif()
set(SKOLEMFC_FULL_VERSION "1.0.1")

string(REPLACE "." ";" SKOLEMFC_FULL_VERSION_LIST ${SKOLEMFC_FULL_VERSION})
SetVersionNumber("PROJECT" ${SKOLEMFC_FULL_VERSION_LIST})
MESSAGE(STATUS "PROJECT_VERSION: ${PROJECT_VERSION}")
MESSAGE(STATUS "PROJECT_VERSION_MAJOR: ${PROJECT_VERSION_MAJOR}")
MESSAGE(STATUS "PROJECT_VERSION_MINOR: ${PROJECT_VERSION_MINOR}")
MESSAGE(STATUS "PROJECT_VERSION_PATCH: ${PROJECT_VERSION_PATCH}")


add_subdirectory(src)
