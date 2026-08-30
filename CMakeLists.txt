cmake_minimum_required(VERSION 3.25)

project(Butter VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)

set(BUTTER_POLKIT_ACTION_DIR
    "${CMAKE_INSTALL_DATADIR}/polkit-1/actions"
    CACHE PATH "Install directory for Butter's Polkit actions")
set(BUTTER_EXPECTED_ARCHITECTURE "" CACHE STRING
    "Required native target architecture (for CI and packaging checks)")

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" BUTTER_TARGET_ARCHITECTURE)
if(BUTTER_TARGET_ARCHITECTURE MATCHES "^(arm64|aarch64)$")
    set(BUTTER_TARGET_ARCHITECTURE "aarch64")
elseif(BUTTER_TARGET_ARCHITECTURE MATCHES "^(amd64|x86_64)$")
    set(BUTTER_TARGET_ARCHITECTURE "x86_64")
endif()

if(BUTTER_EXPECTED_ARCHITECTURE AND
   NOT BUTTER_TARGET_ARCHITECTURE STREQUAL BUTTER_EXPECTED_ARCHITECTURE)
    message(FATAL_ERROR
        "Butter expected ${BUTTER_EXPECTED_ARCHITECTURE}, but CMake targets "
        "${BUTTER_TARGET_ARCHITECTURE}")
endif()
message(STATUS "Butter target architecture: ${BUTTER_TARGET_ARCHITECTURE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Test)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(butter
    src/main.cpp
    src/AgentNarrator.cpp
    src/FilesystemBackend.cpp
    src/BtrfsBackend.cpp
    src/HomeScanner.cpp
    src/HomeShadow.cpp
    src/ShadowCommon.cpp
    src/SpaceHistory.cpp
    src/RecoveryAudit.cpp
    src/ThemeBridge.cpp
)

target_include_directories(butter PRIVATE src)

qt_add_qml_module(butter
    URI Butter
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AgentSpinner.qml
        qml/ButterGlyph.qml
        qml/ButterMark.qml
        qml/ButterWordmark.qml
        qml/ChromeButton.qml
        qml/RecoveryRail.qml
        qml/RefreshGlyph.qml
        qml/ShadowGhost.qml
        qml/SpaceArc.qml
        qml/SpaceHistoryArea.qml
        qml/Surface.qml
        qml/TerminalGlyph.qml
        qml/Treemap.qml
    RESOURCES
        assets/butter.svg
        assets/butter-dish.svg
        assets/butter-stick.svg
        assets/butter-wordmark.svg
)

target_link_libraries(butter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

target_compile_definitions(butter PRIVATE BUTTER_VERSION="${PROJECT_VERSION}")

qt_add_executable(butter-shadow
    src/ButterShadow.cpp
    src/ShadowCommon.cpp
    src/SpaceHistory.cpp
)
target_include_directories(butter-shadow PRIVATE src)
target_link_libraries(butter-shadow PRIVATE Qt6::Core)

qt_add_executable(butter-probe
    src/ButterProbe.cpp
    src/RecoveryAudit.cpp
)
target_include_directories(butter-probe PRIVATE src)
target_link_libraries(butter-probe PRIVATE Qt6::Core)

qt_add_executable(butter-remedy
    src/ButterProbe.cpp
    src/RecoveryAudit.cpp
)
target_include_directories(butter-remedy PRIVATE src)
target_compile_definitions(butter-remedy PRIVATE BUTTER_REMEDY_HELPER=1)
target_link_libraries(butter-remedy PRIVATE Qt6::Core)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(butter_backend_test
        tests/BtrfsBackendTest.cpp
        src/AgentNarrator.cpp
        src/FilesystemBackend.cpp
        src/BtrfsBackend.cpp
        src/HomeScanner.cpp
        src/HomeShadow.cpp
        src/ShadowCommon.cpp
        src/SpaceHistory.cpp
        src/RecoveryAudit.cpp
    )
    target_include_directories(butter_backend_test PRIVATE src)
    target_link_libraries(butter_backend_test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_dependencies(butter_backend_test butter-shadow)
    add_test(NAME butter_backend_test COMMAND butter_backend_test)
endif()

configure_file(
    data/org.omarchy.butter.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.omarchy.butter.policy
    @ONLY
)

install(TARGETS butter butter-shadow butter-probe butter-remedy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.omarchy.butter.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/butter.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME org.omarchy.butter.svg)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.omarchy.butter.policy
        DESTINATION ${BUTTER_POLKIT_ACTION_DIR})
