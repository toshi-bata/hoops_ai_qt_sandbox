#include "HPSModelBrowser.h"
#include "HPSMainWindow.h"
#include "HPSWidget.h"

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

ModelBrowserWidget::ModelBrowserWidget(QWidget* parent): QTreeWidget(parent), componentTree(nullptr), contextItem(nullptr)
{
    setColumnCount(1);
    setHeaderHidden(true);
    setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);

    QTreeWidgetItem* item = new QTreeWidgetItem(static_cast<QTreeWidget*>(nullptr), QStringList(QString("No Model")));
    insertTopLevelItem(0, item);

    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), SLOT(onItemExpanding(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), SLOT(onItemCollapsing(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemClicked(QTreeWidgetItem*, int)), SLOT(onItemClicked(QTreeWidgetItem*, int)));
    connect(this, SIGNAL(needsRepaint()), SLOT(repaintWidget()));
    connect(this, SIGNAL(customContextMenuRequested(QPoint const&)), SLOT(onContextMenu(QPoint const&)));
}

void ModelBrowserWidget::Init(CADModel const& cadModel, Canvas const& canvas)
{
    if (cadModel.Empty())
        Flush();
    else {
        if (componentTree) {
            componentTree->Flush();
            componentTree.reset();
        }

        componentTree = std::make_shared<QtComponentTree>(canvas, this);
        ComponentTreeItemPtr root = std::make_shared<QtComponentTreeItem>(componentTree, cadModel);
        componentTree->SetRoot(root);

        HighlightOptionsKit highlightOptions;
        highlightOptions.SetStyleName("highlight_style").SetNotification(true);
        componentTree->SetHighlightOptions(highlightOptions);

        // auto expand out a few levels
        QTreeWidgetItem* rootTreeItem = topLevelItem(0);
        rootTreeItem->data(0, Qt::ItemDataRole::UserRole).value<QtComponentTreeItem*>()->ExpandInternal();

        QTreeWidgetItem* modelGroupItem = rootTreeItem->child(0);
        modelGroupItem->data(0, Qt::ItemDataRole::UserRole).value<QtComponentTreeItem*>()->ExpandInternal();
    }
}

void ModelBrowserWidget::Flush()
{
    QTreeWidgetItem* rootItem = invisibleRootItem();
    int numberOfChildren = rootItem->childCount();
    for (int i = 0; i < numberOfChildren; ++i) {
        QTreeWidgetItem* item = rootItem->child(i);
        rootItem->removeChild(item);
        delete item;
    }

    QTreeWidgetItem* noModelItem = new QTreeWidgetItem();
    noModelItem->setText(0, "No Model");
    insertTopLevelItem(0, noModelItem);
}

void ModelBrowserWidget::onItemExpanding(QTreeWidgetItem* item)
{
    QVariant shouldSkipExpand = item->data(0, Qt::ItemDataRole::UserRole + 1);
    if (shouldSkipExpand.isValid() && shouldSkipExpand.value<bool>()) {
        item->setData(0, Qt::ItemDataRole::UserRole + 1, QVariant(false));
        return;
    }

    item->data(0, Qt::ItemDataRole::UserRole).value<QtComponentTreeItem*>()->Expand();
}

void ModelBrowserWidget::onItemCollapsing(QTreeWidgetItem* item)
{
    item->data(0, Qt::ItemDataRole::UserRole).value<QtComponentTreeItem*>()->Collapse();

    std::vector<QTreeWidgetItem*> children;
    for (int i = 0; i < item->childCount(); ++i)
        children.push_back(item->child(i));
    for (auto one_child: children)
        item->removeChild(one_child);
}

