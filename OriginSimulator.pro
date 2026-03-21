QT += core widgets network

CONFIG += c++11

TARGET = OriginSimulator
TEMPLATE = app
INCLUDEPATH += healpix/src/cxx/Healpix_cxx
INCLUDEPATH += healpix/src/cxx/cxxsupport
LIBS += -lnova -ltiff
# For Apple Silicon Macs, use:
INCLUDEPATH += /opt/homebrew/include
LIBPATH += /opt/homebrew/lib

# Sources
SOURCES += \
    main.cpp \
    CelestronOriginSimulator.cpp \
    WebSocketConnection.cpp \
    CommandHandler.cpp \
    TiffImageGenerator.cpp \
    StatusSender.cpp \
    ProperHipsClient.cpp \
    EnhancedMosaicCreator.cpp \
    healpix/src/cxx/Healpix_cxx/healpix_base.cc \
    healpix/src/cxx/Healpix_cxx/healpix_tables.cc \
    healpix/src/cxx/cxxsupport/geom_utils.cc \
    healpix/src/cxx/cxxsupport/string_utils.cc \
    healpix/src/cxx/cxxsupport/error_handling.cc \
    healpix/src/cxx/cxxsupport/pointing.cc \
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
    TiffImageGenerator.h \
    StatusSender.h \
    moc_predefs.h \

# For Xcode project generation
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
}

# Enable debug output
CONFIG += debug_and_release
