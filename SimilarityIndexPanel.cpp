#include "SimilarityIndexPanel.h"

#include "HPSWidget.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QItemSelectionModel>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QProgressDialog>
#include <QProgressBar>
#include <QPushButton>
#include <QRunnable>
#include <QSettings>
#include <QSet>
#include <QSlider>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QThread>
#include <QThreadPool>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QImage>

#include <vector>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

// Number of physical CPU cores, used as the default pass-1 worker count for the folder add.
// Benchmarks (see hoops-ai-embeddings-benchmark: make_report_full / plan.md / README_heavy) put the
// index-embedding sweet spot near the PHYSICAL-core count; pushing to the logical count only adds
// RAM pressure for little gain. Returns 0 when it cannot be determined so the caller can fall back
// to QThread::idealThreadCount().
int detectPhysicalCores()
{
#ifdef Q_OS_WIN
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len == 0)
        return 0;
    std::vector<char> buf(len);
    auto* first = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, first, &len))
        return 0;
    int cores = 0;
    char* ptr = buf.data();
    char* const end = buf.data() + len;
    while (ptr < end) {
        auto* cur = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(ptr);
        if (cur->Relationship == RelationProcessorCore)
            ++cores;
        if (cur->Size == 0)
            break;
        ptr += cur->Size;
    }
    return cores;
#else
    return 0;
#endif
}

// Bytes of currently-available physical RAM, used to size the default pass-2 worker count. Pass 2
// re-embeds the memory-heaviest assemblies, so each worker needs enough headroom (hoops_ai's own
// RAM fallback assumes several GB per heavy part); the default is floor(freeRAM / 4GB). Returns 0
// when it cannot be determined so the caller can fall back to a safe minimum.
quint64 detectAvailablePhysMemoryBytes()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms))
        return 0;
    return static_cast<quint64>(ms.ullAvailPhys);
#else
    return 0;
#endif
}

// Format a byte count as a compact human-readable size (e.g. "1.2 MB", "834 KB") for the report.
QString formatSize(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("?");
    double v = static_cast<double>(bytes);
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    return (u == 0) ? QStringLiteral("%1 B").arg(bytes)
                    : QStringLiteral("%1 %2").arg(v, 0, 'f', 1).arg(QLatin1String(units[u]));
}
// dialog) and the paths hoops_ai writes into its CWD logs (which may use forward OR back slashes,
// and differ in case on Windows). Collapse to forward slashes and lower-case on Windows.
QString normPathKey(const QString& p)
{
    QString s = p;
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
#ifdef Q_OS_WIN
    s = s.toLower();
#endif
    return s;
}

// hoops_ai writes error_summary.json into the PROCESS CWD (the same CWD as this Qt process, since
// the bridge runs hoops_ai in-process) and OVERWRITES it on every embed_shape_batch call. Parse
// the freshest one into normalizedPath -> reason. The reason's first line is kept; a cumulative
// timeout carries "Timeout" so the caller can distinguish a timed-out heavy assembly from a
// genuine CAD error. Returns an empty map when the file is absent/unreadable.
QMap<QString, QString> readErrorSummaryReasons()
{
    QMap<QString, QString> reasons;
    QString const path = QDir(QDir::currentPath()).filePath(QStringLiteral("error_summary.json"));
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return reasons;
    QJsonParseError perr{};
    QJsonDocument const doc = QJsonDocument::fromJson(file.readAll(), &perr);
    file.close();
    if (perr.error != QJsonParseError::NoError || !doc.isArray())
        return reasons;
    for (const QJsonValue& v : doc.array()) {
        if (!v.isObject())
            continue;
        QJsonObject const o = v.toObject();
        QString const item = o.value(QStringLiteral("item")).toString();
        QString error = o.value(QStringLiteral("error")).toString();
        if (item.isEmpty())
            continue;
        error = error.section(QLatin1Char('\n'), 0, 0).trimmed();
        reasons.insert(normPathKey(item), error);
    }
    return reasons;
}

// hoops_ai writes too_heavy_files.log into the process CWD listing the files its parallel executor
// deferred to the built-in single-worker RAM fallback ("Heavy Files (1 worker)"). Format: '#'
// comment/header lines, a blank line, then one forward-slash path per line. Returns the paths
// (original text, not normalized) or an empty list when absent.
QStringList readTooHeavyFiles()
{
    QStringList heavy;
    QString const path = QDir(QDir::currentPath()).filePath(QStringLiteral("too_heavy_files.log"));
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return heavy;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString const line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        heavy << line;
    }
    file.close();
    return heavy;
}

// Format an elapsed duration in seconds as "Hh MMm SSs" (dropping leading zero units) plus the raw
// seconds, e.g. "2m 08s (128.4s)" or "12.3s", for the human-readable report.
QString formatElapsed(double secs)
{
    if (secs < 0)
        secs = 0;
    qint64 const total = static_cast<qint64>(secs + 0.5);
    qint64 const h = total / 3600;
    qint64 const m = (total % 3600) / 60;
    qint64 const s = total % 60;
    QString hms;
    if (h > 0)
        hms = QStringLiteral("%1h %2m %3s").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    else if (m > 0)
        hms = QStringLiteral("%1m %2s").arg(m).arg(s, 2, 10, QLatin1Char('0'));
    else
        hms = QStringLiteral("%1s").arg(s);
    return QStringLiteral("%1 (%2s)").arg(hms).arg(secs, 0, 'f', 1);
}

// Write a human-readable 3-group report of a two-pass folder add next to the current index
// (<index folder>/add_folder_report_<timestamp>.txt) and return its path (empty when it cannot be
// written). The groups are: files embedded in pass 1 (light), files recovered in pass 2 (heavy),
// and files still failing after the retry (permanent) -- exactly the classification the operator
// needs to see which parts made it into the index and which did not.
QString writeAddFolderReport(const QStringList& lightAdded,
                             const QStringList& heavyAdded,
                             const QStringList& permanentFailed,
                             const QMap<QString, QString>& failReasons,
                             const QStringList& heavyFlagged,
                             int pass1Workers, int pass1TimeLimit,
                             int pass2Workers, int pass2TimeLimit,
                             double pass1Secs, double pass2Secs, double totalSecs)
{
    QString msg;
    QString const faiss = HoopsAiIndex::currentIndexDir(msg);
    QString dir = faiss.isEmpty() ? QString() : QFileInfo(faiss).absolutePath();
    if (dir.isEmpty())
        dir = QDir::tempPath();

    QString const stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString const path = QDir(dir).filePath(QStringLiteral("add_folder_report_%1.txt").arg(stamp));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return QString();

    QTextStream out(&file);
    int const totalIn = lightAdded.size() + heavyAdded.size() + permanentFailed.size();
    out << "# Add Folder report - "
        << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    if (!faiss.isEmpty())
        out << "# Index: " << faiss << '\n';
    out << "# Pass 1 (light): num_workers=" << pass1Workers
        << " time_limit=" << pass1TimeLimit << "s\n";
    out << "# Pass 2 (heavy): num_workers=" << pass2Workers
        << " time_limit=" << pass2TimeLimit << "s\n";
    out << "# Elapsed: total=" << formatElapsed(totalSecs)
        << " pass1=" << formatElapsed(pass1Secs)
        << " pass2=" << (pass2Secs > 0 ? formatElapsed(pass2Secs) : QStringLiteral("- (skipped)"))
        << '\n';
    out << "# Totals: input=" << totalIn
        << " light_added=" << lightAdded.size()
        << " heavy_recovered=" << heavyAdded.size()
        << " failed=" << permanentFailed.size();
    // Break the still-failed group down into timed-out (recoverable with a bigger budget) vs.
    // genuine CAD errors (unrecoverable), using the reason parsed from error_summary.json.
    int timeoutFails = 0, errorFails = 0;
    for (const QString& f : permanentFailed) {
        QString const reason = failReasons.value(normPathKey(f));
        if (reason.contains(QStringLiteral("Timeout"), Qt::CaseInsensitive))
            ++timeoutFails;
        else if (!reason.isEmpty())
            ++errorFails;
    }
    out << " (timeout=" << timeoutFails << " error=" << errorFails
        << " other=" << (permanentFailed.size() - timeoutFails - errorFails) << ")\n\n";

    auto section = [&out](const QString& title, const QStringList& files) {
        out << "[" << title << "] (" << files.size() << ")\n";
        for (const QString& f : files)
            out << f << "  [" << formatSize(QFileInfo(f).size()) << "]\n";
        out << '\n';
    };
    section(QStringLiteral("LIGHT - added in pass 1"), lightAdded);
    section(QStringLiteral("HEAVY - recovered in pass 2"), heavyAdded);

    // The failed group is annotated with each file's reason so the operator can tell a heavy
    // assembly that timed out (raise the pass-2 budget) from a file hoops_ai genuinely could not
    // process (a real CAD/geometry error, unrecoverable by retrying).
    out << "[FAILED - not indexed after retry] (" << permanentFailed.size() << ")\n";
    for (const QString& f : permanentFailed) {
        QString const reason = failReasons.value(normPathKey(f));
        out << f << "  [" << formatSize(QFileInfo(f).size()) << "]";
        if (!reason.isEmpty())
            out << "  <= " << reason;
        out << '\n';
    }
    out << '\n';

    // Files hoops_ai's parallel executor deferred to its built-in single-worker RAM fallback
    // ("Heavy Files (1 worker)"). Informational: these are the memory-heaviest parts and are a
    // good hint for tuning pass-2 workers/timeout. Distinct from our timeout-driven pass 2.
    if (!heavyFlagged.isEmpty())
        section(QStringLiteral("HEAVY-FLAGGED (RAM fallback, 1 worker)"), heavyFlagged);

    out.flush();
    file.close();
    return path;
}