void ModelBrowserWidget::onItemClicked(QTreeWidgetItem* item, int column)
{
    HPS_UNREFERENCED(column);
    if (item == nullptr)
        return;

    QtComponentTreeItem* componentItem = item->data(0, Qt::ItemDataRole::UserRole).value<QtComponentTreeItem*>();
    if (componentItem == nullptr)
        return;

    ComponentTree::ItemType itemType = componentItem->GetItemType();
    Component::ComponentType componentType = componentItem->GetComponent().GetComponentType();

    HPSWidget* mainWidget = getMainWidget();

    if ((itemType == ComponentTree::ItemType::ExchangeComponent && componentType != Component::ComponentType::ExchangeFilter) ||
        itemType == ComponentTree::ItemType::ParasolidComponent || itemType == ComponentTree::ItemType::DWGComponent) {
#if defined(USING_EXCHANGE)
        if (componentType == Component::ComponentType::ExchangeView) {
            auto capturePath = componentItem->GetPath();
            mainWidget->ActivateCapture(capturePath);
        }
        else if (componentType == Component::ComponentType::ExchangeDrawingSheet) {
            View newView = Exchange::Sheet(componentItem->GetComponent()).Activate();
            mainWidget->AttachView(newView);
        }
        else if (componentItem->IsHidden() == false) {
            mainWidget->Unhighlight();
            componentItem->Highlight();
            mainWidget->getCanvas()->Update();
        }
#endif
    }
}

void ModelBrowserWidget::repaintWidget() { viewport()->update(); }

void ModelBrowserWidget::onContextMenu(QPoint const& point)
{
    QTreeWidgetItem* item = itemAt(point);

    if (item != nullptr)
        showContextMenu(item, viewport()->mapToGlobal(point));
}

void ModelBrowserWidget::showContextMenu(QTreeWidgetItem* item, QPoint const& position)
{
    contextItem = item->data(0, Qt::ItemDataRole::UserRole).value<QtComponentTreeItem*>();
    if (contextItem != nullptr) {
        QString showHideString;
        Component selectedComponent = contextItem->GetComponent();
        Component::ComponentType componentType = selectedComponent.GetComponentType();
        if (selectedComponent.HasComponentType(Component::ComponentType::ExchangeComponentMask)) {
            if (contextItem->IsHidden())
                showHideString = "Show";
            else
                showHideString = "Hide";

            QMenu contextMenu;
            QAction* isolateAction = new QAction("Isolate");
            QAction* showHideAction = new QAction(showHideString);
            QAction* resetVisibilityAction = new QAction("Reset Visibility");
            contextMenu.addAction(isolateAction);
            contextMenu.addAction(showHideAction);
            contextMenu.addAction(resetVisibilityAction);

            connect(isolateAction, SIGNAL(triggered()), this, SLOT(onIsolate()));
            connect(showHideAction, SIGNAL(triggered()), this, SLOT(onShowHide()));
            connect(resetVisibilityAction, SIGNAL(triggered()), this, SLOT(onResetVisibility()));

            contextMenu.exec(position);
        }
    }
}

void ModelBrowserWidget::onIsolate()
{
    HPSWidget* mainWidget = getMainWidget();
    mainWidget->Unhighlight();

    contextItem->Isolate();
    mainWidget->ZoomToKeyPath(contextItem->GetPath().GetKeyPaths()[0]);

    mainWidget->getCanvas()->Update();
}

void ModelBrowserWidget::onShowHide()
{
    HPSWidget* mainWidget = getMainWidget();
    mainWidget->Unhighlight();

    if (contextItem->IsHidden()) {
        contextItem->Show();

        CADModel cadModel = mainWidget->getCADModel();
        if (ComponentPath(1, &cadModel).IsHidden(*mainWidget->getCanvas()))
            mainWidget->InvalidateZoomKeyPath();
        else
            mainWidget->InvalidateSavedCamera();
    }
    else
        contextItem->Hide();

    mainWidget->getCanvas()->Update();
}

void ModelBrowserWidget::onResetVisibility()
{
    HPSWidget* mainWidget = getMainWidget();
    Canvas canvas = *mainWidget->getCanvas();
    mainWidget->getCADModel().ResetVisibility(canvas);
    mainWidget->RestoreCamera();
    canvas.Update();
}

QtComponentTree::QtComponentTree(Canvas const& in_canvas, ModelBrowserWidget* in_widget):
    ComponentTree(in_canvas), widget(in_widget)
{
}

ModelBrowserWidget* QtComponentTree::GetWidget() { return widget; }

