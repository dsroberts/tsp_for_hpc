if(NOT HWLOC_ROOT)
  set(HWLOC_ROOT $ENV{HWLOC_ROOT})
endif()

find_path(hwloc_INCLUDE_DIR hwloc.h
  HINTS ${HWLOC_ROOT} 
  PATH_SUFFIXES include
)
mark_as_advanced(hwloc_INCLUDE_DIR)

set(_hwloc_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/hwloc)
set(_hwloc_TEST_SRC cmake_hwloc_test.cxx)

if (hwloc_INCLUDE_DIR)
  if (NOT EXISTS ${_hwloc_TEST_DIR}/test_hwloc_version_c)
      file(WRITE "${_hwloc_TEST_DIR}/${_hwloc_TEST_SRC}"
      "#include <hwloc.h>\n"
      "const char* info_ver = \"INFO\" \":\" HWLOC_VERSION;\n"
      "int main(int argc, char **argv) {\n"
      "  int require = 0;\n"
      "  require += info_ver[argc];\n"
      "  return 0;\n"
      "}")
    try_compile(WOKRED SOURCES "${_hwloc_TEST_DIR}/${_hwloc_TEST_SRC}"
      COPY_FILE ${_hwloc_TEST_DIR}/test_hwloc_version_c
    )
  endif()
  if(${WORKED} EXISTS ${_hwloc_TEST_DIR}/test_hwloc_version_c)
  file(STRINGS ${_hwloc_TEST_DIR}/test_hwloc_version_c INFO_STRING
    REGEX "^INFO:"
  )
  string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)(-patch([0-9]+))?"
    hwloc_VERSION "${INFO_STRING}"
  )
  endif()
endif()


# Look for the necessary library
find_library(hwloc_LIBRARY hwloc
  HINTS ${HWLOC_ROOT}
  PATH_SUFFIXES lib lib64
)
mark_as_advanced(hwloc_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(hwloc
    REQUIRED_VARS hwloc_INCLUDE_DIR hwloc_LIBRARY
    VERSION_VAR hwloc_VERSION)

# Create the imported target
if(hwloc_FOUND)
    set(hwloc_INCLUDE_DIRS ${hwloc_INCLUDE_DIR})
    set(hwloc_LIBRARIES ${hwloc_LIBRARY})
    if(NOT TARGET hwloc::hwloc)
        add_library(hwloc::hwloc UNKNOWN IMPORTED)
        set_target_properties(hwloc::hwloc PROPERTIES
            IMPORTED_LOCATION             "${hwloc_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${hwloc_INCLUDE_DIR}")
    endif()
endif()