#include <QtCore/QtGlobal>
#include <QMetaObject>
#include <QElapsedTimer>
#include "HPSMainWindow.h"
#include "HPSWidget.h"
#include "HPSModelBrowser.h"
#include "HPSSegmentBrowser.h"
#include "exchangeimportdialog.h"
#include "hps.h"
#include "sprk_ops.h"
#include "SandboxHighlightOp.h"

#include <cmath>
#include <algorithm>

#if _MSC_VER
    #define snprintf _snprintf
#endif

#ifdef USING_EXCHANGE
    #include <A3DSDKDisableFunctions.h>
    // Raw HOOPS Exchange (A3D) C API declarations, used by onSimilarityComparison to run the
    // geometric model compare (A3DCompareFacesInBrepModels) and render a colored visual diff. The
    // function-pointer storage is emitted in main.cpp (which defines INITIALIZE_A3D_API); here we
    // only need the extern declarations, so INITIALIZE_A3D_API must NOT be defined.
    #include <A3DSDKIncludes.h>
    #if defined _MSC_VER && !defined TS_PUBLISH_DISABLED
        #define PUBLISH_ENABLED
        #include "sprk_publish.h"
    #endif

    #include <QDir>
    #include <QCoreApplication>
    #include <QFileDialog>
    #include <QFileInfo>
    #include <QSettings>
    #include <QMessageBox>
    #include <QVBoxLayout>
    #include <QHBoxLayout>
    #include <QLabel>
    #include <QFrame>
    #include <QCheckBox>
    #include <cmath>

// ts3d_162k_mfr.ckpt has 25 classes (0 = no feature). Mapping raw label IDs to human-readable
// names is ISV business logic and must happen before showing anything to the end user
// (see the design note at the top of hoops_ai_bridge.h).
static const char* kMfrLabelNames[25] = {
    "no-label",                           // 0
    "rectangular_through_slot",           // 1
    "triangular_through_slot",            // 2
    "rectangular_passage",                // 3
    "triangular_passage",                 // 4
    "6sides_passage",                     // 5
    "rectangular_through_step",           // 6
    "2sides_through_step",                // 7
    "slanted_through_step",               // 8
    "rectangular_blind_step",             // 9
    "triangular_blind_step",              // 10
    "rectangular_blind_slot",             // 11
    "rectangular_pocket",                 // 12
    "triangular_pocket",                  // 13
    "6sides_pocket",                      // 14
    "chamfer",                            // 15
    "circular_through_slot",              // 16
    "through_hole",                       // 17
    "circular_blind_step",                // 18
    "horizontal_circular_end_blind_slot", // 19
    "vertical_circular_end_blind_slot",   // 20
    "circular_end_pocket",                // 21
    "o-ring",                             // 22
    "blind_hole",                         // 23
    "fillet"                              // 24
};

// Deterministically derives a color for a given MFR label ID. Label 0 (no feature) is always a
// fixed light gray; other labels are spread around the hue wheel using the golden angle so that
// consecutive IDs stay visually distinguishable.
static HPS::RGBColor ColorForMfrLabel(int label)
{
    if (label <= 0)
        return HPS::RGBColor(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f);

    double const hue = std::fmod(label * 137.508, 360.0);
    double const h = hue / 60.0;
    double const c = 1.0;
    double const x = c * (1.0 - std::fabs(std::fmod(h, 2.0) - 1.0));
    double r = 0.0, g = 0.0, b = 0.0;
    switch (static_cast<int>(h) % 6) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        case 5: r = c; g = 0; b = x; break;
    }
    return HPS::RGBColor(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b));
}

static QString MfrLabelDisplayName(int label)
{
    if (label >= 0 && label < 25)
        return QString::fromLatin1(kMfrLabelNames[label]);
    return QString("label_%1").arg(label);
}

// Fixed colors for the Similarity Comparison visual diff. Both the 3D face coloring
// (ColorLoadedPartsByCompare) and the on-screen legend (UpdateCompareLegend) read these constants,
// so the color a face is painted and the color shown in the legend can never drift apart.
static const HPS::RGBColor kCompareUnchangedColor(150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f);
static const HPS::RGBColor kCompareAddedColor(40.0f / 255.0f, 200.0f / 255.0f, 40.0f / 255.0f);
static const HPS::RGBColor kCompareRemovedColor(220.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f);
#endif

HPSWidget::HPSWidget(QWidget* parent):
    QWidget(parent), displayResourceMonitor(false), enableSimpleShadows(false), enableFrameRate(false), smoothRendering(true),
    eyeDome(false), validPixelToWindowMatrix(false)
#ifdef USING_EXCHANGE
    ,
    mfrLegendPanel(nullptr)
#endif
{
    setAttribute(Qt::WA_PaintOnScreen);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    setAttribute(Qt::WA_NoBackground);
#else
    setAttribute(Qt::WA_OpaquePaintEvent);
#endif
    setAttribute(Qt::WA_NoSystemBackground);
    setBackgroundRole(QPalette::NoRole);

    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);

    HPS::ApplicationWindowOptionsKit windowOpts;
    windowOpts.SetAntiAliasCapable(true);

    canvas = HPS::Factory::CreateCanvas(static_cast<HPS::WindowHandle>(GetWindowID()), "HPS Qt Sandbox", windowOpts);
    Q_ASSERT(sizeof(HPS::WindowHandle) >= sizeof(WId));

    view = HPS::Factory::CreateView();
    canvas.AttachViewAsLayout(view);

    // Solid background color for the view (set on the window key, behind the transparent view).
    ApplyViewBackground();

    EnableAxisTriad(view);

    setupSceneDefaults();
}

HPSWidget::~HPSWidget()
{
    view.Delete();
    canvas.GetAttachedLayout().Delete();
    canvas.Delete();
    cad_model.Delete();
}

WId HPSWidget::GetWindowID() { return winId(); }

void HPSWidget::setupSceneDefaults()
{
    // Delete our model if we have one already
    if (model.Type() != HPS::Type::None) {
        // The shape-map lives as a subsegment of the model; deleting the model destroys it.
        // Drop our cached keys so a later ShowShapeMap never deletes a dangling segment.
        m_shapeMapPoints.clear();
        m_shapeMapSegment = HPS::SegmentKey();
        m_shapeMapHighlightSegment = HPS::SegmentKey();
        if (m_shapeMapLegendPanel) {
            delete m_shapeMapLegendPanel;
            m_shapeMapLegendPanel = nullptr;
        }
        model.Delete();
    }

    model = HPS::Factory::CreateModel();
    view.AttachModel(model);

    view.GetOperatorControl()
        .Push(new HPS::MouseWheelOperator(), HPS::Operator::Priority::Low)
        .Push(new HPS::ZoomOperator(HPS::MouseButtons::ButtonMiddle()))
        .Push(new HPS::PanOperator(HPS::MouseButtons::ButtonRight()))
        .Push(new HPS::OrbitOperator(HPS::MouseButtons::ButtonLeft()));

    HPS::Database::GetEventDispatcher().Subscribe(errorHandler, HPS::Object::ClassID<HPS::ErrorEvent>());
    HPS::Database::GetEventDispatcher().Subscribe(warningHandler, HPS::Object::ClassID<HPS::WarningEvent>());
}

void HPSWidget::resizeEvent(QResizeEvent*)
{
    validPixelToWindowMatrix = false;
    canvas.GetWindowKey().UpdateWithNotifier(HPS::Window::UpdateType::Refresh).Wait();
    RepositionShapeMapLegend();
#ifdef USING_EXCHANGE
    RepositionMfrLegend();
    RepositionCompareLegend();
#endif
}

void HPSWidget::paintEvent(QPaintEvent*)
{
#ifndef __APPLE__
    canvas.GetWindowKey().Update(HPS::Window::UpdateType::Refresh);
#endif
}

void HPSWidget::mousePressEvent(QMouseEvent* event)
{
    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(buildMouseEvent(event, HPS::MouseEvent::Action::ButtonDown, 1));
}

void HPSWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(buildMouseEvent(event, HPS::MouseEvent::Action::ButtonDown, 2));
}

void HPSWidget::mouseReleaseEvent(QMouseEvent* event)
{
    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(buildMouseEvent(event, HPS::MouseEvent::Action::ButtonUp, 0));
}

void HPSWidget::mouseMoveEvent(QMouseEvent* event)
{
    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(buildMouseEvent(event, HPS::MouseEvent::Action::Move, 0));
}

void HPSWidget::keyPressEvent(QKeyEvent* event)
{
    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(buildKeyboardEvent(event, HPS::KeyboardEvent::Action::KeyDown));
}

void HPSWidget::keyReleaseEvent(QKeyEvent* event)
{
    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(buildKeyboardEvent(event, HPS::KeyboardEvent::Action::KeyUp));
}

HPS::MatrixKit const& HPSWidget::getPixelToWindowMatrix()
{
    if (!validPixelToWindowMatrix) {
        KeyArray key_array;
        key_array.push_back(canvas.GetWindowKey());
        KeyPath(key_array).ComputeTransform(HPS::Coordinate::Space::Pixel, HPS::Coordinate::Space::Window, pixelToWindowMatrix);
        validPixelToWindowMatrix = true;
    }

    return pixelToWindowMatrix;
}

void HPSWidget::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    HPS::Point pos(event->x(), event->y(), 0);
#else
    HPS::Point pos(event->position().x(), event->position().y(), 0);
#endif
    pos = getPixelToWindowMatrix().Transform(pos);

    HPS::MouseEvent out_event;
    out_event.CurrentAction = HPS::MouseEvent::Action::Scroll;
    out_event.Location = pos;

    // NOTE: the delta() function is obsolete as of QT5.
    // Try to replace it with pixelDelta or angleDelta
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out_event.WheelDelta = event->delta();
#else
    out_event.WheelDelta = event->angleDelta().y();
#endif

    getModifierKeys(&out_event);

    canvas.GetWindowKey().GetEventDispatcher().InjectEvent(out_event);
}

HPS::MouseEvent HPSWidget::buildMouseEvent(QMouseEvent* in_event, HPS::MouseEvent::Action action, size_t click_count)
{
    auto scaleFactor = this->devicePixelRatio();

    HPS::MouseEvent out_event;

    out_event.CurrentAction = action;
    out_event.ClickCount = click_count;

    if (in_event->button() == Qt::MouseButton::LeftButton)
        out_event.CurrentButton = HPS::MouseButtons::ButtonLeft();
    else if (in_event->button() == Qt::MouseButton::RightButton)
        out_event.CurrentButton = HPS::MouseButtons::ButtonRight();
    else if (in_event->button() == Qt::MouseButton::MiddleButton)
        out_event.CurrentButton = HPS::MouseButtons::ButtonMiddle();

    HPS::Point pos(in_event->x() * scaleFactor, in_event->y() * scaleFactor, 0);
    pos = getPixelToWindowMatrix().Transform(pos);
    out_event.Location = pos;

    getModifierKeys(&out_event);

    return out_event;
}

HPS::KeyboardEvent HPSWidget::buildKeyboardEvent(QKeyEvent* in_event, HPS::KeyboardEvent::Action action)
{
    HPS::KeyboardEvent out_event;
    out_event.CurrentAction = action;

    HPS::KeyboardCodeArray code;
    code.push_back((HPS::KeyboardCode)in_event->key());
    out_event.KeyboardCodes = code;

    getModifierKeys(&out_event);

    return out_event;
}

void HPSWidget::getModifierKeys(HPS::InputEvent* event)
{
    Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
    if (modifiers.testFlag(Qt::KeyboardModifier::ShiftModifier))
        event->ModifierKeyState.Shift(true);
    if (modifiers.testFlag(Qt::KeyboardModifier::ControlModifier))
        event->ModifierKeyState.Control(true);
}

void HPSWidget::dragEnterEvent(QDragEnterEvent* event) { event->acceptProposedAction(); }

void HPSWidget::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        QString filename = urls.at(0).toLocalFile();
#ifdef USING_EXCHANGE
        onFileOpen(filename);
#else
        QString ext = QFileInfo(filename).suffix().toLower();

        if (ext == "hsf" || ext == "stl" || ext == "obj")
            onFileOpen(filename);
#endif
    }
}

void HPSWidget::onFileNew()
{
    // Restore scene defaults if we already initialized our canvas
    if (canvas.Type() != HPS::Type::None) {
        setupSceneDefaults();
        canvas.Update();
    }

    initializeBrowsers();
}