void QtComponentTree::Flush()
{
    ComponentTree::Flush();

    QTreeWidgetItem* rootItem = widget->invisibleRootItem();
    int numberOfChildren = rootItem->childCount();
    for (int i = 0; i < numberOfChildren; ++i) {
        QTreeWidgetItem* item = rootItem->child(i);
        rootItem->removeChild(item);
        delete item;
    }
}

QtComponentTreeItem::QtComponentTreeItem(ComponentTreePtr const& in_tree, CADModel const& in_cad_model):
    ComponentTreeItem(in_tree, in_cad_model), treeItem(nullptr)
{
}

QtComponentTreeItem::QtComponentTreeItem(ComponentTreePtr const& in_tree,
                                         Component const& in_component,
                                         ComponentTree::ItemType in_type):
    ComponentTreeItem(in_tree, in_component, in_type),
    treeItem(nullptr)
{
}

QIcon QtComponentTreeItem::GetIcon(ComponentTreeItem* item, bool visible)
{
    QString filename("imgUnknownIcon");
    ComponentTree::ItemType itemType = item->GetItemType();
    Component::ComponentType componentType = Component::ComponentType::None;

    switch (itemType) {
        case ComponentTree::ItemType::ExchangeViewGroup:
        case ComponentTree::ItemType::ExchangeAnnotationViewGroup:
        case ComponentTree::ItemType::ExchangePMIGroup:
            filename = "dirMarkIcon";
            break;

        case ComponentTree::ItemType::ExchangeModelGroup:
            filename = "dirGeometryIcon";
            break;

        case ComponentTree::ItemType::DWGModelFile:
        case ComponentTree::ItemType::ExchangeModelFile:
            filename = "classIcon";
            break;

        case ComponentTree::ItemType::ExchangeComponent: {
            Component component = item->GetComponent();
            componentType = component.GetComponentType();

            switch (componentType) {
                case Component::ComponentType::ExchangeProductOccurrence: {
                    // different icons if this product occurrence has product occurrences as children or not
                    ComponentArray subcomponents = component.GetSubcomponents();
                    auto it = std::find_if(subcomponents.begin(), subcomponents.end(), [](Component const& comp) {
                        return comp.GetComponentType() == Component::ComponentType::ExchangeProductOccurrence;
                    });
                    if (it == subcomponents.end())
                        filename = "dirProduct2Icon";
                    else
                        filename = "dirProductIcon";
                } break;

                case Component::ComponentType::ExchangeRISet:
                    filename = "dirGeometryIcon";
                    break;

                case Component::ComponentType::ExchangeRIPlane:
                    filename = "imgPlanIcon";
                    break;

                case Component::ComponentType::ExchangeRIDirection:
                    filename = "imgDirectionIcon";
                    break;

                case Component::ComponentType::ExchangeRICoordinateSystem:
                    filename = "imgAxisIcon";
                    break;

                case Component::ComponentType::ExchangeRIBRepModel:
                case Component::ComponentType::ExchangeRIPolyBRepModel:
                    filename = "imgSolidIcon";
                    break;

                case Component::ComponentType::ExchangeRICurve:
                    filename = "imgCurveIcon";
                    break;

                case Component::ComponentType::ExchangeRIPolyWire:
                    filename = "imgCompCurveIcon";
                    break;

                case Component::ComponentType::ExchangeRIPointSet:
                    filename = "imgPointCloudIcon";
                    break;

                case Component::ComponentType::ExchangeView: {
                    if (BooleanMetadata(component.GetMetadata("IsAnnotationCapture")).GetValue())
                        filename = "cadViewIcon";
                    else
                        filename = "pmiViewProductViewIcon";
                } break;

                case Component::ComponentType::ExchangePMI:
                case Component::ComponentType::ExchangePMIText:
                case Component::ComponentType::ExchangePMIRichText:
                    filename = "markupIcon";
                    break;

                case Component::ComponentType::ExchangePMIGDT:
                    filename = "cadTolerIcon";
                    break;

                case Component::ComponentType::ExchangePMIRoughness:
                    filename = "cadRoughIcon";
                    break;

                case Component::ComponentType::ExchangePMILineWelding:
                    filename = "cadLineWeldingIcon";
                    break;

                case Component::ComponentType::ExchangePMISpotWelding:
                    filename = "cadSpotWeldingIcon";
                    break;

                case Component::ComponentType::ExchangePMIDatum:
                    filename = "cadReferIcon";
                    break;

                case Component::ComponentType::ExchangePMIDimension:
                    filename = "dimDistanceIcon";
                    break;

                case Component::ComponentType::ExchangePMIBalloon:
                    filename = "cadBalloonIcon";
                    break;

                case Component::ComponentType::ExchangePMICoordinate:
                    filename = "dimCoordinateIcon";
                    break;

                case Component::ComponentType::ExchangePMIFastener:
                    filename = "cadFasternerIcon";
                    break;

                case Component::ComponentType::ExchangePMILocator:
                    filename = "cadLocatorIcon";
                    break;

                case Component::ComponentType::ExchangePMIMeasurementPoint:
                    filename = "cadMeasurementPointIcon";
                    break;

                case Component::ComponentType::ExchangeDrawingModel:
                    filename = "imgDrawingModelIcon";
                    break;

                case Component::ComponentType::ExchangeDrawingSheet:
                    filename = "imgDrawingSheetIcon";
                    break;

                case Component::ComponentType::ExchangeDrawingView:
                    filename = "imgDrawingViewIcon";
                    break;
            }
        } break;

        case ComponentTree::ItemType::ParasolidModelFile:
            filename = "classIcon";
            break;

        case ComponentTree::ItemType::ParasolidComponent: {
            switch (componentType) {
                case Component::ComponentType::ParasolidAssembly:
                    filename = "dirProduct2Icon";
                    break;

                case Component::ComponentType::ParasolidTopoBody:
                    filename = "imgSolidIcon";
                    break;

                case Component::ComponentType::ParasolidInstance: {
                    Component component = item->GetComponent();
                    Component::ComponentType subcomponentType = component.GetSubcomponents()[0].GetComponentType();
                    if (subcomponentType == Component::ComponentType::ParasolidTopoBody)
                        filename = "imgSolidIcon";
                    else if (subcomponentType == Component::ComponentType::ParasolidAssembly)
                        filename = "dirProduct2Icon";
                } break;
            }
        } break;
    }

    if (!visible && itemType != ComponentTree::ItemType::ExchangeModelFile &&
        itemType != ComponentTree::ItemType::ExchangeModelGroup && itemType != ComponentTree::ItemType::ExchangeViewGroup &&
        itemType != ComponentTree::ItemType::ExchangeAnnotationViewGroup &&
        itemType != ComponentTree::ItemType::ExchangePMIGroup &&
        (itemType != ComponentTree::ItemType::ExchangeComponent || componentType != Component::ComponentType::ExchangeView) &&
        itemType != ComponentTree::ItemType::ParasolidModelFile) {
        filename[0] = filename[0].toUpper();
        filename = "hidden" + filename;
    }

    return QIcon(":/HPSMainWindow/" + filename);
}

