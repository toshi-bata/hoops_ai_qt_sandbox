#pragma once
#include "QtWidgets/QtWidgets"
#include "sprk.h"

#ifdef USING_EXCHANGE
    #include "sprk_exchange.h"
#endif

using namespace HPS;

class QtComponentTree;
class QtComponentTreeItem;
class ModelBrowserWidget;
class ConfigurationWidget;

using QtComponentTreePtr = std::shared_ptr<QtComponentTree>;

/* A TabWidget contained by a dockable pane.
    The tabs allow the users to switch between the Model Browser
    and the Configuration Browser. */
class TabbedView: public QTabWidget {
    Q_OBJECT

  public:
    TabbedView();
    ModelBrowserWidget* getModelBrowser();
    ConfigurationWidget* getConfigurationBrowser();
  private slots:
    void onCurrentTabChanged(int index);

  private:
    ModelBrowserWidget* modelBrowserWidget;
    ConfigurationWidget* configurationBrowserWidget;
};

/* The ModelBrowserWidget is a tree widget which contains the component structure
    of the loaded model. Information in this browser will only appear when loading models
    through the Exchange, Parasolid or DWG Sprockets, since only those models will contain
    Components.

    From this browser it is possible to inspect the component structure, highlight components
    by clicking on them, and to isolate, hide and show components through a context menu, activated
    by right clicking on a component.

    The original model visibility can be restored through the Reset Visibility option of the context menu.

    The tree is populated dynamically as nodes as expanded.
    Currently only some of the component types, those deemed most interesting, are represented by the Model Browser
    (for example, individual faces, edges and vertices are not represented, even though they all have associated components). */
class ModelBrowserWidget: public QTreeWidget {
    Q_OBJECT;

  public:
    ModelBrowserWidget(QWidget* parent = nullptr);

  public slots:
    /* Initializes the Model Browser based on the CADModel that was loaded in the sandbox. */
    void Init(CADModel const& cadModel, Canvas const& canvas);

  private slots:
    void onItemExpanding(QTreeWidgetItem* item);
    void onItemCollapsing(QTreeWidgetItem* item);
    void onItemClicked(QTreeWidgetItem* item, int column);
    void repaintWidget();
    void onContextMenu(QPoint const& point);
    void onIsolate();
    void onShowHide();
    void onResetVisibility();

  signals:
    void needsRepaint();

  private:
    /* Removes all items from the Model Browser. */
    void Flush();

    void showContextMenu(QTreeWidgetItem* item, QPoint const& position);
    QtComponentTreePtr componentTree;
    QtComponentTreeItem* contextItem;
};

/* QtComponentTree, derived from HPS::ComponentTree, provides Visualize related
    functionalities that affect the whole Model Browser tree, such as:
    - providing a Highlight style to use when selecting a node,
    - re-expanding the tree to a specified state and
    - flushing the tree of nodes. */
class QtComponentTree: public ComponentTree {
  public:
    QtComponentTree(Canvas const& in_canvas, ModelBrowserWidget* in_widget);

    ModelBrowserWidget* GetWidget();

    virtual void Flush() override;

  private:
    ModelBrowserWidget* widget;
};

/* QtComponentTreeItem, derived from HPS::ComponentTreeItem, provides Visualize related
    functionalities that affect a single node in the Model Browser tree, such as:
    - checking if an item is hidden
    - checking is an item is highlighted
    - populating / flushing part of the tree as nodes are expanded and collapsed
    - detecting when a node is hidden or shown
    - access to the Component associated with this node, and its associated ComponentPath */
class QtComponentTreeItem: public ComponentTreeItem {
  public:
    QtComponentTreeItem(ComponentTreePtr const& in_tree, CADModel const& in_cad_model);
    QtComponentTreeItem(ComponentTreePtr const& in_tree, Component const& in_component, ComponentTree::ItemType in_type);

    virtual ComponentTreeItemPtr AddChild(Component const& in_component, ComponentTree::ItemType in_type);

    void ExpandInternal();
    virtual void Expand() override;
    virtual void Collapse() override;

    virtual void OnHighlight(HighlightOptionsKit const& in_options) override;
    virtual void OnUnhighlight(HighlightOptionsKit const& in_options) override;

    virtual void OnHide() override;
    virtual void OnShow() override;

  private:
    QIcon GetIcon(ComponentTreeItem* item, bool visible);

    inline void SetTreeItem(QTreeWidgetItem* item) { treeItem = item; }

    inline ModelBrowserWidget* GetTreeWidget() const;
    QTreeWidgetItem* treeItem;
};

Q_DECLARE_METATYPE(QtComponentTreeItem*);

/* The ConfigurationWidget is a tree Widget which lists the available Configurations for the loaded CADModel.
    At the moment, only SolidWorks, CATIA-V4 and I-DEAS files support configurations.
    Double clicking on a configuration will re-import the current file using the selected configuration. */
class ConfigurationWidget: public QTreeWidget {
    Q_OBJECT

  public:
    ConfigurationWidget(QWidget* parent = nullptr);

  public slots:
    void Init(CADModel const& cadModel);

  private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

  private:
    void Flush();

#if defined(USING_EXCHANGE)
    void insertConfigurationInTree(QTreeWidgetItem* root, Exchange::Configuration const& configuration);
    QTreeWidgetItem* findConfigurationInTree(UTF8Array const& configuration, QTreeWidgetItem const* root);
    UTF8Array getSelectedConfiguration();
#endif
};