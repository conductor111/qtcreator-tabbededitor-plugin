cmake_policy(SET CMP0003 NEW)
cmake_policy(SET CMP0012 NEW)
cmake_policy(SET CMP0054 NEW)


# Qt Creator build path
set(CMAKE_PREFIX_PATH "../qtcreator_build")
# Path to Qt library if its location is custom
#list(APPEND CMAKE_PREFIX_PATH "../Qt-5.15.7")

project(TabbedEditor)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(QtCreator COMPONENTS Core REQUIRED)
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
set(QtX Qt${QT_VERSION_MAJOR})
set(BUILD_PLUGIN_TABBEDEDITOR ON)

add_qtc_plugin(${PROJECT_NAME}
  PLUGIN_DEPENDS
     QtCreator::Core
     QtCreator::ProjectExplorer
     QtCreator::TextEditor
  DEPENDS
    QtCreator::ExtensionSystem
    QtCreator::Utils
  SOURCES
    constants.h
    plugin.cpp
    plugin.h
    tabbar.cpp
    tabbar.h
    resources/res.qrc
    README.md
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
