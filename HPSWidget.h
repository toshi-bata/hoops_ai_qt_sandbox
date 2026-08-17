#ifndef HPSWIDGET_H
#define HPSWIDGET_H

#include <QtWidgets/QtWidgets>
#include "sprk.h"
#include "HPSHandlers.h"
#include "HoopsAiIndex.h"

#ifdef USING_EXCHANGE
    #include "sprk_exchange.h"
    #include "hoops_ai_bridge.h"
    #include "hoops_license.h"
    #include <QMap>
    #include <vector>
#endif

class ModelBrowserWidget;
class ConfigurationWidget;
class SegmentBrowserTree;

class HPSWidget: public QWidget {
    Q_OBJECT
  public:
    HPSWidget(QWidget* parent = 0);
    ~HPSWidget();

    /* Returns the canvas */
    HPS::Canvas* getCanvas() { return &canvas; }

    /* Returns the current CADModel.
        If no CADModel has been loaded, cad_model will be of type HPS::Type::None. */
    HPS::CADModel getCADModel() { return cad_model; }

    /* Returns true when exactly two parts are currently loaded (a base model opened with File > Open
        and a second model brought in with File > Add), i.e. when the Similarity Comparison command is
        applicable. */
    bool hasTwoPartsForSimilarity() const {
#ifdef USING_EXCHANGE
        return !m_firstPartPath.isEmpty() && !m_secondPartPath.isEmpty();
#else
        return false;
#endif
    }

    /* Sets up default operators and event handlers */
    void setupSceneDefaults();

    /* Attaches in_view to the canvas.
        Operators that are attached to the current view are moved to in_view
        A distant light is inserted in in_view */
    void AttachView(HPS::View const& in_view);

    /* Shows the orientation axis triad in the lower-left corner of the given view. Applied to
        every view the widget attaches so the triad persists across model loads / view resets. */
    void EnableAxisTriad(HPS::View const& in_view);

    /* Sets the solid background color of the view. Applied to the window key (behind the transparent
        view subwindow), which is stable across model loads so the color persists. */
    void ApplyViewBackground();

    /* Same as AttachView, but the camera smoothly moves from the camera
        contained by the current view to the camera contained in in_view. */
    void AttachViewWithSmoothTransition(HPS::View& in_view);

    /* Unhighlights everything that uses the default highlight style.
        Injects events notifying Components that a unhighlight event happened,
        so that the Model Browser state can be updated. */
    void Unhighlight();

    /* Smoothly zooms the camera to the bounding of keyPath. */
    void ZoomToKeyPath(HPS::KeyPath const& keyPath);

    /* Smoothly restores the camera to the settings it contained before
        it was zoomed in through a call to ZoomToKeyPath. */
    void RestoreCamera();

    /* Resets the value of the key path stored through a call to ZoomToKeyPath*/
    void InvalidateZoomKeyPath();

    /* Resets the value of the camera calculated through the call to ZoomToKeyPath*/
    void InvalidateSavedCamera();

    void initializeBrowsers();

    /* Given the selection results from a view-side pick (the "Point" select operator), if a shape
        map is currently shown, resolves the picked marker to its part id (nearest stored point to
        the selection position), highlights that point, and emits shapeMapPartPicked. Returns true
        when a shape-map point was resolved, so the caller skips the normal CAD highlight path. */
    bool PickShapeMapPart(HPS::SelectionResults const& results);

    /* Clears the shape-map point highlight (and tells the panel to deselect) when a shape map is
        shown. No-op otherwise. Called on an empty-space click so clicking nothing clears the pick. */
    bool ClearShapeMapHighlight();

#ifdef USING_EXCHANGE
    /* Activates an Exchange Capture and attaches it to the current canvas. */
    void ActivateCapture(HPS::ComponentPath const& capturePath);

    /* Imports the specified Exhcange configuration.
        The configuration will be loaded, and its default view will be attached
        to the canvas, replacing the current one. */
    void ImportConfiguration(HPS::UTF8Array const& configuration);
#endif