HPS::Stream::ImportNotifier HPSWidget::importHSFFile(QString filename, QProgressDialog* progressDlg, bool& success)
{
    HPS::IOResult status = HPS::IOResult::Failure;
    HPS::Stream::ImportNotifier notifier;
    try {
        HPS::Stream::ImportOptionsKit ioKit;
        ioKit.SetSegment(model.GetSegmentKey());
        ioKit.SetAlternateRoot(model.GetLibraryKey());
        ioKit.SetPortfolio(model.GetPortfolioKey());

        notifier = HPS::Stream::File::Import(filename.toUtf8(), ioKit);
        float percent_complete = 0;
        status = notifier.Status(percent_complete);
        while (status == HPS::IOResult::InProgress) {
            if (progressDlg->wasCanceled()) {
                notifier.Cancel();
                progressDlg->setValue(0);
                success = false;
                return notifier;
            }
            progressDlg->setValue((int)(percent_complete * 100));
            status = notifier.Status(percent_complete);
        }
    }
    catch (HPS::IOException const& ex) {
        status = ex.result;
    }

    if (status == HPS::IOResult::Failure) {
        success = false;
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Error loading file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    else if (status == HPS::IOResult::FileNotFound) {
        success = false;
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Could not find file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    notifier.Wait();
    return notifier;
}

void HPSWidget::importSTLFile(QString filename, QProgressDialog* progressDlg, bool& success)
{
    HPS::IOResult status = HPS::IOResult::Failure;
    HPS::STL::ImportNotifier notifier;
    try {
        HPS::STL::ImportOptionsKit ioKit = HPS::STL::ImportOptionsKit::GetDefault();
        ioKit.SetSegment(model.GetSegmentKey());

        notifier = HPS::STL::File::Import(filename.toUtf8(), ioKit);
        float percent_complete = 0;
        status = notifier.Status(percent_complete);
        while (status == HPS::IOResult::InProgress) {
            if (progressDlg->wasCanceled()) {
                notifier.Cancel();
                progressDlg->setValue(0);
                success = false;
                return;
            }
            progressDlg->setValue((int)(percent_complete * 100));
            status = notifier.Status(percent_complete);
        }
    }
    catch (HPS::IOException const& ex) {
        success = false;
        status = ex.result;
    }

    if (status == HPS::IOResult::Failure) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Error loading file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    else if (status == HPS::IOResult::FileNotFound) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Could not find file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    notifier.Wait();
}

void HPSWidget::importOBJFile(QString filename, QProgressDialog* progressDlg, bool& success)
{
    HPS::IOResult status = HPS::IOResult::Failure;
    HPS::OBJ::ImportNotifier notifier;
    try {
        HPS::OBJ::ImportOptionsKit ioKit;
        ioKit.SetSegment(model.GetSegmentKey());
        ioKit.SetPortfolio(model.GetPortfolioKey());

        notifier = HPS::OBJ::File::Import(filename.toUtf8(), ioKit);
        float percent_complete = 0;
        status = notifier.Status(percent_complete);
        while (status == HPS::IOResult::InProgress) {
            if (progressDlg->wasCanceled()) {
                notifier.Cancel();
                progressDlg->setValue(0);
                success = false;
                return;
            }
            progressDlg->setValue((int)(percent_complete * 100));
            status = notifier.Status(percent_complete);
        }
    }
    catch (HPS::IOException const& ex) {
        status = ex.result;
        success = false;
    }

    if (status == HPS::IOResult::Failure) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Error loading file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    else if (status == HPS::IOResult::FileNotFound) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Could not find file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    notifier.Wait();
}

void HPSWidget::ImportPointCloudFile(QString filename, QProgressDialog* progressDlg, bool& success)
{
    HPS::IOResult status = HPS::IOResult::Failure;
    HPS::PointCloud::ImportNotifier notifier;
    try {
        HPS::PointCloud::ImportOptionsKit ioKit;
        ioKit.SetSegment(model.GetSegmentKey());

        notifier = HPS::PointCloud::File::Import(filename.toUtf8(), ioKit);
        float percent_complete = 0;
        status = notifier.Status(percent_complete);
        while (status == HPS::IOResult::InProgress) {
            if (progressDlg->wasCanceled()) {
                notifier.Cancel();
                progressDlg->setValue(0);
                success = false;
                return;
            }
            progressDlg->setValue((int)(percent_complete * 100));
            status = notifier.Status(percent_complete);
        }
    }
    catch (HPS::IOException const& ex) {
        status = ex.result;
        success = false;
    }

    if (status == HPS::IOResult::Failure) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Error loading file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    else if (status == HPS::IOResult::FileNotFound) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Could not find file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    notifier.Wait();
}

#ifdef USING_EXCHANGE
void HPSWidget::importExchangeFile(QString filename, bool& success, HPS::Exchange::ImportOptionsKit const* customImportOptions)
{
    HPS::IOResult status = HPS::IOResult::Failure;
    HPS::Exchange::ImportNotifier notifier;
    try {
        HPS::Exchange::ImportOptionsKit ioKit;
        ioKit.SetBRepMode(HPS::Exchange::BRepMode::BRepAndTessellation);

        notifier = HPS::Exchange::File::Import(filename.toUtf8(), customImportOptions == nullptr ? ioKit : *customImportOptions);

        ExchangeImportDialog dlg(notifier, this);
        int index = filename.lastIndexOf(QString("/"));
        if (index != -1)
            dlg.setWindowTitle(filename.right(filename.size() - index));
        else {
            int index = filename.lastIndexOf(QString("\\"));
            if (index != -1)
                dlg.setWindowTitle(filename.right(filename.size() - index));
            else
                dlg.setWindowTitle(filename);
        }
        dlg.exec();

        status = notifier.Status();
        success = dlg.WasImportSuccessful();
    }
    catch (HPS::IOException const& ex) {
        status = ex.result;
        success = false;
    }

    if (status == HPS::IOResult::Failure) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Error loading file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    else if (status == HPS::IOResult::FileNotFound) {
        QMessageBox msgBox;
        char error_message[1024];
        sprintf(error_message, "Could not find file %s", filename.toUtf8().constData());
        msgBox.setText(error_message);
        msgBox.exec();
    }

    cad_model = notifier.GetCADModel();
}

void HPSWidget::ActivateCapture(HPS::ComponentPath const& capturePath)
{
    Exchange::Capture capture = (Exchange::Capture)(capturePath.Front());
    View newView = capture.Activate(capturePath);
    SegmentKey newViewSegment = newView.GetSegmentKey();
    CameraKit newCamera;
    newViewSegment.ShowCamera(newCamera);

    newCamera.UnsetNearLimit();
    CameraKit defaultCameraWithoutNearLimit = CameraKit::GetDefault().UnsetNearLimit();
    if (newCamera == defaultCameraWithoutNearLimit) {
        View oldView = canvas.GetFrontView();
        CameraKit oldCamera;
        oldView.GetSegmentKey().ShowCamera(oldCamera);

        newViewSegment.SetCamera(oldCamera);
        newView.FitWorld();
    }

    AttachViewWithSmoothTransition(newView);
}

void HPSWidget::ImportConfiguration(HPS::UTF8Array const& configuration)
{
    if (configuration.empty())
        return;

    HPS::UTF8 filename = StringMetadata(cad_model.GetMetadata("Filename")).GetValue();
    HPS::Exchange::ImportOptionsKit options;
    options.SetConfiguration(configuration).SetBRepMode(HPS::Exchange::BRepMode::BRepAndTessellation);

    bool success = false;
    importExchangeFile(filename.GetBytes(), success, &options);

    if (success)
        updatePlanes();

    initializeBrowsers();
}

bool HPSWidget::s_mfrModelLoaded = false;
QString HPSWidget::s_mfrCkptPath;

QString HPSWidget::DefaultModelDir()
{
    // site-packages / models are resolved by the client (see main.cpp). Development builds locate the
    // checkpoints under HOOPS_AI_HOME; redistributed builds use the "models" folder next to the exe.
    char const* aiHome = getenv("HOOPS_AI_HOME");
    QString dir;
    if (aiHome != nullptr && *aiHome != '\0')
        dir = QDir::cleanPath(QString::fromLocal8Bit(aiHome) + "/packages/trained_ml_models");
    else
        dir = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../models");

    if (!QDir(dir).exists())
        dir = QCoreApplication::applicationDirPath();
    return dir;
}

QString HPSWidget::PromptForCheckpoint(QWidget* parent)
{
    return QFileDialog::getOpenFileName(parent, QObject::tr("Select Model Checkpoint"), DefaultModelDir(),
                                        QObject::tr("Checkpoints (*.ckpt)"));
}

void HPSWidget::ShowLoadError(QWidget* parent, QString const& title, QString const& fullMessage)
{
    // Backend load failures embed a full Python traceback (and can dump every state_dict key), which
    // makes a plain QMessageBox grow far taller than the screen. Keep the visible text to a concise
    // summary and push the raw message into the collapsible "Show Details…" area, which is scrollable.
    QString summary = fullMessage;
    int const nl = summary.indexOf(QLatin1Char('\n'));
    if (nl >= 0)
        summary = summary.left(nl);
    int const tb = summary.indexOf(QStringLiteral("Traceback"));
    if (tb >= 0)
        summary = summary.left(tb).trimmed();
    if (summary.isEmpty())
        summary = QObject::tr("Failed to load the model.");

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(title);
    box.setText(summary);
    if (fullMessage.trimmed() != summary)
        box.setDetailedText(fullMessage);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

bool HPSWidget::LoadMfrModelFrom(QString const& path, QString& out_errorMessage)
{
    if (path.isEmpty())
        return false;

    // Optional type heuristic: this is the MFR slot, so an "embed" file name is probably the wrong kind.
    QString const fileName = QFileInfo(path).fileName();
    if (fileName.toLower().contains("embed")) {
        QMessageBox::StandardButton const btn = QMessageBox::warning(
            nullptr, QObject::tr("Load MFR Model"),
            QObject::tr("The selected file name looks like a similarity-search model:\n%1\n"
                        "Load it as the MFR model anyway?").arg(fileName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return false;
    }

    char errBuf[8192] = {0};
    bool const ok = HoopsAI_LoadMFRModel(path.toUtf8().constData(), errBuf, sizeof(errBuf));
    if (!ok) {
        out_errorMessage = QObject::tr("Failed to load the selected file as an MFR model "
                                       "(it may be a different kind of model): %1")
                               .arg(errBuf);
        return false;
    }
    s_mfrCkptPath = path;
    s_mfrModelLoaded = true;
    return true;
}

bool HPSWidget::EnsureMfrReady(QString& out_errorMessage)
{
    if (s_mfrModelLoaded)
        return true;

    QString const path = PromptForCheckpoint(nullptr);
    if (path.isEmpty()) {
        out_errorMessage = QObject::tr("No model was selected");
        return false;
    }
    if (!LoadMfrModelFrom(path, out_errorMessage)) {
        if (out_errorMessage.isEmpty())
            out_errorMessage = QObject::tr("No model was selected");
        return false;
    }
    return true;
}

bool HPSWidget::s_embeddingsModelLoaded = false;
QString HPSWidget::s_embeddingsCkptPath;

bool HPSWidget::LoadEmbeddingsModelFrom(QString const& path, QString& out_errorMessage)
{
    if (path.isEmpty())
        return false;

    // Optional type heuristic: this is the Embeddings slot, so an "mfr" file name is probably wrong.
    QString const fileName = QFileInfo(path).fileName();
    if (fileName.toLower().contains("mfr")) {
        QMessageBox::StandardButton const btn = QMessageBox::warning(
            nullptr, QObject::tr("Load Similarity Search Model"),
            QObject::tr("The selected file name looks like an MFR model:\n%1\n"
                        "Load it as the similarity-search model anyway?").arg(fileName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return false;
    }

    char errBuf[8192] = {0};
    // Loading the embeddings checkpoint is the slow step; show the wait cursor immediately (right
    // after the file is chosen) instead of only once control returns to onSimilarityComparison.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();
    bool const ok = HoopsAI_LoadEmbeddingsModel(path.toUtf8().constData(), errBuf, sizeof(errBuf));
    QApplication::restoreOverrideCursor();
    if (!ok) {
        out_errorMessage = QObject::tr("Failed to load the selected file as a similarity-search model "
                                       "(it may be a different kind of model): %1")
                               .arg(errBuf);
        return false;
    }
    s_embeddingsCkptPath = path;
    s_embeddingsModelLoaded = true;
    return true;
}

bool HPSWidget::EnsureEmbeddingsReady(QString& out_errorMessage)
{
    if (s_embeddingsModelLoaded)
        return true;

    QString const path = PromptForCheckpoint(nullptr);
    if (path.isEmpty()) {
        out_errorMessage = QObject::tr("No model was selected");
        return false;
    }
    if (!LoadEmbeddingsModelFrom(path, out_errorMessage)) {
        if (out_errorMessage.isEmpty())
            out_errorMessage = QObject::tr("No model was selected");
        return false;
    }
    return true;
}

QMap<int, int> HPSWidget::ColorFacesByMfrLabels(std::vector<int> const& labels, int labelCount, bool& out_indexMismatch)
{
    QMap<int, int> labelCounts;
    out_indexMismatch = false;

    if (cad_model.Type() == HPS::Type::None)
        return labelCounts;

    // The MFR labels are one-per-CAD-face, in the same order as the ExchangeTopoFace components of
    // the model, so the faces are enumerated directly (there is no need to go through the
    // ExchangeTopoShell topology). The number of ExchangeTopoFace components is expected to match
    // labelCount.
    ComponentArray const faces = cad_model.GetAllSubcomponents(Component::ComponentType::ExchangeTopoFace);

    if (static_cast<int>(faces.size()) != labelCount)
        out_indexMismatch = true;

    for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        int label = 0;
        if (faceIndex < static_cast<size_t>(labelCount))
            label = labels[faceIndex];
        else
            out_indexMismatch = true;

        labelCounts[label] = labelCounts.value(label, 0) + 1;

        RGBColor const color = ColorForMfrLabel(label);

        // Each ExchangeTopoFace is tessellated into its own Visualize shell. That shell is made up of
        // many polygon faces (the triangles of the tessellation), all of which belong to this single
        // CAD face, so every polygon face of the shell is colored with the CAD face's MFR color.
        for (Key const& key : faces[faceIndex].GetKeys()) {
            ShellKey shellKey(key);
            if (shellKey.Type() == HPS::Type::ShellKey)
                shellKey.SetFaceRGBColorsByRange(0, shellKey.GetFaceCount(), color);
        }
    }

    return labelCounts;
}

void HPSWidget::UpdateMfrLegend(QMap<int, int> const& labelCounts)
{
    delete mfrLegendPanel;
    mfrLegendPanel = nullptr;

    if (labelCounts.isEmpty())
        return;

    QWidget* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("mfrLegendPanel"));
    // WA_TranslucentBackground requires a compositing window manager to alpha-blend the panel
    // over the HOOPS canvas; on Linux setups without a compositor (e.g. this VirtualBox VM's
    // window manager) the panel's frame shows but its semi-transparent content never gets
    // composited, so the legend appears empty. Use a fully opaque background instead - it looks
    // the same in practice and does not depend on compositing being available.
    // The stylesheet is scoped to the #mfrLegendPanel object name (rather than the QWidget type)
    // so it does not cascade down to the row/title child widgets - an unscoped stylesheet would
    // otherwise paint the same background/border on every child QWidget too, drawing a box around
    // each row and squeezing its text against that border.
    panel->setStyleSheet(QStringLiteral("#mfrLegendPanel { background-color: rgb(255, 255, 255); border: 1px solid gray; }"));

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    QWidget* titleRow = new QWidget(panel);
    QHBoxLayout* titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    QLabel* title = new QLabel("MFR Result", titleRow);
    title->setStyleSheet("font-weight: bold; border: none;");
    titleLayout->addWidget(title);
    titleLayout->addStretch();

    QPushButton* closeButton = new QPushButton("X", titleRow);
    closeButton->setFixedSize(16, 16);
    closeButton->setToolTip("Clear MFR colors");
    closeButton->setStyleSheet("QPushButton { border: 1px solid gray; font-weight: bold; padding: 0; }");
    connect(closeButton, &QPushButton::clicked, this, &HPSWidget::ClearMfrColors);
    titleLayout->addWidget(closeButton);

    layout->addWidget(titleRow);

    for (auto it = labelCounts.constBegin(); it != labelCounts.constEnd(); ++it) {
        int const label = it.key();
        int const faceCount = it.value();
        HPS::RGBColor const color = ColorForMfrLabel(label);

        QWidget* row = new QWidget(panel);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        QFrame* swatch = new QFrame(row);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid black;")
                                   .arg(static_cast<int>(color.red * 255))
                                   .arg(static_cast<int>(color.green * 255))
                                   .arg(static_cast<int>(color.blue * 255)));
        rowLayout->addWidget(swatch);

        QLabel* text = new QLabel(QString("%1 (%2)").arg(MfrLabelDisplayName(label)).arg(faceCount), row);
        text->setStyleSheet("border: none;");
        rowLayout->addWidget(text);
        rowLayout->addStretch();

        layout->addWidget(row);
    }

    panel->adjustSize();
    mfrLegendPanel = panel;
    RepositionMfrLegend();
    mfrLegendPanel->show();
    mfrLegendPanel->raise();
}

void HPSWidget::RepositionMfrLegend()
{
    if (mfrLegendPanel == nullptr)
        return;

    int const margin = 10;
    mfrLegendPanel->move(width() - mfrLegendPanel->width() - margin, margin);
    mfrLegendPanel->raise();
}

void HPSWidget::ClearMfrColors()
{
    if (cad_model.Type() != HPS::Type::None) {
        ComponentArray const faces = cad_model.GetAllSubcomponents(Component::ComponentType::ExchangeTopoFace);
        for (Component const& faceComponent : faces) {
            for (Key const& key : faceComponent.GetKeys()) {
                ShellKey shellKey(key);
                if (shellKey.Type() == HPS::Type::ShellKey)
                    shellKey.UnsetFaceColors();
            }
        }
        canvas.Update();
    }

    delete mfrLegendPanel;
    mfrLegendPanel = nullptr;
}
#endif

void HPSWidget::AttachView(HPS::View const& in_view)
{
    HPS::View old_view = canvas.GetFrontView();

    canvas.AttachViewAsLayout(in_view);

    HPS::OperatorPtrArray operators;
    auto oldViewOperatorCtrl = old_view.GetOperatorControl();
    auto newViewOperatorCtrl = in_view.GetOperatorControl();
    oldViewOperatorCtrl.Show(HPS::Operator::Priority::Low, operators);
    newViewOperatorCtrl.Set(operators, HPS::Operator::Priority::Low);
    oldViewOperatorCtrl.Show(HPS::Operator::Priority::Default, operators);
    newViewOperatorCtrl.Set(operators, HPS::Operator::Priority::Default);
    oldViewOperatorCtrl.Show(HPS::Operator::Priority::High, operators);
    newViewOperatorCtrl.Set(operators, HPS::Operator::Priority::High);

    HPS::DistantLightKit light;
    light.SetDirection(HPS::Vector(1, 0, -1.5f));
    light.SetCameraRelative(true);

    // Delete previous light before inserting new one
    if (mainDistantLight.Type() != HPS::Type::None)
        mainDistantLight.Delete();
    mainDistantLight = canvas.GetFrontView().GetSegmentKey().InsertDistantLight(light);

    old_view.Delete();

    view = in_view;

    // A recreated view starts without the triad, so re-enable it on every attach.
    EnableAxisTriad(view);
}

void HPSWidget::ApplyViewBackground()
{
    // Solid background color for the view. It lives on the window key (not the view segment): the
    // view's subwindow is transparent by default, so the window background shows through behind the
    // model, and the window key is stable across model loads so the color persists. Setting it on
    // the view segment instead makes that subwindow an opaque fill that renders black.
    HPS::RGBAColor const bg(217.0f / 255.0f, 211.0f / 255.0f, 199.0f / 255.0f);

    HPS::WindowKey windowKey = canvas.GetWindowKey();
    windowKey.GetSubwindowControl().SetBackground(HPS::Subwindow::Background::SolidColor);
    windowKey.GetMaterialMappingControl().SetWindowColor(bg);
}

void HPSWidget::EnableAxisTriad(HPS::View const& in_view)
{
    // Show the orientation axis triad anchored in the lower-left corner of the view. Interactive so
    // the user can click an axis to snap the camera to that standard orientation.
    HPS::View(in_view).GetAxisTriadControl()
        .SetVisibility(true)
        .SetLocation(HPS::AxisTriadControl::Location::BottomLeft)
        .SetInteractivity(true);
}

void HPSWidget::AttachViewWithSmoothTransition(View& newView)
{
    View oldView = canvas.GetFrontView();
    CameraKit oldCamera;
    oldView.GetSegmentKey().ShowCamera(oldCamera);

    SegmentKey newViewSegment = newView.GetSegmentKey();
    CameraKit newCamera;
    newViewSegment.ShowCamera(newCamera);

    AttachView(newView);

    newViewSegment.SetCamera(oldCamera);
    newView.SmoothTransition(newCamera);
}

void HPSWidget::Unhighlight()
{
    HighlightOptionsKit highlightOptions;
    highlightOptions.SetStyleName("highlight_style").SetNotification(true);

    canvas.GetWindowKey().GetHighlightControl().Unhighlight(highlightOptions);
    EventDispatcher dispatcher = Database::GetEventDispatcher();
    dispatcher.InjectEvent(
        HPS::HighlightEvent(HPS::HighlightEvent::Action::Unhighlight, HPS::SelectionResults(), highlightOptions));
    dispatcher.InjectEvent(HPS::ComponentHighlightEvent(
        HPS::ComponentHighlightEvent::Action::Unhighlight, canvas, 0, HPS::ComponentPath(), highlightOptions));
}

void HPSWidget::ShowShapeMap(const QVector<ShapeMapPoint>& points,
                             const QVector<ShapeMapLegendEntry>& legend)
{
    // Remove any previous map (its segment tree, remembered positions and legend overlay).
    m_shapeMapPoints.clear();
    m_shapeMapGroupSegments.clear();
    if (m_shapeMapSegment.Type() != HPS::Type::None) {
        m_shapeMapSegment.Delete();
        m_shapeMapSegment = HPS::SegmentKey();
    }
    m_shapeMapHighlightSegment = HPS::SegmentKey();
    ShowShapeMapLegend(QVector<ShapeMapLegendEntry>()); // clear any existing legend overlay
    if (points.isEmpty())
        return;

#ifdef USING_EXCHANGE
    // The map is shown on a clean scene, so the loaded CAD - and with it any MFR / compare result -
    // is about to be dropped below. Their legends are plain Qt overlays that would otherwise survive
    // the model they describe, so retire them now, while cad_model is still valid.
    ClearFaceAnalysisOverlays();
#endif

    // Present the map on a clean scene: drop any loaded CAD (both the Visualize model and, for
    // Exchange, the CADModel) and replace the current front view (which may be an Exchange capture
    // sub-view) with a fresh base view. Otherwise the map points would be drawn over the loaded
    // geometry, and for Exchange the attached model belongs to the CAD (mixed coordinate systems).
    HPS::View freshView = HPS::Factory::CreateView();
    AttachView(freshView); // front view <- freshView; old view deleted; `view` updated to freshView
    if (cad_model.Type() != HPS::Type::None) {
        cad_model.Delete();
        cad_model = HPS::CADModel(); // drop the dangling key so browsers re-init to an empty model
    }
    if (model.Type() != HPS::Type::None)
        model.Delete();
    model = HPS::Factory::CreateModel();
    view.AttachModel(model);
    initializeBrowsers(); // clear model/segment/config browsers that referenced the unloaded CAD

    HPS::SegmentKey mapSeg = model.GetSegmentKey().Subsegment("shape_map");
    m_shapeMapSegment = mapSeg;
    // The map is a marker cloud. Each point is a standalone Marker primitive (not a shell vertex):
    // markers honor the segment's MaterialMappingControl marker color, whereas shell vertices are
    // drawn in the geometry's (unset -> black) vertex color, which is why the earlier shell-based
    // approach showed only black dots. Set the shared glyph/size on the root; color is set per group.
    mapSeg.GetVisibilityControl().SetMarkers(true).SetFaces(false).SetEdges(false).SetLines(false);
    mapSeg.GetMarkerAttributeControl()
        .SetSymbol("solid circle")
        .SetSize(0.007f, HPS::Marker::SizeUnits::SubscreenRelative);

    // Group point indices by their resolved color so each distinct color becomes one subsegment
    // with a single marker color. Also remember each id's position for HighlightShapeMapPoint.
    QMap<QString, QVector<int>> byColor;
    for (int i = 0; i < points.size(); ++i) {
        ShapeMapPoint const& p = points[i];
        QString const colorKey = QStringLiteral("%1_%2_%3")
                                     .arg(int(p.cr * 255.0f))
                                     .arg(int(p.cg * 255.0f))
                                     .arg(int(p.cb * 255.0f));
        byColor[colorKey].push_back(i);
        m_shapeMapPoints.insert(p.id, HPS::Point(p.x, p.y, p.z));
    }

    int groupIdx = 0;
    for (auto it = byColor.constBegin(); it != byColor.constEnd(); ++it, ++groupIdx) {
        QVector<int> const& idxs = it.value();
        ShapeMapPoint const& first = points[idxs.first()];
        HPS::SegmentKey colorSeg = mapSeg.Subsegment(
            (QStringLiteral("group_") + QString::number(groupIdx)).toUtf8().constData());
        colorSeg.GetVisibilityControl().SetMarkers(true);
        colorSeg.GetMaterialMappingControl().SetMarkerColor(
            HPS::RGBAColor(first.cr, first.cg, first.cb, 1.0f));
        for (int j = 0; j < idxs.size(); ++j) {
            ShapeMapPoint const& p = points[idxs[j]];
            colorSeg.InsertMarker(HPS::Point(p.x, p.y, p.z));
        }
        // Remember the group segment by its color key so the legend checkboxes can toggle it.
        m_shapeMapGroupSegments.insert(it.key(), colorSeg);
    }

    // Reference XYZ grid planes / axis labels around the cloud, so it reads like a 3D plot.
    AddShapeMapGrid(mapSeg, points);

    // View the map from an isometric angle (Z up), so the three grid planes and the point depth are
    // visible at once, then frame the whole map. FitWorld keeps this orientation and only fits the
    // camera distance/field to the map extents.
    {
        HPS::Point lo(m_shapeMapBounds[0], m_shapeMapBounds[1], m_shapeMapBounds[2]);
        HPS::Point hi(m_shapeMapBounds[3], m_shapeMapBounds[4], m_shapeMapBounds[5]);
        HPS::Point center((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f);
        HPS::Vector dir(1.0f, -1.2f, 0.85f);
        dir.Normalize();
        HPS::Point pos = center + dir;
        canvas.GetFrontView().GetSegmentKey().GetCameraControl()
            .SetTarget(center)
            .SetPosition(pos)
            .SetUpVector(HPS::Vector(0.0f, 0.0f, 1.0f));
    }

    // Frame the whole map, then show the matching legend overlay.
    canvas.GetFrontView().FitWorld(mapSeg);
    canvas.Update();
    ShowShapeMapLegend(legend);
}

void HPSWidget::AddShapeMapGrid(HPS::SegmentKey mapSeg, const QVector<ShapeMapPoint>& points)
{
    if (points.isEmpty())
        return;

    // --- Bounding box of the point cloud ------------------------------------
    float xmin = points[0].x, xmax = xmin;
    float ymin = points[0].y, ymax = ymin;
    float zmin = points[0].z, zmax = zmin;
    for (const ShapeMapPoint& p : points) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
        zmin = std::min(zmin, p.z); zmax = std::max(zmax, p.z);
    }

    // --- "Nice" rounded range + tick step per axis (~6 divisions) -----------
    auto niceAxis = [](float lo, float hi, float& nlo, float& nhi, float& step) {
        float range = hi - lo;
        if (range <= 1e-6f) range = 1.0f;
        float raw = range / 6.0f;
        float mag = std::pow(10.0f, std::floor(std::log10(raw)));
        float norm = raw / mag;
        float niceNorm = (norm < 1.5f) ? 1.0f : (norm < 3.0f) ? 2.0f : (norm < 7.0f) ? 5.0f : 10.0f;
        step = niceNorm * mag;
        nlo = std::floor(lo / step) * step;
        nhi = std::ceil(hi / step) * step;
    };
    float x0, x1, xs, y0, y1, ys, z0, z1, zs;
    niceAxis(xmin, xmax, x0, x1, xs);
    niceAxis(ymin, ymax, y0, y1, ys);
    niceAxis(zmin, zmax, z0, z1, zs);

    // Remember the rounded box so ShowShapeMap can aim the isometric camera at its center.
    m_shapeMapBounds[0] = x0; m_shapeMapBounds[1] = y0; m_shapeMapBounds[2] = z0;
    m_shapeMapBounds[3] = x1; m_shapeMapBounds[4] = y1; m_shapeMapBounds[5] = z1;

    // Grid geometry lives under the map segment so it is removed together with the map. Medium-grey
    // lines and dark axis titles, so both read on the light view background. Only lines and text show.
    HPS::SegmentKey grid = mapSeg.Subsegment("grid");
    grid.GetVisibilityControl().SetLines(true).SetText(true)
        .SetMarkers(false).SetFaces(false).SetEdges(false);
    // The grid is decoration only: keep it out of picking so clicking a grid line does not resolve
    // to (and highlight) the nearest map point.
    grid.GetSelectabilityControl().SetEverything(false);
    grid.GetMaterialMappingControl()
        .SetLineColor(HPS::RGBAColor(0.45f, 0.45f, 0.48f, 1.0f))
        .SetTextColor(HPS::RGBAColor(0.15f, 0.15f, 0.18f, 1.0f));
    grid.GetLineAttributeControl().SetWeight(1.0f);
    grid.GetTextAttributeControl()
        .SetSize(12.0f, HPS::Text::SizeUnits::Points)
        .SetAlignment(HPS::Text::Alignment::Center);

    auto ticks = [](float a, float b, float s, QVector<float>& out) {
        for (float v = a; v <= b + s * 0.5f; v += s) out.push_back(v);
    };
    QVector<float> xt, yt, zt;
    ticks(x0, x1, xs, xt);
    ticks(y0, y1, ys, yt);
    ticks(z0, z1, zs, zt);

    auto line = [&](float ax, float ay, float az, float bx, float by, float bz) {
        grid.InsertLine(HPS::Point(ax, ay, az), HPS::Point(bx, by, bz));
    };

    // --- Grid lines on three planes: floor (z=z0), back (y=y1), left (x=x0) -
    for (float y : yt) line(x0, y, z0, x1, y, z0);   // floor: along X
    for (float x : xt) line(x, y0, z0, x, y1, z0);   // floor: along Y
    for (float z : zt) line(x0, y1, z, x1, y1, z);   // back wall: along X
    for (float x : xt) line(x, y1, z0, x, y1, z1);   // back wall: along Z
    for (float z : zt) line(x0, y0, z, x0, y1, z);   // left wall: along Y
    for (float y : yt) line(x0, y, z0, x0, y, z1);   // left wall: along Z

    // Axis titles only (the map coordinates are unitless PCA-projection values, so numeric ticks
    // would be meaningless). Placed just outside the box near each axis' positive end.
    float const ox = (x1 - x0) * 0.06f + 1e-3f;
    float const oy = (y1 - y0) * 0.06f + 1e-3f;
    auto label = [&](float px, float py, float pz, const QString& s) {
        grid.InsertText(HPS::Point(px, py, pz), s.toUtf8().constData());
    };
    label((x0 + x1) * 0.5f, y0 - oy * 2.0f, z0, QStringLiteral("X"));
    label(x1 + ox * 2.0f, (y0 + y1) * 0.5f, z0, QStringLiteral("Y"));
    label(x0 - ox * 2.0f, y0 - oy, (z0 + z1) * 0.5f, QStringLiteral("Z"));
}

void HPSWidget::ShowShapeMapLegend(const QVector<ShapeMapLegendEntry>& legend)
{
    delete m_shapeMapLegendPanel;
    m_shapeMapLegendPanel = nullptr;
    if (legend.isEmpty())
        return;

    QWidget* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("shapeMapLegendPanel"));
    // See UpdateMfrLegend for why the background must be opaque (no compositor on this Linux
    // setup) and why the stylesheet is scoped to the panel's object name rather than the QWidget
    // type (otherwise it cascades to the row/title children, drawing a box around each row and
    // squeezing its text against that border).
    panel->setStyleSheet(QStringLiteral("#shapeMapLegendPanel { background-color: rgb(255, 255, 255); border: 1px solid gray; }"));

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    QWidget* titleRow = new QWidget(panel);
    QHBoxLayout* titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);
    QLabel* title = new QLabel("Shape Embedding Map", titleRow);
    title->setStyleSheet("font-weight: bold; border: none;");
    titleLayout->addWidget(title);
    titleLayout->addStretch();
    QPushButton* closeButton = new QPushButton("X", titleRow);
    closeButton->setFixedSize(16, 16);
    closeButton->setToolTip("Clear the shape map");
    closeButton->setStyleSheet("QPushButton { border: 1px solid gray; font-weight: bold; padding: 0; }");
    connect(closeButton, &QPushButton::clicked, this, [this]() {
        if (m_shapeMapSegment.Type() != HPS::Type::None) {
            m_shapeMapSegment.Delete();
            m_shapeMapSegment = HPS::SegmentKey();
        }
        m_shapeMapHighlightSegment = HPS::SegmentKey();
        m_shapeMapPoints.clear();
        m_shapeMapGroupSegments.clear();
        delete m_shapeMapLegendPanel;
        m_shapeMapLegendPanel = nullptr;
        canvas.Update();
    });
    titleLayout->addWidget(closeButton);
    layout->addWidget(titleRow);

    for (const ShapeMapLegendEntry& e : legend) {
        QWidget* row = new QWidget(panel);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        // Per-group show/hide toggle. Checked = the group's markers are visible in the 3D view.
        // The color key matches the "r_g_b" grouping used to build the map subsegments.
        QString const colorKey = QStringLiteral("%1_%2_%3")
                                     .arg(int(e.r * 255.0f))
                                     .arg(int(e.g * 255.0f))
                                     .arg(int(e.b * 255.0f));
        QCheckBox* toggle = new QCheckBox(row);
        toggle->setChecked(true);
        toggle->setToolTip(QString("Show/hide %1 points").arg(e.label));
        toggle->setStyleSheet("border: none;");
        connect(toggle, &QCheckBox::toggled, this,
                [this, colorKey](bool checked) { SetShapeMapGroupVisible(colorKey, checked); });
        rowLayout->addWidget(toggle);

        QFrame* swatch = new QFrame(row);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid black;")
                                  .arg(int(e.r * 255.0f))
                                  .arg(int(e.g * 255.0f))
                                  .arg(int(e.b * 255.0f)));
        rowLayout->addWidget(swatch);

        QLabel* text = new QLabel(QString("%1 (%2)").arg(e.label).arg(e.count), row);
        text->setStyleSheet("border: none;");
        rowLayout->addWidget(text);
        rowLayout->addStretch();

        layout->addWidget(row);
    }

    panel->adjustSize();
    m_shapeMapLegendPanel = panel;
    RepositionShapeMapLegend();
    m_shapeMapLegendPanel->show();
    m_shapeMapLegendPanel->raise();
}

void HPSWidget::RepositionShapeMapLegend()
{
    if (m_shapeMapLegendPanel == nullptr)
        return;
    int const margin = 10;
    m_shapeMapLegendPanel->move(margin, margin); // top-left (the MFR legend uses the top-right)
    m_shapeMapLegendPanel->raise();
}

void HPSWidget::SetShapeMapGroupVisible(const QString& colorKey, bool visible)
{
    auto const it = m_shapeMapGroupSegments.constFind(colorKey);
    if (it == m_shapeMapGroupSegments.constEnd())
        return;
    HPS::SegmentKey seg = it.value();
    if (seg.Type() == HPS::Type::None)
        return;
    seg.GetVisibilityControl().SetMarkers(visible);
    canvas.Update();
}

void HPSWidget::HighlightShapeMapPoint(const QString& partId)
{
    if (m_shapeMapSegment.Type() == HPS::Type::None)
        return;
    // Rebuild the single-marker highlight subsegment from scratch each time.
    if (m_shapeMapHighlightSegment.Type() != HPS::Type::None) {
        m_shapeMapHighlightSegment.Delete();
        m_shapeMapHighlightSegment = HPS::SegmentKey();
    }
    auto const it = m_shapeMapPoints.constFind(partId);
    if (partId.isEmpty() || it == m_shapeMapPoints.constEnd()) {
        canvas.Update();
        return;
    }
    HPS::SegmentKey hi = m_shapeMapSegment.Subsegment("highlight");
    m_shapeMapHighlightSegment = hi;
    hi.GetVisibilityControl().SetMarkers(true);
    hi.GetMaterialMappingControl().SetMarkerColor(HPS::RGBAColor(1.0f, 1.0f, 0.0f, 1.0f)); // yellow
    hi.GetMarkerAttributeControl()
        .SetSymbol("solid circle")
        .SetSize(0.018f, HPS::Marker::SizeUnits::SubscreenRelative);
    hi.InsertMarker(it.value());
    canvas.Update();
}

bool HPSWidget::PickShapeMapPart(HPS::SelectionResults const& results)
{
    if (m_shapeMapSegment.Type() == HPS::Type::None || m_shapeMapPoints.isEmpty())
        return false;
    if (results.GetCount() == 0)
        return false;

    // Resolve the world-space selection position and map it back to the nearest stored point's
    // part id. This avoids depending on marker key identity and is robust even if the pick reports
    // a parent segment rather than the exact marker.
    HPS::SelectionResultsIterator it = results.GetIterator();
    HPS::WorldPoint pickPos;
    if (!it.IsValid() || !it.GetItem().ShowSelectionPosition(pickPos))
        return false;

    QString bestId;
    float bestDistSq = 0.0f;
    for (auto pit = m_shapeMapPoints.constBegin(); pit != m_shapeMapPoints.constEnd(); ++pit) {
        HPS::Point const& p = pit.value();
        float const dx = p.x - pickPos.x;
        float const dy = p.y - pickPos.y;
        float const dz = p.z - pickPos.z;
        float const distSq = dx * dx + dy * dy + dz * dz;
        if (bestId.isEmpty() || distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = pit.key();
        }
    }
    if (bestId.isEmpty())
        return false;

    HighlightShapeMapPoint(bestId);         // yellow marker on the picked point (view side)
    emit shapeMapPartPicked(bestId);        // ask the panel to select the matching gallery item
    return true;
}

bool HPSWidget::ClearShapeMapHighlight()
{
    if (m_shapeMapSegment.Type() == HPS::Type::None)
        return false;                       // no map shown -> nothing to clear
    HighlightShapeMapPoint(QString());      // remove the yellow highlight marker (view side)
    emit shapeMapPartPicked(QString());     // ask the panel to clear its gallery selection
    return true;
}

#ifdef USING_EXCHANGE
// Linux (and most Unix) filesystems are case-sensitive, so a plain "*.sldprt" glob would silently
// hide files saved with an upper-case extension (e.g. "part.SLDPRT", common with SolidWorks/CAD
// exports). A [aA]-style bracket class fixes that in Qt's own matcher, but the native Windows file
// dialog does NOT understand bracket classes and would then stop showing "*.SLDPRT" entirely.
// To work on every platform (native Windows dialog included), emit an explicit lower-case AND
// upper-case variant of each space-separated pattern token instead.
static QString CaseInsensitiveGlob(const QString& exts)
{
    QStringList out;
    const QStringList tokens = exts.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    out.reserve(tokens.size() * 2);
    for (const QString& tok : tokens) {
        const QString lower = tok.toLower();
        const QString upper = tok.toUpper();
        if (!out.contains(lower))
            out << lower;
        if (upper != lower && !out.contains(upper))
            out << upper;
    }
    return out.join(QLatin1Char(' '));
}

QString HPSWidget::exchangeFileDialogFilter() const
{
    auto entry = [](const QString& label, const QString& exts) {
        return label + QStringLiteral(" (") + CaseInsensitiveGlob(exts) + QStringLiteral(")");
    };

    const QString allExts =
        QStringLiteral("*.3ds *.3mf *.3dxml *.sat *.sab *.dgn *.dwg *.dxf *.dwf *.dwfx *_pd *.model *.dlv *.exp "
                        "*.session *.CATPart *.CATProduct *.CATShape *.CATDrawing "
                        "*.cgr *.dae *.prt *.prt.* *.neu *.neu.* *.asm *.asm.* *.xas *.xpr *.fbx *.gltf *.glb *.arc *.unv *.mf1 *.prt "
                        "*.pkg *.ifc *.ifczip *.igs *.iges *.ipt *.iam "
                        "*.jt *.kmz *.nwd *.prt *.pdf *.prc *.x_t *.xmt *.x_b *.xmt_txt *.rvt *.3dm *.stp *.step *.stpz *.stp.z *.stl "
                        "*.par *.asm *.pwd *.psm "
                        "*.sldprt *.sldasm *.sldfpp *.asm *.u3d *.vda *.wrl *.vrml *.obj *.xv3 *.xv0 *.hsf *.ptx *.pts *.xyz");

    QStringList filters;
    filters << entry(tr("All Supported Files"), allExts);
    filters << entry(tr("HOOPS Stream Files"), QStringLiteral("*.hsf"));
    filters << entry(tr("StereoLithography Files"), QStringLiteral("*.stl"));
    filters << entry(tr("Wavefront Files"), QStringLiteral("*.obj"));
    filters << entry(tr("Point Cloud Files"), QStringLiteral("*.ptx *.pts *.xyz"));
    filters << entry(tr("3D Studio Files"), QStringLiteral("*.3ds"));
    filters << entry(tr("3D Manufacturing Files"), QStringLiteral("*.3mf"));
    filters << entry(tr("3DXML Files"), QStringLiteral("*.3dxml"));
    filters << entry(tr("ACIS SAT Files"), QStringLiteral("*.sat *.sab"));
    filters << entry(tr("AutoCAD Files"), QStringLiteral("*.dwg *.dxf"));
    filters << entry(tr("Autodesk DWF Files"), QStringLiteral("*.dwf *.dwfx"));
    filters << entry(tr("CADDS Files"), QStringLiteral("*_pd"));
    filters << entry(tr("CATIA V4 Files"), QStringLiteral("*.model *.dlv *.exp *.session"));
    filters << entry(tr("CATIA V5 Files"), QStringLiteral("*.CATPart *.CATProduct *.CATShape *.CATDrawing"));
    filters << entry(tr("CGR Files"), QStringLiteral("*.cgr"));
    filters << entry(tr("Collada Files"), QStringLiteral("*.dae"));
    filters << entry(tr("Creo (ProE) Files"), QStringLiteral("*.prt *.prt.* *.neu *.neu.* *.asm *.asm.* *.xas *.xpr"));
    filters << entry(tr("DGN Files"), QStringLiteral("*.dgn"));
    filters << entry(tr("FBX Files"), QStringLiteral("*.fbx"));
    filters << entry(tr("GLTF Files"), QStringLiteral("*.gltf *.glb"));
    filters << entry(tr("I-DEAS Files"), QStringLiteral("*.arc *.unv *.mf1 *.prt *.pkg"));
    filters << entry(tr("IFC Files"), QStringLiteral("*.ifc *.ifczip"));
    filters << entry(tr("IGES Files"), QStringLiteral("*.igs *.iges"));
    filters << entry(tr("Inventor Files"), QStringLiteral("*.ipt *.iam"));
    filters << entry(tr("JT Files"), QStringLiteral("*.jt"));
    filters << entry(tr("KMZ Files"), QStringLiteral("*.kmz"));
    filters << entry(tr("Navisworks Files"), QStringLiteral("*.nwd"));
    filters << entry(tr("NX (Unigraphics) Files"), QStringLiteral("*.prt"));
    filters << entry(tr("PDF Files"), QStringLiteral("*.pdf"));
    filters << entry(tr("PRC Files"), QStringLiteral("*.prc"));
    filters << entry(tr("Parasolid Files"), QStringLiteral("*.x_t *.xmt *.x_b *.xmt_txt"));
    filters << entry(tr("Revit Files"), QStringLiteral("*.rvt"));
    filters << entry(tr("Rhino Files"), QStringLiteral("*.3dm"));
    filters << entry(tr("STEP Files"), QStringLiteral("*.stp *.step *.stpz *.stp.z"));
    filters << entry(tr("SolidEdge Files"), QStringLiteral("*.par *.asm *.pwd *.psm"));
    filters << entry(tr("SolidWorks Files"), QStringLiteral("*.sldprt *.sldasm *.sldfpp *.asm"));
    filters << entry(tr("Universal 3D Files"), QStringLiteral("*.u3d"));
    filters << entry(tr("VDA Files"), QStringLiteral("*.vda"));
    filters << entry(tr("VRML Files"), QStringLiteral("*.wrl *.vrml"));
    filters << entry(tr("WaveFront Files"), QStringLiteral("*.obj"));
    filters << entry(tr("XVL Files"), QStringLiteral("*.xv3 *.xv0"));
    filters << tr("All Files (*.*)");

    return filters.join(QStringLiteral(";;"));
}
#endif

void HPSWidget::onFileAdd(QString filename)
{
#ifdef USING_EXCHANGE
    if (cad_model.Type() == HPS::Type::None) {
        QMessageBox::warning(this, tr("Add File"), tr("No model is currently open. Open a model before adding another."));
        return;
    }

    if (filename.isEmpty()) {
        QSettings settings;
        QString const last = settings.value(QStringLiteral("lastModelDir")).toString();
        filename = QFileDialog::getOpenFileName(this, tr("Add File"),
                                                last.isEmpty() ? QStringLiteral(".") : last,
                                                exchangeFileDialogFilter(), 0);
        if (!filename.isEmpty())
            settings.setValue(QStringLiteral("lastModelDir"), QFileInfo(filename).absolutePath());
    }
    if (filename.isEmpty())
        return;

    HPS::ComponentPath location;
    location.PushBack(cad_model);

    HPS::Exchange::ImportOptionsKit options;
    options.SetBRepMode(HPS::Exchange::BRepMode::BRepAndTessellation);
    options.SetLocation(location);

    /* A newly added model invalidates any previously displayed MFR result, and any previous compare
        diff (its stored face shells would become stale). */
    ClearFaceAnalysisOverlays();

    bool success = false;
    importExchangeFile(filename, success, &options);

    if (success) {
        updatePlanes();
        m_secondPartPath = filename;
        // Use the same plain cast the rest of this file uses for the owning main window (browsers,
        // setCurrentFile, ...). qobject_cast here could return null in edge cases (RTTI/type-info
        // across module boundaries) even though the widget is the main window's central widget, which
        // would silently skip enabling the Similarity Comparison menu while the add itself succeeds.
        HPSMainWindow* mw = (HPSMainWindow*)parentWidget();
        if (mw)
            mw->updateModelDependentActions();
    }

    initializeBrowsers();
#else
    Q_UNUSED(filename);
#endif
}

void HPSWidget::onFileOpen(QString filename)
{
    QSettings settings;
    QString const startDir = settings.value(QStringLiteral("lastModelDir"),
                                             QStringLiteral(".")).toString();
    bool const fromDialog = filename.isEmpty();
#ifdef USING_EXCHANGE
    if (filename.isEmpty())
        filename = QFileDialog::getOpenFileName(this, tr("Open File"), startDir, exchangeFileDialogFilter(), 0);
#else
    if (filename.isEmpty())
        filename = QFileDialog::getOpenFileName(this,
                                                tr("Open File"),
                                                startDir,
                                                tr("HOOPS Stream Files (*.hsf);;StereoLithography Files (*.stl);;Wavefront Files "
                                                   "(*.obj);;Point Cloud Files (*.ptx *.pts *.xyz)"),
                                                0);
#endif
    if (fromDialog && !filename.isEmpty())
        settings.setValue(QStringLiteral("lastModelDir"), QFileInfo(filename).absolutePath());

    if (filename.size() > 0) {
        QProgressDialog* progressDlg;
        progressDlg = new QProgressDialog("Loading File...", "Cancel", 0, 100, parentWidget());
#ifndef USING_EXCHANGE
        progressDlg->setWindowModality(Qt::WindowModal);
        progressDlg->setValue(0);
        progressDlg->show();
#endif

        // Any previously shown shape map lives as a subsegment of a model that is about to be
        // torn down and rebuilt below. Drop the cached keys unconditionally so a later
        // ShowShapeMap never dereferences or deletes a dangling segment. This must run even when
        // `model` is already None (e.g. after a previous Exchange load, where `model` was deleted
        // but not recreated).
        m_shapeMapPoints.clear();
        m_shapeMapSegment = HPS::SegmentKey();
        m_shapeMapHighlightSegment = HPS::SegmentKey();
        if (m_shapeMapLegendPanel) {
            delete m_shapeMapLegendPanel;
            m_shapeMapLegendPanel = nullptr;
        }

#ifdef USING_EXCHANGE
        // Drop any MFR / compare result belonging to the previous model: their legend overlays are
        // plain Qt widgets, independent of the 3D scene, so they would otherwise stay on screen, and
        // the compare's stored face shells are about to become stale as the model is torn down and
        // rebuilt below. Must run here, while cad_model is still valid.
        ClearFaceAnalysisOverlays();
#endif

        // Delete our model if we have one already
        if (model.Type() != HPS::Type::None) {
            model.Delete();
        }

        if (cad_model.Type() != HPS::Type::None)
            cad_model.Delete();

        // Create a new model for our view to attach to
        model = HPS::Factory::CreateModel();

        bool success = true;
        if (filename.endsWith(".hsf")) {
            HPS::Stream::ImportNotifier stream_notifier = importHSFFile(filename, progressDlg, success);
            if (success) {
                view.AttachModel(model);

                HPS::CameraKit defaultCamera;
                if (stream_notifier.GetResults().ShowDefaultCamera(defaultCamera))
                    view.GetSegmentKey().SetCamera(defaultCamera);
            }
        }
        else if (filename.endsWith(".stl")) {
            importSTLFile(filename, progressDlg, success);
            if (success) {
                view.AttachModel(model);
                canvas.GetFrontView().FitWorld();
            }
        }
        else if (filename.endsWith(".obj")) {
            importOBJFile(filename, progressDlg, success);
            if (success) {
                view.AttachModel(model);
                canvas.GetFrontView().FitWorld();
            }
        }
        else if (filename.endsWith(".ptx") || filename.endsWith(".pts") || filename.endsWith(".xyz")) {
            ImportPointCloudFile(filename, progressDlg, success);
            if (success) {
                view.AttachModel(model);
                canvas.GetFrontView().FitWorld();
            }
        }
#ifdef USING_EXCHANGE
        else {
            model.Delete();
            cad_model.Delete();
            // (Any MFR / compare legend was already cleared above, while cad_model was still valid.)
            importExchangeFile(filename, success);
        }
#else
        else {
            QMessageBox::critical(this, "Unsupported file extension", "This file format is not handled.");
        }
#endif

        if (success) {
            HPS::DistantLightKit light;
            light.SetDirection(HPS::Vector(1, 0, -1.5f));
            light.SetCameraRelative(true);

            // Delete previous light before inserting new one
            if (mainDistantLight.Type() != HPS::Type::None)
                mainDistantLight.Delete();
            mainDistantLight = canvas.GetFrontView().GetSegmentKey().InsertDistantLight(light);

            canvas.UpdateWithNotifier(HPS::Window::UpdateType::Exhaustive).Wait();
            progressDlg->setValue(100);
            delete progressDlg;

            HPSMainWindow* mw = (HPSMainWindow*)parentWidget();
#ifdef USING_EXCHANGE
            /* A fresh Open resets the similarity part tracking: this file becomes the base part and
                any previously added second part is discarded. */
            m_firstPartPath = filename;
            m_secondPartPath.clear();
#endif
            mw->setCurrentFile(filename);
        }
        else {
            delete progressDlg;
            model.GetSegmentKey().Flush();
        }

        initializeBrowsers();
    }
}

void HPSWidget::onFileSaveAs()
{
    QString selected_filter;
#if defined _MSC_VER && defined(USING_EXCHANGE) && defined(PUBLISH_ENABLED)
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Save As...",
                                                    ".",
                                                    "HOOPS Stream Files (*.hsf);;PDF (*.pdf);;Postscript Files (*.ps);;JPEG "
                                                    "Image Files (*.jpeg);;PNG Image Files (*.png);;3D PDF (*.pdf)",
                                                    &selected_filter);
#else
    QString filename = QFileDialog::getSaveFileName(
        this,
        "Save As...",
        ".",
        "HOOPS Stream Files (*.hsf);;PDF (*.pdf);;Postscript Files (*.ps);;JPEG Image Files (*.jpeg);;PNG Image Files (*.png)",
        &selected_filter);
#endif

    if (filename.size() > 0) {
        if (QString::compare(selected_filter, QString("HOOPS Stream Files (*.hsf)"), Qt::CaseInsensitive) == 0) {
            QProgressDialog* progressDlg;
            progressDlg = new QProgressDialog("Saving File...", "Cancel", 0, 100, parentWidget());
            progressDlg->setWindowModality(Qt::WindowModal);
            progressDlg->show();
            progressDlg->setValue(0);

            HPS::Stream::ExportOptionsKit eok;
            HPS::SegmentKey exportFromHere;

            HPS::Model model = canvas.GetFrontView().GetAttachedModel();
            if (model.Type() == HPS::Type::None)
                exportFromHere = canvas.GetFrontView().GetSegmentKey();
            else
                exportFromHere = model.GetSegmentKey();

            HPS::Stream::ExportNotifier notifier;
            HPS::IOResult status;
            try {
                notifier = HPS::Stream::File::Export(filename.toUtf8(), exportFromHere, eok);
                float percent_complete = 0;
                status = notifier.Status(percent_complete);
                while (status == HPS::IOResult::InProgress) {
                    if (progressDlg->wasCanceled()) {
                        notifier.Cancel();
                        progressDlg->setValue(0);
                        break;
                    }
                    progressDlg->setValue((int)(percent_complete * 100));
                    status = notifier.Status(percent_complete);
                }
            }
            catch (HPS::IOException const& e) {
                QMessageBox msgBox;
                char error_message[1024];
                snprintf(error_message, 1024, "HPS::Stream::File::Export threw an exception: %s", e.what());
                msgBox.setText(error_message);
                msgBox.exec();
            }
        }
        else if (QString::compare(selected_filter, QString("PDF (*.pdf)"), Qt::CaseInsensitive) == 0) {
            try {
                HPS::Hardcopy::File::Export(filename.toUtf8(),
                                            HPS::Hardcopy::File::Driver::PDF,
                                            canvas.GetWindowKey(),
                                            HPS::Hardcopy::File::ExportOptionsKit::GetDefault());
            }
            catch (HPS::IOException const& e) {
                QMessageBox msgBox;
                char error_message[1024];
                snprintf(error_message, 1024, "HPS::Hardcopy::File::Export threw an exception: %s", e.what());
                msgBox.setText(error_message);
                msgBox.exec();
            }
        }
        else if (QString::compare(selected_filter, QString("Postscript Files (*.ps)"), Qt::CaseInsensitive) == 0) {
            try {
                HPS::Hardcopy::File::Export(filename.toUtf8(),
                                            HPS::Hardcopy::File::Driver::Postscript,
                                            canvas.GetWindowKey(),
                                            HPS::Hardcopy::File::ExportOptionsKit::GetDefault());
            }
            catch (HPS::IOException const& e) {
                QMessageBox msgBox;
                char error_message[1024];
                snprintf(error_message, 1024, "HPS::Hardcopy::File::Export threw an exception: %s", e.what());
                msgBox.setText(error_message);
                msgBox.exec();
            }
        }
        else if (QString::compare(selected_filter, QString("JPEG Image Files (*.jpeg)"), Qt::CaseInsensitive) == 0) {
            HPS::Image::ExportOptionsKit eok;
            eok.SetFormat(HPS::Image::Format::Jpeg);

            try {
                HPS::Image::File::Export(filename.toUtf8(), canvas.GetWindowKey(), eok);
            }
            catch (HPS::IOException const& e) {
                QMessageBox msgBox;
                char error_message[1024];
                snprintf(error_message, 1024, "HPS::Image::File::Export threw an exception: %s", e.what());
                msgBox.setText(error_message);
                msgBox.exec();
            }
        }
        else if (QString::compare(selected_filter, QString("PNG Image Files (*.png)"), Qt::CaseInsensitive) == 0) {
            try {
                HPS::Image::File::Export(filename.toUtf8(), canvas.GetWindowKey(), HPS::Image::ExportOptionsKit::GetDefault());
            }
            catch (HPS::IOException const& e) {
                QMessageBox msgBox;
                char error_message[1024];
                snprintf(error_message, 1024, "HPS::Image::File::Export threw an exception: %s", e.what());
                msgBox.setText(error_message);
                msgBox.exec();
            }
        }
#if defined _MSC_VER && defined(USING_EXCHANGE) && defined(PUBLISH_ENABLED)
        else if (QString::compare(selected_filter, QString("3D PDF (*.pdf)"), Qt::CaseInsensitive) == 0) {
            try {
                HPS::SprocketPath sprocket_path(*getCanvas(),
                                                getCanvas()->GetAttachedLayout(),
                                                getCanvas()->GetFrontView(),
                                                getCanvas()->GetFrontView().GetAttachedModel());
                HPS::Publish::ExportOptionsKit export_kit;
                HPS::Publish::File::ExportPDF(sprocket_path.GetKeyPath(), filename.toUtf8(), export_kit);
            }
            catch (HPS::IOException const& e) {
                QMessageBox msgBox;
                char error_message[1024];
                snprintf(error_message, 1024, "HPS::Publish::Export threw an exception: %s", e.what());
                msgBox.setText(error_message);
                msgBox.exec();
            }
        }
#endif
    }
}

