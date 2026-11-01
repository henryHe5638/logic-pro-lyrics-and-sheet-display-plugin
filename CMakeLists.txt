cmake_minimum_required(VERSION 3.22)

project(LogicLyricsAndSheet VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.build/JUCE" CACHE PATH "Path to a JUCE checkout")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

juce_add_plugin(LogicLyricsAndSheet
  COMPANY_NAME "Local"
  BUNDLE_ID "local.logiclyricsandsheet"
  PLUGIN_MANUFACTURER_CODE LyrS
  PLUGIN_CODE LySh
  FORMATS AU Standalone
  PRODUCT_NAME "LyricsAndSheet"
  DESCRIPTION "LRC lyrics and PDF/image sheet music display synced to the host playhead"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  AU_MAIN_TYPE kAudioUnitType_Effect
  COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(LogicLyricsAndSheet
  PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PdfRenderer.mm
    Source/PdfRenderer.h
    Source/GifFrames.mm
    Source/GifFrames.h
    Source/Core/LrcDocument.cpp
    Source/Core/LrcDocument.h
    Source/Core/CueModel.cpp
    Source/Core/CueModel.h
)

target_compile_definitions(LogicLyricsAndSheet
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LogicLyricsAndSheet
  PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    "-framework CoreGraphics"
    "-framework CoreFoundation"
    "-framework ImageIO"
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Logic only loads code-signed Audio Units on Apple Silicon. Ad-hoc signing is
# enough for local use, and the bundle is copied into the user Components folder.
add_custom_command(TARGET LogicLyricsAndSheet_AU POST_BUILD
  COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:LogicLyricsAndSheet_AU>"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/Library/Audio/Plug-Ins/Components"
  COMMAND ${CMAKE_COMMAND} -E rm -rf
          "$ENV{HOME}/Library/Audio/Plug-Ins/Components/LogicLyricsAndSheet.component"
  COMMAND ${CMAKE_COMMAND} -E rm -rf
          "$ENV{HOME}/Library/Audio/Plug-Ins/Components/LyricsAndSheet.component"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_BUNDLE_DIR:LogicLyricsAndSheet_AU>"
          "$ENV{HOME}/Library/Audio/Plug-Ins/Components/LyricsAndSheet.component"
  COMMENT "Signing and installing LogicLyricsAndSheet.component"
  VERBATIM
)
