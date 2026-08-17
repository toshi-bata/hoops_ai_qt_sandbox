#include "HPSPropertyBrowser.h"
#include "HPSSegmentBrowser.h"
#include "HPSWidget.h"
#include "HPSMainWindow.h"

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
} // namespace

Property::BaseComboBox::BaseComboBox(): QComboBox() {}

void Property::BaseComboBox::onIndexChanged(int newIndex)
{
    // should be handled by the derived class
    Q_ASSERT(false);
}

template<typename T>
Property::PropertyComboBox<T>::PropertyComboBox(Property::BaseEnumProperty<T>* base_property):
    Property::BaseComboBox(), base_property(base_property)
{
}

template<typename T>
void Property::PropertyComboBox<T>::connectEvents()
{
    connect(this, SIGNAL(currentIndexChanged(int)), SLOT(onIndexChanged(int)));
}

template<typename T>
void Property::PropertyComboBox<T>::onIndexChanged(int newIndex)
{
    if (base_property->getTypeFromValue()) {
        base_property->enableValidProperties();
        base_property->onChildChanged();
    }
}

Property::BoolComboBox::BoolComboBox(Property::BoolProperty* bool_property):
    Property::BaseComboBox(), bool_property(bool_property)
{
}

void Property::BoolComboBox::connectEvents() { connect(this, SIGNAL(currentIndexChanged(int)), SLOT(onIndexChanged(int))); }

void Property::BoolComboBox::onIndexChanged(int) { bool_property->updateValue(); }

Property::PropertySpinBox::PropertySpinBox(ArraySizeProperty* array_size_property):
    QSpinBox(), array_size_property(array_size_property)
{
}

void Property::PropertySpinBox::connectEvents() { connect(this, SIGNAL(valueChanged(int)), SLOT(onValueChanged(int))); }

void Property::PropertySpinBox::onValueChanged(int) { array_size_property->onEndEdit(); }

using namespace Property;

PropertyBrowserTree::PropertyBrowserTree(QWidget* parent, QPushButton* applyButton):
    QTreeWidget(parent), sceneTreeItem(nullptr), topLevelItem(nullptr), applyButton(applyButton)
{
    setColumnCount(2);
    setHeaderHidden(true);
    setEditTriggers(QAbstractItemView::EditTrigger::SelectedClicked);

    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);

    setItemDelegateForColumn(0, new NoEditDelegate(this));

    connect(this, SIGNAL(itemClicked(QTreeWidgetItem*, int)), SLOT(onItemClicked(QTreeWidgetItem*, int)));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*, int)), SLOT(onItemChanged(QTreeWidgetItem*, int)));
    connect(applyButton, SIGNAL(clicked(bool)), this, SLOT(onApplyClicked(bool)));
}

void PropertyBrowserTree::addProperty(QtSceneTreeItem* item) { return addProperty(item, item->GetItemType()); }