void HPSWidget::onOperatorOrbit()
{
    auto ctrl = canvas.GetFrontView().GetOperatorControl();
    ctrl.Pop(HPS::Operator::Priority::High); // drop the Point select operator if it is active
    ctrl.Pop();
    ctrl.Push(new HPS::OrbitOperator(HPS::MouseButtons::ButtonLeft()));
}

void HPSWidget::onOperatorPan()
{
    auto ctrl = canvas.GetFrontView().GetOperatorControl();
    ctrl.Pop(HPS::Operator::Priority::High);
    ctrl.Pop();
    ctrl.Push(new HPS::PanOperator(HPS::MouseButtons::ButtonLeft()));
}

void HPSWidget::onOperatorZoomArea()
{
    auto ctrl = canvas.GetFrontView().GetOperatorControl();
    ctrl.Pop(HPS::Operator::Priority::High);
    ctrl.Pop();
    ctrl.Push(new HPS::ZoomBoxOperator(HPS::MouseButtons::ButtonLeft()));
}

void HPSWidget::onOperatorFly()
{
    auto ctrl = canvas.GetFrontView().GetOperatorControl();
    ctrl.Pop(HPS::Operator::Priority::High);
    ctrl.Pop();
    ctrl.Push(new HPS::FlyOperator());
}

