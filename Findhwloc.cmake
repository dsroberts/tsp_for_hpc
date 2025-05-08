if(NOT HWLOC_ROOT)
  set(HWLOC_ROOT $ENV{HWLOC_ROOT})
endif()

find_path(hwloc_INCLUDE_DIR hwloc.h
  HINTS ${HWLOC_ROOT} 
  PATH_SUFFIXES include
)
mark_as_advanced(hwloc_INCLUDE_DIR)

if (hwloc_INCLUDE_DIR)
  file(STRINGS ${hwloc_INCLUDE_DIR}/hwloc/autogen/config.h _ver_line
    REGEX "^#define HWLOC_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
    LIMIT_COUNT 1 )
  string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
    hwloc_VERSION "${_ver_line}")
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