  public slots:
    void onFileOpen(QString filename = QString());
    /* Imports an additional CAD file into the model that is currently open, placing it under the
        existing CADModel (see HPS::Exchange::ImportOptionsKit::SetLocation). Only meaningful once a
        model is already open; the File > Add menu action is disabled otherwise.
        NOTE: declared unconditionally (not under #ifdef USING_EXCHANGE) so moc always registers it
        as a slot — moc is not invoked with -DUSING_EXCHANGE, so a guarded declaration would be
        omitted from the meta-object and the menu connection would fail silently at runtime. */
    void onFileAdd(QString filename = QString());

    /* Renders the index "shape map" in the 3D view: one colored marker per point (each point already
        carries its resolved RGB color, decided on the Qt side from tags or clusters), on a clean
        base view (any loaded CAD is dropped first). Shows a matching legend overlay from `legend`,
        remembers each id's position for HighlightShapeMapPoint, and fits the camera to the map.
        Declared unconditionally (not under USING_EXCHANGE) so moc always registers it as a slot. */
    void ShowShapeMap(const QVector<ShapeMapPoint>& points,
                      const QVector<ShapeMapLegendEntry>& legend);

    /* Highlights the shape-map point for the given file id (a larger marker in a highlight color on
        top of the cluster clouds). An empty/unknown id clears the current highlight. No-op when no
        shape map is currently shown. */
    void HighlightShapeMapPoint(const QString& partId);

  signals:
    // Emitted when the user picks a marker in the shape map with the view's select operator
    // (view -> panel): the panel selects the matching part in the gallery. Empty id when cleared.
    void shapeMapPartPicked(const QString& partId);
    // Emitted with a short status line (e.g. MFR / compare bridge-API timing) shown by the host in
    // the main-window status bar. timeoutMs mirrors QStatusBar::showMessage (0 = until replaced).
    void statusMessage(const QString& text, int timeoutMs);

  private slots:
    void onFileNew();
    void onFileSaveAs();

    void onOperatorOrbit();
    void onOperatorPan();
    void onOperatorZoomArea();
    void onOperatorFly();
    void onOperatorZoomFit();
    void onOperatorPoint();
    void onOperatorArea();

    void onModeSimpleShadow();
    void onModeFrameRate();
    void onModeSmooth();
    void onModeHiddenLine();
    void onModeEyeDome();

    void onUserCode1();
    void onMfrInference();
    void onSimilarityComparison();

  protected:
    void resizeEvent(QResizeEvent*);
    void paintEvent(QPaintEvent* e);