void HPSWidget::onOperatorZoomFit() { canvas.GetFrontView().FitWorld().Update(); }

void HPSWidget::onOperatorPoint()
{
    // The select operator sits at High priority ON TOP of a left-button Orbit at Default priority.
    // SandboxHighlightOperator::OnMouseDown returns false when the click did not hit anything, so
    // an empty-space left-drag falls through to the Orbit operator below and rotates the model,
    // while a click on a marker/part is consumed and highlighted. Force Orbit as the Default left
    // operator so the fallback is always rotation regardless of the previously active operator.
    auto ctrl = canvas.GetFrontView().GetOperatorControl();
    ctrl.Pop(HPS::Operator::Priority::High); // avoid stacking multiple select operators
    ctrl.Pop();
    ctrl.Push(new HPS::OrbitOperator(HPS::MouseButtons::ButtonLeft()));
    ctrl.Push(new SandboxHighlightOperator(this), HPS::Operator::Priority::High);
}

void HPSWidget::onOperatorArea()
{
    canvas.GetFrontView().GetOperatorControl().Pop();
    canvas.GetFrontView().GetOperatorControl().Push(new HPS::HighlightAreaOperator(HPS::MouseButtons::ButtonLeft()));
}

void HPSWidget::onModeSimpleShadow()
{
    // Toggle state and bail early if we're disabling
    enableSimpleShadows = !enableSimpleShadows;
    if (enableSimpleShadows == false) {
        canvas.GetFrontView().GetSegmentKey().GetVisualEffectsControl().SetSimpleShadow(false);
        canvas.Update();
        return;
    }

    updatePlanes();
}

