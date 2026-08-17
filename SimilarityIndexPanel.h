#ifndef SIMILARITY_INDEX_PANEL_H
#define SIMILARITY_INDEX_PANEL_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QAtomicInt>
#include <QAbstractListModel>
#include <QSize>

#include "HoopsAiIndex.h"

class QListView;
class QListWidget;
class QListWidgetItem;
class QThread;
class QProgressDialog;
class QProgressBar;
class QSlider;
class QLabel;
class QComboBox;
class QPushButton;
class QTimer;
class QImage;

/* Sidecar metadata database for a similarity index. Everything here is a Qt-side concept (the
    bridge/FAISS index knows nothing about it): each registered part id can carry 0..N free-form
    tag names (e.g. "bolt", "nut", "bracket") AND a lightweight file signature (mtime + size) used
    to skip re-embedding files that have not changed since they were last added. The whole thing is
    persisted as JSON next to the index, at <indexBase>.json (same folder and stem as
    <indexBase>.faiss), so it travels with the index and is loaded/saved together with it. Plain
    value class (no QObject), so it needs no moc and can live directly in the panel.

    JSON schema (per part id the value is an object):
        { "version": 1,
          "parts": {
            "<id>": { "tags": ["bolt", ...], "file": { "mtime": <sec>, "size": <bytes> } }
          } }
    The legacy layout where "<id>" maps directly to a tag ARRAY (no file signature) is still read,
    so pre-existing tag files load unchanged and are rewritten in the object form on the next save. */
class MetaStore {
  public:
    /* Lightweight per-file change signature. size < 0 means "unknown / not recorded". */
    struct FileSig {
        qint64 mtime = 0;   // Last-modified time, seconds since epoch.
        qint64 size = -1;   // File size in bytes (-1 => not recorded).
        bool valid() const { return size >= 0; }
    };
    /* Point the store at the tag file derived from an index .faiss path (<stem>.json next to it)
        and load it if present. A missing file is not an error (the store is simply empty). Returns
        false only when a file exists but could not be parsed (outError is filled). */
    bool load(const QString& faissPath, QString* outError = nullptr);
    /* Write the current map to the .json path set by load(). Returns false + outError on failure. */
    bool save(QString* outError = nullptr) const;
    /* Forget every mapping and the associated file path (used when the index is closed). */
    void clear();

    /* All distinct tag names currently in use, sorted case-insensitively. */
    QStringList allTags() const;
    /* The tags assigned to one part id (empty when none / unknown). */
    QStringList tagsForPart(const QString& partId) const;
    /* The part ids carrying a given tag. */
    QStringList partsForTag(const QString& tag) const;
    bool partHasTag(const QString& partId, const QString& tag) const;

    /* Assign tag to every id in parts (idempotent per part). Does not save; the caller saves. */
    void assignTagToParts(const QString& tag, const QStringList& parts);
    /* Replace the full set of tags on a single part (empty list clears it). Does not save. */
    void setTagsForPart(const QString& partId, const QStringList& tags);
    /* Remove tag from every part that has it. Does not save; the caller saves. */
    void removeTag(const QString& tag);

    /* --- File-change signatures (used by Add Folder to skip unchanged files) --- */
    /* The recorded signature for a part id (an invalid FileSig when none is stored). */
    FileSig fileSignature(const QString& partId) const;
    /* True when a signature is stored for partId and it matches the given mtime+size, i.e. the file
        has not changed since it was last embedded and can be skipped. */
    bool fileUnchanged(const QString& partId, qint64 mtime, qint64 size) const;
    /* Record/replace the file signature for a part id. Does not save; the caller saves. */
    void setFileSignature(const QString& partId, qint64 mtime, qint64 size);

    QString jsonPath() const { return m_jsonPath; }

  private:
    QString m_jsonPath;                       // <indexBase>.json (empty when no index is loaded).
    QMap<QString, QSet<QString>> m_partTags;  // partId -> set of tag names.
    QMap<QString, FileSig> m_partFile;        // partId -> last-embedded file signature.
};

