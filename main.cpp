#include "HPSMainWindow.h"
#include <QtWidgets/QApplication>
#include <QDir>
#include "hoops_license.h"

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef USING_EXCHANGE
    #include <A3DSDKDisableFunctions.h>
    // Bring in the raw HOOPS Exchange (A3D) C API so this application can call Exchange functions
    // directly (e.g. A3DCompareFacesInBrepModels for the Similarity Comparison visual diff), in
    // addition to the HPS::Exchange wrappers. INITIALIZE_A3D_API must be defined in exactly one
    // translation unit (here) so the A3D function-pointer storage is emitted once; every other
    // source that needs the API includes <A3DSDKIncludes.h> without this macro to get extern
    // declarations. The pointers are bound at runtime by A3DSDKLoadLibrary below.
    #define INITIALIZE_A3D_API
    #include <A3DSDKIncludes.h>
    #include "hoops_ai_bridge.h"
    #include <string>
    #ifndef TS_PUBLISH_DISABLED
        #define PUBLISH_ENABLED
    #endif

// Tracks whether the raw A3D library was successfully loaded and initialized, so A3DDllTerminate()
// is only called at shutdown when there is something to terminate (the function pointer would be
// null otherwise).
static bool g_exchangeApiInitialized = false;
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // A GUI process has no console by default, so a console subprocess spawned later (e.g. by the
    // embedded HOOPS AI Python runtime) would otherwise implicitly allocate and briefly flash its
    // own new console window. Allocating and hiding one here up front means such subprocesses
    // attach to this existing hidden console instead.
    AllocConsole();
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow != nullptr)
        ShowWindow(consoleWindow, SW_HIDE);
#endif

    HPS::World world(HOOPS_LICENSE);

    QApplication app(argc, argv);

    HPS::UTF8 exePath = app.applicationDirPath().toLocal8Bit().data();
    QString const appDir = app.applicationDirPath();

    char const* hvisualize_env = getenv("HVISUALIZE_INSTALL_DIR");
    if (hvisualize_env != nullptr) {
        HPS::UTF8 const HVISUALIZE_INSTALL_DIR(hvisualize_env);
        world.SetMaterialLibraryDirectory(HVISUALIZE_INSTALL_DIR + "/samples/data/materials");
        world.SetFontDirectory(HVISUALIZE_INSTALL_DIR + "/fonts");
    }
    else {
        // No env var: prefer the redistribution layout, where the Visualize materials and fonts are
        // bundled at the package root next to bin/ (<exe>\..\materials and <exe>\..\fonts), and fall
        // back to the development source-tree layout when those bundled folders are absent. This lets
        // a redistributed package run by a plain folder copy, with no environment variables set.
        QString const pkgRoot = QDir::cleanPath(appDir + "/..");
        QString const materialsRedist = pkgRoot + "/materials";
        QString const fontsRedist = pkgRoot + "/fonts";
#ifdef _MACH_
        // The executable lives within the .app bundle on OSX, need to account for this
        QString const materialsDev = QDir::cleanPath(appDir + "/../../../../../samples/data/materials");
        QString const fontsDev = QDir::cleanPath(appDir + "/../../../../../fonts");
#else
        QString const materialsDev = QDir::cleanPath(appDir + "/../../samples/data/materials");
        QString const fontsDev = QDir::cleanPath(appDir + "/../../fonts");
#endif
        QString const materialsDir = QDir(materialsRedist).exists() ? materialsRedist : materialsDev;
        QString const fontsDir = QDir(fontsRedist).exists() ? fontsRedist : fontsDev;
        world.SetMaterialLibraryDirectory(HPS::UTF8(materialsDir.toUtf8().constData()));
        world.SetFontDirectory(HPS::UTF8(fontsDir.toUtf8().constData()));
    }

