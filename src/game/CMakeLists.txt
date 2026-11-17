add_executable(orbit_courier main.cpp)
target_link_libraries(orbit_courier PRIVATE oc_core oc_warnings raylib)

if(MSVC)
    target_compile_definitions(orbit_courier PRIVATE _CRT_SECURE_NO_WARNINGS) # std::getenv
endif()

if(WIN32)
    # Windowed app without a console; still a normal main().
    set_target_properties(orbit_courier PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    if(MSVC)
        target_link_options(orbit_courier PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
    endif()
elseif(APPLE)
    target_link_libraries(orbit_courier PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

if(EMSCRIPTEN)
    set_target_properties(orbit_courier PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    target_link_options(orbit_courier PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web
        --shell-file ${PROJECT_SOURCE_DIR}/web/shell.html)
    set_property(TARGET orbit_courier APPEND PROPERTY LINK_DEPENDS ${PROJECT_SOURCE_DIR}/web/shell.html)
endif()