/* Worker object that performs the blocking bridge calls (add / search). It is moved onto its own
    QThread so the embedded-Python calls never run on the GUI thread. Results are delivered back to
    the GUI thread through queued signals; the worker only returns plain data (paths, scores) so the
    GUI thread can build the QPixmaps. */
class IndexWorker : public QObject {
    Q_OBJECT
  public:
    explicit IndexWorker(QObject* parent = nullptr) : QObject(parent) {}

    // Cancellation flag for the folder batch. Set from the GUI thread (atomic, so no locking) and
    // polled by doAddFolder between files; resetCancel() must be called before starting a batch.
    void resetCancel() { m_cancel.storeRelaxed(0); }
    void requestCancel() { m_cancel.storeRelaxed(1); }

  public slots:
    void doAdd(const QString& cadPath);
    void doSearch(const QString& cadPath, int topK);
    // Assembly-to-assembly search (HoopsAiIndex::searchAssembly). Reuses the searchFinished signal
    // since it produces the same QVector<SimHit> shape as doSearch.
    void doSearchAssembly(const QString& cadPath, int topK);
    // Computes the shape map of the current index (HoopsAiIndex::computeShapeMap) off the GUI
    // thread; dims is 2 or 3 and nClusters<=0 means auto. onlyIds restricts the map to those file
    // ids (empty => whole index). Result delivered via shapeMapComputed.
    void doComputeShapeMap(int nClusters, int dims, const QStringList& onlyIds);
    // Batch-adds every CAD file in cadPaths to the current index using a TWO-PASS strategy that
    // recovers heavy assemblies which would otherwise time out. Pass 1 embeds the whole list with
    // pass1Workers parallel workers and a pass1TimeLimit-second per-file budget (the many light
    // files finish here). Any file dropped with a Timeout is retried in pass 2 with pass2Workers
    // (few, since a single heavy file is not sped up by workers -- only across-file parallelism
    // helps) and a larger pass2TimeLimit budget. The three resulting groups (light-added,
    // heavy-added, still-failed) are written to a report log next to the index. A time-limit <= 0
    // keeps hoops_ai's 120 s default. Progress is reported as phase ticks (each pass is one
    // uninterruptible bulk call), so the panel shows an indeterminate busy bar.
    void doAddFolder(const QStringList& cadPaths, int pass1Workers, int pass1TimeLimit,
                     int pass2Workers, int pass2TimeLimit);
    // Lists the parts already registered in the current index (up to limit; <=0 means all).
    void doListParts(int limit);

  signals:
    void addFinished(bool success, const QString& message);
    void searchFinished(bool success, const QVector<SimHit>& hits, const QString& message,
                        qint64 elapsedMs);
    // pass identifies which of the two passes the tick belongs to (1 or 2); the GUI routes it to
    // that pass's own progress bar so pass 1's final state (including its error/timeout counts)
    // stays visible while pass 2 runs below it. done/total are the current pass's file counts and
    // text is the human-readable status line (may include "errors N" / "heavy N").
    void addFolderProgress(int pass, int done, int total, const QString& text);
    // Emitted right after Pass 1 finishes and its faiss index has been saved, carrying the files
    // that made it into the index in Pass 1. The GUI commits their change signatures to the sidecar
    // JSON immediately, so that a force-kill during the (long) Pass 2 still leaves Pass 1's results
    // fully persisted in BOTH the faiss index and the JSON.
    void addFolderPass1Committed(const QStringList& addedFiles);
    void addFolderFinished(int added, int failed, bool canceled, const QString& details,
                           const QString& logPath, const QStringList& addedFiles);
    void listPartsFinished(bool success, const QVector<SimHit>& hits, const QString& message);
    // Emitted when doComputeShapeMap finishes: success + the computed points (one per file id) +
    // the number of clusters produced, or an error message.
    void shapeMapComputed(bool success, const QVector<ShapeMapPoint>& points, int clusterCount,
                          const QString& message);

