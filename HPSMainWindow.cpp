#include "HPSMainWindow.h"
#include "HPSModelBrowser.h"
#include "HPSSegmentBrowser.h"
#include "HPSPropertyBrowser.h"
#include "SimilarityIndexPanel.h"
#include <QMessageBox>
#include <QStatusBar>

HPSMainWindow::HPSMainWindow(QWidget* parent):
    QMainWindow(parent), modelBrowserAndConfigurations(nullptr), segmentBrowser(nullptr), propertyBrowser(nullptr)
{
    widget = new HPSWidget();
    setCentralWidget(widget);

    setupGui();
    setWindowTitle("HPS Qt Sandbox");
}

HPSMainWindow::~HPSMainWindow() {}

void HPSMainWindow::setupGui()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* fileMenuNew = fileMenu->addAction("New");
    QAction* fileMenuOpen = fileMenu->addAction(QIcon(":/HPSMainWindow/openIcon"), "Open");
    fileMenuAdd = fileMenu->addAction(QIcon(":/HPSMainWindow/openIcon"), "Add");
    fileMenuAdd->setEnabled(false);
    QAction* fileMenuSaveAs = fileMenu->addAction("Save As");
    fileMenu->addSeparator();
    QAction* fileMenuLoadMfr = fileMenu->addAction("Load MFR Model…");
    QAction* fileMenuLoadSimilarity = fileMenu->addAction("Load Similarity Search Model…");
    fileMenu->addSeparator();
    for (int i = 0; i < maxRecentFiles; ++i) {
        recentFiles[i] = new QAction(this);
        fileMenu->addAction(recentFiles[i]);
        recentFiles[i]->setVisible(false);
        connect(recentFiles[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
    }

    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();

    settings.setValue("recentFileList", files);

    int numRecentFiles = qMin(files.size(), (int)maxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 %2").arg(i + 1).arg(strippedName(files[i]));
        recentFiles[i]->setText(text);
        recentFiles[i]->setData(files[i]);
        recentFiles[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < maxRecentFiles; ++j)
        recentFiles[j]->setVisible(false);

    fileMenu->addSeparator();
    QAction* fileMenuExit = fileMenu->addAction("Exit");

    connect(fileMenuNew, SIGNAL(triggered()), widget, SLOT(onFileNew()));
    connect(fileMenuOpen, SIGNAL(triggered()), widget, SLOT(onFileOpen()));
    connect(fileMenuAdd, SIGNAL(triggered()), widget, SLOT(onFileAdd()));
    connect(fileMenuSaveAs, SIGNAL(triggered()), widget, SLOT(onFileSaveAs()));
    connect(fileMenuLoadMfr, SIGNAL(triggered()), this, SLOT(onLoadMfrModel()));
    connect(fileMenuLoadSimilarity, SIGNAL(triggered()), this, SLOT(onLoadSimilarityModel()));
    connect(fileMenuExit, SIGNAL(triggered()), this, SLOT(quit()));

    QMenu* operatorsMenu = menuBar()->addMenu(tr("&Operators"));
    QAction* operatorsMenuOrbit = operatorsMenu->addAction(QIcon(":/HPSMainWindow/orbitIcon"), "Orbit");
    QAction* operatorsMenuPan = operatorsMenu->addAction(QIcon(":/HPSMainWindow/panIcon"), "Pan");
    QAction* operatorsMenuZoomArea = operatorsMenu->addAction(QIcon(":/HPSMainWindow/zoomAreaIcon"), "Zoom Area");
    QAction* operatorsMenuFly = operatorsMenu->addAction(QIcon(":/HPSMainWindow/flyIcon"), "Fly");
    operatorsMenu->addSeparator();
    QAction* operatorsMenuZoomFit = operatorsMenu->addAction(QIcon(":/HPSMainWindow/zoomFitIcon"), "Zoom Fit");
    operatorsMenu->addSeparator();
    QAction* operatorsMenuPoint = operatorsMenu->addAction(QIcon(":/HPSMainWindow/pointIcon"), "Point");
    QAction* operatorsMenuArea = operatorsMenu->addAction(QIcon(":/HPSMainWindow/areaIcon"), "Area");

    connect(operatorsMenuOrbit, SIGNAL(triggered()), widget, SLOT(onOperatorOrbit()));
    connect(operatorsMenuPan, SIGNAL(triggered()), widget, SLOT(onOperatorPan()));
    connect(operatorsMenuZoomArea, SIGNAL(triggered()), widget, SLOT(onOperatorZoomArea()));
    connect(operatorsMenuFly, SIGNAL(triggered()), widget, SLOT(onOperatorFly()));
    connect(operatorsMenuZoomFit, SIGNAL(triggered()), widget, SLOT(onOperatorZoomFit()));
    connect(operatorsMenuPoint, SIGNAL(triggered()), widget, SLOT(onOperatorPoint()));
    connect(operatorsMenuArea, SIGNAL(triggered()), widget, SLOT(onOperatorArea()));

    QMenu* modesMenu = menuBar()->addMenu(tr("&Modes"));
    QAction* modesMenuSimpleShadow = modesMenu->addAction(QIcon(":/HPSMainWindow/genericIcon"), "Simple Shadow");
    QAction* modesMenuFrameRate = modesMenu->addAction(QIcon(":/HPSMainWindow/genericIcon"), "Frame Rate");
    QAction* modesMenuSmooth = modesMenu->addAction(QIcon(":/HPSMainWindow/smoothIcon"), "Smooth");
    QAction* modesMenuHiddenLine = modesMenu->addAction(QIcon(":/HPSMainWindow/hiddenLineIcon"), "Hidden Line");
    QAction* modesMenuEyeDome = modesMenu->addAction(QIcon(":/HPSMainWindow/genericIcon"), "Eye Dome Lighting");

    connect(modesMenuSimpleShadow, SIGNAL(triggered()), widget, SLOT(onModeSimpleShadow()));
    connect(modesMenuFrameRate, SIGNAL(triggered()), widget, SLOT(onModeFrameRate()));
    connect(modesMenuSmooth, SIGNAL(triggered()), widget, SLOT(onModeSmooth()));
    connect(modesMenuHiddenLine, SIGNAL(triggered()), widget, SLOT(onModeHiddenLine()));
    connect(modesMenuEyeDome, SIGNAL(triggered()), widget, SLOT(onModeEyeDome()));

    QDockWidget* modelBrowserDock = new QDockWidget("Model Browser", this);
    modelBrowserDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    QDockWidget* segmentBrowserDock = new QDockWidget("Segment Browser", this);
    segmentBrowserDock->setAllowedAreas(Qt::RightDockWidgetArea);
    QDockWidget* propertyBrowserDock = new QDockWidget("Properties", this);
    propertyBrowserDock->setAllowedAreas(Qt::RightDockWidgetArea);

    QMenu* browsersMenu = menuBar()->addMenu(tr("&Browsers"));
    browsersMenu->addAction(modelBrowserDock->toggleViewAction());
    browsersMenu->addAction(segmentBrowserDock->toggleViewAction());

    QDockWidget* similarityDock = new QDockWidget("Similarity Search", this);
    similarityDock->setAllowedAreas(Qt::RightDockWidgetArea);

    modelBrowserAndConfigurations = new TabbedView();
    modelBrowserDock->setWidget(reinterpret_cast<QWidget*>(modelBrowserAndConfigurations));
    addDockWidget(Qt::LeftDockWidgetArea, modelBrowserDock);
    modelBrowserDock->hide();

    segmentBrowser = new SegmentBrowserWidget(this);
    segmentBrowserDock->setWidget(reinterpret_cast<QWidget*>(segmentBrowser));
    addDockWidget(Qt::RightDockWidgetArea, segmentBrowserDock);
    segmentBrowserDock->hide();

    propertyBrowser = new PropertyWidget(this);
    propertyBrowserDock->setWidget(reinterpret_cast<QWidget*>(propertyBrowser));
    addDockWidget(Qt::RightDockWidgetArea, propertyBrowserDock);
    propertyBrowserDock->hide();

    similarityPanel = new SimilarityIndexPanel(this);
    similarityDock->setWidget(similarityPanel);
    addDockWidget(Qt::RightDockWidgetArea, similarityDock);
    similarityDock->hide();
    // Double-clicking a search hit loads that CAD file into the 3D view.
    connect(similarityPanel, SIGNAL(loadCadRequested(QString)), widget, SLOT(onFileOpen(QString)));
    // The Shape Map command renders the index clusters in the 3D view; selecting a part in the
    // panel highlights its point on the map.
    connect(similarityPanel, &SimilarityIndexPanel::shapeMapReady,
            widget, &HPSWidget::ShowShapeMap);
    connect(similarityPanel, &SimilarityIndexPanel::partSelected,
            widget, &HPSWidget::HighlightShapeMapPoint);
    // Reverse direction: picking a marker in the 3D map (Point select operator) selects the
    // matching part in the panel gallery.
    connect(widget, &HPSWidget::shapeMapPartPicked,
            similarityPanel, &SimilarityIndexPanel::selectPartInList);
    // Show MFR / Similarity Comparison bridge-API timing in the main-window status bar.
    connect(widget, &HPSWidget::statusMessage,
            this, [this](const QString& text, int timeoutMs) {
                statusBar()->showMessage(text, timeoutMs);
            });
    // Show part/assembly search timing (bridge round-trip) in the main-window status bar.
    connect(similarityPanel, &SimilarityIndexPanel::statusMessage,
            this, [this](const QString& text, int timeoutMs) {
                statusBar()->showMessage(text, timeoutMs);
            });

    QMenu* userCodeMenu = menuBar()->addMenu(tr("&User Code"));
    QAction* userCodeMenu1 = userCodeMenu->addAction(QIcon(":/HPSMainWindow/genericIcon"), "User Code 1");
    connect(userCodeMenu1, SIGNAL(triggered()), widget, SLOT(onUserCode1()));

    // HOOPS AI features. MFR classifies faces of the loaded model; Similarity Comparison ranks the
    // shape similarity of the two currently loaded parts (File > Open + File > Add).
    QMenu* hoopsAiMenu = menuBar()->addMenu(tr("&HOOPS AI"));
    QAction* mfrAction = hoopsAiMenu->addAction(QIcon(":/HPSMainWindow/genericIcon"), "MFR Inference");
    similarityAction = hoopsAiMenu->addAction(QIcon(":/HPSMainWindow/genericIcon"), "Similarity Comparison");
    similarityAction->setEnabled(false);

    connect(mfrAction, SIGNAL(triggered()), widget, SLOT(onMfrInference()));
    connect(similarityAction, SIGNAL(triggered()), widget, SLOT(onSimilarityComparison()));

    // Toggle for the Similarity Search dock panel (index build/search, tagging, shape map). Kept in
    // the HOOPS AI menu so the panel is discoverable alongside the other HOOPS AI features.
    hoopsAiMenu->addSeparator();
    hoopsAiMenu->addAction(similarityDock->toggleViewAction());

    QToolBar* toolbar = new QToolBar("main_toolbar", this);
    QAction* toolbarOpen = toolbar->addAction(QIcon(":/HPSMainWindow/openIcon"), "Open");
    toolbar->addSeparator();
    QAction* toolbarOrbit = toolbar->addAction(QIcon(":/HPSMainWindow/orbitIcon"), "Orbit");
    QAction* toolbarPan = toolbar->addAction(QIcon(":/HPSMainWindow/panIcon"), "Pan");
    QAction* toolbarZoomArea = toolbar->addAction(QIcon(":/HPSMainWindow/zoomAreaIcon"), "Zoom Area");
    QAction* toolbarFly = toolbar->addAction(QIcon(":/HPSMainWindow/flyIcon"), "Fly");
    toolbar->addSeparator();
    QAction* toolbarFit = toolbar->addAction(QIcon(":/HPSMainWindow/zoomFitIcon"), "Zoom Fit");
    toolbar->addSeparator();
    QAction* toolbarPoint = toolbar->addAction(QIcon(":/HPSMainWindow/pointIcon"), "Point");
    QAction* toolbarArea = toolbar->addAction(QIcon(":/HPSMainWindow/areaIcon"), "Area");
    toolbar->addSeparator();
    toolbarSmooth = toolbar->addAction(QIcon(":/HPSMainWindow/smoothIcon"), "Smooth");
    QAction* toolbarHiddenLine = toolbar->addAction(QIcon(":/HPSMainWindow/hiddenLineIcon"), "Hidden Line");

    toolbarOrbit->setCheckable(true);
    toolbarPan->setCheckable(true);
    toolbarZoomArea->setCheckable(true);
    toolbarFly->setCheckable(true);
    toolbarPoint->setCheckable(true);
    toolbarArea->setCheckable(true);
    toolbarSmooth->setCheckable(true);
    toolbarHiddenLine->setCheckable(true);

    QActionGroup* operatorStates = new QActionGroup(this);
    operatorStates->addAction(toolbarOrbit);
    operatorStates->addAction(toolbarPan);
    operatorStates->addAction(toolbarZoomArea);
    operatorStates->addAction(toolbarFly);
    operatorStates->addAction(toolbarPoint);
    operatorStates->addAction(toolbarArea);
    toolbarOrbit->setChecked(true);
    operatorStates->setExclusive(true);

    QActionGroup* modesStates = new QActionGroup(this);
    modesStates->addAction(toolbarSmooth);
    modesStates->addAction(toolbarHiddenLine);
    toolbarSmooth->setChecked(true);
    modesStates->setExclusive(true);

    this->addToolBar(toolbar);

    connect(toolbarOpen, SIGNAL(triggered()), widget, SLOT(onFileOpen()));
    connect(toolbarOrbit, SIGNAL(triggered()), widget, SLOT(onOperatorOrbit()));
    connect(toolbarPan, SIGNAL(triggered()), widget, SLOT(onOperatorPan()));
    connect(toolbarZoomArea, SIGNAL(triggered()), widget, SLOT(onOperatorZoomArea()));
    connect(toolbarFly, SIGNAL(triggered()), widget, SLOT(onOperatorFly()));
    connect(toolbarFit, SIGNAL(triggered()), widget, SLOT(onOperatorZoomFit()));
    connect(toolbarPoint, SIGNAL(triggered()), widget, SLOT(onOperatorPoint()));
    connect(toolbarArea, SIGNAL(triggered()), widget, SLOT(onOperatorArea()));
    connect(toolbarSmooth, SIGNAL(triggered()), widget, SLOT(onModeSmooth()));
    connect(toolbarHiddenLine, SIGNAL(triggered()), widget, SLOT(onModeHiddenLine()));

    widget->initializeBrowsers();
}

void HPSMainWindow::quit() { close(); }

void HPSMainWindow::setCurrentFile(QString const& filename)
{
    QSettings settings;
    curFile = filename;
    setWindowFilePath(curFile);

    if (similarityPanel)
        similarityPanel->setCurrentCadPath(filename);

    QStringList files = settings.value("recentFileList").toStringList();
    files.prepend(filename);
    while (files.size() > maxRecentFiles)
        files.removeLast();

    settings.setValue("recentFileList", files);

    int numRecentFiles = qMin(files.size(), (int)maxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 %2").arg(i + 1).arg(strippedName(files[i]));
        recentFiles[i]->setText(text);
        recentFiles[i]->setData(files[i]);
        recentFiles[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < maxRecentFiles; ++j)
        recentFiles[j]->setVisible(false);

    updateModelDependentActions();
}

void HPSMainWindow::updateModelDependentActions()
{
    if (fileMenuAdd)
        fileMenuAdd->setEnabled(widget->getCADModel().Type() != HPS::Type::None);
    if (similarityAction)
        similarityAction->setEnabled(widget->hasTwoPartsForSimilarity());
}

QString HPSMainWindow::strippedName(QString const& fullFileName) { return QFileInfo(fullFileName).fileName(); }

void HPSMainWindow::openRecentFile()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action)
        widget->onFileOpen(action->data().toString());
}

void HPSMainWindow::onLoadMfrModel()
{
#ifdef USING_EXCHANGE
    QString const path = HPSWidget::PromptForCheckpoint(this);
    if (path.isEmpty())
        return; // user cancelled

    QString errorMessage;
    if (HPSWidget::LoadMfrModelFrom(path, errorMessage)) {
        QMessageBox::information(this, tr("Load MFR Model"),
                                 tr("MFR model loaded:\n%1").arg(path));
    }
    else if (!errorMessage.isEmpty()) {
        HPSWidget::ShowLoadError(this, tr("Load MFR Model"), errorMessage);
    }
#endif
}

void HPSMainWindow::onLoadSimilarityModel()
{
#ifdef USING_EXCHANGE
    QString const path = HPSWidget::PromptForCheckpoint(this);
    if (path.isEmpty())
        return; // user cancelled

    QString errorMessage;
    if (HPSWidget::LoadEmbeddingsModelFrom(path, errorMessage)) {
        QMessageBox::information(this, tr("Load Similarity Search Model"),
                                 tr("Similarity search model loaded:\n%1\n\n"
                                    "The similarity index has been reset "
                                    "(vectors from different models are not comparable).")
                                     .arg(path));
    }
    else if (!errorMessage.isEmpty()) {
        HPSWidget::ShowLoadError(this, tr("Load Similarity Search Model"), errorMessage);
    }
#endif
}

void HPSMainWindow::addProperty(QtSceneTreeItem* item, HPS::SceneTree::ItemType itemType)
{
    propertyBrowser->addProperty(item, itemType);
}

void HPSMainWindow::unsetAttribute(QtSceneTreeItem* item) { propertyBrowser->unsetAttribute(item); }

void HPSMainWindow::flushProperties() { propertyBrowser->flush(); }

ModelBrowserWidget* HPSMainWindow::getModelBrowser()
{
    if (modelBrowserAndConfigurations == nullptr)
        return nullptr;

    return modelBrowserAndConfigurations->getModelBrowser();
}

ConfigurationWidget* HPSMainWindow::getConfigurationBrowser()
{
    if (modelBrowserAndConfigurations == nullptr)
        return nullptr;

    return modelBrowserAndConfigurations->getConfigurationBrowser();
}

SegmentBrowserTree* HPSMainWindow::getSegmentBrowser()
{
    if (segmentBrowser == nullptr)
        return nullptr;

    return segmentBrowser->getSegmentTree();
}

PropertyWidget* HPSMainWindow::getPropertyBrowser() { return propertyBrowser; }
