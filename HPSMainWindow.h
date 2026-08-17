#ifndef HPSMAINWINDOW_H
#define HPSMAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include "HPSWidget.h"

class ModelBrowserWidget;
class ConfigurationWidget;
class SegmentBrowserWidget;
class TabbedView;
class QtSceneTreeItem;
class PropertyWidget;
class SimilarityIndexPanel;

class HPSMainWindow: public QMainWindow {
    Q_OBJECT

  public:
    HPSMainWindow(QWidget* parent = 0);
    ~HPSMainWindow();
    void setCurrentFile(QString const& filename);

    /* Enables the File > Add and Similarity Comparison actions based on how many parts are currently
        loaded: Add requires an open model; Similarity Comparison requires two parts (Open + Add). */
    void updateModelDependentActions();

    ModelBrowserWidget* getModelBrowser();
    ConfigurationWidget* getConfigurationBrowser();
    SegmentBrowserTree* getSegmentBrowser();
    PropertyWidget* getPropertyBrowser();

    // This particular button needs to be updated from the Widget if
    // hidden line is on when the user requests frame rate mode
    QAction* toolbarSmooth;

  public slots:
    void addProperty(QtSceneTreeItem* item, HPS::SceneTree::ItemType itemType);
    void unsetAttribute(QtSceneTreeItem* item);
    void flushProperties();

  private:
    void setupGui();
    QString strippedName(QString const& fullFileName);

    static int const maxRecentFiles = 5;
    QAction* recentFiles[5];
    QAction* fileMenuAdd = nullptr;
    QAction* similarityAction = nullptr;
    QString curFile;

  private slots:
    void quit();
    void openRecentFile();
    void onLoadMfrModel();
    void onLoadSimilarityModel();

  protected:
    HPSWidget* widget;
    TabbedView* modelBrowserAndConfigurations;
    SegmentBrowserWidget* segmentBrowser;
    PropertyWidget* propertyBrowser;
    SimilarityIndexPanel* similarityPanel = nullptr;

    QPaintEngine* paintEngine() const { return 0; }
};

#endif // HPSMAINWINDOW_H
