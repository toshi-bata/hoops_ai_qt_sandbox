#include "HPSSegmentBrowser.h"
#include "HPSMainWindow.h"
#include "HPSWidget.h"
#include "HPSPropertyBrowser.h"
#include <assert.h>

namespace {
    HPSWidget* getMainWidget()
    {
        QWidgetList list = qApp->topLevelWidgets();
        for (auto const& one_widget: list) {
            if (one_widget->inherits("HPSMainWindow")) {
                HPSMainWindow* mw = (HPSMainWindow*)one_widget;
                return (HPSWidget*)mw->centralWidget();
            }
        }

        return nullptr;
    }

    HPSMainWindow* getMainWindow()
    {
        QWidgetList list = qApp->topLevelWidgets();
        for (auto const& one_widget: list) {
            if (one_widget->inherits("HPSMainWindow"))
                return (HPSMainWindow*)one_widget;
        }

        return nullptr;
    }
} // namespace

QtSceneTree::QtSceneTree(Canvas const& in_canvas, SegmentBrowserTree* in_tree): HPS::SceneTree(in_canvas), tree(in_tree) {}

void QtSceneTree::Flush()
{
    SceneTree::Flush();
    getTreeWidget()->clear();
}

QtSceneTreeItem::QtSceneTreeItem(SceneTreePtr const& in_tree, Model const& in_model):
    HPS::SceneTreeItem(in_tree, in_model), treeItem(nullptr)
{
}

QtSceneTreeItem::QtSceneTreeItem(SceneTreePtr const& in_tree, View const& in_view):
    HPS::SceneTreeItem(in_tree, in_view), treeItem(nullptr)
{
}

QtSceneTreeItem::QtSceneTreeItem(SceneTreePtr const& in_tree, Layout const& in_layout):
    HPS::SceneTreeItem(in_tree, in_layout), treeItem(nullptr)
{
}

QtSceneTreeItem::QtSceneTreeItem(SceneTreePtr const& in_tree, Canvas const& in_canvas):
    HPS::SceneTreeItem(in_tree, in_canvas), treeItem(nullptr)
{
}

QtSceneTreeItem::QtSceneTreeItem(SceneTreePtr const& in_tree,
                                 Key const& in_key,
                                 SceneTree::ItemType in_type,
                                 char const* in_title):
    HPS::SceneTreeItem(in_tree, in_key, in_type, in_title),
    treeItem(nullptr)
{
}

QtSceneTreeItem::~QtSceneTreeItem() {}

SceneTreeItemPtr QtSceneTreeItem::AddChild(Key const& in_key, SceneTree::ItemType in_type, char const* in_title)
{
    auto child = std::make_shared<QtSceneTreeItem>(GetTree(), in_key, in_type, in_title);

    QTreeWidgetItem* child_item = new QTreeWidgetItem();
    child_item->setText(0, child->GetTitle().GetBytes());
    child_item->setChildIndicatorPolicy(child->HasChildren() ? QTreeWidgetItem::ChildIndicatorPolicy::ShowIndicator :
                                                               QTreeWidgetItem::ChildIndicatorPolicy::DontShowIndicator);
    child_item->setIcon(0, getIcon(*child, SelectionState::Unselected));
    QVariant data_in;
    data_in.setValue(child.get());
    child_item->setData(0, Qt::ItemDataRole::UserRole, data_in);

    if (treeItem == nullptr)
        getTreeWidget()->insertTopLevelItem(0, child_item);
    else
        treeItem->addChild(child_item);

    child->setTreeItem(child_item);

    return child;
}

void QtSceneTreeItem::Expand()
{
    SegmentBrowserTree* tree = getTreeWidget();
    if (tree != nullptr) {
        if (treeItem == nullptr)
            SceneTreeItem::Expand();
        else if (!treeItem->isExpanded()) {
            SceneTreeItem::Expand();
            treeItem->setData(0, Qt::ItemDataRole::UserRole + 1, QVariant(true));
            treeItem->setExpanded(true);
        }
        else if (treeItem->isExpanded())
            SceneTreeItem::Expand();
    }
}

