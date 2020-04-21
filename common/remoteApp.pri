
INCLUDEPATH += $$PWD
include($$PWD/ZipUtils/ZipUtils.pri)

SOURCES += \
    $$PWD/GoogleAnalytics.cpp \
    $$PWD/RemoteService.cpp \
    $$PWD/Task.cpp \
    $$PWD/TapisCurl.cpp

HEADERS += \
    $$PWD/GoogleAnalytics.h \
    $$PWD/RemoteService.h \
    $$PWD/TapisCurl.h \
    $$PWD/Task.h