    void mousePressEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);

    void keyPressEvent(QKeyEvent* e);
    void keyReleaseEvent(QKeyEvent* e);

    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);

    void focusOutEvent(QFocusEvent*);

    QPaintEngine* paintEngine() const { return 0; }

  private:
    HPS::Canvas canvas;
    HPS::View view;
    HPS::Model model;
    HPS::CADModel cad_model;
    HPS::DistantLightKey mainDistantLight;

    // Shape map (index cluster visualization) state. m_shapeMapSegment is the root subsegment that
    // holds the per-cluster marker clouds; it is invalid (Type::None) when no map is shown.
    // Rebuilds the shape-map legend overlay (top-left) from the given entries; clears it when empty.
    void ShowShapeMapLegend(const QVector<ShapeMapLegendEntry>& legend);
    // Draws reference XYZ grid planes (floor + two back walls), tick labels and axis titles around
    // the map point cloud, so the 3D scatter reads like a matplotlib 3D plot. Grid geometry is a
    // subsegment of `mapSeg`, so it is removed together with the map.
    void AddShapeMapGrid(HPS::SegmentKey mapSeg, const QVector<ShapeMapPoint>& points);
    // Positions m_shapeMapLegendPanel in the top-left corner of this widget.
    void RepositionShapeMapLegend();
    // Shows/hides all markers of the shape-map group identified by a "r_g_b" (0..255) color key,
    // driven by the per-row checkboxes in the legend overlay.
    void SetShapeMapGroupVisible(const QString& colorKey, bool visible);

    HPS::SegmentKey m_shapeMapSegment;
    HPS::SegmentKey m_shapeMapHighlightSegment;
    QHash<QString, HPS::Point> m_shapeMapPoints;   // File id -> map position (for highlight).
    QHash<QString, HPS::SegmentKey> m_shapeMapGroupSegments; // "r_g_b" color key -> group subsegment.
    float m_shapeMapBounds[6] = {0, 0, 0, 0, 0, 0}; // Rounded grid box: {x0,y0,z0, x1,y1,z1}.
    QWidget* m_shapeMapLegendPanel = nullptr;      // Top-left legend overlay for the shape map.
    bool displayResourceMonitor;
    bool enableSimpleShadows;
    bool enableFrameRate;
    bool smoothRendering;
    bool eyeDome;

    HPS::KeyPath zoomToKeyPath;
    HPS::CameraKit preZoomToKeyPathCamera;

    MyErrorHandler errorHandler;
    MyWarningHandler warningHandler;

    WId GetWindowID();

    bool validPixelToWindowMatrix;
    HPS::MatrixKit pixelToWindowMatrix;

    HPS::MatrixKit const& getPixelToWindowMatrix();
    HPS::MouseEvent buildMouseEvent(QMouseEvent* in_event, HPS::MouseEvent::Action action, size_t click_count);
    HPS::KeyboardEvent buildKeyboardEvent(QKeyEvent* in_event, HPS::KeyboardEvent::Action action);
    void getModifierKeys(HPS::InputEvent* event);
    void updatePlanes();

    HPS::Stream::ImportNotifier importHSFFile(QString filename, QProgressDialog* progressDlg, bool& success);
    void importSTLFile(QString filename, QProgressDialog* progressDlg, bool& success);
    void importOBJFile(QString filename, QProgressDialog* progressDlg, bool& success);
    void ImportPointCloudFile(QString filename, QProgressDialog* progressDlg, bool& success);

