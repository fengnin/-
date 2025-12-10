QT       += core gui network multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audioinput.cpp \
    audiooutput.cpp \
    chatmessage.cpp \
    logqueue.cpp \
    main.cpp \
    myvideosurface.cpp \
    netheader.cpp \
    partner.cpp \
    recvsolve.cpp \
    screen.cpp \
    sendimg.cpp \
    sendtext.cpp \
    tcpsocket.cpp \
    textedit.cpp \
    widget.cpp

HEADERS += \
    audioinput.h \
    audiooutput.h \
    chatmessage.h \
    logqueue.h \
    myvideosurface.h \
    netheader.h \
    partner.h \
    recvsolve.h \
    screen.h \
    sendimg.h \
    sendtext.h \
    tcpsocket.h \
    textedit.h \
    widget.h

FORMS += \
    widget.ui
QMAKE_CXXFLAGS += -execution-charset:utf-8 -source-charset:utf-8

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    source/source.qrc
