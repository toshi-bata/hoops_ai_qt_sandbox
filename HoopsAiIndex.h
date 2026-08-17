#ifndef HOOPS_AI_INDEX_H
#define HOOPS_AI_INDEX_H

#include <QString>
#include <QVector>
#include <QMetaType>
#include <functional>

/* One similarity-search hit resolved from the current index, with its thumbnail path already
    resolved via HoopsAI_GetPartThumbnailPath. Plain data so it can be handed from a worker
    thread to the GUI thread through a queued signal (see the Q_DECLARE_METATYPE below). */
struct SimHit {
    QString id;                  // Registered part id (the CAD file path in this application).
    float   score = 0.0f;        // Similarity score returned by HoopsAI_SearchIndex.
    QString name;                // Display name (file name portion of id).
    QString thumbnailPath;       // Absolute '/'-separated PNG path (empty when unavailable).
    bool    thumbnailExists = false; // Whether the PNG is actually on disk.
    QString kind;                // "Part"/"Assembly" for browse listings (empty => hide the line).
};

/* One point of the index "shape map": a file id placed in a low-dimensional (2D/3D) cluster
    layout. Produced by HoopsAiIndex::computeShapeMap (bridge HoopsAI_ComputeIndexShapeMap) and
    handed from the worker thread to the GUI thread via a queued signal, so it is plain data with
    a Q_DECLARE_METATYPE below. x/y/z are scaled into a [-1,1] cube; z is 0 for a 2D projection. */
struct ShapeMapPoint {
    QString id;             // File id (the CAD file path), matching SimHit::id / the gallery ids.
    float   x = 0.0f;
    float   y = 0.0f;
    float   z = 0.0f;
    int     cluster = 0;    // 0-based cluster id from k-means (used when no tags color the map).
    // Resolved display color (0..1 RGB), decided on the Qt side from tags (when present) or the
    // cluster id. The 3D view renders each point as a marker of this color.
    float   cr = 0.6f;
    float   cg = 0.6f;
    float   cb = 0.6f;
};

/* One row of the shape-map legend: a colored group (a tag, a "Cluster N", or "Untagged") and how
    many points belong to it. Built on the Qt side alongside the per-point colors and handed to the
    3D view so it can draw a matching legend overlay. Plain data (Q_DECLARE_METATYPE below). */
struct ShapeMapLegendEntry {
    QString label;          // Tag name, "Cluster N", or "Untagged".
    float   r = 0.6f;
    float   g = 0.6f;
    float   b = 0.6f;
    int     count = 0;      // Number of points carrying this color.
};

struct IndexInfo {
    bool    hasIndex = false;
    QString faissPath;
    int     fileCount = 0;
    int     bodyCount = 0;
    int     assemblyCount = 0;
    int     singlePartCount = 0;
    int     dimension = 0;
};

/* Thin QString-facing wrapper over the hoops_ai_bridge C ABI index functions used by the Qt UI
    (open/close, add, search, list, stats, shape map, thumbnail resolution). Every call is blocking
    and may take seconds (it drives the embedded Python interpreter), so add/search should be issued
    from a worker thread. The bridge serializes its own calls internally (global mutex + GIL), so
    calling from a worker thread is safe. Errors/warnings come back through the out-parameter as
    UTF-8 bridge text (which may be a full Python traceback). */
class HoopsAiIndex {
  public:
    /* Open (creating it when missing) the index .faiss file and make it the current index.
        Returns false and fills outMessage on failure. */
    static bool openIndex(const QString& faissFilePath, QString& outMessage);

    /* Override the base directory used to resolve part thumbnails (PNGs) for the current index.
        Pass a folder that holds the "<parent>/<stem>_white.png" images (e.g. an index shipped with
        a separate image folder such as the tutorial's "images_tmcad"). An empty dir clears the
        override, reverting to the bridge's default per-index folder. The bridge clears the override
        on every openIndex, so re-apply it after opening. Returns false and fills outMessage on
        failure. */
    static bool setThumbnailDir(const QString& dir, QString& outMessage);

    /* Register the CAD file in the current index (the part id defaults to the file path). Returns
        true on success. outWarning may still be non-empty even when the call succeeds (e.g. the
        thumbnail could not be rendered); treat a non-empty outWarning on success as a warning. */
    static bool addCad(const QString& cadPath, QString& outWarning);

    /* Batch-register every CAD file in cadPaths into the current index using the bridge's
        embed_shape_batch-backed folder API (one bulk embed call + a single index save), instead
        of a per-file addCad loop. On return outAdded/outFailed hold the tallies, outFailedFiles
        lists the paths that failed (when derivable), and outWarning carries the first non-fatal
        warning (e.g. a thumbnail that could not be rendered) even on success. numWorkers is the
        embed_shape_batch worker count; pass 1 from the embedded-interpreter host. timeLimitSeconds
        is the per-file embedding budget (<= 0 keeps hoops_ai's 120 s default); raise it to let a
        heavy assembly finish instead of timing out and being dropped from the index. Returns
        false + outWarning (used as the error message) when the whole batch call fails. */
    static bool addCadFolder(const QStringList& cadPaths, int numWorkers, int timeLimitSeconds,
                             int& outAdded, int& outFailed, QStringList& outFailedFiles,
                             QString& outWarning);