void HPSWidget::onModeEyeDome()
{
    // Toggle state and bail early if we're disabling
    eyeDome = !eyeDome;

    HPS::WindowKey window = canvas.GetWindowKey();
    window.GetPostProcessEffectsControl().SetEyeDomeLighting(eyeDome);

    auto visual_effects_control = window.GetVisualEffectsControl();
    visual_effects_control.SetEyeDomeLightingEnabled(eyeDome);

    canvas.Update();
}

void HPSWidget::onModeFrameRate()
{
    float const frameRate = 20.0f;

    // Toggle frame rate and set.  Note that 0 disables frame rate.
    enableFrameRate = !enableFrameRate;
    if (enableFrameRate) {
        canvas.SetFrameRate(frameRate);
        if (!smoothRendering) {
            smoothRendering = true;
            HPSMainWindow* mw = (HPSMainWindow*)parentWidget();
            mw->toolbarSmooth->setChecked(true);
            canvas.GetFrontView().SetRenderingMode(HPS::Rendering::Mode::Phong);
        }
    }
    else
        canvas.SetFrameRate(0);

    canvas.Update();
}

void HPSWidget::onModeSmooth()
{
    if (!smoothRendering) {
        canvas.GetFrontView().SetRenderingMode(HPS::Rendering::Mode::Phong);
        canvas.Update();
        smoothRendering = true;
    }
}