#ifdef USING_EXCHANGE
    void
        importExchangeFile(QString filename, bool& success, HPS::Exchange::ImportOptionsKit const* customImportOptions = nullptr);

    /* Returns the file-type filter string used by the Open/Add file dialogs for Exchange builds. */
    QString exchangeFileDialogFilter() const;

    /* HOOPS AI MFR (Manufacturing Feature Recognition) support (User Code 2). */

    /* HOOPS AI itself is initialized once at application startup (see main.cpp, InitializeHoopsAI()
        analog to Exchange's SetExchangeLibraryDirectory()). This flag only tracks whether an MFR
        checkpoint has been loaded yet. HoopsAI_LoadMFRModel may be called again to swap models:
        loading the same path is a no-op, a different path replaces the current MFR model. */
    static bool s_mfrModelLoaded;

    /* Full path of the currently loaded MFR checkpoint (empty until one is loaded). The MFR and
        Embeddings slots are kept independent: loading one never affects the other. */
    static QString s_mfrCkptPath;

  public:
    /* Loads the given .ckpt as the MFR model via HoopsAI_LoadMFRModel (UTF-8). Returns false with an
        empty out_errorMessage when path is empty or the caller declines the type-mismatch warning
        (the caller should treat this as "cancelled"); returns false with a message on a real load
        failure. On success stores the path in s_mfrCkptPath and sets s_mfrModelLoaded. Callable from
        the File menu (HPSMainWindow) to load or swap the MFR model. */
    static bool LoadMfrModelFrom(QString const& path, QString& out_errorMessage);

  private:
    /* Ensures an MFR model is loaded. If one is already loaded, returns true immediately. Otherwise
        prompts the user with a checkpoint-open dialog (rooted at DefaultModelDir()) and loads the
        selection. Returns false and fills out_errorMessage on cancel/failure. */
    static bool EnsureMfrReady(QString& out_errorMessage);

    /* Assigns a color to every ExchangeTopoFace component of cad_model based on the per-face MFR
        labels returned by HoopsAI_RunMFRInference, and returns the number of faces found for each
        label (used to build the legend). Sets out_indexMismatch to true if the number of face
        components found in cad_model does not match labelCount (see design note in HPSWidget.cpp). */
    QMap<int, int> ColorFacesByMfrLabels(std::vector<int> const& labels, int labelCount, bool& out_indexMismatch);

    /* Creates or updates the overlay legend panel (top-right corner of the canvas) listing every
        label that is actually present in labelCounts, using kMfrLabelNames for the display name. */
    void UpdateMfrLegend(QMap<int, int> const& labelCounts);

    /* Positions mfrLegendPanel in the top-right corner of this widget. */
    void RepositionMfrLegend();

    /* Removes the per-face MFR colors applied by ColorFacesByMfrLabels (via ShellKey::UnsetFaceColors)
        from every ExchangeTopoFace shell, updates the canvas, and removes the legend panel. Invoked
        from the legend's close button. */
    void ClearMfrColors();

    QWidget* mfrLegendPanel;

    /* HOOPS AI Similarity Comparison — geometric visual diff (Model Compare) support.

        Runs HOOPS Exchange's A3DCompareFacesInBrepModels on the two loaded parts and renders the
        colored result model in the 3D view (faces colored by status: unchanged / added / removed,
        with modified sub-regions), plus an overlay legend. Declared here (implemented under
        USING_EXCHANGE) and driven from onSimilarityComparison. */

    /* Loads firstPath and secondPath as B-rep A3DAsmModelFiles, compares their faces within
        tolerance (model unit), color-codes the two parts already shown in the view (via
        ColorLoadedPartsByCompare), and returns per-status face counts. Returns false + outMessage
        on any failure. */
    bool RunModelCompareAndDisplay(QString const& firstPath, QString const& secondPath, double tolerance,
                                   int& outUnchanged, int& outRemoved, int& outAdded, QString& outMessage);

    /* Paints the two parts already loaded in the view (base part from File > Open, then the part
        from File > Add, which share one CADModel) according to the compare's per-face match arrays:
        matched faces -> Unchanged (gray), unmatched old faces -> Removed (red), unmatched new faces
        -> Added (green). The match arrays are passed as void* so the header stays free of the raw
        A3D headers; sizes are the old/new face counts. Returns false + outMessage on failure (e.g.
        the view's face count doesn't match old+new). */
    bool ColorLoadedPartsByCompare(void* oldFaceMatch, unsigned int oldFaceSize,
                                   void* newFaceMatch, unsigned int newFaceSize, QString& outMessage);

    /* Creates/updates the compare legend overlay (top-right): a show/hide checkbox + color swatch +
        counted label for each status group, and an info block (compared file names and, when
        available, the overall cosine similarity) below them. */
    void UpdateCompareLegend(int unchanged, int removed, int added, QString const& infoText);

    /* Shows or hides one status group's faces in the 3D view. group: 0 = Unchanged, 1 = Added,
        2 = Removed. Driven by the legend's per-group checkboxes. */
    void SetCompareGroupVisible(int group, bool visible);

    /* Positions compareLegendPanel in the top-right corner of this widget. */
    void RepositionCompareLegend();

    /* Removes the compare legend overlay and clears the diff coloring from the two loaded parts. */
    void ClearCompareLegend();

    /* Retires every face-analysis result currently on screen - MFR and Similarity Comparison - by
        clearing both their per-face colors/visibilities and their legend overlays.

        The two results are mutually exclusive by nature: they color the same ExchangeTopoFace shells
        and their legend panels are both pinned to the top-right corner of the canvas. Starting one
        command therefore has to retire the other explicitly; otherwise the previous panel stays on
        screen behind the new one, and its state (e.g. a compare group hidden with a legend checkbox)
        keeps affecting the geometry. Called by onMfrInference / onSimilarityComparison just before
        the new result is applied, and wherever the loaded model is replaced or dropped (File > Open,
        File > Add, ShowShapeMap) - in that case while cad_model is still valid. */
    void ClearFaceAnalysisOverlays();

    QWidget* compareLegendPanel = nullptr;

    /* Face shells of each compare status group, captured by ColorLoadedPartsByCompare so the legend
        checkboxes can toggle each group's visibility (index 0 = Unchanged, 1 = Added, 2 = Removed). */
    std::vector<HPS::ShellKey> m_compareGroupShells[3];

    /* HOOPS AI Shape Embeddings / Similarity Comparison support (User Code 3). */

    /* Tracks whether an embeddings checkpoint has been loaded yet, analogous to s_mfrModelLoaded.
        HoopsAI_LoadEmbeddingsModel may be called again to swap models: loading the same path is a
        no-op, a different path replaces the current model and resets the shared similarity index
        (vectors from different models are not comparable). */
    static bool s_embeddingsModelLoaded;

    /* Full path of the currently loaded embeddings checkpoint (empty until one is loaded). */
    static QString s_embeddingsCkptPath;

  public:
    /* Loads the given .ckpt as the similarity-search (embeddings) model via HoopsAI_LoadEmbeddingsModel
        (UTF-8). Same false/empty-message "cancelled" convention as LoadMfrModelFrom. On success stores
        the path in s_embeddingsCkptPath and sets s_embeddingsModelLoaded. Callable from the File menu
        (HPSMainWindow) to load or swap the similarity-search model. */
    static bool LoadEmbeddingsModelFrom(QString const& path, QString& out_errorMessage);

  public:
    /* Ensures a similarity-search model is loaded, prompting with a checkpoint-open dialog if needed.
        Returns false and fills out_errorMessage on cancel/failure. Callable from other panels (e.g.
        SimilarityIndexPanel) so opening an index also guarantees a model is loaded first. */
    static bool EnsureEmbeddingsReady(QString& out_errorMessage);

    /* Returns whether a similarity-search (embeddings) model has already been loaded. Lets callers
        (e.g. SimilarityIndexPanel) decide whether to show a notice before prompting for a checkpoint. */
    static bool IsEmbeddingsModelLoaded() { return s_embeddingsModelLoaded; }

  private:
    /* Default directory for the checkpoint-open dialog: <HOOPS_AI_HOME>\packages\trained_ml_models when
        HOOPS_AI_HOME is set, otherwise <exe>\..\models. Falls back to the exe directory if that folder
        does not exist. */
    static QString DefaultModelDir();

  public:
    /* Shows a "Checkpoints (*.ckpt)" open dialog rooted at DefaultModelDir(); returns the chosen path
        (empty if the user cancels). Callable from the File menu (HPSMainWindow). */
    static QString PromptForCheckpoint(QWidget* parent);

    /* Shows a critical error box for a failed model load. Backend failures embed a full Python
        traceback, so this keeps the visible text to a short summary (everything before "Traceback")
        and moves the raw message into the collapsible "Show Details…" area to avoid an oversized
        dialog. Callable from the File menu (HPSMainWindow). */
    static void ShowLoadError(QWidget* parent, QString const& title, QString const& fullMessage);

  private:

    /* Original on-disk file paths of the two loaded parts: the base model (File > Open) and the model
        added on top of it (File > Add). Used by the Similarity Comparison command, which passes them to
        HoopsAI_CompareEmbeddings. Cleared/reset appropriately in onFileOpen / onFileAdd. */
    QString m_firstPartPath;
    QString m_secondPartPath;
#endif

    ModelBrowserWidget* getModelBrowser();
    ConfigurationWidget* getConfigurationBrowser();
    SegmentBrowserTree* getSegmentBrowser();
};

#endif // HPSWIDGET_H