void QtSceneTreeItem::Collapse() { SceneTreeItem::Collapse(); }

void QtSceneTreeItem::Select()
{
    SceneTreeItem::Select();

    if (treeItem) {
        QFont font = treeItem->font(0);
        font.setBold(true);
        treeItem->setFont(0, font);
        treeItem->setIcon(0, getIcon(*this, SelectionState::Selected));

        emit getTreeWidget()->needsRepaint();
    }
}

void QtSceneTreeItem::Unselect()
{
    SceneTreeItem::Unselect();

    if (treeItem) {
        QFont font = treeItem->font(0);
        font.setBold(false);
        treeItem->setFont(0, font);
        treeItem->setIcon(0, getIcon(*this, SelectionState::Unselected));

        emit getTreeWidget()->needsRepaint();
    }
}

QIcon QtSceneTreeItem::getIcon(SceneTreeItem const& item, SelectionState state)
{
    QString filename("imgUnknownIcon");
    SceneTree::ItemType itemType = item.GetItemType();

    switch (itemType) {
        case SceneTree::ItemType::Segment: {
            if (state == SelectionState::Unselected)
                filename = "segmentIcon";
            else
                filename = "highlightedSegmentIcon";
        } break;

        case SceneTree::ItemType::AttributeFilter:
        case SceneTree::ItemType::ConditionalExpression:
        case SceneTree::ItemType::NamedStyle:
        case SceneTree::ItemType::SegmentStyle:
        case SceneTree::ItemType::StyleGroup:
        case SceneTree::ItemType::Include:
        case SceneTree::ItemType::IncludeGroup:
        case SceneTree::ItemType::Portfolio:
        case SceneTree::ItemType::PortfolioGroup: {
            filename = "includeIcon";
        } break;

        case SceneTree::ItemType::StaticModelSegment: {
            filename = "staticModelIcon";
        } break;

        case SceneTree::ItemType::Reference: {
            filename = "referenceIcon";
        } break;

        default: {
            if (item.HasItemType(SceneTree::ItemType::Attribute) || itemType == SceneTree::ItemType::AttributeGroup)
                filename = "attributeIcon";
            else if (item.HasItemType(SceneTree::ItemType::Geometry) || itemType == SceneTree::ItemType::GeometryGroup ||
                     item.HasItemType(SceneTree::ItemType::Definition) ||
                     item.HasItemType(SceneTree::ItemType::DefinitionGroup)) {
                if (state == SelectionState::Unselected)
                    filename = "geometryIcon";
                else
                    filename = "highlightedGeometryIcon";
            }
            else if (item.HasItemType(SceneTree::ItemType::Group)) {
                if (state == SelectionState::Unselected)
                    filename = "groupIcon";
                else
                    filename = "highlightedGroupIcon";
            }
        }
    }

    return QIcon(":/HPSMainWindow/" + filename);
}

SegmentBrowserTree* QtSceneTreeItem::getTreeWidget()
{
    SceneTreePtr tree = GetTree();
    if (tree != nullptr)
        return std::static_pointer_cast<QtSceneTree>(tree)->getTreeWidget();
    return nullptr;
}

