find_package(wxWidgets 3 REQUIRED std xml html adv aui core net base richtext)

include(${wxWidgets_USE_FILE})


file(GLOB SOURCE_FILES *.cpp *.h)

include(FindPkgConfig)
pkg_search_module(WXSVG libwxsvg wxsvg)
if (WXSVG_FOUND)
  message(STATUS "Building with system wxsvg includes")
  add_library(WXSVG INTERFACE)
  target_include_directories(WXSVG INTERFACE ${WXSVG_INCLUDE_DIR})
  target_link_libraries(WXSVG INTERFACE ${WXSVG_LIBRARIES})
else ()
  file(GLOB_RECURSE WXSVG_FILES "../libs/wxsvg/*.cpp")
  pkg_search_module(LIBCAIRO libcairo cairo)
  if (LIBCAIRO_FOUND)
    message(STATUS "Building with system cairo includes")
    add_library(LIBCAIRO INTERFACE)
    target_include_directories(LIBCAIRO INTERFACE ${CAIRO_INCLUDE_DIR})
    target_link_libraries(LIBCAIRO INTERFACE ${CAIRO_LIBRARIES})
  else()
    file(GLOB_RECURSE CAIRO_FILES "../libs/cairo/*.cpp")
    set(SOURCE_FILES ${CAIRO_FILES} ${SOURCE_FILES})

    pkg_search_module(ZLIB libzlib zlib)
    if (ZLIB_FOUND)
      message(STATUS "Building with system zlib")
      add_library(ZLIB INTERFACE)
      target_include_directories(ZLIB INTERFACE ${ZLIB_INCLUDE_DIR})
      target_link_libraries(ZLIB INTERFACE ${ZLIB_LIBRARIES})
    else()
      file(GLOB_RECURSE ZLIB_FILES "../libs/zlib/*.cpp")
      set(SOURCE_FILES ${ZLIB_FILES} ${SOURCE_FILES})
    endif()
    pkg_search_module(LIBPNG libpng png)
    if (LIBPNG_FOUND)
      message(STATUS "Building with system libpng")
      add_library(LIBPNG INTERFACE)
      target_include_directories(LIBPNG INTERFACE ${LIBPNG_INCLUDE_DIR})
      target_link_libraries(LIBPNG INTERFACE ${LIBPNG_LIBRARIES})
    else()
      file(GLOB_RECURSE LIBPNG_FILES "../libs/libpng/*.cpp")
      set(SOURCE_FILES ${LIBPNG_FILES} ${SOURCE_FILES})
    endif()
  endif ()
endif()


# We provide our own manifest. MSVC seems not to autodetect that.
if(MSVC)
  message(STATUS "MSVC detected => Trying to disable the inclusion of the automatic manifest.")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /MANIFEST:NO")
  set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /MANIFEST:NO")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /MANIFEST:NO")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /MANIFEST:NO")
  set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} /MANIFEST:NO")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /MANIFEST:NO")
endif()

# Include our Resources file that contains our manifest
if(WIN32)
  include_directories(${CMAKE_SOURCE_DIR}/data/winrc)
  set(SOURCE_FILES Resources.rc ${SOURCE_FILES})
endif()

