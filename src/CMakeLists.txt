cmake_minimum_required(VERSION 3.21)
project(SteamSwitch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Core engine (no Qt; pure std + OS APIs) ---------------------------------
set(CORE_SOURCES
    core/model.cpp
    core/vdf.cpp
    core/json.cpp
    core/http.cpp
    core/appdata.cpp
    core/settings.cpp
    core/steam_paths.cpp
    core/steam_games.cpp
    core/steam_accounts.cpp
    core/steam_switcher.cpp
    core/steam_launcher.cpp
    core/covers.cpp
    core/store_covers.cpp
    core/epic_games.cpp
    core/gog_games.cpp
    core/xbox_games.cpp
    core/sha256.cpp
    core/stores/steam_store.cpp
    core/stores/epic_store.cpp
    core/stores/gog_store.cpp
    core/stores/xbox_store.cpp
)
if(WIN32)
    list(APPEND CORE_SOURCES core/platform_win.cpp)
else()
    list(APPEND CORE_SOURCES core/platform_posix.cpp)
endif()

add_library(sscore STATIC ${CORE_SOURCES})
target_include_directories(sscore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(sscore PRIVATE shell32 advapi32 user32)
endif()

# --- Diagnostic CLI (Phase 1 parity gate) ------------------------------------
add_executable(ssdiag tools/ssdiag.cpp)
target_link_libraries(ssdiag PRIVATE sscore)

# --- Headless tests ----------------------------------------------------------
enable_testing()
file(GLOB TEST_SOURCES tests/test_*.cpp tests/test_runner.cpp)
add_executable(sstests ${TEST_SOURCES})
target_link_libraries(sstests PRIVATE sscore)
add_test(NAME sstests COMMAND sstests)

# --- Qt 6 GUI (optional: only if Qt6 is found) -------------------------------
find_package(Qt6 COMPONENTS Core Gui Quick Network QUIET)
if(Qt6_FOUND)
    qt_standard_project_setup(REQUIRES 6.5)
    qt_add_executable(SteamSwitch
        ui/main.cpp
        ui/Backend.cpp
        ui/GameModel.cpp
        ui/GameFilterModel.cpp
        ui/QtFetcher.cpp
    )
    # Theme.qml and AppState.qml are QML singletons.
    set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    set_source_files_properties(ui/qml/AppState.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    qt_add_qml_module(SteamSwitch
        URI SteamSwitch
        VERSION 1.0
        QML_FILES
            ui/qml/Main.qml
            ui/qml/Theme.qml
            ui/qml/AppState.qml
            ui/qml/AmbientBackground.qml
            ui/qml/SideNavItem.qml
            ui/qml/StoreChip.qml
            ui/qml/Toggle.qml
            ui/qml/GameCard.qml
            ui/qml/LibraryView.qml
            ui/qml/DetailView.qml
            ui/qml/AccountsView.qml
            ui/qml/SettingsView.qml
            ui/qml/ManagePanel.qml
            ui/qml/OnboardingOverlay.qml
    )
    # Bundle the UI fonts into the binary so the typography ships without any system
    # install. ORBIT uses Space Grotesk (display) + Manrope (body); Cairo covers
    # Arabic. (Hanken Grotesk / Geist kept as fallbacks from the earlier build.)
    qt_add_resources(SteamSwitch "appfonts"
        PREFIX "/fonts"
        BASE "ui/fonts"
        FILES
            ui/fonts/space-grotesk-500.ttf
            ui/fonts/space-grotesk-600.ttf
            ui/fonts/space-grotesk-700.ttf
            ui/fonts/manrope-400.ttf
            ui/fonts/manrope-500.ttf
            ui/fonts/manrope-600.ttf
            ui/fonts/manrope-700.ttf
            ui/fonts/manrope-800.ttf
            ui/fonts/hanken-grotesk-400.ttf
            ui/fonts/hanken-grotesk-500.ttf
            ui/fonts/hanken-grotesk-600.ttf
            ui/fonts/hanken-grotesk-700.ttf
            ui/fonts/geist-500.ttf
            ui/fonts/geist-600.ttf
            ui/fonts/cairo-arabic-400.ttf
            ui/fonts/cairo-arabic-600.ttf
            ui/fonts/cairo-arabic-700.ttf
    )
    target_link_libraries(SteamSwitch PRIVATE sscore Qt6::Core Qt6::Gui Qt6::Quick Qt6::Network)
    # TODO: flip back to ON once the UI is verified — OFF gives a console so QML
    # load errors are visible while debugging.
    set_target_properties(SteamSwitch PROPERTIES WIN32_EXECUTABLE OFF)
else()
    message(STATUS "Qt6 not found — building core + ssdiag + tests only (no GUI).")
endif()