ComponentTreeItemPtr QtComponentTreeItem::AddChild(Component const& in_component, ComponentTree::ItemType in_type)
{
    auto child = std::make_shared<QtComponentTreeItem>(GetTree(), in_component, in_type);

    UTF8 title = child->GetTitle();
    if (title.Empty())
        title = "Unnamed";

    QTreeWidgetItem* child_item = new QTreeWidgetItem();
    child_item->setText(0, title.GetBytes());
    child_item->setChildIndicatorPolicy(child->HasChildren() ? QTreeWidgetItem::ChildIndicatorPolicy::ShowIndicator :
                                                               QTreeWidgetItem::ChildIndicatorPolicy::DontShowIndicator);
    QVariant data_in;
    data_in.setValue(child.get());
    child_item->setData(0, Qt::ItemDataRole::UserRole, data_in);

    child_item->setIcon(0, GetIcon(child.get(), !child->IsHidden()));

    if (treeItem == nullptr)
        GetTreeWidget()->insertTopLevelItem(0, child_item);
    else
        treeItem->addChild(child_item);

    child->SetTreeItem(child_item);

    return child;
}

void QtComponentTreeItem::ExpandInternal()
{
    ModelBrowserWidget* widget = GetTreeWidget();
    if (widget != nullptr) {
        if (treeItem == nullptr || !treeItem->isExpanded()) {
            ComponentTreeItem::Expand();
            treeItem->setData(0, Qt::ItemDataRole::UserRole + 1, QVariant(true));
            treeItem->setExpanded(true);
        }
    }
}

