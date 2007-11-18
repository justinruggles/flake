# Flake CMake project file

# This project file and the included custom modules are based on the Aften
# CMake files written by Prakash Punnoor

CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")
Project(Flake C)

SET(SO_MAJOR_VERSION "0")
SET(SO_MINOR_VERSION "11")
SET(SO_BUILD_VERSION "5")
SET(SO_VERSION "${SO_MAJOR_VERSION}.${SO_MINOR_VERSION}.${SO_BUILD_VERSION}")

IF(${Flake_SOURCE_DIR} MATCHES ${Flake_BINARY_DIR})
  MESSAGE("Please do an out-of-tree build:")
  MESSAGE("rm -f CMakeCache.txt; mkdir -p default; cd default; cmake ..; make")
  MESSAGE(FATAL_ERROR "in-tree-build detected")
ENDIF(${Flake_SOURCE_DIR} MATCHES ${Flake_BINARY_DIR})

INCLUDE(${CMAKE_ROOT}/Modules/TestBigEndian.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/CheckCSourceCompiles.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/ConfigHelper.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/HeaderTests.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/FlagsTests.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/CompilerVisibility.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/DetectCompiler.cmake)

OPTION(SHARED "build shared Flake library" OFF)

INCLUDE_DIRECTORIES(${Flake_BINARY_DIR}/)
INCLUDE_DIRECTORIES(${Flake_SOURCE_DIR}/)
INCLUDE_DIRECTORIES(${Flake_SOURCE_DIR}/libflake)
INCLUDE_DIRECTORIES(${Flake_SOURCE_DIR}/flake)
INCLUDE_DIRECTORIES(${Flake_SOURCE_DIR}/libpcm_io)

SET(LIBFLAKE_SRCS libflake/crc.c
                  libflake/encode.c
                  libflake/lpc.c
                  libflake/md5.c
                  libflake/metadata.c
                  libflake/optimize.c
                  libflake/rice.c
                  libflake/vbs.c)

SET(FLAKE_SRCS flake/flake.c)

SET(LIBPCM_IO_SRCS libpcm_io/byteio.c
                   libpcm_io/convert.c
                   libpcm_io/formats.c
                   libpcm_io/pcm_io.c
                   libpcm_io/raw.c
                   libpcm_io/wav.c)


IF(CMAKE_UNAME)
  EXEC_PROGRAM(uname ARGS -m OUTPUT_VARIABLE CMAKE_SYSTEM_MACHINE)
ELSE(CMAKE_UNAME)
  MESSAGE("Could not detect machine type")
  IF(CMAKE_SYSTEM MATCHES "Windows")
    MESSAGE(STATUS "Assuming i386 machine")
    SET(CMAKE_SYSTEM_MACHINE i386)
  ENDIF(CMAKE_SYSTEM MATCHES "Windows")
ENDIF(CMAKE_UNAME)

DETECT_COMPILER()

IF(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_C_FLAGS_RELEASE "-funroll-loops -fomit-frame-pointer ${CMAKE_C_FLAGS_RELEASE}")
  SET(ADD_CFLAGS "-pedantic -Wall -Wpointer-arith -Wredundant-decls -Wformat -Wunused ${ADD_CFLAGS}")

  # gcc 2.9.5 doesn't know following
  CHECK_CFLAGS_APPEND(ADD_EXCLUSIVE_CFLAGS "-std=c99")
  CHECK_CFLAGS_APPEND(ADD_CFLAGS "-Wdisabled-optimization")
  CHECK_CFLAGS_APPEND(ADD_CFLAGS "-Wfloat-equal")
  CHECK_CFLAGS_APPEND(ADD_EXCLUSIVE_CFLAGS "-Wbad-function-cast")

  # gcc 3.0 doesn't know following
  CHECK_CFLAGS_APPEND(ADD_EXCLUSIVE_CFLAGS "-Wdeclaration-after-statement")
  CHECK_CFLAGS_APPEND(ADD_CFLAGS "-Wextra")
  CHECK_CFLAGS_APPEND(ADD_CFLAGS "-Wno-switch")

  # gcc 4.1 doesn't know following
  CHECK_CFLAGS_APPEND(ADD_CFLAGS "-Wstrict-overflow")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_COMPILER_IS_ICC)
  # icc ignores bogus options, so detection makes no sense
  SET(CMAKE_C_FLAGS_RELEASE "-O3 -funroll-loops -fomit-frame-pointer ${CMAKE_C_FLAGS_RELEASE}")
  SET(ADD_CFLAGS "-std=c99 -Wall ${ADD_CFLAGS}")
ENDIF(CMAKE_COMPILER_IS_ICC)

