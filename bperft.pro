TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

CONFIG(release, debug|release) {
  message(Release build!)
  DEFINES += NDEBUG
}

SOURCES += \
    bperft.cpp

DISTFILES += \
    README.md
