execute_process (
    COMMAND ${GIT_EXECUTABLE} rev-list --tags --max-count=1
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE EXITCODE1
    OUTPUT_VARIABLE TAGHASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

execute_process (
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE EXITCODE2
    OUTPUT_VARIABLE COMMITHASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

string (COMPARE EQUAL "${EXITCODE1}:${EXITCODE2}" "0:0" FULL_SUCCESS)
string (COMPARE EQUAL "${EXITCODE2}" "0" COMMIT_SUCCESS)
if (FULL_SUCCESS)
    set(OPENMW_VERSION_COMMITHASH "${COMMITHASH}")
    set(OPENMW_VERSION_TAGHASH "${TAGHASH}")
    message(STATUS "OpenMW version ${OPENMW_VERSION}")
elseif (COMMIT_SUCCESS)
    set(OPENMW_VERSION_COMMITHASH "${COMMITHASH}")
    message(STATUS "OpenMW version ${OPENMW_VERSION}")
else ()
    message(WARNING "Failed to get valid version information from Git")
endif ()

# duplicated from OpenMWMacros.cmake
macro (configure_resource_file source_path destination_dir_base dest_path_relative)
	if (MSVC)
		configure_file(${source_path} "${destination_dir_base}/Debug/${dest_path_relative}")
		configure_file(${source_path} "${destination_dir_base}/Release/${dest_path_relative}")
		configure_file(${source_path} "${destination_dir_base}/RelWithDebInfo/${dest_path_relative}")
		configure_file(${source_path} "${destination_dir_base}/MinSizeRel/${dest_path_relative}")
	else (MSVC)
		configure_file(${source_path} "${destination_dir_base}/${dest_path_relative}")
	endif (MSVC)
endmacro (configure_resource_file)

configure_resource_file(${VERSION_IN_FILE} ${VERSION_FILE_PATH_BASE} ${VERSION_FILE_PATH_RELATIVE})