// Show a bridge error the same way HPSWidget::ShowLoadError does: a concise summary in the box and
// the full Python traceback pushed into the collapsible "Show Details…" area.
void showBridgeError(QWidget* parent, const QString& title, const QString& fullMessage)
{
    QString summary = fullMessage;
    int const nl = summary.indexOf(QLatin1Char('\n'));
    if (nl >= 0)
        summary = summary.left(nl);
    int const tb = summary.indexOf(QStringLiteral("Traceback"));
    if (tb >= 0)
        summary = summary.left(tb).trimmed();
    if (summary.isEmpty())
        summary = QObject::tr("The operation failed.");

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(title);
    box.setText(summary);
    if (fullMessage.trimmed() != summary)
        box.setDetailedText(fullMessage);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

// Build a neutral grey placeholder pixmap carrying the file name, used when a hit has no thumbnail
// on disk. Must run on the GUI thread (QPixmap).
QPixmap makePlaceholder(const QSize& size, const QString& text)
{
    QPixmap pm(size);
    pm.fill(QColor(0xB0, 0xB0, 0xB0));
    QPainter p(&pm);
    p.setPen(QColor(0x30, 0x30, 0x30));
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    p.drawText(pm.rect().adjusted(4, 4, -4, -4),
               Qt::AlignCenter | Qt::TextWordWrap, text);
    p.end();
    return pm;
}

// Off-GUI-thread thumbnail decoder. Loads and scales one PNG, then posts the result back to the
// model on the GUI thread (queued) tagged with the generation it was requested in, so the model can
// drop it if the row set changed meanwhile. Auto-deletes after run().
class ThumbnailLoadTask : public QRunnable {
  public:
    ThumbnailLoadTask(QPointer<IndexGalleryModel> model, int row, const QString& path,
                      const QSize& size, int generation)
        : m_model(model), m_row(row), m_path(path), m_size(size), m_generation(generation)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QImage img(m_path);
        if (!img.isNull())
            img = img.scaled(m_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (IndexGalleryModel* model = m_model) {
            QMetaObject::invokeMethod(model, "onThumbnailReady", Qt::QueuedConnection,
                                      Q_ARG(int, m_row), Q_ARG(QString, m_path),
                                      Q_ARG(QImage, img), Q_ARG(int, m_generation));
        }
    }

  private:
    QPointer<IndexGalleryModel> m_model;
    int     m_row;
    QString m_path;
    QSize   m_size;
    int     m_generation;
};

constexpr int kThumbW = 160;
constexpr int kThumbH = 120;
constexpr int kTopK = 300;       // Hits fetched (ON) into a pool the similarity slider filters.
constexpr int kListLimit = 0;    // Index listing (OFF): 0 => list every part (paged under the hood).
// Slider works in 1/1000 units so a single step maps to 0.002 similarity (step = kThreshStep).
// The minimum is mode-dependent (see refreshResults): assembly-to-assembly scores are blended and
// run low, so assembly search needs a low floor to surface any hits; part scores are also geometry-
// blended and become non-discriminative below ~0.6 (thousands of parts cluster there, beyond the
// fetched pool), so part search uses a higher floor to keep "shown == all above threshold" honest.
constexpr int kThreshMin = 400;   // Assembly slider min => 0.400 similarity (low blended scores).
constexpr int kPartThreshMin = 600; // Part slider min => 0.600 similarity (discriminative range only).
constexpr int kThreshMax = 1000;  // Slider max => 1.000 similarity.
constexpr int kThreshDefault = 750; // Default => 0.750 similarity.
constexpr int kThreshStep = 2;    // One tick => 0.002 similarity.
constexpr double kThreshScale = 1000.0;

// Wildcard filters (case-insensitive on Windows) for the CAD formats the Exchange bridge can import.
// Linux (and most Unix) filesystems are case-sensitive, so a plain "*.sldprt" name filter would
// silently skip files saved with an upper-case extension (e.g. "part.SLDPRT", common with
// SolidWorks/CAD exports). Turn every letter into a [aA]-style bracket class so QDirIterator's
// glob matching finds the file regardless of case, on every platform.
QString caseInsensitiveGlob(const QString& pattern)
{
    QString out;
    out.reserve(pattern.size() * 2);
    for (QChar c : pattern) {
        if (c.isLetter()) {
            out += QLatin1Char('[');
            out += c.toLower();
            out += c.toUpper();
            out += QLatin1Char(']');
        } else {
            out += c;
        }
    }
    return out;
}

// Mirrors the "All Supported Files" list used by HPSWidget's open dialog.
QStringList cadNameFilters()
{
    static const QStringList kExtensions{
        QStringLiteral("*.3ds"),   QStringLiteral("*.3mf"),   QStringLiteral("*.3dxml"),
        QStringLiteral("*.sat"),   QStringLiteral("*.sab"),   QStringLiteral("*.dgn"),
        QStringLiteral("*.dwg"),   QStringLiteral("*.dxf"),   QStringLiteral("*.dwf"),
        QStringLiteral("*.dwfx"),  QStringLiteral("*.model"), QStringLiteral("*.dlv"),
        QStringLiteral("*.exp"),   QStringLiteral("*.session"),
        QStringLiteral("*.CATPart"), QStringLiteral("*.CATProduct"), QStringLiteral("*.CATShape"),
        QStringLiteral("*.CATDrawing"),
        QStringLiteral("*.cgr"),   QStringLiteral("*.dae"),   QStringLiteral("*.prt"),
        QStringLiteral("*.neu"),   QStringLiteral("*.asm"),   QStringLiteral("*.xas"),
        QStringLiteral("*.xpr"),   QStringLiteral("*.fbx"),   QStringLiteral("*.gltf"),
        QStringLiteral("*.glb"),   QStringLiteral("*.arc"),   QStringLiteral("*.unv"),
        QStringLiteral("*.mf1"),   QStringLiteral("*.pkg"),   QStringLiteral("*.ifc"),
        QStringLiteral("*.ifczip"),QStringLiteral("*.igs"),   QStringLiteral("*.iges"),
        QStringLiteral("*.ipt"),   QStringLiteral("*.iam"),   QStringLiteral("*.jt"),
        QStringLiteral("*.kmz"),   QStringLiteral("*.nwd"),   QStringLiteral("*.prc"),
        QStringLiteral("*.x_t"),   QStringLiteral("*.xmt"),   QStringLiteral("*.x_b"),
        QStringLiteral("*.xmt_txt"), QStringLiteral("*.rvt"), QStringLiteral("*.3dm"),
        QStringLiteral("*.stp"),   QStringLiteral("*.step"),  QStringLiteral("*.stpz"),
        QStringLiteral("*.stl"),   QStringLiteral("*.par"),   QStringLiteral("*.pwd"),
        QStringLiteral("*.psm"),   QStringLiteral("*.sldprt"),QStringLiteral("*.sldasm"),
        QStringLiteral("*.sldfpp"),QStringLiteral("*.u3d"),   QStringLiteral("*.vda"),
        QStringLiteral("*.wrl"),   QStringLiteral("*.vrml"),  QStringLiteral("*.obj"),
        QStringLiteral("*.xv3"),   QStringLiteral("*.xv0"),   QStringLiteral("*.hsf")};

    QStringList filters;
    filters.reserve(kExtensions.size());
    for (const QString& ext : kExtensions)
        filters << caseInsensitiveGlob(ext);
    return filters;
}

// Recursively collect the paths of all CAD files under folder, sorted for stable ordering.
QStringList collectCadFiles(const QString& folder)
{
    QStringList paths;
    QDirIterator it(folder, cadNameFilters(), QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
        paths << it.next();
    paths.sort(Qt::CaseInsensitive);
    return paths;
}

} // namespace

// ------------------------------------------------------------------------- IndexGalleryModel

IndexGalleryModel::IndexGalleryModel(QObject* parent) : QAbstractListModel(parent)
{
    // A generous pixmap cache keeps recently seen thumbnails resident so scrolling back over the
    // gallery does not re-decode them (default is only ~10MB). 128MB holds ~1700 160x120 thumbs.
    if (QPixmapCache::cacheLimit() < 131072)
        QPixmapCache::setCacheLimit(131072);
}

void IndexGalleryModel::setRows(const QVector<SimHit>& rows, bool showScore)
{
    beginResetModel();
    m_rows = rows;
    m_showScore = showScore;
    ++m_generation;   // Invalidate any in-flight decodes belonging to the previous row set.
    m_inFlight.clear();
    endResetModel();
}

void IndexGalleryModel::clearRows()
{
    beginResetModel();
    m_rows.clear();
    ++m_generation;
    m_inFlight.clear();
    endResetModel();
}

QString IndexGalleryModel::idAt(int row) const
{
    return (row >= 0 && row < m_rows.size()) ? m_rows[row].id : QString();
}

int IndexGalleryModel::rowForId(const QString& id) const
{
    if (id.isEmpty())
        return -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].id == id)
            return i;
    }
    return -1;
}

int IndexGalleryModel::rowForPath(const QString& path) const
{
    if (path.isEmpty())
        return -1;
    QString const key = normPathKey(path);
    for (int i = 0; i < m_rows.size(); ++i) {
        if (normPathKey(m_rows[i].id) == key)
            return i;
    }
    return -1;
}

int IndexGalleryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant IndexGalleryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    const SimHit& h = m_rows[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return h.name;
    case IndexGalleryModel::ScoreRole:
        return h.score;
    case IndexGalleryModel::ShowScoreRole:
        return m_showScore;
    case IndexGalleryModel::KindRole:
        return h.kind;
    case Qt::ToolTipRole:
    case Qt::UserRole:
        return h.id;
    case Qt::TextAlignmentRole:
        return static_cast<int>(Qt::AlignHCenter | Qt::AlignTop);
    case Qt::DecorationRole: {
        if (h.thumbnailExists && !h.thumbnailPath.isEmpty()) {
            QPixmap pm;
            if (QPixmapCache::find(h.thumbnailPath, &pm))
                return QIcon(pm);
            requestThumbnail(index.row(), h.thumbnailPath);  // Decode off-thread; show a placeholder until ready.
        }
        return QIcon(makePlaceholder(m_thumbSize, h.name));
    }
    default:
        return QVariant();
    }
}

void IndexGalleryModel::requestThumbnail(int row, const QString& path) const
{
    if (path.isEmpty() || m_inFlight.contains(path))
        return;  // Already decoding this thumbnail; avoid duplicate tasks.
    m_inFlight.insert(path);
    auto* task = new ThumbnailLoadTask(const_cast<IndexGalleryModel*>(this), row, path,
                                       m_thumbSize, m_generation);
    QThreadPool::globalInstance()->start(task);
}

void IndexGalleryModel::onThumbnailReady(int row, const QString& path, const QImage& image,
                                         int generation)
{
    m_inFlight.remove(path);
    if (generation != m_generation)
        return;  // The row set changed after this decode was queued; drop the stale image.
    if (!image.isNull())
        QPixmapCache::insert(path, QPixmap::fromImage(image));
    // Refresh the row that requested it if it still shows this thumbnail.
    if (row >= 0 && row < m_rows.size() && m_rows[row].thumbnailPath == path) {
        QModelIndex const idx = index(row);
        emit dataChanged(idx, idx, QVector<int>{Qt::DecorationRole});
    }
}

// ----------------------------------------------------------------------- GalleryItemDelegate

/* Draws each gallery cell with the text ABOVE the thumbnail (matching the tutorial's plot layout):
   line 1 = "#<rank>  <score>" (search results only), line 2 = file name, then the thumbnail below.
   The default QListView icon-mode layout paints text under the icon, so a custom delegate is used. */
class GalleryItemDelegate : public QStyledItemDelegate {
  public:
    explicit GalleryItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QFontMetrics const fm(option.font);
        int const textBlockH = fm.height() * lineCount(index) + 4;
        return QSize(kThumbW + 8, kPad + textBlockH + kThumbH + kPad);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();

        const QRect r = option.rect;

        // Paint selection / hover background across the whole cell ourselves. Delegating this to
        // the style's CE_ItemViewItem (with text/icon cleared) collapses the highlight rect to the
        // empty text region, which both hid the highlight and squeezed the drawn text.
        const bool selected = option.state & QStyle::State_Selected;
        if (selected) {
            painter->fillRect(r, option.palette.brush(QPalette::Highlight));
        } else if (option.state & QStyle::State_MouseOver) {
            QColor hover = option.palette.color(QPalette::Highlight);
            hover.setAlpha(48);
            painter->fillRect(r, hover);
        }

        QFontMetrics const fm(option.font);
        int const lineH = fm.height();

        // Text stack, top to bottom: "#<rank>  <score>" (search only), "Part"/"Assembly" (when
        // known), file name.
        QStringList lines;
        if (index.data(IndexGalleryModel::ShowScoreRole).toBool()) {
            int const rank = index.row() + 1;
            double const score = index.data(IndexGalleryModel::ScoreRole).toDouble();
            lines << QStringLiteral("#%1  %2").arg(rank).arg(score, 0, 'f', 4);
        }
        QString const kind = index.data(IndexGalleryModel::KindRole).toString();
        if (!kind.isEmpty())
            lines << kind;
        lines << index.data(Qt::DisplayRole).toString();  // file name

        painter->setPen(option.palette.color(selected ? QPalette::HighlightedText
                                                       : QPalette::Text));

        QRect textRect(r.left() + kPad, r.top() + kPad, r.width() - 2 * kPad, lineH);
        for (const QString& line : lines) {
            painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter,
                              fm.elidedText(line, Qt::ElideRight, textRect.width()));
            textRect.moveTop(textRect.top() + lineH);
        }

        int const iconTop = r.top() + kPad + lineH * lines.size() + 4;
        QIcon const icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            QPixmap const pm = icon.pixmap(QSize(kThumbW, kThumbH));
            qreal const dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
            int const pw = int(pm.width() / dpr);
            int const ph = int(pm.height() / dpr);
            int const ix = r.left() + (r.width() - pw) / 2;
            int const iy = iconTop + (kThumbH - ph) / 2;
            painter->drawPixmap(QPoint(ix, iy), pm);
        }

        painter->restore();
    }

  private:
    static constexpr int kPad = 4;

    // Number of text lines a cell will draw (kept in sync between sizeHint and paint).
    static int lineCount(const QModelIndex& index)
    {
        int n = 1;  // file name
        if (index.data(IndexGalleryModel::ShowScoreRole).toBool())
            ++n;
        if (!index.data(IndexGalleryModel::KindRole).toString().isEmpty())
            ++n;
        return n;
    }
};

// ------------------------------------------------------------------------------- MetaStore

namespace {
// The tag sidecar file for an index is <stem>.json next to <stem>.faiss (same folder + stem).
QString tagJsonPathFor(const QString& faissPath)
{
    QFileInfo const fi(faissPath);
    return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".json");
}
// The "All parts" combobox sentinel is stored as item data so it is language-independent.
const QString kAllTagsData = QStringLiteral("__ALL__");
// The "Tagged" sentinel: selecting it shows only parts carrying at least one tag.
const QString kTaggedData = QStringLiteral("__TAGGED__");
// Sentinels for the "Part" / "Assembly" kind filters (matched against SimHit::kind).
const QString kPartData = QStringLiteral("__PART__");
const QString kAssemblyData = QStringLiteral("__ASSEMBLY__");
} // namespace

bool MetaStore::load(const QString& faissPath, QString* outError)
{
    m_partTags.clear();
    m_partFile.clear();
    m_jsonPath = tagJsonPathFor(faissPath);

    QFile f(m_jsonPath);
    if (!f.exists())
        return true; // No metadata yet is perfectly normal.
    if (!f.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QObject::tr("Cannot open metadata file: %1").arg(m_jsonPath);
        return false;
    }
    QByteArray const data = f.readAll();
    f.close();

    QJsonParseError perr{};
    QJsonDocument const doc = QJsonDocument::fromJson(data, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) *outError = QObject::tr("Metadata file is not valid JSON: %1").arg(perr.errorString());
        return false;
    }
    // Current schema: { "version": 1, "parts": { "<id>": { "tags": [...],
    //                   "file": { "mtime": <sec>, "size": <bytes> } }, ... } }
    // Legacy schema (still read): "<id>" maps directly to a tag ARRAY with no file signature.
    QJsonObject const parts = doc.object().value(QStringLiteral("parts")).toObject();
    for (auto it = parts.begin(); it != parts.end(); ++it) {
        QSet<QString> tags;
        QJsonArray tagArr;
        if (it.value().isArray()) {
            tagArr = it.value().toArray();                       // legacy: value is the tag array.
        } else if (it.value().isObject()) {
            QJsonObject const o = it.value().toObject();
            tagArr = o.value(QStringLiteral("tags")).toArray();
            QJsonObject const fo = o.value(QStringLiteral("file")).toObject();
            if (!fo.isEmpty()) {
                FileSig sig;
                sig.mtime = static_cast<qint64>(fo.value(QStringLiteral("mtime")).toDouble(0));
                sig.size = static_cast<qint64>(fo.value(QStringLiteral("size")).toDouble(-1));
                if (sig.valid())
                    m_partFile.insert(it.key(), sig);
            }
        }
        for (const QJsonValue& v : tagArr) {
            QString const t = v.toString().trimmed();
            if (!t.isEmpty())
                tags.insert(t);
        }
        if (!tags.isEmpty())
            m_partTags.insert(it.key(), tags);
    }
    return true;
}

