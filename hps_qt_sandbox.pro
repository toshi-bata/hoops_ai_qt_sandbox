
QT += core gui widgets

# ================================================================
# Run `qmake` to generate makefile for a release build
# Run `qmake CONFIG+=debug` to generate makefile for a debug build
# ================================================================

# These following options can be toggled for different behavior
USE_DEBUG_HPS_LIBS = 0 # If using debug HPS libs set this to true (unneccessary for windows as debug libs are always shipped)
USING_EXCHANGE = 1 # Set to 1 if you intend to use exchange with the qt_sandbox

# Past this point all variables should be left unmodified
USING_EXCHANGE_PACKAGE = 0
equals(USING_EXCHANGE, 1) {
    USING_EXCHANGE_PACKAGE = 1
}

# for every lib you want to package list space-separated here
HPS_LIBS = hps_core hps_sprk hps_sprk_ops

linux*: {
    # QtX11Extras was removed in Qt6 (and is unused by this project anyway: window embedding
    # goes through QWidget::winId(), see HPSWidget::GetWindowID), so only pull it in under Qt5.
    lessThan(QT_MAJOR_VERSION, 6) {
        QT += x11extras
    }
    DEFINES += LINUX_SYSTEM

    contains(QMAKE_HOST.arch,x86_64){
        PLAT_NAME = linux64
        equals(USING_EXCHANGE, 1) {
            HPS_LIBS += hps_sprk_publish
        }
    } else {
        PLAT_NAME = linux_arm64-v8a
    }

    # The Linux Visualize package ships the shared libs in bin/<PLAT_NAME> (the same folder as
    # DESTDIR below), so link against HVISUALIZE_INSTALL_DIR rather than assuming the sandbox lives
    # inside the SDK tree ($${PWD}/../../bin, which only holds when built from samples/).
    LIB_PATH = $$(HVISUALIZE_INSTALL_DIR)/bin/$${PLAT_NAME}
}

win32-msvc* {
    PLAT_NAME = win64_v142
    vc_toolset = $$(VCToolsVersion)
    greaterThan(vc_toolset, 14.3)|equals(vc_toolset, 14.3){
        PLAT_NAME = win64_v143
    }
    message(QMake Platform: $$PLAT_NAME)

    LIB_PATH = $$(HVISUALIZE_INSTALL_DIR)/lib/$${PLAT_NAME}

    CONFIG(debug, debug|release) {
        USE_DEBUG_HPS_LIBS = 1 # windows package includes debug libs for ABI compatibility
    }

    equals(USING_EXCHANGE, 1) {
        HPS_LIBS += hps_sprk_publish
    }
}

macx: {
    PLAT_NAME = macos
    LIB_PATH = $${PWD}/../../bin/$${PLAT_NAME}
    LIBS += -Wl,-rpath,@executable_path,-rpath,@executable_path/../Frameworks
}

TEMPLATE = app

INCLUDEPATH += $$(HVISUALIZE_INSTALL_DIR)/include
DEPENDPATH += $$(HVISUALIZE_INSTALL_DIR)/include

