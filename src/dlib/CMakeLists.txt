#
# This is a CMake makefile.  You can find the cmake utility and
# information about it at http://www.cmake.org
#


cmake_minimum_required(VERSION 2.8.12)
project(dlib)


include(cmake_utils/set_compiler_specific_options.cmake)


# Adhere to GNU filesystem layout conventions
include(GNUInstallDirs)

# default to a Release build (except if CMAKE_BUILD_TYPE is set)
include(cmake_utils/release_build_by_default)
include(cmake_utils/use_cpp_11.cmake)


set(CPACK_PACKAGE_VERSION_MAJOR "19")
set(CPACK_PACKAGE_VERSION_MINOR "8")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
# Set DLIB_VERSION in the including CMake file so they can use it to do whatever they want. 
get_directory_property(has_parent PARENT_DIRECTORY)
if(has_parent)
   set(DLIB_VERSION ${VERSION} PARENT_SCOPE)
   if (NOT DEFINED DLIB_IN_PROJECT_BUILD)
      set(DLIB_IN_PROJECT_BUILD true)
   endif()
endif()

if (CMAKE_VERSION VERSION_LESS "3.9.0")
   # Set only because there are old target_link_libraries() statements in the
   # FindCUDA.cmake file that comes with CMake that error out if the new behavior
   # is used.  In newer versions of CMake we can instead set CUDA_LINK_LIBRARIES_KEYWORD which fixes this issue.
   cmake_policy(SET CMP0023 OLD)
else()
   set(CUDA_LINK_LIBRARIES_KEYWORD PUBLIC)
endif()


macro (enable_preprocessor_switch option_name)
   list(APPEND active_preprocessor_switches "-D${option_name}")
endmacro()

macro (disable_preprocessor_switch option_name)
   if (active_preprocessor_switches)
      list(REMOVE_ITEM active_preprocessor_switches "-D${option_name}")
   endif()
endmacro()

macro (toggle_preprocessor_switch option_name)
   if (${option_name})
      enable_preprocessor_switch(${option_name})
   else()
      disable_preprocessor_switch(${option_name})
   endif()
endmacro()



# Suppress superfluous randlib warnings about libdlib.a having no symbols on MacOSX.
if (APPLE)
    set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