void HPSWidget::onModeHiddenLine()
{
    if (smoothRendering) {
        canvas.GetFrontView().SetRenderingMode(HPS::Rendering::Mode::FastHiddenLine);
        canvas.SetFrameRate(0);
        canvas.Update();
        smoothRendering = false;
    }
}

void HPSWidget::updatePlanes()
{
    if (enableSimpleShadows) {
        canvas.GetFrontView().SetSimpleShadow(true);

        float const opacity = 0.3f;
        unsigned int const resolution = 512;
        unsigned int const blurring = 20;

        HPS::SegmentKey viewSegment = canvas.GetFrontView().GetSegmentKey();

        // Set opacity in simple shadow color
        HPS::RGBAColor color(0.4f, 0.4f, 0.4f, opacity);
        if (viewSegment.GetVisualEffectsControl().ShowSimpleShadowColor(color))
            color.alpha = opacity;

        viewSegment.GetVisualEffectsControl()
            .SetSimpleShadow(enableSimpleShadows, resolution, blurring)
            .SetSimpleShadowColor(color);
        canvas.Update();
    }
}

void HPSWidget::initializeBrowsers()
{
    ModelBrowserWidget* modelBrowser = getModelBrowser();
    QMetaObject::invokeMethod(modelBrowser, "Init", Qt::AutoConnection, Q_ARG(CADModel, cad_model), Q_ARG(Canvas, canvas));

    ConfigurationWidget* configurationBrowser = getConfigurationBrowser();
    QMetaObject::invokeMethod(configurationBrowser, "Init", Qt::AutoConnection, Q_ARG(CADModel, cad_model));

    SegmentBrowserTree* segmentBrowser = getSegmentBrowser();
    QMetaObject::invokeMethod(segmentBrowser, "Init", Qt::AutoConnection, Q_ARG(Canvas, canvas));
}