  private:
    QAtomicInt m_cancel{0};
    // Which pass (1 or 2) is currently running; read by the live-progress callback (worker thread)
    // to label the tick text. 0 while idle.
    int m_currentPass = 0;
};

/* List model backing the thumbnail gallery (a virtualized QListView). It holds a lightweight
    QVector<SimHit> and decodes thumbnails lazily off the GUI thread, so indexes with tens of
    thousands of parts scroll and re-filter without stutter: data() returns a cached QPixmap when
    one is available, otherwise a name placeholder plus a queued background decode whose result is
    delivered later via onThumbnailReady(). A generation counter invalidates in-flight decodes
    whenever the row set changes (open / search / filter), so stale images are dropped. Lives in the
    already-moc'd panel header so no extra moc wiring is needed. */
class IndexGalleryModel : public QAbstractListModel {
    Q_OBJECT
  public:
    // Extra roles consumed by the gallery delegate to render text above the thumbnail.
    enum GalleryRole {
        ScoreRole = Qt::UserRole + 1,      // float: similarity score for the row.
        ShowScoreRole = Qt::UserRole + 2,  // bool: whether this listing carries scores (search vs. browse).
        KindRole = Qt::UserRole + 3        // QString: per-row "Part"/"Assembly" (empty = hide the line).
    };

    explicit IndexGalleryModel(QObject* parent = nullptr);

    void setThumbnailSize(const QSize& size) { m_thumbSize = size; }
    // Replace all rows (begin/endResetModel). showScore appends the similarity score under the name.
    void setRows(const QVector<SimHit>& rows, bool showScore);
    void clearRows();

    // The part id for a row (empty when out of range); used to resolve double-clicks.
    QString idAt(int row) const;
    // Row index carrying the given part id, or -1 if none. Used to select a part from a view pick.
    int rowForId(const QString& id) const;
    // Row index whose part id matches the given file path after path normalization (slash/case),
    // or -1 if none. Used to locate the currently-open model in the browse listing.
    int rowForPath(const QString& path) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

  private slots:
    // Delivered (queued) from a background decode task; installs the pixmap and refreshes its row.
    void onThumbnailReady(int row, const QString& path, const QImage& image, int generation);

  private:
    void requestThumbnail(int row, const QString& path) const;

    QVector<SimHit>       m_rows;
    bool                  m_showScore = false;
    QSize                 m_thumbSize{160, 120};
    int                   m_generation = 0;  // Bumped on every setRows/clear; tags decode tasks.
    mutable QSet<QString> m_inFlight;        // Thumbnail paths currently being decoded.
};

/* Dockable panel that drives the similar-parts index: open/create an index directory, add the
    model currently shown in the 3D view, and search the index. Results are shown as a thumbnail
    grid (QListWidget in IconMode). Double-clicking a hit asks the host to load that CAD file. */
class SimilarityIndexPanel : public QWidget {
    Q_OBJECT
  public:
    explicit SimilarityIndexPanel(QWidget* parent = nullptr);
    ~SimilarityIndexPanel() override;

  public slots:
    // Called by the host whenever a new model is shown in the 3D view; this path becomes both the
    // "add" target and the search query.
    void setCurrentCadPath(const QString& cadPath);

    // Selects the gallery item carrying partId and scrolls it into view (view -> panel: the user
    // picked a marker in the 3D shape map). No-op when the id is not in the current listing.
    void selectPartInList(const QString& partId);

  signals:
    // Emitted when the user double-clicks a hit: requests the host load this CAD file.
    void loadCadRequested(const QString& cadPath);
    // Emitted when the shape map has been computed (via the Map command): the host renders these
    // points as a colored point cloud in the 3D view and shows the legend. Colors are resolved on
    // the Qt side (by tag when the index has tags, otherwise by k-means cluster).
    void shapeMapReady(const QVector<ShapeMapPoint>& points,
                       const QVector<ShapeMapLegendEntry>& legend);
    // Emitted when the user selects a single part in the gallery: the host highlights the matching
    // point on the shape map (empty id clears the highlight).
    void partSelected(const QString& partId);
    // Emitted with a short human-readable status line (e.g. part/assembly search timing) that the
    // host shows in the main-window status bar. timeoutMs<=0 leaves the message until replaced.
    void statusMessage(const QString& text, int timeoutMs);