void QtComponentTreeItem::Expand()
{
    ModelBrowserWidget* widget = GetTreeWidget();
    if (widget != nullptr) {
        if (treeItem) {
            if (treeItem->isExpanded() == false) {
                ComponentTreeItem::Expand();
                treeItem->setData(0, Qt::ItemDataRole::UserRole + 1, QVariant(true));
                treeItem->setExpanded(true);
                return;
            }
        }
        if (treeItem == nullptr || treeItem->isExpanded())
            ComponentTreeItem::Expand();
    }
}

void QtComponentTreeItem::Collapse() { ComponentTreeItem::Collapse(); }

void QtComponentTreeItem::OnHighlight(HighlightOptionsKit const& in_options)
{
    ComponentTreeItem::OnHighlight(in_options);

    if (treeItem) {
        QFont font = treeItem->font(0);
        font.setBold(true);
        treeItem->setFont(0, font);

        emit GetTreeWidget()->needsRepaint();
    }
}

void QtComponentTreeItem::OnUnhighlight(HighlightOptionsKit const& in_options)
{
    ComponentTreeItem::OnUnhighlight(in_options);

    if (treeItem) {
        QFont font = treeItem->font(0);
        font.setBold(false);
        treeItem->setFont(0, font);

        emit GetTreeWidget()->needsRepaint();
    }
}

void QtComponentTreeItem::OnHide()
{
    // the top of the Qt Tree Widget is not associated with a Component
    if (treeItem == nullptr)
        return;

    ComponentTreeItem::OnHide();

    treeItem->setIcon(0, GetIcon(this, false));
    emit GetTreeWidget()->needsRepaint();
}

void QtComponentTreeItem::OnShow()
{
    // the top of the Qt Tree Widget is not associated with a Component
    if (treeItem == nullptr)
        return;

    ComponentTreeItem::OnShow();

    treeItem->setIcon(0, GetIcon(this, true));
    emit GetTreeWidget()->needsRepaint();
}

ModelBrowserWidget* QtComponentTreeItem::GetTreeWidget() const
{
    return std::static_pointer_cast<QtComponentTree>(GetTree())->GetWidget();
}

ConfigurationWidget::ConfigurationWidget(QWidget* parent): QTreeWidget(parent)
{
    setColumnCount(1);
    setHeaderHidden(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);

    QTreeWidgetItem* item = new QTreeWidgetItem(static_cast<QTreeWidget*>(nullptr), QStringList(QString("No Configurations")));
    insertTopLevelItem(0, item);

    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), SLOT(onItemDoubleClicked(QTreeWidgetItem*, int)));
}

void ConfigurationWidget::Init(CADModel const& cadModel)
{
#if defined(USING_EXCHANGE)
    if (cadModel.Empty())
        Flush();
    else {
        UTF8 fileFormat = StringMetadata(cadModel.GetMetadata("FileFormat")).GetValue();
        if (fileFormat == "SolidWorks" || fileFormat == "CATIA V4" || fileFormat == "I-DEAS") {
            // Only these formats have a concept of configurations
            Exchange::CADModel exchangeCADModel(cadModel);
            Exchange::ConfigurationArray configurations;
            if (fileFormat == "SolidWorks")
                configurations = exchangeCADModel.GetConfigurations();
            else {
                UTF8 filename = StringMetadata(cadModel.GetMetadata("Filename")).GetValue();
                configurations = Exchange::File::GetConfigurations(filename);
            }

            if (configurations.empty())
                Flush();
            else {
                std::vector<QTreeWidgetItem*> children;
                QTreeWidgetItem* root = invisibleRootItem();
                for (int i = 0; i < root->childCount(); ++i)
                    children.push_back(root->child(i));
                for (auto one_child: children)
                    root->removeChild(one_child);

                QTreeWidgetItem* topLevelItem = new QTreeWidgetItem();
                topLevelItem->setText(0, "Configurations");
                insertTopLevelItem(0, topLevelItem);

                for (auto const& config: configurations)
                    insertConfigurationInTree(topLevelItem, config);

                topLevelItem->setExpanded(true);

                UTF8Array currentConfiguration = exchangeCADModel.GetCurrentConfiguration();
                if (!currentConfiguration.empty()) {
                    QTreeWidgetItem* selectedConfiguration = findConfigurationInTree(currentConfiguration, topLevelItem);
                    if (selectedConfiguration != nullptr) {
                        QFont font = selectedConfiguration->font(0);
                        font.setBold(true);
                        selectedConfiguration->setFont(0, font);
                        selectedConfiguration->setExpanded(true);
                    }
                }
            }
        }
        else
            Flush();
    }
#endif
}

