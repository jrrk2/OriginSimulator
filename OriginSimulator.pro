######################################################################
# Celestron Origin Simulator .pro file for macOS/XCode
######################################################################

# Project configuration
VERSION = 1.0.0
TEMPLATE = app
TARGET = CelestronOriginSimulator

# Create separate build directories for debug and release
CONFIG(debug, debug|release) {
    DESTDIR = build/debug
    OBJECTS_DIR = build/debug/obj
    MOC_DIR = build/debug/moc
    RCC_DIR = build/debug/rcc
    UI_DIR = build/debug/ui
} else {
    DESTDIR = build/release
    OBJECTS_DIR = build/release/obj
    MOC_DIR = build/release/moc
    RCC_DIR = build/release/rcc
    UI_DIR = build/release/ui
}

# macOS specific settings
CONFIG += app_bundle
QMAKE_MACOSX_DEPLOYMENT_TARGET = 13.0  # Using your specified target

# Include path
INCLUDEPATH += .

# Enable modern C++ features
CONFIG += c++17

# Qt modules
QT += core widgets network websockets httpserver

# Source files
SOURCES += OriginSimulator.cpp

# Header files
HEADERS +=

# Default rules
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Compiler and linker flags for macOS
macx {
    QMAKE_CXXFLAGS += -Werror=return-type
    QMAKE_LFLAGS += -Wl,-rpath,@executable_path/../Frameworks
    
    # For XCode build
    CONFIG += debug_and_release build_all relative_qt_rpath
    
    # Bundle identifier
    QMAKE_TARGET_BUNDLE_PREFIX = uk.kimmitt
    QMAKE_BUNDLE = CelestronOriginSimulator

    # Info.plist
    QMAKE_INFO_PLIST = Info.plist
    
    # Deployment target
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 13.0
}

# Post-build commands for macOS
macx {
    # Update version in Info.plist
    deploy.commands = /usr/libexec/PlistBuddy -c \"Set :CFBundleShortVersionString $$VERSION\" $$DESTDIR/$${TARGET}.app/Contents/Info.plist
    
    # Create simulator data directories
    deploy.commands += && mkdir -p $$DESTDIR/$${TARGET}.app/Contents/Resources/simulator_data/Images/Temp
    deploy.commands += && mkdir -p $$DESTDIR/$${TARGET}.app/Contents/Resources/simulator_data/Images/Astrophotography
    
    # Make sure this runs after the target is built
    deploy.depends = $(TARGET)
    
    # Add the deploy step to your build process
    QMAKE_EXTRA_TARGETS += deploy
    POST_TARGETDEPS += deploy
}