void HPSWidget::focusOutEvent(QFocusEvent*) { canvas.GetWindowKey().GetEventDispatcher().InjectEvent(HPS::FocusLostEvent()); }

ModelBrowserWidget* HPSWidget::getModelBrowser()
{
    HPSMainWindow* mw = (HPSMainWindow*)parentWidget();
    return mw->getModelBrowser();
}

ConfigurationWidget* HPSWidget::getConfigurationBrowser()
{
    HPSMainWindow* mw = (HPSMainWindow*)parentWidget();
    return mw->getConfigurationBrowser();
}

SegmentBrowserTree* HPSWidget::getSegmentBrowser()
{
    HPSMainWindow* mw = (HPSMainWindow*)parentWidget();
    return mw->getSegmentBrowser();
}

void HPSWidget::ZoomToKeyPath(KeyPath const& keyPath)
{
    BoundingKit bounding;
    if (keyPath.ShowNetBounding(true, bounding)) {
        zoomToKeyPath = keyPath;

        View frontView = canvas.GetFrontView();
        frontView.GetSegmentKey().ShowCamera(preZoomToKeyPathCamera);

        CameraKit fittedCamera;
        frontView.ComputeFitWorldCamera(bounding, fittedCamera);
        frontView.SmoothTransition(fittedCamera);
    }
}

void HPSWidget::RestoreCamera()
{
    if (!preZoomToKeyPathCamera.Empty()) {
        canvas.GetFrontView().SmoothTransition(preZoomToKeyPathCamera);
        Database::Sleep(500);

        InvalidateZoomKeyPath();
        InvalidateSavedCamera();
    }
}

void HPSWidget::InvalidateZoomKeyPath() { zoomToKeyPath.Reset(); }

void HPSWidget::InvalidateSavedCamera() { preZoomToKeyPathCamera.Reset(); }

void HPSWidget::onUserCode1()
{
    // Toggle display of resource monitor using the DebuggingControl
    displayResourceMonitor = !displayResourceMonitor;
    canvas.GetWindowKey().GetDebuggingControl().SetResourceMonitor(displayResourceMonitor);

    canvas.Update();
}

void HPSWidget::onMfrInference()
{
#ifdef USING_EXCHANGE
    // MFR (Manufacturing Feature Recognition): classify every face of the currently loaded model
    // and color it according to the recognized feature, then show a legend of what was found.
    if (cad_model.Type() == HPS::Type::None) {
        QMessageBox::warning(this, "MFR", "Please open a CAD file first.");
        return;
    }

    // 1. Ensure an MFR model is loaded. This may prompt the user with a checkpoint-selection dialog,
    // so it runs before the wait cursor is set. Once loaded, the model is reused for later runs.
    QString errorMessage;
    if (!EnsureMfrReady(errorMessage)) {
        ShowLoadError(this, "MFR", errorMessage);
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // 2. Export the imported model to a temporary PRC file - this is what gets handed to HOOPS AI.
    QDir tempDir(QDir::tempPath() + "/hoops_ai_mfr");
    if (!tempDir.exists())
        tempDir.mkpath(".");
    QString const prcPath = tempDir.filePath("model.prc");

    bool exportOk = false;
    try {
        HPS::Exchange::ExportNotifier exportNotifier = HPS::Exchange::File::ExportPRC(
            cad_model, prcPath.toUtf8().constData(), HPS::Exchange::ExportPRCOptionsKit::GetDefault());
        exportNotifier.Wait();
        exportOk = (exportNotifier.Status() == HPS::IOResult::Success);
    }
    catch (HPS::IOException const&) {
        exportOk = false;
    }

    if (!exportOk) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "MFR", QString("Failed to export the model to PRC: %1").arg(prcPath));
        return;
    }

    // 3. Run inference on the exported PRC file.
    std::vector<int> labels(100000, 0);
    int labelCount = 0;
    char errBuf[8192] = {0};
    QElapsedTimer mfrTimer;
    mfrTimer.start();
    bool const inferOk = HoopsAI_RunMFRInference(prcPath.toUtf8().constData(), labels.data(),
                                                  static_cast<int>(labels.size()), &labelCount, errBuf, sizeof(errBuf));
    qint64 const mfrElapsedMs = mfrTimer.elapsed();

    QApplication::restoreOverrideCursor();

    if (!inferOk) {
        QMessageBox::critical(this, "MFR", QString("MFR inference failed: %1").arg(errBuf));
        return;
    }

    emit statusMessage(tr("MFR inference: %1 ms (bridge, incl. overhead)").arg(mfrElapsedMs), 0);

    // 4. Retire whatever face-analysis result is still on screen from a previous command - an
    // earlier MFR run, or a Similarity Comparison whose legend occupies this same corner and whose
    // per-face colors/hidden groups would otherwise survive underneath the new MFR colors. Done
    // here, at the point the new result is about to be applied, so a cancelled or failed inference
    // above leaves the previous result untouched.
    ClearFaceAnalysisOverlays();

    // 5. Color every face according to its label and update the legend.
    bool indexMismatch = false;
    QMap<int, int> const labelCounts = ColorFacesByMfrLabels(labels, labelCount, indexMismatch);
    canvas.UpdateWithNotifier(HPS::Window::UpdateType::Exhaustive).Wait();
    UpdateMfrLegend(labelCounts);

    if (indexMismatch) {
        QMessageBox::warning(this, "MFR",
                              QString("The number of faces found in the model does not match the number of MFR labels "
                                      "(labelCount=%1). Face coloring may be misaligned.")
                                  .arg(labelCount));
    }
#endif
}

void HPSWidget::onSimilarityComparison()
{
#ifdef USING_EXCHANGE
    // Similarity Comparison: compare the two currently loaded parts (the base model opened with
    // File > Open and the model brought in with File > Add). Two things are produced:
    //   1) a geometric visual diff, rendered in the 3D view with the faces color-coded by status
    //      (HOOPS Exchange's A3DCompareFacesInBrepModels), and
    //   2) an overall shape-similarity score (HOOPS AI embeddings). This needs an embeddings model,
    //      so if none is loaded yet the user is prompted for a checkpoint up front - before any
    //      processing - and cancelling that prompt cancels the whole command.
    if (m_firstPartPath.isEmpty() || m_secondPartPath.isEmpty()) {
        QMessageBox::warning(this, "Similarity Comparison",
                              "Two parts are required. Open a model with File > Open, then bring in a second "
                              "one with File > Add before running the comparison.");
        return;
    }

    // The two source paths, used both for the visual diff and the optional cosine score below. The
    // diff now recolors the two parts in place (it no longer replaces the model), so these members
    // stay valid, but we still take local copies to keep the two uses in sync.
    QString const firstPath = m_firstPartPath;
    QString const secondPath = m_secondPartPath;

    // Gather every input needed for BOTH results before doing any work. The overall similarity
    // score needs a HOOPS AI embeddings model; if none is loaded yet, prompt for a checkpoint now,
    // up front. Cancelling that prompt cancels the whole command - nothing is computed or shown -
    // rather than running the compare and then silently skipping the score.
    QString embErr;
    if (!EnsureEmbeddingsReady(embErr))
        return;

    // All inputs are in hand: now run everything under a single busy cursor, then show the result.
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // 1) Overall shape-similarity score (HOOPS AI embeddings; the model is guaranteed loaded above).
    // Computed first, before the visual diff, so that once the geometry is recolored the legend can
    // appear immediately - otherwise the model would change color and then sit there while the
    // score is still being computed.
    QString similarityLine;
    {
        float similarity = 0.0f;
        char errBuf[8192] = {0};
        QElapsedTimer cmpTimer;
        cmpTimer.start();
        bool const scored = HoopsAI_CompareEmbeddings(firstPath.toUtf8().constData(),
                                                      secondPath.toUtf8().constData(),
                                                      &similarity, errBuf, sizeof(errBuf));
        qint64 const cmpElapsedMs = cmpTimer.elapsed();
        if (scored) {
            similarityLine = QString("\nSimilarity (cosine): %1").arg(similarity, 0, 'f', 4);
            emit statusMessage(tr("Similarity compare: %1 ms (bridge, incl. overhead)").arg(cmpElapsedMs), 0);
        } else {
            similarityLine = QString("\nSimilarity: unavailable\n(%1)").arg(QString::fromUtf8(errBuf).trimmed());
        }
    }

    // Retire whatever face-analysis result is still on screen from a previous command - an MFR run
    // (its legend occupies this same corner, and its per-face colors would survive on any face the
    // diff does not repaint) or an earlier comparison. Done here, once every input is in hand, so a
    // cancelled checkpoint prompt above leaves the previous result untouched.
    ClearFaceAnalysisOverlays();

    // 2) Geometric visual diff. Tolerance is the maximum distance (in the model's unit, typically
    // mm) under which a face of one model still matches a face of the other. 0.01 detects real
    // geometry changes while tolerating tessellation/registration noise.
    double const compareTolerance = 0.01;
    int unchangedFaces = 0, removedFaces = 0, addedFaces = 0;
    QString compareError;
    bool const compareOk = RunModelCompareAndDisplay(firstPath, secondPath, compareTolerance,
                                                      unchangedFaces, removedFaces, addedFaces, compareError);

    QApplication::restoreOverrideCursor();

    if (!compareOk) {
        QMessageBox::critical(this, "Similarity Comparison",
                              QString("Model compare failed:\n%1").arg(compareError));
        return;
    }

    // Move the summary that used to live in a modal dialog into the legend's info block, so the
    // file names and the overall similarity stay on screen next to the color key.
    QString const infoText = QString("Old: %1\nNew: %2%3")
                                 .arg(QFileInfo(firstPath).fileName())
                                 .arg(QFileInfo(secondPath).fileName())
                                 .arg(similarityLine);

    UpdateCompareLegend(unchangedFaces, removedFaces, addedFaces, infoText);

    // Also surface the result in a modal dialog (as it used to be shown), since the similarity
    // score under the legend is easy to miss. The legend stays on screen next to the color key.
    QString const dialogText = QString("Old: %1\nNew: %2%3\n\nUnchanged faces: %4\nRemoved (old only): %5\nAdded (new only): %6")
                                   .arg(QFileInfo(firstPath).fileName())
                                   .arg(QFileInfo(secondPath).fileName())
                                   .arg(similarityLine)
                                   .arg(unchangedFaces)
                                   .arg(removedFaces)
                                   .arg(addedFaces);
    QMessageBox::information(this, "Similarity Comparison", dialogText);
#endif
}

