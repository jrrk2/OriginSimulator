QT += core widgets network

CONFIG += c++11

TARGET = OriginSimulator
TEMPLATE = app
DEFINES += ORIGIN_SIM_SOURCE_DIR=\\\"$$PWD\\\"     # where data/gaia_dr3_g12.bin is found when the build is out of the tree
INCLUDEPATH += healpixmirror/src/cxx/Healpix_cxx
INCLUDEPATH += healpixmirror/src/cxx/cxxsupport
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
    FitsImageGenerator.cpp \
    StatusSender.cpp \
    ProperHipsClient.cpp \
    EnhancedMosaicCreator.cpp \
    healpixmirror/src/cxx/Healpix_cxx/healpix_base.cc \
    healpixmirror/src/cxx/Healpix_cxx/healpix_tables.cc \
    healpixmirror/src/cxx/cxxsupport/geom_utils.cc \
    healpixmirror/src/cxx/cxxsupport/string_utils.cc \
    healpixmirror/src/cxx/cxxsupport/error_handling.cc \
    healpixmirror/src/cxx/cxxsupport/pointing.cc \
    MountModel.cpp \
    GaiaSky.cpp \
    GaiaFetch.cpp \
   
# Headers
HEADERS += \
    TelescopeState.h \
    CelestronOriginSimulator.h \
    WebSocketConnection.h \
    CommandHandler.h \
    TiffImageGenerator.h \
    FitsImageGenerator.h \
    StatusSender.h \
    ProperHipsClient.h \
    EnhancedMosaicCreator.h \
    MountModel.h \
    GaiaSky.h \
    GaiaFetch.h \

# For Xcode project generation
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
}

# Enable debug output
CONFIG += debug_and_release
