function(loadVersion version)
    set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${VERSION_FILE})
    file(READ "${VERSION_FILE}" read_version)
    string(STRIP ${read_version} read_version)
    set(${version} ${read_version} PARENT_SCOPE)
endfunction()

macro(setVariableInParent variable value)
    get_directory_property(hasParent PARENT_DIRECTORY)

    if (hasParent)
        set(${variable} "${value}" PARENT_SCOPE)
    else ()
        set(${variable} "${value}")
    endif ()
endmacro()