bool MetaStore::save(QString* outError) const
{
    if (m_jsonPath.isEmpty()) {
        if (outError) *outError = QObject::tr("No index is open.");
        return false;
    }
    // Union of all part ids that carry tags and/or a file signature.
    QSet<QString> ids;
    for (auto it = m_partTags.begin(); it != m_partTags.end(); ++it)
        if (!it.value().isEmpty())
            ids.insert(it.key());
    for (auto it = m_partFile.begin(); it != m_partFile.end(); ++it)
        if (it.value().valid())
            ids.insert(it.key());

    QJsonObject parts;
    for (const QString& id : ids) {
        QJsonObject entry;
        auto const tIt = m_partTags.constFind(id);
        if (tIt != m_partTags.constEnd() && !tIt.value().isEmpty()) {
            QStringList tags = tIt.value().values();
            tags.sort(Qt::CaseInsensitive);
            entry.insert(QStringLiteral("tags"), QJsonArray::fromStringList(tags));
        }
        auto const fIt = m_partFile.constFind(id);
        if (fIt != m_partFile.constEnd() && fIt.value().valid()) {
            QJsonObject fo;
            fo.insert(QStringLiteral("mtime"), static_cast<double>(fIt.value().mtime));
            fo.insert(QStringLiteral("size"), static_cast<double>(fIt.value().size));
            entry.insert(QStringLiteral("file"), fo);
        }
        parts.insert(id, entry);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("parts"), parts);

    QFile f(m_jsonPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outError) *outError = QObject::tr("Cannot write metadata file: %1").arg(m_jsonPath);
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

void MetaStore::clear()
{
    m_partTags.clear();
    m_partFile.clear();
    m_jsonPath.clear();
}

QStringList MetaStore::allTags() const
{
    QSet<QString> all;
    for (auto it = m_partTags.begin(); it != m_partTags.end(); ++it)
        all.unite(it.value());
    QStringList list = all.values();
    list.sort(Qt::CaseInsensitive);
    return list;
}

QStringList MetaStore::tagsForPart(const QString& partId) const
{
    auto const it = m_partTags.constFind(partId);
    if (it == m_partTags.constEnd())
        return {};
    QStringList list = it.value().values();
    list.sort(Qt::CaseInsensitive);
    return list;
}

QStringList MetaStore::partsForTag(const QString& tag) const
{
    QStringList list;
    for (auto it = m_partTags.begin(); it != m_partTags.end(); ++it) {
        if (it.value().contains(tag))
            list << it.key();
    }
    return list;
}

bool MetaStore::partHasTag(const QString& partId, const QString& tag) const
{
    auto const it = m_partTags.constFind(partId);
    return it != m_partTags.constEnd() && it.value().contains(tag);
}

void MetaStore::assignTagToParts(const QString& tag, const QStringList& parts)
{
    QString const t = tag.trimmed();
    if (t.isEmpty())
        return;
    for (const QString& p : parts) {
        if (p.isEmpty())
            continue;
        m_partTags[p].insert(t);
    }
}

void MetaStore::setTagsForPart(const QString& partId, const QStringList& tags)
{
    QString const p = partId.trimmed();
    if (p.isEmpty())
        return;
    QSet<QString> set;
    for (const QString& t : tags) {
        QString const tt = t.trimmed();
        if (!tt.isEmpty())
            set.insert(tt);
    }
    if (set.isEmpty())
        m_partTags.remove(p);
    else
        m_partTags[p] = set;
}

void MetaStore::removeTag(const QString& tag)
{
    // Remove the tag from every part; drop parts that end up with no tags.
    for (auto it = m_partTags.begin(); it != m_partTags.end(); ) {
        it.value().remove(tag);
        if (it.value().isEmpty())
            it = m_partTags.erase(it);
        else
            ++it;
    }
}

MetaStore::FileSig MetaStore::fileSignature(const QString& partId) const
{
    auto const it = m_partFile.constFind(partId);
    return it == m_partFile.constEnd() ? FileSig{} : it.value();
}

bool MetaStore::fileUnchanged(const QString& partId, qint64 mtime, qint64 size) const
{
    auto const it = m_partFile.constFind(partId);
    if (it == m_partFile.constEnd() || !it.value().valid())
        return false;
    return it.value().mtime == mtime && it.value().size == size;
}

void MetaStore::setFileSignature(const QString& partId, qint64 mtime, qint64 size)
{
    QString const p = partId.trimmed();
    if (p.isEmpty() || size < 0)
        return;
    FileSig sig;
    sig.mtime = mtime;
    sig.size = size;
    m_partFile[p] = sig;
}



// ----------------------------------------------------------------------------- IndexWorker

void IndexWorker::doAdd(const QString& cadPath)
{
    QString message;
    bool const ok = HoopsAiIndex::addCad(cadPath, message);
    emit addFinished(ok, message);
}

void IndexWorker::doSearch(const QString& cadPath, int topK)
{
    QVector<SimHit> hits;
    QString message;
    QElapsedTimer timer;
    timer.start();
    bool const ok = HoopsAiIndex::search(cadPath, topK, hits, message);
    qint64 const elapsedMs = timer.elapsed();
    emit searchFinished(ok, hits, message, elapsedMs);
}

void IndexWorker::doSearchAssembly(const QString& cadPath, int topK)
{
    QVector<SimHit> hits;
    QString message;
    QElapsedTimer timer;
    timer.start();
    bool const ok = HoopsAiIndex::searchAssembly(cadPath, topK, hits, message);
    qint64 const elapsedMs = timer.elapsed();
    emit searchFinished(ok, hits, message, elapsedMs);
}

void IndexWorker::doComputeShapeMap(int nClusters, int dims, const QStringList& onlyIds)
{
    QVector<ShapeMapPoint> points;
    int clusterCount = 0;
    QString message;
    bool const ok = HoopsAiIndex::computeShapeMap(nClusters, dims, onlyIds, points, clusterCount, message);
    emit shapeMapComputed(ok, points, clusterCount, message);
}

void IndexWorker::doListParts(int limit)
{
    // Page through the whole index (or up to an optional positive cap) so large indexes are
    // fetched in bounded chunks instead of one huge buffer, then hand the full listing back.
    QVector<SimHit> all;
    QString message;
    constexpr int kPageSize = 2000;
    int offset = 0;
    int total = 0;
    bool ok = true;
    for (;;) {
        QVector<SimHit> page;
        int pageTotal = 0;
        ok = HoopsAiIndex::listPartsPaged(offset, kPageSize, page, pageTotal, message);
        if (!ok)
            break;
        total = pageTotal;
        all += page;
        offset += page.size();
        if (limit > 0 && all.size() >= limit) {
            if (all.size() > limit)
                all.resize(limit);
            break;
        }
        // Stop when the whole index is consumed or a short page signals the end.
        if (page.isEmpty() || offset >= total)
            break;
    }
    emit listPartsFinished(ok, all, message);
}

void IndexWorker::doAddFolder(const QStringList& cadPaths, int pass1Workers, int pass1TimeLimit,
                              int pass2Workers, int pass2TimeLimit)
{
    int const total = cadPaths.size();

    // Honor a cancel requested before the batch starts (each bulk embed call itself cannot be
    // interrupted mid-flight, so the gaps between passes are the only cancellation points).
    if (m_cancel.loadRelaxed()) {
        emit addFolderFinished(0, 0, /*canceled=*/true, QString(), QString(), QStringList());
        return;
    }

    // Wall-clock timers for the report: the whole batch, plus each pass individually.
    QElapsedTimer totalTimer;
    totalTimer.start();
    double pass1Secs = 0.0;
    double pass2Secs = 0.0;

    // ---- Pass 1: the whole list, many workers, short (default) per-file budget. The many light
    // files finish here; heavy assemblies that exceed pass1TimeLimit are dropped with a Timeout
    // and come back in p1FailedFiles. ----
    emit addFolderProgress(1, 0, total,
                           tr("embedding %n light file(s)…", "", total));

    // Feature B: forward hoops_ai's live tqdm progress to the busy dialog. The callback fires on
    // THIS worker thread from inside the bridge; it only reads m_currentPass (worker-thread state)
    // and emits a queued signal, so it is safe. phase 1 is hoops_ai's built-in heavy 1-worker
    // fallback (RAM-driven), phase 0 the main pool.
    m_currentPass = 1;
    HoopsAiIndex::setAddFolderProgressCallback(
        [this](int phase, int done, int total, int errors, int heavy) {
            QString phaseTxt = (phase == 1) ? tr("heavy fallback (1 worker)") : tr("embedding");
            QString txt = tr("%1 %2/%3").arg(phaseTxt).arg(done).arg(total);
            if (errors > 0)
                txt += tr(" · errors %1").arg(errors);
            if (heavy > 0)
                txt += tr(" · heavy %1").arg(heavy);
            emit addFolderProgress(m_currentPass, done, total, txt);
        });

    int p1Added = 0, p1Failed = 0;
    QStringList p1FailedFiles;
    QString p1Warning;
    QElapsedTimer p1Timer;
    p1Timer.start();
    bool const ok1 = HoopsAiIndex::addCadFolder(cadPaths, pass1Workers, pass1TimeLimit,
                                                p1Added, p1Failed, p1FailedFiles, p1Warning);
    pass1Secs = p1Timer.nsecsElapsed() / 1e9;
    // Feature A: hoops_ai wrote too_heavy_files.log into this process CWD for pass 1 (RAM fallback
    // list). Capture it now, before pass 2 overwrites it.
    QStringList heavyFlagged = readTooHeavyFiles();
    if (!ok1) {
        // The whole pass-1 call failed; nothing was embedded. Surface the error.
        HoopsAiIndex::setAddFolderProgressCallback({});
        emit addFolderProgress(1, total, total, tr("failed"));
        emit addFolderFinished(0, total, /*canceled=*/false,
                               p1Warning.section(QLatin1Char('\n'), 0, 0), QString(), QStringList());
        return;
    }

    // Files that succeeded in pass 1 = everything not reported as failed.
    QSet<QString> const p1FailedSet(p1FailedFiles.begin(), p1FailedFiles.end());
    QStringList lightAdded;
    lightAdded.reserve(cadPaths.size());
    for (const QString& f : cadPaths)
        if (!p1FailedSet.contains(f))
            lightAdded << f;

    // Pass 1's faiss index is now saved (addCadFolder saves once at its end). Tell the GUI to
    // persist these files' change signatures to the sidecar JSON now, so a force-kill during the
    // long Pass 2 still leaves Pass 1 fully recorded in BOTH files.
    emit addFolderPass1Committed(lightAdded);

    // ---- Route pass-1 failures by their recorded reason. Capture error_summary.json NOW: the
    // pass-2 embed call overwrites it, so this is the only point where we can see WHY each pass-1
    // file failed. Only files that TIMED OUT (genuinely heavy assemblies needing a larger per-file
    // budget) are worth retrying in pass 2. Files that failed with a deterministic CAD error
    // (graph not found / NoRootInModel / scs-image / list-index / Exception: Failed / ...) never
    // recover with more time, so we mark them permanently failed now instead of spending a full
    // pass-2 budget on each. An unknown reason (not in error_summary) is treated as retryable to
    // stay safe. This is the "use pass-1 results to drive pass-2 exclusion" policy. ----
    QMap<QString, QString> const p1Reasons = readErrorSummaryReasons();
    QStringList pass2Retry;
    QStringList hardFailed;
    for (const QString& f : p1FailedFiles) {
        QString const reason = p1Reasons.value(normPathKey(f));
        if (reason.isEmpty() || reason.contains(QStringLiteral("Timeout"), Qt::CaseInsensitive))
            pass2Retry << f;
        else
            hardFailed << f;
    }

    // ---- Pass 2: retry ONLY the timed-out (heavy) files with a larger per-file budget and few
    // workers. A single heavy file is not sped up by workers, so a small count is enough;
    // across-file parallelism still helps when several heavy files remain. ----
    QStringList heavyAdded;
    QStringList permanentFailed = p1FailedFiles; // default (pass 2 skipped/failed): none recovered
    QString p2Warning;
    bool retriedPass2 = false;
    if (!pass2Retry.isEmpty() && !m_cancel.loadRelaxed()) {
        retriedPass2 = true;
        m_currentPass = 2;
        emit addFolderProgress(2, 0, pass2Retry.size(),
                               tr("retrying %n heavy file(s)…", "", pass2Retry.size()));
        int p2Added = 0, p2Failed = 0;
        QStringList p2FailedFiles;
        QElapsedTimer p2Timer;
        p2Timer.start();
        bool const ok2 = HoopsAiIndex::addCadFolder(pass2Retry, pass2Workers, pass2TimeLimit,
                                                    p2Added, p2Failed, p2FailedFiles, p2Warning);
        pass2Secs = p2Timer.nsecsElapsed() / 1e9;
        // Union pass-2's RAM-fallback list into the heavy-flagged set for the report.
        for (const QString& f : readTooHeavyFiles())
            if (!heavyFlagged.contains(f))
                heavyFlagged << f;
        if (ok2) {
            QSet<QString> const p2FailedSet(p2FailedFiles.begin(), p2FailedFiles.end());
            permanentFailed = hardFailed; // hard CAD errors never entered pass 2, but still failed
            heavyAdded.reserve(pass2Retry.size());
            for (const QString& f : pass2Retry) {
                if (p2FailedSet.contains(f))
                    permanentFailed << f;
                else
                    heavyAdded << f;
            }
        }
        // If ok2 is false the whole pass-2 call failed; keep every pass-1 failure as permanent
        // (permanentFailed already holds them all) and note the error below.
    }
    // If pass2Retry is empty (all pass-1 failures were hard CAD errors), pass 2 is skipped and
    // permanentFailed already equals p1FailedFiles (== hardFailed), which is correct.

    // Stop forwarding progress; the batch is done.
    HoopsAiIndex::setAddFolderProgressCallback({});

    // Feature A: read the freshest error_summary.json (written by hoops_ai into this CWD, reflecting
    // the LAST pass that ran) to attach a per-file reason to each still-failed file, so the report
    // distinguishes a timed-out heavy assembly from a genuine CAD error.
    QMap<QString, QString> failReasons = readErrorSummaryReasons();
    // Pass 2's error_summary.json only covers the files pass 2 actually processed. Re-attach the
    // pass-1 reasons for files we short-circuited (hard CAD errors that never entered pass 2) so
    // the report still explains why each was not indexed.
    for (const QString& f : hardFailed) {
        QString const key = normPathKey(f);
        if (!failReasons.contains(key))
            failReasons.insert(key, p1Reasons.value(key));
    }

    int const addedTotal = lightAdded.size() + heavyAdded.size();
    int const failedTotal = permanentFailed.size();
    double const totalSecs = totalTimer.nsecsElapsed() / 1e9;

    // Persist the groups to a report next to the index so the operator can audit exactly which
    // parts made it into the index, which did not (and why), and which were RAM-heavy.
    QString const logPath = writeAddFolderReport(lightAdded, heavyAdded, permanentFailed,
                                                 failReasons, heavyFlagged,
                                                 pass1Workers, pass1TimeLimit,
                                                 pass2Workers, pass2TimeLimit,
                                                 pass1Secs, pass2Secs, totalSecs);

    // Build the details text shown in the finished dialog's collapsible area.
    QStringList problems;
    problems << tr("Elapsed: %1 (pass 1: %2%3)")
                    .arg(formatElapsed(totalSecs), formatElapsed(pass1Secs),
                         pass2Secs > 0 ? tr(", pass 2: %1").arg(formatElapsed(pass2Secs))
                                       : QString());
    problems << tr("Light files added (pass 1): %1").arg(lightAdded.size());
    if (retriedPass2)
        problems << tr("Heavy files recovered (pass 2): %1").arg(heavyAdded.size());
    problems << tr("Failed (not indexed): %1").arg(permanentFailed.size());
    if (!logPath.isEmpty())
        problems << tr("Report: %1").arg(logPath);
    if (!p1Warning.isEmpty())
        problems << p1Warning.section(QLatin1Char('\n'), 0, 0);
    if (!p2Warning.isEmpty())
        problems << p2Warning.section(QLatin1Char('\n'), 0, 0);
    if (!permanentFailed.isEmpty()) {
        problems << QString();
        problems << tr("Not indexed:");
        for (const QString& f : permanentFailed) {
            QString const reason = failReasons.value(normPathKey(f));
            QString line = QStringLiteral("  %1").arg(QFileInfo(f).fileName());
            if (reason.contains(QStringLiteral("Timeout"), Qt::CaseInsensitive))
                line += tr("  (timeout)");
            else if (!reason.isEmpty())
                line += tr("  (error: %1)").arg(reason);
            problems << line;
        }
    }

    // The files that actually made it into the index (light in pass 1 + heavy recovered in pass 2).
    // The GUI commits their file-change signatures so the next Add Folder can skip them.
    QStringList addedFiles = lightAdded;
    addedFiles += heavyAdded;

    emit addFolderProgress(1, total, total, QString());
    emit addFolderFinished(addedTotal, failedTotal, /*canceled=*/false,
                           problems.join(QLatin1Char('\n')), logPath, addedFiles);
}

// -------------------------------------------------------------------- SimilarityIndexPanel

SimilarityIndexPanel::SimilarityIndexPanel(QWidget* parent)
    : QWidget(parent)
{
    // Register the types carried by queued signals between the worker thread and the GUI thread.
    qRegisterMetaType<SimHit>("SimHit");
    qRegisterMetaType<QVector<SimHit>>("QVector<SimHit>");
    qRegisterMetaType<ShapeMapPoint>("ShapeMapPoint");
    qRegisterMetaType<QVector<ShapeMapPoint>>("QVector<ShapeMapPoint>");
    qRegisterMetaType<ShapeMapLegendEntry>("ShapeMapLegendEntry");
    qRegisterMetaType<QVector<ShapeMapLegendEntry>>("QVector<ShapeMapLegendEntry>");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* toolbar = new QToolBar(this);
    // Compact, icon-only toolbar: the labels moved to tooltips so the bar no longer overflows with
    // text. Icons are bundled 48x48 PNGs from the shared resource file (:/HPSMainWindow/…); Qt
    // scales them to the toolbar icon size (and picks the full-res source on HiDPI). Tooltips carry
    // the full action name (and, where set below, extra help).
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_openAction   = toolbar->addAction(tr("Open Index…"), this, &SimilarityIndexPanel::onOpenIndex);
    m_openAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simOpenIcon")));
    m_addAction    = toolbar->addAction(tr("Add Current Model"), this, &SimilarityIndexPanel::onAddCurrent);
    m_addAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simAddModelIcon")));
    m_addFolderAction = toolbar->addAction(tr("Add Folder…"), this, &SimilarityIndexPanel::onAddFolder);
    m_addFolderAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simAddFolderIcon")));
    m_searchAction = toolbar->addAction(tr("Search with Current Model"), this, &SimilarityIndexPanel::onSearchToggled);
    m_searchAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simSearchIcon")));
    m_searchAction->setCheckable(true);
    m_searchAction->setToolTip(tr("Search with Current Model\n"
                                  "ON: search the index for parts similar to the current model.\n"
                                  "OFF: show the parts already registered in the index."));
    m_assemblySearchAction = toolbar->addAction(tr("Search Similar Assembly"), this, &SimilarityIndexPanel::onAssemblySearchToggled);
    m_assemblySearchAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simSearchAssemblyIcon")));
    m_assemblySearchAction->setCheckable(true);
    m_assemblySearchAction->setToolTip(tr("Search Similar Assembly\n"
                                          "ON: search the index for whole assemblies similar to the\n"
                                          "current model (assembly-to-assembly matching).\n"
                                          "OFF: show the parts already registered in the index."));
    m_mapAction = toolbar->addAction(tr("Shape Embedding Map"), this, &SimilarityIndexPanel::onShapeMap);
    m_mapAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simMapIcon")));
    m_mapAction->setToolTip(tr("Shape Embedding Map\n"
                               "Project the TAGGED parts of the index into a 2D/3D cluster map and\n"
                               "show it in the 3D view (one point per file, colored by tag). Click a\n"
                               "part in the list to highlight its point on the map."));
    m_assignTagAction = toolbar->addAction(tr("Assign Tag…"), this, &SimilarityIndexPanel::onAssignTag);
    m_assignTagAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simAssignTagIcon")));
    m_assignTagAction->setToolTip(tr("Assign Tag…\n"
                                     "Assign a tag to every part currently shown by the similarity\n"
                                     "slider (search mode): the displayed cluster becomes that tag."));
    m_removeTagAction = toolbar->addAction(tr("Remove Tag"), this, &SimilarityIndexPanel::onRemoveTag);
    m_removeTagAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simRemoveTagIcon")));
    m_removeTagAction->setToolTip(tr("Remove Tag\n"
                                     "Remove the tag selected in the filter from all its parts."));
    m_closeAction  = toolbar->addAction(tr("Close"), this, &SimilarityIndexPanel::onCloseIndex);
    m_closeAction->setIcon(QIcon(QStringLiteral(":/HPSMainWindow/simCloseIcon")));
    layout->addWidget(toolbar);

    // Similarity threshold bar: filters the cached search hits live (search mode only). Hidden
    // until a search runs so it does not clutter the index-listing view.
    m_thresholdBar = new QWidget(this);
    auto* threshLayout = new QHBoxLayout(m_thresholdBar);
    threshLayout->setContentsMargins(6, 2, 6, 2);
    threshLayout->addWidget(new QLabel(tr("Min similarity:"), m_thresholdBar));
    m_threshold = new QSlider(Qt::Horizontal, m_thresholdBar);
    m_threshold->setRange(kThreshMin, kThreshMax);
    m_threshold->setValue(kThreshDefault);
    m_threshold->setSingleStep(kThreshStep);
    m_threshold->setPageStep(kThreshStep * 5);
    m_threshold->setTracking(true);
    threshLayout->addWidget(m_threshold, 1);
    m_thresholdValueLabel = new QLabel(m_thresholdBar);
    m_thresholdValueLabel->setMinimumWidth(36);
    threshLayout->addWidget(m_thresholdValueLabel);
    connect(m_threshold, &QSlider::valueChanged, this, &SimilarityIndexPanel::onThresholdChanged);
    m_thresholdValueLabel->setText(QStringLiteral("%1").arg(thresholdValue(), 0, 'f', 3));
    m_thresholdBar->setVisible(false);
    layout->addWidget(m_thresholdBar);

    // Tag filter bar: in the index-listing (OFF) mode, narrows the shown parts to one tag. The
    // first entry is "All" (no filtering); every tag in the store follows. Hidden in search mode,
    // where all parts are search targets and tag filtering does not apply.
    m_tagBar = new QWidget(this);
    auto* tagLayout = new QHBoxLayout(m_tagBar);
    tagLayout->setContentsMargins(6, 2, 6, 2);
    tagLayout->addWidget(new QLabel(tr("Filter:"), m_tagBar));
    m_tagFilter = new QComboBox(m_tagBar);
    m_tagFilter->addItem(tr("All"), kAllTagsData);
    tagLayout->addWidget(m_tagFilter, 1);
    connect(m_tagFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimilarityIndexPanel::onTagFilterChanged);
    m_tagBar->setVisible(false);
    layout->addWidget(m_tagBar);

    m_view = new QListView(this);
    m_view->setViewMode(QListView::IconMode);
    m_view->setIconSize(QSize(kThumbW, kThumbH));
    m_view->setResizeMode(QListView::Adjust);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setWordWrap(true);
    m_view->setMovement(QListView::Static);
    m_view->setSpacing(8);
    m_view->setUniformItemSizes(true);
    // Batched layout keeps the view responsive while thousands of rows are inserted, and only
    // visible rows ever pull their (lazily decoded) thumbnail through the model's data().
    m_view->setLayoutMode(QListView::Batched);
    m_view->setBatchSize(200);
    m_view->setItemDelegate(new GalleryItemDelegate(m_view));
    m_model = new IndexGalleryModel(this);
    m_model->setThumbnailSize(QSize(kThumbW, kThumbH));
    m_view->setModel(m_model);
    connect(m_view, &QListView::doubleClicked,
            this, &SimilarityIndexPanel::onItemActivated);
    // Single-click a gallery item -> highlight the matching point on the shape map (if shown).
    connect(m_view, &QListView::clicked, this, [this](const QModelIndex& index) {
        emit partSelected(m_model->idAt(index.row()));
    });
    // Selecting a thumbnail shows that part's file name / path / tags in the bottom info area;
    // clearing the selection (clicking an empty area, or a filter/reset that empties the model)
    // restores the index metadata view.
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) {
                QModelIndexList const sel = m_view->selectionModel()->selectedIndexes();
                if (!sel.isEmpty() && sel.first().isValid()) {
                    showPartInfo(m_model->idAt(sel.first().row()));
                } else {
                    hidePartInfo();
                    // Selection cleared (e.g. clicking an empty area of the gallery): also clear the
                    // matching point highlight on the shape map. HighlightShapeMapPoint is a no-op
                    // when no map is shown.
                    emit partSelected(QString());
                }
            });
    layout->addWidget(m_view);

    m_infoPanel = new QWidget(this);
    auto* infoLayout = new QGridLayout(m_infoPanel);
    infoLayout->setContentsMargins(6, 6, 6, 6);
    infoLayout->setHorizontalSpacing(8);
    infoLayout->setVerticalSpacing(2);

    auto addInfoRow = [this, infoLayout](int row, const QString& title, QLabel*& valueLabel) {
        auto* titleLabel = new QLabel(title, m_infoPanel);
        QFont bold = titleLabel->font();
        bold.setBold(true);
        titleLabel->setFont(bold);
        titleLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        infoLayout->addWidget(titleLabel, row, 0);

        valueLabel = new QLabel(m_infoPanel);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setWordWrap(true);
        valueLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        // Don't let a long value (e.g. a file path) drive the panel's minimum width, so switching
        // between the index-info and part-info panels never changes the dock width.
        valueLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        infoLayout->addWidget(valueLabel, row, 1);
    };

    addInfoRow(0, tr("Index:"), m_infoNameLabel);
    addInfoRow(1, tr("Path:"), m_infoPathLabel);
    addInfoRow(2, tr("Files:"), m_infoFileCountLabel);
    addInfoRow(3, tr("Bodies:"), m_infoBodyCountLabel);
    addInfoRow(4, tr("Assemblies:"), m_infoAssemblyCountLabel);
    addInfoRow(5, tr("Single-part files:"), m_infoSinglePartCountLabel);
    addInfoRow(6, tr("Dimension:"), m_infoDimensionLabel);
    infoLayout->setColumnStretch(1, 1);
    m_infoPanel->setVisible(false);
    layout->addWidget(m_infoPanel);

    // Part-info panel: occupies the same slot as m_infoPanel; shown while a gallery thumbnail is
    // selected (file name / path / tags).
    m_partInfoPanel = new QWidget(this);
    auto* partLayout = new QGridLayout(m_partInfoPanel);
    partLayout->setContentsMargins(6, 6, 6, 6);
    partLayout->setHorizontalSpacing(8);
    partLayout->setVerticalSpacing(2);

    auto addPartRow = [this, partLayout](int row, const QString& title, QLabel*& valueLabel) {
        auto* titleLabel = new QLabel(title, m_partInfoPanel);
        QFont bold = titleLabel->font();
        bold.setBold(true);
        titleLabel->setFont(bold);
        titleLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        partLayout->addWidget(titleLabel, row, 0);

        valueLabel = new QLabel(m_partInfoPanel);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setWordWrap(true);
        valueLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        // Don't let a long value (e.g. a file path) drive the panel's minimum width, so switching
        // between the part-info and index-info panels never changes the dock width.
        valueLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        partLayout->addWidget(valueLabel, row, 1);
    };

    addPartRow(0, tr("Part:"), m_partNameLabel);
    addPartRow(1, tr("Type:"), m_partTypeLabel);
    addPartRow(2, tr("Bodies:"), m_partBodiesLabel);
    addPartRow(3, tr("Path:"), m_partPathLabel);
    addPartRow(4, tr("Tags:"), m_partTagsLabel);
    // "Edit…" button beneath the tags value: lets the user add/remove tags on this single part.
    m_partEditTagsButton = new QPushButton(tr("Edit Tags…"), m_partInfoPanel);
    m_partEditTagsButton->setToolTip(tr("Add or remove tags on this part."));
    connect(m_partEditTagsButton, &QPushButton::clicked, this, &SimilarityIndexPanel::onEditPartTags);
    partLayout->addWidget(m_partEditTagsButton, 5, 1, Qt::AlignLeft);
    partLayout->setColumnStretch(1, 1);
    m_partInfoPanel->setVisible(false);
    layout->addWidget(m_partInfoPanel);

    // Debounce the similarity slider: while the user drags, coalesce the rapid value changes into
    // a single re-filter so we don't rebuild the visible model on every tick.
    m_thresholdDebounce = new QTimer(this);
    m_thresholdDebounce->setSingleShot(true);
    m_thresholdDebounce->setInterval(120);
    connect(m_thresholdDebounce, &QTimer::timeout,
            this, &SimilarityIndexPanel::applyThresholdFilter);

    // Worker thread for the blocking add/search bridge calls.
    m_thread = new QThread(this);
    m_worker = new IndexWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &SimilarityIndexPanel::requestAdd,
            m_worker, &IndexWorker::doAdd, Qt::QueuedConnection);
    connect(this, &SimilarityIndexPanel::requestSearch,
            m_worker, &IndexWorker::doSearch, Qt::QueuedConnection);
    connect(this, &SimilarityIndexPanel::requestSearchAssembly,
            m_worker, &IndexWorker::doSearchAssembly, Qt::QueuedConnection);
    connect(this, &SimilarityIndexPanel::requestShapeMap,
            m_worker, &IndexWorker::doComputeShapeMap, Qt::QueuedConnection);
    connect(this, &SimilarityIndexPanel::requestAddFolder,
            m_worker, &IndexWorker::doAddFolder, Qt::QueuedConnection);
    connect(this, &SimilarityIndexPanel::requestListParts,
            m_worker, &IndexWorker::doListParts, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::addFinished,
            this, &SimilarityIndexPanel::onAddFinished, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::searchFinished,
            this, &SimilarityIndexPanel::onSearchFinished, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::addFolderProgress,
            this, &SimilarityIndexPanel::onAddFolderProgress, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::addFolderPass1Committed,
            this, &SimilarityIndexPanel::onAddFolderPass1Committed, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::addFolderFinished,
            this, &SimilarityIndexPanel::onAddFolderFinished, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::listPartsFinished,
            this, &SimilarityIndexPanel::onListPartsFinished, Qt::QueuedConnection);
    connect(m_worker, &IndexWorker::shapeMapComputed,
            this, &SimilarityIndexPanel::onShapeMapComputed, Qt::QueuedConnection);
    m_thread->start();

    updateActionState();
}

