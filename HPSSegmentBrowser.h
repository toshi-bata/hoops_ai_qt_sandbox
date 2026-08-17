#pragma once
#include "QtWidgets/QtWidgets"
#include "qcombobox.h"
#include "qcheckbox.h"
#include "sprk.h"

using namespace HPS;

template<typename T>
struct Hasher {
    inline size_t operator()(T const& key) const
    {
        size_t hash = (size_t)key;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash = hash ^ (hash >> 15);
        return hash;
    }
};

class SegmentBrowserTree;

class QtSceneTree: public HPS::SceneTree {
  public:
    QtSceneTree(Canvas const& in_canvas, SegmentBrowserTree* in_tree);

    SegmentBrowserTree* getTreeWidget() const { return tree; }

    void Flush() override;

  private:
    SegmentBrowserTree* tree;
};

class QtSceneTreeItem: public HPS::SceneTreeItem {
  public:
    QtSceneTreeItem(SceneTreePtr const& in_tree, Model const& in_model);
    QtSceneTreeItem(SceneTreePtr const& in_tree, View const& in_view);
    QtSceneTreeItem(SceneTreePtr const& in_tree, Layout const& in_layout);
    QtSceneTreeItem(SceneTreePtr const& in_tree, Canvas const& in_canvas);
    QtSceneTreeItem(SceneTreePtr const& in_tree, Key const& in_key, SceneTree::ItemType in_type, char const* in_title = nullptr);

    virtual ~QtSceneTreeItem();

    SceneTreeItemPtr AddChild(Key const& in_key, SceneTree::ItemType in_type, char const* in_title) override;
    void Expand() override;
    void Collapse() override;
    void Select() override;
    void Unselect() override;

    QTreeWidgetItem* getTreeItem() { return treeItem; }

    SegmentBrowserTree* getTreeWidget();

  private:
    enum SelectionState {
        Selected,
        Unselected,
    };

    QIcon getIcon(SceneTreeItem const& item, SelectionState state);

    inline void setTreeItem(QTreeWidgetItem* item) { treeItem = item; }

    QTreeWidgetItem* treeItem;
};

Q_DECLARE_METATYPE(QtSceneTreeItem*);

class SegmentBrowserTree: public QTreeWidget {
    Q_OBJECT;

  public:
    SegmentBrowserTree(QWidget* parent = nullptr);

    inline std::shared_ptr<QtSceneTree> getSceneTreePtr() { return sceneTree; }

    void removeChildren(QTreeWidgetItem* item);

  public slots:
    void Init(Canvas const& in_canvas);

  private slots:
    void onItemExpanding(QTreeWidgetItem* item);
    void onItemCollapsing(QTreeWidgetItem* item);
    void repaintWidget();
    void onItemDblClicked(QTreeWidgetItem* item, int column);
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* prev);
    void onContextMenu(QPoint const& point);

    void onAddMaterial();
    void onAddCamera();
    void onAddModellingMatrix();
    void onAddTextureMatrix();
    void onAddCulling();
    void onAddCurveAttribute();
    void onAddCylinderAttribute();
    void onAddEdgeAttribute();
    void onAddLightingAttribute();
    void onAddLineAttribute();
    void onAddMarkerAttribute();
    void onAddSurfaceAttribute();
    void onAddSelectability();
    void onAddSphereAttribute();
    void onAddSubwindow();
    void onAddTextAttribute();
    void onAddTransparency();
    void onAddVisibility();
    void onAddVisualEffects();
    void onAddPerformance();
    void onAddDrawingAttribute();
    void onAddHiddenLineAttribute();
    void onAddMaterialPalette();
    void onAddContourLine();
    void onAddCondition();
    void onAddBounding();
    void onAddAttributeLock();
    void onAddTransformMask();
    void onAddColorInterpolation();
    void onAddCuttingSectionAttribute();
    void onAddPriority();
    void onUnsetAttribute();

  signals:
    void needsRepaint();

  private:
    void showContextMenu(QTreeWidgetItem* item, QPoint const& position);
    void keyPressEvent(QKeyEvent* event) override;
    std::shared_ptr<QtSceneTree> sceneTree;
    std::unordered_map<SceneTree::ItemType const, Search::Type, Hasher<SceneTree::ItemType const>> searchTypeMap;
    QtSceneTreeItem* contextItem;
    QMenu segmentContextMenu;
    QMenu attributeContextMenu;
};

class SegmentBrowserWidget: public QWidget {
    Q_OBJECT;

  public:
    SegmentBrowserWidget(QWidget* parent = nullptr);

    SegmentBrowserTree* getSegmentTree() { return tree; }

    QComboBox* getComboBox() { return comboBox; }

  private slots:
    void onComboBoxSelectionChanged(int inSelectedIndex);
    void onCheckBoxClicked(bool checked);

  private:
    enum Root {
        Model = 0,
        View,
        Layout,
        Canvas,
    };

    QComboBox* comboBox;
    QCheckBox* checkBox;
    SegmentBrowserTree* tree;
};