#ifdef USING_EXCHANGE
    char const* exchange_env = getenv("HEXCHANGE_INSTALL_DIR");
    QString exchangeBinDirQ;
    QString exchangeResourceDirQ;
    if (exchange_env != nullptr) {
        QString bin_subdir;
    #if defined(_MSC_VER)
        #ifdef WIN64
        bin_subdir = "win64_v142/";
        #else
        bin_subdir = "win32_v142/";
        #endif
    #elif defined(_MACH_)
        bin_subdir = "macos/";
    #elif defined(__linux__)
        #ifdef _LP64
            #if defined(__aarch64__) || defined(__arm64__)
            bin_subdir = "linux_arm64-v8a/";
            #else
            bin_subdir = "linux64/";
            #endif
        #else
            bin_subdir = "linux32/";
        #endif
    #endif
        QString const installDir = QString::fromLocal8Bit(exchange_env);
        exchangeBinDirQ = QDir::cleanPath(installDir + "/bin/" + bin_subdir);
        exchangeResourceDirQ = QDir::cleanPath(installDir + "/bin/resource");
    }
    else {
        // No env var: redistribution layout. The Exchange binaries are flattened next to the exe
        // (in bin/) and the Publish "resource" folder is bundled at the package root
        // (<exe>\..\resource), so a redistributed package runs by a plain folder copy.
        exchangeBinDirQ = appDir;
        exchangeResourceDirQ = QDir::cleanPath(appDir + "/../resource");
    }

    if (!exchangeBinDirQ.isEmpty()) {
        HPS::UTF8 const exchangeBinDir(exchangeBinDirQ.toUtf8().constData());
        world.SetExchangeLibraryDirectory(exchangeBinDir);

        // Also load and initialize the raw A3D API against the same Exchange binaries so this
        // application can call Exchange functions directly (used by the Similarity Comparison
        // visual diff, which calls A3DCompareFacesInBrepModels). HPS keeps its own private copy of
        // Exchange for the scene graph; this second, app-owned binding is independent and is the
        // documented way to mix raw Exchange calls into a Visualize application.
        if (!A3DSDKLoadLibraryA(exchangeBinDir.GetBytes())) {
            QMessageBox msgWarning;
            msgWarning.setText("WARNING!\nCannot load the HOOPS Exchange library for the direct A3D API.\n"
                               "The Similarity Comparison visual diff will not be available.");
            msgWarning.setIcon(QMessageBox::Warning);
            msgWarning.setWindowTitle("Exchange A3D API");
            msgWarning.exec();
        }
        else {
            A3DLicPutUnifiedLicense(HOOPS_LICENSE);
            if (A3DDllInitialize(A3D_DLL_MAJORVERSION, A3D_DLL_MINORVERSION) != A3D_SUCCESS) {
                QMessageBox msgWarning;
                msgWarning.setText("WARNING!\nCannot initialize the HOOPS Exchange library for the direct A3D API.\n"
                                   "The Similarity Comparison visual diff will not be available.");
                msgWarning.setIcon(QMessageBox::Warning);
                msgWarning.setWindowTitle("Exchange A3D API");
                msgWarning.exec();
            }
            else {
                g_exchangeApiInitialized = true;
            }
        }
    }
    else {
        QMessageBox msgWarning;
        msgWarning.setText("WARNING!\nCannot find Exchange libraries");
        msgWarning.setIcon(QMessageBox::Warning);
        msgWarning.setWindowTitle("Exchange Missing");
        msgWarning.exec();
    }

    #ifdef PUBLISH_ENABLED
    if (!exchangeResourceDirQ.isEmpty()) {
        world.SetPublishResourceDirectory(HPS::UTF8(exchangeResourceDirQ.toUtf8().constData()));
    }
    else {
        QMessageBox msgWarning;
        msgWarning.setText("WARNING!\nCannot find HOOPS Exchange Advanced Publishing resources");
        msgWarning.setIcon(QMessageBox::Warning);
        msgWarning.setWindowTitle("HOOPS Exchange Advanced Publishing Resources Missing");
        msgWarning.exec();
    }
    #endif

    // Initialize the HOOPS AI bridge (embedded Python interpreter) once at startup, the same way
    // Exchange is initialized above. If it fails, no HOOPS AI command can ever work, so the
    // application exits immediately instead of letting each command re-check and fail later.
    // site-packages is resolved by the client and passed to the bridge as UTF-8. The bridge no
    // longer validates it or auto-detects site-packages: it simply adds the given path to sys.path.
    // The venv's internal layout differs by platform (see samples/test_client.cpp in
    // hoops_ai_native_bridge for the same logic in a portable console client):
    //   - Development:    derived from HOOPS_AI_HOME  -> Windows: <home>\.venv\Lib\site-packages
    //                                                     Linux:   <home>/.venv/lib/python3.12/site-packages
    //   - Redistribution: derived from the exe dir    -> Windows: <exe>\..\.venv\Lib\site-packages
    //                                                     Linux:   <exe>/../.venv/lib/python3.12/site-packages
    // Build the path with QString/QDir so non-ASCII user names in the redistribution layout survive.
#ifdef _WIN32
    QString const venvSitePackagesSubdir = QStringLiteral("/.venv/Lib/site-packages");
#else
    QString const venvSitePackagesSubdir = QStringLiteral("/.venv/lib/python3.12/site-packages");
#endif
    char const* aiHome = getenv("HOOPS_AI_HOME");
    QString sitePackages;
    if (aiHome != nullptr && *aiHome != '\0')
        sitePackages = QDir::cleanPath(QString::fromLocal8Bit(aiHome) + venvSitePackagesSubdir);
    else
        sitePackages = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/.." + venvSitePackagesSubdir);
    // Keep the UTF-8 buffer alive until HoopsAI_Initialize returns; its constData() is passed below.
    QByteArray sitePkgUtf8 = sitePackages.toUtf8();

    // pythonHome is optional. HOOPS_AI_PYTHON_HOME is only an override: when it is unset we pass
    // nullptr and the bridge auto-detects Python 3.12 from the Windows registry (PEP 514). When it
    // is set, that explicit value takes priority over the registry auto-detection.
    char const* aiPythonHome = getenv("HOOPS_AI_PYTHON_HOME");

    char aiErrBuf[8192] = {0};
    bool const aiInitOk = HoopsAI_Initialize(sitePkgUtf8.constData(),
                                              (aiPythonHome != nullptr && *aiPythonHome != '\0') ? aiPythonHome : nullptr,
                                              HOOPS_LICENSE, aiErrBuf, sizeof(aiErrBuf));
    if (!aiInitOk) {
        QMessageBox msgCritical;
        msgCritical.setText(QString("WARNING!\nFailed to initialize HOOPS AI:\n%1").arg(aiErrBuf));
        msgCritical.setIcon(QMessageBox::Critical);
        msgCritical.setWindowTitle("HOOPS AI Initialization Failed");
        msgCritical.exec();
        return 1;
    }
#endif

    app.setOrganizationName("TechSoft3D");
    app.setApplicationName("hps_qt_sandbox");
    app.setQuitOnLastWindowClosed(true);
    HPSMainWindow w;
    w.show();
    w.resize(800, 600);
    int const result = app.exec();

#ifdef USING_EXCHANGE
    HoopsAI_Shutdown();
    if (g_exchangeApiInitialized)
        A3DDllTerminate();
#endif

    return result;
}
