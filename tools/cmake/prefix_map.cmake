# Utilities for remapping path prefixes
#
# __generate_prefix_map
# Prepares the list of compiler flags for remapping various paths
# to fixed names. This is used when reproducible builds are required.
# This function also creates a gdbinit file for the debugger to
# remap the substituted paths back to the real paths in the filesystem.
function(__generate_prefix_map compile_options_var)
    set(compile_options)
    set(gdbinit_dir ${BUILD_DIR}/gdbinit)
    set(gdbinit_path "${gdbinit_dir}/prefix_map")
    idf_build_get_property(idf_path IDF_PATH)
    idf_build_get_property(build_components BUILD_COMPONENTS)

    if(CONFIG_COMPILER_HIDE_PATHS_MACROS)
        list(APPEND compile_options "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.")
        list(APPEND compile_options "-fmacro-prefix-map=${idf_path}=/IDF")
    endif()

    if(CONFIG_APP_REPRODUCIBLE_BUILD)
        list(APPEND compile_options "-fdebug-prefix-map=${idf_path}=/IDF")
        list(APPEND compile_options "-fdebug-prefix-map=${PROJECT_DIR}=/IDF_PROJECT")
        list(APPEND compile_options "-fdebug-prefix-map=${BUILD_DIR}=/IDF_BUILD")

        # Generate mapping for component paths
        set(gdbinit_file_lines)
        foreach(component_name ${build_components})
            idf_component_get_property(component_dir ${component_name} COMPONENT_DIR)

            string(TOUPPER ${component_name} component_name_uppercase)
            set(substituted_path "/COMPONENT_${component_name_uppercase}_DIR")
            list(APPEND compile_options "-fdebug-prefix-map=${component_dir}=${substituted_path}")
            string(APPEND gdbinit_file_lines "set substitute-path ${substituted_path} ${component_dir}\n")
        endforeach()

        # Mapping for toolchain path
        # For clang, we need to use proper target flags to determine the sysroot
        if(CMAKE_C_COMPILER MATCHES ".*clang.*")
            # Get the target from the compiler flags
            string(REGEX MATCH "--target=([^ ]+)" TARGET_MATCH "${CMAKE_C_FLAGS}")
            if(TARGET_MATCH)
                set(CLANG_TARGET_FLAG "--target=${CMAKE_MATCH_1}")
            else()
                set(CLANG_TARGET_FLAG "--target=riscv32-esp-elf")
            endif()
            
            # Get the march flag
            string(REGEX MATCH "-march=([^ ]+)" MARCH_MATCH "${CMAKE_C_FLAGS}")
            if(MARCH_MATCH)
                set(CLANG_MARCH_FLAG "-march=${CMAKE_MATCH_1}")
            else()
                set(CLANG_MARCH_FLAG "-march=rv32imac_zicsr_zifencei")
            endif()
            
            # Get the mabi flag
            string(REGEX MATCH "-mabi=([^ ]+)" MABI_MATCH "${CMAKE_C_FLAGS}")
            if(MABI_MATCH)
                set(CLANG_MABI_FLAG "-mabi=${CMAKE_MATCH_1}")
            else()
                set(CLANG_MABI_FLAG "-mabi=ilp32")
            endif()
            
            execute_process(
                COMMAND ${CMAKE_C_COMPILER} ${CLANG_TARGET_FLAG} ${CLANG_MARCH_FLAG} ${CLANG_MABI_FLAG} -print-file-name=libc.a
                OUTPUT_VARIABLE compiler_sysroot
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            if(compiler_sysroot AND NOT compiler_sysroot STREQUAL "libc.a")
                # Extract the sysroot from the library path
                get_filename_component(compiler_sysroot "${compiler_sysroot}" DIRECTORY)
                get_filename_component(compiler_sysroot "${compiler_sysroot}/.." REALPATH)
            else()
                # Fallback: use relative path from compiler
                get_filename_component(compiler_sysroot "${CMAKE_C_COMPILER}/../.." REALPATH)
            endif()
        else()
            # For GCC and other compilers, use the original approach
            execute_process(
                COMMAND ${CMAKE_C_COMPILER} -print-sysroot
                OUTPUT_VARIABLE compiler_sysroot
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(compiler_sysroot STREQUAL "")
                # Fallback: use relative path from compiler
                get_filename_component(compiler_sysroot "${CMAKE_C_COMPILER}/../.." REALPATH)
            endif()
        endif()
        list(APPEND compile_options "-fdebug-prefix-map=${compiler_sysroot}=/TOOLCHAIN")
        string(APPEND gdbinit_file_lines "set substitute-path /TOOLCHAIN ${compiler_sysroot}\n")

        file(WRITE "${BUILD_DIR}/prefix_map_gdbinit" "${gdbinit_file_lines}")  # TODO IDF-11667
        idf_build_set_property(DEBUG_PREFIX_MAP_GDBINIT "${gdbinit_path}")
    else()
        set(gdbinit_file_lines "# There is no prefix map defined for the project.\n")
    endif()
    # Write prefix_map_gdbinit file even it is empty.
    file(MAKE_DIRECTORY ${gdbinit_dir})
    file(WRITE "${gdbinit_path}" "${gdbinit_file_lines}")
    idf_build_set_property(GDBINIT_FILES_PREFIX_MAP "${gdbinit_path}")

    set(${compile_options_var} ${compile_options} PARENT_SCOPE)
endfunction()
