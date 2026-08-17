#pragma once
#include "QtWidgets/QtWidgets"
#include "qpushbutton.h"
#include "sprk.h"

using namespace HPS;

class QtSceneTreeItem;

namespace Property {
    template<typename EnumType>
    class BaseEnumProperty;
    class BoolProperty;

    /*
        Qt does not support Q_OBJECT on templated classes, so we are splitting the implemenation
        between a base class which handles user input and a derived templated class which handles
        the property logic.
    */

    class BaseComboBox: public QComboBox {
        Q_OBJECT;

      public:
        BaseComboBox();

      public slots:
        virtual void onIndexChanged(int newIndex);
    };

    template<typename T>
    class PropertyComboBox: public BaseComboBox {
      public:
        PropertyComboBox(BaseEnumProperty<T>* base_property);
        void connectEvents();

      public:
        virtual void onIndexChanged(int newIndex);

      private:
        BaseEnumProperty<T>* base_property;
    };

    class BoolComboBox: public BaseComboBox {
      public:
        BoolComboBox(BoolProperty* bool_property);
        void connectEvents();

      public:
        virtual void onIndexChanged(int newIndex);

      private:
        BoolProperty* bool_property;
    };

    class ArraySizeProperty;

    class PropertySpinBox: public QSpinBox {
        Q_OBJECT;
      public slots:
        void onValueChanged(int newValue);

      public:
        PropertySpinBox(ArraySizeProperty* array_size_property);
        void connectEvents();

      private:
        ArraySizeProperty* array_size_property;
    };

} // namespace Property

class NoEditDelegate: public QStyledItemDelegate {
  public:
    NoEditDelegate(QObject* parent = 0): QStyledItemDelegate(parent) {}

    virtual QWidget* createEditor(QWidget* parent, QStyleOptionViewItem const& option, QModelIndex const& index) const
    {
        return nullptr;
    }
};

#include "properties.h"

class PropertyBrowserTree: public QTreeWidget {
    Q_OBJECT;

  public:
    PropertyBrowserTree(QWidget* parent = nullptr, QPushButton* applyButton = nullptr);

    void addProperty(QtSceneTreeItem* item);
    void addProperty(QtSceneTreeItem* item, SceneTree::ItemType itemType);
    void unsetAttribute(QtSceneTreeItem* item);
    void flush();

  public slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onApplyClicked(bool);

  private:
    void reexpandTree();
    void updateCanvas();

    QtSceneTreeItem* sceneTreeItem;
    QTreeWidgetItem* topLevelItem;
    std::unique_ptr<Property::RootProperty> rootProperty;
    QPushButton* applyButton;
};

class PropertyWidget: public QWidget {
    Q_OBJECT;

  public:
    PropertyWidget(QWidget* parent = nullptr);

    void flush();
    void addProperty(QtSceneTreeItem* item, SceneTree::ItemType itemType);
    void unsetAttribute(QtSceneTreeItem* item);

  private:
    PropertyBrowserTree* tree;
    QPushButton* applyButton;
};