SimilarityIndexPanel::~SimilarityIndexPanel()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
}

QString SimilarityIndexPanel::defaultIndexRoot() const
{
    QString const root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/indexes");
    QDir().mkpath(root);
    return root;
}

void SimilarityIndexPanel::setCurrentCadPath(const QString& cadPath)
{
    bool const changed = (cadPath != m_currentCadPath);
    m_currentCadPath = cadPath;
    updateActionState();
    // When a search toggle is ON, loading a new model re-runs the active similarity search on it.
    if (changed && m_indexOpen && !m_busy && isSearchMode() && !m_currentCadPath.isEmpty())
        refreshResults();
}

void SimilarityIndexPanel::selectPartInList(const QString& partId)
{
    if (!m_view || !m_model)
        return;
    if (partId.isEmpty()) {
        // Empty id (e.g. an empty-space click cleared the map highlight): drop the gallery
        // selection too, so the panel and the 3D view stay in sync.
        m_view->selectionModel()->clearSelection();
        return;
    }
    int const row = m_model->rowForId(partId);
    if (row < 0)
        return;
    QModelIndex const index = m_model->index(row, 0);
    // Programmatic selection: this drives QListView's selection/current item but does not emit the
    // view's clicked() signal, so it will not loop back through partSelected into the 3D highlight.
    m_view->setCurrentIndex(index);
    m_view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    m_view->scrollTo(index, QAbstractItemView::EnsureVisible);
}