#ifdef USING_EXCHANGE
bool HPSWidget::RunModelCompareAndDisplay(QString const& firstPath, QString const& secondPath, double tolerance,
                                          int& outUnchanged, int& outRemoved, int& outAdded, QString& outMessage)
{
    outUnchanged = outRemoved = outAdded = 0;

    // Load each part as its own A3DAsmModelFile with B-rep geometry (the compare works on B-rep, so
    // no tessellation is requested here; the result set carries its own facets for display).
    A3DRWParamsLoadData sLoad;
    A3D_INITIALIZE_DATA(A3DRWParamsLoadData, sLoad);
    sLoad.m_sGeneral.m_bReadSolids = A3D_TRUE;
    sLoad.m_sGeneral.m_bReadSurfaces = A3D_TRUE;
    sLoad.m_sGeneral.m_bReadWireframes = A3D_FALSE;
    sLoad.m_sGeneral.m_bReadPmis = A3D_FALSE;
    sLoad.m_sGeneral.m_eReadingMode2D3D = kA3DRead_3D;
    sLoad.m_sGeneral.m_eReadGeomTessMode = kA3DReadGeomOnly;

    A3DAsmModelFile* firstModel = nullptr;
    A3DAsmModelFile* secondModel = nullptr;
    A3DStatus status = A3DAsmModelFileLoadFromFile(firstPath.toUtf8().constData(), &sLoad, &firstModel);
    if (status != A3D_SUCCESS || firstModel == nullptr) {
        outMessage = QString("Could not load the first model (status %1):\n%2").arg(status).arg(firstPath);
        return false;
    }
    status = A3DAsmModelFileLoadFromFile(secondPath.toUtf8().constData(), &sLoad, &secondModel);
    if (status != A3D_SUCCESS || secondModel == nullptr) {
        A3DAsmModelFileDelete(firstModel);
        outMessage = QString("Could not load the second model (status %1):\n%2").arg(status).arg(secondPath);
        return false;
    }

    A3DCompareOutputData compareOut;
    A3D_INITIALIZE_DATA(A3DCompareOutputData, compareOut);
    status = A3DCompareFacesInBrepModels(firstModel, secondModel, tolerance, &compareOut);
    if (status != A3D_SUCCESS) {
        A3DAsmModelFileDelete(firstModel);
        A3DAsmModelFileDelete(secondModel);
        outMessage = QString("A3DCompareFacesInBrepModels failed (status %1).").arg(status);
        return false;
    }

    // Per-face status is a boolean (a face either fully matches a face of the other model or not):
    //   old faces that don't match  -> removed (present only in the old model)
    //   new faces that don't match  -> added   (present only in the new model)
    //   matched faces               -> unchanged
    // Geometry modifications surface as red (old side) + green (new side) pairs, and the result set
    // additionally sub-tessellates the changed sub-regions for a finer visual.
    for (A3DUns32 i = 0; i < compareOut.m_uiOldFaceSize; ++i) {
        if (compareOut.m_pOldFaceMatch != nullptr && compareOut.m_pOldFaceMatch[i])
            ++outUnchanged;
        else
            ++outRemoved;
    }
    for (A3DUns32 i = 0; i < compareOut.m_uiNewFaceSize; ++i) {
        if (compareOut.m_pNewFaceMatch == nullptr || !compareOut.m_pNewFaceMatch[i])
            ++outAdded;
    }

    // Color the two parts already shown in the 3D view (the base part from File > Open and the
    // added part from File > Add) directly, keyed on the per-face match status. Coloring the live
    // scene - rather than importing Exchange's own pre-colored result model, whose colors we cannot
    // control - lets us paint the exact colors listed in the legend, so the two always agree.
    bool const displayed = ColorLoadedPartsByCompare(compareOut.m_pOldFaceMatch, compareOut.m_uiOldFaceSize,
                                                     compareOut.m_pNewFaceMatch, compareOut.m_uiNewFaceSize,
                                                     outMessage);

    // Free the compare output (documented convention: call again with null model files), then the
    // two input model files. Done after the display import has fully built its own scene graph.
    A3DCompareFacesInBrepModels(nullptr, nullptr, tolerance, &compareOut);
    A3DAsmModelFileDelete(firstModel);
    A3DAsmModelFileDelete(secondModel);

    // The caller builds the legend (it also has the file names and optional similarity score to
    // show in the legend's info block).
    return displayed;
}

bool HPSWidget::ColorLoadedPartsByCompare(void* oldFaceMatch, unsigned int oldFaceSize,
                                          void* newFaceMatch, unsigned int newFaceSize, QString& outMessage)
{
    if (cad_model.Type() == HPS::Type::None) {
        outMessage = "No model is loaded to color.";
        return false;
    }

    A3DBool const* const oldMatch = static_cast<A3DBool const*>(oldFaceMatch);
    A3DBool const* const newMatch = static_cast<A3DBool const*>(newFaceMatch);

    // The two parts live in one CADModel (File > Add merges the second part into the model opened
    // with File > Open), so the ExchangeTopoFace components come back in load order as
    // [old-part faces ..., new-part faces ...] - the same order Exchange enumerates the faces in
    // for the compare's m_pOldFace / m_pNewFace arrays.
    ComponentArray const faces = cad_model.GetAllSubcomponents(Component::ComponentType::ExchangeTopoFace);
    size_t const expected = static_cast<size_t>(oldFaceSize) + static_cast<size_t>(newFaceSize);
    if (faces.size() != expected) {
        outMessage = QString("Face-count mismatch: the view shows %1 faces but the compare reported %2 "
                             "(old %3 + new %4). The two loaded parts may not be the files that were compared.")
                         .arg(faces.size())
                         .arg(expected)
                         .arg(oldFaceSize)
                         .arg(newFaceSize);
        return false;
    }

    // Each ExchangeTopoFace is tessellated into its own Visualize shell whose polygon faces all
    // belong to that one CAD face, so the whole shell is painted with the face's status color and
    // recorded under its status group so the legend checkboxes can later toggle its visibility.
    for (int g = 0; g < 3; ++g)
        m_compareGroupShells[g].clear();

    auto colorFace = [this](Component const& faceComponent, HPS::RGBColor const& color, int group) {
        for (Key const& key : faceComponent.GetKeys()) {
            ShellKey shellKey(key);
            if (shellKey.Type() == HPS::Type::ShellKey) {
                shellKey.SetFaceRGBColorsByRange(0, shellKey.GetFaceCount(), color);
                m_compareGroupShells[group].push_back(shellKey);
            }
        }
    };

    // Group indices match the legend: 0 = Unchanged, 1 = Added, 2 = Removed.
    // Old part: matched faces are unchanged (gray); unmatched faces exist only in the old part and
    // are therefore removed (red).
    for (unsigned int i = 0; i < oldFaceSize; ++i) {
        bool const matched = (oldMatch != nullptr && oldMatch[i]);
        colorFace(faces[i], matched ? kCompareUnchangedColor : kCompareRemovedColor, matched ? 0 : 2);
    }
    // New part: matched faces are unchanged (gray); unmatched faces exist only in the new part and
    // are therefore added (green).
    for (unsigned int j = 0; j < newFaceSize; ++j) {
        bool const matched = (newMatch == nullptr || newMatch[j]);
        colorFace(faces[static_cast<size_t>(oldFaceSize) + j], matched ? kCompareUnchangedColor : kCompareAddedColor,
                  matched ? 0 : 1);
    }

    canvas.UpdateWithNotifier(HPS::Window::UpdateType::Exhaustive).Wait();
    return true;
}

void HPSWidget::UpdateCompareLegend(int unchanged, int removed, int added, QString const& infoText)
{
    delete compareLegendPanel;
    compareLegendPanel = nullptr;

    // The swatch colors are read from the same constants ColorLoadedPartsByCompare paints the 3D
    // faces with, so the legend can never disagree with the model. Only the three states that are
    // detectable per-face are listed; a geometry modification shows up as a removed(old, red) plus
    // an added(new, green) face rather than a distinct color.
    auto to255 = [](float c) { return static_cast<int>(c * 255.0f + 0.5f); };
    struct Row {
        QString label;
        HPS::RGBColor color;
        int count;
        int group;  // matches m_compareGroupShells / SetCompareGroupVisible: 0/1/2
    };
    QVector<Row> const rows = {
        {QString("Unchanged"), kCompareUnchangedColor, unchanged, 0},
        {QString("Added (new only)"), kCompareAddedColor, added, 1},
        {QString("Removed (old only)"), kCompareRemovedColor, removed, 2},
    };

    QWidget* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("compareLegendPanel"));
    // See UpdateMfrLegend for why the background must be opaque (no compositor on this Linux
    // setup) and why the stylesheet is scoped to the panel's object name rather than the QWidget
    // type (otherwise it cascades to the row/title children, drawing a box around each row and
    // squeezing its text against that border).
    panel->setStyleSheet(QStringLiteral("#compareLegendPanel { background-color: rgb(255, 255, 255); border: 1px solid gray; }"));

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    QWidget* titleRow = new QWidget(panel);
    QHBoxLayout* titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    QLabel* title = new QLabel("Compare Result", titleRow);
    title->setStyleSheet("font-weight: bold; border: none;");
    titleLayout->addWidget(title);
    titleLayout->addStretch();

    QPushButton* closeButton = new QPushButton("X", titleRow);
    closeButton->setFixedSize(16, 16);
    closeButton->setToolTip("Clear the compare colors");
    closeButton->setStyleSheet("QPushButton { border: 1px solid gray; font-weight: bold; padding: 0; }");
    connect(closeButton, &QPushButton::clicked, this, &HPSWidget::ClearCompareLegend);
    titleLayout->addWidget(closeButton);

    layout->addWidget(titleRow);

    for (Row const& row : rows) {
        QWidget* rowWidget = new QWidget(panel);
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        // Per-group show/hide toggle. Checked = the group's faces are visible in the 3D view.
        QCheckBox* toggle = new QCheckBox(rowWidget);
        toggle->setChecked(true);
        toggle->setToolTip(QString("Show/hide %1 faces").arg(row.label));
        toggle->setStyleSheet("border: none;");
        int const group = row.group;
        connect(toggle, &QCheckBox::toggled, this,
                [this, group](bool checked) { SetCompareGroupVisible(group, checked); });
        rowLayout->addWidget(toggle);

        QFrame* swatch = new QFrame(rowWidget);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid black;")
                                  .arg(to255(row.color.red))
                                  .arg(to255(row.color.green))
                                  .arg(to255(row.color.blue)));
        rowLayout->addWidget(swatch);

        QString const text = QString("%1 (%2)").arg(row.label).arg(row.count);
        QLabel* labelWidget = new QLabel(text, rowWidget);
        labelWidget->setStyleSheet("border: none;");
        rowLayout->addWidget(labelWidget);
        rowLayout->addStretch();

        layout->addWidget(rowWidget);
    }

    // Info block (the summary that used to be shown in a modal dialog): compared file names and,
    // when an embeddings model is loaded, the overall cosine similarity.
    if (!infoText.isEmpty()) {
        QFrame* separator = new QFrame(panel);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet("color: gray;");
        layout->addWidget(separator);

        QLabel* info = new QLabel(infoText, panel);
        info->setStyleSheet("border: none;");
        info->setWordWrap(true);
        layout->addWidget(info);
    }

    panel->adjustSize();
    compareLegendPanel = panel;
    RepositionCompareLegend();
    compareLegendPanel->show();
    compareLegendPanel->raise();
}

void HPSWidget::SetCompareGroupVisible(int group, bool visible)
{
    if (group < 0 || group > 2)
        return;

    // Each CAD face is its own shell; toggling the visibility of all of that shell's polygon faces
    // shows or hides the face fill for the whole group. (Edges are separate geometry and remain as
    // a light wireframe for context.)
    for (HPS::ShellKey& shellKey : m_compareGroupShells[group]) {
        if (shellKey.Type() == HPS::Type::ShellKey)
            shellKey.SetFaceVisibilitiesByRange(0, shellKey.GetFaceCount(), visible);
    }
    canvas.Update();
}

void HPSWidget::RepositionCompareLegend()
{
    if (compareLegendPanel == nullptr)
        return;

    int const margin = 10;
    compareLegendPanel->move(width() - compareLegendPanel->width() - margin, margin);
    compareLegendPanel->raise();
}

void HPSWidget::ClearCompareLegend()
{
    // Remove the diff coloring from the two parts so the view returns to its normal appearance, and
    // restore the visibility of any groups that were hidden via the legend checkboxes.
    if (cad_model.Type() != HPS::Type::None) {
        ComponentArray const faces = cad_model.GetAllSubcomponents(Component::ComponentType::ExchangeTopoFace);
        for (Component const& faceComponent : faces) {
            for (Key const& key : faceComponent.GetKeys()) {
                ShellKey shellKey(key);
                if (shellKey.Type() == HPS::Type::ShellKey) {
                    shellKey.UnsetFaceColors();
                    shellKey.SetFaceVisibilitiesByRange(0, shellKey.GetFaceCount(), true);
                }
            }
        }
        canvas.Update();
    }

    for (int g = 0; g < 3; ++g)
        m_compareGroupShells[g].clear();

    delete compareLegendPanel;
    compareLegendPanel = nullptr;
}

void HPSWidget::ClearFaceAnalysisOverlays()
{
    // Only one face-analysis result can be shown at a time (see the header for why), so this simply
    // retires both: each Clear* call is a no-op when that result is not currently displayed.
    ClearMfrColors();      // MFR legend + the per-face MFR colors
    ClearCompareLegend();  // compare legend + the diff colors + the per-group face visibilities
}
#endif
