QT += core widgets network

CONFIG += c++11 sdk_no_version_check

TARGET = OriginSimulator
TEMPLATE = app
INCLUDEPATH += healpixmirror/src/cxx/Healpix_cxx
INCLUDEPATH += healpixmirror/src/cxx/cxxsupport
INCLUDEPATH += /opt/homebrew/include

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
    DSSFitsManager.cpp \

# Headers
HEADERS += \
    TelescopeState.h \
    CelestronOriginSimulator.h \
    WebSocketConnection.h \
    CommandHandler.h \
    StatusSender.h \
    moc_predefs.h \
    DSSFitsManager.h \
    EnhancedMosaicCreator.h \
    ProperHipsClient.h \
    
# For Xcode project generation
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
    LIBS += /opt/homebrew/lib/libcfitsio.dylib
}

# Enable debug output
CONFIG += debug_and_release