bool SimilarityIndexPanel::isSearchMode() const
{
    return (m_searchAction && m_searchAction->isChecked()) ||
           (m_assemblySearchAction && m_assemblySearchAction->isChecked());
}

void SimilarityIndexPanel::updateActionState()
{
    bool const hasModel = !m_currentCadPath.isEmpty();
    bool const searchMode = isSearchMode();
    m_openAction->setEnabled(!m_busy);
    m_addAction->setEnabled(!m_busy && m_indexOpen && hasModel);
    m_addFolderAction->setEnabled(!m_busy && m_indexOpen);
    m_searchAction->setEnabled(!m_busy && m_indexOpen && hasModel);
    m_assemblySearchAction->setEnabled(!m_busy && m_indexOpen && hasModel);
    if (m_mapAction)
        m_mapAction->setEnabled(!m_busy && m_indexOpen);
    m_closeAction->setEnabled(!m_busy && m_indexOpen);

    // Tagging: assign works on the displayed cluster in search mode; the tag filter combobox and
    // Remove Tag only apply in the index-listing (OFF) mode. Remove needs a specific tag selected.
    m_assignTagAction->setEnabled(!m_busy && m_indexOpen && searchMode && !m_searchHits.isEmpty());
    if (m_tagFilter)
        m_tagFilter->setEnabled(!m_busy && m_indexOpen && !searchMode);
    m_removeTagAction->setEnabled(!m_busy && m_indexOpen && !searchMode && !currentTagFilter().isEmpty());
}

void SimilarityIndexPanel::beginBusy(const QString& label)
{
    m_busy = true;
    updateActionState();
    // Bridge calls cannot be interrupted, so use a modal busy dialog with no cancel button.
    m_progress = new QProgressDialog(label, QString(), 0, 0, this);
    m_progress->setWindowModality(Qt::ApplicationModal);
    m_progress->setMinimumDuration(0);
    m_progress->setCancelButton(nullptr);
    m_progress->setValue(0);
    m_progress->show();
    QApplication::setOverrideCursor(Qt::WaitCursor);
}

void SimilarityIndexPanel::endBusy()
{
    QApplication::restoreOverrideCursor();
    if (m_progress) {
        m_progress->close();
        m_progress->deleteLater();
        m_progress = nullptr;
    }
    m_busy = false;
    updateActionState();
}

