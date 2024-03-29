QT       += core gui network charts serialport charts websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG(debug,debug|release) { COMPILE = debug  } else { COMPILE = release  }
IDE_APP_PATH = $$PWD/../Output/$$COMPILE
DESTDIR = $$IDE_APP_PATH


SOURCES += \
    iosystem.cpp \
    main.cpp \
    pugixml.cpp \
    widget.cpp

HEADERS += \
    iosystem.h \
    pugixml.hpp \
    widget.h

FORMS += \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
