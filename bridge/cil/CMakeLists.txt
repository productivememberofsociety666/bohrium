cmake_minimum_required(VERSION 2.8)
set(BRIDGE_CIL true CACHE BOOL "BRIDGE-CIL: Build the CIL-bridge.")
if(NOT BRIDGE_CIL)
    return()
endif()

set(Mono_FIND_QUIETLY 1)
find_package(Mono)

if(BRIDGE_CIL AND MONO_FOUND AND PYTHONINTERP_FOUND)

  # Determine names of output .cs files based on .ctpl input names
  FILE(GLOB files codegen/templates/*.ctpl)
  SET(ctpl_outputs)

  FOREACH(file ${files})
    GET_FILENAME_COMPONENT(basename ${file} NAME_WE)
    GET_FILENAME_COMPONENT(targetfolder ${file} PATH)
    SET(ctpl_outputs ${ctpl_outputs} ${targetfolder}/../output/${basename}.cs)
  ENDFOREACH(file)

  file(GLOB ctpl_sources codegen/templates/*.ctpl codegen/*.json codegen/*.py)

  add_custom_command(
    OUTPUT ${ctpl_outputs}
    COMMAND ${PY_SCRIPT} codegen/gen.py
    DEPENDS ${ctpl_sources}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set(build_type "Release")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(build_type "Debug")
  endif()


  file(GLOB build_deps ${CMAKE_CURRENT_SOURCE_DIR}/NumCIL.Bohrium/*.cs)
  set(target_file ${CMAKE_CURRENT_BINARY_DIR}/NumCIL.Bohrium/bin/${build_type}/NumCIL.Bohrium.dll)
  add_custom_command(
      OUTPUT ${target_file}
      COMMAND ${XBUILD_EXECUTABLE} /property:Configuration=${build_type} /property:OutputPath=${CMAKE_CURRENT_BINARY_DIR}/NumCIL.Bohrium/bin/${build_type} ${CMAKE_CURRENT_SOURCE_DIR}/NumCIL.Bohrium/NumCIL.Bohrium.csproj
      DEPENDS ${ctpl_outputs} ${build_deps} ${CMAKE_CURRENT_SOURCE_DIR}/NumCIL.Bohrium/NumCIL.Bohrium.csproj
  )

  # Add a target, and register a simple name
  add_custom_target(
    numcil_bohrium ALL
    DEPENDS numcil bhc ${target_file}
  )

  install(FILES ${target_file} DESTINATION lib/mono COMPONENT bohrium-numcil)
  install(CODE "execute_process(COMMAND gacutil -i ${CMAKE_INSTALL_PREFIX}/lib/mono/NumCIL.Bohrium.dll)")  

endif()
