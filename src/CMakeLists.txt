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

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CRYPTOMINISAT5_INCLUDE_DIRS})
include_directories(${ARJUN_INCLUDE_DIRS})
include_directories(${LOUVAIN_COMMUNITIES_INCLUDE_DIRS})
include_directories(${APPROXMC_INCLUDE_DIRS})
include_directories(${UNIGEN_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

if (NOT WIN32)
    add_cxx_flag_if_supported("-Wno-bitfield-constant-conversion")
    #add_cxx_flag_if_supported("-Wduplicated-cond")
    #add_cxx_flag_if_supported("-Wduplicated-branches")
    add_cxx_flag_if_supported("-Wlogical-op")
    add_cxx_flag_if_supported("-Wrestrict")
    add_cxx_flag_if_supported("-Wnull-dereference")
    add_cxx_flag_if_supported("-Wdouble-promotion")
    add_cxx_flag_if_supported("-Wshadow")
    add_cxx_flag_if_supported("-Wformat=2")
    add_cxx_flag_if_supported("-Wextra-semi")
    add_cxx_flag_if_supported("-pedantic")
    #add_cxx_flag_if_supported("-Wdeprecated")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/GitSHA1.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp" @ONLY)


SET(SOURCES
    skolemfc-int.cpp
	skolemfc.cpp
	${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp)

ADD_LIBRARY(skolemfc ${SOURCES} ${LIB})
target_link_libraries (skolemfc
  LINK_PUBLIC ${CRYPTOMINISAT5_LIBRARIES}
  LINK_PUBLIC ${ARJUN_LIBRARIES}
  LINK_PUBLIC ${APPROXMC_LIBRARIES}
  LINK_PUBLIC ${UNIGEN_LIBRARIES}
  LINK_PUBLIC ${LOUVAIN_COMMUNITIES_LIBRARIES}
  LINK_PUBLIC ${Boost_LIBRARIES} # due to serialization
)

set_target_properties(skolemfc PROPERTIES
    PUBLIC_HEADER "${skolemfc_public_headers}"
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

add_executable (skolemfc-bin
    main.cpp
)

set(skolemfc_exec_link_libs
  skolemfc
  ${GMPXX_LIB}
  ${GMP_LIB}
  ${Boost_LIBRARIES} # due to program_options
)

IF (ZLIB_FOUND)
    SET(skolemfc_exec_link_libs ${skolemfc_exec_link_libs} ${ZLIB_LIBRARY})
ENDIF()

target_link_libraries (skolemfc-bin
  ${skolemfc_exec_link_libs}
)

set_target_properties(skolemfc-bin PROPERTIES
    OUTPUT_NAME skolemfc
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE)