  private slots:
    void onOpenIndex();
    void onAddCurrent();
    void onAddFolder();
    // Toggled handler for the checkable "Search with Current Model" button: ON runs a similarity
    // search on the current model; OFF shows the parts already in the index.
    void onSearchToggled(bool checked);
    // Toggled handler for the checkable "Search Similar Assembly" button: ON runs an assembly-to-
    // assembly similarity search on the current model; OFF shows the parts already in the index.
    // Mutually exclusive with the part-search toggle.
    void onAssemblySearchToggled(bool checked);
    void onCloseIndex();
    // Builds and shows the shape map of the current index in the 3D view (Map command).
    void onShapeMap();
    // Delivered (queued) from the worker when the shape map has been computed: ends the busy state
    // and forwards the points to the host (shapeMapReady) or shows the error.
    void onShapeMapComputed(bool success, const QVector<ShapeMapPoint>& points, int clusterCount,
                            const QString& message);
    void onItemActivated(const QModelIndex& index);
    void onAddFinished(bool success, const QString& message);
    void onSearchFinished(bool success, const QVector<SimHit>& hits, const QString& message,
                          qint64 elapsedMs);
    void onAddFolderProgress(int pass, int done, int total, const QString& text);
    // Commits Pass 1's file-change signatures to the sidecar JSON as soon as Pass 1 has saved its
    // faiss index, keeping the two files in sync even if Pass 2 is force-killed.
    void onAddFolderPass1Committed(const QStringList& addedFiles);
    void onAddFolderFinished(int added, int failed, bool canceled, const QString& details,
                             const QString& logPath, const QStringList& addedFiles);
    void onListPartsFinished(bool success, const QVector<SimHit>& hits, const QString& message);
    // Re-filters the cached search hits by the similarity slider without hitting the bridge again.
    void onThresholdChanged(int value);
    // Tag combobox changed (listing mode only): re-filters the cached listing by the chosen tag.
    void onTagFilterChanged(int index);
    // Assigns a tag (new or existing) to every part currently shown by the similarity slider
    // (search mode only): the displayed cluster becomes members of that tag.
    void onAssignTag();
    // Removes the tag currently selected in the combobox from all its parts (listing mode only),
    // then resets the combobox to "All".
    void onRemoveTag();
    // Edits the tag set of the part currently shown in the part-info panel via a small dialog,
    // then saves and refreshes the affected UI (part panel, tag combobox).
    void onEditPartTags();

  signals:
    // Internal: hand work to the worker thread (connected with Qt::QueuedConnection).
    void requestAdd(const QString& cadPath);
    void requestSearch(const QString& cadPath, int topK);
    void requestSearchAssembly(const QString& cadPath, int topK);
    void requestShapeMap(int nClusters, int dims, const QStringList& onlyIds);
    void requestAddFolder(const QStringList& cadPaths, int pass1Workers, int pass1TimeLimit,
                          int pass2Workers, int pass2TimeLimit);
    void requestListParts(int limit);

