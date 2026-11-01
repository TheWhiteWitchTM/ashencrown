cmake_minimum_required(VERSION 3.21)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ASHEN_VERSION_TEXT)
string(STRIP "${ASHEN_VERSION_TEXT}" ASHEN_VERSION_TEXT)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" ASHEN_VERSION_TRIPLE "${ASHEN_VERSION_TEXT}")
if(NOT ASHEN_VERSION_TRIPLE)
    message(FATAL_ERROR "VERSION must start with major.minor.patch")
endif()

project(AshenCrown VERSION ${ASHEN_VERSION_TRIPLE} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Multimedia WebSockets)
find_package(Threads REQUIRED)

# The release packages leave this off. MinGW has no Qt WebEngine, and the
# Chromium library pushes the Linux archive over GitHub's file limit.
option(ASHENCROWN_WITH_WEBENGINE "Open Buy me a coffee inside the window" ON)
set(ASHENCROWN_USE_WEBENGINE OFF)
if(ASHENCROWN_WITH_WEBENGINE)
    find_package(Qt6 QUIET COMPONENTS WebEngineQuick)
    if(TARGET Qt6::WebEngineQuick)
        set(ASHENCROWN_USE_WEBENGINE ON)
    endif()
endif()

qt_add_executable(ashencrown
    src/main.cpp
    src/GameController.cpp
    src/ChessEngine.cpp
    src/ChessAI.cpp
    src/NetCode.cpp
    src/NatTraverse.cpp
    src/NetSession.cpp
    src/BskyClient.cpp
)

# The Linux host rcc would emit a zstd hook a cross-built Qt may not load.
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
    set_target_properties(ashencrown PROPERTIES AUTORCC_OPTIONS "--no-zstd")
endif()

file(GLOB_RECURSE ASHEN_ASSETS CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "assets/*")

set(ASHEN_QML
    qml/Main.qml
    qml/CoffeePane.qml
    qml/StartScreen.qml
    qml/WarLobby.qml
    qml/GameView.qml
    qml/FightScene.qml
    qml/AshButton.qml
    qml/BoardTable.qml
    qml/TeamFigure.qml
    qml/SlabMesh.qml
    qml/WarpQuad.qml
    qml/fight.js
    qml/board3d.js
)

qt_add_resources(ashencrown "app_res"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    PREFIX "/"
    FILES ${ASHEN_QML} ${ASHEN_ASSETS}
)

# Bluesky signs with OpenSSL EVP. The headers are declared in BskyClient.cpp.
# Linux has the SONAME only, because libssl-dev is not installed.
if(WIN32 OR APPLE)
    if(NOT OPENSSL_ROOT_DIR)
        message(FATAL_ERROR "ashencrown: set OPENSSL_ROOT_DIR to a static OpenSSL 3")
    endif()
    set(_ashen_lib_mode "${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}")
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    find_library(CRYPTO_LIB NAMES crypto libcrypto
        PATHS "${OPENSSL_ROOT_DIR}/lib" "${OPENSSL_ROOT_DIR}/lib64"
        NO_DEFAULT_PATH REQUIRED)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "${_ashen_lib_mode}")
else()
    find_library(CRYPTO_LIB NAMES libcrypto.so.3
        PATHS /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu
        REQUIRED)
endif()

target_link_libraries(ashencrown PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Multimedia
    Qt6::WebSockets
    Threads::Threads
    ${CRYPTO_LIB}
)

if(ASHENCROWN_USE_WEBENGINE)
    target_link_libraries(ashencrown PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(ashencrown PRIVATE ASHENCROWN_HAS_WEBENGINE=1)
endif()

if(WIN32)
    target_link_libraries(ashencrown PRIVATE ws2_32 gdi32 crypt32 bcrypt advapi32)
    set_target_properties(ashencrown PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    target_link_libraries(ashencrown PRIVATE "-framework CoreFoundation" "-framework Security")
    set_target_properties(ashencrown PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks")
else()
    set(_qt_lib "")
    if(DEFINED QT6_INSTALL_PREFIX AND DEFINED QT6_INSTALL_LIBS)
        set(_qt_lib "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBS}")
    endif()
    set(_rpath "\$ORIGIN/../lib")
    if(_qt_lib)
        set(_rpath "${_rpath};${_qt_lib}")
    endif()
    set_target_properties(ashencrown PROPERTIES
        BUILD_RPATH "${_rpath}"
        INSTALL_RPATH "\$ORIGIN/../lib")
    target_link_options(ashencrown PRIVATE -Wl,--disable-new-dtags)
endif()
