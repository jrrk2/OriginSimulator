QT += core widgets network

CONFIG += c++17 sdk_no_version_check

TARGET = OriginSimulator
TEMPLATE = app
INCLUDEPATH += healpixmirror/src/cxx/Healpix_cxx
INCLUDEPATH += healpixmirror/src/cxx/cxxsupport
INCLUDEPATH += /opt/homebrew/include

# PCL integration for Gaia star catalog
PCLSRCDIR = /Users/jonathan/PCL/src
PCLINCDIR = /Users/jonathan/PCL/include
PCLLIBDIR = /Users/jonathan/PCL/src/pcl/macosx/g++/arm64/Release

INCLUDEPATH += $$PCLINCDIR
INCLUDEPATH += $$PCLSRCDIR/3rdparty

DEFINES += _REENTRANT __PCL_MACOSX __PCL_NO_PERFORMANCE_CRITICAL_MATH_ROUTINES

# Sources
SOURCES += \
    main.cpp \
    CelestronOriginSimulator.cpp \
    WebSocketConnection.cpp \
    CommandHandler.cpp \
    StatusSender.cpp \
    ProperHipsClient.cpp \
    EnhancedMosaicCreator.cpp \
    healpixmirror/src/cxx/Healpix_cxx/healpix_base.cc \
    healpixmirror/src/cxx/Healpix_cxx/healpix_tables.cc \
    healpixmirror/src/cxx/cxxsupport/geom_utils.cc \
    healpixmirror/src/cxx/cxxsupport/string_utils.cc \
    healpixmirror/src/cxx/cxxsupport/error_handling.cc \
    healpixmirror/src/cxx/cxxsupport/pointing.cc \
    GaiaStarFieldRenderer.cpp \
    StellariumDSOOverlay.cpp \

# Headers
HEADERS += \
    TelescopeState.h \
    CelestronOriginSimulator.h \
    WebSocketConnection.h \
    CommandHandler.h \
    StatusSender.h \
    moc_predefs.h \
    EnhancedMosaicCreator.h \
    ProperHipsClient.h \
    GaiaStarFieldRenderer.h \
    StellariumDSOOverlay.h \

# For Xcode project generation
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
    LIBS += -L/opt/homebrew/lib
    LIBS += -L$$PCLLIBDIR
    LIBS += -lPCL-pxi -llz4-pxi -lzstd-pxi -lzlib-pxi -lRFC6234-pxi
    LIBS += -llcms-pxi -lcminpack-pxi -lMock-pxi -lFITS-pxm -lTIFF-pxm -lXISF-pxm
    LIBS += -lfftw3 -lfftw3_threads
    QT += svg
    LIBS += -framework AppKit
    QMAKE_LFLAGS += -Wl,-dead_strip
}

# Enable debug output
CONFIG += debug_and_release