  private:
    void updateActionState();
    // Records the change signatures (from m_pendingSignatures) of the given added files into m_meta
    // and saves the sidecar JSON. Used after each pass so the JSON stays aligned with the faiss
    // index that pass just saved. Missing entries (files not in m_pendingSignatures) are ignored.
    void commitFileSignatures(const QStringList& files);
    // True when either similarity-search toggle is on (part search or assembly search). Both off
    // means the index-listing mode.
    bool isSearchMode() const;
    void beginBusy(const QString& label);
    void endBusy();
    QString defaultIndexRoot() const;
    // Refreshes the result list according to the toggle: search the current model when the Search
    // button is ON (and a model is loaded), otherwise show the parts already in the index.
    void refreshResults();
    // Populates the thumbnail list from hits. When showScore is true each item shows its similarity
    // score under the name (search results); otherwise only the name is shown (index listing).
    void populateHits(const QVector<SimHit>& hits, bool showScore);
    // In browse (non-search) mode, if the model currently open in the 3D view is present in the
    // listing, selects it and scrolls it into view. No-op otherwise (cheap: a single row lookup).
    void selectCurrentModelInList();
    // Scrolls the gallery to the given row, retrying across event-loop cycles until the batched
    // (QListView::Batched) layout has placed that row so scrollTo can actually reach it. token
    // guards against a newer listing superseding this scroll request.
    void scrollToRowWhenReady(int row, int token, int attempt);
    // Shows the cached search hits whose score passes the similarity slider (search mode only).
    void applyThresholdFilter();
    // Current similarity threshold from the slider (0.400 E.000).
    double thresholdValue() const;
    // Sets the slider's minimum to the floor appropriate for the active search mode (part vs
    // assembly) and clamps the current value up into the new range. See kPartThreshMin / kThreshMin.
    void applyThresholdRangeForMode(bool assemblyMode);
    // Ensures thumbnails resolve for the just-opened index: if the bridge's default per-index image
    // folder is missing, prompts (once, remembered per index in QSettings) for a separate image
    // folder and applies it via HoopsAiIndex::setThumbnailDir. faissPath is the opened .faiss path.
    void applyThumbnailDirForIndex(const QString& faissPath);
    // Shows/hides the similarity slider bar (only meaningful in search mode).
    void setThresholdBarVisible(bool visible);
    // Shows/hides the tag filter bar (only meaningful in the index-listing / OFF mode).
    void setTagBarVisible(bool visible);
    // Rebuilds the tag combobox from the tag store ("All" first, then every tag), trying to keep
    // the given selection (falls back to "All"). Signals are blocked during the rebuild.
    void rebuildTagCombo(const QString& keepTag = QString());
    // The tag currently chosen in the combobox, or an empty string when "All" or "Tagged" is
    // selected (both are non-specific and handled via their sentinel data, not by name).
    QString currentTagFilter() const;
    // True when the special "Tagged" entry is selected (show only parts carrying at least one tag).
    bool isTaggedFilter() const;
    // Returns "Part"/"Assembly" when the corresponding kind entry is selected, else an empty string.
    QString kindFilter() const;
    // Selects the special "Tagged" combobox entry (no-op when the index has no tags), applying the
    // filter in listing mode. Used by the Map command to restrict the listing to tagged parts.
    void selectTaggedFilter();
    // Re-filters the cached listing hits by the current tag selection (listing mode only).
    void applyTagFilter();
    // The ids of the search hits currently passing the similarity slider (the displayed cluster).
    QStringList displayedSearchIds() const;
    void refreshIndexInfo(bool warnOnFailure = false);
    void setIndexInfo(const IndexInfo& info);
    void clearIndexInfo();

    // Populates the bottom info area with the selected gallery part's file name, path and tags,
    // temporarily replacing the index-metadata view. Empty id restores the index view.
    void showPartInfo(const QString& partId);
    // Restores the index-metadata view in the bottom info area (hides the part-info panel).
    void hidePartInfo();