    /* Live progress of the CURRENT/next addCadFolder call. The bridge parses hoops_ai's tqdm bar
        and invokes this callback (possibly many times per second) from the WORKER thread, so the
        callback must be cheap and thread-safe (e.g. emit a queued Qt signal). phase: 0 = main
        worker pool, 1 = heavy-file 1-worker fallback, -1 = unknown; done/total are the current
        bar's counts; errors/heavy are the tqdm postfix counters (-1 when absent). Pass a default-
        constructed (empty) std::function to stop forwarding. Registration is process-global. */
    using ProgressCallback =
        std::function<void(int phase, int done, int total, int errors, int heavy)>;
    static void setAddFolderProgressCallback(ProgressCallback callback);

    /* Search the current index for up to topK parts similar to cadPath and fill outHits (each hit
        already carries its resolved thumbnail path/existence). Returns false + outMessage on error;
        an empty index yields true with an empty outHits. */
    static bool search(const QString& cadPath, int topK, QVector<SimHit>& outHits, QString& outMessage);

    /* Assembly-to-assembly similarity search: ranks whole ASSEMBLIES in the current index against
        cadPath (via HoopsAI_SearchSimilarAssembly, an optimal one-to-one part matching with TF-IDF
        rare-part weighting and a bag-of-parts blend), filling outHits with up to topK assemblies.
        hit.score carries the final blended similarity; hit.id/name/thumbnail are the assembly file.
        Uses the bridge defaults for the tuning knobs (candidateK/simThresh/bopWeight/coverageMode)
        and enables the TF-IDF rare-part weighting. Returns false + outMessage on error; an empty
        index yields true with an empty outHits. */
    static bool searchAssembly(const QString& cadPath, int topK, QVector<SimHit>& outHits,
                               QString& outMessage);

    /* List the parts registered in the current index (up to limit, or all when limit <= 0), filling
        outHits with each part's id/name and resolved thumbnail. The score field is left at 0 (there
        is no query). Returns false + outMessage on error; an empty/absent index yields true with an
        empty outHits. */
    static bool listParts(int limit, QVector<SimHit>& outHits, QString& outMessage);
    /* Paged listing backed by HoopsAI_ListIndexPartsPaged: fetch up to `count` parts starting at
        `offset` (0-based) from the current index, filling outHits (id/name/thumbnail/exists; score
        stays 0) and outTotal with the index's total part count. A single bridge call resolves the
        whole window's thumbnails at once, avoiding the per-part O(n) resolution of listParts(), so
        this scales to indexes with tens of thousands of parts. Returns false + outMessage on error;
        an empty/absent index yields true with empty outHits and outTotal==0. */
    static bool listPartsPaged(int offset, int count, QVector<SimHit>& outHits, int& outTotal,
                               QString& outMessage);

    /* Compute the "shape map" of the current index for cluster visualization: aggregates the
        stored per-body vectors to one point per file id, projects them to 2D/3D (dims=2 or 3) via
        PCA, and assigns a cluster id per point via k-means (nClusters<=0 => auto). When onlyIds is
        non-empty the projection and clustering are restricted to those file ids (used to map only
        tagged parts); pass an empty list to map the whole index. Fills outPoints (id/x/y/z/cluster;
        coords in a [-1,1] cube) and outClusterCount. No CAD re-embedding, so it scales to tens of
        thousands of files. Returns false + outMessage on error; an empty index yields true with
        empty outPoints. */
    static bool computeShapeMap(int nClusters, int dims, const QStringList& onlyIds,
                                QVector<ShapeMapPoint>& outPoints,
                                int& outClusterCount, QString& outMessage);

    /* Close the current index (in-memory only; the files on disk are kept). */
    static bool close(QString& outMessage);
    /* Detailed information about the current index via HoopsAI_GetIndexStats.
       hasIndex=false means no index is open. */
    static bool currentIndexInfo(IndexInfo& outInfo, QString& outMessage);

    /* .faiss file path of the current index, or an empty string when none is open. */
    static QString currentIndexDir(QString& outMessage);

    /* Body/component count of a single registered part in the current index (>= 1). Sets
        outIsAssembly to true when the part has >= 2 bodies (same rule as the index stats'
        assembly/single-part split). Returns false + outMessage when there is no current index or
        the id is not registered. */
    static bool partBodyCount(const QString& partId, int& outBodyCount, bool& outIsAssembly,
                              QString& outMessage);
};

Q_DECLARE_METATYPE(SimHit)
Q_DECLARE_METATYPE(QVector<SimHit>)
Q_DECLARE_METATYPE(ShapeMapPoint)
Q_DECLARE_METATYPE(QVector<ShapeMapPoint>)
Q_DECLARE_METATYPE(ShapeMapLegendEntry)
Q_DECLARE_METATYPE(QVector<ShapeMapLegendEntry>)

#endif // HOOPS_AI_INDEX_H
