#include "HoopsAiIndex.h"

#include <QByteArray>
#include <QFileInfo>
#include <vector>

#ifdef USING_EXCHANGE
    #include "hoops_ai_bridge.h"
#endif

namespace {

#ifdef USING_EXCHANGE
/* Shared error-buffer size, matching the convention used elsewhere in this project. */
constexpr int kErrBufSize = 8192;

/* Registered folder-add progress callback and the C trampoline handed to the bridge. The bridge
   stores a plain function pointer + void* userData; we route userData==nullptr and keep the
   std::function in this translation unit. The bridge only ever invokes the trampoline while a
   folder add runs on the worker thread. */
HoopsAiIndex::ProgressCallback g_addFolderProgressCb;

extern "C" void addFolderProgressTrampoline(int phase, int done, int total, int errors, int heavy,
                                            void* /*userData*/) {
    if (g_addFolderProgressCb)
        g_addFolderProgressCb(phase, done, total, errors, heavy);
}
#endif

} // namespace

bool HoopsAiIndex::openIndex(const QString& faissFilePath, QString& outMessage)
{
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    // createIfMissing=true: a not-yet-existing .faiss file becomes a fresh index (a new
    // embeddings model must be loaded first so the bridge can derive the index dimension).
    bool const ok = HoopsAI_OpenIndex(faissFilePath.toUtf8().constData(), /*createIfMissing=*/true,
                                      errBuf, sizeof(errBuf));
    if (!ok)
        outMessage = QString::fromUtf8(errBuf);
    return ok;
#else
    Q_UNUSED(faissFilePath);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::setThumbnailDir(const QString& dir, QString& outMessage)
{
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    // An empty dir clears the override on the bridge (revert to the default per-index folder).
    bool const ok = HoopsAI_SetThumbnailDir(dir.isEmpty() ? "" : dir.toUtf8().constData(),
                                            errBuf, sizeof(errBuf));
    if (!ok)
        outMessage = QString::fromUtf8(errBuf);
    return ok;
#else
    Q_UNUSED(dir);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::addCad(const QString& cadPath, QString& outWarning)
{
    outWarning.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    int indexCount = 0;
    // partId is nullptr so the bridge uses the CAD file path itself as the id.
    bool const ok = HoopsAI_AddCADToIndex(cadPath.toUtf8().constData(), nullptr, &indexCount,
                                          errBuf, sizeof(errBuf));
    // On both success and failure the bridge may leave text in errBuf: on success it is a
    // non-fatal warning (e.g. thumbnail generation failed), on failure it is the error.
    outWarning = QString::fromUtf8(errBuf);
    return ok;
#else
    Q_UNUSED(cadPath);
    outWarning = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::addCadFolder(const QStringList& cadPaths, int numWorkers, int timeLimitSeconds,
                                int& outAdded, int& outFailed, QStringList& outFailedFiles,
                                QString& outWarning)
{
    outAdded = 0;
    outFailed = 0;
    outFailedFiles.clear();
    outWarning.clear();
#ifdef USING_EXCHANGE
    if (cadPaths.isEmpty())
        return true;

    // Keep the UTF-8 byte arrays alive for the duration of the call; the bridge takes an array
    // of const char* that must stay valid until HoopsAI_AddCADFolderToIndex returns.
    std::vector<QByteArray> utf8;
    utf8.reserve(cadPaths.size());
    std::vector<const char*> ptrs;
    ptrs.reserve(cadPaths.size());
    for (const QString& p : cadPaths) {
        utf8.push_back(p.toUtf8());
        ptrs.push_back(utf8.back().constData());
    }

    char errBuf[kErrBufSize] = {0};
    // Generous newline-joined buffer for the failed-path list (ids are full CAD paths).
    std::vector<char> failedBuf(262144, '\0');
    int added = 0;
    int failed = 0;
    int indexCount = 0;

    bool const ok = HoopsAI_AddCADFolderToIndex(ptrs.data(), static_cast<int>(ptrs.size()),
                                                numWorkers, timeLimitSeconds, &added, &failed,
                                                failedBuf.data(), static_cast<int>(failedBuf.size()),
                                                &indexCount, errBuf, sizeof(errBuf));
    outAdded = added;
    outFailed = failed;
    // On success errBuf may hold a non-fatal warning; on failure it holds the error.
    outWarning = QString::fromUtf8(errBuf);

    QString const failedJoined = QString::fromUtf8(failedBuf.data());
    outFailedFiles = failedJoined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return ok;
#else
    Q_UNUSED(cadPaths);
    Q_UNUSED(numWorkers);
    Q_UNUSED(timeLimitSeconds);
    outWarning = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

void HoopsAiIndex::setAddFolderProgressCallback(ProgressCallback callback)
{
#ifdef USING_EXCHANGE
    g_addFolderProgressCb = std::move(callback);
    // Register the trampoline only while a callback is set so the bridge suppresses tqdm (and the
    // stderr shim) again once we clear it.
    HoopsAI_SetProgressCallback(g_addFolderProgressCb ? &addFolderProgressTrampoline : nullptr,
                                nullptr);
#else
    Q_UNUSED(callback);
#endif
}

bool HoopsAiIndex::search(const QString& cadPath, int topK, QVector<SimHit>& outHits, QString& outMessage)
{
    outHits.clear();
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    // Same buffer sizing as samples/test_client.cpp: a generous newline-joined id buffer and a
    // parallel score array.
    std::vector<char> idsBuf(65536, '\0');
    std::vector<float> scores(1024, 0.0f);
    int const maxResults = static_cast<int>(scores.size());
    int resultCount = 0;

    if (!HoopsAI_SearchIndex(cadPath.toUtf8().constData(), topK,
                             idsBuf.data(), static_cast<int>(idsBuf.size()),
                             scores.data(), maxResults, &resultCount,
                             errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }

    // outIds is a single newline-delimited UTF-8 string; split it and zip with scores.
    QString const idsJoined = QString::fromUtf8(idsBuf.data());
    QStringList const ids = idsJoined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    int const count = qMin(ids.size(), resultCount);
    outHits.reserve(count);
    for (int i = 0; i < count; ++i) {
        SimHit hit;
        hit.id = ids[i];
        hit.score = (i < maxResults) ? scores[i] : 0.0f;
        hit.name = QFileInfo(hit.id).fileName();

        // Resolve the thumbnail path (cheap: no CAD access, no rendering).
        char thumbBuf[4096] = {0};
        bool exists = false;
        char thumbErr[kErrBufSize] = {0};
        if (HoopsAI_GetPartThumbnailPath(hit.id.toUtf8().constData(), thumbBuf, sizeof(thumbBuf),
                                         &exists, thumbErr, sizeof(thumbErr))) {
            hit.thumbnailPath = QString::fromUtf8(thumbBuf);
            hit.thumbnailExists = exists;
        }
        outHits.push_back(hit);
    }

    // Part search routes through CADSearch.search_by_shape (tutorial parity). When the query file is
    // itself registered in the index, that path can drop the query from its own candidate list, so
    // the exact self-match no longer ranks at 1.0. The UI expects the query in the list (a tag put
    // on the displayed cluster must also land on the query), so if the query is registered and not
    // already present, prepend it as a self-hit at 1.0 - mirroring searchAssembly below. A query
    // that is NOT registered (e.g. a demo file whose twin lives in the index under another path)
    // fails the lookup and is left out, so no duplicate self-hit is added.
    bool alreadyPresent = false;
    for (const SimHit& h : outHits) {
        if (h.id == cadPath) { alreadyPresent = true; break; }
    }
    if (!alreadyPresent) {
        int selfBodies = 0;
        bool selfIsAssembly = false;
        char bcErr[kErrBufSize] = {0};
        if (HoopsAI_GetPartBodyCount(cadPath.toUtf8().constData(), &selfBodies, &selfIsAssembly,
                                     bcErr, sizeof(bcErr))) {
            SimHit selfHit;
            selfHit.id = cadPath;
            selfHit.score = 1.0f;
            selfHit.name = QFileInfo(selfHit.id).fileName();

            char thumbBuf[4096] = {0};
            bool exists = false;
            char thumbErr[kErrBufSize] = {0};
            if (HoopsAI_GetPartThumbnailPath(selfHit.id.toUtf8().constData(), thumbBuf,
                                             sizeof(thumbBuf), &exists, thumbErr, sizeof(thumbErr))) {
                selfHit.thumbnailPath = QString::fromUtf8(thumbBuf);
                selfHit.thumbnailExists = exists;
            }
            outHits.prepend(selfHit);
        }
    }
    return true;
#else
    Q_UNUSED(cadPath);
    Q_UNUSED(topK);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::searchAssembly(const QString& cadPath, int topK, QVector<SimHit>& outHits,
                                  QString& outMessage)
{
    outHits.clear();
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    // Generous newline-joined id buffer (assembly ids are full CAD paths) plus parallel result
    // arrays. maxResults caps every out array; scores/geom/coverage/matched/candidate are optional
    // but we request them all so the buffers stay in step with resultCount.
    std::vector<char> idsBuf(65536, '\0');
    constexpr int maxResults = 1024;
    std::vector<float> scores(maxResults, 0.0f);
    std::vector<float> geomScores(maxResults, 0.0f);
    std::vector<float> coverages(maxResults, 0.0f);
    std::vector<int> matchedParts(maxResults, 0);
    std::vector<int> candidateParts(maxResults, 0);
    int resultCount = 0;

    // Tuning knobs: pass the bridge's own defaults (candidateK<=0 => 30, simThresh<=0 => 0.80,
    // bopWeight<0 => 0.30, coverageMode=nullptr => "symmetric") and enable TF-IDF rare-part
    // weighting so common hardware does not dominate the ranking.
    if (!HoopsAI_SearchSimilarAssembly(cadPath.toUtf8().constData(), topK,
                                       /*candidateK=*/0, /*simThresh=*/0.0f, /*bopWeight=*/-1.0f,
                                       /*coverageMode=*/nullptr, /*useIdf=*/true,
                                       idsBuf.data(), static_cast<int>(idsBuf.size()),
                                       scores.data(), geomScores.data(), coverages.data(),
                                       matchedParts.data(), candidateParts.data(),
                                       maxResults, &resultCount,
                                       errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }

    QString const idsJoined = QString::fromUtf8(idsBuf.data());
    QStringList const ids = idsJoined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    int const count = qMin(ids.size(), resultCount);
    outHits.reserve(count);
    for (int i = 0; i < count; ++i) {
        SimHit hit;
        hit.id = ids[i];
        // Final blended similarity (the value the UI's similarity slider filters on).
        hit.score = (i < maxResults) ? scores[i] : 0.0f;
        hit.name = QFileInfo(hit.id).fileName();

        char thumbBuf[4096] = {0};
        bool exists = false;
        char thumbErr[kErrBufSize] = {0};
        if (HoopsAI_GetPartThumbnailPath(hit.id.toUtf8().constData(), thumbBuf, sizeof(thumbBuf),
                                         &exists, thumbErr, sizeof(thumbErr))) {
            hit.thumbnailPath = QString::fromUtf8(thumbBuf);
            hit.thumbnailExists = exists;
        }
        outHits.push_back(hit);
    }

    // The assembly matcher deliberately drops the query from its candidate set
    // (assembly_matcher_py.h: `candidates.discard(query_path)`), so unlike part search the query
    // assembly never ranks itself at 1.0. That is correct for retrieval, but the UI needs the query
    // in the list so a tag assigned to the displayed cluster also lands on the query itself. If the
    // query is registered in the current index, prepend it as a self-hit at score 1.0.
    bool alreadyPresent = false;
    for (const SimHit& h : outHits) {
        if (h.id == cadPath) { alreadyPresent = true; break; }
    }
    if (!alreadyPresent) {
        int selfBodies = 0;
        bool selfIsAssembly = false;
        char bcErr[kErrBufSize] = {0};
        // Registered-in-index test: exact id lookup, identical semantics to the matcher's own
        // `query_path in self._asm_rows`. Only registered queries can be tagged, so a non-registered
        // query (searching with a model not yet added) is left out.
        if (HoopsAI_GetPartBodyCount(cadPath.toUtf8().constData(), &selfBodies, &selfIsAssembly,
                                     bcErr, sizeof(bcErr))) {
            SimHit selfHit;
            selfHit.id = cadPath;
            selfHit.score = 1.0f;
            selfHit.name = QFileInfo(selfHit.id).fileName();

            char thumbBuf[4096] = {0};
            bool exists = false;
            char thumbErr[kErrBufSize] = {0};
            if (HoopsAI_GetPartThumbnailPath(selfHit.id.toUtf8().constData(), thumbBuf,
                                             sizeof(thumbBuf), &exists, thumbErr, sizeof(thumbErr))) {
                selfHit.thumbnailPath = QString::fromUtf8(thumbBuf);
                selfHit.thumbnailExists = exists;
            }
            outHits.prepend(selfHit);
        }
    }
    return true;
#else
    Q_UNUSED(cadPath);
    Q_UNUSED(topK);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::computeShapeMap(int nClusters, int dims, const QStringList& onlyIds,
                                   QVector<ShapeMapPoint>& outPoints,
                                   int& outClusterCount, QString& outMessage)
{
    outPoints.clear();
    outClusterCount = 0;
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    // Generous newline-joined id buffer (ids are full CAD paths). maxPoints caps the parallel
    // coordinate/cluster arrays; size it well above any realistic index (grown if needed below).
    std::vector<char> idsBuf(1 << 21, '\0');   // 2 MiB
    constexpr int maxPoints = 100000;
    std::vector<float> coords(static_cast<size_t>(maxPoints) * 3, 0.0f);
    std::vector<int> clusters(maxPoints, 0);
    int pointCount = 0;
    int clusterCount = 0;

    // Optional subset filter: newline-join the ids to restrict the map to (empty => whole index).
    QByteArray const filterUtf8 = onlyIds.isEmpty() ? QByteArray() : onlyIds.join('\n').toUtf8();
    char const* filterArg = onlyIds.isEmpty() ? nullptr : filterUtf8.constData();

    if (!HoopsAI_ComputeIndexShapeMap(nClusters, dims, filterArg,
                                     idsBuf.data(), static_cast<int>(idsBuf.size()),
                                     coords.data(), clusters.data(),
                                     maxPoints, &pointCount, &clusterCount,
                                     errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }

    QString const idsJoined = QString::fromUtf8(idsBuf.data());
    QStringList const ids = idsJoined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    int const count = qMin(ids.size(), pointCount);
    outPoints.reserve(count);
    for (int i = 0; i < count; ++i) {
        ShapeMapPoint p;
        p.id = ids[i];
        p.x = coords[static_cast<size_t>(i) * 3 + 0];
        p.y = coords[static_cast<size_t>(i) * 3 + 1];
        p.z = coords[static_cast<size_t>(i) * 3 + 2];
        p.cluster = clusters[i];
        outPoints.push_back(p);
    }
    outClusterCount = clusterCount;
    return true;
#else
    Q_UNUSED(nClusters);
    Q_UNUSED(dims);
    Q_UNUSED(onlyIds);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::listParts(int limit, QVector<SimHit>& outHits, QString& outMessage)
{    outHits.clear();
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    // Generous newline-joined id buffer (ids are full CAD paths, so they can be long).
    std::vector<char> idsBuf(262144, '\0');
    int resultCount = 0;

    if (!HoopsAI_ListIndexParts(idsBuf.data(), static_cast<int>(idsBuf.size()),
                                limit, &resultCount, errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }

    QString const idsJoined = QString::fromUtf8(idsBuf.data());
    QStringList const ids = idsJoined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    outHits.reserve(ids.size());
    for (const QString& id : ids) {
        SimHit hit;
        hit.id = id;
        hit.score = 0.0f;
        hit.name = QFileInfo(id).fileName();

        char thumbBuf[4096] = {0};
        bool exists = false;
        char thumbErr[kErrBufSize] = {0};
        if (HoopsAI_GetPartThumbnailPath(id.toUtf8().constData(), thumbBuf, sizeof(thumbBuf),
                                         &exists, thumbErr, sizeof(thumbErr))) {
            hit.thumbnailPath = QString::fromUtf8(thumbBuf);
            hit.thumbnailExists = exists;
        }
        outHits.push_back(hit);
    }
    return true;
#else
    Q_UNUSED(limit);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::listPartsPaged(int offset, int count, QVector<SimHit>& outHits, int& outTotal,
                                  QString& outMessage)
{
    outHits.clear();
    outTotal = 0;
    outMessage.clear();
#ifdef USING_EXCHANGE
    if (count <= 0)
        return true;

    char errBuf[kErrBufSize] = {0};
    // Roomy per-part budget: ids are full CAD paths and thumbnails are absolute paths, so allow
    // ~1KB per entry for each of the two newline-joined buffers.
    std::vector<char> idsBuf(static_cast<size_t>(count) * 1024, '\0');
    std::vector<char> thumbsBuf(static_cast<size_t>(count) * 1024, '\0');
    std::vector<char> kindsBuf(static_cast<size_t>(count) * 16, '\0');
    std::vector<unsigned char> existsBuf(static_cast<size_t>(count), 0);
    int resultCount = 0;
    int totalCount = 0;

    if (!HoopsAI_ListIndexPartsPaged(offset, count,
                                     idsBuf.data(), static_cast<int>(idsBuf.size()),
                                     thumbsBuf.data(), static_cast<int>(thumbsBuf.size()),
                                     kindsBuf.data(), static_cast<int>(kindsBuf.size()),
                                     existsBuf.data(), count,
                                     &resultCount, &totalCount,
                                     errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }

    outTotal = totalCount;

    // ids, thumbs and kinds are positionally parallel newline-joined lists. A part with no derivable
    // thumbnail yields an empty segment, so keep empty parts and zip strictly by index up to
    // resultCount (the exists byte array is the authoritative per-part on-disk flag).
    QStringList const ids =
        QString::fromUtf8(idsBuf.data()).split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QStringList const thumbs =
        QString::fromUtf8(thumbsBuf.data()).split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QStringList const kinds =
        QString::fromUtf8(kindsBuf.data()).split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    outHits.reserve(resultCount);
    for (int i = 0; i < resultCount; ++i) {
        SimHit hit;
        hit.id = ids.value(i);
        hit.score = 0.0f;
        hit.name = QFileInfo(hit.id).fileName();
        hit.thumbnailPath = thumbs.value(i);
        hit.thumbnailExists = (i < static_cast<int>(existsBuf.size())) && existsBuf[i] != 0;
        // Present the stored "part"/"assembly" kind capitalized for the gallery's kind line.
        QString const k = kinds.value(i).trimmed();
        if (k.compare(QLatin1String("assembly"), Qt::CaseInsensitive) == 0)
            hit.kind = QStringLiteral("Assembly");
        else if (k.compare(QLatin1String("part"), Qt::CaseInsensitive) == 0)
            hit.kind = QStringLiteral("Part");
        outHits.push_back(hit);
    }
    return true;
#else
    Q_UNUSED(offset);
    Q_UNUSED(count);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::close(QString& outMessage)
{
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    bool const ok = HoopsAI_CloseIndex(errBuf, sizeof(errBuf));
    if (!ok)
        outMessage = QString::fromUtf8(errBuf);
    return ok;
#else
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

bool HoopsAiIndex::currentIndexInfo(IndexInfo& outInfo, QString& outMessage)
{
    outInfo = IndexInfo{};
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    char pathBuf[4096] = {0};
    bool hasIndex = false;
    int fileCount = 0, bodyCount = 0, assemblyCount = 0, singlePartCount = 0, dim = 0;
    if (!HoopsAI_GetIndexStats(pathBuf, sizeof(pathBuf), &hasIndex,
                               &fileCount, &bodyCount, &assemblyCount, &singlePartCount, &dim,
                               errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }
    outInfo.hasIndex = hasIndex;
    outInfo.faissPath = hasIndex ? QString::fromUtf8(pathBuf) : QString();
    outInfo.fileCount = fileCount;
    outInfo.bodyCount = bodyCount;
    outInfo.assemblyCount = assemblyCount;
    outInfo.singlePartCount = singlePartCount;
    outInfo.dimension = dim;
    return true;
#else
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}

QString HoopsAiIndex::currentIndexDir(QString& outMessage)
{
    IndexInfo info;
    if (!currentIndexInfo(info, outMessage))
        return QString();
    return info.faissPath;
}

bool HoopsAiIndex::partBodyCount(const QString& partId, int& outBodyCount, bool& outIsAssembly,
                                 QString& outMessage)
{
    outBodyCount = 0;
    outIsAssembly = false;
    outMessage.clear();
#ifdef USING_EXCHANGE
    char errBuf[kErrBufSize] = {0};
    int bodies = 0;
    bool isAssembly = false;
    if (!HoopsAI_GetPartBodyCount(partId.toUtf8().constData(), &bodies, &isAssembly,
                                  errBuf, sizeof(errBuf))) {
        outMessage = QString::fromUtf8(errBuf);
        return false;
    }
    outBodyCount = bodies;
    outIsAssembly = isAssembly;
    return true;
#else
    Q_UNUSED(partId);
    outMessage = QStringLiteral("HOOPS AI is not available in this build.");
    return false;
#endif
}