    QListView*       m_view = nullptr;           // Virtualized thumbnail gallery (IconMode).
    IndexGalleryModel* m_model = nullptr;        // Backing model with lazy async thumbnails.
    QThread*         m_thread = nullptr;
    IndexWorker*     m_worker = nullptr;
    QProgressDialog* m_progress = nullptr;
    // Two-pass Add Folder progress dialog: a custom modal dialog with one progress bar per pass so
    // pass 1's final state (with its error/timeout counts) stays on screen while pass 2 runs in the
    // lower bar. Only used by the Add Folder flow; other busy operations still use m_progress.
    QDialog*         m_addFolderDialog = nullptr;
    QProgressBar*    m_pass1Bar = nullptr;
    QLabel*          m_pass1Status = nullptr;
    QWidget*         m_pass2Section = nullptr;   // Title + bar + status; disabled until pass 2 runs.
    QProgressBar*    m_pass2Bar = nullptr;
    QLabel*          m_pass2Status = nullptr;
    QWidget*         m_thresholdBar = nullptr;   // Container for the similarity slider + labels.
    QSlider*         m_threshold = nullptr;      // Min-similarity slider (400 E000 => 0.400 E.000).
    QLabel*          m_thresholdValueLabel = nullptr;
    QTimer*          m_thresholdDebounce = nullptr;  // Coalesces rapid slider ticks into one filter.
    QWidget*         m_tagBar = nullptr;         // Container for the tag filter combobox (OFF mode).
    QComboBox*       m_tagFilter = nullptr;      // "All" + one entry per tag; filters the listing.
    QWidget*         m_infoPanel = nullptr;      // Bottom panel showing current index metadata.
    QLabel*          m_infoNameLabel = nullptr;
    QLabel*          m_infoPathLabel = nullptr;
    QLabel*          m_infoFileCountLabel = nullptr;
    QLabel*          m_infoBodyCountLabel = nullptr;
    QLabel*          m_infoAssemblyCountLabel = nullptr;
    QLabel*          m_infoSinglePartCountLabel = nullptr;
    QLabel*          m_infoDimensionLabel = nullptr;

    // Part-info view: sits in the same layout slot as m_infoPanel and is shown instead of it while
    // a gallery thumbnail is selected (file name / path / tags). See showPartInfo/hidePartInfo.
    QWidget*         m_partInfoPanel = nullptr;
    QLabel*          m_partNameLabel = nullptr;
    QLabel*          m_partTypeLabel = nullptr;
    QLabel*          m_partBodiesLabel = nullptr;
    QLabel*          m_partPathLabel = nullptr;
    QLabel*          m_partTagsLabel = nullptr;
    QPushButton*     m_partEditTagsButton = nullptr;  // "Edit…" button in the part-info Tags row.
    QString          m_partInfoId;                    // Part id currently shown in m_partInfoPanel.
    // Remembers whether the index-metadata panel should be visible, so hidePartInfo() can restore
    // its correct visibility once the part panel is hidden.
    bool             m_indexInfoVisible = false;

    QString m_currentCadPath;   // Model currently in the 3D view (add target / search query).
    // Increments on each browse-listing scroll request; a running scrollToRowWhenReady retry chain
    // aborts once it sees a newer token (i.e. a fresh listing has superseded it).
    int m_scrollToken = 0;
    QString m_indexFaissPath;   // .faiss path of the open index (used to locate the tag .json).
    bool    m_indexOpen = false;
    bool    m_busy = false;

    // Sidecar metadata database for the open index (loaded on open, saved on assign/remove and
    // after an Add Folder batch records file-change signatures).
    MetaStore m_meta;

    // Add Folder change-detection bookkeeping. Before a batch, m_pendingSignatures caches the
    // current (mtime,size) of every file we send to embed; m_lastSkippedCount counts files that
    // were unchanged since last time and therefore excluded from the batch. On finish, the added
    // files' cached signatures are committed to m_meta so the next Add Folder can skip them.
    QHash<QString, QPair<qint64, qint64>> m_pendingSignatures;
    int m_lastSkippedCount = 0;

    // Cached search hits (fetched with a generous topK) so the similarity slider can re-filter the
    // displayed list without issuing another bridge search.
    QVector<SimHit> m_searchHits;
    // Cached index listing (OFF mode) so the tag combobox can re-filter it without re-listing.
    QVector<SimHit> m_listHits;

    // Toolbar actions kept as members so they can be enabled/disabled by state.
    class QAction* m_openAction = nullptr;
    class QAction* m_addAction = nullptr;
    class QAction* m_addFolderAction = nullptr;
    class QAction* m_searchAction = nullptr;
    class QAction* m_assemblySearchAction = nullptr;
    class QAction* m_mapAction = nullptr;
    class QAction* m_assignTagAction = nullptr;
    class QAction* m_removeTagAction = nullptr;
    class QAction* m_closeAction = nullptr;
};

#endif // SIMILARITY_INDEX_PANEL_H