SegmentBrowserTree::SegmentBrowserTree(QWidget* parent): QTreeWidget(parent), contextItem(nullptr)
{
    setColumnCount(1);
    setHeaderHidden(true);
    setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);

    QTreeWidgetItem* item = new QTreeWidgetItem(static_cast<QTreeWidget*>(nullptr), QStringList(QString("No Segments")));
    insertTopLevelItem(0, item);

    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), SLOT(onItemExpanding(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), SLOT(onItemCollapsing(QTreeWidgetItem*)));
    connect(this, SIGNAL(needsRepaint()), SLOT(repaintWidget()));
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), SLOT(onItemDblClicked(QTreeWidgetItem*, int)));
    connect(this, SIGNAL(customContextMenuRequested(QPoint const&)), SLOT(onContextMenu(QPoint const&)));
    connect(this,
            SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
            SLOT(onCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)));

    searchTypeMap[SceneTree::ItemType::CuttingSectionGroup] = Search::Type::CuttingSection;
    searchTypeMap[SceneTree::ItemType::ShellGroup] = Search::Type::Shell;
    searchTypeMap[SceneTree::ItemType::MeshGroup] = Search::Type::Mesh;
    searchTypeMap[SceneTree::ItemType::GridGroup] = Search::Type::Grid;
    searchTypeMap[SceneTree::ItemType::NURBSSurfaceGroup] = Search::Type::NURBSSurface;
    searchTypeMap[SceneTree::ItemType::CylinderGroup] = Search::Type::Cylinder;
    searchTypeMap[SceneTree::ItemType::SphereGroup] = Search::Type::Sphere;
    searchTypeMap[SceneTree::ItemType::PolygonGroup] = Search::Type::Polygon;
    searchTypeMap[SceneTree::ItemType::CircleGroup] = Search::Type::Circle;
    searchTypeMap[SceneTree::ItemType::CircularWedgeGroup] = Search::Type::CircularWedge;
    searchTypeMap[SceneTree::ItemType::EllipseGroup] = Search::Type::Ellipse;
    searchTypeMap[SceneTree::ItemType::LineGroup] = Search::Type::Line;
    searchTypeMap[SceneTree::ItemType::NURBSCurveGroup] = Search::Type::NURBSCurve;
    searchTypeMap[SceneTree::ItemType::CircularArcGroup] = Search::Type::CircularArc;
    searchTypeMap[SceneTree::ItemType::EllipticalArcGroup] = Search::Type::EllipticalArc;
    searchTypeMap[SceneTree::ItemType::InfiniteLineGroup] = Search::Type::InfiniteLine;
    searchTypeMap[SceneTree::ItemType::InfiniteRayGroup] = Search::Type::InfiniteRay;
    searchTypeMap[SceneTree::ItemType::MarkerGroup] = Search::Type::Marker;
    searchTypeMap[SceneTree::ItemType::TextGroup] = Search::Type::Text;
    searchTypeMap[SceneTree::ItemType::ReferenceGroup] = Search::Type::Reference;
    searchTypeMap[SceneTree::ItemType::DistantLightGroup] = Search::Type::DistantLight;
    searchTypeMap[SceneTree::ItemType::SpotlightGroup] = Search::Type::Spotlight;

    // setup segment context menu
    QMenu* addAttributeMenu = segmentContextMenu.addMenu("Add Attribute");
    addAttributeMenu->addAction("Material", this, SLOT(onAddMaterial()));
    addAttributeMenu->addAction("Camera", this, SLOT(onAddCamera()));
    addAttributeMenu->addAction("Modelling Matrix", this, SLOT(onAddModellingMatrix()));
    addAttributeMenu->addAction("Texture Matrix", this, SLOT(onAddTextureMatrix()));
    addAttributeMenu->addAction("Culling", this, SLOT(onAddCulling()));
    addAttributeMenu->addAction("Curve Attribute", this, SLOT(onAddCurveAttribute()));
    addAttributeMenu->addAction("Cylinder Attribute", this, SLOT(onAddCylinderAttribute()));
    addAttributeMenu->addAction("Edge Attribute", this, SLOT(onAddEdgeAttribute()));
    addAttributeMenu->addAction("Lighting Attribute", this, SLOT(onAddLightingAttribute()));
    addAttributeMenu->addAction("Line Attribute", this, SLOT(onAddLineAttribute()));
    addAttributeMenu->addAction("Marker Attribute", this, SLOT(onAddMarkerAttribute()));
    addAttributeMenu->addAction("Surface Attribute", this, SLOT(onAddSurfaceAttribute()));
    addAttributeMenu->addAction("Selectability", this, SLOT(onAddSelectability()));
    addAttributeMenu->addAction("Sphere Attribute", this, SLOT(onAddSphereAttribute()));
    addAttributeMenu->addAction("Subwindow", this, SLOT(onAddSubwindow()));
    addAttributeMenu->addAction("Text Attribute", this, SLOT(onAddTextAttribute()));
    addAttributeMenu->addAction("Transparency", this, SLOT(onAddTransparency()));
    addAttributeMenu->addAction("Visibility", this, SLOT(onAddVisibility()));
    addAttributeMenu->addAction("Visual Effects", this, SLOT(onAddVisualEffects()));
    addAttributeMenu->addAction("Performance", this, SLOT(onAddPerformance()));
    addAttributeMenu->addAction("Drawing Attribute", this, SLOT(onAddDrawingAttribute()));
    addAttributeMenu->addAction("Hidden Line Attribute", this, SLOT(onAddHiddenLineAttribute()));
    addAttributeMenu->addAction("Material Palette", this, SLOT(onAddMaterialPalette()));
    addAttributeMenu->addAction("Contour Line", this, SLOT(onAddContourLine()));
    addAttributeMenu->addAction("Condition", this, SLOT(onAddCondition()));
    addAttributeMenu->addAction("Bounding", this, SLOT(onAddBounding()));
    addAttributeMenu->addAction("Attribute Lock", this, SLOT(onAddAttributeLock()));
    addAttributeMenu->addAction("Transform Mask", this, SLOT(onAddTransformMask()));
    addAttributeMenu->addAction("Color Interpolation", this, SLOT(onAddColorInterpolation()));
    addAttributeMenu->addAction("Cutting Section Attribute", this, SLOT(onAddCuttingSectionAttribute()));
    addAttributeMenu->addAction("Priority", this, SLOT(onAddPriority()));

    // setup attribute context menu
    attributeContextMenu.addAction("Unset", this, SLOT(onUnsetAttribute()));
}