# Don't try to call add_library(dlib) and setup dlib's stuff if it has already
# been done by some other part of the current cmake project.  We do this
# because it avoids getting warnings/errors about cmake policy CMP0002.  This
# happens when a project tries to call add_subdirectory() on dlib more than
# once.  This most often happens when the top level of a project depends on two
# or more other things which both depend on dlib. 
if (NOT TARGET dlib)

   set (DLIB_ISO_CPP_ONLY_STR 
   "Enable this if you don't want to compile any non-ISO C++ code (i.e. you don't use any of the API Wrappers)" )
   set (DLIB_NO_GUI_SUPPORT_STR 
   "Enable this if you don't want to compile any of the dlib GUI code" )
   set (DLIB_ENABLE_STACK_TRACE_STR 
   "Enable this if you want to turn on the DLIB_STACK_TRACE macros" )
   set (DLIB_USE_BLAS_STR
   "Disable this if you don't want to use a BLAS library" )
   set (DLIB_USE_LAPACK_STR
   "Disable this if you don't want to use a LAPACK library" )
   set (DLIB_USE_CUDA_STR
   "Disable this if you don't want to use NVIDIA CUDA" )
   set (DLIB_PNG_SUPPORT_STR
   "Disable this if you don't want to link against libpng" )
   set (DLIB_GIF_SUPPORT_STR
   "Disable this if you don't want to link against libgif" )
   set (DLIB_JPEG_SUPPORT_STR
   "Disable this if you don't want to link against libjpeg" )
   set (DLIB_LINK_WITH_SQLITE3_STR
   "Disable this if you don't want to link against sqlite3" )
   #set (DLIB_USE_FFTW_STR "Disable this if you don't want to link against fftw" )
   set (DLIB_USE_MKL_FFT_STR
   "Disable this is you don't want to use the MKL DFTI FFT implementation" )
   set (DLIB_ENABLE_ASSERTS_STR
   "Enable this if you want to turn on the DLIB_ASSERT macro" )


   option(DLIB_ENABLE_ASSERTS ${DLIB_ENABLE_ASSERTS_STR} OFF)
   option(DLIB_ISO_CPP_ONLY ${DLIB_ISO_CPP_ONLY_STR} OFF)
   toggle_preprocessor_switch(DLIB_ISO_CPP_ONLY)
   option(DLIB_NO_GUI_SUPPORT ${DLIB_NO_GUI_SUPPORT_STR} OFF)
   toggle_preprocessor_switch(DLIB_NO_GUI_SUPPORT)
   option(DLIB_ENABLE_STACK_TRACE ${DLIB_ENABLE_STACK_TRACE_STR} OFF)
   toggle_preprocessor_switch(DLIB_ENABLE_STACK_TRACE)

   if(DLIB_ENABLE_ASSERTS)
      # Set these variables so they are set in the config.h.in file when dlib
      # is installed.
      set (DLIB_DISABLE_ASSERTS false)
      set (ENABLE_ASSERTS true) 
      enable_preprocessor_switch(ENABLE_ASSERTS)
      disable_preprocessor_switch(DLIB_DISABLE_ASSERTS)
   else()
      # Set these variables so they are set in the config.h.in file when dlib
      # is installed.
      set (DLIB_DISABLE_ASSERTS true)
      set (ENABLE_ASSERTS false) 
      disable_preprocessor_switch(ENABLE_ASSERTS)
      # Never force the asserts off when doing an in project build.  The only
      # time this matters is when using visual studio.  The visual studio IDE
      # has a drop down that lets the user select either release or debug
      # builds.  The DLIB_ASSERT macro is setup to enable/disable automatically
      # based on this drop down (via preprocessor magic).  However, if
      # DLIB_DISABLE_ASSERTS is defined it permanently disables asserts no
      # matter what, which would defeat the visual studio drop down.  So here
      # we make a point to not do that kind of severe disabling when in a
      # project build.  It should also be pointed out that DLIB_DISABLE_ASSERTS
      # is only needed when building and installing dlib as a separately
      # installed library.  It doesn't matter when doing an in project build. 
      if (NOT DLIB_IN_PROJECT_BUILD)
         enable_preprocessor_switch(DLIB_DISABLE_ASSERTS)
      endif()
   endif()

   if (DLIB_ISO_CPP_ONLY)
      option(DLIB_JPEG_SUPPORT ${DLIB_JPEG_SUPPORT_STR} OFF)
      option(DLIB_LINK_WITH_SQLITE3 ${DLIB_LINK_WITH_SQLITE3_STR} OFF)
      option(DLIB_USE_BLAS ${DLIB_USE_BLAS_STR} OFF)
      option(DLIB_USE_LAPACK ${DLIB_USE_LAPACK_STR} OFF)
      option(DLIB_USE_CUDA ${DLIB_USE_CUDA_STR} OFF)
      option(DLIB_PNG_SUPPORT ${DLIB_PNG_SUPPORT_STR} OFF)
      option(DLIB_GIF_SUPPORT ${DLIB_GIF_SUPPORT_STR} OFF)
      #option(DLIB_USE_FFTW ${DLIB_USE_FFTW_STR} OFF)
      option(DLIB_USE_MKL_FFT ${DLIB_USE_MKL_FFT_STR} OFF)
   else()
      option(DLIB_JPEG_SUPPORT ${DLIB_JPEG_SUPPORT_STR} ON)
      option(DLIB_LINK_WITH_SQLITE3 ${DLIB_LINK_WITH_SQLITE3_STR} ON)
      option(DLIB_USE_BLAS ${DLIB_USE_BLAS_STR} ON)
      option(DLIB_USE_LAPACK ${DLIB_USE_LAPACK_STR} ON)
      option(DLIB_USE_CUDA ${DLIB_USE_CUDA_STR} ON)
      option(DLIB_PNG_SUPPORT ${DLIB_PNG_SUPPORT_STR} ON)
      option(DLIB_GIF_SUPPORT ${DLIB_GIF_SUPPORT_STR} ON)
      #option(DLIB_USE_FFTW ${DLIB_USE_FFTW_STR} ON)
      option(DLIB_USE_MKL_FFT ${DLIB_USE_MKL_FFT_STR} ON)
   endif()
   toggle_preprocessor_switch(DLIB_JPEG_SUPPORT)
   toggle_preprocessor_switch(DLIB_USE_BLAS)
   toggle_preprocessor_switch(DLIB_USE_LAPACK)
   toggle_preprocessor_switch(DLIB_USE_CUDA)
   toggle_preprocessor_switch(DLIB_PNG_SUPPORT) 
   toggle_preprocessor_switch(DLIB_GIF_SUPPORT) 
   #toggle_preprocessor_switch(DLIB_USE_FFTW)
   toggle_preprocessor_switch(DLIB_USE_MKL_FFT)


   set(source_files 
         base64/base64_kernel_1.cpp
         entropy_decoder/entropy_decoder_kernel_1.cpp
         entropy_decoder/entropy_decoder_kernel_2.cpp
         entropy_encoder/entropy_encoder_kernel_1.cpp
         entropy_encoder/entropy_encoder_kernel_2.cpp
         tokenizer/tokenizer_kernel_1.cpp
         unicode/unicode.cpp
         dir_nav/dir_nav_kernel_1.cpp
         dir_nav/dir_nav_kernel_2.cpp
         misc_api/misc_api_kernel_1.cpp
         misc_api/misc_api_kernel_2.cpp
         threads/multithreaded_object_extension.cpp
         threads/threaded_object_extension.cpp
         threads/threads_kernel_1.cpp
         threads/threads_kernel_2.cpp
         threads/threads_kernel_shared.cpp
         threads/thread_pool_extension.cpp
         threads/async.cpp
         timer/timer.cpp
         stack_trace.cpp
         gui_widgets/fonts.cpp
            gui_widgets/widgets.cpp
            gui_widgets/drawable.cpp
            gui_widgets/canvas_drawing.cpp
            gui_widgets/style.cpp
            gui_widgets/base_widgets.cpp
            gui_core/gui_core_kernel_1.cpp
            gui_core/gui_core_kernel_2.cpp
   )


   set(dlib_needed_libraries)
   set(dlib_needed_includes)

   if (DLIB_ISO_CPP_ONLY)

   else()
      if(UNIX)
         set(CMAKE_THREAD_PREFER_PTHREAD ON)
         find_package(Threads REQUIRED)
         set(dlib_needed_libraries ${dlib_needed_libraries} ${CMAKE_THREAD_LIBS_INIT})
      endif()

      # we want to link to the right stuff depending on our platform.  
      if (WIN32 AND NOT CYGWIN) ###############################################################################
         if (DLIB_NO_GUI_SUPPORT)
            set (dlib_needed_libraries ws2_32 winmm)
         else()
            set (dlib_needed_libraries ws2_32 winmm comctl32 gdi32 imm32)
         endif()
      elseif(APPLE) ############################################################################
         set(CMAKE_MACOSX_RPATH 1)
         if (NOT DLIB_NO_GUI_SUPPORT)
            find_package(X11 QUIET)
            if (X11_FOUND)
               # If both X11 and anaconda are installed, it's possible for the
               # anaconda path to appear before /opt/X11, so we remove anaconda.
               foreach (ITR ${X11_INCLUDE_DIR})
                  if ("${ITR}" MATCHES "(.*)(Ana|ana|mini)conda(.*)")
                     list (REMOVE_ITEM X11_INCLUDE_DIR ${ITR})
                  endif ()
               endforeach(ITR)
               include_directories(${X11_INCLUDE_DIR})
               set (dlib_needed_libraries ${dlib_needed_libraries} ${X11_LIBRARIES})
            else()
               find_library(xlib X11)
               # Make sure X11 is in the include path.  Note that we look for
               # Xlocale.h rather than Xlib.h because it avoids finding a partial
               # copy of the X11 headers on systems with anaconda installed.
               find_path(xlib_path Xlocale.h
                  PATHS 
                  /Developer/SDKs/MacOSX10.4u.sdk/usr/X11R6/include
                  /opt/local/include
                  PATH_SUFFIXES X11
                  )
               if (xlib AND xlib_path)
                  get_filename_component(x11_path ${xlib_path} PATH CACHE)
                  include_directories(${x11_path})
                  set(dlib_needed_libraries ${dlib_needed_libraries} ${xlib} )
                  set(X11_FOUND 1)
               endif()
            endif()
            if (NOT X11_FOUND)
               message(" *****************************************************************************")
               message(" *** DLIB GUI SUPPORT DISABLED BECAUSE X11 DEVELOPMENT LIBRARIES NOT FOUND ***")
               message(" *** Make sure XQuartz is installed if you want GUI support.               ***")
               message(" *** You can download XQuartz from: http://xquartz.macosforge.org/landing/ ***")
               message(" *****************************************************************************")
               set(DLIB_NO_GUI_SUPPORT ON CACHE STRING ${DLIB_NO_GUI_SUPPORT_STR} FORCE )
               enable_preprocessor_switch(DLIB_NO_GUI_SUPPORT)
            endif()
         endif()

         mark_as_advanced(pthreadlib xlib xlib_path x11_path)
      else () ##################################################################################
         # link to the nsl library if it exists.  this is something you need sometimes 
         find_library(nsllib nsl)
         if (nsllib)
            set (dlib_needed_libraries ${dlib_needed_libraries} ${nsllib})
         endif ()

         # link to the socket library if it exists.  this is something you need on solaris
         find_library(socketlib socket)
         if (socketlib)
            set (dlib_needed_libraries ${dlib_needed_libraries} ${socketlib})
         endif ()

         if (NOT DLIB_NO_GUI_SUPPORT)
            include(FindX11)
            if (X11_FOUND)
               include_directories(${X11_INCLUDE_DIR})
               set (dlib_needed_libraries ${dlib_needed_libraries} ${X11_LIBRARIES})
            else()
               message(" *****************************************************************************")
               message(" *** DLIB GUI SUPPORT DISABLED BECAUSE X11 DEVELOPMENT LIBRARIES NOT FOUND ***")
               message(" *** Make sure libx11-dev is installed if you want GUI support.            ***")
               message(" *** On Ubuntu run: sudo apt-get install libx11-dev                        ***")
               message(" *****************************************************************************")
               set(DLIB_NO_GUI_SUPPORT ON CACHE STRING ${DLIB_NO_GUI_SUPPORT_STR} FORCE )
               enable_preprocessor_switch(DLIB_NO_GUI_SUPPORT)
            endif()
         endif()

         mark_as_advanced(nsllib pthreadlib socketlib)
      endif () ##################################################################################


      INCLUDE (CheckFunctionExists)

      if (DLIB_USE_BLAS OR DLIB_USE_LAPACK OR DLIB_USE_MKL_FFT)
          # Try to find BLAS, LAPACK and MKL
         include(cmake_utils/cmake_find_blas.txt)

         if (DLIB_USE_BLAS)
            if (blas_found)
               set (dlib_needed_libraries ${dlib_needed_libraries} ${blas_libraries})
            else()
               set(DLIB_USE_BLAS OFF CACHE STRING ${DLIB_USE_BLAS_STR} FORCE )
               toggle_preprocessor_switch(DLIB_USE_BLAS)
            endif()
         endif()

         if (DLIB_USE_LAPACK)
            if (lapack_found)
               set (dlib_needed_libraries ${dlib_needed_libraries} ${lapack_libraries})
               if (lapack_with_underscore)
                  set(LAPACK_FORCE_UNDERSCORE 1)
                  enable_preprocessor_switch(LAPACK_FORCE_UNDERSCORE)
               elseif (lapack_without_underscore)
                  set(LAPACK_FORCE_NOUNDERSCORE 1)
                  enable_preprocessor_switch(LAPACK_FORCE_NOUNDERSCORE)
               endif ()
            else()
               set(DLIB_USE_LAPACK OFF CACHE STRING ${DLIB_USE_LAPACK_STR} FORCE )
               toggle_preprocessor_switch(DLIB_USE_LAPACK)
            endif()
         endif()

         if (DLIB_USE_MKL_FFT)
            if (found_intel_mkl AND found_intel_mkl_headers)
               set (dlib_needed_includes ${dlib_needed_includes} ${mkl_include_dir})
               set (dlib_needed_libraries ${dlib_needed_libraries} ${mkl_libraries})
            else()
               set(DLIB_USE_MKL_FFT OFF CACHE STRING ${DLIB_USE_MKL_FFT_STR} FORCE )
               toggle_preprocessor_switch(DLIB_USE_MKL_FFT)
            endif()
         endif()
      endif()


      # Tell CMake to build dlib via add_library()/cuda_add_library()
      add_library(dlib STATIC ${source_files} )

      if (UNIX AND NOT DLIB_IN_PROJECT_BUILD)
            add_library(dlib_shared SHARED ${source_files} )
            add_dependencies(dlib_shared dlib)
      endif()

   endif ()  ##### end of if NOT DLIB_ISO_CPP_ONLY ##########################################################


   target_include_directories(dlib
                              INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
                              INTERFACE $<INSTALL_INTERFACE:include>
                              PUBLIC ${dlib_needed_includes}
                              )
   target_link_libraries(dlib PRIVATE ${dlib_needed_libraries})
   if (DLIB_IN_PROJECT_BUILD)
      target_compile_options(dlib PUBLIC ${active_preprocessor_switches})
   else()
      target_compile_options(dlib PRIVATE ${active_preprocessor_switches})
   endif()
   if (UNIX AND NOT DLIB_IN_PROJECT_BUILD)
      target_include_directories(dlib_shared
                                 INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
                                 INTERFACE $<INSTALL_INTERFACE:include>
                                 PUBLIC ${dlib_needed_includes}
                                 )
      target_link_libraries(dlib_shared PUBLIC ${dlib_needed_libraries})
      target_compile_options(dlib_shared PRIVATE ${active_preprocessor_switches})
   endif()


   # Allow the unit tests to ask us to compile the all/source.cpp file just to make sure it compiles.
   if (DLIB_TEST_COMPILE_ALL_SOURCE_CPP)
      add_library(dlib_all_source_cpp STATIC all/source.cpp) 
      target_link_libraries(dlib_all_source_cpp dlib)
      target_compile_options(dlib_all_source_cpp PUBLIC ${active_preprocessor_switches})
      enable_cpp11_for_target(dlib_all_source_cpp)
   endif()

   if (TARGET dlib)
      enable_cpp11_for_target(dlib)
      target_compile_options(dlib PUBLIC ${active_compile_opts})
   endif()
   if (TARGET dlib_shared)
      enable_cpp11_for_target(dlib_shared)
      target_compile_options(dlib_shared PUBLIC ${active_compile_opts})
   endif()

   # Install the library
   if (NOT DLIB_IN_PROJECT_BUILD)
       if(UNIX)
           set_target_properties(dlib_shared PROPERTIES
                                        OUTPUT_NAME dlib 
                                        VERSION ${VERSION})
           install(TARGETS dlib dlib_shared
                   EXPORT dlib 
                   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # Windows (including cygwin) considers .dll to be runtime artifacts
                   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
       else()
           install(TARGETS dlib
                   EXPORT dlib 
                   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # Windows considers .dll to be runtime artifacts
                   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
       endif()

       install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dlib
               FILES_MATCHING PATTERN "*.h" PATTERN "*.cmake"
               REGEX "${CMAKE_CURRENT_BINARY_DIR}" EXCLUDE)


       configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
       # overwrite config.h with the configured one
       install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dlib)

       configure_file(${PROJECT_SOURCE_DIR}/revision.h.in ${CMAKE_CURRENT_BINARY_DIR}/revision.h)
       install(FILES ${CMAKE_CURRENT_BINARY_DIR}/revision.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dlib)

       ## Config.cmake generation and installation

       set(ConfigPackageLocation "${CMAKE_INSTALL_LIBDIR}/cmake/dlib")
       install(EXPORT dlib
            NAMESPACE dlib::
            DESTINATION ${ConfigPackageLocation})

       configure_file(cmake_utils/dlibConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/config/dlibConfig.cmake" @ONLY)

       include(CMakePackageConfigHelpers)
       write_basic_package_version_file(
           "${CMAKE_CURRENT_BINARY_DIR}/config/dlibConfigVersion.cmake"
           VERSION ${VERSION}
           COMPATIBILITY AnyNewerVersion
           )

       install(FILES 
                    "${CMAKE_CURRENT_BINARY_DIR}/config/dlibConfig.cmake" 
                    "${CMAKE_CURRENT_BINARY_DIR}/config/dlibConfigVersion.cmake" 
               DESTINATION ${ConfigPackageLocation})

       ## dlib-1.pc generation and installation

       configure_file("cmake_utils/dlib.pc.in" "dlib-1.pc" @ONLY)
       install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dlib-1.pc"
           DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

   endif()

endif()

add_library(dlib::dlib ALIAS dlib)