void PropertyBrowserTree::addProperty(QtSceneTreeItem* item, HPS::SceneTree::ItemType itemType)
{
    flush();

    HPS::Key key = item->GetKey();
    switch (itemType) {
            // Segment or WindowKey
        case HPS::SceneTree::ItemType::Segment: {
            if (key.Type() == HPS::Type::ApplicationWindowKey) {
                HPS::ApplicationWindowKey window(key);
                rootProperty.reset(new Property::ApplicationWindowKeyProperty(invisibleRootItem(), window));
            }
            else {
                HPS::SegmentKey segment(key);
                rootProperty.reset(new Property::SegmentKeyProperty(invisibleRootItem(), segment, item->GetKeyPath()));
            }
        } break;

        case HPS::SceneTree::ItemType::AttributeFilter: {
            HPS::Type keyType = key.Type();
            if (keyType == HPS::Type::StyleKey) {
                HPS::StyleKey style(key);
                rootProperty.reset(new Property::StyleKeyAttributeFilterProperty(invisibleRootItem(), style));
            }
            else if (keyType == HPS::Type::IncludeKey) {
                HPS::IncludeKey include(key);
                rootProperty.reset(new Property::IncludeKeyAttributeFilterProperty(invisibleRootItem(), include));
            }
        } break;

        // Geometry
        case HPS::SceneTree::ItemType::CuttingSection: {
            HPS::CuttingSectionKey geometry(key);
            rootProperty.reset(new Property::CuttingSectionKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Shell: {
            HPS::ShellKey geometry(key);
            rootProperty.reset(new Property::ShellKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Mesh: {
            HPS::MeshKey geometry(key);
            rootProperty.reset(new Property::MeshKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Grid: {
            HPS::GridKey geometry(key);
            rootProperty.reset(new Property::GridKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::NURBSSurface: {
            HPS::NURBSSurfaceKey geometry(key);
            rootProperty.reset(new Property::NURBSSurfaceKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Cylinder: {
            HPS::CylinderKey geometry(key);
            rootProperty.reset(new Property::CylinderKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Sphere: {
            HPS::SphereKey geometry(key);
            rootProperty.reset(new Property::SphereKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Polygon: {
            HPS::PolygonKey geometry(key);
            rootProperty.reset(new Property::PolygonKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Circle: {
            HPS::CircleKey geometry(key);
            rootProperty.reset(new Property::CircleKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::CircularWedge: {
            HPS::CircularWedgeKey geometry(key);
            rootProperty.reset(new Property::CircularWedgeKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Ellipse: {
            HPS::EllipseKey geometry(key);
            rootProperty.reset(new Property::EllipseKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Line: {
            HPS::LineKey geometry(key);
            rootProperty.reset(new Property::LineKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::NURBSCurve: {
            HPS::NURBSCurveKey geometry(key);
            rootProperty.reset(new Property::NURBSCurveKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::CircularArc: {
            HPS::CircularArcKey geometry(key);
            rootProperty.reset(new Property::CircularArcKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::EllipticalArc: {
            HPS::EllipticalArcKey geometry(key);
            rootProperty.reset(new Property::EllipticalArcKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::InfiniteLine:
        case HPS::SceneTree::ItemType::InfiniteRay: {
            HPS::InfiniteLineKey geometry(key);
            rootProperty.reset(new Property::InfiniteLineKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Marker: {
            HPS::MarkerKey geometry(key);
            rootProperty.reset(new Property::MarkerKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Text: {
            HPS::TextKey geometry(key);
            rootProperty.reset(new Property::TextKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Reference: {
            // should we do anything in this case?
        } break;

        case HPS::SceneTree::ItemType::DistantLight: {
            HPS::DistantLightKey geometry(key);
            rootProperty.reset(new Property::DistantLightKitProperty(invisibleRootItem(), geometry));
        } break;

        case HPS::SceneTree::ItemType::Spotlight: {
            HPS::SpotlightKey geometry(key);
            rootProperty.reset(new Property::SpotlightKitProperty(invisibleRootItem(), geometry));
        } break;

        // Attributes
        case HPS::SceneTree::ItemType::Material: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::MaterialMappingKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Camera: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::CameraKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::ModellingMatrix: {
            if (key.HasType(HPS::Type::SegmentKey)) {
                HPS::SegmentKey segment(key);
                rootProperty.reset(new Property::SegmentKeyModellingMatrixProperty(invisibleRootItem(), segment));
            }
            else if (key.Type() == HPS::Type::ReferenceKey) {
                HPS::ReferenceKey reference(key);
                rootProperty.reset(new Property::ReferenceKeyModellingMatrixProperty(invisibleRootItem(), reference));
            }
        } break;

        case HPS::SceneTree::ItemType::UserData: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SegmentKeyUserDataProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::TextureMatrix: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SegmentKeyTextureMatrixProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Culling: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::CullingKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::CurveAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::CurveAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::CylinderAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::CylinderAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::EdgeAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::EdgeAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::LightingAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::LightingAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::LineAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::LineAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::MarkerAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::MarkerAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::SurfaceAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::NURBSSurfaceAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Selectability: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SelectabilityKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::SphereAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SphereAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Subwindow: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SubwindowKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::TextAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::TextAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Transparency: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::TransparencyKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Visibility: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::VisibilityKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::VisualEffects: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::VisualEffectsKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Performance: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::PerformanceKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::DrawingAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::DrawingAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::HiddenLineAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::HiddenLineAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::MaterialPalette: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SegmentKeyMaterialPaletteProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::ContourLine: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::ContourLineKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Condition: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SegmentKeyConditionProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Bounding: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::BoundingKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::AttributeLock: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::AttributeLockKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::TransformMask: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::TransformMaskKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::ColorInterpolation: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::ColorInterpolationKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::CuttingSectionAttribute: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::CuttingSectionAttributeKitProperty(invisibleRootItem(), segment));
        } break;

        case HPS::SceneTree::ItemType::Priority: {
            HPS::SegmentKey segment(key);
            rootProperty.reset(new Property::SegmentKeyPriorityProperty(invisibleRootItem(), segment));
        } break;

        // window attributes
        case HPS::SceneTree::ItemType::Debugging: {
            HPS::WindowKey window(key);
            rootProperty.reset(new Property::DebuggingKitProperty(invisibleRootItem(), window));
        } break;

        case HPS::SceneTree::ItemType::PostProcessEffects: {
            HPS::WindowKey window(key);
            rootProperty.reset(new Property::PostProcessEffectsKitProperty(invisibleRootItem(), window));
        } break;

        case HPS::SceneTree::ItemType::SelectionOptions: {
            HPS::WindowKey window(key);
            rootProperty.reset(new Property::SelectionOptionsKitProperty(invisibleRootItem(), window));
        } break;

        case HPS::SceneTree::ItemType::UpdateOptions: {
            HPS::WindowKey window(key);
            rootProperty.reset(new Property::UpdateOptionsKitProperty(invisibleRootItem(), window));
        } break;

        // definitions
        case HPS::SceneTree::ItemType::MaterialPaletteDefinition: {
            HPS::MaterialPaletteDefinition mpdef;
            HPS::PortfolioKey(key).ShowMaterialPaletteDefinition(item->GetTitle(), mpdef);
            rootProperty.reset(new Property::MaterialPaletteDefinitionProperty(invisibleRootItem(), mpdef));
        } break;

        case HPS::SceneTree::ItemType::TextureDefinition: {
            HPS::TextureDefinition tdef;
            HPS::PortfolioKey(key).ShowTextureDefinition(item->GetTitle(), tdef);
            rootProperty.reset(new Property::TextureDefinitionProperty(invisibleRootItem(), tdef));
        } break;

        case HPS::SceneTree::ItemType::CubeMapDefinition: {
            HPS::CubeMapDefinition cmdef;
            HPS::PortfolioKey(key).ShowCubeMapDefinition(item->GetTitle(), cmdef);
            rootProperty.reset(new Property::CubeMapDefinitionProperty(invisibleRootItem(), cmdef));
        } break;

        case HPS::SceneTree::ItemType::ImageDefinition: {
            HPS::ImageDefinition idef;
            if (HPS::PortfolioKey(key).ShowImageDefinition(item->GetTitle(), idef) ||
                item->GetKeyPath().ShowEffectiveImageDefinition(item->GetTitle(), idef))
                rootProperty.reset(new Property::ImageDefinitionProperty(invisibleRootItem(), idef));
        } break;

        case HPS::SceneTree::ItemType::LegacyShaderDefinition: {
            HPS::LegacyShaderDefinition sdef;
            HPS::PortfolioKey(key).ShowLegacyShaderDefinition(item->GetTitle(), sdef);
            rootProperty.reset(new Property::LegacyShaderDefinitionProperty(invisibleRootItem(), sdef));
        } break;

        case HPS::SceneTree::ItemType::LinePatternDefinition: {
            HPS::LinePatternDefinition lpdef;
            HPS::PortfolioKey(key).ShowLinePatternDefinition(item->GetTitle(), lpdef);
            rootProperty.reset(new Property::LinePatternDefinitionProperty(invisibleRootItem(), lpdef));
        } break;

        case HPS::SceneTree::ItemType::GlyphDefinition: {
            HPS::GlyphDefinition gdef;
            HPS::PortfolioKey(key).ShowGlyphDefinition(item->GetTitle(), gdef);
            rootProperty.reset(new Property::GlyphDefinitionProperty(invisibleRootItem(), gdef));
        } break;

        case HPS::SceneTree::ItemType::ShapeDefinition: {
            HPS::ShapeDefinition sdef;
            HPS::PortfolioKey(key).ShowShapeDefinition(item->GetTitle(), sdef);
            rootProperty.reset(new Property::ShapeDefinitionProperty(invisibleRootItem(), sdef));
        } break;
    }

    if (rootProperty) {
        sceneTreeItem = item;
        expandAll();
        resizeColumnToContents(0);
    }
}

void PropertyBrowserTree::flush()
{
    clear();
    sceneTreeItem = nullptr;
    rootProperty.reset();
}

void PropertyBrowserTree::onItemClicked(QTreeWidgetItem* item, int column)
{
    if (column == 0) {
        QVariant data = item->data(column, Qt::ItemDataRole::UserRole);
        if (data.isValid() && data.toInt() == PropertyType::eSettableProperty) {
            if (!item->isHidden()) {
                Property::SettableProperty* settable_item = reinterpret_cast<Property::SettableProperty*>(item);
                settable_item->isSet(!settable_item->isSet());
            }
        }
    }
    else if (column == 1) {
        QVariant data = item->data(column, Qt::ItemDataRole::UserRole);
        if (data.isValid() &&
            (data.toInt() == PropertyType::eEditableProperty || data.toInt() == PropertyType::eEditableUnsignedProperty ||
             data.toInt() == PropertyType::eEditableUnitProperty || data.toInt() == PropertyType::eEditableUTF8Property)) {
            item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
            editItem(item, column);
        }
    }
}

void PropertyBrowserTree::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == 1) {
        QVariant data = item->data(column, Qt::ItemDataRole::UserRole);
        if (data.isValid() && data.toInt() == PropertyType::eEditableProperty) {
            Property::BaseEditableProperty* editable_item = reinterpret_cast<Property::BaseEditableProperty*>(item);
            if (editable_item->getNewValue())
                editable_item->onChildChanged();
        }
        else if (data.isValid() && data.toInt() == PropertyType::eEditableUnsignedProperty) {
            Property::BaseEditableProperty* editable_item = reinterpret_cast<Property::BaseEditableProperty*>(item);

            bool conversion_successful = false;
            float value = editable_item->text(1).toFloat(&conversion_successful);
            if (!conversion_successful)
                return;
            else if (value < 0) {
                editable_item->setText(1, QString().setNum(fabs(value)));
                return;
            }

            if (editable_item->getNewValue())
                editable_item->onChildChanged();
        }
        else if (data.isValid() && data.toInt() == PropertyType::eEditableUnitProperty) {
            Property::BaseEditableProperty* editable_item = reinterpret_cast<Property::BaseEditableProperty*>(item);

            bool conversion_successful = false;
            float value = editable_item->text(1).toFloat(&conversion_successful);
            if (!conversion_successful)
                return;
            else if (value < 0.0f) {
                editable_item->setText(1, QString().setNum(0));
                return;
            }
            else if (value > 1.0f) {
                editable_item->setText(1, QString().setNum(1));
                return;
            }

            if (editable_item->getNewValue())
                editable_item->onChildChanged();
        }
        else if (data.isValid() && data.toInt() == PropertyType::eEditableUTF8Property) {
            Property::UTF8Property* utf8_property = reinterpret_cast<Property::UTF8Property*>(item);
            utf8_property->onEndEdit();
        }
    }
}

void PropertyBrowserTree::onApplyClicked(bool)
{
    if (rootProperty) {
        rootProperty->Apply();
        reexpandTree();
        updateCanvas();

        flush();
    }
}

void PropertyBrowserTree::unsetAttribute(QtSceneTreeItem* item)
{
    if (!item->HasItemType(HPS::SceneTree::ItemType::Attribute))
        return;

    bool attributeWasUnset = true;
    HPS::SceneTree::ItemType itemType = item->GetItemType();
    HPS::SegmentKey segment(item->GetKey());
    switch (itemType) {
        case HPS::SceneTree::ItemType::Material: {
            segment.UnsetMaterialMapping();
        } break;

        case HPS::SceneTree::ItemType::Camera: {
            segment.UnsetCamera();
        } break;

        case HPS::SceneTree::ItemType::ModellingMatrix: {
            segment.UnsetModellingMatrix();
        } break;

        case HPS::SceneTree::ItemType::UserData: {
            segment.UnsetAllUserData();
        } break;

        case HPS::SceneTree::ItemType::TextureMatrix: {
            segment.UnsetTextureMatrix();
        } break;

        case HPS::SceneTree::ItemType::Culling: {
            segment.UnsetCulling();
        } break;

        case HPS::SceneTree::ItemType::CurveAttribute: {
            segment.UnsetCurveAttribute();
        } break;

        case HPS::SceneTree::ItemType::CylinderAttribute: {
            segment.UnsetCylinderAttribute();
        } break;

        case HPS::SceneTree::ItemType::EdgeAttribute: {
            segment.UnsetEdgeAttribute();
        } break;

        case HPS::SceneTree::ItemType::LightingAttribute: {
            segment.UnsetLightingAttribute();
        } break;

        case HPS::SceneTree::ItemType::LineAttribute: {
            segment.UnsetLineAttribute();
        } break;

        case HPS::SceneTree::ItemType::MarkerAttribute: {
            segment.UnsetMarkerAttribute();
        } break;

        case HPS::SceneTree::ItemType::SurfaceAttribute: {
            segment.UnsetNURBSSurfaceAttribute();
        } break;

        case HPS::SceneTree::ItemType::Selectability: {
            segment.UnsetSelectability();
        } break;

        case HPS::SceneTree::ItemType::SphereAttribute: {
            segment.UnsetSphereAttribute();
        } break;

        case HPS::SceneTree::ItemType::Subwindow: {
            segment.UnsetSubwindow();
        } break;

        case HPS::SceneTree::ItemType::TextAttribute: {
            segment.UnsetTextAttribute();
        } break;

        case HPS::SceneTree::ItemType::Transparency: {
            segment.UnsetTransparency();
        } break;

        case HPS::SceneTree::ItemType::Visibility: {
            segment.UnsetVisibility();
        } break;

        case HPS::SceneTree::ItemType::VisualEffects: {
            segment.UnsetVisualEffects();
        } break;

        case HPS::SceneTree::ItemType::Performance: {
            segment.UnsetPerformance();
        } break;

        case HPS::SceneTree::ItemType::DrawingAttribute: {
            segment.UnsetDrawingAttribute();
        } break;

        case HPS::SceneTree::ItemType::HiddenLineAttribute: {
            segment.UnsetHiddenLineAttribute();
        } break;

        case HPS::SceneTree::ItemType::MaterialPalette: {
            segment.UnsetMaterialPalette();
        } break;

        case HPS::SceneTree::ItemType::ContourLine: {
            segment.UnsetContourLine();
        } break;

        case HPS::SceneTree::ItemType::Condition: {
            segment.UnsetConditions();
        } break;

        case HPS::SceneTree::ItemType::Bounding: {
            segment.UnsetBounding();
        } break;

        case HPS::SceneTree::ItemType::AttributeLock: {
            segment.UnsetAttributeLock();
        } break;

        case HPS::SceneTree::ItemType::TransformMask: {
            segment.UnsetTransformMask();
        } break;

        case HPS::SceneTree::ItemType::ColorInterpolation: {
            segment.UnsetColorInterpolation();
        } break;

        case HPS::SceneTree::ItemType::CuttingSectionAttribute: {
            segment.UnsetCuttingSectionAttribute();
        } break;

        case HPS::SceneTree::ItemType::Priority: {
            segment.UnsetPriority();
        } break;

        default: {
            attributeWasUnset = false;
        } break;
    }

    if (attributeWasUnset) {
        sceneTreeItem = item;
        reexpandTree();
        updateCanvas();
        flush();
    }
}

void PropertyBrowserTree::reexpandTree()
{
    if (sceneTreeItem == nullptr)
        return;

    if (sceneTreeItem->HasItemType(HPS::SceneTree::ItemType::Attribute)) {
        QTreeWidgetItem* segmentItem = sceneTreeItem->getTreeItem()->parent()->parent();
        QtSceneTreeItem* segmentSceneTreeItem = segmentItem->data(0, Qt::ItemDataRole::UserRole).value<QtSceneTreeItem*>();
        sceneTreeItem->getTreeWidget()->removeChildren(segmentItem);
        segmentSceneTreeItem->ReExpand();
    }
    else if (sceneTreeItem->GetItemType() == HPS::SceneTree::ItemType::Segment) {
        sceneTreeItem->getTreeWidget()->removeChildren(sceneTreeItem->getTreeItem());
        sceneTreeItem->ReExpand();
    }
}

void PropertyBrowserTree::updateCanvas() { getMainWidget()->getCanvas()->Update(); }

PropertyWidget::PropertyWidget(QWidget* parent): QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout();

    applyButton = new QPushButton("Apply", this);
    tree = new PropertyBrowserTree(this, applyButton);

    layout->addWidget(tree);
    layout->addWidget(applyButton);

    setLayout(layout);
}

void PropertyWidget::flush()
{
    tree->flush();
    parentWidget()->setWindowTitle("Properties");
}

void PropertyWidget::addProperty(QtSceneTreeItem* item, SceneTree::ItemType itemType)
{
    if (itemType == SceneTree::ItemType::None)
        tree->addProperty(item);
    else
        tree->addProperty(item, itemType);
}

void PropertyWidget::unsetAttribute(QtSceneTreeItem* item) { tree->unsetAttribute(item); }