include(oblo_conan)

option(OBLO_ENABLE_ASSERT "Enables internal asserts" OFF)

define_property(GLOBAL PROPERTY oblo_3rdparty_targets BRIEF_DOCS "3rd party targets" FULL_DOCS "List of 3rd party targets")

macro(oblo_remove_cxx_flag _option_regex)
    string(TOUPPER ${CMAKE_BUILD_TYPE} _build_type)

    string(REGEX REPLACE "${_option_regex}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "${_option_regex}" "" CMAKE_CXX_FLAGS_${_build_type} "${CMAKE_CXX_FLAGS_${_build_type}}")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_${_build_type} "${CMAKE_CXX_FLAGS_${_build_type}}" PARENT_SCOPE)
endmacro(oblo_remove_cxx_flag)

function(oblo_init_compiler_settings)
    if(MSVC)
        # Warning as errors
        add_compile_options(/W4 /WX)

        # Disable optimizations if specified
        if(OBLO_DISABLE_COMPILER_OPTIMIZATIONS)
            oblo_remove_cxx_flag("(\/O([a-z])?[0-9a-z])")
            add_compile_options(/Od)
        endif()
    else()
        message(SEND_ERROR "Not supported yet")
    endif()

    if(OBLO_ENABLE_ASSERT)
        add_definitions(-DOBLO_ENABLE_ASSERT)
    endif()
endfunction(oblo_init_compiler_settings)

function(oblo_find_source_files)
    file(GLOB_RECURSE _src src/*.cpp)
    file(GLOB_RECURSE _private_includes src/*.hpp)
    file(GLOB_RECURSE _public_includes include/*.hpp)
    file(GLOB_RECURSE _test_src test/*.cpp test/*.hpp)

    set(_oblo_src ${_src} PARENT_SCOPE)
    set(_oblo_private_includes ${_private_includes} PARENT_SCOPE)
    set(_oblo_public_includes ${_public_includes} PARENT_SCOPE)
    set(_oblo_test_src ${_test_src} PARENT_SCOPE)
endfunction(oblo_find_source_files)

function(oblo_add_source_files target)
    target_sources(${target} PRIVATE ${_oblo_src} ${_oblo_private_includes} PUBLIC ${_oblo_public_includes})
endfunction(oblo_add_source_files)

function(oblo_setup_source_groups target)
    source_group("Private\\Source" FILES ${_oblo_src})
    source_group("Private\\Headers" FILES ${_oblo_private_includes})
    source_group("Public\\Headers" FILES ${_oblo_public_includes})
endfunction(oblo_setup_source_groups)

function(oblo_setup_include_dirs target)
    target_include_directories(
        ${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_include_directories(
        ${target} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
endfunction(oblo_setup_include_dirs)

function(oblo_add_executable name)
    set(_target "${name}")
    oblo_find_source_files()
    add_executable(${_target})
    oblo_add_source_files(${_target})
    oblo_setup_include_dirs(${_target})
    oblo_setup_source_groups(${_target})
    add_executable("oblo::${name}" ALIAS ${_target})
endfunction(oblo_add_executable target)

function(oblo_add_library name)
    set(_target "oblo_${name}")
    oblo_find_source_files()

    if(NOT DEFINED _oblo_src)
        # Header only library
        add_library(${_target} INTERFACE)

        target_include_directories(
            ${_target} INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )

        target_sources(${_target} INTERFACE ${_oblo_public_includes})

        add_custom_target(${_target}-interface SOURCES ${_oblo_public_includes})
    else()
        # Regular C++ library
        add_library(${_target})
        oblo_add_source_files(${_target})
        oblo_setup_include_dirs(${_target})
    endif()

    if(DEFINED _oblo_test_src)
        set(_test_target "oblo_test_${name}")
        add_executable(${_test_target} ${_oblo_test_src})
        target_link_libraries(${_test_target} ${_target} 3rdparty::gtest)

        add_executable("oblo::test::${name}" ALIAS ${_test_target})
    endif()

    add_library("oblo::${name}" ALIAS ${_target})
    oblo_setup_source_groups(${_target})
endfunction(oblo_add_library target)

function(oblo_3rdparty_create_aliases)
    get_property(_targets GLOBAL PROPERTY oblo_3rdparty_targets)

    foreach(_target ${_targets})
        add_library("3rdparty::${_target}" ALIAS ${_target})
    endforeach(_target ${_targets})
endfunction(oblo_3rdparty_create_aliases)