void ConfigurationWidget::Flush()
{
    std::vector<QTreeWidgetItem*> children;
    QTreeWidgetItem* root = invisibleRootItem();
    for (int i = 0; i < root->childCount(); ++i)
        children.push_back(root->child(i));
    for (auto one_child: children)
        root->removeChild(one_child);

    QTreeWidgetItem* topLevelItem = new QTreeWidgetItem();
    topLevelItem->setText(0, "No Configurations");
    insertTopLevelItem(0, topLevelItem);
}

#if defined(USING_EXCHANGE)
void ConfigurationWidget::insertConfigurationInTree(QTreeWidgetItem* root, Exchange::Configuration const& configuration)
{
    UTF8 configurationName = configuration.GetName();
    QTreeWidgetItem* configurationItem = new QTreeWidgetItem;
    configurationItem->setText(0, configurationName.GetBytes());
    root->insertChild(0, configurationItem);

    Exchange::ConfigurationArray subConfigurations = configuration.GetSubconfigurations();
    for (auto const& subConfig: subConfigurations)
        insertConfigurationInTree(configurationItem, subConfig);
}

QTreeWidgetItem* ConfigurationWidget::findConfigurationInTree(UTF8Array const& configuration, QTreeWidgetItem const* root)
{
    if (root->childCount() == 0)
        return nullptr;

    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* childItem = root->child(i);
        if (childItem->text(0) == configuration[0]) {
            if (configuration.size() == 1)
                return childItem;
            else
                return findConfigurationInTree(UTF8Array(configuration.begin() + 1, configuration.end()), childItem);
        }
    }

    return nullptr;
}

UTF8Array ConfigurationWidget::getSelectedConfiguration()
{
    UTF8Array newlySelectedConfiguration;
    QList<QTreeWidgetItem*> selections = selectedItems();
    if (!selections.empty()) {
        QTreeWidgetItem* item = selections[0];
        do {
            QString text = item->text(0);
            if (text == "Configurations" || text == "No Configurations")
                break;
            newlySelectedConfiguration.emplace_back(text.toStdString().c_str());
        } while ((item = item->parent()) != invisibleRootItem());
    }

    std::reverse(newlySelectedConfiguration.begin(), newlySelectedConfiguration.end());

    return newlySelectedConfiguration;
}
#endif

void ConfigurationWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    HPS_UNREFERENCED(column);

    if (item != nullptr) {
#if defined(USING_EXCHANGE)
        UTF8Array configuration = getSelectedConfiguration();
        getMainWidget()->ImportConfiguration(configuration);
#endif
    }
}

TabbedView::TabbedView()
{
    setTabPosition(QTabWidget::South);

    modelBrowserWidget = new ModelBrowserWidget();
    configurationBrowserWidget = new ConfigurationWidget();
    addTab(modelBrowserWidget, "Model Browser");
    addTab(configurationBrowserWidget, "Configurations");

    connect(this, SIGNAL(currentChanged(int)), this, SLOT(onCurrentTabChanged(int)));
}

ModelBrowserWidget* TabbedView::getModelBrowser() { return modelBrowserWidget; }

ConfigurationWidget* TabbedView::getConfigurationBrowser() { return configurationBrowserWidget; }

void TabbedView::onCurrentTabChanged(int index)
{
    switch (index) {
        case 0:
            parentWidget()->setWindowTitle("Model Browser");
            break;
        case 1:
            parentWidget()->setWindowTitle("Configurations");
            break;
    }
}