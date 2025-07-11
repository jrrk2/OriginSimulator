QT += core widgets network

CONFIG += c++11

TARGET = OriginSimulator
TEMPLATE = app
INCLUDEPATH += healpixmirror/src/cxx/Healpix_cxx
INCLUDEPATH += healpixmirror/src/cxx/cxxsupport
LIBS += -lnova
# For Apple Silicon Macs, use:
INCLUDEPATH += /opt/homebrew/include
LIBPATH += /opt/homebrew/lib

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
    moc_CelestronOriginSimulator.cpp \
    moc_CommandHandler.cpp \
    moc_EnhancedMosaicCreator.cpp \
    moc_ProperHipsClient.cpp \
    moc_StatusSender.cpp \
    moc_WebSocketConnection.cpp \
   
# Headers
HEADERS += \
    TelescopeState.h \
    CelestronOriginSimulator.h \
    WebSocketConnection.h \
    CommandHandler.h \
    StatusSender.h \
    moc_predefs.h \

# For Xcode project generation
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
}

# Enable debug output
CONFIG += debug_and_release