IF(MSVC)
  # msvc ignores bogus options, so detection makes no sense
  SET(CMAKE_C_FLAGS_RELEASE "/Ot /Oy /Oi /Ox ${CMAKE_C_FLAGS_RELEASE}")
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ENDIF(MSVC)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Release")
ENDIF(NOT CMAKE_BUILD_TYPE)

TEST_BIG_ENDIAN(CMAKE_WORDS_BIGENDIAN)
IF(CMAKE_WORDS_BIGENDIAN)
  ADD_DEFINE(WORDS_BIGENDIAN)
ENDIF(CMAKE_WORDS_BIGENDIAN)

IF(UNIX)
  SET(LIBM "m")

  TEST_COMPILER_VISIBILITY()

  IF(APPLE)
    ADD_DEFINE(SYS_DARWIN)
  ENDIF(APPLE)
ENDIF(UNIX)

ADD_DEFINITIONS(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)

# fail if inttypes.h is not found
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
IF(NOT HAVE_INTTYPES_H)
  MESSAGE("Flake requires inttypes.h and stdlib.h")
  MESSAGE("If you are using MS Visual Studio, try using the headers from")
  MESSAGE("http://code.google.com/p/msinttypes/")
  MESSAGE(FATAL_ERROR "inttypes.h not found")
ENDIF(NOT HAVE_INTTYPES_H)

CHECK_INCLUDE_FILE_DEFINE(byteswap.h HAVE_BYTESWAP_H)
CHECK_FUNCTION_DEFINE("#include <string.h>" "strnlen" "(\"help\", 6)" HAVE_STRNLEN)

# output SVN version to config.h
EXECUTE_PROCESS(COMMAND svn info --xml WORKING_DIRECTORY ${Flake_SOURCE_DIR}
OUTPUT_VARIABLE SVN_INFO ERROR_QUIET)
STRING(REGEX MATCH "<entry[^>]+" SVN_VERSION "${SVN_INFO}")
STRING(REGEX MATCH "revision=\"[0123456789]+\"" SVN_VERSION "${SVN_VERSION}")
STRING(REGEX MATCH "[0123456789]+" SVN_VERSION "${SVN_VERSION}")
IF(SVN_VERSION)
  ADD_DEFINE("SVN_VERSION \"${SVN_VERSION}\"")
ENDIF(SVN_VERSION)

GENERATE_CONFIG_H()

SET(CMAKE_C_FLAGS "${ADD_CFLAGS} ${ADD_EXCLUSIVE_CFLAGS} ${CMAKE_C_FLAGS}")

IF(SHARED)
  ADD_LIBRARY(flake SHARED ${LIBFLAKE_SRCS})
  SET_TARGET_PROPERTIES(flake PROPERTIES VERSION ${SO_VERSION} SOVERSION ${SO_MAJOR_VERSION})
  SET_TARGET_PROPERTIES(flake PROPERTIES LINKER_LANGUAGE C)
  SET_TARGET_PROPERTIES(flake PROPERTIES DEFINE_SYMBOL FLAKE_BUILD_LIBRARY)
  TARGET_LINK_LIBRARIES(flake ${LIBM} ${ADD_LIBS})
  SET(INSTALL_TARGETS ${INSTALL_TARGETS} flake)
ENDIF(SHARED)

ADD_LIBRARY(flake_static STATIC ${LIBFLAKE_SRCS})
SET_TARGET_PROPERTIES(flake_static PROPERTIES LINKER_LANGUAGE C)
SET_TARGET_PROPERTIES(flake_static PROPERTIES COMPILE_FLAGS -DFLAKE_BUILD_LIBRARY)
TARGET_LINK_LIBRARIES(flake_static ${LIBM} ${ADD_LIBS})

# building a separate static lib for the pcm audio decoder
ADD_LIBRARY(pcm_io STATIC ${LIBPCM_IO_SRCS})

ADD_EXECUTABLE(flake_exe ${FLAKE_SRCS})
SET_TARGET_PROPERTIES(flake_exe PROPERTIES OUTPUT_NAME flake)
SET_TARGET_PROPERTIES(flake_exe PROPERTIES LINKER_LANGUAGE C)
IF(WIN32)
  # When linking to static lib, dllimport mustn't be used
  SET_TARGET_PROPERTIES(flake_exe PROPERTIES COMPILE_FLAGS -DFLAKE_BUILD_LIBRARY)
ENDIF(WIN32)
TARGET_LINK_LIBRARIES(flake_exe pcm_io flake_static)

ADD_EXECUTABLE(wavinfo util/wavinfo.c)
TARGET_LINK_LIBRARIES(wavinfo pcm_io)

INSTALL(TARGETS ${INSTALL_TARGETS} flake_exe wavinfo
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
INSTALL(FILES ${INSTALL_HEADERS} libflake/flake.h DESTINATION include/flake)
