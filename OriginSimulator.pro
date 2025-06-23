QT += core widgets network

CONFIG += c++11

TARGET = OriginSimulator
TEMPLATE = app

# Sources
SOURCES += \
    main.cpp \
    CelestronOriginSimulator.cpp \
    WebSocketConnection.cpp \
    CommandHandler.cpp \
    StatusSender.cpp

# Headers
HEADERS += \
    TelescopeState.h \
    CelestronOriginSimulator.h \
    WebSocketConnection.h \
    CommandHandler.h \
    StatusSender.h

# For Xcode project generation
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.12
}

# Enable debug output
CONFIG += debug_and_release