# We put Version.h into binary dir
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# OSX binary folders
if(APPLE)
  # DEFINE FILES
  file(GLOB RESOURCES_FILES
    ${CMAKE_BINARY_DIR}/data/wxmathml.lisp
    ${CMAKE_SOURCE_DIR}/test/testbench_simple.wxmx
    ${CMAKE_SOURCE_DIR}/data/io.github.wxmaxima_developers.wxMaxima.png
    ${CMAKE_SOURCE_DIR}/data/io.github.wxmaxima_developers.wxMaxima.svg)
  file(GLOB RESOURCES_FILES_OSX
    ${CMAKE_SOURCE_DIR}/art/wxmac.icns
    ${CMAKE_SOURCE_DIR}/art/wxmac-doc.icns
    ${CMAKE_SOURCE_DIR}/art/wxmac-doc-wxm.icns
    ${CMAKE_SOURCE_DIR}/art/wxmac-doc-wxmx.icns)
  file(GLOB HELP_FILES
    ${CMAKE_SOURCE_DIR}/info/*.html
    ${CMAKE_SOURCE_DIR}/info/*.jpg
    ${CMAKE_SOURCE_DIR}/info/*.png
    ${CMAKE_SOURCE_DIR}/info/*.hhp)
  file(GLOB CONTENT_FILES
    ${CMAKE_BINARY_DIR}/data/Info.plist
    ${CMAKE_SOURCE_DIR}/data/PkgInfo)
  file(GLOB FONT_FILES
    ${CMAKE_SOURCE_DIR}/data/fonts/*.ttf)
    # DEFINE DIRECTORIES
    set(APP_DIR ${CMAKE_BINARY_DIR}/wxMaxima.app/Contents)
    set(APP_MACOSX_DIR ${APP_DIR}/MacOS)
    set(APP_RESOURCES_DIR ${APP_DIR}/Resources)
    set(APP_HELP_DIR ${APP_RESOURCES_DIR}/help)
    set(APP_FONTS_DIR ${APP_RESOURCES_DIR}/fonts)
    # CUSTOM TARGET
    add_custom_target(wxMaxima.app)
    add_custom_command(TARGET wxMaxima.app PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_MACOSX_DIR})
    add_custom_command(TARGET wxMaxima.app PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_HELP_DIR})
    add_custom_command(TARGET wxMaxima.app
      COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_FONTS_DIR})
    foreach(f ${CONTENT_FILES})
      add_custom_command(TARGET wxMaxima.app
        COMMAND ${CMAKE_COMMAND} -E copy ${f}  ${APP_DIR})
    endforeach()
    foreach(f ${RESOURCES_FILES})
      add_custom_command(TARGET wxMaxima.app
        COMMAND ${CMAKE_COMMAND} -E copy ${f}  ${APP_RESOURCES_DIR})
    endforeach()
    foreach(f ${RESOURCES_FILES_OSX})
      add_custom_command(TARGET wxMaxima.app
        COMMAND ${CMAKE_COMMAND} -E copy ${f}  ${APP_RESOURCES_DIR})
    endforeach()
    foreach(f ${HELP_FILES})
      add_custom_command(TARGET wxMaxima.app
        COMMAND ${CMAKE_COMMAND} -E copy ${f}  ${APP_HELP_DIR})
    endforeach()
    foreach(f ${FONT_FILES})
      add_custom_command(TARGET wxMaxima.app
        COMMAND ${CMAKE_COMMAND} -E copy ${f} ${APP_FONTS_DIR})
    endforeach()
endif()

if(WIN32)
    add_executable(wxmaxima WIN32 ${SOURCE_FILES})
elseif(APPLE)
    add_executable(wxmaxima ${SOURCE_FILES})
    set_target_properties(wxmaxima PROPERTIES
              RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/wxMaxima.app/Contents/MacOS
              RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/wxMaxima.app/Contents/MacOS
              MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/Info.plist
              RESOURCE "${ALLRERESOURCES}"
              )
    # make wxmathml.lisp ship with the Mac App
    add_dependencies(wxmaxima wxMaxima.app)
else()
    add_executable(wxmaxima ${SOURCE_FILES})
endif()

target_link_libraries(wxmaxima ${wxWidgets_LIBRARIES} ${WXSVG_LIBRARIES})

include(CheckIncludeFiles)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Version.h.cin ${CMAKE_CURRENT_BINARY_DIR}/Version.h)

if(WIN32)
    install(TARGETS wxmaxima RUNTIME DESTINATION wxMaxima)
else()
    install(TARGETS wxmaxima RUNTIME DESTINATION bin)
endif()
