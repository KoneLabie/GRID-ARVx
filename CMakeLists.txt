cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS build")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(GRID_ARVx VERSION 0.4.18 LANGUAGES C CXX)
if(MSVC)
    add_compile_options(/utf-8)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GRID_BUILD_APP "Build standalone application" ON)
option(GRID_BUILD_PLUGINS "Build DAW plugins" ON)
option(GRID_ENABLE_CLAP "Build CLAP using the pinned JUCE adapter" ON)
option(GRID_ENABLE_VST2 "Reserved for an authorized legacy implementation" OFF)
option(GRID_ENABLE_AAX "Reserved for an authorized AAX implementation" OFF)
option(GRID_CORE_TESTS_ONLY "Test the audio core without downloading JUCE" OFF)
option(GRID_BUILD_ENGINE_PROBE "Build the local native-engine inspection tool (not installed)" OFF)
option(GRID_USE_VEL_ENGINE "Use the Vel Engine backend" OFF)
if(GRID_USE_VEL_ENGINE AND NOT WIN32)
    message(FATAL_ERROR "The authorized engine copy is Windows x64 only. Other platform engines are not supplied.")
endif()
if(GRID_ENABLE_VST2 OR GRID_ENABLE_AAX)
    message(FATAL_ERROR "VST2/AAX are reserved options, not implemented in this foundation. Leave them OFF. SDK authorization and wrapper work are required.")
endif()
include(CTest)
if(BUILD_TESTING)
    add_executable(grid_audio_tests tests/audio_tests.cpp)
    target_include_directories(grid_audio_tests PRIVATE Source)
    find_package(Threads REQUIRED)
    target_link_libraries(grid_audio_tests PRIVATE Threads::Threads)
    add_test(NAME audio_core COMMAND grid_audio_tests)
    add_executable(grid_control_tests tests/control_tests.cpp)
    target_include_directories(grid_control_tests PRIVATE Source)
    target_link_libraries(grid_control_tests PRIVATE Threads::Threads)
    add_test(NAME midi_controls COMMAND grid_control_tests)
endif()
if(GRID_CORE_TESTS_ONLY)
    return()
endif()