void SegmentBrowserTree::Init(Canvas const& in_canvas)
{
    sceneTree = std::make_shared<QtSceneTree>(in_canvas, this);

    SegmentBrowserWidget* widget = reinterpret_cast<SegmentBrowserWidget*>(parentWidget());
    emit widget->getComboBox()->currentIndexChanged(0);
}

void SegmentBrowserTree::onItemExpanding(QTreeWidgetItem* item)
{
    QVariant shouldSkipExpand = item->data(0, Qt::ItemDataRole::UserRole + 1);
    if (shouldSkipExpand.isValid() && shouldSkipExpand.value<bool>()) {
        item->setData(0, Qt::ItemDataRole::UserRole + 1, QVariant(false));
        return;
    }

    item->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>()->Expand();
}

void SegmentBrowserTree::removeChildren(QTreeWidgetItem* item)
{
    std::vector<QTreeWidgetItem*> children;
    for (int i = 0; i < item->childCount(); ++i)
        children.push_back(item->child(i));
    for (auto one_child: children)
        item->removeChild(one_child);
}

void SegmentBrowserTree::onItemCollapsing(QTreeWidgetItem* item)
{
    item->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>()->Collapse();
    removeChildren(item);
}

void SegmentBrowserTree::repaintWidget() { viewport()->update(); }

void SegmentBrowserTree::onItemDblClicked(QTreeWidgetItem* item, int column)
{
    HPS_UNREFERENCED(column);
    if (item == nullptr)
        return;

    QtSceneTreeItem* sceneItem = item->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>();
    if (sceneItem == nullptr)
        return;

    if (sceneItem->IsHighlighted())
        sceneItem->Unhighlight();
    else
        sceneItem->Highlight();
}

void SegmentBrowserTree::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* prev)
{
    HPS_UNREFERENCED(prev);
    if (current) {
        HPSMainWindow* mw = getMainWindow();
        QtSceneTreeItem* sceneItem = current->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>();
        mw->addProperty(sceneItem, SceneTree::ItemType::None);
    }
}

void SegmentBrowserTree::onContextMenu(QPoint const& point)
{
    QTreeWidgetItem* item = itemAt(point);

    if (item != nullptr)
        showContextMenu(item, viewport()->mapToGlobal(point));
}

void SegmentBrowserTree::showContextMenu(QTreeWidgetItem* item, QPoint const& position)
{
    contextItem = item->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>();
    if (contextItem != nullptr) {
        SceneTree::ItemType itemType = contextItem->GetItemType();
        if (itemType == SceneTree::ItemType::Segment)
            segmentContextMenu.exec(position);
        else if (contextItem->HasItemType(SceneTree::ItemType::Attribute) && itemType != SceneTree::ItemType::Portfolio &&
                 !contextItem->GetKey().HasType(HPS::Type::WindowKey))
            attributeContextMenu.exec(position);
        else {
            contextItem = nullptr;
            return;
        }
    }
}