equals(USING_EXCHANGE_PACKAGE, 1) {
    equals(USING_EXCHANGE, 1) {
        DEFINES += USING_EXCHANGE
        HPS_LIBS += hps_sprk_exchange
    }

    INCLUDEPATH += $$(HEXCHANGE_INSTALL_DIR)/include

    # hoops_ai_native_bridge: header is shared by both platforms; the library itself
    # (hoops_ai_bridge.dll/.lib on Windows, libhoops_ai_bridge.so on Linux) lives in a
    # platform-specific subfolder of HAI_BRIDGE_INSTALL_DIR (see that repo's CMakeLists.txt).
    INCLUDEPATH += $$(HAI_BRIDGE_INSTALL_DIR)/include
    DEPENDPATH += $$(HAI_BRIDGE_INSTALL_DIR)/include

    linux*: {
        # bin/<PLAT_NAME> holds libhoops_ai_bridge.so directly (no separate import lib on Linux);
        # PLAT_NAME (linux64 / linux_arm64-v8a) matches the bridge's own BIN_PLATFORM_DIR naming.
        HAI_BRIDGE_LIB_DIR = $$(HAI_BRIDGE_INSTALL_DIR)/bin/$${PLAT_NAME}
        LIBS += -L$${HAI_BRIDGE_LIB_DIR} -lhoops_ai_bridge
        QMAKE_LFLAGS += -Wl,-rpath,$${HAI_BRIDGE_LIB_DIR}
        # libhoops_ai_bridge.so embeds CPython and leaves the Py* symbols undefined, to be
        # satisfied by libpython3.12.so.1.0 that the bridge pulls in at runtime. When linking this
        # app against a prebuilt bridge on a host without a shared libpython (this Ubuntu 22.04 has a
        # static-only Python 3.12), ld would otherwise fail with "undefined reference to Py*".
        # Defer those to runtime. Remove once a locally-built bridge with a resolvable libpython
        # (see LINUX_BUILD_CONTINUE.md, bridge rebuild) is in place.
        QMAKE_LFLAGS += -Wl,--allow-shlib-undefined
    }
    win32-msvc* {
        # Windows keeps the import library separate from the runtime DLL (lib/win64, not bin/win64).
        LIBS += -L$$(HAI_BRIDGE_INSTALL_DIR)/lib/win64 -lhoops_ai_bridge
    }
}

SOURCES += \
    main.cpp \
    HPSWidget.cpp \
    HPSMainWindow.cpp \
	exchangeimportdialog.cpp \
    HPSModelBrowser.cpp \
	HPSSegmentBrowser.cpp \
    HPSPropertyBrowser.cpp \
    HoopsAiIndex.cpp \
    SimilarityIndexPanel.cpp \
    SandboxHighlightOp.cpp

HEADERS  += \
    HPSWidget.h \
    HPSMainWindow.h \
    HPSHandlers.h \
    exchangeimportdialog.h \
    HPSModelBrowser.h \
	HPSSegmentBrowser.h \
    HPSPropertyBrowser.h \
    HoopsAiIndex.h \
    SimilarityIndexPanel.h \
    properties.h \
    SandboxHighlightOp.h

TARGET = hps_qt_sandbox

RESOURCES += \
    Resources/HPSMainWindow.qrc

CONFIG(debug, debug|release) {
    equals(USE_DEBUG_HPS_LIBS, 1) {
        LIBS += -L$${LIB_PATH}d
        DESTDIR = $$(HVISUALIZE_INSTALL_DIR)/bin/$${PLAT_NAME}d
    }
    else {
        LIBS += -L$${LIB_PATH}
        DESTDIR = $$(HVISUALIZE_INSTALL_DIR)/bin/$${PLAT_NAME}
    }
}
else {
    LIBS += -L$${LIB_PATH}
    DESTDIR = $$(HVISUALIZE_INSTALL_DIR)/bin/$${PLAT_NAME}
}

for(LIB, HPS_LIBS){
    LIBS += -l$${LIB}
}

win32-msvc* {
    QMAKE_LFLAGS += -bigobj
    QMAKE_CXXFLAGS += -bigobj -Zc:__cplusplus
    CONFIG += c++17
}

linux*: {
    DOLLAR=$
    # Qt6 requires (and this project's headers, e.g. std::filesystem-free code, are fine with)
    # C++17; matches the Windows branch's CONFIG += c++17 below.
    CONFIG += c++17
    QMAKE_LFLAGS += -Wl,-rpath,\'$${DOLLAR}$${DOLLAR}ORIGIN\'

    LIBS += -lX11 -lXmu -ldl
}

macx: {
    for(LIB, HPS_LIBS){
        LibraryDependencies.files += $${LIB_PATH}/lib$${LIB}.dylib
    }

    equals(USING_EXCHANGE_PACKAGE, 1) {
        LibraryDependencies.files += $$(HEXCHANGE_INSTALL_DIR)/bin/macos/libA3DLIBS.dylib
    }

    LibraryDependencies.path = Contents/MacOS
    QMAKE_BUNDLE_DATA += LibraryDependencies
}