include(FetchContent)
# Complete commits keep dependencies stable. Nothing needs to be preinstalled in the repo.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 51d11a2be6d5c97ccf12b4e5e827006e19f0555a)
FetchContent_MakeAvailable(JUCE)
if(BUILD_TESTING)
    juce_add_console_app(GridMappingTests PRODUCT_NAME "GRID Mapping Tests")
    target_sources(GridMappingTests PRIVATE tests/performance_mapping_tests.cpp Source/Plugin/Processor.cpp)
    target_include_directories(GridMappingTests PRIVATE Source)
    target_compile_definitions(GridMappingTests PRIVATE GRID_STANDALONE=1 GRID_USE_VEL_ENGINE=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GridMappingTests PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
    add_test(NAME performance_mapping COMMAND GridMappingTests)
endif()
if(GRID_BUILD_ENGINE_PROBE)
    juce_add_gui_app(GridEngineProbe PRODUCT_NAME "GRID Engine Probe" VERSION ${PROJECT_VERSION})
    target_sources(GridEngineProbe PRIVATE tools/NativeEngineProbe.cpp Source/Engine/VelEngine.cpp Source/Engine/EngineFiles.cpp Source/Engine/EngineSurface.cpp Source/Engine/PanelTheme.cpp)
    target_include_directories(GridEngineProbe PRIVATE Source)
    target_compile_definitions(GridEngineProbe PRIVATE
        GRID_ENGINE_PROBE=1
        JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_VST=0 JUCE_PLUGINHOST_AU=0
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GridEngineProbe PRIVATE juce::juce_audio_utils juce::juce_cryptography juce::juce_opengl
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
set(GRID_SOURCES Source/Plugin/Processor.cpp Source/Plugin/Editor.cpp Source/Plugin/Dialogs.cpp Source/Plugin/EditorChecks.cpp Source/Plugin/RecordingChecks.cpp Source/Plugin/TransitionChecks.cpp Source/Visual/Canvas.cpp Source/Visual/ReferenceFx.cpp Source/Visual/Scenes.cpp Source/Visual/NativeGeometry.cpp Source/Visual/TextScene.cpp Source/Output/Recorder.cpp Source/Audio/SystemInput.cpp)
function(grid_configure target)
    target_sources(${target} PRIVATE ${GRID_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 GRID_APP_VERSION="${PROJECT_VERSION}")
    if(GRID_USE_VEL_ENGINE)
        target_sources(${target} PRIVATE Source/Engine/VelEngine.cpp Source/Engine/EngineFiles.cpp Source/Engine/EngineSurface.cpp Source/Engine/PanelTheme.cpp)
        target_compile_definitions(${target} PUBLIC GRID_USE_VEL_ENGINE=1 JUCE_PLUGINHOST_VST3=1)
    endif()
    target_link_libraries(${target} PRIVATE juce::juce_audio_utils juce::juce_opengl juce::juce_cryptography
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endfunction()

if(GRID_BUILD_PLUGINS)
    set(GRID_FORMATS VST3)
    if(APPLE)
        list(APPEND GRID_FORMATS AU)
    endif()
    juce_add_plugin(GridARVxPlugin
        COMPANY_NAME "Cenaceae" BUNDLE_ID "com.cenaceae.gridarvx.plugin"
        PLUGIN_MANUFACTURER_CODE KnLb PLUGIN_CODE GAvx
        PRODUCT_NAME "GRID-ARVx" FORMATS ${GRID_FORMATS}
        IS_SYNTH FALSE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Analyzer AU_MAIN_TYPE kAudioUnitType_Effect)
    grid_configure(GridARVxPlugin)
    if(GRID_ENABLE_CLAP)
        set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(clap_juce_extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(clap_juce_extensions)
        clap_juce_extensions_plugin(TARGET GridARVxPlugin
            CLAP_ID "com.cenaceae.gridarvx" CLAP_FEATURES audio-effect analyzer
            CLAP_USE_JUCE_PARAMETER_RANGES ALL)
    endif()
endif()
if(GRID_BUILD_APP)
    juce_add_gui_app(GridARVxApp COMPANY_NAME "Cenaceae" PRODUCT_NAME "GRID-ARVx"
        VERSION ${PROJECT_VERSION}
        BUNDLE_ID "com.cenaceae.gridarvx.app"
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "GRID-ARVx uses your chosen audio input to animate visuals.")
    if(WIN32)
        # JUCE 8.0.4 omits this dependency, leaving old version resources behind
        # when the same build directory is reused after a version change.
        get_target_property(GRID_APP_INFO GridARVxApp JUCE_INFO_FILE)
        get_target_property(GRID_APP_GENERATED GridARVxApp JUCE_GENERATED_SOURCES_DIRECTORY)
        add_custom_command(OUTPUT "${GRID_APP_GENERATED}/GridARVxApp_resources.rc"
            APPEND DEPENDS "${GRID_APP_INFO}")
    endif()
    grid_configure(GridARVxApp)
    target_sources(GridARVxApp PRIVATE Source/App/Main.cpp)
    target_compile_definitions(GridARVxApp PRIVATE GRID_STANDALONE=1)
    if(GRID_USE_VEL_ENGINE)
        set(GRID_VEL_ENGINE_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/VelEngine/Windows/x64/Vel Engine.vst3")
        if(NOT EXISTS "${GRID_VEL_ENGINE_BUNDLE}/Contents/x86_64-win/Vel Engine.vst3")
            message(FATAL_ERROR "The Vel Engine build requires the separately supplied engine bundle. Installed Visual Lab Pro is never used as a fallback.")
        endif()
        add_custom_command(TARGET GridARVxApp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${GRID_VEL_ENGINE_BUNDLE}"
                "$<TARGET_FILE_DIR:GridARVxApp>/Engine/Vel Engine.vst3")
    endif()
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/grid-dependencies.json"
    CONTENT "{\"juce\":\"${juce_SOURCE_DIR}\",\"clap\":\"${clap_juce_extensions_SOURCE_DIR}\"}\n")