void SegmentBrowserTree::onAddMaterial()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Material));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddCamera()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Camera));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddModellingMatrix()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::ModellingMatrix));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddTextureMatrix()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::TextureMatrix));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddCulling()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Culling));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddCurveAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::CurveAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddCylinderAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::CylinderAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddEdgeAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::EdgeAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddLightingAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::LightingAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddLineAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::LineAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddMarkerAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::MarkerAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddSurfaceAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::SurfaceAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddSelectability()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Selectability));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddSphereAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::SphereAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddSubwindow()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Subwindow));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddTextAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::TextAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddTransparency()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Transparency));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddVisibility()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Visibility));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddVisualEffects()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::VisualEffects));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddPerformance()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Performance));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddDrawingAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::DrawingAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddHiddenLineAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::HiddenLineAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddMaterialPalette()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::MaterialPalette));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddContourLine()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::ContourLine));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddCondition()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Condition));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddBounding()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Bounding));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddAttributeLock()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::AttributeLock));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddTransformMask()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::TransformMask));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddColorInterpolation()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::ColorInterpolation));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddCuttingSectionAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::CuttingSectionAttribute));
    contextItem = nullptr;
}

void SegmentBrowserTree::onAddPriority()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw,
                              "addProperty",
                              Qt::AutoConnection,
                              Q_ARG(QtSceneTreeItem*, contextItem),
                              Q_ARG(HPS::SceneTree::ItemType, SceneTree::ItemType::Priority));
    contextItem = nullptr;
}

void SegmentBrowserTree::onUnsetAttribute()
{
    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw, "unsetAttribute", Qt::AutoConnection, Q_ARG(QtSceneTreeItem*, contextItem));
    contextItem = nullptr;
}

void SegmentBrowserTree::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        QList<QTreeWidgetItem*> items = selectedItems();
        if (!items.empty()) {
            QtSceneTreeItem* selectedItem = items[0]->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>();
            if (selectedItem != nullptr) {
                SceneTree::ItemType itemType = selectedItem->GetItemType();

                auto getRelatives = [](QTreeWidgetItem* selectedItem,
                                       QTreeWidgetItem*& outParent,
                                       QTreeWidgetItem*& outSibling,
                                       QTreeWidgetItem*& outPrevSibling) {
                    outParent = selectedItem->parent();
                    if (outParent != nullptr) {
                        int index = outParent->indexOfChild(selectedItem);
                        if (index == 0)
                            outPrevSibling = nullptr;
                        else
                            outPrevSibling = outParent->child(index - 1);

                        if (index == outParent->childCount() - 1)
                            outSibling = nullptr;
                        else
                            outSibling = outParent->child(index + 1);
                    }
                    else {
                        outSibling = nullptr;
                        outPrevSibling = nullptr;
                    }
                };

                auto deleteSelectedItem = [](QTreeWidgetItem* selectedItem,
                                             QTreeWidgetItem*& parent,
                                             QTreeWidgetItem*& prevSibling,
                                             QTreeWidgetItem*& nextSibling) {
                    if (nextSibling != nullptr)
                        nextSibling->setSelected(true);
                    else if (prevSibling != nullptr)
                        prevSibling->setSelected(true);
                    else
                        parent->setSelected(true);

                    parent->removeChild(selectedItem);
                };

                QTreeWidgetItem* parent = nullptr;
                QTreeWidgetItem* sibling = nullptr;
                QTreeWidgetItem* prevSibling = nullptr;
                if (itemType == SceneTree::ItemType::Segment) {
                    getRelatives(items[0], parent, sibling, prevSibling);
                    if (parent == nullptr)
                        return;

                    selectedItem->GetKey().Delete();

                    deleteSelectedItem(items[0], parent, prevSibling, sibling);
                    emit needsRepaint();
                    getMainWidget()->getCanvas()->Update();
                }
                else if (selectedItem->HasItemType(SceneTree::ItemType::Geometry)) {
                    getRelatives(items[0], parent, sibling, prevSibling);

                    selectedItem->GetKey().Delete();

                    deleteSelectedItem(items[0], parent, prevSibling, sibling);
                    emit needsRepaint();
                    getMainWidget()->getCanvas()->Update();
                }
                else if (selectedItem->HasItemType(SceneTree::ItemType::GeometryGroupMask)) {
                    getRelatives(items[0], parent, sibling, prevSibling);

                    SegmentKey groupKey(selectedItem->GetKey());
                    SearchResults results;
                    groupKey.Find(searchTypeMap[selectedItem->GetItemType()], Search::Space::SegmentOnly, results);
                    SearchResultsIterator it;
                    while (it.IsValid()) {
                        it.GetItem().Delete();
                        it.Next();
                    }

                    deleteSelectedItem(items[0], parent, prevSibling, sibling);
                    emit needsRepaint();
                    getMainWidget()->getCanvas()->Update();
                }
            }
        }
    }
}