void SimilarityIndexPanel::onOpenIndex()
{
    QSettings settings;
    QString const last = settings.value(QStringLiteral("lastIndexFile")).toString();
    QString const startPath = last.isEmpty() ? (defaultIndexRoot() + QStringLiteral("/my_index.faiss"))
                                             : last;

    // The index is a .faiss file the user opens or creates (its parent folder name is free).
    // Use a save dialog with overwrite confirmation disabled so an existing .faiss opens
    // silently while a new name creates a fresh index.
    QString dir = QFileDialog::getSaveFileName(
        this, tr("Open or Create Index (.faiss)"), startPath,
        tr("FAISS Index (*.faiss)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (dir.isEmpty())
        return;
    // Ensure the chosen path carries the .faiss extension (the bridge keys off it).
    if (!dir.endsWith(QStringLiteral(".faiss"), Qt::CaseInsensitive))
        dir += QStringLiteral(".faiss");

    // Opening an index requires the similarity-search (embeddings) model, since adding/searching
    // reuses it. If it has not been loaded yet, tell the user first, then (on OK) prompt for a
    // checkpoint. Model loading is the slow step, so the wait cursor is set only after the dialogs
    // are dismissed and kept until loading finishes.
#ifdef USING_EXCHANGE
    if (!HPSWidget::IsEmbeddingsModelLoaded()) {
        QMessageBox::StandardButton const btn = QMessageBox::information(
            this, tr("Open Index"),
            tr("The similarity-search model has not been loaded yet.\n"
               "Click OK to select a model checkpoint (.ckpt) to load."),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
        if (btn != QMessageBox::Ok)
            return;

        QString const ckpt = HPSWidget::PromptForCheckpoint(this);
        if (ckpt.isEmpty())
            return; // user cancelled the checkpoint dialog

        QString loadError;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        // Force the pending cursor change to be applied before the blocking load starts.
        QApplication::processEvents();
        bool const loaded = HPSWidget::LoadEmbeddingsModelFrom(ckpt, loadError);
        QApplication::restoreOverrideCursor();
        if (!loaded) {
            HPSWidget::ShowLoadError(this, tr("Open Index"), loadError);
            return;
        }
    }
#endif

    QString message;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    // openIndex runs synchronously on the GUI thread and blocks the event loop, so force the
    // pending cursor change to be applied before the blocking call starts.
    QApplication::processEvents();
    bool const opened = HoopsAiIndex::openIndex(dir, message);
    QApplication::restoreOverrideCursor();
    if (!opened) {
        showBridgeError(this, tr("Open Index"), message);
        return;
    }

    settings.setValue(QStringLiteral("lastIndexFile"), dir);
    m_indexOpen = true;
    m_indexFaissPath = dir;
    m_model->clearRows();

    // Resolve where this index's thumbnail PNGs live. The bridge defaults to a per-index folder
    // (<indexBase>/), but some distributed indexes ship their images elsewhere (e.g. the
    // tutorial's "images_tmcad"). If the default folder is missing, offer to pick the image folder
    // so hits still render with thumbnails instead of grey placeholders.
    applyThumbnailDirForIndex(dir);

    // Load the sidecar tag database (if any) alongside the index, then build the tag combobox.
    QString tagErr;
    if (!m_meta.load(dir, &tagErr))
        QMessageBox::warning(this, tr("Open Index"),
                             tr("The index opened, but its tag file could not be read:\n%1").arg(tagErr));
    rebuildTagCombo();

    // A freshly opened index starts in OFF mode: show the parts it already contains.
    m_searchAction->setChecked(false);
    // The window title shows the index name (the .faiss file stem).
    setWindowTitle(tr("Similarity Search - %1").arg(QFileInfo(dir).completeBaseName()));
    refreshIndexInfo(/*warnOnFailure=*/true);
    updateActionState();
    refreshResults();
}

void SimilarityIndexPanel::applyThumbnailDirForIndex(const QString& faissPath)
{
    // Default per-index image folder used by the bridge: the .faiss path without its extension.
    QFileInfo const fi(faissPath);
    QString const defaultImgDir = fi.dir().filePath(fi.completeBaseName());

    QSettings settings;
    settings.beginGroup(QStringLiteral("thumbnailDirs"));
    QString const key = QDir::toNativeSeparators(faissPath);
    bool remembered = settings.contains(key);
    QString chosen = settings.value(key).toString();  // "" == user chose to skip
    settings.endGroup();

    // A remembered custom folder that no longer exists should not be trusted: fall through to
    // re-prompting so the user can point at the (possibly moved) images again.
    if (remembered && !chosen.isEmpty() && !QDir(chosen).exists()) {
        remembered = false;
        chosen.clear();
    }

    auto persist = [&](const QString& value) {
        QSettings s;
        s.beginGroup(QStringLiteral("thumbnailDirs"));
        s.setValue(key, value);
        s.endGroup();
    };
    auto apply = [&](const QString& dir) {
        QString msg;
        if (!HoopsAiIndex::setThumbnailDir(dir, msg) && !msg.isEmpty())
            qWarning() << "setThumbnailDir failed:" << msg;
    };

    if (remembered) {
        // Honour the previous decision without prompting again.
        apply(chosen);  // empty clears the override (default folder / placeholders)
        return;
    }

    if (QDir(defaultImgDir).exists()) {
        // The bridge's default folder is present; use it (clear any stale override).
        apply(QString());
        return;
    }

    // No default image folder: ask whether to point at a separate one.
    QMessageBox::StandardButton const btn = QMessageBox::question(
        this, tr("Thumbnail Images Not Found"),
        tr("This index has no thumbnail image folder at:\n%1\n\n"
           "Select the folder that contains the part images (e.g. \"images_tmcad\") "
           "to show thumbnails?\n\nChoose \"No\" to load without thumbnails.")
            .arg(QDir::toNativeSeparators(defaultImgDir)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (btn != QMessageBox::Yes) {
        apply(QString());     // load with placeholders
        persist(QString());   // remember "skip" so we do not nag on every open
        return;
    }

    QString const startDir = fi.dir().absolutePath();
    QString const picked = QFileDialog::getExistingDirectory(
        this, tr("Select Thumbnail Image Folder"), startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (picked.isEmpty()) {
        apply(QString());     // cancelled: placeholders, and ask again next open
        return;
    }

    apply(picked);
    persist(picked);
}

void SimilarityIndexPanel::onAddCurrent()
{
    if (!m_indexOpen || m_currentCadPath.isEmpty() || m_busy)
        return;
    beginBusy(tr("Adding model to index…"));
    emit requestAdd(m_currentCadPath);
}

void SimilarityIndexPanel::onAddFolder()
{
    if (!m_indexOpen || m_busy)
        return;

    QSettings settings;
    QString const last = settings.value(QStringLiteral("lastAddFolder")).toString();
    QString const startDir = last.isEmpty() ? defaultIndexRoot() : last;

    QString const folder = QFileDialog::getExistingDirectory(
        this, tr("Select Folder of CAD Files"), startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folder.isEmpty())
        return;
    settings.setValue(QStringLiteral("lastAddFolder"), folder);

    // Scanning a deep tree can take a moment, so show the wait cursor while enumerating.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();
    QStringList const cadPaths = collectCadFiles(folder);
    QApplication::restoreOverrideCursor();

    if (cadPaths.isEmpty()) {
        QMessageBox::information(this, tr("Add Folder"),
                                 tr("No CAD files were found in the selected folder."));
        return;
    }

    // Change detection: cache each file's current (mtime,size) and skip files whose signature is
    // unchanged since they were last embedded (recorded in the sidecar metadata). Files that are
    // new or modified go into toEmbed; the rest are skipped so the batch does not re-embed them.
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_pendingSignatures.clear();
    QStringList toEmbed;
    toEmbed.reserve(cadPaths.size());
    for (const QString& p : cadPaths) {
        QFileInfo const fi(p);
        qint64 const mtime = fi.lastModified().toSecsSinceEpoch();
        qint64 const size = fi.size();
        m_pendingSignatures.insert(p, qMakePair(mtime, size));
        if (m_meta.fileUnchanged(p, mtime, size))
            continue;  // unchanged since last add -> skip re-embedding
        toEmbed << p;
    }
    QApplication::restoreOverrideCursor();
    m_lastSkippedCount = cadPaths.size() - toEmbed.size();

    if (toEmbed.isEmpty()) {
        m_pendingSignatures.clear();
        QMessageBox::information(
            this, tr("Add Folder"),
            tr("All %n file(s) are unchanged since the last add; nothing to re-index.", "",
               cadPaths.size()));
        return;
    }

    int const total = toEmbed.size();

    // Two-pass configuration shown in an editable confirm dialog. Defaults: pass 1 uses one worker
    // per PHYSICAL core with hoops_ai's short budget (benchmarks: the embedding sweet spot is the
    // physical-core count, not the logical count, which only adds RAM pressure); pass 2 re-adds
    // only the files that timed out, with a worker count sized to free RAM (heavy assemblies are
    // memory hungry: round(freeRAM / 4GB)) and a much larger budget. The dialog always shows freshly-computed
    // recommended defaults (not last-used) so the values are deterministic per machine/state.
    int physCores = detectPhysicalCores();
    if (physCores <= 0)
        physCores = qMax(1, QThread::idealThreadCount());
    quint64 const freeBytes = detectAvailablePhysMemoryBytes();
    // Round (not floor) free-RAM/4GB so a machine sitting just under a 4 GB boundary (e.g. 15.7 GB
    // free -> 3.9) rounds up to the expected worker count. Still depends on the free RAM at launch.
    double const freeGb4 = static_cast<double>(freeBytes) / (quint64(4) * 1024 * 1024 * 1024);
    int memWorkers = static_cast<int>(freeGb4 + 0.5);
    if (memWorkers < 1)
        memWorkers = 1;

    int const defPass1Workers = physCores;
    int const defPass1TimeLimit = 120;
    int const defPass2Workers = memWorkers;
    int const defPass2TimeLimit = 1200;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Folder"));
    auto* dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->addWidget(new QLabel(
        tr("Found %n CAD file(s) to add to the index.", "", total), &dlg));
    if (m_lastSkippedCount > 0)
        dlgLayout->addWidget(new QLabel(
            tr("(%n unchanged file(s) will be skipped.)", "", m_lastSkippedCount), &dlg));
    dlgLayout->addWidget(new QLabel(
        tr("Pass 1 embeds all files; Pass 2 re-embeds only the ones that time out."), &dlg));

    auto* form = new QFormLayout();
    auto makeSpin = [&dlg](int value, int min, int max, int step, const QString& suffix) {
        auto* sb = new QSpinBox(&dlg);
        sb->setRange(min, max);
        sb->setSingleStep(step);
        sb->setValue(qBound(min, value, max));
        if (!suffix.isEmpty())
            sb->setSuffix(suffix);
        return sb;
    };
    QSpinBox* const sbP1Workers = makeSpin(defPass1Workers, 1, 256, 1, QString());
    QSpinBox* const sbP1Time = makeSpin(defPass1TimeLimit, 1, 100000, 30, tr(" s"));
    QSpinBox* const sbP2Workers = makeSpin(defPass2Workers, 1, 256, 1, QString());
    QSpinBox* const sbP2Time = makeSpin(defPass2TimeLimit, 1, 100000, 60, tr(" s"));
    form->addRow(tr("Pass 1 workers:"), sbP1Workers);
    form->addRow(tr("Pass 1 time limit:"), sbP1Time);
    form->addRow(tr("Pass 2 workers:"), sbP2Workers);
    form->addRow(tr("Pass 2 time limit:"), sbP2Time);
    dlgLayout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Proceed"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    int const pass1Workers = sbP1Workers->value();
    int const pass1TimeLimit = sbP1Time->value();
    int const pass2Workers = sbP2Workers->value();
    int const pass2TimeLimit = sbP2Time->value();

    // The batch runs as one uninterruptible bulk-embed call per pass, so use an indeterminate
    // per-pass progress display. A custom dialog carries TWO bars: pass 1 on top (kept visible with
    // its final error/timeout counts) and pass 2 below (activated only if heavy files are retried).
    // The GUI stays responsive because the bridge call runs on the worker thread.
    m_busy = true;
    updateActionState();

    m_addFolderDialog = new QDialog(this);
    m_addFolderDialog->setWindowTitle(tr("Add Folder"));
    m_addFolderDialog->setModal(true);
    // No close button / cancel: the bulk embed call cannot be interrupted mid-flight.
    m_addFolderDialog->setWindowFlags(m_addFolderDialog->windowFlags()
                                      & ~Qt::WindowCloseButtonHint & ~Qt::WindowContextHelpButtonHint);
    auto* afLayout = new QVBoxLayout(m_addFolderDialog);
    afLayout->addWidget(new QLabel(
        tr("Adding %n CAD file(s) to the index…", "", total), m_addFolderDialog));

    // ---- Pass 1 section ----
    auto* p1Title = new QLabel(tr("Pass 1  Eembedding all files"), m_addFolderDialog);
    QFont sectionFont = p1Title->font();
    sectionFont.setBold(true);
    p1Title->setFont(sectionFont);
    afLayout->addWidget(p1Title);
    m_pass1Bar = new QProgressBar(m_addFolderDialog);
    m_pass1Bar->setRange(0, total);
    m_pass1Bar->setValue(0);
    afLayout->addWidget(m_pass1Bar);
    m_pass1Status = new QLabel(tr("Starting…"), m_addFolderDialog);
    afLayout->addWidget(m_pass1Status);

    // ---- Pass 2 section (disabled until a pass-2 retry actually starts) ----
    m_pass2Section = new QWidget(m_addFolderDialog);
    auto* p2Layout = new QVBoxLayout(m_pass2Section);
    p2Layout->setContentsMargins(0, 0, 0, 0);
    auto* p2Title = new QLabel(tr("Pass 2  Eretrying heavy files"), m_pass2Section);
    p2Title->setFont(sectionFont);
    p2Layout->addWidget(p2Title);
    m_pass2Bar = new QProgressBar(m_pass2Section);
    m_pass2Bar->setRange(0, 1);   // Empty determinate bar (no animation) until pass 2 starts.
    m_pass2Bar->setValue(0);
    p2Layout->addWidget(m_pass2Bar);
    m_pass2Status = new QLabel(tr("Waiting…"), m_pass2Section);
    p2Layout->addWidget(m_pass2Status);
    m_pass2Section->setEnabled(false);   // Greyed out until pass 2 runs.
    afLayout->addWidget(m_pass2Section);

    m_addFolderDialog->show();

    m_worker->resetCancel();

    emit requestAddFolder(toEmbed, pass1Workers, pass1TimeLimit, pass2Workers, pass2TimeLimit);
}

void SimilarityIndexPanel::onAddFolderProgress(int pass, int done, int total, const QString& text)
{
    if (!m_addFolderDialog)
        return;
    if (pass == 2) {
        // First pass-2 tick activates the lower section. total > 0 turns the busy bar into a
        // determinate one sized to the retry set; total == 0 is a label-only phase tick.
        m_pass2Section->setEnabled(true);
        if (total > 0) {
            if (m_pass2Bar->maximum() != total || m_pass2Bar->minimum() != 0)
                m_pass2Bar->setRange(0, total);
            m_pass2Bar->setValue(qBound(0, done, total));
        }
        if (!text.isEmpty())
            m_pass2Status->setText(text);
        return;
    }
    // pass 1 (default): update the top bar; its label keeps whatever errors/heavy counts arrived
    // last, so those stay visible while pass 2 runs below.
    if (total > 0) {
        if (m_pass1Bar->maximum() != total || m_pass1Bar->minimum() != 0)
            m_pass1Bar->setRange(0, total);
        m_pass1Bar->setValue(qBound(0, done, total));
    }
    if (!text.isEmpty())
        m_pass1Status->setText(text);
}

void SimilarityIndexPanel::commitFileSignatures(const QStringList& files)
{
    if (files.isEmpty())
        return;
    int committed = 0;
    for (const QString& f : files) {
        auto const it = m_pendingSignatures.constFind(f);
        if (it == m_pendingSignatures.constEnd())
            continue;
        m_meta.setFileSignature(f, it.value().first, it.value().second);
        ++committed;
    }
    if (committed > 0) {
        QString saveErr;
        if (!m_meta.save(&saveErr))
            QMessageBox::warning(this, tr("Add Folder"),
                                 tr("Could not save change-tracking metadata:\n%1").arg(saveErr));
    }
}

void SimilarityIndexPanel::onAddFolderPass1Committed(const QStringList& addedFiles)
{
    // Persist Pass 1's signatures as soon as its faiss index is saved, keeping the JSON aligned
    // with the index even if Pass 2 is force-killed. m_pendingSignatures is still populated here
    // (it is only cleared when the whole batch finishes).
    commitFileSignatures(addedFiles);
}

void SimilarityIndexPanel::onAddFolderFinished(int added, int failed, bool canceled,
                                               const QString& details, const QString& logPath,
                                               const QStringList& addedFiles)
{
    if (m_addFolderDialog) {
        m_addFolderDialog->close();
        m_addFolderDialog->deleteLater();
        m_addFolderDialog = nullptr;
        m_pass1Bar = nullptr;
        m_pass1Status = nullptr;
        m_pass2Section = nullptr;
        m_pass2Bar = nullptr;
        m_pass2Status = nullptr;
    }
    m_busy = false;
    updateActionState();

    // Commit the change signatures of every file that made it into the index (Pass 1's light files
    // were already committed when Pass 1 finished; committing them again here is harmless and also
    // records Pass 2's recovered heavy files). Only added files get a signature; failed ones are
    // left without one so they are retried next time.
    if (!canceled)
        commitFileSignatures(addedFiles);
    m_pendingSignatures.clear();

    QString summary = canceled ? tr("Batch canceled.\n") : QString();
    summary += tr("Added: %1\nFailed: %2").arg(added).arg(failed);
    if (m_lastSkippedCount > 0)
        summary += tr("\nSkipped (unchanged): %1").arg(m_lastSkippedCount);
    if (!logPath.isEmpty())
        summary += tr("\n\nA detailed report was saved to:\n%1").arg(QDir::toNativeSeparators(logPath));
    m_lastSkippedCount = 0;

    QMessageBox box(this);
    box.setWindowTitle(tr("Add Folder"));
    box.setIcon(failed > 0 ? QMessageBox::Warning : QMessageBox::Information);
    box.setText(summary);

    // The old collapsible "Show Details…" area carried too little information. When a report file
    // was written, offer to open the full report instead. Fall back to the collapsible details
    // only when no report exists (e.g. the batch was canceled or failed before a report was made).
    QPushButton* openReportBtn = nullptr;
    if (!logPath.isEmpty())
        openReportBtn = box.addButton(tr("Open Report…"), QMessageBox::ActionRole);
    else if (!details.isEmpty())
        box.setDetailedText(details);

    box.setStandardButtons(QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Ok);
    box.exec();

    if (openReportBtn != nullptr && box.clickedButton() == openReportBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));

    // In OFF (listing) mode, refresh so the newly added parts appear.
    refreshIndexInfo();
    if (!isSearchMode())
        refreshResults();
}

void SimilarityIndexPanel::onSearchToggled(bool checked)
{
    if (!m_indexOpen || m_busy) {
        // Ignore toggles while busy or with no index; keep the button state consistent.
        return;
    }
    if (checked) {
        if (m_currentCadPath.isEmpty()) {
            // Cannot search without a current model; revert to OFF and inform the user.
            m_searchAction->setChecked(false);
            QMessageBox::information(this, tr("Search"),
                                     tr("Load a model into the 3D view first to search for similar parts."));
            return;
        }
        // Part and assembly search are mutually exclusive: turning this on turns the other off
        // (silently, so its toggled handler does not re-enter refreshResults).
        if (m_assemblySearchAction->isChecked()) {
            QSignalBlocker const blocker(m_assemblySearchAction);
            m_assemblySearchAction->setChecked(false);
        }
    }
    refreshResults();
}

void SimilarityIndexPanel::onAssemblySearchToggled(bool checked)
{
    if (!m_indexOpen || m_busy) {
        return;
    }
    if (checked) {
        if (m_currentCadPath.isEmpty()) {
            m_assemblySearchAction->setChecked(false);
            QMessageBox::information(this, tr("Search Similar Assembly"),
                                     tr("Load a model into the 3D view first to search for similar assemblies."));
            return;
        }
        if (m_searchAction->isChecked()) {
            QSignalBlocker const blocker(m_searchAction);
            m_searchAction->setChecked(false);
        }
    }
    refreshResults();
}

void SimilarityIndexPanel::onShapeMap()
{
    if (!m_indexOpen || m_busy)
        return;

    // Restrict the map to tagged parts only: gather the distinct ids carrying any tag and pass them
    // to the bridge, which computes the projection/clustering on that subset alone. Without at least
    // one tagged part there is nothing to map, so prompt the user instead of computing an empty map.
    QStringList taggedIds;
    {
        QSet<QString> seen;
        for (const QString& tag : m_meta.allTags()) {
            for (const QString& id : m_meta.partsForTag(tag)) {
                if (!seen.contains(id)) { seen.insert(id); taggedIds << id; }
            }
        }
    }
    if (taggedIds.isEmpty()) {
        QMessageBox::information(this, tr("Shape Embedding Map"),
                                 tr("No tagged parts to map. Assign a tag to some parts first "
                                    "(the map is limited to tagged parts)."));
        return;
    }

    // Switch the listing to the "Tagged" filter so the panel mirrors the map's scope (tagged parts
    // only) while the map is shown.
    selectTaggedFilter();

    // Compute the map off the GUI thread (auto cluster count, 3D projection). The host renders the
    // result in the 3D view when shapeMapReady fires from onShapeMapComputed.
    beginBusy(tr("Computing shape map…"));
    emit requestShapeMap(/*nClusters=*/0, /*dims=*/3, taggedIds);
}

// Qualitative palette (0..1 RGB) shared by tag- and cluster-colored shape maps; wraps when there
// are more groups than entries. Kept in sync (by intent) with the 3D view's swatch colors, which
// come straight from the per-point colors we set here.
static void shapeMapPalette(int i, float& r, float& g, float& b)
{
    static const float kPalette[][3] = {
        {0.90f,0.10f,0.10f},{0.12f,0.47f,0.90f},{0.20f,0.70f,0.20f},{0.95f,0.60f,0.10f},
        {0.60f,0.25f,0.85f},{0.10f,0.75f,0.75f},{0.85f,0.40f,0.65f},{0.55f,0.55f,0.20f},
        {0.30f,0.30f,0.90f},{0.20f,0.60f,0.45f},{0.75f,0.75f,0.15f},{0.45f,0.55f,0.95f},
        {0.95f,0.45f,0.20f},{0.35f,0.75f,0.35f},{0.70f,0.20f,0.45f},{0.20f,0.45f,0.70f},
        {0.85f,0.65f,0.30f},{0.50f,0.30f,0.65f},{0.30f,0.65f,0.65f},{0.65f,0.35f,0.25f},
        {0.40f,0.70f,0.20f},{0.60f,0.20f,0.70f},{0.25f,0.55f,0.85f},{0.80f,0.30f,0.55f},
    };
    int const n = int(sizeof(kPalette) / sizeof(kPalette[0]));
    int const k = ((i % n) + n) % n;
    r = kPalette[k][0]; g = kPalette[k][1]; b = kPalette[k][2];
}
static const float kUntaggedGray[3] = {0.6f, 0.6f, 0.6f};
// A distinct, darker gray for parts carrying more than one tag (they belong to several clusters at
// once, so no single tag color represents them): shown under a dedicated "Multiple tags" legend.
static const float kMultiTagGray[3] = {0.4f, 0.4f, 0.4f};

void SimilarityIndexPanel::onShapeMapComputed(bool success, const QVector<ShapeMapPoint>& points,
                                              int clusterCount, const QString& message)
{
    Q_UNUSED(clusterCount);
    endBusy();
    if (!success) {
        QMessageBox::critical(this, tr("Shape Embedding Map"),
                              tr("Failed to compute the shape map:\n%1").arg(message));
        return;
    }
    if (points.isEmpty()) {
        QMessageBox::information(this, tr("Shape Embedding Map"),
                                 tr("The current index has no parts to map."));
        return;
    }

    // Resolve per-point colors and the legend. Prefer tags (a user-meaningful grouping) when the
    // index has any; otherwise fall back to the k-means clusters computed by the bridge.
    QVector<ShapeMapPoint> colored = points;
    QVector<ShapeMapLegendEntry> legend;

    bool anyTagged = false;
    for (const ShapeMapPoint& p : colored) {
        if (!m_meta.tagsForPart(p.id).isEmpty()) { anyTagged = true; break; }
    }

    if (anyTagged) {
        QStringList const tags = m_meta.allTags(); // sorted case-insensitively
        QMap<QString, int> tagColorIndex;
        for (int i = 0; i < tags.size(); ++i)
            tagColorIndex.insert(tags[i], i);
        QMap<QString, int> tagCounts;
        int untaggedCount = 0;
        int multiTagCount = 0;
        for (ShapeMapPoint& p : colored) {
            QStringList pt = m_meta.tagsForPart(p.id);
            if (pt.isEmpty()) {
                p.cr = kUntaggedGray[0]; p.cg = kUntaggedGray[1]; p.cb = kUntaggedGray[2];
                ++untaggedCount;
            } else if (pt.size() > 1) {
                // Multi-tag parts get a single dedicated gray so they stand out from the single-tag
                // (colored) clusters instead of being forced into one arbitrary tag's color.
                p.cr = kMultiTagGray[0]; p.cg = kMultiTagGray[1]; p.cb = kMultiTagGray[2];
                ++multiTagCount;
            } else {
                pt.sort(Qt::CaseInsensitive);
                QString const tag = pt.first();
                shapeMapPalette(tagColorIndex.value(tag, 0), p.cr, p.cg, p.cb);
                tagCounts[tag] = tagCounts.value(tag, 0) + 1;
            }
        }
        for (const QString& tag : tags) {
            if (tagCounts.value(tag, 0) == 0)
                continue; // this tag is not carried by any mapped part
            ShapeMapLegendEntry e;
            e.label = tag;
            e.count = tagCounts.value(tag);
            shapeMapPalette(tagColorIndex.value(tag, 0), e.r, e.g, e.b);
            legend.push_back(e);
        }
        if (multiTagCount > 0) {
            ShapeMapLegendEntry e;
            e.label = tr("Multiple tags");
            e.count = multiTagCount;
            e.r = kMultiTagGray[0]; e.g = kMultiTagGray[1]; e.b = kMultiTagGray[2];
            legend.push_back(e);
        }
        if (untaggedCount > 0) {
            ShapeMapLegendEntry e;
            e.label = tr("Untagged");
            e.count = untaggedCount;
            e.r = kUntaggedGray[0]; e.g = kUntaggedGray[1]; e.b = kUntaggedGray[2];
            legend.push_back(e);
        }
    } else {
        QMap<int, int> clusterCounts;
        for (ShapeMapPoint& p : colored) {
            shapeMapPalette(p.cluster, p.cr, p.cg, p.cb);
            clusterCounts[p.cluster] = clusterCounts.value(p.cluster, 0) + 1;
        }
        for (auto it = clusterCounts.constBegin(); it != clusterCounts.constEnd(); ++it) {
            ShapeMapLegendEntry e;
            e.label = tr("Cluster %1").arg(it.key());
            e.count = it.value();
            shapeMapPalette(it.key(), e.r, e.g, e.b);
            legend.push_back(e);
        }
    }

    emit shapeMapReady(colored, legend);
}

void SimilarityIndexPanel::refreshResults()
{
    if (!m_indexOpen || m_busy)
        return;

    bool const hasModel = !m_currentCadPath.isEmpty();
    bool const partSearch = m_searchAction->isChecked() && hasModel;
    bool const assemblySearch = m_assemblySearchAction->isChecked() && hasModel;
    bool const searchMode = partSearch || assemblySearch;
    // The similarity slider filters cosine/blended scores (0.400 E.000) for both part and assembly
    // search; the tag filter only applies to the index listing.
    setThresholdBarVisible(searchMode);
    setTagBarVisible(!searchMode);

    if (partSearch) {
        applyThresholdRangeForMode(false);
        beginBusy(tr("Searching index…"));
        emit requestSearch(m_currentCadPath, kTopK);
    } else if (assemblySearch) {
        applyThresholdRangeForMode(true);
        beginBusy(tr("Searching similar assemblies…"));
        emit requestSearchAssembly(m_currentCadPath, kTopK);
    } else {
        m_searchHits.clear();
        beginBusy(tr("Loading index parts…"));
        emit requestListParts(kListLimit);
    }
}

double SimilarityIndexPanel::thresholdValue() const
{
    return m_threshold ? m_threshold->value() / kThreshScale : (kThreshDefault / kThreshScale);
}

void SimilarityIndexPanel::applyThresholdRangeForMode(bool assemblyMode)
{
    if (!m_threshold)
        return;
    const int newMin = assemblyMode ? kThreshMin : kPartThreshMin;
    if (m_threshold->minimum() == newMin)
        return;
    // Block signals so the automatic value clamp (when the old value falls below the new floor)
    // does not spawn a redundant applyThresholdFilter -- refreshResults is already re-running the
    // search. Update the value label manually to reflect any clamped value.
    QSignalBlocker const blocker(m_threshold);
    m_threshold->setMinimum(newMin);
    if (m_thresholdValueLabel)
        m_thresholdValueLabel->setText(QStringLiteral("%1").arg(thresholdValue(), 0, 'f', 3));
}

void SimilarityIndexPanel::setThresholdBarVisible(bool visible)
{
    if (m_thresholdBar)
        m_thresholdBar->setVisible(visible);
}

void SimilarityIndexPanel::setTagBarVisible(bool visible)
{
    if (m_tagBar)
        m_tagBar->setVisible(visible);
}

QString SimilarityIndexPanel::currentTagFilter() const
{
    if (!m_tagFilter)
        return QString();
    // "All"/"Tagged"/"Part"/"Assembly" carry sentinels (not a specific tag name); the rest are tags.
    QString const data = m_tagFilter->currentData().toString();
    if (data == kAllTagsData || data == kTaggedData ||
        data == kPartData || data == kAssemblyData)
        return QString();
    return m_tagFilter->currentText();
}

bool SimilarityIndexPanel::isTaggedFilter() const
{
    return m_tagFilter && m_tagFilter->currentData().toString() == kTaggedData;
}

QString SimilarityIndexPanel::kindFilter() const
{
    if (!m_tagFilter)
        return QString();
    QString const data = m_tagFilter->currentData().toString();
    if (data == kPartData)
        return QStringLiteral("Part");
    if (data == kAssemblyData)
        return QStringLiteral("Assembly");
    return QString();
}

void SimilarityIndexPanel::selectTaggedFilter()
{
    if (!m_tagFilter)
        return;
    int const idx = m_tagFilter->findData(kTaggedData);
    if (idx < 0)
        return; // No tags in this index: the "Tagged" entry is absent.
    if (m_tagFilter->currentIndex() != idx)
        m_tagFilter->setCurrentIndex(idx); // fires onTagFilterChanged -> applyTagFilter (listing mode)
}

void SimilarityIndexPanel::rebuildTagCombo(const QString& keepTag)
{
    if (!m_tagFilter)
        return;
    // keepTag only names real tags; otherwise preserve the current selection, including the
    // All/Tagged/Part/Assembly sentinels (kept by their data value, not by display text).
    QString const prevData = m_tagFilter->currentData().toString();
    bool const keepSentinel = keepTag.isEmpty() &&
        (prevData == kTaggedData || prevData == kPartData || prevData == kAssemblyData);
    QString const wanted = keepTag.isEmpty() ? currentTagFilter() : keepTag;

    QSignalBlocker const blocker(m_tagFilter);
    m_tagFilter->clear();
    m_tagFilter->addItem(tr("All"), kAllTagsData);
    // Kind filters: always available since browse listings carry a Part/Assembly kind per part.
    m_tagFilter->addItem(tr("Part"), kPartData);
    m_tagFilter->addItem(tr("Assembly"), kAssemblyData);
    QStringList const tags = m_meta.allTags();
    if (!tags.isEmpty())
        m_tagFilter->addItem(tr("Tagged"), kTaggedData); // only useful once some tag exists
    for (const QString& tag : tags)
        m_tagFilter->addItem(tag, tag);

    int idx = 0; // default to "All"
    if (keepSentinel) {
        int const found = m_tagFilter->findData(prevData);
        if (found >= 0)
            idx = found;
    } else if (!wanted.isEmpty()) {
        int const found = m_tagFilter->findText(wanted);
        if (found >= 0)
            idx = found;
    }
    m_tagFilter->setCurrentIndex(idx);
}

QStringList SimilarityIndexPanel::displayedSearchIds() const
{
    // Both part and assembly search filter by the similarity slider, so the tag-assign cluster is
    // exactly the set of hits currently passing the threshold.
    double const thr = thresholdValue();
    QStringList ids;
    ids.reserve(m_searchHits.size());
    for (const SimHit& h : m_searchHits) {
        if (h.score >= thr)
            ids << h.id;
    }
    return ids;
}

void SimilarityIndexPanel::refreshIndexInfo(bool warnOnFailure)
{
    if (!m_indexOpen) {
        clearIndexInfo();
        return;
    }

    IndexInfo info;
    QString message;
    if (!HoopsAiIndex::currentIndexInfo(info, message)) {
        clearIndexInfo();
        if (warnOnFailure)
            showBridgeError(this, tr("Index Info"), message);
        return;
    }
    setIndexInfo(info);
}

void SimilarityIndexPanel::setIndexInfo(const IndexInfo& info)
{
    if (!m_infoPanel)
        return;

    QString const path = info.hasIndex ? info.faissPath : m_indexFaissPath;
    QFileInfo const fi(path);

    if (m_infoNameLabel)
        m_infoNameLabel->setText(fi.completeBaseName().isEmpty() ? tr("(unknown)") : fi.completeBaseName());
    if (m_infoPathLabel)
        m_infoPathLabel->setText(path.isEmpty() ? tr("(none)") : QDir::toNativeSeparators(path));
    if (m_infoFileCountLabel)
        m_infoFileCountLabel->setText(QString::number(info.fileCount));
    if (m_infoBodyCountLabel)
        m_infoBodyCountLabel->setText(QString::number(info.bodyCount));
    if (m_infoAssemblyCountLabel)
        m_infoAssemblyCountLabel->setText(QString::number(info.assemblyCount));
    if (m_infoSinglePartCountLabel)
        m_infoSinglePartCountLabel->setText(QString::number(info.singlePartCount));
    if (m_infoDimensionLabel)
        m_infoDimensionLabel->setText(QString::number(info.dimension));

    m_indexInfoVisible = (info.hasIndex || !path.isEmpty());
    // Don't steal the slot from the part-info panel while a thumbnail is selected.
    bool const partShown = m_partInfoPanel && m_partInfoPanel->isVisible();
    m_infoPanel->setVisible(m_indexInfoVisible && !partShown);
}

void SimilarityIndexPanel::clearIndexInfo()
{
    if (m_infoNameLabel)
        m_infoNameLabel->clear();
    if (m_infoPathLabel)
        m_infoPathLabel->clear();
    if (m_infoFileCountLabel)
        m_infoFileCountLabel->clear();
    if (m_infoBodyCountLabel)
        m_infoBodyCountLabel->clear();
    if (m_infoAssemblyCountLabel)
        m_infoAssemblyCountLabel->clear();
    if (m_infoSinglePartCountLabel)
        m_infoSinglePartCountLabel->clear();
    if (m_infoDimensionLabel)
        m_infoDimensionLabel->clear();
    if (m_infoPanel)
        m_infoPanel->setVisible(false);
    m_indexInfoVisible = false;
}

void SimilarityIndexPanel::showPartInfo(const QString& partId)
{
    if (!m_partInfoPanel)
        return;
    if (partId.isEmpty()) {
        hidePartInfo();
        return;
    }

    m_partInfoId = partId;

    QFileInfo const fi(partId);
    if (m_partNameLabel)
        m_partNameLabel->setText(fi.fileName().isEmpty() ? partId : fi.fileName());
    if (m_partPathLabel)
        m_partPathLabel->setText(QDir::toNativeSeparators(partId));
    if (m_partTagsLabel) {
        QStringList const tags = m_meta.tagsForPart(partId);
        m_partTagsLabel->setText(tags.isEmpty() ? tr("(none)") : tags.join(QStringLiteral(", ")));
    }

    // Resolve part vs assembly + body/component count from the index (cheap bridge call).
    int bodies = 0;
    bool isAssembly = false;
    QString bcMsg;
    if (HoopsAiIndex::partBodyCount(partId, bodies, isAssembly, bcMsg)) {
        if (m_partTypeLabel)
            m_partTypeLabel->setText(isAssembly ? tr("Assembly") : tr("Part"));
        if (m_partBodiesLabel)
            m_partBodiesLabel->setText(QString::number(bodies));
    } else {
        if (m_partTypeLabel)
            m_partTypeLabel->setText(tr("(unknown)"));
        if (m_partBodiesLabel)
            m_partBodiesLabel->setText(QStringLiteral("-"));
    }

    if (m_infoPanel)
        m_infoPanel->setVisible(false);
    m_partInfoPanel->setVisible(true);
}

void SimilarityIndexPanel::hidePartInfo()
{
    m_partInfoId.clear();
    if (m_partInfoPanel)
        m_partInfoPanel->setVisible(false);
    if (m_infoPanel)
        m_infoPanel->setVisible(m_indexInfoVisible);
}

void SimilarityIndexPanel::applyTagFilter()
{
    QString const tag = currentTagFilter();
    QString const kind = kindFilter();
    if (!kind.isEmpty()) {
        // Show only parts (or only assemblies) by their stored kind.
        QVector<SimHit> shown;
        shown.reserve(m_listHits.size());
        for (const SimHit& h : m_listHits) {
            if (h.kind == kind)
                shown.push_back(h);
        }
        populateHits(shown, /*showScore=*/false);
    } else if (isTaggedFilter()) {
        // Show only parts that carry at least one tag.
        QVector<SimHit> shown;
        shown.reserve(m_listHits.size());
        for (const SimHit& h : m_listHits) {
            if (!m_meta.tagsForPart(h.id).isEmpty())
                shown.push_back(h);
        }
        populateHits(shown, /*showScore=*/false);
    } else if (tag.isEmpty()) {
        populateHits(m_listHits, /*showScore=*/false);
    } else {
        QVector<SimHit> shown;
        shown.reserve(m_listHits.size());
        for (const SimHit& h : m_listHits) {
            if (m_meta.partHasTag(h.id, tag))
                shown.push_back(h);
        }
        populateHits(shown, /*showScore=*/false);
    }
    // Remove Tag depends on whether a specific tag is selected.
    updateActionState();
}

void SimilarityIndexPanel::onTagFilterChanged(int /*index*/)
{
    // Only meaningful in listing mode; the combobox is disabled in search mode anyway.
    if (m_indexOpen && !m_busy && !isSearchMode())
        applyTagFilter();
}

void SimilarityIndexPanel::onAssignTag()
{
    if (!m_indexOpen || m_busy || !isSearchMode())
        return;

    QStringList const ids = displayedSearchIds();
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Assign Tag"),
                                 tr("There are no parts in the current cluster to tag.\n"
                                    "Run a search and adjust the similarity slider first."));
        return;
    }

    // Offer the existing tags (editable) so a new or existing tag name can be chosen. Reusing an
    // existing name simply merges this cluster into that tag.
    QStringList const existing = m_meta.allTags();
    bool ok = false;
    QString const tag = QInputDialog::getItem(
        this, tr("Assign Tag"),
        tr("Tag for the %n displayed part(s):", "", ids.size()),
        existing, /*current=*/0, /*editable=*/true, &ok).trimmed();
    if (!ok || tag.isEmpty())
        return;

    m_meta.assignTagToParts(tag, ids);
    QString saveErr;
    if (!m_meta.save(&saveErr)) {
        QMessageBox::warning(this, tr("Assign Tag"),
                             tr("The tag was applied in memory but could not be saved:\n%1").arg(saveErr));
    }
    // Reflect the (possibly new) tag in the combobox for when the user switches to listing mode.
    rebuildTagCombo(tag);
    // Refresh the part-info panel so the newly assigned tag shows immediately for the selection.
    if (m_partInfoPanel && m_partInfoPanel->isVisible() && m_view && m_view->selectionModel()) {
        QModelIndexList const sel = m_view->selectionModel()->selectedIndexes();
        if (!sel.isEmpty() && sel.first().isValid())
            showPartInfo(m_model->idAt(sel.first().row()));
    }
    QMessageBox::information(this, tr("Assign Tag"),
                             tr("Tag \"%1\" assigned to %n part(s).", "", ids.size()).arg(tag));
}

void SimilarityIndexPanel::onRemoveTag()
{
    if (!m_indexOpen || m_busy || isSearchMode())
        return;
    QString const tag = currentTagFilter();
    if (tag.isEmpty())
        return; // "All" selected: nothing to remove.

    int const count = m_meta.partsForTag(tag).size();
    if (QMessageBox::question(
            this, tr("Remove Tag"),
            tr("Remove the tag \"%1\" from %n part(s)?", "", count).arg(tag),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    m_meta.removeTag(tag);
    QString saveErr;
    if (!m_meta.save(&saveErr)) {
        QMessageBox::warning(this, tr("Remove Tag"),
                             tr("The tag was removed in memory but could not be saved:\n%1").arg(saveErr));
    }
    // The removed tag disappears from the combobox; reset to "All" and re-show the full listing.
    rebuildTagCombo(QString());
    applyTagFilter();
}

void SimilarityIndexPanel::onEditPartTags()
{
    if (!m_indexOpen || m_busy)
        return;
    QString const partId = m_partInfoId;
    if (partId.isEmpty())
        return;

    // Pre-fill with the part's current tags as a comma-separated list; the user edits the whole set
    // at once (add by typing a new name, remove by deleting it). Empty result clears all tags.
    QStringList const current = m_meta.tagsForPart(partId);
    bool ok = false;
    QString const text = QInputDialog::getText(
        this, tr("Edit Tags"),
        tr("Comma-separated tags for this part:"),
        QLineEdit::Normal, current.join(QStringLiteral(", ")), &ok);
    if (!ok)
        return;

    QStringList tags;
    for (const QString& t : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QString const tt = t.trimmed();
        if (!tt.isEmpty() && !tags.contains(tt, Qt::CaseInsensitive))
            tags << tt;
    }

    m_meta.setTagsForPart(partId, tags);
    QString saveErr;
    if (!m_meta.save(&saveErr)) {
        QMessageBox::warning(this, tr("Edit Tags"),
                             tr("The tags were changed in memory but could not be saved:\n%1").arg(saveErr));
    }
    // Keep the combobox in sync (a tag may have been created or become unused) and refresh the
    // part panel so the new tag set shows immediately.
    rebuildTagCombo(currentTagFilter());
    showPartInfo(partId);
}

void SimilarityIndexPanel::applyThresholdFilter()
{
    double const thr = thresholdValue();
    QVector<SimHit> shown;
    shown.reserve(m_searchHits.size());
    for (const SimHit& h : m_searchHits) {
        if (h.score >= thr)
            shown.push_back(h);
    }
    populateHits(shown, /*showScore=*/true);
}

void SimilarityIndexPanel::onThresholdChanged(int value)
{
    if (m_thresholdValueLabel)
        m_thresholdValueLabel->setText(QStringLiteral("%1").arg(value / kThreshScale, 0, 'f', 3));
    // Re-filter the cached hits (no new bridge search). Only meaningful in search mode. Coalesce
    // the rapid slider ticks with a short debounce so dragging stays smooth on large result sets.
    if (isSearchMode() && !m_searchHits.isEmpty())
        m_thresholdDebounce->start();
    else if (isSearchMode())
        m_model->clearRows();
}

void SimilarityIndexPanel::onCloseIndex()
{
    if (m_busy)
        return;
    QString message;
    if (!HoopsAiIndex::close(message)) {
        showBridgeError(this, tr("Close Index"), message);
        return;
    }
    m_indexOpen = false;
    m_indexFaissPath.clear();
    m_model->clearRows();
    m_searchHits.clear();
    m_listHits.clear();
    m_meta.clear();
    rebuildTagCombo();
    clearIndexInfo();
    m_searchAction->setChecked(false);
    m_assemblySearchAction->setChecked(false);
    setThresholdBarVisible(false);
    setTagBarVisible(false);
    setWindowTitle(tr("Similarity Search"));
    updateActionState();
}

void SimilarityIndexPanel::onItemActivated(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    QString const cadPath = index.data(Qt::UserRole).toString();
    if (!cadPath.isEmpty())
        emit loadCadRequested(cadPath);
}

void SimilarityIndexPanel::onAddFinished(bool success, const QString& message)
{
    endBusy();
    if (!success) {
        showBridgeError(this, tr("Add to Index"), message);
        return;
    }
    // On success a non-empty message is a non-fatal warning (e.g. thumbnail render failed).
    if (!message.isEmpty()) {
        QMessageBox::warning(this, tr("Add to Index"), message);
    } else {
        QMessageBox::information(this, tr("Add to Index"),
                                 tr("Model added to the index."));
    }
    // In OFF (listing) mode, refresh so the newly added part appears.
    refreshIndexInfo();
    if (!isSearchMode())
        refreshResults();
}

void SimilarityIndexPanel::onSearchFinished(bool success, const QVector<SimHit>& hits,
                                            const QString& message, qint64 elapsedMs)
{
    endBusy();
    if (!success) {
        showBridgeError(this, tr("Search Index"), message);
        return;
    }

    bool const assembly = m_assemblySearchAction && m_assemblySearchAction->isChecked();
    // Bridge round-trip time (marshaling + Python search_by_shape / assembly matcher). Compare this
    // against the tutorial's bare search_by_shape timing to see the native-bridge overhead.
    emit statusMessage(tr("%1 search: %2 ms (bridge, incl. overhead) — %3 hits")
                           .arg(assembly ? tr("Assembly") : tr("Part"))
                           .arg(elapsedMs)
                           .arg(hits.size()),
                       0);

    // Cache the full hit pool so the similarity slider can re-filter it without re-searching.
    m_searchHits = hits;
    setTagBarVisible(false);
    // Both part and assembly search share the similarity slider (0.400 E.000) to filter the cached
    // scores.
    setThresholdBarVisible(true);
    applyThresholdFilter();
    // Assign Tag becomes available once there is a displayed cluster.
    updateActionState();

    if (hits.isEmpty()) {
        QMessageBox::information(this, tr("Search Index"),
                                 assembly ? tr("No similar assemblies were found.")
                                          : tr("No similar parts were found."));
    }
}

void SimilarityIndexPanel::onListPartsFinished(bool success, const QVector<SimHit>& hits, const QString& message)
{
    endBusy();
    if (!success) {
        showBridgeError(this, tr("Index Parts"), message);
        return;
    }
    // Cache the full listing so the tag combobox can re-filter it without re-listing, then show it
    // through the current tag filter. No popup here (the listing refreshes silently).
    m_listHits = hits;
    applyTagFilter();
    // The gallery was just rebuilt (e.g. after releasing a search): if the model currently open in
    // the 3D view is in the index, select it and scroll it into view so the user keeps their place.
    // Defer to the next event-loop cycle: right after a model reset the IconMode view has not laid
    // out its items yet, so an immediate scrollTo would be a no-op.
    QTimer::singleShot(0, this, [this]() { selectCurrentModelInList(); });
}

void SimilarityIndexPanel::populateHits(const QVector<SimHit>& hits, bool showScore)
{
    // Hand the rows to the gallery model; thumbnails are decoded lazily and asynchronously as the
    // view requests visible rows, so even tens of thousands of hits populate without blocking. The
    // per-row Part/Assembly line is carried on SimHit::kind (set only for browse listings; search
    // results are homogeneous, so they leave it empty and show no kind line).
    m_model->setRows(hits, showScore);
}

void SimilarityIndexPanel::selectCurrentModelInList()
{
    // Only meaningful for the browse listing (search results are ordered by score, not the index).
    if (!m_view || !m_model || m_currentCadPath.isEmpty() || isSearchMode())
        return;
    int const row = m_model->rowForPath(m_currentCadPath);
    if (row < 0)
        return; // The open model is not in the index: leave the gallery unselected.
    QModelIndex const index = m_model->index(row, 0);
    m_view->setCurrentIndex(index);
    m_view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    // The gallery uses QListView::Batched layout, so a row far down the list is not positioned yet
    // right after a reset; scrolling to it must wait until the batch that contains it is laid out.
    scrollToRowWhenReady(row, ++m_scrollToken, 0);
}

void SimilarityIndexPanel::scrollToRowWhenReady(int row, int token, int attempt)
{
    // Abort if a newer listing/scroll request superseded this one, or the panel switched to search.
    if (!m_view || !m_model || token != m_scrollToken || isSearchMode())
        return;
    if (row < 0 || row >= m_model->rowCount())
        return;
    QModelIndex const index = m_model->index(row, 0);
    m_view->scrollTo(index, QAbstractItemView::PositionAtCenter);
    // scrollTo only reaches rows the batched layout has already placed. Until then visualRect() is
    // empty; keep retrying on later event-loop cycles (batches progress each cycle) up to a bound.
    QRect const rect = m_view->visualRect(index);
    bool const placed = rect.isValid() && !rect.isEmpty();
    if (placed || attempt >= 120)
        return;
    QTimer::singleShot(16, this, [this, row, token, attempt]() {
        scrollToRowWhenReady(row, token, attempt + 1);
    });
}