SegmentBrowserWidget::SegmentBrowserWidget(QWidget* parent): QWidget(parent)
{
    comboBox = new QComboBox(this);
    checkBox = new QCheckBox("Properties", this);
    tree = new SegmentBrowserTree(this);

    comboBox->addItem("Model");
    comboBox->addItem("View");
    comboBox->addItem("Layout");
    comboBox->addItem("Canvas");

    QVBoxLayout* layout = new QVBoxLayout(this);
    QHBoxLayout* hLayout = new QHBoxLayout(this);
    hLayout->addWidget(comboBox);
    hLayout->addWidget(checkBox);
    layout->addLayout(hLayout);
    layout->addWidget(tree);
    setLayout(layout);

    connect(comboBox, SIGNAL(currentIndexChanged(int)), SLOT(onComboBoxSelectionChanged(int)));
    connect(checkBox, SIGNAL(clicked(bool)), SLOT(onCheckBoxClicked(bool)));
}

void SegmentBrowserWidget::onComboBoxSelectionChanged(int inSelectedIndex)
{
    HPSWidget* mainWidget = getMainWidget();

    HPS::Canvas canvas = *mainWidget->getCanvas();
    bool hasCanvas = canvas.Type() != Type::None;
    HPS::Layout layout = canvas.GetAttachedLayout();
    bool hasLayout = hasCanvas && layout.Type() != Type::None;
    HPS::View view = (hasLayout && layout.GetLayerCount() > 0 ? layout.GetFrontView() : HPS::View());
    bool hasView = hasLayout && view.Type() != Type::None && view.GetSegmentKey().Type() != Type::None;
    HPS::Model model = (hasView ? view.GetAttachedModel() : HPS::Model());
    bool hasModel = hasView && model.Type() != Type::None && model.GetSegmentKey().Type() != Type::None;

    auto treePtr = tree->getSceneTreePtr();
    SceneTreeItemPtr root;
    switch (inSelectedIndex) {
        case Root::Model: {
            if (hasModel)
                root = std::make_shared<QtSceneTreeItem>(treePtr, model);
        } break;

        case Root::View: {
            if (hasView)
                root = std::make_shared<QtSceneTreeItem>(treePtr, view);
        } break;

        case Root::Layout: {
            if (hasLayout)
                root = std::make_shared<QtSceneTreeItem>(treePtr, layout);
        }

        case Root::Canvas: {
            if (hasCanvas)
                root = std::make_shared<QtSceneTreeItem>(treePtr, canvas);
        } break;

        default:
            assert(false);
    }

    if (root)
        treePtr->SetRoot(root);
    else
        treePtr->Flush();

    HPSMainWindow* mw = getMainWindow();
    QMetaObject::invokeMethod(mw, "flushProperties", Qt::AutoConnection);
}

void SegmentBrowserWidget::onCheckBoxClicked(bool checked)
{
    HPSMainWindow* mw = getMainWindow();
    if (checked)
        mw->getPropertyBrowser()->parentWidget()->show();
    else
        mw->getPropertyBrowser()->parentWidget()->hide();
}
