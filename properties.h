//This file was generated automatically by the genproperties.py script. Do not edit.
#include "QtWidgets/QtWidgets"
#include <deque>
#include "hps.h"

namespace Property
{

	// deque doesn't specialize the bool type, so we use it to store lists of boolean values
	typedef std::deque<bool, HPS::Allocator<bool>> BoolDeque;

	enum PropertyType
	{
		eEditableProperty = 0,
		eEditableUnsignedProperty,
		eEditableUnitProperty,
		eSettableProperty,
		eEditableUTF8Property,
	};

	class RootProperty
	{
	public:
		RootProperty()
			: tree(nullptr)
			, item(nullptr)
		{
		}

		RootProperty(QTreeWidget * tree)
			: tree(tree)
			, item(nullptr)
		{
		}

		RootProperty(QTreeWidgetItem * item)
			: tree(nullptr)
			, item(item)
		{

		}

		virtual void Apply()
		{

		}

	protected:
		QTreeWidget * tree;
		QTreeWidgetItem * item;
	};

	class BaseProperty : public QTreeWidgetItem
	{
	public:
		//Group Property - name only, no value
		BaseProperty(const char * title)
		{
			setText(0, title);
		}

		//Property with name and value
		BaseProperty(const char * title, QVariant const & value)
		{
			setText(0, title);
			if (value.userType() == QMetaType::Float)
			{
				QString formatted_string;
				formatted_string.setNum(value.toFloat());
				setText(1, formatted_string);
			}
			else
				setText(1, value.toString());
		}

		// Show this property and any sub-properties that are enabled.
		// Disabled sub-properties will not be shown.
		virtual void smartShow(bool show = true)
		{
			setHidden(!show);

			if (show)
			{
				int subItemCount = childCount();
				for (int i = 0; i < subItemCount; ++i)
				{
					BaseProperty * subItem = static_cast<BaseProperty *>(child(i));
					bool subItemShow = show;
					if (subItem->isDisabled())
						subItemShow = false;
					if (subItemShow)
						setExpanded(true);
					subItem->smartShow(subItemShow);
				}
			}
		}

		// Invoked by a child when it needs to notify it's parent it was modified
		virtual void onChildChanged()
		{
			BaseProperty * parentProperty = static_cast<BaseProperty *>(parent());
			if (parentProperty)
				parentProperty->onChildChanged();
		}
	};

	template <typename T>
	class ImmutableTypeProperty : public BaseProperty
	{
	public:
		ImmutableTypeProperty(const char * name, T typeValue)
			: BaseProperty(name, (QVariant)typeValue)
		{
			setFlags(flags() & ~Qt::ItemFlag::ItemIsEditable);
		}
	};

	typedef ImmutableTypeProperty<unsigned int> ImmutableUnsignedIntProperty;
	typedef ImmutableTypeProperty<float> ImmutableFloatProperty;

	class ImmutableUTF8Property : public BaseProperty
	{
	public:
		ImmutableUTF8Property(const char * name, HPS::UTF8 const & utf8Value)
			: BaseProperty(name, utf8Value.GetBytes())
		{
			setFlags(flags() & ~Qt::ItemFlag::ItemIsEditable);
		}
	};

	// Some values (e.g., size_t or intptr_t on 64-bit builds) won't display properly in a property grid.
	// For immutable values we want to display that are of these types, we can convert them to strings and store them as immutable strings.
	template <typename T>
	class ImmutableTypeAsUTF8Property : public ImmutableUTF8Property
	{
	public:
		ImmutableTypeAsUTF8Property(const char * name, T typeValue)
			: ImmutableUTF8Property(name, std::to_wstring(typeValue).c_str())
		{}
	};

	typedef ImmutableTypeAsUTF8Property<intptr_t> ImmutableIntPtrTProperty;
	typedef ImmutableTypeAsUTF8Property<size_t> ImmutableSizeTProperty;

	class ImmutableBoolProperty : public BaseProperty
	{
	public:
		ImmutableBoolProperty(const char * name, bool boolValue)
			: BaseProperty(name, boolValue ? "true" : "false")
		{
			setFlags(flags() & ~Qt::ItemFlag::ItemIsEditable);
		}
	};

	template <
		typename Kit,
		typename ArrayValue,
		bool (Kit::*ShowArray)(std::vector<ArrayValue, HPS::Allocator<ArrayValue>> &) const
	>
	class ImmutableArraySizeProperty : public BaseProperty
	{
	private:
		typedef std::vector<ArrayValue, HPS::Allocator<ArrayValue>> ArrayType;

	public:
		ImmutableArraySizeProperty(const char * name, const char * sizeName, Kit const & kit)
			: BaseProperty(name)
		{
			ArrayType values;
			(kit.*ShowArray)(values);
			addChild(new ImmutableSizeTProperty(sizeName, values.size()));
		}
	};

	class ImmutablePointProperty : public BaseProperty
	{
	public:
		ImmutablePointProperty(const char * name, HPS::Point const & point)
			: BaseProperty(name)
		{
			addChild(new ImmutableFloatProperty("X", point.x));
			addChild(new ImmutableFloatProperty("Y", point.y));
			addChild(new ImmutableFloatProperty("Z", point.z));
		}
	};

	class ImmutableSimpleSphereProperty : public BaseProperty
	{
	public:
		ImmutableSimpleSphereProperty(const char * name, HPS::SimpleSphere const & sphere)
			: BaseProperty(name)
		{
			addChild(new ImmutablePointProperty("Center", sphere.center));
			addChild(new ImmutableFloatProperty("Radius", sphere.radius));
		}
	};

	class ImmutableSimpleCuboidProperty : public BaseProperty
	{
	public:
		ImmutableSimpleCuboidProperty(const char * name, HPS::SimpleCuboid const & cuboid)
			: BaseProperty(name)
		{
			addChild(new ImmutablePointProperty("Min", cuboid.min));
			addChild(new ImmutablePointProperty("Max", cuboid.max));
		}
	};

	class BoolProperty : public BaseProperty
	{
	public:
		BoolProperty(QTreeWidget * tree, const char * name, bool & boolValue)
			: BaseProperty(name, (QVariant)boolValue)
			, boolValue(boolValue)
			, tree(tree)
		{
			comboBox = new BoolComboBox(this);
		}

		void setupComboBox()
		{
			comboBox->addItem("False");
			comboBox->addItem("True");
			comboBox->setCurrentIndex(boolValue ? 1 : 0);

			tree->setItemWidget(this, 1, comboBox);
			comboBox->connectEvents();
		}

		virtual void updateValue()
		{
			boolValue = comboBox->currentIndex() == 0 ? false : true;
			onChildChanged();
		}

	protected:
		bool & boolValue;
		BoolComboBox * comboBox;
		QTreeWidget * tree;
	};

	// A boolean property that disables its sibling properties when false and enables them when true.
	class ConditionalBoolProperty : public BoolProperty
	{
	public:
		ConditionalBoolProperty(QTreeWidget* tree, const char * name, bool & boolValue)
			: BoolProperty(tree, name, boolValue)
		{
		}

		virtual void onUpdateValue()
		{
			boolValue = comboBox->currentIndex() == 0 ? false : true;
			enableValidProperties();
			onChildChanged();
		}

		virtual void enableValidProperties()
		{
			QTreeWidgetItem * parentItem = parent();
			int siblingCount = childCount();
			for (int i = siblingCount; i > 0; --i)
			{
				QTreeWidgetItem * sibling = parentItem->child(i);
				sibling->setDisabled(!boolValue);
			}
		}

		void onChildChanged() override
		{
			BaseProperty * parentItem = static_cast<BaseProperty *>(parent());
			parentItem->smartShow();
		}
	};

	class BaseEditableProperty : public BaseProperty
	{
	public:
		BaseEditableProperty(const char * name, QVariant const & typeValue)
			: BaseProperty(name, typeValue)
		{}

		virtual bool getNewValue()
		{
			//should be handled by derived class
			Q_ASSERT(false);

			return false;
		}
	};

		template < typename T,
	    typename std::enable_if<std::is_same<T, int>::value>::type * = nullptr>
		void getValueFromEditableTypeProperty (QString const & in_text, T & out_value)
		{
			out_value = in_text.toInt();
		}

		template < typename T,
	    typename std::enable_if<std::is_same<T, unsigned int>::value>::type * = nullptr>
		void getValueFromEditableTypeProperty (QString const & in_text, T & out_value)
		{
			out_value = in_text.toUInt();
		}

		template < typename T,
	    typename std::enable_if<std::is_same<T, float>::value>::type * = nullptr>
		void getValueFromEditableTypeProperty (QString const & in_text, T & out_value)
		{
			out_value = in_text.toFloat();
		}

		template < typename T,
	    typename std::enable_if<std::is_same<T, double>::value>::type * = nullptr>
		void getValueFromEditableTypeProperty (QString const & in_text, T & out_value)
		{
			out_value = (double)in_text.toFloat();
		}

		template < typename T,
	    typename std::enable_if<std::is_same<T, HPS::byte>::value>::type * = nullptr>
		void getValueFromEditableTypeProperty (QString const & in_text, T & out_value)
		{
			if (in_text.toStdString().empty())
				out_value = "";
			else
				out_value = in_text.toStdString().c_str()[0];
		}

	template <typename T>
	class EditableTypeProperty : public BaseEditableProperty
	{
	public:
		EditableTypeProperty(const char * name, T & typeValue)
			: BaseEditableProperty(name, (QVariant)typeValue)
			, typeValue(typeValue)
		{
			setFlags(flags() | Qt::ItemFlag::ItemIsEditable);

			QVariant data_in;
			data_in.setValue((int)PropertyType::eEditableProperty);
			setData(1, Qt::ItemDataRole::UserRole, data_in);
		}

		virtual bool getNewValue()
		{
			QString value = text(1);
			T newTypeValue;
			getValueFromEditableTypeProperty(value, newTypeValue);

			if (typeValue == newTypeValue)
				return false;

			typeValue = newTypeValue;
			return true;
		}

	private:
		T & typeValue;
	};


	//Just like EditableTypeProperty<float>, but input can only be positive
	template <typename T>
	class UnsignedEditableTypeProperty : public BaseEditableProperty
	{
	public:
		UnsignedEditableTypeProperty(const char * name, T & typeValue)
			: BaseEditableProperty(name, (QVariant)typeValue)
			, typeValue(typeValue)
		{
			setFlags(flags() | Qt::ItemFlag::ItemIsEditable);

			QVariant data_in;
			data_in.setValue((int)PropertyType::eEditableUnsignedProperty);
			setData(1, Qt::ItemDataRole::UserRole, data_in);
		}

	private:
		virtual bool getNewValue()
		{
			QString value = text(1);
			T newTypeValue;

			if (std::is_same<T, float>::value)
				newTypeValue = static_cast<T>(value.toFloat());
			else
			{
				Q_ASSERT(false);
				return false;
			}

			if (typeValue == newTypeValue)
				return false;

			typeValue = newTypeValue;
			return true;
		}

		T & typeValue;
	};

	typedef EditableTypeProperty<int> IntProperty;
	typedef EditableTypeProperty<unsigned int> UnsignedIntProperty;
	typedef EditableTypeProperty<HPS::byte> ByteProperty;
	typedef UnsignedEditableTypeProperty<float> UnsignedFloatProperty;

	//Just like EditableTypeProperty<float>, but input can only be [0-1]
	class UnitFloatProperty : public BaseEditableProperty
	{
	public:
		UnitFloatProperty(const char * name, float & typeValue)
			: BaseEditableProperty(name, (QVariant)typeValue)
			, typeValue(typeValue)
		{
			setFlags(flags() | Qt::ItemFlag::ItemIsEditable);

			QVariant data_in;
			data_in.setValue((int)PropertyType::eEditableUnitProperty);
			setData(1, Qt::ItemDataRole::UserRole, data_in);
		}

		virtual bool getNewValue()
		{
			QString value = text(1);
			float newTypeValue = value.toFloat();

			if (typeValue == newTypeValue)
				return false;

			typeValue = newTypeValue;
			return true;
		}

	private:
		float & typeValue;
	};

	template <typename EnumType>
	class BaseEnumProperty : public BaseProperty
	{
	protected:
		typedef std::vector<EnumType, HPS::Allocator<EnumType>> EnumTypeArray;

	public:
		BaseEnumProperty(const char * name, EnumType & enumValue)
			: BaseProperty(name)
			, enumValue(enumValue)
		{
			// You can use the drop-down list to change values, but you can't modify the text.
			setFlags(flags() & ~Qt::ItemFlag::ItemIsEditable);

			comboBox = new PropertyComboBox<EnumType>(this);
		}

	public slots:
		void onUpdateValue(int newIndex)
		{
			if (getTypeFromValue())
			{
				enableValidProperties();
				onChildChanged();
			}
		}

	public:
		void onChildChanged() override
		{
			BaseProperty * parentItem = static_cast<BaseProperty *>(parent());
			parentItem->smartShow();

			BaseProperty::onChildChanged();
		}

		virtual void enableValidProperties()
		{
			// Derived classes should override to toggle valid properties on enum value changes if necessary.
		}

		virtual bool getTypeFromValue()
		{
			int currentIndex = comboBox->currentIndex();
			EnumType newEnumValue = _enumValues[currentIndex];

			if (enumValue == newEnumValue)
				return false;

			enumValue = newEnumValue;
			return true;
		}

	protected:
		void initializeEnumValues(EnumTypeArray const & enumValues, HPS::UTF8Array const & enumStrings, QTreeWidget * tree)
		{
			_enumValues = enumValues;

			tree->setItemWidget(this, 1, comboBox);

			Q_ASSERT(_enumValues.size() == enumStrings.size());
			for (size_t i = 0; i < _enumValues.size(); ++i)
			{
				comboBox->addItem(enumStrings[i].GetBytes());
				if (_enumValues[i] == enumValue)
					comboBox->setCurrentIndex(i);
			}

			comboBox->connectEvents();
		}

	protected:
		EnumType & enumValue;

	private:
		EnumTypeArray _enumValues;
		PropertyComboBox<EnumType> * comboBox;
	};

	class RGBAColorProperty : public BaseProperty
	{
	public:
		RGBAColorProperty(
			const char * name,
			HPS::RGBAColor & color)
			: BaseProperty(name)
		{
			addChild(new UnitFloatProperty("Red", color.red));
			addChild(new UnitFloatProperty("Green", color.green));
			addChild(new UnitFloatProperty("Blue", color.blue));
			addChild(new UnitFloatProperty("Alpha", color.alpha));
		}
	};

	class RGBColorProperty : public BaseProperty
	{
	public:
		RGBColorProperty(
			const char * name,
			HPS::RGBColor & color)
			: BaseProperty(name)
		{
			addChild(new UnitFloatProperty("Red", color.red));
			addChild(new UnitFloatProperty("Green", color.green));
			addChild(new UnitFloatProperty("Blue", color.blue));
		}
	};

	template <typename T>
	class BaseFloatProperty : public EditableTypeProperty<T>
	{
	public:
		BaseFloatProperty(const char * name, T & typeValue)
			: EditableTypeProperty<T>(name, typeValue)
		{}
	};

	typedef BaseFloatProperty<float> FloatProperty;
	typedef BaseFloatProperty<double> DoubleProperty;

	//goes from -127 to 128
	class SByteProperty : public BaseProperty
	{
	public:
		SByteProperty(const char * name, HPS::sbyte & sbyteValue)
			: BaseProperty(name, static_cast<QVariant>(static_cast<char>(sbyteValue)))
			, sbyteValue(sbyteValue)
		{}

		void onUpdateValue()
		{
			if (getNewValue())
				onChildChanged();
		}

	private:
		bool getNewValue()
		{
			QString string = text(1);
			int intValue = atoi(string.toStdString().c_str());
			auto newSbyteValue = static_cast<HPS::sbyte>(intValue);
			if (sbyteValue == newSbyteValue)
				return false;

			sbyteValue = newSbyteValue;
			return true;
		}

	protected:
		HPS::sbyte sbyteValue;
	};

	class PointProperty : public BaseProperty
	{
	public:
		PointProperty(
			const char * name,
			HPS::Point & point)
			: BaseProperty(name)
		{
			addChild(new FloatProperty("X", point.x));
			addChild(new FloatProperty("Y", point.y));
			addChild(new FloatProperty("Z", point.z));
		}
	};

	class VectorProperty : public BaseProperty
	{
	public:
		VectorProperty(
			const char * name,
			HPS::Vector & vec)
			: BaseProperty(name)
		{
			addChild(new FloatProperty("X", vec.x));
			addChild(new FloatProperty("Y", vec.y));
			addChild(new FloatProperty("Z", vec.z));
		}
	};

	class PlaneProperty : public BaseProperty
	{
	public:
		PlaneProperty(
			const char * name,
			HPS::Plane & plane)
			: BaseProperty(name)
		{
			addChild(new FloatProperty("A", plane.a));
			addChild(new FloatProperty("B", plane.b));
			addChild(new FloatProperty("C", plane.c));
			addChild(new FloatProperty("D", plane.d));
		}
	};

	class RectangleProperty : public BaseProperty
	{
	public:
		RectangleProperty(
			const char * name,
			HPS::Rectangle & rectangle)
			: BaseProperty(name)
		{
			addChild(new FloatProperty("Left", rectangle.left));
			addChild(new FloatProperty("Right", rectangle.right));
			addChild(new FloatProperty("Bottom", rectangle.bottom));
			addChild(new FloatProperty("Top", rectangle.top));
		}
	};

	class IntRectangleProperty : public BaseProperty
	{
	public:
		IntRectangleProperty(
			const char * name,
			HPS::IntRectangle & rectangle)
			: BaseProperty(name)
		{
			addChild(new IntProperty("Left", rectangle.left));
			addChild(new IntProperty("Right", rectangle.right));
			addChild(new IntProperty("Bottom", rectangle.bottom));
			addChild(new IntProperty("Top", rectangle.top));
		}
	};

	class SimpleSphereProperty : public BaseProperty
	{
	public:
		SimpleSphereProperty(
			const char * name,
			HPS::SimpleSphere & sphere)
			: BaseProperty(name)
		{
			addChild(new PointProperty("Center", sphere.center));
			addChild(new UnsignedFloatProperty("Radius", sphere.radius));
		}
	};

	class SimpleCuboidProperty : public BaseProperty
	{
	public:
		SimpleCuboidProperty(
			const char * name,
			HPS::SimpleCuboid & cuboid)
			: BaseProperty(name)
		{
			addChild(new PointProperty("Min", cuboid.min));
			addChild(new PointProperty("Max", cuboid.max));
		}
	};

	class GlyphPointProperty : public BaseProperty
	{
	public:
		GlyphPointProperty(
			const char * name,
			HPS::GlyphPoint & point)
			: BaseProperty(name)
		{
			addChild(new SByteProperty("X", point.x));
			addChild(new SByteProperty("Y", point.y));
		}
	};

	class UTF8Property : public BaseProperty
	{
	public:
		UTF8Property(
			const char * name,
			HPS::UTF8 & utf8Value)
			: BaseProperty(name, utf8Value.GetBytes())
			, utf8Value(utf8Value)
		{
			setFlags(flags() | Qt::ItemFlag::ItemIsEditable);

			QVariant data_in;
			data_in.setValue((int)PropertyType::eEditableUTF8Property);
			setData(1, Qt::ItemDataRole::UserRole, data_in);
		}

		void onEndEdit()
		{
			if (getUTF8FromValue())
				onChildChanged();
		}

	private:
		bool getUTF8FromValue()
		{
			HPS::UTF8 newUtf8Value(text(1).toStdString().data());

			if (utf8Value == newUtf8Value)
				return false;

			utf8Value = newUtf8Value;
			return true;
		}

	private:
		HPS::UTF8 & utf8Value;
	};

	// A (group) property with a checkbox that sets/unsets a component of an attribute
	class SettableProperty : public BaseProperty
	{
	public:
		SettableProperty(const char * title)
			: BaseProperty(title)
			, _isSet(false)
		{
			setCheckState(0, Qt::CheckState::Unchecked);

			QVariant data_in;
			data_in.setValue((int)PropertyType::eSettableProperty);
			setData(0, Qt::ItemDataRole::UserRole, data_in);
		}

		void smartShow(bool show = true) override
		{
			BaseProperty::smartShow(show);
			showEnabledChildren();
		}

		void onChildChanged() override
		{
			if (_isSet)
				set();
			else
				unset();
			BaseProperty::onChildChanged();
		}

		void isSet(bool isSet)
		{
			_isSet = isSet;
			showEnabledChildren();
			onChildChanged();

			if (isSet)
				setCheckState(0, Qt::CheckState::Checked);
			else
				setCheckState(0, Qt::CheckState::Unchecked);
		}

		bool isSet() const
		{
			return _isSet;
		}

	protected:
		virtual void set() = 0;
		virtual void unset() = 0;

		virtual void showEnabledChildren()
		{
			int numberOfChildren = childCount();
			for (int i = numberOfChildren - 1; i >= 0; --i)
			{
				BaseProperty * childItem = static_cast<BaseProperty *>(child(i));
				bool show = _isSet;

				if (childItem->isDisabled())
				{
					// We only want to show children that are enabled.
					show = false;
				}

				childItem->smartShow(show);
			}
		}

	private:
		bool _isSet;
	};

	// An array property that *can* be set/unset
	class SettableArrayProperty : public SettableProperty
	{
	public:
		SettableArrayProperty(
			const char * name,
			int firstItemIndex = 1)
			: SettableProperty(name)
			, firstItemIndex(firstItemIndex)
		{}

	protected:
		virtual void ResizeArrays() = 0;

		virtual bool AddOrDeleteItems(
			unsigned int newItemCount,
			unsigned int oldItemCount)
		{
			if (newItemCount == oldItemCount)
				return false;

			ResizeArrays();

			if (newItemCount < oldItemCount)
			{
				unsigned int startIndex = firstItemIndex + newItemCount;
				unsigned int endIndex = firstItemIndex + oldItemCount - 1;
				DeleteItems(startIndex, endIndex);
			}
			else
			{
				if (oldItemCount > 0)
				{
					// Delete the old items because they may have references bound to invalid addresses if
					// the underlying arrays had to be re-allocated.
					unsigned int startIndex = firstItemIndex;
					unsigned int endIndex = firstItemIndex + oldItemCount - 1;
					DeleteItems(startIndex, endIndex);
				}

				AddItems();
				showEnabledChildren();
			}

			return true;
		}

		virtual void AddItems() = 0;

		virtual void DeleteItems(
			unsigned int startIndex,
			unsigned int endIndex)
		{
			for (unsigned int i = endIndex; i >= startIndex; --i)
			{
				removeChild(child(i));
				if (i == 0)
					break;
			}
		}

	protected:
		int firstItemIndex;
	};

	// The second column of tree items contains a spin box
	class ArraySizeProperty : public BaseProperty
	{
	public:
		ArraySizeProperty(
			const char * name,
			unsigned int & arraySize,
			unsigned int minValue = 1,
			unsigned int maxValue = std::numeric_limits<int>::max())
			: BaseProperty(name, arraySize)
			, arraySize(arraySize)
			, minValue(minValue)
			, maxValue(maxValue)
		{
			spinbox = new PropertySpinBox(this);
			spinbox->setRange(minValue, maxValue);
			spinbox->setValue(arraySize);
		}

		void setupSpinBox(QTreeWidget * tree)
		{
			tree->setItemWidget(this, 1, spinbox);
			spinbox->connectEvents();
		}

		void onEndEdit()
		{
			if (getSizeFromValue())
				onChildChanged();
		}

	private:
		bool getSizeFromValue()
		{
			int newArraySize = spinbox->value();
			if (newArraySize == 0)
				return false;
			if (arraySize == newArraySize)
				return false;
			arraySize = newArraySize;
			return true;
		}

		unsigned int & arraySize;
		unsigned int minValue;
		unsigned int maxValue;

		PropertySpinBox * spinbox;
	};

	// An array property that *can't* be set/unset
	class ArrayProperty : public BaseProperty
	{
	private:
		enum PropertyTypeIndex
		{
			CountPropertyIndex = 0,
			FirstItemIndex,
		};

	public:
		ArrayProperty(
			const char * name)
			: BaseProperty(name)
		{}

	protected:
		virtual void ResizeArrays() = 0;

		virtual bool AddOrDeleteItems(
			unsigned int newItemCount,
			unsigned int oldItemCount)
		{
			if (newItemCount == oldItemCount)
				return false;

			ResizeArrays();

			if (newItemCount < oldItemCount)
			{
				unsigned int startIndex = FirstItemIndex + newItemCount;
				unsigned int endIndex = FirstItemIndex + oldItemCount - 1;
				DeleteItems(startIndex, endIndex);
			}
			else
			{
				if (oldItemCount > 0)
				{
					// Delete the old items because they may have references bound to invalid addresses if
					// the underlying arrays had to be re-allocated.
					unsigned int startIndex = FirstItemIndex;
					unsigned int endIndex = FirstItemIndex + oldItemCount - 1;
					DeleteItems(startIndex, endIndex);
				}

				AddItems();
			}

			return true;
		}

		virtual void AddItems() = 0;

		virtual void DeleteItems(
			unsigned int startIndex,
			unsigned int endIndex)
		{
			for (unsigned int i = endIndex; i >= startIndex; --i)
			{
				auto item = child(i);
				parent()->removeChild(item);
			}
		}
	};

	class TextMetricsOptionsProperty : public BaseEnumProperty<HPS::TextMetrics::Options>
	{
	public:
		TextMetricsOptionsProperty(
			QTreeWidget * tree,
			HPS::TextMetrics::Options & enumValue,
			const char * name = "Options")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::TextMetrics::Options::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::TextMetrics::Options::Extended; enumStrings[1] = "Extended";
			enumValues[2] = HPS::TextMetrics::Options::PerGlyph; enumStrings[2] = "PerGlyph";
			enumValues[3] = HPS::TextMetrics::Options::ExtendedPerGlyph; enumStrings[3] = "ExtendedPerGlyph";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextMetricsUnitsProperty : public BaseEnumProperty<HPS::TextMetrics::Units>
	{
	public:
		TextMetricsUnitsProperty(
			QTreeWidget * tree,
			HPS::TextMetrics::Units & enumValue,
			const char * name = "Units")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(1); HPS::UTF8Array enumStrings(1);
			enumValues[0] = HPS::TextMetrics::Units::Fractional; enumStrings[0] = "Fractional";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class WindowDriverProperty : public BaseEnumProperty<HPS::Window::Driver>
	{
	public:
		WindowDriverProperty(
			QTreeWidget * tree,
			HPS::Window::Driver & enumValue,
			const char * name = "Driver")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Window::Driver::Default3D; enumStrings[0] = "Default3D";
			enumValues[1] = HPS::Window::Driver::OpenGL; enumStrings[1] = "OpenGL";
			enumValues[2] = HPS::Window::Driver::OpenGL2; enumStrings[2] = "OpenGL2";
			enumValues[3] = HPS::Window::Driver::DirectX11; enumStrings[3] = "DirectX11";
			enumValues[4] = HPS::Window::Driver::OpenGL2Mesa; enumStrings[4] = "OpenGL2Mesa";
			enumValues[5] = HPS::Window::Driver::Metal; enumStrings[5] = "Metal";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class WindowUpdateTypeProperty : public BaseEnumProperty<HPS::Window::UpdateType>
	{
	public:
		WindowUpdateTypeProperty(
			QTreeWidget * tree,
			HPS::Window::UpdateType & enumValue,
			const char * name = "UpdateType")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Window::UpdateType::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Window::UpdateType::Complete; enumStrings[1] = "Complete";
			enumValues[2] = HPS::Window::UpdateType::Refresh; enumStrings[2] = "Refresh";
			enumValues[3] = HPS::Window::UpdateType::CompileOnly; enumStrings[3] = "CompileOnly";
			enumValues[4] = HPS::Window::UpdateType::Exhaustive; enumStrings[4] = "Exhaustive";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SubwindowTypeProperty : public BaseEnumProperty<HPS::Subwindow::Type>
	{
	public:
		SubwindowTypeProperty(
			QTreeWidget * tree,
			HPS::Subwindow::Type & enumValue,
			const char * name = "Type")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Subwindow::Type::Standard; enumStrings[0] = "Standard";
			enumValues[1] = HPS::Subwindow::Type::Lightweight; enumStrings[1] = "Lightweight";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SubwindowBorderProperty : public BaseEnumProperty<HPS::Subwindow::Border>
	{
	public:
		SubwindowBorderProperty(
			QTreeWidget * tree,
			HPS::Subwindow::Border & enumValue,
			const char * name = "Border")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Subwindow::Border::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Subwindow::Border::Inset; enumStrings[1] = "Inset";
			enumValues[2] = HPS::Subwindow::Border::InsetBold; enumStrings[2] = "InsetBold";
			enumValues[3] = HPS::Subwindow::Border::Overlay; enumStrings[3] = "Overlay";
			enumValues[4] = HPS::Subwindow::Border::OverlayBold; enumStrings[4] = "OverlayBold";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SubwindowRenderingAlgorithmProperty : public BaseEnumProperty<HPS::Subwindow::RenderingAlgorithm>
	{
	public:
		SubwindowRenderingAlgorithmProperty(
			QTreeWidget * tree,
			HPS::Subwindow::RenderingAlgorithm & enumValue,
			const char * name = "RenderingAlgorithm")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Subwindow::RenderingAlgorithm::ZBuffer; enumStrings[0] = "ZBuffer";
			enumValues[1] = HPS::Subwindow::RenderingAlgorithm::HiddenLine; enumStrings[1] = "HiddenLine";
			enumValues[2] = HPS::Subwindow::RenderingAlgorithm::FastHiddenLine; enumStrings[2] = "FastHiddenLine";
			enumValues[3] = HPS::Subwindow::RenderingAlgorithm::Priority; enumStrings[3] = "Priority";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShellComponentProperty : public BaseEnumProperty<HPS::Shell::Component>
	{
	public:
		ShellComponentProperty(
			QTreeWidget * tree,
			HPS::Shell::Component & enumValue,
			const char * name = "Component")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Shell::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::Shell::Component::Edges; enumStrings[1] = "Edges";
			enumValues[2] = HPS::Shell::Component::Vertices; enumStrings[2] = "Vertices";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShellHandednessOptimizationProperty : public BaseEnumProperty<HPS::Shell::HandednessOptimization>
	{
	public:
		ShellHandednessOptimizationProperty(
			QTreeWidget * tree,
			HPS::Shell::HandednessOptimization & enumValue,
			const char * name = "HandednessOptimization")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Shell::HandednessOptimization::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Shell::HandednessOptimization::Fix; enumStrings[1] = "Fix";
			enumValues[2] = HPS::Shell::HandednessOptimization::Reverse; enumStrings[2] = "Reverse";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MeshComponentProperty : public BaseEnumProperty<HPS::Mesh::Component>
	{
	public:
		MeshComponentProperty(
			QTreeWidget * tree,
			HPS::Mesh::Component & enumValue,
			const char * name = "Component")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Mesh::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::Mesh::Component::Edges; enumStrings[1] = "Edges";
			enumValues[2] = HPS::Mesh::Component::Vertices; enumStrings[2] = "Vertices";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CellularVolumeComponentProperty : public BaseEnumProperty<HPS::CellularVolume::Component>
	{
	public:
		CellularVolumeComponentProperty(
			QTreeWidget * tree,
			HPS::CellularVolume::Component & enumValue,
			const char * name = "Component")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::CellularVolume::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::CellularVolume::Component::Edges; enumStrings[1] = "Edges";
			enumValues[2] = HPS::CellularVolume::Component::Vertices; enumStrings[2] = "Vertices";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CellularVolumeCellTypeProperty : public BaseEnumProperty<HPS::CellularVolume::CellType>
	{
	public:
		CellularVolumeCellTypeProperty(
			QTreeWidget * tree,
			HPS::CellularVolume::CellType & enumValue,
			const char * name = "CellType")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(8); HPS::UTF8Array enumStrings(8);
			enumValues[0] = HPS::CellularVolume::CellType::Simplex; enumStrings[0] = "Simplex";
			enumValues[1] = HPS::CellularVolume::CellType::Pyramid; enumStrings[1] = "Pyramid";
			enumValues[2] = HPS::CellularVolume::CellType::Wedge; enumStrings[2] = "Wedge";
			enumValues[3] = HPS::CellularVolume::CellType::Box; enumStrings[3] = "Box";
			enumValues[4] = HPS::CellularVolume::CellType::Face; enumStrings[4] = "Face";
			enumValues[5] = HPS::CellularVolume::CellType::Polyhedron; enumStrings[5] = "Polyhedron";
			enumValues[6] = HPS::CellularVolume::CellType::Mixed; enumStrings[6] = "Mixed";
			enumValues[7] = HPS::CellularVolume::CellType::Separator; enumStrings[7] = "Separator";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class InfiniteLineTypeProperty : public BaseEnumProperty<HPS::InfiniteLine::Type>
	{
	public:
		InfiniteLineTypeProperty(
			QTreeWidget * tree,
			HPS::InfiniteLine::Type & enumValue,
			const char * name = "Type")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::InfiniteLine::Type::Line; enumStrings[0] = "Line";
			enumValues[1] = HPS::InfiniteLine::Type::Ray; enumStrings[1] = "Ray";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TrimTypeProperty : public BaseEnumProperty<HPS::Trim::Type>
	{
	public:
		TrimTypeProperty(
			QTreeWidget * tree,
			HPS::Trim::Type & enumValue,
			const char * name = "Type")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Trim::Type::Line; enumStrings[0] = "Line";
			enumValues[1] = HPS::Trim::Type::Curve; enumStrings[1] = "Curve";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TrimOperationProperty : public BaseEnumProperty<HPS::Trim::Operation>
	{
	public:
		TrimOperationProperty(
			QTreeWidget * tree,
			HPS::Trim::Operation & enumValue,
			const char * name = "Operation")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Trim::Operation::Keep; enumStrings[0] = "Keep";
			enumValues[1] = HPS::Trim::Operation::Remove; enumStrings[1] = "Remove";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SpotlightOuterConeUnitsProperty : public BaseEnumProperty<HPS::Spotlight::OuterConeUnits>
	{
	public:
		SpotlightOuterConeUnitsProperty(
			QTreeWidget * tree,
			HPS::Spotlight::OuterConeUnits & enumValue,
			const char * name = "OuterConeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Spotlight::OuterConeUnits::Degrees; enumStrings[0] = "Degrees";
			enumValues[1] = HPS::Spotlight::OuterConeUnits::FieldRadius; enumStrings[1] = "FieldRadius";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SpotlightInnerConeUnitsProperty : public BaseEnumProperty<HPS::Spotlight::InnerConeUnits>
	{
	public:
		SpotlightInnerConeUnitsProperty(
			QTreeWidget * tree,
			HPS::Spotlight::InnerConeUnits & enumValue,
			const char * name = "InnerConeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Spotlight::InnerConeUnits::Degrees; enumStrings[0] = "Degrees";
			enumValues[1] = HPS::Spotlight::InnerConeUnits::FieldRadius; enumStrings[1] = "FieldRadius";
			enumValues[2] = HPS::Spotlight::InnerConeUnits::Percent; enumStrings[2] = "Percent";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CylinderComponentProperty : public BaseEnumProperty<HPS::Cylinder::Component>
	{
	public:
		CylinderComponentProperty(
			QTreeWidget * tree,
			HPS::Cylinder::Component & enumValue,
			const char * name = "Component")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Cylinder::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::Cylinder::Component::Edges; enumStrings[1] = "Edges";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CylinderCappingProperty : public BaseEnumProperty<HPS::Cylinder::Capping>
	{
	public:
		CylinderCappingProperty(
			QTreeWidget * tree,
			HPS::Cylinder::Capping & enumValue,
			const char * name = "Capping")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Cylinder::Capping::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Cylinder::Capping::First; enumStrings[1] = "First";
			enumValues[2] = HPS::Cylinder::Capping::Last; enumStrings[2] = "Last";
			enumValues[3] = HPS::Cylinder::Capping::Both; enumStrings[3] = "Both";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CylinderOrientationProperty : public BaseEnumProperty<HPS::Cylinder::Orientation>
	{
	public:
		CylinderOrientationProperty(
			QTreeWidget * tree,
			HPS::Cylinder::Orientation & enumValue,
			const char * name = "Orientation")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(8); HPS::UTF8Array enumStrings(8);
			enumValues[0] = HPS::Cylinder::Orientation::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Cylinder::Orientation::DefaultRadii; enumStrings[1] = "DefaultRadii";
			enumValues[2] = HPS::Cylinder::Orientation::InvertRadii; enumStrings[2] = "InvertRadii";
			enumValues[3] = HPS::Cylinder::Orientation::InvertRadiiOnly; enumStrings[3] = "InvertRadiiOnly";
			enumValues[4] = HPS::Cylinder::Orientation::DefaultColors; enumStrings[4] = "DefaultColors";
			enumValues[5] = HPS::Cylinder::Orientation::InvertColors; enumStrings[5] = "InvertColors";
			enumValues[6] = HPS::Cylinder::Orientation::InvertColorsOnly; enumStrings[6] = "InvertColorsOnly";
			enumValues[7] = HPS::Cylinder::Orientation::InvertAll; enumStrings[7] = "InvertAll";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class HighlightSearchScopeProperty : public BaseEnumProperty<HPS::HighlightSearch::Scope>
	{
	public:
		HighlightSearchScopeProperty(
			QTreeWidget * tree,
			HPS::HighlightSearch::Scope & enumValue,
			const char * name = "Scope")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::HighlightSearch::Scope::AtOrAbovePath; enumStrings[0] = "AtOrAbovePath";
			enumValues[1] = HPS::HighlightSearch::Scope::AtOrBelowPath; enumStrings[1] = "AtOrBelowPath";
			enumValues[2] = HPS::HighlightSearch::Scope::ExactPath; enumStrings[2] = "ExactPath";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MaterialTextureParameterizationProperty : public BaseEnumProperty<HPS::Material::Texture::Parameterization>
	{
	public:
		MaterialTextureParameterizationProperty(
			QTreeWidget * tree,
			HPS::Material::Texture::Parameterization & enumValue,
			const char * name = "Parameterization")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::Material::Texture::Parameterization::Cylinder; enumStrings[0] = "Cylinder";
			enumValues[1] = HPS::Material::Texture::Parameterization::PhysicalReflection; enumStrings[1] = "PhysicalReflection";
			enumValues[2] = HPS::Material::Texture::Parameterization::Object; enumStrings[2] = "Object";
			enumValues[3] = HPS::Material::Texture::Parameterization::NaturalUV; enumStrings[3] = "NaturalUV";
			enumValues[4] = HPS::Material::Texture::Parameterization::ReflectionVector; enumStrings[4] = "ReflectionVector";
			enumValues[5] = HPS::Material::Texture::Parameterization::SurfaceNormal; enumStrings[5] = "SurfaceNormal";
			enumValues[6] = HPS::Material::Texture::Parameterization::Sphere; enumStrings[6] = "Sphere";
			enumValues[7] = HPS::Material::Texture::Parameterization::UV; enumStrings[7] = "UV";
			enumValues[8] = HPS::Material::Texture::Parameterization::World; enumStrings[8] = "World";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MaterialTextureTilingProperty : public BaseEnumProperty<HPS::Material::Texture::Tiling>
	{
	public:
		MaterialTextureTilingProperty(
			QTreeWidget * tree,
			HPS::Material::Texture::Tiling & enumValue,
			const char * name = "Tiling")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Material::Texture::Tiling::Clamp; enumStrings[0] = "Clamp";
			enumValues[1] = HPS::Material::Texture::Tiling::Repeat; enumStrings[1] = "Repeat";
			enumValues[2] = HPS::Material::Texture::Tiling::Reflect; enumStrings[2] = "Reflect";
			enumValues[3] = HPS::Material::Texture::Tiling::Trim; enumStrings[3] = "Trim";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MaterialTextureInterpolationProperty : public BaseEnumProperty<HPS::Material::Texture::Interpolation>
	{
	public:
		MaterialTextureInterpolationProperty(
			QTreeWidget * tree,
			HPS::Material::Texture::Interpolation & enumValue,
			const char * name = "Interpolation")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Texture::Interpolation::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Material::Texture::Interpolation::Bilinear; enumStrings[1] = "Bilinear";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MaterialTextureDecimationProperty : public BaseEnumProperty<HPS::Material::Texture::Decimation>
	{
	public:
		MaterialTextureDecimationProperty(
			QTreeWidget * tree,
			HPS::Material::Texture::Decimation & enumValue,
			const char * name = "Decimation")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Texture::Decimation::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Material::Texture::Decimation::Anisotropic; enumStrings[1] = "Anisotropic";
			enumValues[2] = HPS::Material::Texture::Decimation::Mipmap; enumStrings[2] = "Mipmap";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MaterialTextureChannelMappingProperty : public BaseEnumProperty<HPS::Material::Texture::ChannelMapping>
	{
	public:
		MaterialTextureChannelMappingProperty(
			QTreeWidget * tree,
			HPS::Material::Texture::ChannelMapping & enumValue,
			const char * name = "ChannelMapping")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Material::Texture::ChannelMapping::Red; enumStrings[0] = "Red";
			enumValues[1] = HPS::Material::Texture::ChannelMapping::Green; enumStrings[1] = "Green";
			enumValues[2] = HPS::Material::Texture::ChannelMapping::Blue; enumStrings[2] = "Blue";
			enumValues[3] = HPS::Material::Texture::ChannelMapping::Alpha; enumStrings[3] = "Alpha";
			enumValues[4] = HPS::Material::Texture::ChannelMapping::Zero; enumStrings[4] = "Zero";
			enumValues[5] = HPS::Material::Texture::ChannelMapping::One; enumStrings[5] = "One";
			enumValues[6] = HPS::Material::Texture::ChannelMapping::Luminance; enumStrings[6] = "Luminance";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class PostProcessEffectsAmbientOcclusionQualityProperty : public BaseEnumProperty<HPS::PostProcessEffects::AmbientOcclusion::Quality>
	{
	public:
		PostProcessEffectsAmbientOcclusionQualityProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffects::AmbientOcclusion::Quality & enumValue,
			const char * name = "Quality")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::PostProcessEffects::AmbientOcclusion::Quality::Fastest; enumStrings[0] = "Fastest";
			enumValues[1] = HPS::PostProcessEffects::AmbientOcclusion::Quality::Nicest; enumStrings[1] = "Nicest";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class PostProcessEffectsBloomShapeProperty : public BaseEnumProperty<HPS::PostProcessEffects::Bloom::Shape>
	{
	public:
		PostProcessEffectsBloomShapeProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffects::Bloom::Shape & enumValue,
			const char * name = "Shape")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::PostProcessEffects::Bloom::Shape::Star; enumStrings[0] = "Star";
			enumValues[1] = HPS::PostProcessEffects::Bloom::Shape::Radial; enumStrings[1] = "Radial";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class PerformanceDisplayListsProperty : public BaseEnumProperty<HPS::Performance::DisplayLists>
	{
	public:
		PerformanceDisplayListsProperty(
			QTreeWidget * tree,
			HPS::Performance::DisplayLists & enumValue,
			const char * name = "DisplayLists")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Performance::DisplayLists::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Performance::DisplayLists::Geometry; enumStrings[1] = "Geometry";
			enumValues[2] = HPS::Performance::DisplayLists::Segment; enumStrings[2] = "Segment";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class PerformanceStaticModelProperty : public BaseEnumProperty<HPS::Performance::StaticModel>
	{
	public:
		PerformanceStaticModelProperty(
			QTreeWidget * tree,
			HPS::Performance::StaticModel & enumValue,
			const char * name = "StaticModel")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Performance::StaticModel::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Performance::StaticModel::Attribute; enumStrings[1] = "Attribute";
			enumValues[2] = HPS::Performance::StaticModel::AttributeSpatial; enumStrings[2] = "AttributeSpatial";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class PerformanceStaticConditionsProperty : public BaseEnumProperty<HPS::Performance::StaticConditions>
	{
	public:
		PerformanceStaticConditionsProperty(
			QTreeWidget * tree,
			HPS::Performance::StaticConditions & enumValue,
			const char * name = "StaticConditions")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Performance::StaticConditions::Independent; enumStrings[0] = "Independent";
			enumValues[1] = HPS::Performance::StaticConditions::Single; enumStrings[1] = "Single";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class AttributeLockTypeProperty : public BaseEnumProperty<HPS::AttributeLock::Type>
	{
	public:
		AttributeLockTypeProperty(
			QTreeWidget * tree,
			HPS::AttributeLock::Type & enumValue,
			const char * name = "Type")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(116); HPS::UTF8Array enumStrings(116);
			enumValues[0] = HPS::AttributeLock::Type::Everything; enumStrings[0] = "Everything";
			enumValues[1] = HPS::AttributeLock::Type::Visibility; enumStrings[1] = "Visibility";
			enumValues[2] = HPS::AttributeLock::Type::VisibilityCuttingSections; enumStrings[2] = "VisibilityCuttingSections";
			enumValues[3] = HPS::AttributeLock::Type::VisibilityCutEdges; enumStrings[3] = "VisibilityCutEdges";
			enumValues[4] = HPS::AttributeLock::Type::VisibilityCutFaces; enumStrings[4] = "VisibilityCutFaces";
			enumValues[5] = HPS::AttributeLock::Type::VisibilityWindows; enumStrings[5] = "VisibilityWindows";
			enumValues[6] = HPS::AttributeLock::Type::VisibilityText; enumStrings[6] = "VisibilityText";
			enumValues[7] = HPS::AttributeLock::Type::VisibilityLines; enumStrings[7] = "VisibilityLines";
			enumValues[8] = HPS::AttributeLock::Type::VisibilityEdgeLights; enumStrings[8] = "VisibilityEdgeLights";
			enumValues[9] = HPS::AttributeLock::Type::VisibilityMarkerLights; enumStrings[9] = "VisibilityMarkerLights";
			enumValues[10] = HPS::AttributeLock::Type::VisibilityFaceLights; enumStrings[10] = "VisibilityFaceLights";
			enumValues[11] = HPS::AttributeLock::Type::VisibilityGenericEdges; enumStrings[11] = "VisibilityGenericEdges";
			enumValues[12] = HPS::AttributeLock::Type::VisibilityHardEdges; enumStrings[12] = "VisibilityHardEdges";
			enumValues[13] = HPS::AttributeLock::Type::VisibilityAdjacentEdges; enumStrings[13] = "VisibilityAdjacentEdges";
			enumValues[14] = HPS::AttributeLock::Type::VisibilityInteriorSilhouetteEdges; enumStrings[14] = "VisibilityInteriorSilhouetteEdges";
			enumValues[15] = HPS::AttributeLock::Type::VisibilityShadowEmitting; enumStrings[15] = "VisibilityShadowEmitting";
			enumValues[16] = HPS::AttributeLock::Type::VisibilityShadowReceiving; enumStrings[16] = "VisibilityShadowReceiving";
			enumValues[17] = HPS::AttributeLock::Type::VisibilityShadowCasting; enumStrings[17] = "VisibilityShadowCasting";
			enumValues[18] = HPS::AttributeLock::Type::VisibilityMarkers; enumStrings[18] = "VisibilityMarkers";
			enumValues[19] = HPS::AttributeLock::Type::VisibilityVertices; enumStrings[19] = "VisibilityVertices";
			enumValues[20] = HPS::AttributeLock::Type::VisibilityFaces; enumStrings[20] = "VisibilityFaces";
			enumValues[21] = HPS::AttributeLock::Type::VisibilityPerimeterEdges; enumStrings[21] = "VisibilityPerimeterEdges";
			enumValues[22] = HPS::AttributeLock::Type::VisibilityNonCulledEdges; enumStrings[22] = "VisibilityNonCulledEdges";
			enumValues[23] = HPS::AttributeLock::Type::VisibilityMeshQuadEdges; enumStrings[23] = "VisibilityMeshQuadEdges";
			enumValues[24] = HPS::AttributeLock::Type::VisibilityCutGeometry; enumStrings[24] = "VisibilityCutGeometry";
			enumValues[25] = HPS::AttributeLock::Type::VisibilityEdges; enumStrings[25] = "VisibilityEdges";
			enumValues[26] = HPS::AttributeLock::Type::VisibilityGeometry; enumStrings[26] = "VisibilityGeometry";
			enumValues[27] = HPS::AttributeLock::Type::VisibilityLights; enumStrings[27] = "VisibilityLights";
			enumValues[28] = HPS::AttributeLock::Type::VisibilityShadows; enumStrings[28] = "VisibilityShadows";
			enumValues[29] = HPS::AttributeLock::Type::Material; enumStrings[29] = "Material";
			enumValues[30] = HPS::AttributeLock::Type::MaterialGeometry; enumStrings[30] = "MaterialGeometry";
			enumValues[31] = HPS::AttributeLock::Type::MaterialCutGeometry; enumStrings[31] = "MaterialCutGeometry";
			enumValues[32] = HPS::AttributeLock::Type::MaterialAmbientLightUpColor; enumStrings[32] = "MaterialAmbientLightUpColor";
			enumValues[33] = HPS::AttributeLock::Type::MaterialAmbientLightDownColor; enumStrings[33] = "MaterialAmbientLightDownColor";
			enumValues[34] = HPS::AttributeLock::Type::MaterialAmbientLightColor; enumStrings[34] = "MaterialAmbientLightColor";
			enumValues[35] = HPS::AttributeLock::Type::MaterialWindowColor; enumStrings[35] = "MaterialWindowColor";
			enumValues[36] = HPS::AttributeLock::Type::MaterialWindowContrastColor; enumStrings[36] = "MaterialWindowContrastColor";
			enumValues[37] = HPS::AttributeLock::Type::MaterialLightColor; enumStrings[37] = "MaterialLightColor";
			enumValues[38] = HPS::AttributeLock::Type::MaterialLineColor; enumStrings[38] = "MaterialLineColor";
			enumValues[39] = HPS::AttributeLock::Type::MaterialMarkerColor; enumStrings[39] = "MaterialMarkerColor";
			enumValues[40] = HPS::AttributeLock::Type::MaterialTextColor; enumStrings[40] = "MaterialTextColor";
			enumValues[41] = HPS::AttributeLock::Type::MaterialCutEdgeColor; enumStrings[41] = "MaterialCutEdgeColor";
			enumValues[42] = HPS::AttributeLock::Type::MaterialVertex; enumStrings[42] = "MaterialVertex";
			enumValues[43] = HPS::AttributeLock::Type::MaterialVertexDiffuse; enumStrings[43] = "MaterialVertexDiffuse";
			enumValues[44] = HPS::AttributeLock::Type::MaterialVertexDiffuseColor; enumStrings[44] = "MaterialVertexDiffuseColor";
			enumValues[45] = HPS::AttributeLock::Type::MaterialVertexDiffuseAlpha; enumStrings[45] = "MaterialVertexDiffuseAlpha";
			enumValues[46] = HPS::AttributeLock::Type::MaterialVertexDiffuseTexture; enumStrings[46] = "MaterialVertexDiffuseTexture";
			enumValues[47] = HPS::AttributeLock::Type::MaterialVertexSpecular; enumStrings[47] = "MaterialVertexSpecular";
			enumValues[48] = HPS::AttributeLock::Type::MaterialVertexMirror; enumStrings[48] = "MaterialVertexMirror";
			enumValues[49] = HPS::AttributeLock::Type::MaterialVertexTransmission; enumStrings[49] = "MaterialVertexTransmission";
			enumValues[50] = HPS::AttributeLock::Type::MaterialVertexEmission; enumStrings[50] = "MaterialVertexEmission";
			enumValues[51] = HPS::AttributeLock::Type::MaterialVertexEnvironment; enumStrings[51] = "MaterialVertexEnvironment";
			enumValues[52] = HPS::AttributeLock::Type::MaterialVertexBump; enumStrings[52] = "MaterialVertexBump";
			enumValues[53] = HPS::AttributeLock::Type::MaterialVertexGloss; enumStrings[53] = "MaterialVertexGloss";
			enumValues[54] = HPS::AttributeLock::Type::MaterialEdge; enumStrings[54] = "MaterialEdge";
			enumValues[55] = HPS::AttributeLock::Type::MaterialEdgeDiffuse; enumStrings[55] = "MaterialEdgeDiffuse";
			enumValues[56] = HPS::AttributeLock::Type::MaterialEdgeDiffuseColor; enumStrings[56] = "MaterialEdgeDiffuseColor";
			enumValues[57] = HPS::AttributeLock::Type::MaterialEdgeDiffuseAlpha; enumStrings[57] = "MaterialEdgeDiffuseAlpha";
			enumValues[58] = HPS::AttributeLock::Type::MaterialEdgeDiffuseTexture; enumStrings[58] = "MaterialEdgeDiffuseTexture";
			enumValues[59] = HPS::AttributeLock::Type::MaterialEdgeSpecular; enumStrings[59] = "MaterialEdgeSpecular";
			enumValues[60] = HPS::AttributeLock::Type::MaterialEdgeMirror; enumStrings[60] = "MaterialEdgeMirror";
			enumValues[61] = HPS::AttributeLock::Type::MaterialEdgeTransmission; enumStrings[61] = "MaterialEdgeTransmission";
			enumValues[62] = HPS::AttributeLock::Type::MaterialEdgeEmission; enumStrings[62] = "MaterialEdgeEmission";
			enumValues[63] = HPS::AttributeLock::Type::MaterialEdgeEnvironment; enumStrings[63] = "MaterialEdgeEnvironment";
			enumValues[64] = HPS::AttributeLock::Type::MaterialEdgeBump; enumStrings[64] = "MaterialEdgeBump";
			enumValues[65] = HPS::AttributeLock::Type::MaterialEdgeGloss; enumStrings[65] = "MaterialEdgeGloss";
			enumValues[66] = HPS::AttributeLock::Type::MaterialFace; enumStrings[66] = "MaterialFace";
			enumValues[67] = HPS::AttributeLock::Type::MaterialFaceDiffuse; enumStrings[67] = "MaterialFaceDiffuse";
			enumValues[68] = HPS::AttributeLock::Type::MaterialFaceDiffuseColor; enumStrings[68] = "MaterialFaceDiffuseColor";
			enumValues[69] = HPS::AttributeLock::Type::MaterialFaceDiffuseAlpha; enumStrings[69] = "MaterialFaceDiffuseAlpha";
			enumValues[70] = HPS::AttributeLock::Type::MaterialFaceDiffuseTexture; enumStrings[70] = "MaterialFaceDiffuseTexture";
			enumValues[71] = HPS::AttributeLock::Type::MaterialFaceSpecular; enumStrings[71] = "MaterialFaceSpecular";
			enumValues[72] = HPS::AttributeLock::Type::MaterialFaceMirror; enumStrings[72] = "MaterialFaceMirror";
			enumValues[73] = HPS::AttributeLock::Type::MaterialFaceTransmission; enumStrings[73] = "MaterialFaceTransmission";
			enumValues[74] = HPS::AttributeLock::Type::MaterialFaceEmission; enumStrings[74] = "MaterialFaceEmission";
			enumValues[75] = HPS::AttributeLock::Type::MaterialFaceEnvironment; enumStrings[75] = "MaterialFaceEnvironment";
			enumValues[76] = HPS::AttributeLock::Type::MaterialFaceBump; enumStrings[76] = "MaterialFaceBump";
			enumValues[77] = HPS::AttributeLock::Type::MaterialFaceGloss; enumStrings[77] = "MaterialFaceGloss";
			enumValues[78] = HPS::AttributeLock::Type::MaterialBackFace; enumStrings[78] = "MaterialBackFace";
			enumValues[79] = HPS::AttributeLock::Type::MaterialBackFaceDiffuse; enumStrings[79] = "MaterialBackFaceDiffuse";
			enumValues[80] = HPS::AttributeLock::Type::MaterialBackFaceDiffuseColor; enumStrings[80] = "MaterialBackFaceDiffuseColor";
			enumValues[81] = HPS::AttributeLock::Type::MaterialBackFaceDiffuseAlpha; enumStrings[81] = "MaterialBackFaceDiffuseAlpha";
			enumValues[82] = HPS::AttributeLock::Type::MaterialBackFaceDiffuseTexture; enumStrings[82] = "MaterialBackFaceDiffuseTexture";
			enumValues[83] = HPS::AttributeLock::Type::MaterialBackFaceSpecular; enumStrings[83] = "MaterialBackFaceSpecular";
			enumValues[84] = HPS::AttributeLock::Type::MaterialBackFaceMirror; enumStrings[84] = "MaterialBackFaceMirror";
			enumValues[85] = HPS::AttributeLock::Type::MaterialBackFaceTransmission; enumStrings[85] = "MaterialBackFaceTransmission";
			enumValues[86] = HPS::AttributeLock::Type::MaterialBackFaceEmission; enumStrings[86] = "MaterialBackFaceEmission";
			enumValues[87] = HPS::AttributeLock::Type::MaterialBackFaceEnvironment; enumStrings[87] = "MaterialBackFaceEnvironment";
			enumValues[88] = HPS::AttributeLock::Type::MaterialBackFaceBump; enumStrings[88] = "MaterialBackFaceBump";
			enumValues[89] = HPS::AttributeLock::Type::MaterialBackFaceGloss; enumStrings[89] = "MaterialBackFaceGloss";
			enumValues[90] = HPS::AttributeLock::Type::MaterialFrontFace; enumStrings[90] = "MaterialFrontFace";
			enumValues[91] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuse; enumStrings[91] = "MaterialFrontFaceDiffuse";
			enumValues[92] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuseColor; enumStrings[92] = "MaterialFrontFaceDiffuseColor";
			enumValues[93] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuseAlpha; enumStrings[93] = "MaterialFrontFaceDiffuseAlpha";
			enumValues[94] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuseTexture; enumStrings[94] = "MaterialFrontFaceDiffuseTexture";
			enumValues[95] = HPS::AttributeLock::Type::MaterialFrontFaceSpecular; enumStrings[95] = "MaterialFrontFaceSpecular";
			enumValues[96] = HPS::AttributeLock::Type::MaterialFrontFaceMirror; enumStrings[96] = "MaterialFrontFaceMirror";
			enumValues[97] = HPS::AttributeLock::Type::MaterialFrontFaceTransmission; enumStrings[97] = "MaterialFrontFaceTransmission";
			enumValues[98] = HPS::AttributeLock::Type::MaterialFrontFaceEmission; enumStrings[98] = "MaterialFrontFaceEmission";
			enumValues[99] = HPS::AttributeLock::Type::MaterialFrontFaceEnvironment; enumStrings[99] = "MaterialFrontFaceEnvironment";
			enumValues[100] = HPS::AttributeLock::Type::MaterialFrontFaceBump; enumStrings[100] = "MaterialFrontFaceBump";
			enumValues[101] = HPS::AttributeLock::Type::MaterialFrontFaceGloss; enumStrings[101] = "MaterialFrontFaceGloss";
			enumValues[102] = HPS::AttributeLock::Type::MaterialCutFace; enumStrings[102] = "MaterialCutFace";
			enumValues[103] = HPS::AttributeLock::Type::MaterialCutFaceDiffuse; enumStrings[103] = "MaterialCutFaceDiffuse";
			enumValues[104] = HPS::AttributeLock::Type::MaterialCutFaceDiffuseColor; enumStrings[104] = "MaterialCutFaceDiffuseColor";
			enumValues[105] = HPS::AttributeLock::Type::MaterialCutFaceDiffuseAlpha; enumStrings[105] = "MaterialCutFaceDiffuseAlpha";
			enumValues[106] = HPS::AttributeLock::Type::MaterialCutFaceDiffuseTexture; enumStrings[106] = "MaterialCutFaceDiffuseTexture";
			enumValues[107] = HPS::AttributeLock::Type::MaterialCutFaceSpecular; enumStrings[107] = "MaterialCutFaceSpecular";
			enumValues[108] = HPS::AttributeLock::Type::MaterialCutFaceMirror; enumStrings[108] = "MaterialCutFaceMirror";
			enumValues[109] = HPS::AttributeLock::Type::MaterialCutFaceTransmission; enumStrings[109] = "MaterialCutFaceTransmission";
			enumValues[110] = HPS::AttributeLock::Type::MaterialCutFaceEmission; enumStrings[110] = "MaterialCutFaceEmission";
			enumValues[111] = HPS::AttributeLock::Type::MaterialCutFaceEnvironment; enumStrings[111] = "MaterialCutFaceEnvironment";
			enumValues[112] = HPS::AttributeLock::Type::MaterialCutFaceBump; enumStrings[112] = "MaterialCutFaceBump";
			enumValues[113] = HPS::AttributeLock::Type::MaterialCutFaceGloss; enumStrings[113] = "MaterialCutFaceGloss";
			enumValues[114] = HPS::AttributeLock::Type::Camera; enumStrings[114] = "Camera";
			enumValues[115] = HPS::AttributeLock::Type::Selectability; enumStrings[115] = "Selectability";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class DrawingHandednessProperty : public BaseEnumProperty<HPS::Drawing::Handedness>
	{
	public:
		DrawingHandednessProperty(
			QTreeWidget * tree,
			HPS::Drawing::Handedness & enumValue,
			const char * name = "Handedness")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Drawing::Handedness::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Drawing::Handedness::Left; enumStrings[1] = "Left";
			enumValues[2] = HPS::Drawing::Handedness::Right; enumStrings[2] = "Right";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class DrawingOverlayProperty : public BaseEnumProperty<HPS::Drawing::Overlay>
	{
	public:
		DrawingOverlayProperty(
			QTreeWidget * tree,
			HPS::Drawing::Overlay & enumValue,
			const char * name = "Overlay")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Drawing::Overlay::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Drawing::Overlay::Default; enumStrings[1] = "Default";
			enumValues[2] = HPS::Drawing::Overlay::WithZValues; enumStrings[2] = "WithZValues";
			enumValues[3] = HPS::Drawing::Overlay::InPlace; enumStrings[3] = "InPlace";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class DrawingClipOperationProperty : public BaseEnumProperty<HPS::Drawing::ClipOperation>
	{
	public:
		DrawingClipOperationProperty(
			QTreeWidget * tree,
			HPS::Drawing::ClipOperation & enumValue,
			const char * name = "ClipOperation")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Drawing::ClipOperation::Keep; enumStrings[0] = "Keep";
			enumValues[1] = HPS::Drawing::ClipOperation::Remove; enumStrings[1] = "Remove";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class DrawingClipSpaceProperty : public BaseEnumProperty<HPS::Drawing::ClipSpace>
	{
	public:
		DrawingClipSpaceProperty(
			QTreeWidget * tree,
			HPS::Drawing::ClipSpace & enumValue,
			const char * name = "ClipSpace")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Drawing::ClipSpace::Window; enumStrings[0] = "Window";
			enumValues[1] = HPS::Drawing::ClipSpace::World; enumStrings[1] = "World";
			enumValues[2] = HPS::Drawing::ClipSpace::Object; enumStrings[2] = "Object";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class HiddenLineAlgorithmProperty : public BaseEnumProperty<HPS::HiddenLine::Algorithm>
	{
	public:
		HiddenLineAlgorithmProperty(
			QTreeWidget * tree,
			HPS::HiddenLine::Algorithm & enumValue,
			const char * name = "Algorithm")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::HiddenLine::Algorithm::None; enumStrings[0] = "None";
			enumValues[1] = HPS::HiddenLine::Algorithm::ZBuffer; enumStrings[1] = "ZBuffer";
			enumValues[2] = HPS::HiddenLine::Algorithm::ZSort; enumStrings[2] = "ZSort";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SelectionLevelProperty : public BaseEnumProperty<HPS::Selection::Level>
	{
	public:
		SelectionLevelProperty(
			QTreeWidget * tree,
			HPS::Selection::Level & enumValue,
			const char * name = "Level")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Selection::Level::Segment; enumStrings[0] = "Segment";
			enumValues[1] = HPS::Selection::Level::Entity; enumStrings[1] = "Entity";
			enumValues[2] = HPS::Selection::Level::Subentity; enumStrings[2] = "Subentity";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SelectionSortingProperty : public BaseEnumProperty<HPS::Selection::Sorting>
	{
	public:
		SelectionSortingProperty(
			QTreeWidget * tree,
			HPS::Selection::Sorting & enumValue,
			const char * name = "Sorting")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Selection::Sorting::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::Selection::Sorting::Proximity; enumStrings[1] = "Proximity";
			enumValues[2] = HPS::Selection::Sorting::ZSorting; enumStrings[2] = "ZSorting";
			enumValues[3] = HPS::Selection::Sorting::Default; enumStrings[3] = "Default";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SelectionAlgorithmProperty : public BaseEnumProperty<HPS::Selection::Algorithm>
	{
	public:
		SelectionAlgorithmProperty(
			QTreeWidget * tree,
			HPS::Selection::Algorithm & enumValue,
			const char * name = "Algorithm")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Selection::Algorithm::Visual; enumStrings[0] = "Visual";
			enumValues[1] = HPS::Selection::Algorithm::Analytic; enumStrings[1] = "Analytic";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SelectionGranularityProperty : public BaseEnumProperty<HPS::Selection::Granularity>
	{
	public:
		SelectionGranularityProperty(
			QTreeWidget * tree,
			HPS::Selection::Granularity & enumValue,
			const char * name = "Granularity")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Selection::Granularity::General; enumStrings[0] = "General";
			enumValues[1] = HPS::Selection::Granularity::Detailed; enumStrings[1] = "Detailed";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CameraProjectionProperty : public BaseEnumProperty<HPS::Camera::Projection>
	{
	public:
		CameraProjectionProperty(
			QTreeWidget * tree,
			HPS::Camera::Projection & enumValue,
			const char * name = "Projection")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Camera::Projection::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Camera::Projection::Perspective; enumStrings[1] = "Perspective";
			enumValues[2] = HPS::Camera::Projection::Orthographic; enumStrings[2] = "Orthographic";
			enumValues[3] = HPS::Camera::Projection::Stretched; enumStrings[3] = "Stretched";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SelectabilityValueProperty : public BaseEnumProperty<HPS::Selectability::Value>
	{
	public:
		SelectabilityValueProperty(
			QTreeWidget * tree,
			HPS::Selectability::Value & enumValue,
			const char * name = "Value")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Selectability::Value::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::Selectability::Value::On; enumStrings[1] = "On";
			enumValues[2] = HPS::Selectability::Value::ForcedOn; enumStrings[2] = "ForcedOn";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TransparencyMethodProperty : public BaseEnumProperty<HPS::Transparency::Method>
	{
	public:
		TransparencyMethodProperty(
			QTreeWidget * tree,
			HPS::Transparency::Method & enumValue,
			const char * name = "Method")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Transparency::Method::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Transparency::Method::Blended; enumStrings[1] = "Blended";
			enumValues[2] = HPS::Transparency::Method::ScreenDoor; enumStrings[2] = "ScreenDoor";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TransparencyAlgorithmProperty : public BaseEnumProperty<HPS::Transparency::Algorithm>
	{
	public:
		TransparencyAlgorithmProperty(
			QTreeWidget * tree,
			HPS::Transparency::Algorithm & enumValue,
			const char * name = "Algorithm")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Transparency::Algorithm::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Transparency::Algorithm::Painters; enumStrings[1] = "Painters";
			enumValues[2] = HPS::Transparency::Algorithm::ZSortNicest; enumStrings[2] = "ZSortNicest";
			enumValues[3] = HPS::Transparency::Algorithm::ZSortFastest; enumStrings[3] = "ZSortFastest";
			enumValues[4] = HPS::Transparency::Algorithm::DepthPeeling; enumStrings[4] = "DepthPeeling";
			enumValues[5] = HPS::Transparency::Algorithm::WeightedBlended; enumStrings[5] = "WeightedBlended";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TransparencyAreaUnitsProperty : public BaseEnumProperty<HPS::Transparency::AreaUnits>
	{
	public:
		TransparencyAreaUnitsProperty(
			QTreeWidget * tree,
			HPS::Transparency::AreaUnits & enumValue,
			const char * name = "AreaUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Transparency::AreaUnits::Percent; enumStrings[0] = "Percent";
			enumValues[1] = HPS::Transparency::AreaUnits::Pixels; enumStrings[1] = "Pixels";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TransparencyPreferenceProperty : public BaseEnumProperty<HPS::Transparency::Preference>
	{
	public:
		TransparencyPreferenceProperty(
			QTreeWidget * tree,
			HPS::Transparency::Preference & enumValue,
			const char * name = "Preference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Transparency::Preference::Nicest; enumStrings[0] = "Nicest";
			enumValues[1] = HPS::Transparency::Preference::Fastest; enumStrings[1] = "Fastest";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MarkerDrawingPreferenceProperty : public BaseEnumProperty<HPS::Marker::DrawingPreference>
	{
	public:
		MarkerDrawingPreferenceProperty(
			QTreeWidget * tree,
			HPS::Marker::DrawingPreference & enumValue,
			const char * name = "DrawingPreference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Marker::DrawingPreference::Nicest; enumStrings[0] = "Nicest";
			enumValues[1] = HPS::Marker::DrawingPreference::Fastest; enumStrings[1] = "Fastest";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class MarkerSizeUnitsProperty : public BaseEnumProperty<HPS::Marker::SizeUnits>
	{
	public:
		MarkerSizeUnitsProperty(
			QTreeWidget * tree,
			HPS::Marker::SizeUnits & enumValue,
			const char * name = "SizeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Marker::SizeUnits::ScaleFactor; enumStrings[0] = "ScaleFactor";
			enumValues[1] = HPS::Marker::SizeUnits::ObjectSpace; enumStrings[1] = "ObjectSpace";
			enumValues[2] = HPS::Marker::SizeUnits::SubscreenRelative; enumStrings[2] = "SubscreenRelative";
			enumValues[3] = HPS::Marker::SizeUnits::WindowRelative; enumStrings[3] = "WindowRelative";
			enumValues[4] = HPS::Marker::SizeUnits::WorldSpace; enumStrings[4] = "WorldSpace";
			enumValues[5] = HPS::Marker::SizeUnits::Points; enumStrings[5] = "Points";
			enumValues[6] = HPS::Marker::SizeUnits::Pixels; enumStrings[6] = "Pixels";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LightingInterpolationAlgorithmProperty : public BaseEnumProperty<HPS::Lighting::InterpolationAlgorithm>
	{
	public:
		LightingInterpolationAlgorithmProperty(
			QTreeWidget * tree,
			HPS::Lighting::InterpolationAlgorithm & enumValue,
			const char * name = "InterpolationAlgorithm")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Lighting::InterpolationAlgorithm::Phong; enumStrings[0] = "Phong";
			enumValues[1] = HPS::Lighting::InterpolationAlgorithm::Gouraud; enumStrings[1] = "Gouraud";
			enumValues[2] = HPS::Lighting::InterpolationAlgorithm::Flat; enumStrings[2] = "Flat";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextAlignmentProperty : public BaseEnumProperty<HPS::Text::Alignment>
	{
	public:
		TextAlignmentProperty(
			QTreeWidget * tree,
			HPS::Text::Alignment & enumValue,
			const char * name = "Alignment")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::Text::Alignment::TopLeft; enumStrings[0] = "TopLeft";
			enumValues[1] = HPS::Text::Alignment::CenterLeft; enumStrings[1] = "CenterLeft";
			enumValues[2] = HPS::Text::Alignment::BottomLeft; enumStrings[2] = "BottomLeft";
			enumValues[3] = HPS::Text::Alignment::TopCenter; enumStrings[3] = "TopCenter";
			enumValues[4] = HPS::Text::Alignment::Center; enumStrings[4] = "Center";
			enumValues[5] = HPS::Text::Alignment::BottomCenter; enumStrings[5] = "BottomCenter";
			enumValues[6] = HPS::Text::Alignment::TopRight; enumStrings[6] = "TopRight";
			enumValues[7] = HPS::Text::Alignment::CenterRight; enumStrings[7] = "CenterRight";
			enumValues[8] = HPS::Text::Alignment::BottomRight; enumStrings[8] = "BottomRight";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextReferenceFrameProperty : public BaseEnumProperty<HPS::Text::ReferenceFrame>
	{
	public:
		TextReferenceFrameProperty(
			QTreeWidget * tree,
			HPS::Text::ReferenceFrame & enumValue,
			const char * name = "ReferenceFrame")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Text::ReferenceFrame::WorldAligned; enumStrings[0] = "WorldAligned";
			enumValues[1] = HPS::Text::ReferenceFrame::PathAligned; enumStrings[1] = "PathAligned";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextJustificationProperty : public BaseEnumProperty<HPS::Text::Justification>
	{
	public:
		TextJustificationProperty(
			QTreeWidget * tree,
			HPS::Text::Justification & enumValue,
			const char * name = "Justification")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::Justification::Left; enumStrings[0] = "Left";
			enumValues[1] = HPS::Text::Justification::Right; enumStrings[1] = "Right";
			enumValues[2] = HPS::Text::Justification::Center; enumStrings[2] = "Center";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextTransformProperty : public BaseEnumProperty<HPS::Text::Transform>
	{
	public:
		TextTransformProperty(
			QTreeWidget * tree,
			HPS::Text::Transform & enumValue,
			const char * name = "Transform")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Text::Transform::Transformable; enumStrings[0] = "Transformable";
			enumValues[1] = HPS::Text::Transform::NonTransformable; enumStrings[1] = "NonTransformable";
			enumValues[2] = HPS::Text::Transform::CharacterPositionOnly; enumStrings[2] = "CharacterPositionOnly";
			enumValues[3] = HPS::Text::Transform::CharacterPositionAdjusted; enumStrings[3] = "CharacterPositionAdjusted";
			enumValues[4] = HPS::Text::Transform::NonScalingTransformable; enumStrings[4] = "NonScalingTransformable";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextRendererProperty : public BaseEnumProperty<HPS::Text::Renderer>
	{
	public:
		TextRendererProperty(
			QTreeWidget * tree,
			HPS::Text::Renderer & enumValue,
			const char * name = "Renderer")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::Renderer::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Text::Renderer::Driver; enumStrings[1] = "Driver";
			enumValues[2] = HPS::Text::Renderer::Truetype; enumStrings[2] = "Truetype";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextPreferenceProperty : public BaseEnumProperty<HPS::Text::Preference>
	{
	public:
		TextPreferenceProperty(
			QTreeWidget * tree,
			HPS::Text::Preference & enumValue,
			const char * name = "Preference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Text::Preference::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Text::Preference::Vector; enumStrings[1] = "Vector";
			enumValues[2] = HPS::Text::Preference::Raster; enumStrings[2] = "Raster";
			enumValues[3] = HPS::Text::Preference::Exterior; enumStrings[3] = "Exterior";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextSizeUnitsProperty : public BaseEnumProperty<HPS::Text::SizeUnits>
	{
	public:
		TextSizeUnitsProperty(
			QTreeWidget * tree,
			HPS::Text::SizeUnits & enumValue,
			const char * name = "SizeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Text::SizeUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::SizeUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::SizeUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::SizeUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::SizeUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::SizeUnits::Pixels; enumStrings[5] = "Pixels";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextSizeToleranceUnitsProperty : public BaseEnumProperty<HPS::Text::SizeToleranceUnits>
	{
	public:
		TextSizeToleranceUnitsProperty(
			QTreeWidget * tree,
			HPS::Text::SizeToleranceUnits & enumValue,
			const char * name = "SizeToleranceUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Text::SizeToleranceUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::SizeToleranceUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::SizeToleranceUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::SizeToleranceUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::SizeToleranceUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::SizeToleranceUnits::Pixels; enumStrings[5] = "Pixels";
			enumValues[6] = HPS::Text::SizeToleranceUnits::Percent; enumStrings[6] = "Percent";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextMarginUnitsProperty : public BaseEnumProperty<HPS::Text::MarginUnits>
	{
	public:
		TextMarginUnitsProperty(
			QTreeWidget * tree,
			HPS::Text::MarginUnits & enumValue,
			const char * name = "MarginUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Text::MarginUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::MarginUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::MarginUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::MarginUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::MarginUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::MarginUnits::Pixels; enumStrings[5] = "Pixels";
			enumValues[6] = HPS::Text::MarginUnits::Percent; enumStrings[6] = "Percent";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextGreekingUnitsProperty : public BaseEnumProperty<HPS::Text::GreekingUnits>
	{
	public:
		TextGreekingUnitsProperty(
			QTreeWidget * tree,
			HPS::Text::GreekingUnits & enumValue,
			const char * name = "GreekingUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Text::GreekingUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::GreekingUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::GreekingUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::GreekingUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::GreekingUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::GreekingUnits::Pixels; enumStrings[5] = "Pixels";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextGreekingModeProperty : public BaseEnumProperty<HPS::Text::GreekingMode>
	{
	public:
		TextGreekingModeProperty(
			QTreeWidget * tree,
			HPS::Text::GreekingMode & enumValue,
			const char * name = "GreekingMode")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::GreekingMode::Nothing; enumStrings[0] = "Nothing";
			enumValues[1] = HPS::Text::GreekingMode::Lines; enumStrings[1] = "Lines";
			enumValues[2] = HPS::Text::GreekingMode::Box; enumStrings[2] = "Box";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextRegionAlignmentProperty : public BaseEnumProperty<HPS::Text::RegionAlignment>
	{
	public:
		TextRegionAlignmentProperty(
			QTreeWidget * tree,
			HPS::Text::RegionAlignment & enumValue,
			const char * name = "RegionAlignment")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::RegionAlignment::Top; enumStrings[0] = "Top";
			enumValues[1] = HPS::Text::RegionAlignment::Center; enumStrings[1] = "Center";
			enumValues[2] = HPS::Text::RegionAlignment::Bottom; enumStrings[2] = "Bottom";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextLeaderLineSpaceProperty : public BaseEnumProperty<HPS::Text::LeaderLineSpace>
	{
	public:
		TextLeaderLineSpaceProperty(
			QTreeWidget * tree,
			HPS::Text::LeaderLineSpace & enumValue,
			const char * name = "LeaderLineSpace")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Text::LeaderLineSpace::Object; enumStrings[0] = "Object";
			enumValues[1] = HPS::Text::LeaderLineSpace::World; enumStrings[1] = "World";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class TextRegionFittingProperty : public BaseEnumProperty<HPS::Text::RegionFitting>
	{
	public:
		TextRegionFittingProperty(
			QTreeWidget * tree,
			HPS::Text::RegionFitting & enumValue,
			const char * name = "RegionFitting")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Text::RegionFitting::Left; enumStrings[0] = "Left";
			enumValues[1] = HPS::Text::RegionFitting::Center; enumStrings[1] = "Center";
			enumValues[2] = HPS::Text::RegionFitting::Right; enumStrings[2] = "Right";
			enumValues[3] = HPS::Text::RegionFitting::Spacing; enumStrings[3] = "Spacing";
			enumValues[4] = HPS::Text::RegionFitting::Width; enumStrings[4] = "Width";
			enumValues[5] = HPS::Text::RegionFitting::Auto; enumStrings[5] = "Auto";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LineCoordinateSpaceProperty : public BaseEnumProperty<HPS::Line::CoordinateSpace>
	{
	public:
		LineCoordinateSpaceProperty(
			QTreeWidget * tree,
			HPS::Line::CoordinateSpace & enumValue,
			const char * name = "CoordinateSpace")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Line::CoordinateSpace::Object; enumStrings[0] = "Object";
			enumValues[1] = HPS::Line::CoordinateSpace::World; enumStrings[1] = "World";
			enumValues[2] = HPS::Line::CoordinateSpace::NormalizedInnerWindow; enumStrings[2] = "NormalizedInnerWindow";
			enumValues[3] = HPS::Line::CoordinateSpace::NormalizedInnerPixel; enumStrings[3] = "NormalizedInnerPixel";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LineSizeUnitsProperty : public BaseEnumProperty<HPS::Line::SizeUnits>
	{
	public:
		LineSizeUnitsProperty(
			QTreeWidget * tree,
			HPS::Line::SizeUnits & enumValue,
			const char * name = "SizeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Line::SizeUnits::ScaleFactor; enumStrings[0] = "ScaleFactor";
			enumValues[1] = HPS::Line::SizeUnits::ObjectSpace; enumStrings[1] = "ObjectSpace";
			enumValues[2] = HPS::Line::SizeUnits::SubscreenRelative; enumStrings[2] = "SubscreenRelative";
			enumValues[3] = HPS::Line::SizeUnits::WindowRelative; enumStrings[3] = "WindowRelative";
			enumValues[4] = HPS::Line::SizeUnits::WorldSpace; enumStrings[4] = "WorldSpace";
			enumValues[5] = HPS::Line::SizeUnits::Points; enumStrings[5] = "Points";
			enumValues[6] = HPS::Line::SizeUnits::Pixels; enumStrings[6] = "Pixels";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class EdgeSizeUnitsProperty : public BaseEnumProperty<HPS::Edge::SizeUnits>
	{
	public:
		EdgeSizeUnitsProperty(
			QTreeWidget * tree,
			HPS::Edge::SizeUnits & enumValue,
			const char * name = "SizeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Edge::SizeUnits::ScaleFactor; enumStrings[0] = "ScaleFactor";
			enumValues[1] = HPS::Edge::SizeUnits::ObjectSpace; enumStrings[1] = "ObjectSpace";
			enumValues[2] = HPS::Edge::SizeUnits::SubscreenRelative; enumStrings[2] = "SubscreenRelative";
			enumValues[3] = HPS::Edge::SizeUnits::WindowRelative; enumStrings[3] = "WindowRelative";
			enumValues[4] = HPS::Edge::SizeUnits::WorldSpace; enumStrings[4] = "WorldSpace";
			enumValues[5] = HPS::Edge::SizeUnits::Points; enumStrings[5] = "Points";
			enumValues[6] = HPS::Edge::SizeUnits::Pixels; enumStrings[6] = "Pixels";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CuttingSectionModeProperty : public BaseEnumProperty<HPS::CuttingSection::Mode>
	{
	public:
		CuttingSectionModeProperty(
			QTreeWidget * tree,
			HPS::CuttingSection::Mode & enumValue,
			const char * name = "Mode")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::CuttingSection::Mode::None; enumStrings[0] = "None";
			enumValues[1] = HPS::CuttingSection::Mode::Round; enumStrings[1] = "Round";
			enumValues[2] = HPS::CuttingSection::Mode::Square; enumStrings[2] = "Square";
			enumValues[3] = HPS::CuttingSection::Mode::Plane; enumStrings[3] = "Plane";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CuttingSectionCappingLevelProperty : public BaseEnumProperty<HPS::CuttingSection::CappingLevel>
	{
	public:
		CuttingSectionCappingLevelProperty(
			QTreeWidget * tree,
			HPS::CuttingSection::CappingLevel & enumValue,
			const char * name = "CappingLevel")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::CuttingSection::CappingLevel::Entity; enumStrings[0] = "Entity";
			enumValues[1] = HPS::CuttingSection::CappingLevel::Segment; enumStrings[1] = "Segment";
			enumValues[2] = HPS::CuttingSection::CappingLevel::SegmentTree; enumStrings[2] = "SegmentTree";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CuttingSectionCappingUsageProperty : public BaseEnumProperty<HPS::CuttingSection::CappingUsage>
	{
	public:
		CuttingSectionCappingUsageProperty(
			QTreeWidget * tree,
			HPS::CuttingSection::CappingUsage & enumValue,
			const char * name = "CappingUsage")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::CuttingSection::CappingUsage::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::CuttingSection::CappingUsage::On; enumStrings[1] = "On";
			enumValues[2] = HPS::CuttingSection::CappingUsage::Visibility; enumStrings[2] = "Visibility";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CuttingSectionToleranceUnitsProperty : public BaseEnumProperty<HPS::CuttingSection::ToleranceUnits>
	{
	public:
		CuttingSectionToleranceUnitsProperty(
			QTreeWidget * tree,
			HPS::CuttingSection::ToleranceUnits & enumValue,
			const char * name = "ToleranceUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::CuttingSection::ToleranceUnits::Percent; enumStrings[0] = "Percent";
			enumValues[1] = HPS::CuttingSection::ToleranceUnits::WorldSpace; enumStrings[1] = "WorldSpace";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CuttingSectionCuttingLevelProperty : public BaseEnumProperty<HPS::CuttingSection::CuttingLevel>
	{
	public:
		CuttingSectionCuttingLevelProperty(
			QTreeWidget * tree,
			HPS::CuttingSection::CuttingLevel & enumValue,
			const char * name = "CuttingLevel")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::CuttingSection::CuttingLevel::Global; enumStrings[0] = "Global";
			enumValues[1] = HPS::CuttingSection::CuttingLevel::Local; enumStrings[1] = "Local";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CuttingSectionMaterialPreferenceProperty : public BaseEnumProperty<HPS::CuttingSection::MaterialPreference>
	{
	public:
		CuttingSectionMaterialPreferenceProperty(
			QTreeWidget * tree,
			HPS::CuttingSection::MaterialPreference & enumValue,
			const char * name = "MaterialPreference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::CuttingSection::MaterialPreference::Explicit; enumStrings[0] = "Explicit";
			enumValues[1] = HPS::CuttingSection::MaterialPreference::Implicit; enumStrings[1] = "Implicit";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LinePatternSizeUnitsProperty : public BaseEnumProperty<HPS::LinePattern::SizeUnits>
	{
	public:
		LinePatternSizeUnitsProperty(
			QTreeWidget * tree,
			HPS::LinePattern::SizeUnits & enumValue,
			const char * name = "SizeUnits")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::LinePattern::SizeUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::LinePattern::SizeUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::LinePattern::SizeUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::LinePattern::SizeUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::LinePattern::SizeUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::LinePattern::SizeUnits::Pixels; enumStrings[5] = "Pixels";
			enumValues[6] = HPS::LinePattern::SizeUnits::ScaleFactor; enumStrings[6] = "ScaleFactor";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LinePatternInsetBehaviorProperty : public BaseEnumProperty<HPS::LinePattern::InsetBehavior>
	{
	public:
		LinePatternInsetBehaviorProperty(
			QTreeWidget * tree,
			HPS::LinePattern::InsetBehavior & enumValue,
			const char * name = "InsetBehavior")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::LinePattern::InsetBehavior::Overlap; enumStrings[0] = "Overlap";
			enumValues[1] = HPS::LinePattern::InsetBehavior::Trim; enumStrings[1] = "Trim";
			enumValues[2] = HPS::LinePattern::InsetBehavior::Inline; enumStrings[2] = "Inline";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LinePatternJoinProperty : public BaseEnumProperty<HPS::LinePattern::Join>
	{
	public:
		LinePatternJoinProperty(
			QTreeWidget * tree,
			HPS::LinePattern::Join & enumValue,
			const char * name = "Join")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::LinePattern::Join::Mitre; enumStrings[0] = "Mitre";
			enumValues[1] = HPS::LinePattern::Join::Round; enumStrings[1] = "Round";
			enumValues[2] = HPS::LinePattern::Join::Bevel; enumStrings[2] = "Bevel";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LinePatternCapProperty : public BaseEnumProperty<HPS::LinePattern::Cap>
	{
	public:
		LinePatternCapProperty(
			QTreeWidget * tree,
			HPS::LinePattern::Cap & enumValue,
			const char * name = "Cap")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::LinePattern::Cap::Butt; enumStrings[0] = "Butt";
			enumValues[1] = HPS::LinePattern::Cap::Square; enumStrings[1] = "Square";
			enumValues[2] = HPS::LinePattern::Cap::Round; enumStrings[2] = "Round";
			enumValues[3] = HPS::LinePattern::Cap::Mitre; enumStrings[3] = "Mitre";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LinePatternJustificationProperty : public BaseEnumProperty<HPS::LinePattern::Justification>
	{
	public:
		LinePatternJustificationProperty(
			QTreeWidget * tree,
			HPS::LinePattern::Justification & enumValue,
			const char * name = "Justification")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::LinePattern::Justification::Center; enumStrings[0] = "Center";
			enumValues[1] = HPS::LinePattern::Justification::Stretch; enumStrings[1] = "Stretch";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class GlyphFillProperty : public BaseEnumProperty<HPS::Glyph::Fill>
	{
	public:
		GlyphFillProperty(
			QTreeWidget * tree,
			HPS::Glyph::Fill & enumValue,
			const char * name = "Fill")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Glyph::Fill::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Glyph::Fill::Continuous; enumStrings[1] = "Continuous";
			enumValues[2] = HPS::Glyph::Fill::New; enumStrings[2] = "New";
			enumValues[3] = HPS::Glyph::Fill::NewLoop; enumStrings[3] = "NewLoop";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ConditionIntrinsicProperty : public BaseEnumProperty<HPS::Condition::Intrinsic>
	{
	public:
		ConditionIntrinsicProperty(
			QTreeWidget * tree,
			HPS::Condition::Intrinsic & enumValue,
			const char * name = "Intrinsic")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Condition::Intrinsic::Extent; enumStrings[0] = "Extent";
			enumValues[1] = HPS::Condition::Intrinsic::DrawPass; enumStrings[1] = "DrawPass";
			enumValues[2] = HPS::Condition::Intrinsic::InnerPixelWidth; enumStrings[2] = "InnerPixelWidth";
			enumValues[3] = HPS::Condition::Intrinsic::InnerPixelHeight; enumStrings[3] = "InnerPixelHeight";
			enumValues[4] = HPS::Condition::Intrinsic::Selection; enumStrings[4] = "Selection";
			enumValues[5] = HPS::Condition::Intrinsic::QuickMovesProbe; enumStrings[5] = "QuickMovesProbe";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class GridTypeProperty : public BaseEnumProperty<HPS::Grid::Type>
	{
	public:
		GridTypeProperty(
			QTreeWidget * tree,
			HPS::Grid::Type & enumValue,
			const char * name = "Type")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Grid::Type::Quadrilateral; enumStrings[0] = "Quadrilateral";
			enumValues[1] = HPS::Grid::Type::Radial; enumStrings[1] = "Radial";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class GPUPreferenceProperty : public BaseEnumProperty<HPS::GPU::Preference>
	{
	public:
		GPUPreferenceProperty(
			QTreeWidget * tree,
			HPS::GPU::Preference & enumValue,
			const char * name = "Preference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::GPU::Preference::HighPerformance; enumStrings[0] = "HighPerformance";
			enumValues[1] = HPS::GPU::Preference::Integrated; enumStrings[1] = "Integrated";
			enumValues[2] = HPS::GPU::Preference::Specific; enumStrings[2] = "Specific";
			enumValues[3] = HPS::GPU::Preference::Default; enumStrings[3] = "Default";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class CullingFaceProperty : public BaseEnumProperty<HPS::Culling::Face>
	{
	public:
		CullingFaceProperty(
			QTreeWidget * tree,
			HPS::Culling::Face & enumValue,
			const char * name = "Face")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Culling::Face::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::Culling::Face::Back; enumStrings[1] = "Back";
			enumValues[2] = HPS::Culling::Face::Front; enumStrings[2] = "Front";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderPrimitivesProperty : public BaseEnumProperty<HPS::Shader::Primitives>
	{
	public:
		ShaderPrimitivesProperty(
			QTreeWidget * tree,
			HPS::Shader::Primitives & enumValue,
			const char * name = "Primitives")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Shader::Primitives::NoFlags; enumStrings[0] = "NoFlags";
			enumValues[1] = HPS::Shader::Primitives::Triangles; enumStrings[1] = "Triangles";
			enumValues[2] = HPS::Shader::Primitives::Lines; enumStrings[2] = "Lines";
			enumValues[3] = HPS::Shader::Primitives::Points; enumStrings[3] = "Points";
			enumValues[4] = HPS::Shader::Primitives::All; enumStrings[4] = "All";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderPixelInputsProperty : public BaseEnumProperty<HPS::Shader::PixelInputs>
	{
	public:
		ShaderPixelInputsProperty(
			QTreeWidget * tree,
			HPS::Shader::PixelInputs & enumValue,
			const char * name = "PixelInputs")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(14); HPS::UTF8Array enumStrings(14);
			enumValues[0] = HPS::Shader::PixelInputs::NoFlags; enumStrings[0] = "NoFlags";
			enumValues[1] = HPS::Shader::PixelInputs::TexCoord0; enumStrings[1] = "TexCoord0";
			enumValues[2] = HPS::Shader::PixelInputs::TexCoord1; enumStrings[2] = "TexCoord1";
			enumValues[3] = HPS::Shader::PixelInputs::TexCoord2; enumStrings[3] = "TexCoord2";
			enumValues[4] = HPS::Shader::PixelInputs::TexCoord3; enumStrings[4] = "TexCoord3";
			enumValues[5] = HPS::Shader::PixelInputs::TexCoord4; enumStrings[5] = "TexCoord4";
			enumValues[6] = HPS::Shader::PixelInputs::TexCoord5; enumStrings[6] = "TexCoord5";
			enumValues[7] = HPS::Shader::PixelInputs::TexCoord6; enumStrings[7] = "TexCoord6";
			enumValues[8] = HPS::Shader::PixelInputs::TexCoord7; enumStrings[8] = "TexCoord7";
			enumValues[9] = HPS::Shader::PixelInputs::AnyTexCoords; enumStrings[9] = "AnyTexCoords";
			enumValues[10] = HPS::Shader::PixelInputs::EyePosition; enumStrings[10] = "EyePosition";
			enumValues[11] = HPS::Shader::PixelInputs::EyeNormal; enumStrings[11] = "EyeNormal";
			enumValues[12] = HPS::Shader::PixelInputs::ObjectView; enumStrings[12] = "ObjectView";
			enumValues[13] = HPS::Shader::PixelInputs::ObjectNormal; enumStrings[13] = "ObjectNormal";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderVertexInputsProperty : public BaseEnumProperty<HPS::Shader::VertexInputs>
	{
	public:
		ShaderVertexInputsProperty(
			QTreeWidget * tree,
			HPS::Shader::VertexInputs & enumValue,
			const char * name = "VertexInputs")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(11); HPS::UTF8Array enumStrings(11);
			enumValues[0] = HPS::Shader::VertexInputs::NoFlags; enumStrings[0] = "NoFlags";
			enumValues[1] = HPS::Shader::VertexInputs::TexCoord0; enumStrings[1] = "TexCoord0";
			enumValues[2] = HPS::Shader::VertexInputs::TexCoord1; enumStrings[2] = "TexCoord1";
			enumValues[3] = HPS::Shader::VertexInputs::TexCoord2; enumStrings[3] = "TexCoord2";
			enumValues[4] = HPS::Shader::VertexInputs::TexCoord3; enumStrings[4] = "TexCoord3";
			enumValues[5] = HPS::Shader::VertexInputs::TexCoord4; enumStrings[5] = "TexCoord4";
			enumValues[6] = HPS::Shader::VertexInputs::TexCoord5; enumStrings[6] = "TexCoord5";
			enumValues[7] = HPS::Shader::VertexInputs::TexCoord6; enumStrings[7] = "TexCoord6";
			enumValues[8] = HPS::Shader::VertexInputs::TexCoord7; enumStrings[8] = "TexCoord7";
			enumValues[9] = HPS::Shader::VertexInputs::AnyTexCoords; enumStrings[9] = "AnyTexCoords";
			enumValues[10] = HPS::Shader::VertexInputs::Normal; enumStrings[10] = "Normal";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderUniformPrecisionProperty : public BaseEnumProperty<HPS::Shader::UniformPrecision>
	{
	public:
		ShaderUniformPrecisionProperty(
			QTreeWidget * tree,
			HPS::Shader::UniformPrecision & enumValue,
			const char * name = "UniformPrecision")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Shader::UniformPrecision::NoFlags; enumStrings[0] = "NoFlags";
			enumValues[1] = HPS::Shader::UniformPrecision::Low; enumStrings[1] = "Low";
			enumValues[2] = HPS::Shader::UniformPrecision::Medium; enumStrings[2] = "Medium";
			enumValues[3] = HPS::Shader::UniformPrecision::High; enumStrings[3] = "High";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderSamplerOptionsProperty : public BaseEnumProperty<HPS::Shader::Sampler::Options>
	{
	public:
		ShaderSamplerOptionsProperty(
			QTreeWidget * tree,
			HPS::Shader::Sampler::Options & enumValue,
			const char * name = "Options")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::Shader::Sampler::Options::MinFilter; enumStrings[0] = "MinFilter";
			enumValues[1] = HPS::Shader::Sampler::Options::MagFilter; enumStrings[1] = "MagFilter";
			enumValues[2] = HPS::Shader::Sampler::Options::MipFilter; enumStrings[2] = "MipFilter";
			enumValues[3] = HPS::Shader::Sampler::Options::MaxAnisotropy; enumStrings[3] = "MaxAnisotropy";
			enumValues[4] = HPS::Shader::Sampler::Options::MinLOD; enumStrings[4] = "MinLOD";
			enumValues[5] = HPS::Shader::Sampler::Options::MaxLOD; enumStrings[5] = "MaxLOD";
			enumValues[6] = HPS::Shader::Sampler::Options::WidthAddress; enumStrings[6] = "WidthAddress";
			enumValues[7] = HPS::Shader::Sampler::Options::HeightAddress; enumStrings[7] = "HeightAddress";
			enumValues[8] = HPS::Shader::Sampler::Options::BorderColor; enumStrings[8] = "BorderColor";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderSamplerFilterProperty : public BaseEnumProperty<HPS::Shader::Sampler::Filter>
	{
	public:
		ShaderSamplerFilterProperty(
			QTreeWidget * tree,
			HPS::Shader::Sampler::Filter & enumValue,
			const char * name = "Filter")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Shader::Sampler::Filter::Nearest; enumStrings[0] = "Nearest";
			enumValues[1] = HPS::Shader::Sampler::Filter::Linear; enumStrings[1] = "Linear";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderSamplerAddressModeProperty : public BaseEnumProperty<HPS::Shader::Sampler::AddressMode>
	{
	public:
		ShaderSamplerAddressModeProperty(
			QTreeWidget * tree,
			HPS::Shader::Sampler::AddressMode & enumValue,
			const char * name = "AddressMode")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Shader::Sampler::AddressMode::Repeat; enumStrings[0] = "Repeat";
			enumValues[1] = HPS::Shader::Sampler::AddressMode::MirrorRepeat; enumStrings[1] = "MirrorRepeat";
			enumValues[2] = HPS::Shader::Sampler::AddressMode::ClampToEdge; enumStrings[2] = "ClampToEdge";
			enumValues[3] = HPS::Shader::Sampler::AddressMode::ClampToBorder; enumStrings[3] = "ClampToBorder";
			enumValues[4] = HPS::Shader::Sampler::AddressMode::MirrorClampToEdge; enumStrings[4] = "MirrorClampToEdge";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderSamplerBorderColorProperty : public BaseEnumProperty<HPS::Shader::Sampler::BorderColor>
	{
	public:
		ShaderSamplerBorderColorProperty(
			QTreeWidget * tree,
			HPS::Shader::Sampler::BorderColor & enumValue,
			const char * name = "BorderColor")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Shader::Sampler::BorderColor::TransparentBlack; enumStrings[0] = "TransparentBlack";
			enumValues[1] = HPS::Shader::Sampler::BorderColor::Transparent; enumStrings[1] = "Transparent";
			enumValues[2] = HPS::Shader::Sampler::BorderColor::OpaqueBlack; enumStrings[2] = "OpaqueBlack";
			enumValues[3] = HPS::Shader::Sampler::BorderColor::OpaqueWhite; enumStrings[3] = "OpaqueWhite";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class ShaderTextureFormatProperty : public BaseEnumProperty<HPS::Shader::Texture::Format>
	{
	public:
		ShaderTextureFormatProperty(
			QTreeWidget * tree,
			HPS::Shader::Texture::Format & enumValue,
			const char * name = "Format")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(15); HPS::UTF8Array enumStrings(15);
			enumValues[0] = HPS::Shader::Texture::Format::RGBA8Unorm; enumStrings[0] = "RGBA8Unorm";
			enumValues[1] = HPS::Shader::Texture::Format::RGBA8Uint; enumStrings[1] = "RGBA8Uint";
			enumValues[2] = HPS::Shader::Texture::Format::RGBA16Uint; enumStrings[2] = "RGBA16Uint";
			enumValues[3] = HPS::Shader::Texture::Format::RGBA32Uint; enumStrings[3] = "RGBA32Uint";
			enumValues[4] = HPS::Shader::Texture::Format::RGBA32Float; enumStrings[4] = "RGBA32Float";
			enumValues[5] = HPS::Shader::Texture::Format::R8Unorm; enumStrings[5] = "R8Unorm";
			enumValues[6] = HPS::Shader::Texture::Format::R8Uint; enumStrings[6] = "R8Uint";
			enumValues[7] = HPS::Shader::Texture::Format::R16Uint; enumStrings[7] = "R16Uint";
			enumValues[8] = HPS::Shader::Texture::Format::R32Uint; enumStrings[8] = "R32Uint";
			enumValues[9] = HPS::Shader::Texture::Format::R32Float; enumStrings[9] = "R32Float";
			enumValues[10] = HPS::Shader::Texture::Format::RG8Unorm; enumStrings[10] = "RG8Unorm";
			enumValues[11] = HPS::Shader::Texture::Format::RG8Uint; enumStrings[11] = "RG8Uint";
			enumValues[12] = HPS::Shader::Texture::Format::RG16Uint; enumStrings[12] = "RG16Uint";
			enumValues[13] = HPS::Shader::Texture::Format::RG32Uint; enumStrings[13] = "RG32Uint";
			enumValues[14] = HPS::Shader::Texture::Format::RG32Float; enumStrings[14] = "RG32Float";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class DriverEventStereoMatrixProperty : public BaseEnumProperty<HPS::DriverEvent::StereoMatrix>
	{
	public:
		DriverEventStereoMatrixProperty(
			QTreeWidget * tree,
			HPS::DriverEvent::StereoMatrix & enumValue,
			const char * name = "StereoMatrix")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::DriverEvent::StereoMatrix::ViewLeft; enumStrings[0] = "ViewLeft";
			enumValues[1] = HPS::DriverEvent::StereoMatrix::ViewRight; enumStrings[1] = "ViewRight";
			enumValues[2] = HPS::DriverEvent::StereoMatrix::ProjectionLeft; enumStrings[2] = "ProjectionLeft";
			enumValues[3] = HPS::DriverEvent::StereoMatrix::ProjectionRight; enumStrings[3] = "ProjectionRight";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class DrawWindowEventBackgroundTextureFormatProperty : public BaseEnumProperty<HPS::DrawWindowEvent::BackgroundTextureFormat>
	{
	public:
		DrawWindowEventBackgroundTextureFormatProperty(
			QTreeWidget * tree,
			HPS::DrawWindowEvent::BackgroundTextureFormat & enumValue,
			const char * name = "BackgroundTextureFormat")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::DrawWindowEvent::BackgroundTextureFormat::RGBA; enumStrings[0] = "RGBA";
			enumValues[1] = HPS::DrawWindowEvent::BackgroundTextureFormat::BGRA; enumStrings[1] = "BGRA";
			enumValues[2] = HPS::DrawWindowEvent::BackgroundTextureFormat::ImageExternal; enumStrings[2] = "ImageExternal";
			enumValues[3] = HPS::DrawWindowEvent::BackgroundTextureFormat::LumaChromaPair; enumStrings[3] = "LumaChromaPair";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class LegacyShaderParameterizationProperty : public BaseEnumProperty<HPS::LegacyShader::Parameterization>
	{
	public:
		LegacyShaderParameterizationProperty(
			QTreeWidget * tree,
			HPS::LegacyShader::Parameterization & enumValue,
			const char * name = "Parameterization")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::LegacyShader::Parameterization::Cylinder; enumStrings[0] = "Cylinder";
			enumValues[1] = HPS::LegacyShader::Parameterization::PhysicalReflection; enumStrings[1] = "PhysicalReflection";
			enumValues[2] = HPS::LegacyShader::Parameterization::Object; enumStrings[2] = "Object";
			enumValues[3] = HPS::LegacyShader::Parameterization::NaturalUV; enumStrings[3] = "NaturalUV";
			enumValues[4] = HPS::LegacyShader::Parameterization::ReflectionVector; enumStrings[4] = "ReflectionVector";
			enumValues[5] = HPS::LegacyShader::Parameterization::SurfaceNormal; enumStrings[5] = "SurfaceNormal";
			enumValues[6] = HPS::LegacyShader::Parameterization::Sphere; enumStrings[6] = "Sphere";
			enumValues[7] = HPS::LegacyShader::Parameterization::UV; enumStrings[7] = "UV";
			enumValues[8] = HPS::LegacyShader::Parameterization::World; enumStrings[8] = "World";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class HardcopyBackgroundPreferenceProperty : public BaseEnumProperty<HPS::Hardcopy::BackgroundPreference>
	{
	public:
		HardcopyBackgroundPreferenceProperty(
			QTreeWidget * tree,
			HPS::Hardcopy::BackgroundPreference & enumValue,
			const char * name = "BackgroundPreference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Hardcopy::BackgroundPreference::UseBackgroundColor; enumStrings[0] = "UseBackgroundColor";
			enumValues[1] = HPS::Hardcopy::BackgroundPreference::ForceSolidWhite; enumStrings[1] = "ForceSolidWhite";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class HardcopyRenderingAlgorithmProperty : public BaseEnumProperty<HPS::Hardcopy::RenderingAlgorithm>
	{
	public:
		HardcopyRenderingAlgorithmProperty(
			QTreeWidget * tree,
			HPS::Hardcopy::RenderingAlgorithm & enumValue,
			const char * name = "RenderingAlgorithm")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Hardcopy::RenderingAlgorithm::TwoPassPrint; enumStrings[0] = "TwoPassPrint";
			enumValues[1] = HPS::Hardcopy::RenderingAlgorithm::SinglePassPrint; enumStrings[1] = "SinglePassPrint";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class HardcopyPDFFontPreferenceProperty : public BaseEnumProperty<HPS::Hardcopy::PDFFontPreference>
	{
	public:
		HardcopyPDFFontPreferenceProperty(
			QTreeWidget * tree,
			HPS::Hardcopy::PDFFontPreference & enumValue,
			const char * name = "PDFFontPreference")
			: BaseEnumProperty(name, enumValue)
			, tree(tree)
		{
		}
		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Hardcopy::PDFFontPreference::DoNotEmbedFonts; enumStrings[0] = "DoNotEmbedFonts";
			enumValues[1] = HPS::Hardcopy::PDFFontPreference::EmbedFonts; enumStrings[1] = "EmbedFonts";
			initializeEnumValues(enumValues, enumStrings, tree);
		}
	private:
		QTreeWidget * tree;
	};

	class SegmentNameProperty : public BaseProperty
	{
	public:
		SegmentNameProperty(
			HPS::SegmentKey const & key)
			: BaseProperty("Name")
		{
			name = key.Name();
			addChild(new UTF8Property("Value", name));
		}

		HPS::UTF8 GetName() const
		{
			return name;
		}

	private:
		HPS::UTF8 name;
	};

	class NetBoundingProperty : public BaseProperty
	{
	public:
		NetBoundingProperty(
			HPS::KeyPath const & keyPath)
			: BaseProperty("Net Bounding")
		{
			bool validBounding = false;
			HPS::BoundingKit boundingKit;
			if (keyPath.ShowNetBounding(boundingKit))
			{
				HPS::SimpleSphere sphere;
				HPS::SimpleCuboid cuboid;
				if (boundingKit.ShowVolume(sphere, cuboid))
				{
					validBounding = true;
					addChild(new ImmutableSimpleSphereProperty("Sphere", sphere));
					addChild(new ImmutableSimpleCuboidProperty("Cuboid", cuboid));
				}
			}

			if (!validBounding)
				addChild(new ImmutableUTF8Property("None", HPS::UTF8()));
		}
	};

	class SegmentContentCountProperty : public BaseProperty
	{
	private:
		struct SearchTypeData
		{
			SearchTypeData()
			{}

			SearchTypeData(
				const char * name,
				size_t count)
				: name(name)
				, count(count)
			{}

			const char * name;
			size_t count;
		};

		typedef std::map <
			HPS::Search::Type,
			SearchTypeData,
			std::less<HPS::Search::Type>,
			HPS::Allocator<std::pair<HPS::Search::Type const, SearchTypeData>>
		> SearchTypeDataMap;

	public:
		SegmentContentCountProperty(
			HPS::SegmentKey const & key)
			: BaseProperty("Contents")
		{
			CountContents(key);
			for (auto const & countPair : countMap)
			{
				SearchTypeData const & typeData = countPair.second;
				if (typeData.count > 0)
					addChild(new ImmutableSizeTProperty(typeData.name, typeData.count));
			}
		}

	private:
		void CountContents(
			HPS::SegmentKey const & key)
		{
			HPS::Search::Type types[25] = {
				HPS::Search::Type::Segment,
				HPS::Search::Type::Include,
				HPS::Search::Type::CuttingSection,
				HPS::Search::Type::Shell,
				HPS::Search::Type::CellularVolume,
				HPS::Search::Type::Mesh,
				HPS::Search::Type::Grid,
				HPS::Search::Type::NURBSSurface,
				HPS::Search::Type::Cylinder,
				HPS::Search::Type::Sphere,
				HPS::Search::Type::Polygon,
				HPS::Search::Type::Circle,
				HPS::Search::Type::CircularWedge,
				HPS::Search::Type::Ellipse,
				HPS::Search::Type::Line,
				HPS::Search::Type::NURBSCurve,
				HPS::Search::Type::CircularArc,
				HPS::Search::Type::EllipticalArc,
				HPS::Search::Type::InfiniteLine,
				HPS::Search::Type::InfiniteRay,
				HPS::Search::Type::Marker,
				HPS::Search::Type::Text,
				HPS::Search::Type::Reference,
				HPS::Search::Type::DistantLight,
				HPS::Search::Type::Spotlight
			};

			HPS::SearchResults searchResults;
			if (key.Find(25, types, HPS::Search::Space::SegmentOnly, searchResults) > 0)
			{
				countMap[HPS::Search::Type::Segment] = SearchTypeData("Subsegments", 0);
				countMap[HPS::Search::Type::Include] = SearchTypeData("Includes", 0);
				countMap[HPS::Search::Type::CuttingSection] = SearchTypeData("Cutting Sections", 0);
				countMap[HPS::Search::Type::Shell] = SearchTypeData("Shells", 0);
				countMap[HPS::Search::Type::CellularVolume] = SearchTypeData("Cellular Volumes", 0);
				countMap[HPS::Search::Type::Mesh] = SearchTypeData("Meshes", 0);
				countMap[HPS::Search::Type::Grid] = SearchTypeData("Grids", 0);
				countMap[HPS::Search::Type::NURBSSurface] = SearchTypeData("NURBS Surfaces", 0);
				countMap[HPS::Search::Type::Cylinder] = SearchTypeData("Cylinders", 0);
				countMap[HPS::Search::Type::Sphere] = SearchTypeData("Spheres", 0);
				countMap[HPS::Search::Type::Polygon] = SearchTypeData("Polygons", 0);
				countMap[HPS::Search::Type::Circle] = SearchTypeData("Circles", 0);
				countMap[HPS::Search::Type::CircularWedge] = SearchTypeData("Circular Wedges", 0);
				countMap[HPS::Search::Type::Ellipse] = SearchTypeData("Ellipses", 0);
				countMap[HPS::Search::Type::Line] = SearchTypeData("Lines", 0);
				countMap[HPS::Search::Type::NURBSCurve] = SearchTypeData("NURBS Curves", 0);
				countMap[HPS::Search::Type::CircularArc] = SearchTypeData("Circular Arcs", 0);
				countMap[HPS::Search::Type::EllipticalArc] = SearchTypeData("Elliptical Arcs", 0);
				countMap[HPS::Search::Type::InfiniteLine] = SearchTypeData("Infinite Lines", 0);
				countMap[HPS::Search::Type::InfiniteRay] = SearchTypeData("Infinite Rays", 0);
				countMap[HPS::Search::Type::Marker] = SearchTypeData("Markers", 0);
				countMap[HPS::Search::Type::Text] = SearchTypeData("Text", 0);
				countMap[HPS::Search::Type::Reference] = SearchTypeData("References", 0);
				countMap[HPS::Search::Type::DistantLight] = SearchTypeData("Distant Lights", 0);
				countMap[HPS::Search::Type::Spotlight] = SearchTypeData("Spotlights", 0);

				HPS::SearchResultsIterator it = searchResults.GetIterator();
				while (it.IsValid())
				{
					countMap[it.GetResultTypes().front()].count++;
					++it;
				}
			}
		}

	private:
		SearchTypeDataMap countMap;
	};

	class WindowInfoKitPhysicalPixelsProperty : public BaseProperty
	{
	public:
		WindowInfoKitPhysicalPixelsProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("PhysicalPixels")
		{
			unsigned int _width;
			unsigned int _height;
			kit.ShowPhysicalPixels(_width, _height);
			addChild(new ImmutableUnsignedIntProperty("Width", _width));
			addChild(new ImmutableUnsignedIntProperty("Height", _height));
		}
	};

	class WindowInfoKitPhysicalSizeProperty : public BaseProperty
	{
	public:
		WindowInfoKitPhysicalSizeProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("PhysicalSize")
		{
			float _width;
			float _height;
			kit.ShowPhysicalSize(_width, _height);
			addChild(new ImmutableFloatProperty("Width", _width));
			addChild(new ImmutableFloatProperty("Height", _height));
		}
	};

	class WindowInfoKitWindowPixelsProperty : public BaseProperty
	{
	public:
		WindowInfoKitWindowPixelsProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("WindowPixels")
		{
			unsigned int _width;
			unsigned int _height;
			kit.ShowWindowPixels(_width, _height);
			addChild(new ImmutableUnsignedIntProperty("Width", _width));
			addChild(new ImmutableUnsignedIntProperty("Height", _height));
		}
	};

	class WindowInfoKitWindowSizeProperty : public BaseProperty
	{
	public:
		WindowInfoKitWindowSizeProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("WindowSize")
		{
			float _width;
			float _height;
			kit.ShowWindowSize(_width, _height);
			addChild(new ImmutableFloatProperty("Width", _width));
			addChild(new ImmutableFloatProperty("Height", _height));
		}
	};

	class WindowInfoKitResolutionProperty : public BaseProperty
	{
	public:
		WindowInfoKitResolutionProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("Resolution")
		{
			float _horizontal;
			float _vertical;
			kit.ShowResolution(_horizontal, _vertical);
			addChild(new ImmutableFloatProperty("Horizontal", _horizontal));
			addChild(new ImmutableFloatProperty("Vertical", _vertical));
		}
	};

	class WindowInfoKitWindowAspectRatioProperty : public BaseProperty
	{
	public:
		WindowInfoKitWindowAspectRatioProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("WindowAspectRatio")
		{
			float _window_aspect;
			kit.ShowWindowAspectRatio(_window_aspect);
			addChild(new ImmutableFloatProperty("Window Aspect", _window_aspect));
		}
	};

	class WindowInfoKitPixelAspectRatioProperty : public BaseProperty
	{
	public:
		WindowInfoKitPixelAspectRatioProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty("PixelAspectRatio")
		{
			float _pixel_aspect;
			kit.ShowPixelAspectRatio(_pixel_aspect);
			addChild(new ImmutableFloatProperty("Pixel Aspect", _pixel_aspect));
		}
	};

	class SegmentKeyProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			NamePropertyIndex = 0,
		};

	public:
		SegmentKeyProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key,
			HPS::KeyPath const & keyPath)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl->addChild(new SegmentNameProperty(this->key));
			ctrl->addChild(new NetBoundingProperty(keyPath));
			ctrl->addChild(new SegmentContentCountProperty(this->key));
		}

		void Apply() override
		{
			auto nameChild = static_cast<SegmentNameProperty *>(item->child(NamePropertyIndex));
			if (key.Name() != nameChild->GetName())
				key.SetName(nameChild->GetName());
		}

	private:
		HPS::SegmentKey key;
	};

	class ApplicationWindowOptionsKitDriverProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitDriverProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty("Driver")
		{
			HPS::Window::Driver driver;
			kit.ShowDriver(driver);
			HPS::UTF8 driverString;
			switch (driver)
			{
			case HPS::Window::Driver::Default3D: driverString = "Default3D"; break;
			case HPS::Window::Driver::OpenGL: driverString = "OpenGL"; break;
			case HPS::Window::Driver::OpenGL2: driverString = "OpenGL2"; break;
			case HPS::Window::Driver::DirectX11: driverString = "DirectX11"; break;
			default: Q_ASSERT(0);
			}
			addChild(new ImmutableUTF8Property("Driver", driverString));
		}
	};

	class ApplicationWindowOptionsKitAntiAliasCapableProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitAntiAliasCapableProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty("AntiAliasCapable")
		{
			bool state;
			unsigned int samples;
			kit.ShowAntiAliasCapable(state, samples);
			addChild(new ImmutableBoolProperty("State", state));
			if (state)
				addChild(new ImmutableUnsignedIntProperty("Samples", samples));
		}
	};

	class ApplicationWindowOptionsKitPlatformDataProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitPlatformDataProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty("PlatformData")
		{
			intptr_t platformData;
			kit.ShowPlatformData(platformData);
			addChild(new ImmutableIntPtrTProperty("Value", platformData));
		}
	};

	class ApplicationWindowOptionsKitFramebufferRetentionProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitFramebufferRetentionProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty("FramebufferRetention")
		{
			bool retain;
			kit.ShowFramebufferRetention(retain);
			addChild(new ImmutableBoolProperty("Retain", retain));
		}
	};

	class ApplicationWindowKeyProperty : public SegmentKeyProperty
	{
	public:
		ApplicationWindowKeyProperty(
			QTreeWidgetItem * ctrl,
			HPS::ApplicationWindowKey const & key)
			: SegmentKeyProperty(ctrl, key, HPS::KeyPath(1, &key))
		{
			// window info
			{
				HPS::WindowInfoKit kit;
				key.ShowWindowInfo(kit);
				ctrl->addChild(new WindowInfoKitPhysicalPixelsProperty(kit));
				ctrl->addChild(new WindowInfoKitPhysicalSizeProperty(kit));
				ctrl->addChild(new WindowInfoKitWindowPixelsProperty(kit));
				ctrl->addChild(new WindowInfoKitWindowSizeProperty(kit));
				ctrl->addChild(new WindowInfoKitResolutionProperty(kit));
				ctrl->addChild(new WindowInfoKitWindowAspectRatioProperty(kit));
				ctrl->addChild(new WindowInfoKitPixelAspectRatioProperty(kit));
			}

			// application window options
			{
				HPS::ApplicationWindowOptionsKit kit;
				key.ShowWindowOptions(kit);
				ctrl->addChild(new ApplicationWindowOptionsKitDriverProperty(kit));
				ctrl->addChild(new ApplicationWindowOptionsKitAntiAliasCapableProperty(kit));
				ctrl->addChild(new ApplicationWindowOptionsKitPlatformDataProperty(kit));
				ctrl->addChild(new ApplicationWindowOptionsKitFramebufferRetentionProperty(kit));
			}
		}
	};

	class AttributeFilterArrayProperty : public SettableArrayProperty
	{
	public:
		AttributeFilterArrayProperty(
			QTreeWidget * tree,
			HPS::AttributeLockTypeArray const & types)
			: SettableArrayProperty("Filter")
			, tree(tree)
			, types(types)
		{
		}

		void addSubItems()
		{
			bool _isSet = !this->types.empty();
			if (_isSet)
				typeCount = static_cast<unsigned int>(this->types.size());
			else
			{
				typeCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty * array_size = new ArraySizeProperty("Count", typeCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();
			isSet(_isSet);
		}

		HPS::AttributeLockTypeArray GetTypes() const
		{
			return types;
		}

	protected:
		void set() override
		{
			if (typeCount < 1)
				return;
			AddOrDeleteItems(typeCount, static_cast<unsigned int>(types.size()));
		}

		void unset() override
		{
			// nothing to do
		}

		void ResizeArrays() override
		{
			types.resize(typeCount, HPS::AttributeLock::Type::Everything);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < typeCount; ++i)
			{
				std::string itemName = "Type " + std::to_string(i);
				AttributeLockTypeProperty * attributelocktypeproperty = new AttributeLockTypeProperty(tree, types[i], itemName.c_str());
				addChild(attributelocktypeproperty);
				attributelocktypeproperty->setupChoices();
			}
		}

	private:
		QTreeWidget * tree;
		unsigned int typeCount;
		HPS::AttributeLockTypeArray types;
	};

	template <typename Key>
	class AttributeFilterProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			FilterPropertyIndex = 0,
		};

	public:
		AttributeFilterProperty(
			QTreeWidgetItem * ctrl,
			Key const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowFilter(originalTypes);
			AttributeFilterArrayProperty *attributefilterarrayproperty = new AttributeFilterArrayProperty(ctrl->treeWidget(), originalTypes);
			ctrl->addChild(attributefilterarrayproperty);
			attributefilterarrayproperty->addSubItems();
		}

		void Apply() override
		{
			auto filterChild = static_cast<AttributeFilterArrayProperty *>(item->child(FilterPropertyIndex));
			if (filterChild->isSet())
			{
				key.UnsetFilter(originalTypes);
				key.SetFilter(filterChild->GetTypes());
			}
			else
				key.UnsetFilter(originalTypes);
		}

	private:
		Key key;
		HPS::AttributeLockTypeArray originalTypes;
	};

	typedef AttributeFilterProperty<HPS::StyleKey> StyleKeyAttributeFilterProperty;
	typedef AttributeFilterProperty<HPS::IncludeKey> IncludeKeyAttributeFilterProperty;


	template <typename Kit>
	class GeometryPointsProperty : public BaseProperty
	{
	public:
		GeometryPointsProperty(
			Kit const & kit)
			: BaseProperty("Points")
		{
			addChild(new ImmutableSizeTProperty("Count", kit.GetPointCount()));
		}
	};
	typedef GeometryPointsProperty<HPS::ShellKit> ShellKitPointsProperty;
	typedef GeometryPointsProperty<HPS::EXPERIMENTAL::CellularVolumeKit> CellularVolumeKitPointsProperty;
	typedef GeometryPointsProperty<HPS::CylinderKit> CylinderKitPointsProperty;
	typedef GeometryPointsProperty<HPS::LineKit> LineKitPointsProperty;
	typedef GeometryPointsProperty<HPS::NURBSCurveKit> NURBSCurveKitPointsProperty;
	typedef GeometryPointsProperty<HPS::NURBSSurfaceKit> NURBSSurfaceKitPointsProperty;
	typedef GeometryPointsProperty<HPS::PolygonKit> PolygonKitPointsProperty;

	class ShellKitFacelistProperty : public BaseProperty
	{
	public:
		ShellKitFacelistProperty(
			HPS::ShellKit const & kit)
			: BaseProperty("Facelist")
		{
			HPS::IntArray facelist;
			kit.ShowFacelist(facelist);
			addChild(new ImmutableSizeTProperty("Facelist Size", facelist.size()));
			addChild(new ImmutableSizeTProperty("Face Count", kit.GetFaceCount()));
		}
	};

	class CellularVolumeKitCellsListProperty: public BaseProperty {
	public:
	    CellularVolumeKitCellsListProperty(HPS::EXPERIMENTAL::CellularVolumeKit const& kit)
			: BaseProperty("CellsList")
		{
			HPS::IntArray cellslist;
			kit.ShowCellsList(cellslist);
			addChild(new ImmutableSizeTProperty("CellsList Size", cellslist.size()));
			addChild(new ImmutableSizeTProperty("Cell Count", kit.GetCellCount()));
		}
	};

	template <
		typename Kit,
		typename Component>
		HPS::UTF8 getVertexColorQuantityForComponent(
			Kit const & kit,
			Component componentType)
	{
		HPS::MaterialTypeArray types;
		HPS::RGBColorArray rgbColors;
		HPS::RGBAColorArray rgbaColors;
		HPS::FloatArray indices;
		if (kit.ShowVertexColors(componentType, types, rgbColors, rgbaColors, indices))
		{
			if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
				return "All";
			else
				return "Some";
		}
		else
			return "None";
	}

	class ShellKitVertexColorsProperty : public BaseProperty
	{
	public:
		ShellKitVertexColorsProperty(
			HPS::ShellKit const & kit)
			: BaseProperty("VertexColors")
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::Shell::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::Shell::Component::Edges);
			HPS::UTF8 vertexQuantity = getVertexColorQuantityForComponent(kit, HPS::Shell::Component::Vertices);
			addChild(new ImmutableUTF8Property("Faces", faceQuantity));
			addChild(new ImmutableUTF8Property("Edges", edgeQuantity));
			addChild(new ImmutableUTF8Property("Vertices", vertexQuantity));
		}
	};

	class CellularVolumeKitVertexColorsProperty: public BaseProperty {
	public:
	    CellularVolumeKitVertexColorsProperty(HPS::EXPERIMENTAL::CellularVolumeKit const& kit)
			: BaseProperty("VertexColors")
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::CellularVolume::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::CellularVolume::Component::Edges);
			HPS::UTF8 vertexQuantity = getVertexColorQuantityForComponent(kit, HPS::CellularVolume::Component::Vertices);
			addChild(new ImmutableUTF8Property("Faces", faceQuantity));
			addChild(new ImmutableUTF8Property("Edges", edgeQuantity));
			addChild(new ImmutableUTF8Property("Vertices", vertexQuantity));
		}

	  private:
		HPS::UTF8 getVertexColorQuantityForComponent(HPS::EXPERIMENTAL::CellularVolumeKit const& kit, HPS::CellularVolume::Component componentType)
		{
			HPS::MaterialTypeArray types;
			HPS::RGBColorArray rgbColors;
			HPS::FloatArray indices;
			if (kit.ShowVertexColors(componentType, types, rgbColors, indices))
			{
				if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
					return "All";
				else
					return "Some";
			}
			else
				return "None";
		}
	};

	template <
		typename Kit,
		bool (Kit::*ShowNormals)(HPS::BoolArray &, HPS::VectorArray &) const
	>
	class PolyhedronNormalsProperty : public BaseProperty
	{
	public:
		PolyhedronNormalsProperty(
			const char * name,
			Kit const & kit)
			: BaseProperty(name)
		{
			HPS::UTF8 normalQuantity;
			HPS::BoolArray validities;
			HPS::VectorArray normals;
			if ((kit.*ShowNormals)(validities, normals))
			{
				if (std::find(validities.begin(), validities.end(), false) == validities.end())
					normalQuantity = "All";
				else
					normalQuantity = "Some";
			}
			else
				normalQuantity = "None";
			addChild(new ImmutableUTF8Property("Quantity", normalQuantity));
		}
	};

	typedef PolyhedronNormalsProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowVertexNormals
	> BaseShellKitVertexNormalsProperty;
	class ShellKitVertexNormalsProperty : public BaseShellKitVertexNormalsProperty
	{
	public:
		ShellKitVertexNormalsProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitVertexNormalsProperty("VertexNormals", kit)
		{}
	};

	//typedef PolyhedronNormalsProperty<HPS::EXPERIMENTAL::CellularVolumeKit, &HPS::EXPERIMENTAL::CellularVolumeKit::ShowVertexNormals>
	//    BaseCellularVolumeKitVertexNormalsProperty;
	//
	//class CellularVolumeKitVertexNormalsProperty: public BaseCellularVolumeKitVertexNormalsProperty {
	//public:
	//    CellularVolumeKitVertexNormalsProperty(HPS::EXPERIMENTAL::CellularVolumeKit const& kit):
	//      BaseCellularVolumeKitVertexNormalsProperty("VertexNormals", kit)
	//	{}
	//};

	typedef PolyhedronNormalsProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowFaceNormals
	> BaseShellKitFaceNormalsProperty;
	class ShellKitFaceNormalsProperty : public BaseShellKitFaceNormalsProperty
	{
	public:
		ShellKitFaceNormalsProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitFaceNormalsProperty("FaceNormals", kit)
		{}
	};

	typedef PolyhedronNormalsProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowVertexNormals
	> BaseMeshKitVertexNormalsProperty;
	class MeshKitVertexNormalsProperty : public BaseMeshKitVertexNormalsProperty
	{
	public:
		MeshKitVertexNormalsProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitVertexNormalsProperty("VertexNormals", kit)
		{}
	};

	typedef PolyhedronNormalsProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowFaceNormals
	> BaseMeshKitFaceNormalsProperty;
	class MeshKitFaceNormalsProperty : public BaseMeshKitFaceNormalsProperty
	{
	public:
		MeshKitFaceNormalsProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitFaceNormalsProperty("FaceNormals", kit)
		{}
	};

	template <typename Kit>
	class PolyhedronVertexParametersProperty : public BaseProperty
	{
	public:
		PolyhedronVertexParametersProperty(
			Kit const & kit)
			: BaseProperty("VertexParameters")
		{
			size_t paramWidth;
			HPS::UTF8 paramQuantity;
			HPS::BoolArray validities;
			HPS::FloatArray params;
			if (kit.ShowVertexParameters(validities, params, paramWidth))
			{
				if (std::find(validities.begin(), validities.end(), false) == validities.end())
					paramQuantity = "All";
				else
					paramQuantity = "Some";
			}
			else
			{
				paramWidth = 0;
				paramQuantity = "None";
			}
			addChild(new ImmutableUTF8Property("Quantity", paramQuantity));
			if (paramWidth > 0)
				addChild(new ImmutableSizeTProperty("Width", paramWidth));
		}
	};
	typedef PolyhedronVertexParametersProperty<HPS::ShellKit> ShellKitVertexParametersProperty;
	typedef PolyhedronVertexParametersProperty<HPS::EXPERIMENTAL::CellularVolumeKit> CellularVolumeKitVertexParametersProperty;
	typedef PolyhedronVertexParametersProperty<HPS::MeshKit> MeshKitVertexParametersProperty;

	template <
		typename Kit,
		bool (Kit::*ShowVisibilities)(HPS::BoolArray &, HPS::BoolArray &) const
	>
	class PolyhedronVisibilitiesProperty : public BaseProperty
	{
	public:
		PolyhedronVisibilitiesProperty(
			const char * name,
			Kit const & kit)
			: BaseProperty(name)
		{
			HPS::UTF8 visibilityQuantity;
			HPS::BoolArray validities;
			HPS::BoolArray visibilities;
			if ((kit.*ShowVisibilities)(validities, visibilities))
			{
				if (std::find(validities.begin(), validities.end(), false) == validities.end())
					visibilityQuantity = "All";
				else
					visibilityQuantity = "Some";
			}
			else
				visibilityQuantity = "None";
			addChild(new ImmutableUTF8Property("Quantity", visibilityQuantity));
		}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowVertexVisibilities
	> BaseShellKitVertexVisibilitiesProperty;
	class ShellKitVertexVisibilitiesProperty : public BaseShellKitVertexVisibilitiesProperty
	{
	public:
		ShellKitVertexVisibilitiesProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitVertexVisibilitiesProperty("VertexVisibilities", kit)
		{}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowFaceVisibilities
	> BaseShellKitFaceVisibilitiesProperty;
	class ShellKitFaceVisibilitiesProperty : public BaseShellKitFaceVisibilitiesProperty
	{
	public:
		ShellKitFaceVisibilitiesProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitFaceVisibilitiesProperty("FaceVisibilities", kit)
		{}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowVertexVisibilities
	> BaseMeshKitVertexVisibilitiesProperty;
	class MeshKitVertexVisibilitiesProperty : public BaseMeshKitVertexVisibilitiesProperty
	{
	public:
		MeshKitVertexVisibilitiesProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitVertexVisibilitiesProperty("VertexVisibilities", kit)
		{}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowFaceVisibilities
	> BaseMeshKitFaceVisibilitiesProperty;
	class MeshKitFaceVisibilitiesProperty : public BaseMeshKitFaceVisibilitiesProperty
	{
	public:
		MeshKitFaceVisibilitiesProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitFaceVisibilitiesProperty("FaceVisibilities", kit)
		{}
	};

	template <typename Kit>
	class PriorityProperty : public SettableProperty
	{
	public:
		PriorityProperty(
			Kit & kit)
			: SettableProperty("Priority")
			, kit(kit)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPriority(priority);
			if (!_isSet)
				priority = 0;
			addChild(new IntProperty("Value", priority));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetPriority(priority);
		}

		void unset() override
		{
			kit.UnsetPriority();
		}

	private:
		Kit & kit;
		int priority;
	};

	typedef PriorityProperty<HPS::ShellKit> ShellKitPriorityProperty;
	typedef PriorityProperty<HPS::EXPERIMENTAL::CellularVolumeKit> CellularVolumeKitPriorityProperty;
	typedef PriorityProperty<HPS::MeshKit> MeshKitPriorityProperty;
	typedef PriorityProperty<HPS::CylinderKit> CylinderKitPriorityProperty;
	typedef PriorityProperty<HPS::LineKit> LineKitPriorityProperty;
	typedef PriorityProperty<HPS::NURBSCurveKit> NURBSCurveKitPriorityProperty;
	typedef PriorityProperty<HPS::NURBSSurfaceKit> NURBSSurfaceKitPriorityProperty;
	typedef PriorityProperty<HPS::PolygonKit> PolygonKitPriorityProperty;
	typedef PriorityProperty<HPS::TextKit> TextKitPriorityProperty;
	typedef PriorityProperty<HPS::MarkerKit> MarkerKitPriorityProperty;
	typedef PriorityProperty<HPS::DistantLightKit> DistantLightKitPriorityProperty;
	typedef PriorityProperty<HPS::CuttingSectionKit> CuttingSectionKitPriorityProperty;
	typedef PriorityProperty<HPS::SphereKit> SphereKitPriorityProperty;
	typedef PriorityProperty<HPS::CircleKit> CircleKitPriorityProperty;
	typedef PriorityProperty<HPS::CircularArcKit> CircularArcKitPriorityProperty;
	typedef PriorityProperty<HPS::CircularWedgeKit> CircularWedgeKitPriorityProperty;
	typedef PriorityProperty<HPS::InfiniteLineKit> InfiniteLineKitPriorityProperty;
	typedef PriorityProperty<HPS::SpotlightKit> SpotlightKitPriorityProperty;
	typedef PriorityProperty<HPS::EllipseKit> EllipseKitPriorityProperty;
	typedef PriorityProperty<HPS::EllipticalArcKit> EllipticalArcKitPriorityProperty;
	typedef PriorityProperty<HPS::GridKit> GridKitPriorityProperty;

	class SingleUserDataProperty : public BaseProperty
	{
	public:
		SingleUserDataProperty(
			size_t index,
			intptr_t dataIndex,
			size_t dataByteCount)
			: BaseProperty("")
		{
			std::string name = "Data " + std::to_string(index);
			setText(0, name.c_str());

			addChild(new ImmutableIntPtrTProperty("Index", dataIndex));
			addChild(new ImmutableSizeTProperty("Byte Count", dataByteCount));
		}
	};

	template <typename Kit>
	class UserDataProperty : public SettableProperty
	{
	public:
		UserDataProperty(
			Kit & kit)
			: SettableProperty("User Data")
			, kit(kit)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowUserData(indices, data);
			if (_isSet)
			{
				addChild(new ImmutableSizeTProperty("Count", indices.size()));
				for (size_t i = 0; i < indices.size(); ++i)
					addChild(new SingleUserDataProperty(i, indices[i], data[i].size()));
			}
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetUserData(indices, data);
		}

		void unset() override
		{
			kit.UnsetAllUserData();
		}

	private:
		Kit & kit;
		HPS::IntPtrTArray indices;
		HPS::ByteArrayArray data;
	};

	typedef UserDataProperty<HPS::ShellKit> ShellKitUserDataProperty;
	typedef UserDataProperty<HPS::EXPERIMENTAL::CellularVolumeKit> CellularVolumeKitUserDataProperty;
	typedef UserDataProperty<HPS::MeshKit> MeshKitUserDataProperty;
	typedef UserDataProperty<HPS::CylinderKit> CylinderKitUserDataProperty;
	typedef UserDataProperty<HPS::LineKit> LineKitUserDataProperty;
	typedef UserDataProperty<HPS::NURBSCurveKit> NURBSCurveKitUserDataProperty;
	typedef UserDataProperty<HPS::NURBSSurfaceKit> NURBSSurfaceKitUserDataProperty;
	typedef UserDataProperty<HPS::PolygonKit> PolygonKitUserDataProperty;
	typedef UserDataProperty<HPS::TextKit> TextKitUserDataProperty;
	typedef UserDataProperty<HPS::MarkerKit> MarkerKitUserDataProperty;
	typedef UserDataProperty<HPS::DistantLightKit> DistantLightKitUserDataProperty;
	typedef UserDataProperty<HPS::CuttingSectionKit> CuttingSectionKitUserDataProperty;
	typedef UserDataProperty<HPS::SphereKit> SphereKitUserDataProperty;
	typedef UserDataProperty<HPS::CircleKit> CircleKitUserDataProperty;
	typedef UserDataProperty<HPS::CircularArcKit> CircularArcKitUserDataProperty;
	typedef UserDataProperty<HPS::CircularWedgeKit> CircularWedgeKitUserDataProperty;
	typedef UserDataProperty<HPS::InfiniteLineKit> InfiniteLineKitUserDataProperty;
	typedef UserDataProperty<HPS::SpotlightKit> SpotlightKitUserDataProperty;
	typedef UserDataProperty<HPS::EllipseKit> EllipseKitUserDataProperty;
	typedef UserDataProperty<HPS::EllipticalArcKit> EllipticalArcKitUserDataProperty;
	typedef UserDataProperty<HPS::GridKit> GridKitUserDataProperty;

	class TextRotationProperty : public BaseEnumProperty<HPS::Text::Rotation>
	{
	private:
		enum PropertyTypeIndex
		{
			RotationPropertyIndex = 0,
			AnglePropertyIndex,
		};

	public:
		TextRotationProperty(
			QTreeWidget * tree,
			HPS::Text::Rotation & enumValue)
			: BaseEnumProperty("Rotation", enumValue)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::Rotation::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Text::Rotation::Rotate; enumStrings[1] = "Rotate";
			enumValues[2] = HPS::Text::Rotation::FollowPath; enumStrings[2] = "FollowPath";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto angleSibling = static_cast<BaseProperty *>(parent())->child(AnglePropertyIndex);
			if (enumValue == HPS::Text::Rotation::None || enumValue == HPS::Text::Rotation::FollowPath)
				angleSibling->setFlags(angleSibling->flags() & ~Qt::ItemFlag::ItemIsEnabled);
			else if (enumValue == HPS::Text::Rotation::Rotate)
				angleSibling->setFlags(angleSibling->flags() | Qt::ItemFlag::ItemIsEnabled);
		}

	private:
		QTreeWidget * tree;
	};

	template <typename Kit>
	class RotationProperty : public SettableProperty
	{
	public:
		RotationProperty(
			QTreeWidget * tree,
			Kit & kit)
			: SettableProperty("Rotation")
			, kit(kit)
			, tree(tree)
		{

		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRotation(rotation, angle);
			if (_isSet)
			{
				if (rotation != HPS::Text::Rotation::Rotate)
					angle = 0.0f;
			}
			else
			{
				rotation = HPS::Text::Rotation::FollowPath;
				angle = 0.0f;
			}
			auto rotationChild = new TextRotationProperty(tree, rotation);
			addChild(rotationChild);
			rotationChild->setupChoices();
			addChild(new FloatProperty("Angle", angle));
			rotationChild->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetRotation(rotation, angle);
		}

		void unset() override
		{
			kit.UnsetRotation();
		}

	private:
		Kit & kit;
		HPS::Text::Rotation rotation;
		float angle;
		QTreeWidget * tree;
	};

	typedef RotationProperty<HPS::TextKit> TextKitRotationProperty;
	typedef RotationProperty<HPS::TextAttributeKit> TextAttributeKitRotationProperty;

	class DiffuseColorTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			RedPropertyIndex,
			GreenPropertyIndex,
			BluePropertyIndex,
			AlphaPropertyIndex
		};

	public:
		DiffuseColorTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{
		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Type::RGBAColor; enumStrings[0] = "RGBAColor";
			enumValues[1] = HPS::Material::Type::RGBColor; enumStrings[1] = "RGBColor";
			enumValues[2] = HPS::Material::Type::DiffuseChannelAlpha; enumStrings[2] = "Alpha";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto redSibling = static_cast<BaseProperty *>(parent()->child(RedPropertyIndex));
			auto greenSibling = static_cast<BaseProperty *>(parent()->child(GreenPropertyIndex));
			auto blueSibling = static_cast<BaseProperty *>(parent()->child(BluePropertyIndex));
			auto alphaSibling = static_cast<BaseProperty *>(parent()->child(AlphaPropertyIndex));
			if (enumValue == HPS::Material::Type::RGBAColor)
			{
				redSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				greenSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				blueSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				alphaSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::RGBColor)
			{
				redSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				greenSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				blueSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				alphaSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::DiffuseChannelAlpha)
			{
				redSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				greenSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				blueSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				alphaSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	class DiffuseColorProperty : public SettableProperty
	{
	public:
		DiffuseColorProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: SettableProperty("Diffuse Color")
			, material(material)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = false;
			if (this->material.ShowDiffuseColor(rgbColor))
			{
				_isSet = true;
				type = HPS::Material::Type::RGBColor;
				alpha = 1;
			}
			if (this->material.ShowDiffuseAlpha(alpha))
			{
				_isSet = true;
				if (type == HPS::Material::Type::RGBColor)
					type = HPS::Material::Type::RGBAColor;
				else
				{
					type = HPS::Material::Type::DiffuseChannelAlpha;
					rgbColor = HPS::RGBColor::Black();
				}
			}

			if (!_isSet)
			{
				type = HPS::Material::Type::RGBAColor;
				rgbColor = HPS::RGBColor::Black();
				alpha = 1;
			}

			auto typeChild = new DiffuseColorTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new UnitFloatProperty("Red", rgbColor.red));
			addChild(new UnitFloatProperty("Green", rgbColor.green));
			addChild(new UnitFloatProperty("Blue", rgbColor.blue));
			addChild(new UnitFloatProperty("Alpha", alpha));

			typeChild->enableValidProperties();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			// Unset the diffuse color first because we don't want to have multiple sets accumulate.
			// E.g., if diffuse RGB gets set, then the type is switched to alpha and the alpha
			// gets set, we should end up with *only* the alpha set, not the RGB + alpha set.

			if (type == HPS::Material::Type::RGBAColor)
				material.UnsetDiffuseColor().SetDiffuseColor(HPS::RGBAColor(rgbColor, alpha));
			else if (type == HPS::Material::Type::RGBColor)
				material.UnsetDiffuseColor().SetDiffuseColor(rgbColor);
			else if (type == HPS::Material::Type::DiffuseChannelAlpha)
				material.UnsetDiffuseColor().SetDiffuseAlpha(alpha);
		}

		void unset() override
		{
			material.UnsetDiffuseColor();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBColor rgbColor;
		float alpha;
		QTreeWidget * tree;
	};

	class DiffuseTextureLayerTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			ColorPropertyIndex,
		};

	public:
		DiffuseTextureLayerTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{
		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Type::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Material::Type::TextureName; enumStrings[1] = "Texture";
			enumValues[2] = HPS::Material::Type::ModulatedTexture; enumStrings[2] = "Modulated Texture";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto textureSibling = static_cast<BaseProperty *>(parent()->child(TexturePropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent()->child(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::None)
			{
				textureSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}
	private:
		QTreeWidget * tree;
	};

	class DiffuseTextureLayerProperty : public BaseProperty
	{
	public:
		DiffuseTextureLayerProperty(
			QTreeWidget * tree,
			unsigned int layer,
			HPS::Material::Type & type,
			HPS::RGBAColor & modulationColor,
			HPS::UTF8 & textureName)
			: BaseProperty("")
			, type(type)
			, modulationColor(modulationColor)
			, textureName(textureName)
		{
			std::string name = "Layer " + std::to_string(layer);
			setText(0, name.c_str());

			auto typeChild = new DiffuseTextureLayerTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new UTF8Property("Texture", textureName));
			addChild(new RGBAColorProperty("Modulation Color", modulationColor));

			typeChild->enableValidProperties();
		}

	private:
		HPS::Material::Type & type;
		HPS::RGBAColor & modulationColor;
		HPS::UTF8 & textureName;
	};

	class DiffuseTextureProperty : public SettableArrayProperty
	{
	public:
		DiffuseTextureProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: SettableArrayProperty("Diffuse Texture")
			, tree(tree)
			, material(material)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->material.ShowDiffuseTexture(types, modulationColors, textureNames);

			if (_isSet)
			{
				layerCount = static_cast<unsigned int>(types.size());
				for (unsigned int layer = 0; layer < layerCount; ++layer)
				{
					if (types[layer] == HPS::Material::Type::None)
					{
						modulationColors[layer] = HPS::RGBAColor::Black();
						textureNames[layer] = "texture";
					}
					else if (types[layer] == HPS::Material::Type::TextureName)
						modulationColors[layer] = HPS::RGBAColor::Black();
				}
			}
			else
			{
				layerCount = 1;
				ResizeArrays();
			}

			ArraySizeProperty * array_size = new ArraySizeProperty("Layer Count", layerCount, 1, 8);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (layerCount < 1)
				return;

			AddOrDeleteItems(layerCount, static_cast<unsigned int>(types.size()));

			material.UnsetDiffuseTexture();
			for (size_t layer = 0; layer < types.size(); ++layer)
			{
				HPS::Material::Type layerType = types[layer];
				if (layerType == HPS::Material::Type::None)
					continue;

				if (layerType == HPS::Material::Type::TextureName)
					material.SetDiffuseTexture(textureNames[layer], layer);
				else if (layerType == HPS::Material::Type::ModulatedTexture)
					material.SetDiffuseTexture(textureNames[layer], modulationColors[layer], layer);
				else
					Q_ASSERT(0);
			}
		}

		void unset() override
		{
			material.UnsetDiffuseTexture();
		}

		void ResizeArrays() override
		{
			types.resize(layerCount, HPS::Material::Type::TextureName);
			modulationColors.resize(layerCount, HPS::RGBAColor::Black());
			textureNames.resize(layerCount, "texture");
		}

		void AddItems() override
		{
			for (unsigned int layer = 0; layer < layerCount; ++layer)
			{
				auto newLayer = new DiffuseTextureLayerProperty(tree, layer, types[layer], modulationColors[layer], textureNames[layer]);
				addChild(newLayer);
			}
		}

	private:
		QTreeWidget * tree;
		HPS::MaterialKit & material;
		unsigned int layerCount;
		HPS::MaterialTypeArray types;
		HPS::RGBAColorArray modulationColors;
		HPS::UTF8Array textureNames;
	};

	class EnvironmentTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			CubeMapPropertyIndex,
			ColorPropertyIndex,
		};

	public:
		EnvironmentTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Material::Type::TextureName; enumStrings[0] = "Texture";
			enumValues[1] = HPS::Material::Type::ModulatedTexture; enumStrings[1] = "Modulated Texture";
			enumValues[2] = HPS::Material::Type::CubeMapName; enumStrings[2] = "Cube Map";
			enumValues[3] = HPS::Material::Type::ModulatedCubeMap; enumStrings[3] = "Modulated Cube Map";
			enumValues[4] = HPS::Material::Type::None; enumStrings[4] = "None";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto textureSibling = static_cast<BaseProperty *>(parent()->child(TexturePropertyIndex));
			auto cubeMapSibling = static_cast<BaseProperty *>(parent()->child(CubeMapPropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent()->child(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::None)
			{
				textureSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				cubeMapSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				cubeMapSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				cubeMapSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::CubeMapName)
			{
				textureSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				cubeMapSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::ModulatedCubeMap)
			{
				textureSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				cubeMapSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	class EnvironmentProperty : public SettableProperty
	{
	public:
		EnvironmentProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: SettableProperty("Environment")
			, material(material)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			HPS::UTF8 textureOrCubeMapName;
			bool _isSet = this->material.ShowEnvironment(type, modulationColor, textureOrCubeMapName);
			if (_isSet)
			{
				if (type == HPS::Material::Type::None)
				{
					textureName = "texture";
					cubeMapName = "cubemap";
					modulationColor = HPS::RGBAColor::Black();
				}
				else if (type == HPS::Material::Type::TextureName || type == HPS::Material::Type::ModulatedTexture)
				{
					textureName = textureOrCubeMapName;
					cubeMapName = "cubemap";
					if (type == HPS::Material::Type::TextureName)
						modulationColor = HPS::RGBAColor::Black();
				}
				else if (type == HPS::Material::Type::CubeMapName || type == HPS::Material::Type::ModulatedCubeMap)
				{
					cubeMapName = textureOrCubeMapName;
					textureName = "texture";
					if (type == HPS::Material::Type::CubeMapName)
						modulationColor = HPS::RGBAColor::Black();
				}
			}
			else
			{
				type = HPS::Material::Type::TextureName;
				textureName = "texture";
				cubeMapName = "cubemap";
				modulationColor = HPS::RGBAColor::Black();
			}

			auto typeChild = new EnvironmentTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new UTF8Property("Texture", textureName));
			addChild(new UTF8Property("CubeMap", cubeMapName));
			addChild(new RGBAColorProperty("ModulationColor", modulationColor));

			typeChild->enableValidProperties();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (type == HPS::Material::Type::None)
				material.SetEnvironmentTexture();
			else if (type == HPS::Material::Type::TextureName)
				material.SetEnvironmentTexture(textureName);
			else if (type == HPS::Material::Type::ModulatedTexture)
				material.SetEnvironmentTexture(textureName, modulationColor);
			else if (type == HPS::Material::Type::CubeMapName)
				material.SetEnvironmentCubeMap(cubeMapName);
			else if (type == HPS::Material::Type::ModulatedCubeMap)
				material.SetEnvironmentCubeMap(cubeMapName, modulationColor);
		}

		void unset() override
		{
			material.UnsetEnvironment();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBAColor modulationColor;
		HPS::UTF8 textureName;
		HPS::UTF8 cubeMapName;
		QTreeWidget * tree;
	};

	class ModulatedChannelTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			ColorPropertyIndex,
		};

	public:
		ModulatedChannelTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Type::RGBAColor; enumStrings[0] = "RGBAColor";
			enumValues[1] = HPS::Material::Type::TextureName; enumStrings[1] = "Texture";
			enumValues[2] = HPS::Material::Type::ModulatedTexture; enumStrings[2] = "Modulated Texture";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto textureSibling = static_cast<BaseProperty *>(parent()->child(TexturePropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent()->child(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::RGBAColor)
			{
				textureSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	template <
		bool (HPS::MaterialKit::*ShowFunction)(HPS::Material::Type &, HPS::RGBAColor &, HPS::UTF8 &) const,
		HPS::MaterialKit & (HPS::MaterialKit::*SetColorFunction)(HPS::RGBAColor const &),
		HPS::MaterialKit & (HPS::MaterialKit::*SetTextureFunction)(char const *),
		HPS::MaterialKit & (HPS::MaterialKit::*SetModulatedTextureFunction)(char const *, HPS::RGBAColor const &),
		HPS::MaterialKit & (HPS::MaterialKit::*UnsetFunction)()
	>
	class ModulatedChannelProperty : public SettableProperty
	{
	public:
		ModulatedChannelProperty(
			QTreeWidget * tree,
			const char * const & name,
			HPS::MaterialKit & material)
			: SettableProperty(name)
			, material(material)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = (this->material.*ShowFunction)(type, color, textureName);
			if (_isSet)
			{
				if (type == HPS::Material::Type::RGBAColor)
					textureName = "texture";
				else if (type == HPS::Material::Type::TextureName)
					color = HPS::RGBAColor::Black();
			}
			else
			{
				type = HPS::Material::Type::RGBAColor;
				color = HPS::RGBAColor::Black();
				textureName = "texture";
			}

			auto typeChild = new ModulatedChannelTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new UTF8Property("Texture", textureName));
			addChild(new RGBAColorProperty("Color", color));

			typeChild->enableValidProperties();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (type == HPS::Material::Type::RGBAColor)
				(material.*SetColorFunction)(color);
			else if (type == HPS::Material::Type::TextureName)
				(material.*SetTextureFunction)(textureName);
			else if (type == HPS::Material::Type::ModulatedTexture)
				(material.*SetModulatedTextureFunction)(textureName, color);
		}

		void unset() override
		{
			(material.*UnsetFunction)();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBAColor color;
		HPS::UTF8 textureName;
		QTreeWidget * tree;
	};

	typedef ModulatedChannelProperty <
		&HPS::MaterialKit::ShowSpecular,
		&HPS::MaterialKit::SetSpecular,
		&HPS::MaterialKit::SetSpecular,
		&HPS::MaterialKit::SetSpecular,
		&HPS::MaterialKit::UnsetSpecular
	> SpecularPropertyBase;
	class SpecularProperty : public SpecularPropertyBase
	{
	public:
		SpecularProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: SpecularPropertyBase(tree, "Specular", material)
		{}
	};

	typedef ModulatedChannelProperty <
		&HPS::MaterialKit::ShowMirror,
		&HPS::MaterialKit::SetMirror,
		&HPS::MaterialKit::SetMirror,
		&HPS::MaterialKit::SetMirror,
		&HPS::MaterialKit::UnsetMirror
	> MirrorPropertyBase;
	class MirrorProperty : public MirrorPropertyBase
	{
	public:
		MirrorProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: MirrorPropertyBase(tree, "Mirror", material)
		{}
	};

	typedef ModulatedChannelProperty <
		&HPS::MaterialKit::ShowEmission,
		&HPS::MaterialKit::SetEmission,
		&HPS::MaterialKit::SetEmission,
		&HPS::MaterialKit::SetEmission,
		&HPS::MaterialKit::UnsetEmission
	> EmissionPropertyBase;
	class EmissionProperty : public EmissionPropertyBase
	{
	public:
		EmissionProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: EmissionPropertyBase(tree, "Emission", material)
		{}
	};

	class TransmissionTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			ColorPropertyIndex,
		};

	public:
		TransmissionTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Type::TextureName; enumStrings[0] = "Texture";
			enumValues[1] = HPS::Material::Type::ModulatedTexture; enumStrings[1] = "Modulated Texture";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto textureSibling = static_cast<BaseProperty *>(parent()->child(TexturePropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent()->child(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				colorSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	class TransmissionProperty : public SettableProperty
	{
	public:
		TransmissionProperty(
			QTreeWidget * tree,
			HPS::MaterialKit & material)
			: SettableProperty("Transmission")
			, material(material)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->material.ShowTransmission(type, modulationColor, textureName);
			if (_isSet)
			{
				if (type == HPS::Material::Type::TextureName)
					modulationColor = HPS::RGBAColor::Black();
			}
			else
			{
				type = HPS::Material::Type::TextureName;
				textureName = "texture";
				modulationColor = HPS::RGBAColor::Black();
			}

			auto typeChild = new TransmissionTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new UTF8Property("Texture", textureName));
			addChild(new RGBAColorProperty("Modulation Color", modulationColor));

			typeChild->enableValidProperties();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (type == HPS::Material::Type::TextureName)
				material.SetTransmission(textureName);
			else if (type == HPS::Material::Type::ModulatedTexture)
				material.SetTransmission(textureName, modulationColor);
		}

		void unset() override
		{
			material.UnsetTransmission();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBAColor modulationColor;
		HPS::UTF8 textureName;
		QTreeWidget * tree;
	};

	class BumpProperty : public SettableProperty
	{
	public:
		BumpProperty(
			HPS::MaterialKit & material)
			: SettableProperty("Bump Texture")
			, material(material)
		{
		}

		void addSubItems()
		{
			bool _isSet = material.ShowBump(textureName);
			if (!_isSet)
				textureName = "texture";

			addChild(new UTF8Property("Name", textureName));

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			material.SetBump(textureName);
		}

		void unset() override
		{
			material.UnsetBump();
		}

	private:
		HPS::MaterialKit & material;
		HPS::UTF8 textureName;
	};

	class GlossProperty : public SettableProperty
	{
	public:
		GlossProperty(
			HPS::MaterialKit & material)
			: SettableProperty("Gloss")
			, material(material)
			, glossValue(1)
		{
		}

		void addSubItems()
		{
			bool _isSet = material.ShowGloss(glossValue);

			addChild(new FloatProperty("Value", glossValue));

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			material.SetGloss(glossValue);
		}

		void unset() override
		{
			material.UnsetGloss();
		}

	private:
		HPS::MaterialKit & material;
		float glossValue;
	};

	class LegacyShaderProperty : public SettableProperty
	{
	public:
		LegacyShaderProperty(
			HPS::MaterialKit & material)
			: SettableProperty("Legacy Shader")
			, material(material)
		{
		}

		void addSubItems()
		{
			bool _isSet = material.ShowLegacyShader(shaderName);
			if (!_isSet)
				shaderName = "shader";

			addChild(new UTF8Property("Name", shaderName));

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			material.SetLegacyShader(shaderName);
		}

		void unset() override
		{
			material.UnsetLegacyShader();
		}

	private:
		HPS::MaterialKit & material;
		HPS::UTF8 shaderName;
	};

	class MaterialProperty : public BaseProperty
	{
	public:
		MaterialProperty(
			QTreeWidget * tree,
			const char * const & name,
			HPS::MaterialKit & material)
			: BaseProperty(name)
			, material(material)
		{
			DiffuseColorProperty *diffusecolorproperty = new DiffuseColorProperty(tree, material);
			addChild(diffusecolorproperty);
			diffusecolorproperty->addSubItems();

			DiffuseTextureProperty *diffusetextureproperty = new DiffuseTextureProperty(tree, material);
			addChild(diffusetextureproperty);
			diffusetextureproperty->addSubItems();

			EnvironmentProperty *environmentproperty = new EnvironmentProperty(tree, material);
			addChild(environmentproperty);
			environmentproperty->addSubItems();

			SpecularProperty *specularproperty = new SpecularProperty(tree, material);
			addChild(specularproperty);
			specularproperty->addSubItems();

			MirrorProperty *mirrorproperty = new MirrorProperty(tree, material);
			addChild(mirrorproperty);
			mirrorproperty->addSubItems();

			EmissionProperty *emissionproperty = new EmissionProperty(tree, material);
			addChild(emissionproperty);
			emissionproperty->addSubItems();

			TransmissionProperty *transmissionproperty = new TransmissionProperty(tree, material);
			addChild(transmissionproperty);
			transmissionproperty->addSubItems();

			BumpProperty *bumpproperty = new BumpProperty(material);
			addChild(bumpproperty);
			bumpproperty->addSubItems();

			GlossProperty *glossproperty = new GlossProperty(material);
			addChild(glossproperty);
			glossproperty->addSubItems();

			LegacyShaderProperty *shaderproperty = new LegacyShaderProperty(material);
			addChild(shaderproperty);
			shaderproperty->addSubItems();
		}

	private:
		HPS::MaterialKit & material;
	};

	class ComplexMaterialTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			MaterialPropertyIndex,
			IndexPropertyIndex
		};

	public:
		ComplexMaterialTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Type::FullMaterial; enumStrings[0] = "Full Material";
			enumValues[1] = HPS::Material::Type::MaterialIndex; enumStrings[1] = "Material Index";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto materialSibling = static_cast<BaseProperty *>(parent()->child(MaterialPropertyIndex));
			auto indexSibling = static_cast<BaseProperty *>(parent()->child(IndexPropertyIndex));
			if (enumValue == HPS::Material::Type::FullMaterial)
			{
				indexSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				materialSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::MaterialIndex)
			{
				materialSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				indexSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	template <
		bool (HPS::MaterialMappingKit::*ShowFunction)(HPS::Material::Type &, HPS::MaterialKit &, float &) const,
		HPS::MaterialMappingKit & (HPS::MaterialMappingKit::*SetMaterialFunction)(HPS::MaterialKit const &),
		HPS::MaterialMappingKit & (HPS::MaterialMappingKit::*SetIndexFunction)(float),
		HPS::MaterialMappingKit & (HPS::MaterialMappingKit::*UnsetFunction)()
	>
	class ComplexMaterialProperty : public SettableProperty
	{
	public:
		ComplexMaterialProperty(
			QTreeWidget * tree,
			const char * const & name,
			HPS::MaterialMappingKit & materialMapping)
			: SettableProperty(name)
			, materialMapping(materialMapping)
			, tree(tree)
		{

		}

		void addSubItems()
		{
			bool _isSet = (this->materialMapping.*ShowFunction)(type, material, materialIndex);
			if (_isSet)
			{
				if (type == HPS::Material::Type::FullMaterial)
					materialIndex = 1;
			}
			else
			{
				type = HPS::Material::Type::FullMaterial;
				materialIndex = 1;
			}

			auto typeChild = new ComplexMaterialTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new MaterialProperty(tree, "Material Kit", material));
			addChild(new FloatProperty("Material Index", materialIndex));

			typeChild->enableValidProperties();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (type == HPS::Material::Type::FullMaterial)
				(materialMapping.*SetMaterialFunction)(material);
			else if (type == HPS::Material::Type::MaterialIndex)
				(materialMapping.*SetIndexFunction)(materialIndex);
		}

		void unset() override
		{
			(materialMapping.*UnsetFunction)();
		}

	private:
		HPS::MaterialMappingKit & materialMapping;
		HPS::Material::Type type;
		HPS::MaterialKit material;
		float materialIndex;
		QTreeWidget * tree;
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowFrontFaceMaterial,
		&HPS::MaterialMappingKit::SetFrontFaceMaterial,
		&HPS::MaterialMappingKit::SetFrontFaceMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetFrontFaceMaterial
	> BaseFrontFaceMaterialProperty;
	class FrontFaceMaterialProperty : public BaseFrontFaceMaterialProperty
	{
	public:
		FrontFaceMaterialProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseFrontFaceMaterialProperty(tree, "Front Face Material", materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowBackFaceMaterial,
		&HPS::MaterialMappingKit::SetBackFaceMaterial,
		&HPS::MaterialMappingKit::SetBackFaceMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetBackFaceMaterial
	> BaseBackFaceMaterialProperty;
	class BackFaceMaterialProperty : public BaseBackFaceMaterialProperty
	{
	public:
		BackFaceMaterialProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseBackFaceMaterialProperty(tree, "Back Face Material", materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowEdgeMaterial,
		&HPS::MaterialMappingKit::SetEdgeMaterial,
		&HPS::MaterialMappingKit::SetEdgeMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetEdgeMaterial
	> BaseEdgeMaterialProperty;
	class EdgeMaterialProperty : public BaseEdgeMaterialProperty
	{
	public:
		EdgeMaterialProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseEdgeMaterialProperty(tree, "Edge Material", materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowVertexMaterial,
		&HPS::MaterialMappingKit::SetVertexMaterial,
		&HPS::MaterialMappingKit::SetVertexMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetVertexMaterial
	> BaseVertexMaterialProperty;
	class VertexMaterialProperty : public BaseVertexMaterialProperty
	{
	public:
		VertexMaterialProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseVertexMaterialProperty(tree, "Vertex Material", materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowCutFaceMaterial,
		&HPS::MaterialMappingKit::SetCutFaceMaterial,
		&HPS::MaterialMappingKit::SetCutFaceMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetCutFaceMaterial
	> BaseCutFaceMaterialProperty;
	class CutFaceMaterialProperty : public BaseCutFaceMaterialProperty
	{
	public:
		CutFaceMaterialProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseCutFaceMaterialProperty(tree, "CutFace Material", materialMapping)
		{}
	};

	template <typename Kit>
	class PolyhedronMaterialMappingProperty : public SettableProperty
	{
	public:
		PolyhedronMaterialMappingProperty(
			QTreeWidget * tree,
			Kit & kit)
			: SettableProperty("MaterialMapping")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaterialMapping(materialMapping);
			addChild(new FrontFaceMaterialProperty(tree, materialMapping));
			addChild(new BackFaceMaterialProperty(tree, materialMapping));
			addChild(new EdgeMaterialProperty(tree, materialMapping));
			addChild(new VertexMaterialProperty(tree, materialMapping));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetMaterialMapping(materialMapping);
		}

		void unset() override
		{
			kit.UnsetMaterialMapping();
		}

	private:
		Kit & kit;
		HPS::MaterialMappingKit materialMapping;
		QTreeWidget * tree;
	};
	typedef PolyhedronMaterialMappingProperty<HPS::ShellKit> ShellKitMaterialMappingProperty;
	typedef PolyhedronMaterialMappingProperty<HPS::EXPERIMENTAL::CellularVolumeKit> CellularVolumeKitMaterialMappingProperty;
	typedef PolyhedronMaterialMappingProperty<HPS::MeshKit> MeshKitMaterialMappingProperty;

	template <typename Kit>
	class PolyhedronFaceColorsProperty : public BaseProperty
	{
	public:
		PolyhedronFaceColorsProperty(
			Kit const & kit)
			: BaseProperty("FaceColors")
		{
			HPS::UTF8 colorQuantity;
			HPS::MaterialTypeArray types;
			HPS::RGBColorArray rgbColors;
			HPS::FloatArray indices;
			if (kit.ShowFaceColors(types, rgbColors, indices))
			{
				if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
					colorQuantity = "All";
				else
					colorQuantity = "Some";
			}
			else
				colorQuantity = "None";
			addChild(new ImmutableUTF8Property("Quantity", colorQuantity));
		}
	};
	typedef PolyhedronFaceColorsProperty<HPS::ShellKit> ShellKitFaceColorsProperty;
	typedef PolyhedronFaceColorsProperty<HPS::MeshKit> MeshKitFaceColorsProperty;

	class ShellKitProperty : public RootProperty
	{
	public:
		ShellKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::ShellKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new ShellKitPointsProperty(kit));
			ctrl->addChild(new ShellKitFacelistProperty(kit));
			ctrl->addChild(new ShellKitVertexColorsProperty(kit));
			ctrl->addChild(new ShellKitVertexNormalsProperty(kit));
			ctrl->addChild(new ShellKitVertexParametersProperty(kit));
			ctrl->addChild(new ShellKitVertexVisibilitiesProperty(kit));
			ctrl->addChild(new ShellKitFaceColorsProperty(kit));
			ctrl->addChild(new ShellKitFaceNormalsProperty(kit));
			ctrl->addChild(new ShellKitFaceVisibilitiesProperty(kit));


			ShellKitPriorityProperty *shellkitpriorityproperty = new ShellKitPriorityProperty(kit);
			ctrl->addChild(shellkitpriorityproperty);
			shellkitpriorityproperty->addSubItems();

			ShellKitUserDataProperty *shellkituserdataproperty = new ShellKitUserDataProperty(kit);
			ctrl->addChild(shellkituserdataproperty);
			shellkituserdataproperty->addSubItems();

			ShellKitMaterialMappingProperty *shellkitmaterialmappingproperty = new ShellKitMaterialMappingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(shellkitmaterialmappingproperty);
			shellkitmaterialmappingproperty->addSubItems();
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::ShellKey key;
		HPS::ShellKit kit;
	};

	class CellularVolumeKitProperty: public RootProperty {
	public:
	    CellularVolumeKitProperty(
			QTreeWidgetItem * ctrl, HPS::EXPERIMENTAL::CellularVolumeKey const& key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new CellularVolumeKitPointsProperty(kit));
			ctrl->addChild(new CellularVolumeKitCellsListProperty(kit));
			ctrl->addChild(new CellularVolumeKitVertexColorsProperty(kit));
			//ctrl->addChild(new CellularVolumeKitVertexNormalsProperty(kit));
			ctrl->addChild(new CellularVolumeKitVertexParametersProperty(kit));


			CellularVolumeKitPriorityProperty* cellularvolumekitpriorityproperty = new CellularVolumeKitPriorityProperty(kit);
			ctrl->addChild(cellularvolumekitpriorityproperty);
			cellularvolumekitpriorityproperty->addSubItems();

			CellularVolumeKitUserDataProperty* cellularvolumekituserdataproperty = new CellularVolumeKitUserDataProperty(kit);
			ctrl->addChild(cellularvolumekituserdataproperty);
			cellularvolumekituserdataproperty->addSubItems();

			CellularVolumeKitMaterialMappingProperty* cellularvolumekitmaterialmappingproperty =
	            new CellularVolumeKitMaterialMappingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(cellularvolumekitmaterialmappingproperty);
			cellularvolumekitmaterialmappingproperty->addSubItems();
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::EXPERIMENTAL::CellularVolumeKey key;
		HPS::EXPERIMENTAL::CellularVolumeKit kit;
	};

	class MeshKitPointsProperty : public BaseProperty
	{
	public:
		MeshKitPointsProperty(
			HPS::MeshKit const & kit)
			: BaseProperty("Points")
		{
			size_t rows;
			size_t columns;
			kit.ShowRows(rows);
			kit.ShowColumns(columns);
			addChild(new ImmutableSizeTProperty("Count", kit.GetPointCount()));
			addChild(new ImmutableSizeTProperty("Rows", rows));
			addChild(new ImmutableSizeTProperty("Columns", columns));
		}
	};

	class MeshKitVertexColorsProperty : public BaseProperty
	{
	public:
		MeshKitVertexColorsProperty(
			HPS::MeshKit const & kit)
			: BaseProperty("VertexColors")
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::Mesh::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::Mesh::Component::Edges);
			HPS::UTF8 vertexQuantity = getVertexColorQuantityForComponent(kit, HPS::Mesh::Component::Vertices);
			addChild(new ImmutableUTF8Property("Faces", faceQuantity));
			addChild(new ImmutableUTF8Property("Edges", edgeQuantity));
			addChild(new ImmutableUTF8Property("Vertices", vertexQuantity));
		}
	};

	class MeshKitProperty : public RootProperty
	{
	public:
		MeshKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::MeshKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new MeshKitPointsProperty(kit));
			ctrl->addChild(new MeshKitVertexColorsProperty(kit));
			ctrl->addChild(new MeshKitVertexNormalsProperty(kit));
			ctrl->addChild(new MeshKitVertexParametersProperty(kit));
			ctrl->addChild(new MeshKitVertexVisibilitiesProperty(kit));
			ctrl->addChild(new MeshKitFaceColorsProperty(kit));
			ctrl->addChild(new MeshKitFaceNormalsProperty(kit));
			ctrl->addChild(new MeshKitFaceVisibilitiesProperty(kit));

			MeshKitPriorityProperty *meshkitpriorityproperty = new MeshKitPriorityProperty(kit);
			ctrl->addChild(meshkitpriorityproperty);
			meshkitpriorityproperty->addSubItems();

			MeshKitUserDataProperty *meshkituserdataproperty = new MeshKitUserDataProperty(kit);
			ctrl->addChild(meshkituserdataproperty);
			meshkituserdataproperty->addSubItems();

			MeshKitMaterialMappingProperty *meshkitmaterialmappingproperty = new MeshKitMaterialMappingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(meshkitmaterialmappingproperty);
			meshkitmaterialmappingproperty->addSubItems();


		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::MeshKey key;
		HPS::MeshKit kit;
	};

	class CuttingSectionKitPlanesProperty : public SettableArrayProperty
	{
	public:
		CuttingSectionKitPlanesProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionKit & kit)
			: SettableArrayProperty("Planes")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPlanes(planes);
			if (_isSet)
				planeCount = static_cast<unsigned int>(planes.size());
			else
			{
				planeCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty * array_size = new ArraySizeProperty("Count", planeCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (planeCount < 1)
				return;
			AddOrDeleteItems(planeCount, static_cast<unsigned int>(planes.size()));
			kit.SetPlanes(planes);
		}

		void unset() override
		{
			kit.UnsetPlanes();
		}

		void ResizeArrays() override
		{
			planes.resize(planeCount, HPS::Plane::Zero());
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < planeCount; ++i)
			{
				std::string itemName = "Plane " + std::to_string(i);
				addChild(new PlaneProperty(itemName.c_str(), planes[i]));
			}
		}

	private:
		HPS::CuttingSectionKit & kit;
		unsigned int planeCount;
		HPS::PlaneArray planes;
		QTreeWidget * tree;
	};

	class CuttingSectionKitVisualizationProperty : public BaseProperty
	{
	public:
		CuttingSectionKitVisualizationProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionKit & kit)
			: BaseProperty("Visualization")
			, kit(kit)
		{
			if (!this->kit.ShowVisualization(_mode, _color, _scale))
			{
				_mode = HPS::CuttingSection::Mode::None;
				_color = HPS::RGBAColor(0, 0, 0, 0.25f);
				_scale = 1.0f;
			}
			addChild(new CuttingSectionModeProperty(tree, _mode));
			addChild(new RGBAColorProperty("Color", _color));
			addChild(new FloatProperty("Scale", _scale));
		}

		void onChildChanged() override
		{
			kit.SetVisualization(_mode, _color, _scale);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CuttingSectionKit & kit;
		HPS::CuttingSection::Mode _mode;
		HPS::RGBAColor _color;
		float _scale;
	};

	class CuttingSectionKitProperty : public RootProperty
	{
	public:
		CuttingSectionKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::CuttingSectionKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			CuttingSectionKitPlanesProperty *cuttingsectionkitplanesproperty = new CuttingSectionKitPlanesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(cuttingsectionkitplanesproperty);
			cuttingsectionkitplanesproperty->addSubItems();

			ctrl->addChild(new CuttingSectionKitVisualizationProperty(ctrl->treeWidget(), kit));

			CuttingSectionKitPriorityProperty *cuttingsectionkitpriorityproperty = new CuttingSectionKitPriorityProperty(kit);
			ctrl->addChild(cuttingsectionkitpriorityproperty);
			cuttingsectionkitpriorityproperty->addSubItems();

			CuttingSectionKitUserDataProperty *cuttingsectionkituserdataproperty = new CuttingSectionKitUserDataProperty(kit);
			ctrl->addChild(cuttingsectionkituserdataproperty);
			cuttingsectionkituserdataproperty->addSubItems();


		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CuttingSectionKey key;
		HPS::CuttingSectionKit kit;
	};

	class NURBSSurfaceKitUProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitUProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty("U")
		{
			size_t degree;
			kit.ShowUDegree(degree);
			size_t count;
			kit.ShowUCount(count);
			addChild(new ImmutableSizeTProperty("Degree", degree));
			addChild(new ImmutableSizeTProperty("Count", count));
		}
	};

	class NURBSSurfaceKitVProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitVProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty("V")
		{
			size_t degree;
			kit.ShowVDegree(degree);
			size_t count;
			kit.ShowVCount(count);
			addChild(new ImmutableSizeTProperty("Degree", degree));
			addChild(new ImmutableSizeTProperty("Count", count));
		}
	};

	typedef ImmutableArraySizeProperty <
		HPS::NURBSSurfaceKit,
		float,
		&HPS::NURBSSurfaceKit::ShowWeights
	> BaseNURBSSurfaceKitWeightsProperty;
	class NURBSSurfaceKitWeightsProperty : public BaseNURBSSurfaceKitWeightsProperty
	{
	public:
		NURBSSurfaceKitWeightsProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseNURBSSurfaceKitWeightsProperty("Weights", "Count", kit)
		{}
	};

	class NURBSSurfaceKitKnotsProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitKnotsProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty("Knots")
		{
			HPS::FloatArray uKnots;
			HPS::FloatArray vKnots;
			kit.ShowUKnots(uKnots);
			kit.ShowVKnots(vKnots);
			addChild(new ImmutableSizeTProperty("U Count", uKnots.size()));
			addChild(new ImmutableSizeTProperty("V Count", vKnots.size()));
		}
	};

	class NURBSSurfaceKitTrimsProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitTrimsProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty("Trims")
		{
			HPS::TrimKitArray trims;
			kit.ShowTrims(trims);
			addChild(new ImmutableSizeTProperty("Count", trims.size()));
		}
	};

	class NURBSSurfaceKitProperty : public RootProperty
	{
	public:
		NURBSSurfaceKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::NURBSSurfaceKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new NURBSSurfaceKitUProperty(kit));
			ctrl->addChild(new NURBSSurfaceKitVProperty(kit));
			ctrl->addChild(new NURBSSurfaceKitPointsProperty(kit));
			ctrl->addChild(new NURBSSurfaceKitWeightsProperty(kit));
			ctrl->addChild(new NURBSSurfaceKitKnotsProperty(kit));
			ctrl->addChild(new NURBSSurfaceKitTrimsProperty(kit));

			NURBSSurfaceKitPriorityProperty *nurbssurfacekitpriorityproperty = new NURBSSurfaceKitPriorityProperty(kit);
			ctrl->addChild(nurbssurfacekitpriorityproperty);
			nurbssurfacekitpriorityproperty->addSubItems();

			NURBSSurfaceKitUserDataProperty *nurbssurfacekituserdataproperty = new NURBSSurfaceKitUserDataProperty(kit);
			ctrl->addChild(nurbssurfacekituserdataproperty);
			nurbssurfacekituserdataproperty->addSubItems();


		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::NURBSSurfaceKey key;
		HPS::NURBSSurfaceKit kit;
	};

	typedef ImmutableArraySizeProperty <
		HPS::CylinderKit,
		float,
		&HPS::CylinderKit::ShowRadii
	> BaseCylinderKitRadiiProperty;
	class CylinderKitRadiiProperty : public BaseCylinderKitRadiiProperty
	{
	public:
		CylinderKitRadiiProperty(
			HPS::CylinderKit const & kit)
			: BaseCylinderKitRadiiProperty("Radii", "Count", kit)
		{}
	};

	class CylinderKitCapsProperty : public BaseProperty
	{
	public:
		CylinderKitCapsProperty(
			QTreeWidget * tree,
			HPS::CylinderKit & kit)
			: BaseProperty("Caps")
			, kit(kit)
		{
			this->kit.ShowCaps(capping);
			addChild(new CylinderCappingProperty(tree, capping));
		}

		void onChildChanged() override
		{
			kit.SetCaps(capping);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CylinderKit & kit;
		HPS::Cylinder::Capping capping;
	};

	class CylinderKitVertexColorsProperty : public BaseProperty
	{
	public:
		CylinderKitVertexColorsProperty(
			HPS::CylinderKit const & kit)
			: BaseProperty("VertexColors")
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::Cylinder::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::Cylinder::Component::Edges);
			addChild(new ImmutableUTF8Property("Faces", faceQuantity));
			addChild(new ImmutableUTF8Property("Edges", edgeQuantity));
		}

	private:
		HPS::UTF8 getVertexColorQuantityForComponent(
			HPS::CylinderKit const & kit,
			HPS::Cylinder::Component componentType)
		{
			HPS::MaterialTypeArray types;
			HPS::RGBColorArray rgbColors;
			HPS::FloatArray indices;
			if (kit.ShowVertexColors(componentType, types, rgbColors, indices))
			{
				if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
					return "All";
				else
					return "Some";
			}
			else
				return "None";
		}
	};

	class CylinderKitProperty : public RootProperty
	{
	public:
		CylinderKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::CylinderKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new CylinderKitPointsProperty(kit));
			ctrl->addChild(new CylinderKitRadiiProperty(kit));
			ctrl->addChild(new CylinderKitCapsProperty(ctrl->treeWidget(), kit));
			ctrl->addChild(new CylinderKitVertexColorsProperty(kit));

			CylinderKitPriorityProperty *cylinderkitpriorityproperty = new CylinderKitPriorityProperty(kit);
			ctrl->addChild(cylinderkitpriorityproperty);
			cylinderkitpriorityproperty->addSubItems();

			CylinderKitUserDataProperty *cylinderkituserdataproperty = new CylinderKitUserDataProperty(kit);
			ctrl->addChild(cylinderkituserdataproperty);
			cylinderkituserdataproperty->addSubItems();


		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CylinderKey key;
		HPS::CylinderKit kit;
	};

	class PolygonKitProperty : public RootProperty
	{
	public:
		PolygonKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::PolygonKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new PolygonKitPointsProperty(kit));

			PolygonKitPriorityProperty *polygonkitpriorityproperty = new PolygonKitPriorityProperty(kit);
			ctrl->addChild(polygonkitpriorityproperty);
			polygonkitpriorityproperty->addSubItems();

			PolygonKitUserDataProperty *polygonkituserdataproperty = new PolygonKitUserDataProperty(kit);
			ctrl->addChild(polygonkituserdataproperty);
			polygonkituserdataproperty->addSubItems();


		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::PolygonKey key;
		HPS::PolygonKit kit;
	};

	class LineKitProperty : public RootProperty
	{
	public:
		LineKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::LineKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new LineKitPointsProperty(kit));

			LineKitPriorityProperty *linekitpriorityproperty = new LineKitPriorityProperty(kit);
			ctrl->addChild(linekitpriorityproperty);
			linekitpriorityproperty->addSubItems();

			LineKitUserDataProperty *linekituserdataproperty = new LineKitUserDataProperty(kit);
			ctrl->addChild(linekituserdataproperty);
			linekituserdataproperty->addSubItems();
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::LineKey key;
		HPS::LineKit kit;
	};

	class NURBSCurveKitDegreeProperty : public BaseProperty
	{
	public:
		NURBSCurveKitDegreeProperty(
			HPS::NURBSCurveKit const & kit)
			: BaseProperty("Degree")
		{
			size_t degree;
			kit.ShowDegree(degree);
			addChild(new ImmutableSizeTProperty("Degree", degree));
		}
	};

	typedef ImmutableArraySizeProperty <
		HPS::NURBSCurveKit,
		float,
		&HPS::NURBSCurveKit::ShowWeights
	> BaseNURBSCurveKitWeightsProperty;
	class NURBSCurveKitWeightsProperty : public BaseNURBSCurveKitWeightsProperty
	{
	public:
		NURBSCurveKitWeightsProperty(
			HPS::NURBSCurveKit const & kit)
			: BaseNURBSCurveKitWeightsProperty("Weights", "Count", kit)
		{}
	};

	typedef ImmutableArraySizeProperty <
		HPS::NURBSCurveKit,
		float,
		&HPS::NURBSCurveKit::ShowKnots
	> BaseNURBSCurveKitKnotsProperty;
	class NURBSCurveKitKnotsProperty : public BaseNURBSCurveKitKnotsProperty
	{
	public:
		NURBSCurveKitKnotsProperty(
			HPS::NURBSCurveKit const & kit)
			: BaseNURBSCurveKitKnotsProperty("Knots", "Count", kit)
		{}
	};

	class NURBSCurveKitParametersProperty : public BaseProperty
	{
	public:
		NURBSCurveKitParametersProperty(
			HPS::NURBSCurveKit & kit)
			: BaseProperty("Parameters")
			, kit(kit)
		{
			this->kit.ShowParameters(start, end);
			addChild(new FloatProperty("Start", start));
			addChild(new FloatProperty("End", end));
		}

		void onChildChanged() override
		{
			kit.SetParameters(start, end);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::NURBSCurveKit & kit;
		float start;
		float end;
	};

	class NURBSCurveKitProperty : public RootProperty
	{
	public:
		NURBSCurveKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::NURBSCurveKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl->addChild(new NURBSCurveKitDegreeProperty(kit));
			ctrl->addChild(new NURBSCurveKitPointsProperty(kit));
			ctrl->addChild(new NURBSCurveKitWeightsProperty(kit));
			ctrl->addChild(new NURBSCurveKitKnotsProperty(kit));
			ctrl->addChild(new NURBSCurveKitParametersProperty(kit));

			NURBSCurveKitPriorityProperty *nurbscurvekitpriorityproperty = new NURBSCurveKitPriorityProperty(kit);
			ctrl->addChild(nurbscurvekitpriorityproperty);
			nurbscurvekitpriorityproperty->addSubItems();

			NURBSCurveKitUserDataProperty *nurbscurvekituserdataproperty = new NURBSCurveKitUserDataProperty(kit);
			ctrl->addChild(nurbscurvekituserdataproperty);
			nurbscurvekituserdataproperty->addSubItems();
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::NURBSCurveKey key;
		HPS::NURBSCurveKit kit;
	};

	class SimpleMaterialTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			RedPropertyIndex,
			GreenPropertyIndex,
			BluePropertyIndex,
			AlphaPropertyIndex,
			IndexPropertyIndex
		};

	public:
		SimpleMaterialTypeProperty(
			QTreeWidget * tree,
			HPS::Material::Type & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{
		}

		void setupChoices()
		{
			HPS::MaterialTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Type::RGBAColor; enumStrings[0] = "RGBAColor";
			enumValues[1] = HPS::Material::Type::MaterialIndex; enumStrings[1] = "Material Index";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto redSibling = static_cast<BaseProperty *>(parent()->child(RedPropertyIndex));
			auto greenSibling = static_cast<BaseProperty *>(parent()->child(GreenPropertyIndex));
			auto blueSibling = static_cast<BaseProperty *>(parent()->child(BluePropertyIndex));
			auto alphaSibling = static_cast<BaseProperty *>(parent()->child(AlphaPropertyIndex));
			auto indexSibling = static_cast<BaseProperty *>(parent()->child(IndexPropertyIndex));
			if (enumValue == HPS::Material::Type::RGBAColor)
			{
				redSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				greenSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				blueSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				alphaSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				indexSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::Material::Type::MaterialIndex)
			{
				redSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				greenSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				blueSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				alphaSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				indexSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	template <
		typename Kit,
		bool (Kit::*ShowFunction)(HPS::Material::Type &, HPS::RGBAColor &, float &) const,
		Kit & (Kit::*SetColorFunction)(HPS::RGBAColor const &),
		Kit & (Kit::*SetIndexFunction)(float),
		Kit & (Kit::*UnsetFunction)()
	>
	class SimpleMaterialProperty : public SettableProperty
	{
	public:
		SimpleMaterialProperty(
			QTreeWidget * tree,
			const char * name,
			Kit & kit)
			: SettableProperty(name)
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = (this->kit.*ShowFunction)(type, color, materialIndex);
			if (_isSet)
			{
				if (type == HPS::Material::Type::RGBAColor)
					materialIndex = 0;
				else if (type == HPS::Material::Type::MaterialIndex)
					color = HPS::RGBAColor::Black();
			}
			else
			{
				type = HPS::Material::Type::RGBAColor;
				color = HPS::RGBAColor::Black();
				materialIndex = 0;
			}

			auto typeChild = new SimpleMaterialTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();

			addChild(new UnitFloatProperty("Red", color.red));
			addChild(new UnitFloatProperty("Green", color.green));
			addChild(new UnitFloatProperty("Blue", color.blue));
			addChild(new UnitFloatProperty("Alpha", color.alpha));
			addChild(new UnsignedFloatProperty("Material Index", materialIndex));

			typeChild->enableValidProperties();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (type == HPS::Material::Type::RGBAColor)
				(kit.*SetColorFunction)(color);
			else if (type == HPS::Material::Type::MaterialIndex)
				(kit.*SetIndexFunction)(materialIndex);
		}

		void unset() override
		{
			(kit.*UnsetFunction)();
		}

	private:
		Kit & kit;
		HPS::Material::Type type;
		HPS::RGBAColor color;
		float materialIndex;
		QTreeWidget * tree;
	};

	typedef SimpleMaterialProperty <
		HPS::DistantLightKit,
		&HPS::DistantLightKit::ShowColor,
		&HPS::DistantLightKit::SetColor,
		&HPS::DistantLightKit::SetColorByIndex,
		&HPS::DistantLightKit::UnsetColor
	> BaseDistantLightKitColorProperty;
	class DistantLightKitColorProperty : public BaseDistantLightKitColorProperty
	{
	public:
		DistantLightKitColorProperty(
			QTreeWidget * tree,
			HPS::DistantLightKit & kit)
			: BaseDistantLightKitColorProperty(tree, "Color", kit)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::SpotlightKit,
		&HPS::SpotlightKit::ShowColor,
		&HPS::SpotlightKit::SetColor,
		&HPS::SpotlightKit::SetColorByIndex,
		&HPS::SpotlightKit::UnsetColor
	> BaseSpotlightKitColorProperty;
	class SpotlightKitColorProperty : public BaseSpotlightKitColorProperty
	{
	public:
		SpotlightKitColorProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseSpotlightKitColorProperty(tree, "Color", kit)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowLineColor,
		&HPS::MaterialMappingKit::SetLineColor,
		&HPS::MaterialMappingKit::SetLineMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetLineColor
	> BaseLineColorProperty;
	class LineColorProperty : public BaseLineColorProperty
	{
	public:
		LineColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseLineColorProperty(tree, "LineColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::TextKit,
		&HPS::TextKit::ShowColor,
		&HPS::TextKit::SetColor,
		&HPS::TextKit::SetColorByIndex,
		&HPS::TextKit::UnsetColor
	> BaseTextKitColorProperty;
	class TextKitColorProperty : public BaseTextKitColorProperty
	{
	public:
		TextKitColorProperty(
			QTreeWidget * tree,
			HPS::TextKit & kit)
			: BaseTextKitColorProperty(tree, "Color", kit)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowTextColor,
		&HPS::MaterialMappingKit::SetTextColor,
		&HPS::MaterialMappingKit::SetTextMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetTextColor
	> BaseTextColorProperty;
	class TextColorProperty : public BaseTextColorProperty
	{
	public:
		TextColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseTextColorProperty(tree, "TextColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowMarkerColor,
		&HPS::MaterialMappingKit::SetMarkerColor,
		&HPS::MaterialMappingKit::SetMarkerMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetMarkerColor
	> BaseMarkerColorProperty;
	class MarkerColorProperty : public BaseMarkerColorProperty
	{
	public:
		MarkerColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseMarkerColorProperty(tree, "MarkerColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowWindowColor,
		&HPS::MaterialMappingKit::SetWindowColor,
		&HPS::MaterialMappingKit::SetWindowMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetWindowColor
	> BaseWindowColorProperty;
	class WindowColorProperty : public BaseWindowColorProperty
	{
	public:
		WindowColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseWindowColorProperty(tree, "WindowColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowWindowContrastColor,
		&HPS::MaterialMappingKit::SetWindowContrastColor,
		&HPS::MaterialMappingKit::SetWindowContrastMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetWindowContrastColor
	> BaseWindowContrastColorProperty;
	class WindowContrastColorProperty : public BaseWindowContrastColorProperty
	{
	public:
		WindowContrastColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseWindowContrastColorProperty(tree, "WindowContrastColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowLightColor,
		&HPS::MaterialMappingKit::SetLightColor,
		&HPS::MaterialMappingKit::SetLightMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetLightColor
	> BaseLightColorProperty;
	class LightColorProperty : public BaseLightColorProperty
	{
	public:
		LightColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseLightColorProperty(tree, "LightColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowCutEdgeColor,
		&HPS::MaterialMappingKit::SetCutEdgeColor,
		&HPS::MaterialMappingKit::SetCutEdgeMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetCutEdgeColor
	> BaseCutEdgeColorProperty;
	class CutEdgeColorProperty : public BaseCutEdgeColorProperty
	{
	public:
		CutEdgeColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseCutEdgeColorProperty(tree, "CutEdgeColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowAmbientLightUpColor,
		&HPS::MaterialMappingKit::SetAmbientLightUpColor,
		&HPS::MaterialMappingKit::SetAmbientLightUpMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetAmbientLightUpColor
	> BaseAmbientLightUpColorProperty;
	class AmbientLightUpColorProperty : public BaseAmbientLightUpColorProperty
	{
	public:
		AmbientLightUpColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseAmbientLightUpColorProperty(tree, "AmbientLightUpColor", materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowAmbientLightDownColor,
		&HPS::MaterialMappingKit::SetAmbientLightDownColor,
		&HPS::MaterialMappingKit::SetAmbientLightDownMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetAmbientLightDownColor
	> BaseAmbientLightDownColorProperty;
	class AmbientLightDownColorProperty : public BaseAmbientLightDownColorProperty
	{
	public:
		AmbientLightDownColorProperty(
			QTreeWidget * tree,
			HPS::MaterialMappingKit & materialMapping)
			: BaseAmbientLightDownColorProperty(tree, "AmbientLightDownColor", materialMapping)
		{}
	};

	class MaterialMappingKitProperty : public RootProperty
	{
	public:
		MaterialMappingKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			QTreeWidget * tree = ctrl->treeWidget();
			this->key.ShowMaterialMapping(kit);
			FrontFaceMaterialProperty *frontfacematerialproperty = new FrontFaceMaterialProperty(tree, kit);
			ctrl->addChild(frontfacematerialproperty);
			frontfacematerialproperty->addSubItems();

			BackFaceMaterialProperty *backfacematerialproperty = new BackFaceMaterialProperty(tree, kit);
			ctrl->addChild(backfacematerialproperty);
			backfacematerialproperty->addSubItems();

			EdgeMaterialProperty *edgematerialproperty = new EdgeMaterialProperty(tree, kit);
			ctrl->addChild(edgematerialproperty);
			edgematerialproperty->addSubItems();

			VertexMaterialProperty *vertexmaterialproperty = new VertexMaterialProperty(tree, kit);
			ctrl->addChild(vertexmaterialproperty);
			vertexmaterialproperty->addSubItems();

			LineColorProperty *linecolorproperty = new LineColorProperty(tree, kit);
			ctrl->addChild(linecolorproperty);
			linecolorproperty->addSubItems();

			TextColorProperty *textcolorproperty = new TextColorProperty(tree, kit);
			ctrl->addChild(textcolorproperty);
			textcolorproperty->addSubItems();

			MarkerColorProperty *markercolorproperty = new MarkerColorProperty(tree, kit);
			ctrl->addChild(markercolorproperty);
			markercolorproperty->addSubItems();

			LightColorProperty *lightcolorproperty = new LightColorProperty(tree, kit);
			ctrl->addChild(lightcolorproperty);
			lightcolorproperty->addSubItems();

			WindowColorProperty *windowcolorproperty = new WindowColorProperty(tree, kit);
			ctrl->addChild(windowcolorproperty);
			windowcolorproperty->addSubItems();

			WindowContrastColorProperty *windowcontrastcolorproperty = new WindowContrastColorProperty(tree, kit);
			ctrl->addChild(windowcontrastcolorproperty);
			windowcontrastcolorproperty->addSubItems();

			AmbientLightUpColorProperty *ambientlightupcolorproperty = new AmbientLightUpColorProperty(tree, kit);
			ctrl->addChild(ambientlightupcolorproperty);
			ambientlightupcolorproperty->addSubItems();

			AmbientLightDownColorProperty *ambientlightdowncolorproperty = new AmbientLightDownColorProperty(tree, kit);
			ctrl->addChild(ambientlightdowncolorproperty);
			ambientlightdowncolorproperty->addSubItems();

			CutFaceMaterialProperty *cutfacematerialproperty = new CutFaceMaterialProperty(tree, kit);
			ctrl->addChild(cutfacematerialproperty);
			cutfacematerialproperty->addSubItems();

			CutEdgeColorProperty *cutedgecolorproperty = new CutEdgeColorProperty(tree, kit);
			ctrl->addChild(cutedgecolorproperty);
			cutedgecolorproperty->addSubItems();


		}

		void Apply() override
		{
			key.UnsetMaterialMapping();
			key.SetMaterialMapping(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::MaterialMappingKit kit;
	};

	class CameraKitUpVectorProperty : public BaseProperty
	{
	public:
		CameraKitUpVectorProperty(
			HPS::CameraKit & kit)
			: BaseProperty("UpVector")
			, kit(kit)
		{
			this->kit.ShowUpVector(_up_vector);
			addChild(new VectorProperty("Up Vector", _up_vector));
		}

		void onChildChanged() override
		{
			kit.SetUpVector(_up_vector);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Vector _up_vector;
	};

	class CameraKitPositionProperty : public BaseProperty
	{
	public:
		CameraKitPositionProperty(
			HPS::CameraKit & kit)
			: BaseProperty("Position")
			, kit(kit)
		{
			this->kit.ShowPosition(_position);
			addChild(new PointProperty("Position", _position));
		}

		void onChildChanged() override
		{
			kit.SetPosition(_position);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Point _position;
	};

	class CameraKitTargetProperty : public BaseProperty
	{
	public:
		CameraKitTargetProperty(
			HPS::CameraKit & kit)
			: BaseProperty("Target")
			, kit(kit)
		{
			this->kit.ShowTarget(_target);
			addChild(new PointProperty("Target", _target));
		}

		void onChildChanged() override
		{
			kit.SetTarget(_target);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Point _target;
	};

	class CameraKitProjectionProperty : public BaseProperty
	{
	public:
		CameraKitProjectionProperty(
			QTreeWidget * tree,
			HPS::CameraKit & kit)
			: BaseProperty("Projection")
			, kit(kit)
		{
			this->kit.ShowProjection(_type, _oblique_y_skew, _oblique_x_skew);
			addChild(new CameraProjectionProperty(tree, _type));
			addChild(new FloatProperty("Oblique Y Skew", _oblique_y_skew));
			addChild(new FloatProperty("Oblique X Skew", _oblique_x_skew));
		}

		void onChildChanged() override
		{
			kit.SetProjection(_type, _oblique_y_skew, _oblique_x_skew);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Camera::Projection _type;
		float _oblique_y_skew;
		float _oblique_x_skew;
	};

	class CameraKitFieldProperty : public BaseProperty
	{
	public:
		CameraKitFieldProperty(
			HPS::CameraKit & kit)
			: BaseProperty("Field")
			, kit(kit)
		{
			this->kit.ShowField(_width, _height);
			addChild(new UnsignedFloatProperty("Width", _width));
			addChild(new UnsignedFloatProperty("Height", _height));
		}

		void onChildChanged() override
		{
			kit.SetField(_width, _height);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		float _width;
		float _height;
	};

	class CameraKitNearLimitProperty : public BaseProperty
	{
	public:
		CameraKitNearLimitProperty(
			HPS::CameraKit & kit)
			: BaseProperty("NearLimit")
			, kit(kit)
		{
			this->kit.ShowNearLimit(_near_limit);
			addChild(new FloatProperty("Near Limit", _near_limit));
		}

		void onChildChanged() override
		{
			kit.SetNearLimit(_near_limit);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		float _near_limit;
	};

	class CameraKitProperty : public RootProperty
	{
	public:
		CameraKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			if (!this->key.ShowCamera(kit))
				kit = HPS::CameraKit::GetDefault();

			ctrl->addChild(new CameraKitPositionProperty(kit));
			ctrl->addChild(new CameraKitTargetProperty(kit));
			ctrl->addChild(new CameraKitUpVectorProperty(kit));
			ctrl->addChild(new CameraKitProjectionProperty(ctrl->treeWidget(), kit));
			ctrl->addChild(new CameraKitFieldProperty(kit));
			ctrl->addChild(new CameraKitNearLimitProperty(kit));
		}

		void Apply() override
		{
			key.SetCamera(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CameraKit kit;
	};

	template <
		typename Key,
		bool (Key::*ShowMatrix)(HPS::MatrixKit &) const
	>
	class KeyMatrixProperty : public SettableProperty
	{
	public:
		KeyMatrixProperty(
			const char * name,
			Key const & key)
			: SettableProperty(name)
			, key(key)
		{
		}

		void addSubItems()
		{
			bool _isSet = (key.*ShowMatrix)(matrix);
			matrix.ShowElements(elements);
			for (size_t i = 0; i < elements.size(); ++i)
			{
				auto ithName = std::to_string(i);
				addChild(new FloatProperty(ithName.c_str(), elements[i]));
			}
			isSet(_isSet);
		}

		HPS::MatrixKit GetMatrix() const
		{
			return matrix;
		}

	protected:
		void set() override
		{
			matrix.SetElements(elements);
		}

		void unset() override
		{
			// nothing to do
		}

	private:
		HPS::MatrixKit matrix;
		HPS::FloatArray elements;
		Key const & key;
	};

	class SegmentKeyModellingMatrixProperty : public RootProperty
	{
	private:
		typedef KeyMatrixProperty<HPS::SegmentKey, &HPS::SegmentKey::ShowModellingMatrix> ModellingMatrixProperty;

		enum PropertyTypeIndex
		{
			MatrixPropertyIndex = 0,
		};

	public:
		SegmentKeyModellingMatrixProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ModellingMatrixProperty *modellingmatrixproperty = new ModellingMatrixProperty("Modelling Matrix", this->key);
			ctrl->addChild(modellingmatrixproperty);
			modellingmatrixproperty->addSubItems();
		}

		void Apply() override
		{
			auto matrixChild = static_cast<ModellingMatrixProperty *>(item->child(MatrixPropertyIndex));
			if (matrixChild->isSet())
				key.SetModellingMatrix(matrixChild->GetMatrix());
			else
				key.UnsetModellingMatrix();
		}

	private:
		HPS::SegmentKey key;
	};

	class ReferenceKeyModellingMatrixProperty : public RootProperty
	{
	private:
		typedef KeyMatrixProperty<HPS::ReferenceKey, &HPS::ReferenceKey::ShowModellingMatrix> ModellingMatrixProperty;

		enum PropertyTypeIndex
		{
			MatrixPropertyIndex = 0,
		};

	public:
		ReferenceKeyModellingMatrixProperty(
			QTreeWidgetItem * ctrl,
			HPS::ReferenceKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ModellingMatrixProperty *modellingmatrixproperty = new ModellingMatrixProperty("Modelling Matrix", this->key);
			ctrl->addChild(modellingmatrixproperty);
			modellingmatrixproperty->addSubItems();
		}

		void Apply() override
		{
			auto matrixChild = static_cast<ModellingMatrixProperty *>(item->child(MatrixPropertyIndex));
			if (matrixChild->isSet())
				key.SetModellingMatrix(matrixChild->GetMatrix());
			else
				key.UnsetModellingMatrix();
		}

	private:
		HPS::ReferenceKey key;
	};

	class KeyUserDataProperty : public SettableProperty
	{
	public:
		KeyUserDataProperty(
			HPS::SegmentKey const & key)
			: SettableProperty("User Data")
			, key(key)
		{
		}

		void addSubItems()
		{
			bool _isSet = key.ShowUserData(indices, data);
			if (_isSet)
			{
				addChild(new ImmutableSizeTProperty("Count", indices.size()));
				for (size_t i = 0; i < indices.size(); ++i)
					addChild(new SingleUserDataProperty(i, indices[i], data[i].size()));
			}
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			// nothing to do
		}

		void unset() override
		{
			// nothing to do
		}

	private:
		HPS::IntPtrTArray indices;
		HPS::ByteArrayArray data;
		HPS::SegmentKey const & key;
	};

	class SegmentKeyUserDataProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			UserDataPropertyIndex = 0,
		};

	public:
		SegmentKeyUserDataProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			KeyUserDataProperty *keyuserdataproperty = new KeyUserDataProperty(this->key);
			ctrl->addChild(keyuserdataproperty);
			keyuserdataproperty->addSubItems();
		}

		void Apply() override
		{
			auto userDataChild = static_cast<KeyUserDataProperty *>(item->child(UserDataPropertyIndex));
			if (!userDataChild->isSet())
				key.UnsetAllUserData();
		}

	private:
		HPS::SegmentKey key;
	};

	class SegmentKeyTextureMatrixProperty : public RootProperty
	{
	private:
		typedef KeyMatrixProperty<HPS::SegmentKey, &HPS::SegmentKey::ShowTextureMatrix> TextureMatrixProperty;

		enum PropertyTypeIndex
		{
			MatrixPropertyIndex = 0,
		};

	public:
		SegmentKeyTextureMatrixProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			TextureMatrixProperty *texturematrixproperty = new TextureMatrixProperty("Texture Matrix", this->key);
			ctrl->addChild(texturematrixproperty);
			texturematrixproperty->addSubItems();
		}

		void Apply() override
		{
			auto matrixChild = static_cast<TextureMatrixProperty *>(item->child(MatrixPropertyIndex));
			if (matrixChild->isSet())
				key.SetTextureMatrix(matrixChild->GetMatrix());
			else
				key.UnsetTextureMatrix();
		}

	private:
		HPS::SegmentKey key;
	};

	class LinePatternModifierProperty : public BaseEnumProperty<HPS::LinePattern::Modifier>
	{
	private:
		enum PropertyTypeIndex
		{
			ModifierPropertyIndex = 0,
			GlyphPropertyIndex,
			EnumPropertyIndex,
		};

	public:
		LinePatternModifierProperty(
			QTreeWidget * tree,
			HPS::LinePattern::Modifier & enumValue)
			: BaseEnumProperty("Modifier", enumValue)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::LinePattern::Modifier::GlyphName; enumStrings[0] = "GlyphName";
			enumValues[1] = HPS::LinePattern::Modifier::Enumerated; enumStrings[1] = "Enumerated";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto glyphSibling = static_cast<BaseProperty *>(parent())->child(GlyphPropertyIndex);
			auto enumSibling = static_cast<BaseProperty *>(parent())->child(EnumPropertyIndex);
			if (enumValue == HPS::LinePattern::Modifier::GlyphName)
			{
				glyphSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
				enumSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::LinePattern::Modifier::Enumerated)
			{
				glyphSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
				enumSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	template <
		typename EnumType,
		typename EnumTypeProperty,
		EnumType defaultEnumValue,
		bool (HPS::LinePatternOptionsKit::*ShowFunction)(HPS::LinePattern::Modifier &, HPS::UTF8 &, EnumType &) const,
		HPS::LinePatternOptionsKit & (HPS::LinePatternOptionsKit::*SetGlyph)(char const *),
		HPS::LinePatternOptionsKit & (HPS::LinePatternOptionsKit::*SetEnumType)(EnumType),
		HPS::LinePatternOptionsKit & (HPS::LinePatternOptionsKit::*UnsetFunction)()
	>
	class CapOrJoinProperty : public SettableProperty
	{
	public:
		CapOrJoinProperty(
			QTreeWidget * tree,
			const char * name,
			HPS::LinePatternOptionsKit & kit)
			: SettableProperty(name)
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = (this->kit.*ShowFunction)(_modifier, _glyph, _type);
			if (_isSet)
			{
				if (_modifier == HPS::LinePattern::Modifier::GlyphName)
					_type = defaultEnumValue;
				else if (_modifier == HPS::LinePattern::Modifier::Enumerated)
					_glyph = "glyph";
			}
			else
			{
				_modifier = HPS::LinePattern::Modifier::Enumerated;
				_glyph = "glyph";
				_type = defaultEnumValue;
			}
			auto modifierProperty = new LinePatternModifierProperty(tree, _modifier);
			addChild(modifierProperty);
			modifierProperty->setupChoices();

			addChild(new UTF8Property("Glyph", _glyph));
			addChild(new EnumTypeProperty(tree, _type));
			modifierProperty->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (_modifier == HPS::LinePattern::Modifier::GlyphName)
				(kit.*SetGlyph)(_glyph);
			else if (_modifier == HPS::LinePattern::Modifier::Enumerated)
				(kit.*SetEnumType)(_type);
		}

		void unset() override
		{
			(kit.*UnsetFunction)();
		}

	private:
		HPS::LinePatternOptionsKit & kit;
		HPS::LinePattern::Modifier _modifier;
		HPS::UTF8 _glyph;
		EnumType _type;
		QTreeWidget * tree;
	};

	typedef CapOrJoinProperty <
		HPS::LinePattern::Cap,
		LinePatternCapProperty,
		HPS::LinePattern::Cap::Butt,
		&HPS::LinePatternOptionsKit::ShowStartCap,
		&HPS::LinePatternOptionsKit::SetStartCap,
		&HPS::LinePatternOptionsKit::SetStartCap,
		&HPS::LinePatternOptionsKit::UnsetStartCap
	> BaseLinePatternOptionsKitStartCapProperty;
	class LinePatternOptionsKitStartCapProperty : public BaseLinePatternOptionsKitStartCapProperty
	{
	public:
		LinePatternOptionsKitStartCapProperty(
			QTreeWidget * tree,
			HPS::LinePatternOptionsKit & kit)
			: BaseLinePatternOptionsKitStartCapProperty(tree, "Start Cap", kit)
		{}
	};

	typedef CapOrJoinProperty <
		HPS::LinePattern::Cap,
		LinePatternCapProperty,
		HPS::LinePattern::Cap::Butt,
		&HPS::LinePatternOptionsKit::ShowEndCap,
		&HPS::LinePatternOptionsKit::SetEndCap,
		&HPS::LinePatternOptionsKit::SetEndCap,
		&HPS::LinePatternOptionsKit::UnsetEndCap
	> BaseLinePatternOptionsKitEndCapProperty;
	class LinePatternOptionsKitEndCapProperty : public BaseLinePatternOptionsKitEndCapProperty
	{
	public:
		LinePatternOptionsKitEndCapProperty(
			QTreeWidget * tree,
			HPS::LinePatternOptionsKit & kit)
			: BaseLinePatternOptionsKitEndCapProperty(tree, "End Cap", kit)
		{}
	};

	typedef CapOrJoinProperty <
		HPS::LinePattern::Join,
		LinePatternJoinProperty,
		HPS::LinePattern::Join::Mitre,
		&HPS::LinePatternOptionsKit::ShowJoin,
		&HPS::LinePatternOptionsKit::SetJoin,
		&HPS::LinePatternOptionsKit::SetJoin,
		&HPS::LinePatternOptionsKit::UnsetJoin
	> BaseLinePatternOptionsKitJoinProperty;
	class LinePatternOptionsKitJoinProperty : public BaseLinePatternOptionsKitJoinProperty
	{
	public:
		LinePatternOptionsKitJoinProperty(
			QTreeWidget * tree,
			HPS::LinePatternOptionsKit & kit)
			: BaseLinePatternOptionsKitJoinProperty(tree, "Join", kit)
		{}
	};

	class LinePatternOptionsKitInnerCapProperty : public SettableProperty
	{
	public:
		LinePatternOptionsKitInnerCapProperty(
			QTreeWidget * tree,
			HPS::LinePatternOptionsKit & kit)
			: SettableProperty("InnerCap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowInnerCap(_type);
			if (!_isSet)
				_type = HPS::LinePattern::Cap::Butt;
			addChild(new LinePatternCapProperty(tree, _type));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetInnerCap(_type);
		}

		void unset() override
		{
			kit.UnsetInnerCap();
		}

	private:
		HPS::LinePatternOptionsKit & kit;
		HPS::LinePattern::Cap _type;
		QTreeWidget * tree;
	};

	class LinePatternOptionsKitProperty : public BaseProperty
	{
	public:
		LinePatternOptionsKitProperty(
			QTreeWidget * tree,
			const char * name,
			HPS::LinePatternOptionsKit & kit)
			: BaseProperty(name)
		{
			LinePatternOptionsKitStartCapProperty *linepatternoptionskitstartcapproperty = new LinePatternOptionsKitStartCapProperty(tree, kit);
			addChild(linepatternoptionskitstartcapproperty);
			linepatternoptionskitstartcapproperty->addSubItems();

			LinePatternOptionsKitEndCapProperty *linepatternoptionskitendcapproperty = new LinePatternOptionsKitEndCapProperty(tree, kit);
			addChild(linepatternoptionskitendcapproperty);
			linepatternoptionskitendcapproperty->addSubItems();

			LinePatternOptionsKitInnerCapProperty *linepatternoptionskitinnercapproperty = new LinePatternOptionsKitInnerCapProperty(tree, kit);
			addChild(linepatternoptionskitinnercapproperty);
			linepatternoptionskitinnercapproperty->addSubItems();

			LinePatternOptionsKitJoinProperty *linepatternoptionskitjoinproperty = new LinePatternOptionsKitJoinProperty(tree, kit);
			addChild(linepatternoptionskitjoinproperty);
			linepatternoptionskitjoinproperty->addSubItems();
		}
	};

	class LineAttributeKitPatternProperty : public SettableProperty
	{
	public:
		LineAttributeKitPatternProperty(
			QTreeWidget * tree,
			HPS::LineAttributeKit & kit)
			: SettableProperty("Pattern")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPattern(_pattern, _options);
			if (!_isSet)
			{
				_pattern = "pattern";
				_options = HPS::LinePatternOptionsKit();
			}
			addChild(new UTF8Property("Pattern", _pattern));
			addChild(new LinePatternOptionsKitProperty(tree, "Options", _options));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetPattern(_pattern, _options);
		}

		void unset() override
		{
			kit.UnsetPattern();
		}

	private:
		HPS::LineAttributeKit & kit;
		HPS::UTF8 _pattern;
		HPS::LinePatternOptionsKit _options;
		QTreeWidget * tree;
	};

	class LineAttributeKitWeightProperty : public SettableProperty
	{
	public:
		LineAttributeKitWeightProperty(
			QTreeWidget * tree,
			HPS::LineAttributeKit & kit)
			: SettableProperty("Weight")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWeight(_weight, _units);
			if (!_isSet)
			{
				_weight = 1.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			addChild(new FloatProperty("Weight", _weight));
			addChild(new LineSizeUnitsProperty(tree, _units));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetWeight(_weight, _units);
		}

		void unset() override
		{
			kit.UnsetWeight();
		}

	private:
		HPS::LineAttributeKit & kit;
		float _weight;
		HPS::Line::SizeUnits _units;
		QTreeWidget * tree;
	};

	class LineAttributeKitProperty : public RootProperty
	{
	public:
		LineAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowLineAttribute(kit);
			LineAttributeKitPatternProperty *lineattributekitpatternproperty = new LineAttributeKitPatternProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(lineattributekitpatternproperty);
			lineattributekitpatternproperty->addSubItems();

			LineAttributeKitWeightProperty *lineattributekitweightproperty = new LineAttributeKitWeightProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(lineattributekitweightproperty);
			lineattributekitweightproperty->addSubItems();
		}

		void Apply() override
		{
			key.UnsetLineAttribute();
			key.SetLineAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::LineAttributeKit kit;
	};

	class SubwindowKitSubwindowProperty : public SettableProperty
	{
	public:
		SubwindowKitSubwindowProperty(
			QTreeWidget * tree,
			HPS::SubwindowKit & kit)
			: SettableProperty("Subwindow")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSubwindow(_subwindow_position, _subwindow_offsets, _subwindow_type);
			if (!_isSet)
			{
				_subwindow_position = HPS::Rectangle(-1.0f, 1.0f, -1.0f, 1.0f);
				_subwindow_offsets = HPS::IntRectangle::Zero();
				_subwindow_type = HPS::Subwindow::Type::Standard;
			}
			addChild(new RectangleProperty("Subwindow Position", _subwindow_position));
			addChild(new IntRectangleProperty("Subwindow Offsets", _subwindow_offsets));
			addChild(new SubwindowTypeProperty(tree, _subwindow_type));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetSubwindow(_subwindow_position, _subwindow_offsets, _subwindow_type);
		}

		void unset() override
		{
			kit.UnsetSubwindow();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Rectangle _subwindow_position;
		HPS::IntRectangle _subwindow_offsets;
		HPS::Subwindow::Type _subwindow_type;
		QTreeWidget * tree;
	};

	class SubwindowBackgroundProperty : public BaseEnumProperty<HPS::Subwindow::Background>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			NamePropertyIndex,
		};

	public:
		SubwindowBackgroundProperty(
			QTreeWidget * tree,
			HPS::Subwindow::Background & enumValue)
			: BaseEnumProperty("Type", enumValue)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			EnumTypeArray enumValues(14); HPS::UTF8Array enumStrings(14);
			enumValues[0] = HPS::Subwindow::Background::SolidColor; enumStrings[0] = "SolidColor";
			enumValues[1] = HPS::Subwindow::Background::Image; enumStrings[1] = "Image";
			enumValues[2] = HPS::Subwindow::Background::Cubemap; enumStrings[2] = "Cubemap";
			enumValues[3] = HPS::Subwindow::Background::Blend; enumStrings[3] = "Blend";
			enumValues[4] = HPS::Subwindow::Background::Transparent; enumStrings[4] = "Transparent";
			enumValues[5] = HPS::Subwindow::Background::Interactive; enumStrings[5] = "Interactive";
			enumValues[6] = HPS::Subwindow::Background::GradientTopToBottom; enumStrings[6] = "GradientTopToBottom";
			enumValues[7] = HPS::Subwindow::Background::GradientBottomToTop; enumStrings[7] = "GradientBottomToTop";
			enumValues[8] = HPS::Subwindow::Background::GradientLeftToRight; enumStrings[8] = "GradientLeftToRight";
			enumValues[9] = HPS::Subwindow::Background::GradientRightToLeft; enumStrings[9] = "GradientRightToLeft";
			enumValues[10] = HPS::Subwindow::Background::GradientTopLeftToBottomRight; enumStrings[10] = "GradientTopLeftToBottomRight";
			enumValues[11] = HPS::Subwindow::Background::GradientTopRightToBottomLeft; enumStrings[11] = "GradientTopRightToBottomLeft";
			enumValues[12] = HPS::Subwindow::Background::GradientBottomLeftToTopRight; enumStrings[12] = "GradientBottomLeftToTopRight";
			enumValues[13] = HPS::Subwindow::Background::GradientBottomRightToTopLeft; enumStrings[13] = "GradientBottomRightToTopLeft";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto nameSibling = static_cast<BaseProperty *>(parent())->child(NamePropertyIndex);
			if (enumValue == HPS::Subwindow::Background::Image || enumValue == HPS::Subwindow::Background::Cubemap)
				nameSibling->setFlags(flags() | Qt::ItemFlag::ItemIsEnabled);
			else
				nameSibling->setFlags(flags() & ~Qt::ItemFlag::ItemIsEnabled);
		}

		QTreeWidget * tree;
	};

	class SubwindowKitBackgroundProperty : public SettableProperty
	{
	public:
		SubwindowKitBackgroundProperty(
			QTreeWidget * tree,
			HPS::SubwindowKit & kit)
			: SettableProperty("Background")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackground(_bg_type, _definition_name);
			if (!_isSet)
			{
				_bg_type = HPS::Subwindow::Background::SolidColor;
				_definition_name = "definition_name";
			}
			auto backgroundChild = new SubwindowBackgroundProperty(tree, _bg_type);
			addChild(backgroundChild);
			backgroundChild->setupChoices();
			addChild(new UTF8Property("Definition Name", _definition_name));
			backgroundChild->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetBackground(_bg_type, _definition_name);
		}

		void unset() override
		{
			kit.UnsetBackground();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Subwindow::Background _bg_type;
		HPS::UTF8 _definition_name;
		QTreeWidget * tree;
	};

	class SubwindowKitBorderProperty : public SettableProperty
	{
	public:
		SubwindowKitBorderProperty(
			QTreeWidget * tree,
			HPS::SubwindowKit & kit)
			: SettableProperty("Border")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBorder(_border_type);
			if (!_isSet)
			{
				_border_type = HPS::Subwindow::Border::None;
			}
			addChild(new SubwindowBorderProperty(tree, _border_type));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetBorder(_border_type);
		}

		void unset() override
		{
			kit.UnsetBorder();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Subwindow::Border _border_type;
		QTreeWidget * tree;
	};

	class SubwindowKitRenderingAlgorithmProperty : public SettableProperty
	{
	public:
		SubwindowKitRenderingAlgorithmProperty(
			QTreeWidget * tree,
			HPS::SubwindowKit & kit)
			: SettableProperty("RenderingAlgorithm")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRenderingAlgorithm(_hsra);
			if (!_isSet)
			{
				_hsra = HPS::Subwindow::RenderingAlgorithm::ZBuffer;
			}
			addChild(new SubwindowRenderingAlgorithmProperty(tree, _hsra));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetRenderingAlgorithm(_hsra);
		}

		void unset() override
		{
			kit.UnsetRenderingAlgorithm();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Subwindow::RenderingAlgorithm _hsra;
		QTreeWidget * tree;
	};

	class SubwindowKitModelCompareModeProperty : public SettableProperty
	{
	public:
		SubwindowKitModelCompareModeProperty(
			QTreeWidget * tree,
			HPS::SubwindowKit & kit)
			: SettableProperty("ModelCompareMode")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowModelCompareMode(state, source1, source2);
			HPS::UTF8 source1Name;
			HPS::UTF8 source2Name;
			if (_isSet && state)
			{
				source1Name = source1.Name();
				source2Name = source2.Name();
			}
			else
			{
				state = false;
				source1 = HPS::SegmentKey();
				source1Name = "No Segment";
				source2 = HPS::SegmentKey();
				source2Name = "No Segment";
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new ImmutableUTF8Property("Source1", source1Name));
			addChild(new ImmutableUTF8Property("Source2", source2Name));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetModelCompareMode(state, source1, source2);
		}

		void unset() override
		{
			kit.UnsetModelCompareMode();
		}

	private:
		HPS::SubwindowKit & kit;
		bool state;
		HPS::SegmentKey source1;
		HPS::SegmentKey source2;
		QTreeWidget * tree;
	};

	class SubwindowKitProperty : public RootProperty
	{
	public:
		SubwindowKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSubwindow(kit);
			QTreeWidget * tree = ctrl->treeWidget();
			SubwindowKitSubwindowProperty *subwindowkitsubwindowproperty = new SubwindowKitSubwindowProperty(tree, kit);
			ctrl->addChild(subwindowkitsubwindowproperty);
			subwindowkitsubwindowproperty->addSubItems();

			SubwindowKitBackgroundProperty *subwindowkitbackgroundproperty = new SubwindowKitBackgroundProperty(tree, kit);
			ctrl->addChild(subwindowkitbackgroundproperty);
			subwindowkitbackgroundproperty->addSubItems();

			SubwindowKitBorderProperty *subwindowkitborderproperty = new SubwindowKitBorderProperty(tree, kit);
			ctrl->addChild(subwindowkitborderproperty);
			subwindowkitborderproperty->addSubItems();

			SubwindowKitRenderingAlgorithmProperty *subwindowkitrenderingalgorithmproperty = new SubwindowKitRenderingAlgorithmProperty(tree, kit);
			ctrl->addChild(subwindowkitrenderingalgorithmproperty);
			subwindowkitrenderingalgorithmproperty->addSubItems();

			SubwindowKitModelCompareModeProperty *subwindowkitmodelcomparemodeproperty = new SubwindowKitModelCompareModeProperty(tree, kit);
			ctrl->addChild(subwindowkitmodelcomparemodeproperty);
			subwindowkitmodelcomparemodeproperty->addSubItems();
		}

		void Apply() override
		{
			key.UnsetSubwindow();
			key.SetSubwindow(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::SubwindowKit kit;
	};


	class VisibilityKitCuttingSectionsProperty : public SettableProperty
	{
	public:
		VisibilityKitCuttingSectionsProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("CuttingSections")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCuttingSections(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetCuttingSections(_state);
		}

		void unset() override
		{
			kit.UnsetCuttingSections();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitCutEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitCutEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("CutEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCutEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetCutEdges(_state);
		}

		void unset() override
		{
			kit.UnsetCutEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitCutFacesProperty : public SettableProperty
	{
	public:
		VisibilityKitCutFacesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("CutFaces")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCutFaces(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetCutFaces(_state);
		}

		void unset() override
		{
			kit.UnsetCutFaces();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitWindowsProperty : public SettableProperty
	{
	public:
		VisibilityKitWindowsProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("Windows")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWindows(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetWindows(_state);
		}

		void unset() override
		{
			kit.UnsetWindows();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitTextProperty : public SettableProperty
	{
	public:
		VisibilityKitTextProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("Text")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowText(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetText(_state);
		}

		void unset() override
		{
			kit.UnsetText();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitLinesProperty : public SettableProperty
	{
	public:
		VisibilityKitLinesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("Lines")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLines(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetLines(_state);
		}

		void unset() override
		{
			kit.UnsetLines();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitEdgeLightsProperty : public SettableProperty
	{
	public:
		VisibilityKitEdgeLightsProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("EdgeLights")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEdgeLights(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetEdgeLights(_state);
		}

		void unset() override
		{
			kit.UnsetEdgeLights();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitMarkerLightsProperty : public SettableProperty
	{
	public:
		VisibilityKitMarkerLightsProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("MarkerLights")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMarkerLights(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetMarkerLights(_state);
		}

		void unset() override
		{
			kit.UnsetMarkerLights();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitFaceLightsProperty : public SettableProperty
	{
	public:
		VisibilityKitFaceLightsProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("FaceLights")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaceLights(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetFaceLights(_state);
		}

		void unset() override
		{
			kit.UnsetFaceLights();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitGenericEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitGenericEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("GenericEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowGenericEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetGenericEdges(_state);
		}

		void unset() override
		{
			kit.UnsetGenericEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitInteriorSilhouetteEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitInteriorSilhouetteEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("InteriorSilhouetteEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowInteriorSilhouetteEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetInteriorSilhouetteEdges(_state);
		}

		void unset() override
		{
			kit.UnsetInteriorSilhouetteEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitAdjacentEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitAdjacentEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("AdjacentEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAdjacentEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetAdjacentEdges(_state);
		}

		void unset() override
		{
			kit.UnsetAdjacentEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitHardEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitHardEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("HardEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowHardEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetHardEdges(_state);
		}

		void unset() override
		{
			kit.UnsetHardEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitMeshQuadEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitMeshQuadEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("MeshQuadEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMeshQuadEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetMeshQuadEdges(_state);
		}

		void unset() override
		{
			kit.UnsetMeshQuadEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitNonCulledEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitNonCulledEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("NonCulledEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowNonCulledEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetNonCulledEdges(_state);
		}

		void unset() override
		{
			kit.UnsetNonCulledEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitPerimeterEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitPerimeterEdgesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("PerimeterEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPerimeterEdges(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetPerimeterEdges(_state);
		}

		void unset() override
		{
			kit.UnsetPerimeterEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitFacesProperty : public SettableProperty
	{
	public:
		VisibilityKitFacesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("Faces")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaces(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetFaces(_state);
		}

		void unset() override
		{
			kit.UnsetFaces();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitVerticesProperty : public SettableProperty
	{
	public:
		VisibilityKitVerticesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("Vertices")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertices(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetVertices(_state);
		}

		void unset() override
		{
			kit.UnsetVertices();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitMarkersProperty : public SettableProperty
	{
	public:
		VisibilityKitMarkersProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("Markers")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMarkers(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetMarkers(_state);
		}

		void unset() override
		{
			kit.UnsetMarkers();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitShadowCastingProperty : public SettableProperty
	{
	public:
		VisibilityKitShadowCastingProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("ShadowCasting")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowShadowCasting(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetShadowCasting(_state);
		}

		void unset() override
		{
			kit.UnsetShadowCasting();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitShadowReceivingProperty : public SettableProperty
	{
	public:
		VisibilityKitShadowReceivingProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("ShadowReceiving")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowShadowReceiving(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetShadowReceiving(_state);
		}

		void unset() override
		{
			kit.UnsetShadowReceiving();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitShadowEmittingProperty : public SettableProperty
	{
	public:
		VisibilityKitShadowEmittingProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("ShadowEmitting")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowShadowEmitting(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetShadowEmitting(_state);
		}

		void unset() override
		{
			kit.UnsetShadowEmitting();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitLeaderLinesProperty : public SettableProperty
	{
	public:
		VisibilityKitLeaderLinesProperty(
			QTreeWidget * tree,
			HPS::VisibilityKit & kit)
			: SettableProperty("LeaderLines")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLeaderLines(_state);
			if (!_isSet)
			{
				_state = true;
			}
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetLeaderLines(_state);
		}

		void unset() override
		{
			kit.UnsetLeaderLines();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisibilityKitProperty : public RootProperty
	{
	public:
		VisibilityKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowVisibility(kit);
			QTreeWidget * tree = ctrl->treeWidget();

			VisibilityKitFacesProperty *visibilitykitfacesproperty = new VisibilityKitFacesProperty(tree, kit);
			ctrl->addChild(visibilitykitfacesproperty);
			visibilitykitfacesproperty->addSubItems();

			VisibilityKitLinesProperty *visibilitykitlinesproperty = new VisibilityKitLinesProperty(tree, kit);
			ctrl->addChild(visibilitykitlinesproperty);
			visibilitykitlinesproperty->addSubItems();

			VisibilityKitGenericEdgesProperty *visibilitykitgenericedgesproperty = new VisibilityKitGenericEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitgenericedgesproperty);
			visibilitykitgenericedgesproperty->addSubItems();

			VisibilityKitInteriorSilhouetteEdgesProperty *visibilitykitinteriorsilhouetteedgesproperty = new VisibilityKitInteriorSilhouetteEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitinteriorsilhouetteedgesproperty);
			visibilitykitinteriorsilhouetteedgesproperty->addSubItems();

			VisibilityKitAdjacentEdgesProperty *visibilitykitadjacentedgesproperty = new VisibilityKitAdjacentEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitadjacentedgesproperty);
			visibilitykitadjacentedgesproperty->addSubItems();

			VisibilityKitHardEdgesProperty *visibilitykithardedgesproperty = new VisibilityKitHardEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykithardedgesproperty);
			visibilitykithardedgesproperty->addSubItems();

			VisibilityKitMeshQuadEdgesProperty *visibilitykitmeshquadedgesproperty = new VisibilityKitMeshQuadEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitmeshquadedgesproperty);
			visibilitykitmeshquadedgesproperty->addSubItems();

			VisibilityKitNonCulledEdgesProperty *visibilitykitnoncullededgesproperty = new VisibilityKitNonCulledEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitnoncullededgesproperty);
			visibilitykitnoncullededgesproperty->addSubItems();

			VisibilityKitPerimeterEdgesProperty *visibilitykitperimeteredgesproperty = new VisibilityKitPerimeterEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitperimeteredgesproperty);
			visibilitykitperimeteredgesproperty->addSubItems();

			VisibilityKitTextProperty *visibilitykittextproperty = new VisibilityKitTextProperty(tree, kit);
			ctrl->addChild(visibilitykittextproperty);
			visibilitykittextproperty->addSubItems();

			VisibilityKitLeaderLinesProperty *visibilitykitleaderlinesproperty = new VisibilityKitLeaderLinesProperty(tree, kit);
			ctrl->addChild(visibilitykitleaderlinesproperty);
			visibilitykitleaderlinesproperty->addSubItems();

			VisibilityKitVerticesProperty *visibilitykitverticesproperty = new VisibilityKitVerticesProperty(tree, kit);
			ctrl->addChild(visibilitykitverticesproperty);
			visibilitykitverticesproperty->addSubItems();

			VisibilityKitMarkersProperty *visibilitykitmarkersproperty = new VisibilityKitMarkersProperty(tree, kit);
			ctrl->addChild(visibilitykitmarkersproperty);
			visibilitykitmarkersproperty->addSubItems();

			VisibilityKitEdgeLightsProperty *visibilitykitedgelightsproperty = new VisibilityKitEdgeLightsProperty(tree, kit);
			ctrl->addChild(visibilitykitedgelightsproperty);
			visibilitykitedgelightsproperty->addSubItems();

			VisibilityKitMarkerLightsProperty *visibilitykitmarkerlightsproperty = new VisibilityKitMarkerLightsProperty(tree, kit);
			ctrl->addChild(visibilitykitmarkerlightsproperty);
			visibilitykitmarkerlightsproperty->addSubItems();

			VisibilityKitFaceLightsProperty *visibilitykitfacelightsproperty = new VisibilityKitFaceLightsProperty(tree, kit);
			ctrl->addChild(visibilitykitfacelightsproperty);
			visibilitykitfacelightsproperty->addSubItems();

			VisibilityKitCuttingSectionsProperty *visibilitykitcuttingsectionsproperty = new VisibilityKitCuttingSectionsProperty(tree, kit);
			ctrl->addChild(visibilitykitcuttingsectionsproperty);
			visibilitykitcuttingsectionsproperty->addSubItems();

			VisibilityKitCutFacesProperty *visibilitykitcutfacesproperty = new VisibilityKitCutFacesProperty(tree, kit);
			ctrl->addChild(visibilitykitcutfacesproperty);
			visibilitykitcutfacesproperty->addSubItems();

			VisibilityKitCutEdgesProperty *visibilitykitcutedgesproperty = new VisibilityKitCutEdgesProperty(tree, kit);
			ctrl->addChild(visibilitykitcutedgesproperty);
			visibilitykitcutedgesproperty->addSubItems();

			VisibilityKitWindowsProperty *visibilitykitwindowsproperty = new VisibilityKitWindowsProperty(tree, kit);
			ctrl->addChild(visibilitykitwindowsproperty);
			visibilitykitwindowsproperty->addSubItems();

			VisibilityKitShadowCastingProperty *visibilitykitshadowcastingproperty = new VisibilityKitShadowCastingProperty(tree, kit);
			ctrl->addChild(visibilitykitshadowcastingproperty);
			visibilitykitshadowcastingproperty->addSubItems();

			VisibilityKitShadowReceivingProperty *visibilitykitshadowreceivingproperty = new VisibilityKitShadowReceivingProperty(tree, kit);
			ctrl->addChild(visibilitykitshadowreceivingproperty);
			visibilitykitshadowreceivingproperty->addSubItems();

			VisibilityKitShadowEmittingProperty *visibilitykitshadowemittingproperty = new VisibilityKitShadowEmittingProperty(tree, kit);
			ctrl->addChild(visibilitykitshadowemittingproperty);
			visibilitykitshadowemittingproperty->addSubItems();
		}

		void Apply() override
		{
			key.UnsetVisibility();
			key.SetVisibility(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::VisibilityKit kit;
	};

	class MaterialPaletteProperty : public SettableProperty
	{
	public:
		MaterialPaletteProperty(
			HPS::SegmentKey const & key)
			: SettableProperty("Material Palette")
			, key(key)
		{
		}

		void addSubItems()
		{
			bool _isSet = key.ShowMaterialPalette(palette);
			if (!_isSet)
				palette = "name";
			addChild(new UTF8Property("Name", palette));
			isSet(_isSet);
		}

		HPS::UTF8 GetPalette() const
		{
			return palette;
		}

	protected:
		void set() override
		{
			// nothing to do
		}

		void unset() override
		{
			// nothing to do
		}

	private:
		HPS::UTF8 palette;
		HPS::SegmentKey const & key;
	};

	class SegmentKeyMaterialPaletteProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			PalettePropertyIndex = 0,
		};

	public:
		SegmentKeyMaterialPaletteProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			MaterialPaletteProperty *materialpaletteproperty = new MaterialPaletteProperty(this->key);
			ctrl->addChild(materialpaletteproperty);
			materialpaletteproperty->addSubItems();
		}

		void Apply() override
		{
			auto paletteChild = static_cast<MaterialPaletteProperty *>(item->child(PalettePropertyIndex));
			if (paletteChild->isSet())
				key.SetMaterialPalette(paletteChild->GetPalette());
			else
				key.UnsetMaterialPalette();
		}

	private:
		HPS::SegmentKey key;
	};

	class ConditionProperty : public SettableArrayProperty
	{
	public:
		ConditionProperty(
			QTreeWidget * tree,
			HPS::SegmentKey const & key)
			: SettableArrayProperty("Conditions")
			, tree(tree)
			, key(key)
		{
		}

		void addSubItems()
		{
			bool _isSet = key.ShowConditions(conditions);
			if (_isSet)
				conditionCount = static_cast<unsigned int>(conditions.size());
			else
			{
				conditionCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty * array_size = new ArraySizeProperty("Count", conditionCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();
			isSet(_isSet);
		}

		HPS::UTF8Array GetConditions() const
		{
			return conditions;
		}

	protected:
		void set() override
		{
			if (conditionCount < 1)
				return;
			AddOrDeleteItems(conditionCount, static_cast<unsigned int>(conditions.size()));
		}

		void unset() override
		{
			// nothing to do
		}

		void ResizeArrays() override
		{
			conditions.resize(conditionCount, "condition");
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < conditionCount; ++i)
			{
				std::string name = "Condition " + std::to_string(i);
				auto newCondition = new UTF8Property(name.c_str(), conditions[i]);
				addChild(newCondition);
			}
		}

	private:
		HPS::UTF8Array conditions;
		unsigned int conditionCount;
		QTreeWidget * tree;
		HPS::SegmentKey const & key;
	};

	class SegmentKeyConditionProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			ConditionPropertyIndex = 0,
		};

	public:
		SegmentKeyConditionProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ConditionProperty *conditionproperty = new ConditionProperty(ctrl->treeWidget(), this->key);
			ctrl->addChild(conditionproperty);
			conditionproperty->addSubItems();
		}

		void Apply() override
		{
			auto conditionChild = static_cast<ConditionProperty *>(item->child(ConditionPropertyIndex));
			if (conditionChild->isSet())
				key.SetConditions(conditionChild->GetConditions());
			else
				key.UnsetConditions();
		}

	private:
		HPS::SegmentKey key;
	};

	class SingleAttributeLockProperty : public BaseProperty
	{
	public:
		SingleAttributeLockProperty(
			QTreeWidget * tree,
			unsigned int lock,
			HPS::AttributeLock::Type & type,
			bool & state)
			: BaseProperty("")
			, type(type)
			, state(state)
		{
			std::string name = "Lock " + std::to_string(lock);
			setText(0, name.c_str());

			AttributeLockTypeProperty * attributelocktypeproperty = new AttributeLockTypeProperty(tree, type);
			addChild(attributelocktypeproperty);
			attributelocktypeproperty->setupChoices();
			BoolProperty * bool_property = new BoolProperty(tree, "State", state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

	private:
		HPS::AttributeLock::Type & type;
		bool & state;
	};

	class AttributeLockKitLockProperty : public SettableArrayProperty
	{
	public:
		AttributeLockKitLockProperty(
			QTreeWidget * tree,
			HPS::AttributeLockKit & kit)
			: SettableArrayProperty("Locks")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			HPS::BoolArray statesArray;
			bool _isSet = this->kit.ShowLock(types, statesArray);

			if (_isSet)
			{
				lockCount = static_cast<unsigned int>(types.size());
				states.assign(statesArray.begin(), statesArray.end());
			}
			else
			{
				lockCount = 1;
				ResizeArrays();
			}

			ArraySizeProperty * array_size = new ArraySizeProperty("Count", lockCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (lockCount < 1)
				return;

			AddOrDeleteItems(lockCount, static_cast<unsigned int>(types.size()));

			HPS::BoolArray statesArray(states.begin(), states.end());
			kit.UnsetLock(HPS::AttributeLock::Type::Everything).SetLock(types, statesArray);
		}

		void unset() override
		{
			kit.UnsetLock(HPS::AttributeLock::Type::Everything);
		}

		void ResizeArrays() override
		{
			types.resize(lockCount, HPS::AttributeLock::Type::Everything);
			states.resize(lockCount, false);
		}

		void AddItems() override
		{
			for (unsigned int lock = 0; lock < lockCount; ++lock)
			{
				auto newLock = new SingleAttributeLockProperty(tree, lock, types[lock], states[lock]);
				addChild(newLock);
			}
		}

	private:
		HPS::AttributeLockKit & kit;
		unsigned int lockCount;
		HPS::AttributeLockTypeArray types;
		BoolDeque states;
		QTreeWidget * tree;
	};

	class AttributeLockKitSubsegmentLockOverrideProperty : public SettableArrayProperty
	{
	public:
		AttributeLockKitSubsegmentLockOverrideProperty(
			QTreeWidget * tree,
			HPS::AttributeLockKit & kit)
			: SettableArrayProperty("Subsegment Overrides")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			HPS::BoolArray statesArray;
			bool _isSet = this->kit.ShowSubsegmentLockOverride(types, statesArray);

			if (_isSet)
			{
				lockCount = static_cast<unsigned int>(types.size());
				states.assign(statesArray.begin(), statesArray.end());
			}
			else
			{
				lockCount = 1;
				ResizeArrays();
			}

			ArraySizeProperty * array_size = new ArraySizeProperty("Count", lockCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (lockCount < 1)
				return;

			AddOrDeleteItems(lockCount, static_cast<unsigned int>(types.size()));

			HPS::BoolArray statesArray(states.begin(), states.end());
			kit.UnsetSubsegmentLockOverride(HPS::AttributeLock::Type::Everything).SetSubsegmentLockOverride(types, statesArray);
		}

		void unset() override
		{
			kit.UnsetSubsegmentLockOverride(HPS::AttributeLock::Type::Everything);
		}

		void ResizeArrays() override
		{
			types.resize(lockCount, HPS::AttributeLock::Type::Everything);
			states.resize(lockCount, false);
		}

		void AddItems() override
		{
			for (unsigned int lock = 0; lock < lockCount; ++lock)
			{
				auto newLock = new SingleAttributeLockProperty(tree, lock, types[lock], states[lock]);
				addChild(newLock);
			}
		}

	private:
		HPS::AttributeLockKit & kit;
		unsigned int lockCount;
		HPS::AttributeLockTypeArray types;
		BoolDeque states;
		QTreeWidget * tree;
	};

	class AttributeLockKitProperty : public RootProperty
	{
	public:
		AttributeLockKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowAttributeLock(kit);
			QTreeWidget * tree = ctrl->treeWidget();

			AttributeLockKitLockProperty * attributelockkitlockproperty = new AttributeLockKitLockProperty(tree, kit);
			ctrl->addChild(attributelockkitlockproperty);
			attributelockkitlockproperty->addSubItems();

			AttributeLockKitSubsegmentLockOverrideProperty *attributelockkitsubsegmentlockoverrideproperty = new AttributeLockKitSubsegmentLockOverrideProperty(tree, kit);
			ctrl->addChild(attributelockkitsubsegmentlockoverrideproperty);
			attributelockkitsubsegmentlockoverrideproperty->addSubItems();
		}

		void Apply() override
		{
			key.UnsetAttributeLock();
			key.SetAttributeLock(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::AttributeLockKit kit;
	};

	class KeyPriorityProperty : public SettableProperty
	{
	public:
		KeyPriorityProperty(
			HPS::SegmentKey const & key)
			: SettableProperty("Priority")
			, key(key)
		{
		}

		void addSubItems()
		{
			bool _isSet = key.ShowPriority(priority);
			if (!_isSet)
				priority = 0;
			addChild(new IntProperty("Value", priority));
			isSet(_isSet);
		}

		int GetPriority() const
		{
			return priority;
		}

	protected:
		void set() override
		{
			// nothing to do
		}

		void unset() override
		{
			// nothing to do
		}

	private:
		int priority;
		HPS::SegmentKey const & key;
	};

	class SegmentKeyPriorityProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			PriorityPropertyIndex = 0,
		};

	public:
		SegmentKeyPriorityProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			KeyPriorityProperty *keypriorityproperty = new KeyPriorityProperty(this->key);
			ctrl->addChild(keypriorityproperty);
			keypriorityproperty->addSubItems();
		}

		void Apply() override
		{
			auto priorityChild = static_cast<KeyPriorityProperty *>(item->child(PriorityPropertyIndex));
			if (priorityChild->isSet())
				key.SetPriority(priorityChild->GetPriority());
			else
				key.UnsetPriority();
		}

	private:
		HPS::SegmentKey key;
	};

	class SelectionOptionsKitProximityProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitProximityProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("Proximity")
			, kit(kit)
		{
			this->kit.ShowProximity(_proximity);
			addChild(new FloatProperty("Proximity", _proximity));
		}

		void onChildChanged() override
		{
			kit.SetProximity(_proximity);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		float _proximity;
	};

	class SelectionOptionsKitLevelProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitLevelProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("Level")
			, kit(kit)
		{
			this->kit.ShowLevel(_level);
			SelectionLevelProperty *selectionlevelproperty = new SelectionLevelProperty(tree, _level);
			addChild(selectionlevelproperty);
			selectionlevelproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetLevel(_level);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Level _level;
	};

	class SelectionOptionsKitInternalLimitProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitInternalLimitProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("InternalLimit")
			, kit(kit)
		{
			size_t _limit_st;
			this->kit.ShowInternalLimit(_limit_st);
			_limit = static_cast<unsigned int>(_limit_st);
			addChild(new UnsignedIntProperty("Limit", _limit));
		}

		void onChildChanged() override
		{
			kit.SetInternalLimit(_limit);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		unsigned int _limit;
	};

	class SelectionOptionsKitRelatedLimitProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitRelatedLimitProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("RelatedLimit")
			, kit(kit)
		{
			size_t _limit_st;
			this->kit.ShowRelatedLimit(_limit_st);
			_limit = static_cast<unsigned int>(_limit_st);
			addChild(new UnsignedIntProperty("Limit", _limit));
		}

		void onChildChanged() override
		{
			kit.SetRelatedLimit(_limit);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		unsigned int _limit;
	};

	class SelectionOptionsKitSortingProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitSortingProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("Sorting")
			, kit(kit)
		{
			this->kit.ShowSorting(_sorting);
			SelectionSortingProperty *selectionsortingproperty = new SelectionSortingProperty(tree, _sorting);
			addChild(selectionsortingproperty);
			selectionsortingproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetSorting(_sorting);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Sorting _sorting;
	};

	class SelectionOptionsKitAlgorithmProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitAlgorithmProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("Algorithm")
			, kit(kit)
		{
			this->kit.ShowAlgorithm(_algorithm);
			SelectionAlgorithmProperty *selectionalgorithmproperty = new SelectionAlgorithmProperty(tree, _algorithm);
			addChild(selectionalgorithmproperty);
			selectionalgorithmproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetAlgorithm(_algorithm);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Algorithm _algorithm;
	};

	class SelectionOptionsKitGranularityProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitGranularityProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("Granularity")
			, kit(kit)
		{
			this->kit.ShowGranularity(_granularity);
			SelectionGranularityProperty *selectiongranularityproperty = new SelectionGranularityProperty(tree, _granularity);
			addChild(selectiongranularityproperty);
			selectiongranularityproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetGranularity(_granularity);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Granularity _granularity;
	};

	class SelectionOptionsKitExtentCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitExtentCullingRespectedProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("ExtentCullingRespected")
			, kit(kit)
		{
			this->kit.ShowExtentCullingRespected(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetExtentCullingRespected(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitDeferralExtentCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitDeferralExtentCullingRespectedProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("DeferralExtentCullingRespected")
			, kit(kit)
		{
			this->kit.ShowDeferralExtentCullingRespected(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetDeferralExtentCullingRespected(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitFrustumCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitFrustumCullingRespectedProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("FrustumCullingRespected")
			, kit(kit)
		{
			this->kit.ShowFrustumCullingRespected(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetFrustumCullingRespected(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitVectorCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitVectorCullingRespectedProperty(
			QTreeWidget * tree,
			HPS::SelectionOptionsKit & kit)
			: BaseProperty("VectorCullingRespected")
			, kit(kit)
		{
			this->kit.ShowVectorCullingRespected(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetVectorCullingRespected(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitProperty : public RootProperty
	{
	public:
		SelectionOptionsKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSelectionOptions(kit);
			QTreeWidget * tree = ctrl->treeWidget();
			ctrl->addChild(new SelectionOptionsKitProximityProperty(kit));
			ctrl->addChild(new SelectionOptionsKitLevelProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitInternalLimitProperty(kit));
			ctrl->addChild(new SelectionOptionsKitRelatedLimitProperty(kit));
			ctrl->addChild(new SelectionOptionsKitSortingProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitAlgorithmProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitGranularityProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitExtentCullingRespectedProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitDeferralExtentCullingRespectedProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitFrustumCullingRespectedProperty(tree, kit));
			ctrl->addChild(new SelectionOptionsKitVectorCullingRespectedProperty(tree, kit));
		}

		void Apply() override
		{
			key.SetSelectionOptions(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::SelectionOptionsKit kit;
	};

	template <typename Definition>
	class DefinitionNameProperty : public BaseProperty
	{
	public:
		DefinitionNameProperty(
			Definition const & def)
			: BaseProperty("Name")
		{
			addChild(new ImmutableUTF8Property("Value", def.Name()));
		}
	};

	class MaterialPaletteMaterialArrayProperty : public ArrayProperty
	{
	public:
		MaterialPaletteMaterialArrayProperty(
			QTreeWidget * tree,
			HPS::MaterialPaletteDefinition const & definition)
			: ArrayProperty("Materials")
			, tree(tree)
		{
			definition.Show(materials);
			materialCount = static_cast<unsigned int>(materials.size());
			ArraySizeProperty * array_size = new ArraySizeProperty("Count", materialCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();
		}

		void onChildChanged() override
		{
			AddOrDeleteItems(materialCount, static_cast<unsigned int>(materials.size()));
			ArrayProperty::onChildChanged();
		}

		HPS::MaterialKitArray GetMaterials() const
		{
			return materials;
		}

	protected:
		void ResizeArrays() override
		{
			materials.resize(materialCount);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < materialCount; ++i)
			{
				std::string name = "Material " + std::to_string(i);
				addChild(new MaterialProperty(tree, name.c_str(), materials[i]));
			}
		}

	private:
		unsigned int materialCount;
		HPS::MaterialKitArray materials;
		QTreeWidget * tree;
	};

	class MaterialPaletteDefinitionProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			MaterialArrayPropertyIndex = 0,
		};

	public:
		MaterialPaletteDefinitionProperty(
			QTreeWidgetItem * ctrl,
			HPS::MaterialPaletteDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			QTreeWidget * tree = ctrl->treeWidget();
			ctrl->addChild(new DefinitionNameProperty<HPS::MaterialPaletteDefinition>(this->definition));
			ctrl->addChild(new MaterialPaletteMaterialArrayProperty(tree, this->definition));
		}

		void Apply() override
		{
			auto definitionChild = static_cast<MaterialPaletteMaterialArrayProperty *>(item->child(MaterialArrayPropertyIndex));
			definition.Set(definitionChild->GetMaterials());
		}

	private:
		HPS::MaterialPaletteDefinition definition;
	};

	class TextureOptionsKitDecalProperty : public BaseProperty
	{
	public:
		TextureOptionsKitDecalProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("Decal")
			, kit(kit)
		{
			this->kit.ShowDecal(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetDecal(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		bool _state;
	};

	class TextureOptionsKitDownSamplingProperty : public BaseProperty
	{
	public:
		TextureOptionsKitDownSamplingProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("DownSampling")
			, kit(kit)
		{
			this->kit.ShowDownSampling(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetDownSampling(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		bool _state;
	};

	class TextureOptionsKitModulationProperty : public BaseProperty
	{
	public:
		TextureOptionsKitModulationProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("Modulation")
			, kit(kit)
		{
			this->kit.ShowModulation(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetModulation(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		bool _state;
	};

	class TextureOptionsKitParameterOffsetProperty : public BaseProperty
	{
	public:
		TextureOptionsKitParameterOffsetProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty("ParameterOffset")
			, kit(kit)
		{
			size_t _offset_st;
			this->kit.ShowParameterOffset(_offset_st);
			_offset = static_cast<unsigned int>(_offset_st);
			addChild(new UnsignedIntProperty("Offset", _offset));
		}

		void onChildChanged() override
		{
			kit.SetParameterOffset(_offset);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		unsigned int _offset;
	};

	class TextureOptionsKitParameterizationSourceProperty : public BaseProperty
	{
	public:
		TextureOptionsKitParameterizationSourceProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("ParameterizationSource")
			, kit(kit)
		{
			this->kit.ShowParameterizationSource(_source);
			MaterialTextureParameterizationProperty *materialtextureparameterizationproperty = new MaterialTextureParameterizationProperty(tree, _source);
			addChild(materialtextureparameterizationproperty);
			materialtextureparameterizationproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetParameterizationSource(_source);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Parameterization _source;
	};

	class TextureOptionsKitTilingProperty : public BaseProperty
	{
	public:
		TextureOptionsKitTilingProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("Tiling")
			, kit(kit)
		{
			this->kit.ShowTiling(_tiling);
			MaterialTextureTilingProperty *materialtexturetilingproperty = new MaterialTextureTilingProperty(tree, _tiling);
			addChild(materialtexturetilingproperty);
			materialtexturetilingproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetTiling(_tiling);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Tiling _tiling;
	};

	class TextureOptionsKitInterpolationFilterProperty : public BaseProperty
	{
	public:
		TextureOptionsKitInterpolationFilterProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("InterpolationFilter")
			, kit(kit)
		{
			this->kit.ShowInterpolationFilter(_filter);
			MaterialTextureInterpolationProperty *materialtextureinterpolationproperty = new MaterialTextureInterpolationProperty(tree, _filter);
			addChild(materialtextureinterpolationproperty);
			materialtextureinterpolationproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetInterpolationFilter(_filter);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Interpolation _filter;
	};

	class TextureOptionsKitDecimationFilterProperty : public BaseProperty
	{
	public:
		TextureOptionsKitDecimationFilterProperty(
			QTreeWidget * tree,
			HPS::TextureOptionsKit & kit)
			: BaseProperty("DecimationFilter")
			, kit(kit)
		{
			this->kit.ShowDecimationFilter(_filter);
			MaterialTextureDecimationProperty *materialtexturedecimationproperty = new MaterialTextureDecimationProperty(tree, _filter);
			addChild(materialtexturedecimationproperty);
			materialtexturedecimationproperty->setupChoices();
		}

		void onChildChanged() override
		{
			kit.SetDecimationFilter(_filter);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Decimation _filter;
	};

	template <
		typename Kit,
		bool (Kit::*ShowMatrix)(HPS::MatrixKit &) const,
		Kit & (Kit::*SetMatrix)(HPS::MatrixKit const &),
		Kit & (Kit::*UnsetMatrix)()
	>
	class MatrixKitProperty : public SettableProperty
	{
	public:
		MatrixKitProperty(
			const char * name,
			Kit & kit)
			: SettableProperty(name)
			, kit(kit)
		{
		}

		void addSubItems()
		{
			bool _isSet = (this->kit.*ShowMatrix)(matrix);
			matrix.ShowElements(elements);
			for (size_t i = 0; i < elements.size(); ++i)
			{
				auto ithName = std::to_string(i);
				addChild(new FloatProperty(ithName.c_str(), elements[i]));
			}
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			matrix.SetElements(elements);
			(kit.*SetMatrix)(matrix);
		}

		void unset() override
		{
			(kit.*UnsetMatrix)();
		}

	private:
		Kit & kit;
		HPS::MatrixKit matrix;
		HPS::FloatArray elements;
	};

	typedef MatrixKitProperty <
		HPS::TextureOptionsKit,
		&HPS::TextureOptionsKit::ShowTransformMatrix,
		&HPS::TextureOptionsKit::SetTransformMatrix,
		&HPS::TextureOptionsKit::UnsetTransformMatrix
	> BaseTextureOptionsKitTransformMatrixProperty;
	class TextureOptionsKitTransformMatrixProperty : public BaseTextureOptionsKitTransformMatrixProperty
	{
	public:
		TextureOptionsKitTransformMatrixProperty(
			HPS::TextureOptionsKit & kit)
			: BaseTextureOptionsKitTransformMatrixProperty("TransformMatrix", kit)
		{}
	};

	typedef MatrixKitProperty <
		HPS::TextKit,
		&HPS::TextKit::ShowModellingMatrix,
		&HPS::TextKit::SetModellingMatrix,
		&HPS::TextKit::UnsetModellingMatrix
	> BaseTextKitModellingMatrixProperty;
	class TextKitModellingMatrixProperty : public BaseTextKitModellingMatrixProperty
	{
	public:
		TextKitModellingMatrixProperty(
			QTreeWidget * tree,
			HPS::TextKit & kit)
			: BaseTextKitModellingMatrixProperty("ModellingMatrix", kit)
		{
			(void)tree;
		}
	};

	class SingleTextMarginProperty : public BaseProperty
	{
	public:
		SingleTextMarginProperty(
			QTreeWidget * tree,
			unsigned int margin,
			float & size,
			HPS::Text::MarginUnits & units)
			: BaseProperty("")
			, size(size)
			, units(units)
		{
			std::string name = "Margin " + std::to_string(margin);
			setText(0, name.c_str());
			addChild(new FloatProperty("Size", size));
			TextMarginUnitsProperty *textmarginunitsproperty = new TextMarginUnitsProperty(tree, units);
			addChild(textmarginunitsproperty);
			textmarginunitsproperty->setupChoices();
		}

	private:
		float & size;
		HPS::Text::MarginUnits & units;
	};

	template <typename Kit>
	class BackgroundMarginsProperty : public SettableArrayProperty
	{
	public:
		BackgroundMarginsProperty(
			QTreeWidget * tree,
			Kit & kit)
			: SettableArrayProperty("BackgroundMargins")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackgroundMargins(_sizes, _units);
			if (_isSet)
				marginCount = static_cast<unsigned int>(_sizes.size());
			else
			{
				marginCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty *arraysizeproperty = new ArraySizeProperty("Count", marginCount);
			addChild(arraysizeproperty);
			arraysizeproperty->setupSpinBox(tree);

			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (marginCount < 1)
				return;
			AddOrDeleteItems(marginCount, static_cast<unsigned int>(_sizes.size()));
			kit.SetBackgroundMargins(_sizes, _units);
		}

		void unset() override
		{
			kit.UnsetBackgroundMargins();
		}

		void ResizeArrays() override
		{
			_sizes.resize(marginCount, 0.0f);
			_units.resize(marginCount, HPS::Text::MarginUnits::Percent);
		}

		void AddItems() override
		{
			for (unsigned int margin = 0; margin < marginCount; ++margin)
				addChild(new SingleTextMarginProperty(tree, margin, _sizes[margin], _units[margin]));
		}

	private:
		Kit & kit;
		unsigned int marginCount;
		HPS::FloatArray _sizes;
		HPS::TextMarginUnitsArray _units;
		QTreeWidget * tree;
	};

	typedef BackgroundMarginsProperty<HPS::TextKit> TextKitBackgroundMarginsProperty;
	typedef BackgroundMarginsProperty<HPS::TextAttributeKit> TextAttributeKitBackgroundMarginsProperty;

	class TextKitLeaderLinesProperty : public SettableArrayProperty
	{
	private:
		enum PropertyTypeIndex
		{
			SpacePropertyIndex = 0,
			CountPropertyIndex,
			FirstItemIndex,
		};

	public:
		TextKitLeaderLinesProperty(
			QTreeWidget * tree,
			HPS::TextKit & kit)
			: SettableArrayProperty("LeaderLines", FirstItemIndex)
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLeaderLines(_positions, _space);
			if (_isSet)
				_position_count = static_cast<unsigned int>(_positions.size());
			else
			{
				_position_count = 1;
				ResizeArrays();
				_space = HPS::Text::LeaderLineSpace::Object;
			}

			TextLeaderLineSpaceProperty *textleaderlinespaceproperty = new TextLeaderLineSpaceProperty(tree, _space);
			addChild(textleaderlinespaceproperty);
			textleaderlinespaceproperty->setupChoices();

			ArraySizeProperty *arraysizeproperty = new ArraySizeProperty("Count", _position_count);
			addChild(arraysizeproperty);
			arraysizeproperty->setupSpinBox(tree);

			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (_position_count < 1)
				return;
			AddOrDeleteItems(_position_count, static_cast<unsigned int>(_positions.size()));
			kit.SetLeaderLines(_positions, _space);
		}

		void unset() override
		{
			kit.UnsetLeaderLines();
		}

		void ResizeArrays() override
		{
			_positions.resize(_position_count, HPS::Point::Origin());
		}

		void AddItems() override
		{
			for (unsigned int position = 0; position < _position_count; ++position)
			{
				std::string itemName = "Position " + std::to_string(position);
				addChild(new PointProperty(itemName.c_str(), _positions[position]));
			}
		}

	private:
		HPS::TextKit & kit;
		unsigned int _position_count;
		HPS::PointArray _positions;
		HPS::Text::LeaderLineSpace _space;
		QTreeWidget * tree;
	};

	enum class RegionPointCount
	{
		Two = 2,
		Three = 3,
	};

	class RegionPointCountProperty : public BaseEnumProperty<RegionPointCount>
	{
	private:
		enum PropertyTypeIndex
		{
			CountPropertyIndex = 0,
			Point0PropertyIndex,
			Point1PropertyIndex,
			Point2PropertyIndex,
		};

	public:
		RegionPointCountProperty(
			QTreeWidget * tree,
			RegionPointCount & count)
			: BaseEnumProperty("Count", count)
			, tree(tree)
		{
		}

		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = RegionPointCount::Two; enumStrings[0] = "2";
			enumValues[1] = RegionPointCount::Three; enumStrings[1] = "3";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto point2Sibling = static_cast<BaseProperty *>(parent()->child(Point2PropertyIndex));
			if (enumValue == RegionPointCount::Two)
				point2Sibling->setFlags(point2Sibling->flags() & ~Qt::ItemFlag::ItemIsEditable);
			else if (enumValue == RegionPointCount::Three)
				point2Sibling->setFlags(point2Sibling->flags() | Qt::ItemFlag::ItemIsEditable);
		}

	private:
		QTreeWidget * tree;
	};

	class TextKitRegionProperty : public SettableProperty
	{
	public:
		TextKitRegionProperty(
			QTreeWidget * tree,
			HPS::TextKit & kit)
			: SettableProperty("Region")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRegion(_region_points, _region_alignment, _region_fitting, _region_adjust_direction, _region_relative_coordinates, _region_window_space);
			if (_isSet)
			{
				_region_point_count = static_cast<RegionPointCount>(_region_points.size());
				if (_region_point_count == RegionPointCount::Two)
					_region_points.resize(3, HPS::Point::Origin());
			}
			else
			{
				_region_point_count = RegionPointCount::Three;
				_region_points.resize(3, HPS::Point::Origin());
				_region_points[1] = HPS::Point(1, 0, 0);
				_region_points[2] = HPS::Point(0, 1, 0);
				_region_alignment = HPS::Text::RegionAlignment::Bottom;
				_region_fitting = HPS::Text::RegionFitting::Left;
				_region_adjust_direction = true;
				_region_relative_coordinates = true;
				_region_window_space = false;
			}
			auto countChild = new RegionPointCountProperty(tree, _region_point_count);
			addChild(countChild);
			countChild->setupChoices();
			addChild(new PointProperty("Region Point 0", _region_points[0]));
			addChild(new PointProperty("Region Point 1", _region_points[1]));
			addChild(new PointProperty("Region Point 2", _region_points[2]));

			TextRegionAlignmentProperty *textregionalignmentproperty = new TextRegionAlignmentProperty(tree, _region_alignment);
			addChild(textregionalignmentproperty);
			textregionalignmentproperty->setupChoices();

			TextRegionFittingProperty *textregionfittingproperty = new TextRegionFittingProperty(tree, _region_fitting);
			addChild(textregionfittingproperty);
			textregionfittingproperty->setupChoices();

			{
				BoolProperty * boolproperty = new BoolProperty(tree, "Region Adjust Direction", _region_adjust_direction);
				addChild(boolproperty);
				boolproperty->setupComboBox();
			}

			{
				BoolProperty * boolproperty = new BoolProperty(tree, "Region Relative Coordinates", _region_relative_coordinates);
				addChild(boolproperty);
				boolproperty->setupComboBox();
			}

			{
				BoolProperty * boolproperty = new BoolProperty(tree, "Region Window Space", _region_window_space);
				addChild(boolproperty);
				boolproperty->setupComboBox();
			}

			countChild->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			// Use the overload that takes a count in case the number of region points is less than the size of the region point array.
			auto count = static_cast<size_t>(_region_point_count);
			kit.SetRegion(count, _region_points.data(), _region_alignment, _region_fitting, _region_adjust_direction, _region_relative_coordinates, _region_window_space);
		}

		void unset() override
		{
			kit.UnsetRegion();
		}

	private:
		HPS::TextKit & kit;
		RegionPointCount _region_point_count;
		HPS::PointArray _region_points;
		HPS::Text::RegionAlignment _region_alignment;
		HPS::Text::RegionFitting _region_fitting;
		bool _region_adjust_direction;
		bool _region_relative_coordinates;
		bool _region_window_space;
		QTreeWidget * tree;
	};

	class TextKitPositionProperty : public BaseProperty
	{
	public:
		TextKitPositionProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: BaseProperty("Position")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowPosition(_position);
			addChild(new PointProperty("Position", _position));
		}
		void onChildChanged() override
		{
			kit.SetPosition(_position);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextKit& kit;
		HPS::Point _position;
		QTreeWidget* tree;
	};

	class TextKitTextProperty : public BaseProperty
	{
	public:
		TextKitTextProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: BaseProperty("Text")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowText(_string);
			addChild(new UTF8Property("String", _string));
		}
		void onChildChanged() override
		{
			kit.SetText(_string);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::TextKit& kit;
		HPS::UTF8 _string;
		QTreeWidget* tree;
	};

	class TextKitAlignmentProperty : public SettableProperty
	{
	public:
		TextKitAlignmentProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Alignment")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAlignment(_alignment, _reference_frame, _justification);
			if (!_isSet)
			{
				_alignment = HPS::Text::Alignment::BottomLeft;
				_reference_frame = HPS::Text::ReferenceFrame::WorldAligned;
				_justification = HPS::Text::Justification::Left;
			}
			TextAlignmentProperty* enumObject_0 = new TextAlignmentProperty(tree, _alignment);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			TextReferenceFrameProperty* enumObject_1 = new TextReferenceFrameProperty(tree, _reference_frame);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			TextJustificationProperty* enumObject_2 = new TextJustificationProperty(tree, _justification);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetAlignment(_alignment, _reference_frame, _justification);
		}

		void unset() override
		{
			kit.UnsetAlignment();
		}

	private:
		HPS::TextKit& kit;
		HPS::Text::Alignment _alignment;
		HPS::Text::ReferenceFrame _reference_frame;
		HPS::Text::Justification _justification;
		QTreeWidget* tree;
	};

	class TextKitBoldProperty : public SettableProperty
	{
	public:
		TextKitBoldProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Bold")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBold(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBold(_state);
		}

		void unset() override
		{
			kit.UnsetBold();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		QTreeWidget* tree;
	};

	class TextKitItalicProperty : public SettableProperty
	{
	public:
		TextKitItalicProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Italic")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowItalic(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetItalic(_state);
		}

		void unset() override
		{
			kit.UnsetItalic();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		QTreeWidget* tree;
	};

	class TextKitOverlineProperty : public SettableProperty
	{
	public:
		TextKitOverlineProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Overline")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOverline(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOverline(_state);
		}

		void unset() override
		{
			kit.UnsetOverline();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		QTreeWidget* tree;
	};

	class TextKitStrikethroughProperty : public SettableProperty
	{
	public:
		TextKitStrikethroughProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Strikethrough")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowStrikethrough(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetStrikethrough(_state);
		}

		void unset() override
		{
			kit.UnsetStrikethrough();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		QTreeWidget* tree;
	};

	class TextKitUnderlineProperty : public SettableProperty
	{
	public:
		TextKitUnderlineProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Underline")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowUnderline(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetUnderline(_state);
		}

		void unset() override
		{
			kit.UnsetUnderline();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		QTreeWidget* tree;
	};

	class TextKitSlantProperty : public SettableProperty
	{
	public:
		TextKitSlantProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Slant")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSlant(_angle);
			if (!_isSet)
			{
				_angle = 0.0f;
			}
			addChild(new FloatProperty("Angle", _angle));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSlant(_angle);
		}

		void unset() override
		{
			kit.UnsetSlant();
		}

	private:
		HPS::TextKit& kit;
		float _angle;
		QTreeWidget* tree;
	};

	class TextKitLineSpacingProperty : public SettableProperty
	{
	public:
		TextKitLineSpacingProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("LineSpacing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLineSpacing(_multiplier);
			if (!_isSet)
			{
				_multiplier = 0.0f;
			}
			addChild(new FloatProperty("Multiplier", _multiplier));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLineSpacing(_multiplier);
		}

		void unset() override
		{
			kit.UnsetLineSpacing();
		}

	private:
		HPS::TextKit& kit;
		float _multiplier;
		QTreeWidget* tree;
	};

	class TextKitExtraSpaceProperty : public SettableProperty
	{
	public:
		TextKitExtraSpaceProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("ExtraSpace")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowExtraSpace(_state, _size, _units);
			if (!_isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Size", _size));
			TextSizeUnitsProperty* enumObject_2 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetExtraSpace(_state, _size, _units);
		}

		void unset() override
		{
			kit.UnsetExtraSpace();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		float _size;
		HPS::Text::SizeUnits _units;
		QTreeWidget* tree;
	};

	class TextKitGreekingProperty : public SettableProperty
	{
	public:
		TextKitGreekingProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Greeking")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowGreeking(_state, _size, _units, _mode);
			if (!_isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::GreekingUnits::Pixels;
				_mode = HPS::Text::GreekingMode::Lines;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Size", _size));
			TextGreekingUnitsProperty* enumObject_2 = new TextGreekingUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			TextGreekingModeProperty* enumObject_3 = new TextGreekingModeProperty(tree, _mode);
			addChild(enumObject_3);
			enumObject_3->setupChoices();
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetGreeking(_state, _size, _units, _mode);
		}

		void unset() override
		{
			kit.UnsetGreeking();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		float _size;
		HPS::Text::GreekingUnits _units;
		HPS::Text::GreekingMode _mode;
		QTreeWidget* tree;
	};

	class TextKitSizeToleranceProperty : public SettableProperty
	{
	public:
		TextKitSizeToleranceProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("SizeTolerance")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSizeTolerance(_state, _size, _units);
			if (!_isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeToleranceUnits::Percent;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Size", _size));
			TextSizeToleranceUnitsProperty* enumObject_2 = new TextSizeToleranceUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSizeTolerance(_state, _size, _units);
		}

		void unset() override
		{
			kit.UnsetSizeTolerance();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		float _size;
		HPS::Text::SizeToleranceUnits _units;
		QTreeWidget* tree;
	};

	class TextKitSizeProperty : public SettableProperty
	{
	public:
		TextKitSizeProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Size")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSize(_size, _units);
			if (!_isSet)
			{
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			addChild(new FloatProperty("Size", _size));
			TextSizeUnitsProperty* enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSize(_size, _units);
		}

		void unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::TextKit& kit;
		float _size;
		HPS::Text::SizeUnits _units;
		QTreeWidget* tree;
	};

	class TextKitFontProperty : public SettableProperty
	{
	public:
		TextKitFontProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Font")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFont(_name);
			if (!_isSet)
			{
				_name = "name";
			}
			addChild(new UTF8Property("Name", _name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFont(_name);
		}

		void unset() override
		{
			kit.UnsetFont();
		}

	private:
		HPS::TextKit& kit;
		HPS::UTF8 _name;
		QTreeWidget* tree;
	};

	class TextKitTransformProperty : public SettableProperty
	{
	public:
		TextKitTransformProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Transform")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowTransform(_trans);
			if (!_isSet)
			{
				_trans = HPS::Text::Transform::Transformable;
			}
			TextTransformProperty* enumObject_0 = new TextTransformProperty(tree, _trans);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTransform(_trans);
		}

		void unset() override
		{
			kit.UnsetTransform();
		}

	private:
		HPS::TextKit& kit;
		HPS::Text::Transform _trans;
		QTreeWidget* tree;
	};

	class TextKitRendererProperty : public SettableProperty
	{
	public:
		TextKitRendererProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Renderer")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRenderer(_renderer);
			if (!_isSet)
			{
				_renderer = HPS::Text::Renderer::Default;
			}
			TextRendererProperty* enumObject_0 = new TextRendererProperty(tree, _renderer);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRenderer(_renderer);
		}

		void unset() override
		{
			kit.UnsetRenderer();
		}

	private:
		HPS::TextKit& kit;
		HPS::Text::Renderer _renderer;
		QTreeWidget* tree;
	};

	class TextKitPreferenceProperty : public SettableProperty
	{
	public:
		TextKitPreferenceProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Preference")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPreference(_cutoff, _units, _smaller, _larger);
			if (!_isSet)
			{
				_cutoff = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
				_smaller = HPS::Text::Preference::Default;
				_larger = HPS::Text::Preference::Default;
			}
			addChild(new FloatProperty("Cutoff", _cutoff));
			TextSizeUnitsProperty* enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			TextPreferenceProperty* enumObject_2 = new TextPreferenceProperty(tree, _smaller);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			TextPreferenceProperty* enumObject_3 = new TextPreferenceProperty(tree, _larger);
			addChild(enumObject_3);
			enumObject_3->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPreference(_cutoff, _units, _smaller, _larger);
		}

		void unset() override
		{
			kit.UnsetPreference();
		}

	private:
		HPS::TextKit& kit;
		float _cutoff;
		HPS::Text::SizeUnits _units;
		HPS::Text::Preference _smaller;
		HPS::Text::Preference _larger;
		QTreeWidget* tree;
	};

	class TextKitPathProperty : public SettableProperty
	{
	public:
		TextKitPathProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Path")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPath(_path);
			if (!_isSet)
			{
				_path = HPS::Vector::Unit();
			}
			addChild(new VectorProperty("Path", _path));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPath(_path);
		}

		void unset() override
		{
			kit.UnsetPath();
		}

	private:
		HPS::TextKit& kit;
		HPS::Vector _path;
		QTreeWidget* tree;
	};

	class TextKitSpacingProperty : public SettableProperty
	{
	public:
		TextKitSpacingProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Spacing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSpacing(_multiplier);
			if (!_isSet)
			{
				_multiplier = 0.0f;
			}
			addChild(new FloatProperty("Multiplier", _multiplier));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSpacing(_multiplier);
		}

		void unset() override
		{
			kit.UnsetSpacing();
		}

	private:
		HPS::TextKit& kit;
		float _multiplier;
		QTreeWidget* tree;
	};

	class TextKitBackgroundProperty : public SettableProperty
	{
	public:
		TextKitBackgroundProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("Background")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackground(_state, _name);
			if (!_isSet)
			{
				_state = true;
				_name = "name";
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UTF8Property("Name", _name));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBackground(_state, _name);
		}

		void unset() override
		{
			kit.UnsetBackground();
		}

	private:
		HPS::TextKit& kit;
		bool _state;
		HPS::UTF8 _name;
		QTreeWidget* tree;
	};

	class TextKitBackgroundStyleProperty : public SettableProperty
	{
	public:
		TextKitBackgroundStyleProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("BackgroundStyle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackgroundStyle(_name);
			if (!_isSet)
			{
				_name = "name";
			}
			addChild(new UTF8Property("Name", _name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBackgroundStyle(_name);
		}

		void unset() override
		{
			kit.UnsetBackgroundStyle();
		}

	private:
		HPS::TextKit& kit;
		HPS::UTF8 _name;
		QTreeWidget* tree;
	};

	class TextKitCharacterAttributesProperty : public SettableProperty
	{
	public:
		TextKitCharacterAttributesProperty(
			QTreeWidget* tree,
			HPS::TextKit& kit)
			: SettableProperty("CharacterAttributes")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCharacterAttributes(_attributes);
			addChild(new ImmutableSizeTProperty("Count", _attributes.size()));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCharacterAttributes(_attributes);
		}

		void unset() override
		{
			kit.UnsetCharacterAttributes();
		}

	private:
		HPS::TextKit& kit;
		HPS::CharacterAttributeKitArray _attributes;
		QTreeWidget* tree;
	};

	class TextKitProperty : public RootProperty
	{
	public:
		TextKitProperty(
			QTreeWidgetItem* ctrl,
			HPS::TextKey const& key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Position = new TextKitPositionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Position);
			prop_Position->addSubItems();
			auto prop_Text = new TextKitTextProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Text);
			prop_Text->addSubItems();
			auto prop_Color = new TextKitColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Color);
			prop_Color->addSubItems();
			auto prop_ModellingMatrix = new TextKitModellingMatrixProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ModellingMatrix);
			prop_ModellingMatrix->addSubItems();
			auto prop_Alignment = new TextKitAlignmentProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Alignment);
			prop_Alignment->addSubItems();
			auto prop_Bold = new TextKitBoldProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Bold);
			prop_Bold->addSubItems();
			auto prop_Italic = new TextKitItalicProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Italic);
			prop_Italic->addSubItems();
			auto prop_Overline = new TextKitOverlineProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Overline);
			prop_Overline->addSubItems();
			auto prop_Strikethrough = new TextKitStrikethroughProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Strikethrough);
			prop_Strikethrough->addSubItems();
			auto prop_Underline = new TextKitUnderlineProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Underline);
			prop_Underline->addSubItems();
			auto prop_Slant = new TextKitSlantProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Slant);
			prop_Slant->addSubItems();
			auto prop_LineSpacing = new TextKitLineSpacingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_LineSpacing);
			prop_LineSpacing->addSubItems();
			auto prop_Rotation = new TextKitRotationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Rotation);
			prop_Rotation->addSubItems();
			auto prop_ExtraSpace = new TextKitExtraSpaceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ExtraSpace);
			prop_ExtraSpace->addSubItems();
			auto prop_Greeking = new TextKitGreekingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Greeking);
			prop_Greeking->addSubItems();
			auto prop_SizeTolerance = new TextKitSizeToleranceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SizeTolerance);
			prop_SizeTolerance->addSubItems();
			auto prop_Size = new TextKitSizeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Size);
			prop_Size->addSubItems();
			auto prop_Font = new TextKitFontProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Font);
			prop_Font->addSubItems();
			auto prop_Transform = new TextKitTransformProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Transform);
			prop_Transform->addSubItems();
			auto prop_Renderer = new TextKitRendererProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Renderer);
			prop_Renderer->addSubItems();
			auto prop_Preference = new TextKitPreferenceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Preference);
			prop_Preference->addSubItems();
			auto prop_Path = new TextKitPathProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Path);
			prop_Path->addSubItems();
			auto prop_Spacing = new TextKitSpacingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Spacing);
			prop_Spacing->addSubItems();
			auto prop_Background = new TextKitBackgroundProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Background);
			prop_Background->addSubItems();
			auto prop_BackgroundMargins = new TextKitBackgroundMarginsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BackgroundMargins);
			prop_BackgroundMargins->addSubItems();
			auto prop_BackgroundStyle = new TextKitBackgroundStyleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BackgroundStyle);
			prop_BackgroundStyle->addSubItems();
			auto prop_LeaderLines = new TextKitLeaderLinesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_LeaderLines);
			prop_LeaderLines->addSubItems();
			auto prop_Region = new TextKitRegionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Region);
			prop_Region->addSubItems();
			auto prop_CharacterAttributes = new TextKitCharacterAttributesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CharacterAttributes);
			prop_CharacterAttributes->addSubItems();
			ctrl->addChild(new TextKitPriorityProperty(kit));
			ctrl->addChild(new TextKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::TextKey key;
		HPS::TextKit kit;
	};

	template <typename DefinitionType>
	class TextureOrCubemapProperty : public RootProperty
	{
	public:
		TextureOrCubemapProperty(
			QTreeWidgetItem * ctrl,
			DefinitionType const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.ShowOptions(kit);
			QTreeWidget * tree = ctrl->treeWidget();
			ctrl->addChild(new DefinitionNameProperty<DefinitionType>(this->definition));
			ctrl->addChild(new TextureOptionsKitDecalProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitDownSamplingProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitModulationProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitParameterOffsetProperty(kit));
			ctrl->addChild(new TextureOptionsKitParameterizationSourceProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitTilingProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitInterpolationFilterProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitDecimationFilterProperty(tree, kit));
			ctrl->addChild(new TextureOptionsKitTransformMatrixProperty(kit));
		}

		void Apply() override
		{
			definition.SetOptions(kit);
		}

	private:
		DefinitionType definition;
		HPS::TextureOptionsKit kit;
	};

	typedef TextureOrCubemapProperty<HPS::TextureDefinition> TextureDefinitionProperty;
	typedef TextureOrCubemapProperty<HPS::CubeMapDefinition> CubeMapDefinitionProperty;

	class ImageKitSizeProperty : public BaseProperty
	{
	public:
		ImageKitSizeProperty(
			HPS::ImageKit const & kit)
			: BaseProperty("Size")
		{
			unsigned int width;
			unsigned int height;
			kit.ShowSize(width, height);
			addChild(new ImmutableUnsignedIntProperty("Width", width));
			addChild(new ImmutableUnsignedIntProperty("Height", height));
		}
	};

	typedef ImmutableArraySizeProperty <
		HPS::ImageKit,
		HPS::byte,
		&HPS::ImageKit::ShowData
	> BaseImageKitDataProperty;
	class ImageKitDataProperty : public BaseImageKitDataProperty
	{
	public:
		ImageKitDataProperty(
			HPS::ImageKit const & kit)
			: BaseImageKitDataProperty("Data", "Byte Count", kit)
		{}
	};

	class ImageKitFormatProperty : public BaseProperty
	{
	public:
		ImageKitFormatProperty(
			HPS::ImageKit const & kit)
			: BaseProperty("Format")
		{
			HPS::Image::Format format;
			kit.ShowFormat(format);
			HPS::UTF8 formatString;
			switch (format)
			{
			case HPS::Image::Format::RGB: formatString = "RGB"; break;
			case HPS::Image::Format::RGBA: formatString = "RGBA"; break;
			case HPS::Image::Format::ARGB: formatString = "ARGB"; break;
			case HPS::Image::Format::Mapped8: formatString = "Mapped8"; break;
			case HPS::Image::Format::Grayscale: formatString = "Grayscale"; break;
			case HPS::Image::Format::Bmp: formatString = "Bmp"; break;
			case HPS::Image::Format::Jpeg: formatString = "Jpeg"; break;
			case HPS::Image::Format::Png: formatString = "Png"; break;
			case HPS::Image::Format::Targa: formatString = "Targa"; break;
			case HPS::Image::Format::DXT1: formatString = "DXT1"; break;
			case HPS::Image::Format::DXT3: formatString = "DXT3"; break;
			case HPS::Image::Format::DXT5: formatString = "DXT5"; break;
			default: Q_ASSERT(0);
			}
			addChild(new ImmutableUTF8Property("Format", formatString));
		}
	};

	class ImageKitDownSamplingProperty : public BaseProperty
	{
	public:
		ImageKitDownSamplingProperty(
			QTreeWidget * tree,
			HPS::ImageKit & kit)
			: BaseProperty("DownSampling")
			, kit(kit)
		{
			this->kit.ShowDownSampling(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetDownSampling(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::ImageKit & kit;
		bool _state;
	};

	class ImageKitCompressionQualityProperty : public BaseProperty
	{
	public:
		ImageKitCompressionQualityProperty(
			HPS::ImageKit & kit)
			: BaseProperty("CompressionQuality")
			, kit(kit)
		{
			this->kit.ShowCompressionQuality(_quality);
			addChild(new UnitFloatProperty("Quality", _quality));
		}

		void onChildChanged() override
		{
			kit.SetCompressionQuality(_quality);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::ImageKit & kit;
		float _quality;
	};

	class ImageDefinitionProperty : public RootProperty
	{
	public:
		ImageDefinitionProperty(
			QTreeWidgetItem * ctrl,
			HPS::ImageDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			QTreeWidget * tree = ctrl->treeWidget();
			ctrl->addChild(new DefinitionNameProperty<HPS::ImageDefinition>(this->definition));
			ctrl->addChild(new ImageKitSizeProperty(kit));
			ctrl->addChild(new ImageKitDataProperty(kit));
			ctrl->addChild(new ImageKitFormatProperty(kit));
			ctrl->addChild(new ImageKitDownSamplingProperty(tree, kit));
			ctrl->addChild(new ImageKitCompressionQualityProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::ImageDefinition definition;
		HPS::ImageKit kit;
	};

	class LegacyShaderKitSourceProperty : public BaseProperty
	{
	public:
		LegacyShaderKitSourceProperty(
			HPS::LegacyShaderKit const & kit)
			: BaseProperty("Source")
		{
			HPS::UTF8 _source;
			kit.ShowSource(_source);
			addChild(new ImmutableSizeTProperty("Byte Count", _source.GetLength()));
		}
	};

	class LegacyShaderKitMultitextureProperty : public BaseProperty
	{
	public:
		LegacyShaderKitMultitextureProperty(
			QTreeWidget * tree,
			HPS::LegacyShaderKit & kit)
			: BaseProperty("Multitexture")
			, kit(kit)
		{
			this->kit.ShowMultitexture(_state);
			BoolProperty * bool_property = new BoolProperty(tree, "State", _state);
			addChild(bool_property);
			bool_property->setupComboBox();
		}

		void onChildChanged() override
		{
			kit.SetMultitexture(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::LegacyShaderKit & kit;
		bool _state;
	};

	class LegacyShaderKitParameterizationSourceProperty : public BaseProperty
	{
	public:
		LegacyShaderKitParameterizationSourceProperty(
			QTreeWidget * tree,
			HPS::LegacyShaderKit & kit)
			: BaseProperty("ParameterizationSource")
			, kit(kit)
		{
			this->kit.ShowParameterizationSource(_source);
			addChild(new LegacyShaderParameterizationProperty(tree, _source));
		}

		void onChildChanged() override
		{
			kit.SetParameterizationSource(_source);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::LegacyShaderKit & kit;
		HPS::LegacyShader::Parameterization _source;
	};

	typedef MatrixKitProperty <
		HPS::LegacyShaderKit,
		&HPS::LegacyShaderKit::ShowTransformMatrix,
		&HPS::LegacyShaderKit::SetTransformMatrix,
		&HPS::LegacyShaderKit::UnsetTransformMatrix
	> BaseLegacyShaderKitTransformMatrixProperty;
	class LegacyShaderKitTransformMatrixProperty : public BaseLegacyShaderKitTransformMatrixProperty
	{
	public:
		LegacyShaderKitTransformMatrixProperty(
			HPS::LegacyShaderKit & kit)
			: BaseLegacyShaderKitTransformMatrixProperty("TransformMatrix", kit)
		{}
	};

	class LegacyShaderDefinitionProperty : public RootProperty
	{
	public:
		LegacyShaderDefinitionProperty(
			QTreeWidgetItem * ctrl,
			HPS::LegacyShaderDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			QTreeWidget * tree = ctrl->treeWidget();
			ctrl->addChild(new DefinitionNameProperty<HPS::LegacyShaderDefinition>(definition));
			ctrl->addChild(new LegacyShaderKitSourceProperty(kit));
			ctrl->addChild(new LegacyShaderKitMultitextureProperty(tree, kit));
			ctrl->addChild(new LegacyShaderKitParameterizationSourceProperty(tree, kit));
			ctrl->addChild(new LegacyShaderKitTransformMatrixProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::LegacyShaderDefinition definition;
		HPS::LegacyShaderKit kit;
	};

	typedef ImmutableArraySizeProperty <
		HPS::LinePatternKit,
		HPS::LinePatternParallelKit,
		&HPS::LinePatternKit::ShowParallels
	> BaseLinePatternKitParallelsProperty;
	class LinePatternKitParallelsProperty : public BaseLinePatternKitParallelsProperty
	{
	public:
		LinePatternKitParallelsProperty(
			HPS::LinePatternKit const & kit)
			: BaseLinePatternKitParallelsProperty("Parallels", "Count", kit)
		{}
	};

	class LinePatternKitJoinProperty : public SettableProperty
	{
	public:
		LinePatternKitJoinProperty(
			QTreeWidget * tree,
			HPS::LinePatternKit & kit)
			: SettableProperty("Join")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowJoin(join);
			if (!_isSet)
				join = HPS::LinePattern::Join::Mitre;
			addChild(new LinePatternJoinProperty(tree, join));
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetJoin(join);
		}

		void unset() override
		{
			kit.UnsetJoin();
		}

	private:
		HPS::LinePatternKit & kit;
		HPS::LinePattern::Join join;
		QTreeWidget * tree;
	};

	class LinePatternDefinitionProperty : public RootProperty
	{
	public:
		LinePatternDefinitionProperty(
			QTreeWidgetItem * ctrl,
			HPS::LinePatternDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			ctrl->addChild(new DefinitionNameProperty<HPS::LinePatternDefinition>(this->definition));
			ctrl->addChild(new LinePatternKitParallelsProperty(kit));
			LinePatternKitJoinProperty *linepatternkitjoinproperty = new LinePatternKitJoinProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(linepatternkitjoinproperty);
			linepatternkitjoinproperty->addSubItems();
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::LinePatternDefinition definition;
		HPS::LinePatternKit kit;
	};

	class GlyphKitRadiusProperty : public BaseProperty
	{
	public:
		GlyphKitRadiusProperty(
			HPS::GlyphKit & kit)
			: BaseProperty("Radius")
			, kit(kit)
		{
			this->kit.ShowRadius(radius);
			addChild(new SByteProperty("Value", radius));
		}

		void onChildChanged() override
		{
			kit.SetRadius(radius);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GlyphKit & kit;
		HPS::sbyte radius;
	};

	class GlyphKitOffsetProperty : public BaseProperty
	{
	public:
		GlyphKitOffsetProperty(
			HPS::GlyphKit & kit)
			: BaseProperty("Offset")
			, kit(kit)
		{
			this->kit.ShowOffset(offset);
			addChild(new GlyphPointProperty("Value", offset));
		}

		void onChildChanged() override
		{
			kit.SetOffset(offset);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GlyphKit & kit;
		HPS::GlyphPoint offset;
	};

	typedef ImmutableArraySizeProperty <
		HPS::GlyphKit,
		HPS::GlyphElement,
		&HPS::GlyphKit::ShowElements
	> BaseGlyphKitElementsProperty;
	class GlyphKitElementsProperty : public BaseGlyphKitElementsProperty
	{
	public:
		GlyphKitElementsProperty(
			HPS::GlyphKit const & kit)
			: BaseGlyphKitElementsProperty("Elements", "Count", kit)
		{}
	};

	class GlyphDefinitionProperty : public RootProperty
	{
	public:
		GlyphDefinitionProperty(
			QTreeWidgetItem * ctrl,
			HPS::GlyphDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			ctrl->addChild(new DefinitionNameProperty<HPS::GlyphDefinition>(this->definition));
			ctrl->addChild(new GlyphKitRadiusProperty(kit));
			ctrl->addChild(new GlyphKitOffsetProperty(kit));
			ctrl->addChild(new GlyphKitElementsProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::GlyphDefinition definition;
		HPS::GlyphKit kit;
	};

	typedef ImmutableArraySizeProperty <
		HPS::ShapeKit,
		HPS::ShapeElement,
		&HPS::ShapeKit::ShowElements
	> BaseShapeKitElementsProperty;
	class ShapeKitElementsProperty : public BaseShapeKitElementsProperty
	{
	public:
		ShapeKitElementsProperty(
			HPS::ShapeKit const & kit)
			: BaseShapeKitElementsProperty("Elements", "Count", kit)
		{}
	};

	class ShapeDefinitionProperty : public RootProperty
	{
	public:
		ShapeDefinitionProperty(
			QTreeWidgetItem * ctrl,
			HPS::ShapeDefinition const & definition)
			: RootProperty(ctrl)
		{
			HPS::ShapeKit kit;
			definition.Show(kit);
			ctrl->addChild(new DefinitionNameProperty<HPS::LegacyShaderDefinition>(definition));
			ctrl->addChild(new ShapeKitElementsProperty(kit));
		}
	};

	class ClipRegionLoopProperty : public ArrayProperty
	{
	public:
		ClipRegionLoopProperty(
			QTreeWidget * tree,
			const char * name,
			HPS::PointArray & loopPoints)
			: ArrayProperty(name)
			, loopPoints(loopPoints)
			, tree(tree)
		{

		}

		void addSubItems()
		{
			pointCount = static_cast<unsigned int>(loopPoints.size());

			ArraySizeProperty *arraysizeproperty = new ArraySizeProperty("Point Count", pointCount);
			addChild(arraysizeproperty);
			arraysizeproperty->setupSpinBox(tree);

			AddItems();
		}

		void onChildChanged() override
		{
			AddOrDeleteItems(pointCount, static_cast<unsigned int>(loopPoints.size()));
			ArrayProperty::onChildChanged();
		}

	protected:
		void ResizeArrays() override
		{
			loopPoints.resize(pointCount, HPS::Point::Origin());
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < pointCount; ++i)
			{
				std::string itemName = "Point " + std::to_string(i);
				addChild(new PointProperty(itemName.c_str(), loopPoints[i]));
			}
		}

	private:
		unsigned int pointCount;
		HPS::PointArray & loopPoints;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitClipRegionProperty : public SettableArrayProperty
	{
	private:
		enum PropertyTypeIndex
		{
			SpacePropertyIndex = 0,
			OperationPropertyIndex,
			CountPropertyIndex,
			FirstItemIndex,
		};

	public:
		DrawingAttributeKitClipRegionProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableArrayProperty("Clip Regions", FirstItemIndex)
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowClipRegion(_loops, _space, _operation);
			if (_isSet)
				_loop_count = static_cast<unsigned int>(_loops.size());
			else
			{
				_loop_count = 1;
				ResizeArrays();
				_space = HPS::Drawing::ClipSpace::World;
				_operation = HPS::Drawing::ClipOperation::Keep;
			}
			DrawingClipSpaceProperty *drawingclipspaceproperty = new DrawingClipSpaceProperty(tree, _space);
			addChild(drawingclipspaceproperty);
			drawingclipspaceproperty->setupChoices();

			DrawingClipOperationProperty *drawingclipoperationproperty = new DrawingClipOperationProperty(tree, _operation);
			addChild(drawingclipoperationproperty);
			drawingclipoperationproperty->setupChoices();

			ArraySizeProperty *arraysizeproperty = new ArraySizeProperty("Loop Count", _loop_count);
			addChild(arraysizeproperty);
			arraysizeproperty->setupSpinBox(tree);

			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (_loop_count < 1)
				return;
			AddOrDeleteItems(_loop_count, static_cast<unsigned int>(_loops.size()));
			kit.SetClipRegion(_loops, _space, _operation);
		}

		void unset() override
		{
			kit.UnsetClipRegion();
		}

		void ResizeArrays() override
		{
			_loops.resize(_loop_count, HPS::PointArray(1, HPS::Point::Origin()));
		}

		void AddItems() override
		{
			for (unsigned int loop = 0; loop < _loop_count; ++loop)
			{
				std::string itemName = "Loop " + std::to_string(loop);
				ClipRegionLoopProperty * clip_region_loop_property = new ClipRegionLoopProperty(tree, itemName.c_str(), _loops[loop]);
				addChild(clip_region_loop_property);
				clip_region_loop_property->addSubItems();
			}
		}

	private:
		HPS::DrawingAttributeKit & kit;
		unsigned int _loop_count;
		HPS::PointArrayArray _loops;
		HPS::Drawing::ClipSpace _space;
		HPS::Drawing::ClipOperation _operation;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitOverrideInternalColorProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitOverrideInternalColorProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("OverrideInternalColor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOverrideInternalColor(_kit);
			if (!_isSet)
			{
				_kit = HPS::VisibilityKit();
				_kit.SetEverything(false);
			}

			VisibilityKitFacesProperty *visibilitykitfacesproperty = new VisibilityKitFacesProperty(tree, _kit);
			addChild(visibilitykitfacesproperty);
			visibilitykitfacesproperty->addSubItems();

			VisibilityKitLinesProperty *visibilitykitlinesproperty = new VisibilityKitLinesProperty(tree, _kit);
			addChild(visibilitykitlinesproperty);
			visibilitykitlinesproperty->addSubItems();

			VisibilityKitGenericEdgesProperty *visibilitykitgenericedgesproperty = new VisibilityKitGenericEdgesProperty(tree, _kit);
			addChild(visibilitykitgenericedgesproperty);
			visibilitykitgenericedgesproperty->addSubItems();

			VisibilityKitTextProperty *visibilitykittextproperty = new VisibilityKitTextProperty(tree, _kit);
			addChild(visibilitykittextproperty);
			visibilitykittextproperty->addSubItems();

			VisibilityKitVerticesProperty *visibilitykitverticesproperty = new VisibilityKitVerticesProperty(tree, _kit);
			addChild(visibilitykitverticesproperty);
			visibilitykitverticesproperty->addSubItems();

			VisibilityKitMarkersProperty *visibilitykitmarkersproperty = new VisibilityKitMarkersProperty(tree, _kit);
			addChild(visibilitykitmarkersproperty);
			visibilitykitmarkersproperty->addSubItems();

			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetOverrideInternalColor(_kit);
		}

		void unset() override
		{
			kit.UnsetOverrideInternalColor();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::VisibilityKit _kit;
		QTreeWidget * tree;
	};

	class VisualEffectsKitEyeDomeLightingBackColorProperty : public SettableProperty
	{
	public:
		VisualEffectsKitEyeDomeLightingBackColorProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("EyeDomeLightingBackColor")
			, kit(kit)
			, tree(tree)
		{

		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEyeDomeLightingBackColor(_state, _color);
			if (!_isSet)
			{
				_state = true;
				_color = HPS::RGBColor::Black();
			}
			else if (!_state)
				_color = HPS::RGBColor::Black();
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new RGBColorProperty("Color", _color));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetEyeDomeLightingBackColor(_state, _color);
		}

		void unset() override
		{
			kit.UnsetEyeDomeLightingBackColor();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		HPS::RGBColor _color;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleReflectionVisibilityProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleReflectionVisibilityProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleReflectionVisibility")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleReflectionVisibility(_reflected_types);
			if (!_isSet)
			{
				_reflected_types = HPS::VisibilityKit();
			}
			VisibilityKitFacesProperty *visibilitykitfacesproperty = new VisibilityKitFacesProperty(tree, _reflected_types);
			addChild(visibilitykitfacesproperty);
			visibilitykitfacesproperty->addSubItems();

			VisibilityKitLinesProperty *visibilitykitlinesproperty = new VisibilityKitLinesProperty(tree, _reflected_types);
			addChild(visibilitykitlinesproperty);
			visibilitykitlinesproperty->addSubItems();

			VisibilityKitGenericEdgesProperty *visibilitykitgenericedgesproperty = new VisibilityKitGenericEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitgenericedgesproperty);
			visibilitykitgenericedgesproperty->addSubItems();

			VisibilityKitInteriorSilhouetteEdgesProperty *visibilitykitinteriorsilhouetteedgesproperty = new VisibilityKitInteriorSilhouetteEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitinteriorsilhouetteedgesproperty);
			visibilitykitinteriorsilhouetteedgesproperty->addSubItems();

			VisibilityKitAdjacentEdgesProperty *visibilitykitadjacentedgesproperty = new VisibilityKitAdjacentEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitadjacentedgesproperty);
			visibilitykitadjacentedgesproperty->addSubItems();

			VisibilityKitHardEdgesProperty *visibilitykithardedgesproperty = new VisibilityKitHardEdgesProperty(tree, _reflected_types);
			addChild(visibilitykithardedgesproperty);
			visibilitykithardedgesproperty->addSubItems();

			VisibilityKitMeshQuadEdgesProperty *visibilitykitmeshquadedgesproperty = new VisibilityKitMeshQuadEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitmeshquadedgesproperty);
			visibilitykitmeshquadedgesproperty->addSubItems();

			VisibilityKitNonCulledEdgesProperty *visibilitykitnoncullededgesproperty = new VisibilityKitNonCulledEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitnoncullededgesproperty);
			visibilitykitnoncullededgesproperty->addSubItems();

			VisibilityKitPerimeterEdgesProperty *visibilitykitperimeteredgesproperty = new VisibilityKitPerimeterEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitperimeteredgesproperty);
			visibilitykitperimeteredgesproperty->addSubItems();

			VisibilityKitTextProperty *visibilitykittextproperty = new VisibilityKitTextProperty(tree, _reflected_types);
			addChild(visibilitykittextproperty);
			visibilitykittextproperty->addSubItems();

			VisibilityKitLeaderLinesProperty *visibilitykitleaderlinesproperty = new VisibilityKitLeaderLinesProperty(tree, _reflected_types);
			addChild(visibilitykitleaderlinesproperty);
			visibilitykitleaderlinesproperty->addSubItems();

			VisibilityKitVerticesProperty *visibilitykitverticesproperty = new VisibilityKitVerticesProperty(tree, _reflected_types);
			addChild(visibilitykitverticesproperty);
			visibilitykitverticesproperty->addSubItems();

			VisibilityKitMarkersProperty *visibilitykitmarkersproperty = new VisibilityKitMarkersProperty(tree, _reflected_types);
			addChild(visibilitykitmarkersproperty);
			visibilitykitmarkersproperty->addSubItems();

			VisibilityKitEdgeLightsProperty *visibilitykitedgelightsproperty = new VisibilityKitEdgeLightsProperty(tree, _reflected_types);
			addChild(visibilitykitedgelightsproperty);
			visibilitykitedgelightsproperty->addSubItems();

			VisibilityKitMarkerLightsProperty *visibilitykitmarkerlightsproperty = new VisibilityKitMarkerLightsProperty(tree, _reflected_types);
			addChild(visibilitykitmarkerlightsproperty);
			visibilitykitmarkerlightsproperty->addSubItems();

			VisibilityKitFaceLightsProperty *visibilitykitfacelightsproperty = new VisibilityKitFaceLightsProperty(tree, _reflected_types);
			addChild(visibilitykitfacelightsproperty);
			visibilitykitfacelightsproperty->addSubItems();

			VisibilityKitCuttingSectionsProperty *visibilitykitcuttingsectionsproperty = new VisibilityKitCuttingSectionsProperty(tree, _reflected_types);
			addChild(visibilitykitcuttingsectionsproperty);
			visibilitykitcuttingsectionsproperty->addSubItems();

			VisibilityKitCutFacesProperty *visibilitykitcutfacesproperty = new VisibilityKitCutFacesProperty(tree, _reflected_types);
			addChild(visibilitykitcutfacesproperty);
			visibilitykitcutfacesproperty->addSubItems();

			VisibilityKitCutEdgesProperty *visibilitykitcutedgesproperty = new VisibilityKitCutEdgesProperty(tree, _reflected_types);
			addChild(visibilitykitcutedgesproperty);
			visibilitykitcutedgesproperty->addSubItems();

			VisibilityKitWindowsProperty *visibilitykitwindowsproperty = new VisibilityKitWindowsProperty(tree, _reflected_types);
			addChild(visibilitykitwindowsproperty);
			visibilitykitwindowsproperty->addSubItems();

			VisibilityKitShadowCastingProperty *visibilitykitshadowcastingproperty = new VisibilityKitShadowCastingProperty(tree, _reflected_types);
			addChild(visibilitykitshadowcastingproperty);
			visibilitykitshadowcastingproperty->addSubItems();

			VisibilityKitShadowReceivingProperty *visibilitykitshadowreceivingproperty = new VisibilityKitShadowReceivingProperty(tree, _reflected_types);
			addChild(visibilitykitshadowreceivingproperty);
			visibilitykitshadowreceivingproperty->addSubItems();

			VisibilityKitShadowEmittingProperty *visibilitykitshadowemittingproperty = new VisibilityKitShadowEmittingProperty(tree, _reflected_types);
			addChild(visibilitykitshadowemittingproperty);
			visibilitykitshadowemittingproperty->addSubItems();


			isSet(_isSet);
		}

	protected:
		void set() override
		{
			kit.SetSimpleReflectionVisibility(_reflected_types);
		}

		void unset() override
		{
			kit.UnsetSimpleReflectionVisibility();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::VisibilityKit _reflected_types;
		QTreeWidget * tree;
	};

	class ContourLineModeProperty : public BaseEnumProperty<HPS::ContourLine::Mode>
	{
	private:
		enum PropertyTypeIndex
		{
			ModePropertyIndex = 0,
			IntervalPropertyIndex,
			OffsetPropertyIndex,
			PositionsPropertyIndex,
		};

	public:
		ContourLineModeProperty(
			QTreeWidget * tree,
			HPS::ContourLine::Mode & enumValue)
			: BaseEnumProperty("Mode", enumValue)
			, tree(tree)
		{
		}

		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::ContourLine::Mode::Repeating; enumStrings[0] = "Repeating";
			enumValues[1] = HPS::ContourLine::Mode::Explicit; enumStrings[1] = "Explicit";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto intervalSibling = static_cast<BaseProperty *>(parent()->child(IntervalPropertyIndex));
			auto offsetSibling = static_cast<BaseProperty *>(parent()->child(OffsetPropertyIndex));
			auto positionsSibling = static_cast<BaseProperty *>(parent()->child(PositionsPropertyIndex));
			if (enumValue == HPS::ContourLine::Mode::Repeating)
			{
				intervalSibling->setFlags(intervalSibling->flags() | Qt::ItemFlag::ItemIsEnabled);
				offsetSibling->setFlags(offsetSibling->flags() | Qt::ItemFlag::ItemIsEnabled);
				positionsSibling->setFlags(positionsSibling->flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == HPS::ContourLine::Mode::Explicit)
			{
				intervalSibling->setFlags(intervalSibling->flags() & ~Qt::ItemFlag::ItemIsEnabled);
				offsetSibling->setFlags(offsetSibling->flags() & ~Qt::ItemFlag::ItemIsEnabled);
				positionsSibling->setFlags(positionsSibling->flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	class ContourLineKitPositionsArrayProperty : public ArrayProperty
	{
	public:
		ContourLineKitPositionsArrayProperty(
			QTreeWidget * tree,
			HPS::FloatArray & positions)
			: ArrayProperty("Positions")
			, positions(positions)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			positionCount = static_cast<unsigned int>(positions.size());
			ArraySizeProperty * array_size = new ArraySizeProperty("Count", positionCount);
			addChild(array_size);
			array_size->setupSpinBox(tree);
			AddItems();
		}

		void onChildChanged() override
		{
			AddOrDeleteItems(positionCount, static_cast<unsigned int>(positions.size()));
			ArrayProperty::onChildChanged();
		}

	protected:
		void ResizeArrays() override
		{
			positions.resize(positionCount, 0.0f);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < positionCount; ++i)
			{
				std::string itemName = "Position " + std::to_string(i);
				addChild(new FloatProperty(itemName.c_str(), positions[i]));
			}
		}

	private:
		unsigned int positionCount;
		HPS::FloatArray & positions;
		QTreeWidget * tree;
	};

	class ContourLineKitPositionsProperty : public SettableProperty
	{
	public:
		ContourLineKitPositionsProperty(
			QTreeWidget * tree,
			HPS::ContourLineKit & kit)
			: SettableProperty("Positions")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPositions(mode, positions);
			if (_isSet)
			{
				if (mode == HPS::ContourLine::Mode::Repeating)
				{
					interval = positions[0];
					offset = positions[1];
					positions.resize(1, 0.0f);
				}
				else if (mode == HPS::ContourLine::Mode::Explicit)
				{
					interval = 1.0f;
					offset = 0.0f;
				}
			}
			else
			{
				mode = HPS::ContourLine::Mode::Repeating;
				interval = 1.0f;
				offset = 0.0f;
				positions.resize(1, 0.0f);
			}

			auto modeChild = new ContourLineModeProperty(tree, mode);
			addChild(modeChild);
			modeChild->setupChoices();
			addChild(new UnsignedFloatProperty("Interval", interval));
			addChild(new FloatProperty("Offset", offset));
			ContourLineKitPositionsArrayProperty * contourlinekitpositionsarrayproperty = new ContourLineKitPositionsArrayProperty(tree, positions);
			addChild(contourlinekitpositionsarrayproperty);
			contourlinekitpositionsarrayproperty->addSubItems();
			modeChild->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (mode == HPS::ContourLine::Mode::Repeating)
				kit.SetPositions(interval, offset);
			else if (mode == HPS::ContourLine::Mode::Explicit)
				kit.SetPositions(positions);
		}

		void unset() override
		{
			kit.UnsetPositions();
		}

	private:
		HPS::ContourLineKit & kit;
		HPS::ContourLine::Mode mode;
		float interval;
		float offset;
		HPS::FloatArray positions;
		QTreeWidget * tree;
	};

	class ContourLineKitColorsProperty : public SettableArrayProperty
	{
	public:
		ContourLineKitColorsProperty(
			QTreeWidget * tree,
			HPS::ContourLineKit & kit)
			: SettableArrayProperty("Colors")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowColors(colors);
			if (_isSet)
				colorCount = static_cast<unsigned int>(colors.size());
			else
			{
				colorCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty * array_size_property = new ArraySizeProperty("Count", colorCount);
			addChild(array_size_property);
			array_size_property->setupSpinBox(tree);
			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (colorCount < 1)
				return;
			AddOrDeleteItems(colorCount, static_cast<unsigned int>(colors.size()));
			kit.SetColors(colors);
		}

		void unset() override
		{
			kit.UnsetColors();
		}

		void ResizeArrays() override
		{
			colors.resize(colorCount, HPS::RGBColor::Black());
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < colorCount; ++i)
			{
				std::string itemName = "Color " + std::to_string(i);
				addChild(new RGBColorProperty(itemName.c_str(), colors[i]));
			}
		}

	private:
		HPS::ContourLineKit & kit;
		unsigned int colorCount;
		HPS::RGBColorArray colors;
		QTreeWidget * tree;
	};

	class ContourLineKitPatternsProperty : public SettableArrayProperty
	{
	public:
		ContourLineKitPatternsProperty(
			QTreeWidget * tree,
			HPS::ContourLineKit & kit)
			: SettableArrayProperty("Patterns")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPatterns(patterns);
			if (_isSet)
				patternCount = static_cast<unsigned int>(patterns.size());
			else
			{
				patternCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty * array_size_property = new ArraySizeProperty("Count", patternCount);
			addChild(array_size_property);
			array_size_property->setupSpinBox(tree);
			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (patternCount < 1)
				return;
			AddOrDeleteItems(patternCount, static_cast<unsigned int>(patterns.size()));
			kit.SetPatterns(patterns);
		}

		void unset() override
		{
			kit.UnsetPatterns();
		}

		void ResizeArrays() override
		{
			patterns.resize(patternCount);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < patternCount; ++i)
			{
				std::string itemName = "Pattern " + std::to_string(i);
				addChild(new UTF8Property(itemName.c_str(), patterns[i]));
			}
		}

	private:
		HPS::ContourLineKit & kit;
		unsigned int patternCount;
		HPS::UTF8Array patterns;
		QTreeWidget * tree;
	};

	class SingleContourLineWeightProperty : public BaseProperty
	{
	public:
		SingleContourLineWeightProperty(
			QTreeWidget * tree,
			unsigned int index,
			float & weight,
			HPS::Line::SizeUnits & units)
			: BaseProperty("")
			, weight(weight)
			, units(units)
			, tree(tree)
		{
			std::string name = "Weight " + std::to_string(index);
			setText(0, name.c_str());
		}

		void addSubItems()
		{
			addChild(new FloatProperty("Weight", weight));
			LineSizeUnitsProperty * linesizeunitsproperty = new LineSizeUnitsProperty(tree, units);
			addChild(linesizeunitsproperty);
			linesizeunitsproperty->setupChoices();
		}

	private:
		float & weight;
		HPS::Line::SizeUnits & units;
		QTreeWidget * tree;
	};

	class ContourLineKitWeightsProperty : public SettableArrayProperty
	{
	public:
		ContourLineKitWeightsProperty(
			QTreeWidget * tree,
			HPS::ContourLineKit & kit)
			: SettableArrayProperty("Weights")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWeights(weights, units);
			if (_isSet)
				weightCount = static_cast<unsigned int>(weights.size());
			else
			{
				weightCount = 1;
				ResizeArrays();
			}
			ArraySizeProperty * array_size_property = new ArraySizeProperty("Count", weightCount);
			addChild(array_size_property);
			array_size_property->setupSpinBox(tree);
			AddItems();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (weightCount < 1)
				return;
			AddOrDeleteItems(weightCount, static_cast<unsigned int>(weights.size()));
			kit.SetWeights(weights, units);
		}

		void unset() override
		{
			kit.UnsetWeights();
		}

		void ResizeArrays() override
		{
			weights.resize(weightCount, 1.0f);
			units.resize(weightCount, HPS::Line::SizeUnits::ScaleFactor);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < weightCount; ++i)
			{
				SingleContourLineWeightProperty * prop = new SingleContourLineWeightProperty(tree, i, weights[i], units[i]);
				addChild(prop);
				prop->addSubItems();
			}
		}

	private:
		HPS::ContourLineKit & kit;
		unsigned int weightCount;
		HPS::FloatArray weights;
		HPS::LineSizeUnitsArray units;
		QTreeWidget * tree;
	};

	enum class BoundingVolumeType
	{
		Sphere,
		Cuboid,
	};

	class BoundingVolumeTypeProperty : public BaseEnumProperty<BoundingVolumeType>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			SpherePropertyIndex,
			CuboidPropertyIndex,
		};

	public:
		BoundingVolumeTypeProperty(
			QTreeWidget * tree,
			BoundingVolumeType & type)
			: BaseEnumProperty("Type", type)
			, tree(tree)
		{

		}

		void setupChoices()
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = BoundingVolumeType::Sphere; enumStrings[0] = "Sphere";
			enumValues[1] = BoundingVolumeType::Cuboid; enumStrings[1] = "Cuboid";
			initializeEnumValues(enumValues, enumStrings, tree);
		}

		void enableValidProperties() override
		{
			auto sphereSibling = static_cast<BaseProperty *>(parent()->child(SpherePropertyIndex));
			auto cuboidSibling = static_cast<BaseProperty *>(parent()->child(CuboidPropertyIndex));
			if (enumValue == BoundingVolumeType::Sphere)
			{
				sphereSibling->setFlags(sphereSibling->flags() | Qt::ItemFlag::ItemIsEnabled);
				cuboidSibling->setFlags(cuboidSibling->flags() & ~Qt::ItemFlag::ItemIsEnabled);
			}
			else if (enumValue == BoundingVolumeType::Cuboid)
			{
				sphereSibling->setFlags(sphereSibling->flags() & ~Qt::ItemFlag::ItemIsEnabled);
				cuboidSibling->setFlags(cuboidSibling->flags() | Qt::ItemFlag::ItemIsEnabled);
			}
		}

	private:
		QTreeWidget * tree;
	};

	class BoundingKitVolumeProperty : public SettableProperty
	{
	public:
		BoundingKitVolumeProperty(
			QTreeWidget * tree,
			HPS::BoundingKit & kit)
			: SettableProperty("Volume")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVolume(sphere, cuboid);
			if (!_isSet)
			{
				sphere = HPS::SimpleSphere(HPS::Point::Origin(), 1);
				cuboid = HPS::SimpleCuboid(HPS::Point(-1, -1, -1), HPS::Point(1, 1, 1));
			}
			type = BoundingVolumeType::Cuboid;
			auto typeChild = new BoundingVolumeTypeProperty(tree, type);
			addChild(typeChild);
			typeChild->setupChoices();
			addChild(new SimpleSphereProperty("Sphere", sphere));
			addChild(new SimpleCuboidProperty("Cuboid", cuboid));
			typeChild->enableValidProperties();
			isSet(_isSet);
		}

	protected:
		void set() override
		{
			if (type == BoundingVolumeType::Sphere)
				kit.SetVolume(sphere);
			else if (type == BoundingVolumeType::Cuboid)
				kit.SetVolume(cuboid);
		}

		void unset() override
		{
			kit.UnsetVolume();
		}

	private:
		HPS::BoundingKit & kit;
		BoundingVolumeType type;
		HPS::SimpleSphere sphere;
		HPS::SimpleCuboid cuboid;
		QTreeWidget * tree;
	};

	class TransparencyKitMethodProperty : public SettableProperty
	{
	public:
		TransparencyKitMethodProperty(
			QTreeWidget * tree,
			HPS::TransparencyKit & kit)
			: SettableProperty("Method")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMethod(_style);
			if (!_isSet)
			{
				_style = HPS::Transparency::Method::Blended;
			}
			TransparencyMethodProperty * enumObject_0 = new TransparencyMethodProperty(tree, _style);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMethod(_style);
		}

		void unset() override
		{
			kit.UnsetMethod();
		}

	private:
		HPS::TransparencyKit & kit;
		HPS::Transparency::Method _style;
		QTreeWidget * tree;
	};

	class TransparencyKitAlgorithmProperty : public SettableProperty
	{
	public:
		TransparencyKitAlgorithmProperty(
			QTreeWidget * tree,
			HPS::TransparencyKit & kit)
			: SettableProperty("Algorithm")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAlgorithm(_algorithm);
			if (!_isSet)
			{
				_algorithm = HPS::Transparency::Algorithm::DepthPeeling;
			}
			TransparencyAlgorithmProperty * enumObject_0 = new TransparencyAlgorithmProperty(tree, _algorithm);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetAlgorithm(_algorithm);
		}

		void unset() override
		{
			kit.UnsetAlgorithm();
		}

	private:
		HPS::TransparencyKit & kit;
		HPS::Transparency::Algorithm _algorithm;
		QTreeWidget * tree;
	};

	class TransparencyKitDepthPeelingLayersProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthPeelingLayersProperty(
			QTreeWidget * tree,
			HPS::TransparencyKit & kit)
			: SettableProperty("DepthPeelingLayers")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDepthPeelingLayers(_layers);
			if (!_isSet)
			{
				_layers = 0;
			}
			addChild(new UnsignedIntProperty("Layers", _layers));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDepthPeelingLayers(_layers);
		}

		void unset() override
		{
			kit.UnsetDepthPeelingLayers();
		}

	private:
		HPS::TransparencyKit & kit;
		unsigned int _layers;
		QTreeWidget * tree;
	};

	class TransparencyKitDepthPeelingMinimumAreaProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthPeelingMinimumAreaProperty(
			QTreeWidget * tree,
			HPS::TransparencyKit & kit)
			: SettableProperty("DepthPeelingMinimumArea")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDepthPeelingMinimumArea(_area, _units);
			if (!_isSet)
			{
				_area = 0.0f;
				_units = HPS::Transparency::AreaUnits::Pixels;
			}
			addChild(new FloatProperty("Area", _area));
			TransparencyAreaUnitsProperty * enumObject_1 = new TransparencyAreaUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDepthPeelingMinimumArea(_area, _units);
		}

		void unset() override
		{
			kit.UnsetDepthPeelingMinimumArea();
		}

	private:
		HPS::TransparencyKit & kit;
		float _area;
		HPS::Transparency::AreaUnits _units;
		QTreeWidget * tree;
	};

	class TransparencyKitDepthWritingProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthWritingProperty(
			QTreeWidget * tree,
			HPS::TransparencyKit & kit)
			: SettableProperty("DepthWriting")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDepthWriting(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDepthWriting(_state);
		}

		void unset() override
		{
			kit.UnsetDepthWriting();
		}

	private:
		HPS::TransparencyKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransparencyKitDepthPeelingPreferenceProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthPeelingPreferenceProperty(
			QTreeWidget * tree,
			HPS::TransparencyKit & kit)
			: SettableProperty("DepthPeelingPreference")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDepthPeelingPreference(_preference);
			if (!_isSet)
			{
				_preference = HPS::Transparency::Preference::Fastest;
			}
			TransparencyPreferenceProperty * enumObject_0 = new TransparencyPreferenceProperty(tree, _preference);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDepthPeelingPreference(_preference);
		}

		void unset() override
		{
			kit.UnsetDepthPeelingPreference();
		}

	private:
		HPS::TransparencyKit & kit;
		HPS::Transparency::Preference _preference;
		QTreeWidget * tree;
	};

	class TransparencyKitProperty : public RootProperty
	{
	public:
		TransparencyKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowTransparency(kit);
			auto prop_Method = new TransparencyKitMethodProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Method);
			prop_Method->addSubItems();
			auto prop_Algorithm = new TransparencyKitAlgorithmProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Algorithm);
			prop_Algorithm->addSubItems();
			auto prop_DepthPeelingLayers = new TransparencyKitDepthPeelingLayersProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthPeelingLayers);
			prop_DepthPeelingLayers->addSubItems();
			auto prop_DepthPeelingMinimumArea = new TransparencyKitDepthPeelingMinimumAreaProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthPeelingMinimumArea);
			prop_DepthPeelingMinimumArea->addSubItems();
			auto prop_DepthWriting = new TransparencyKitDepthWritingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthWriting);
			prop_DepthWriting->addSubItems();
			auto prop_DepthPeelingPreference = new TransparencyKitDepthPeelingPreferenceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthPeelingPreference);
			prop_DepthPeelingPreference->addSubItems();
		}

		void Apply() override
		{
			key.UnsetTransparency();
			key.SetTransparency(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::TransparencyKit kit;
	};

	class CullingKitDeferralExtentProperty : public SettableProperty
	{
	public:
		CullingKitDeferralExtentProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("DeferralExtent")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDeferralExtent(_state, _pixels);
			if (!_isSet)
			{
				_state = true;
				_pixels = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UnsignedIntProperty("Pixels", _pixels));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDeferralExtent(_state, _pixels);
		}

		void unset() override
		{
			kit.UnsetDeferralExtent();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		unsigned int _pixels;
		QTreeWidget * tree;
	};

	class CullingKitExtentProperty : public SettableProperty
	{
	public:
		CullingKitExtentProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("Extent")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowExtent(_state, _pixels);
			if (!_isSet)
			{
				_state = true;
				_pixels = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UnsignedIntProperty("Pixels", _pixels));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetExtent(_state, _pixels);
		}

		void unset() override
		{
			kit.UnsetExtent();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		unsigned int _pixels;
		QTreeWidget * tree;
	};

	class CullingKitBackFaceProperty : public SettableProperty
	{
	public:
		CullingKitBackFaceProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("BackFace")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackFace(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBackFace(_state);
		}

		void unset() override
		{
			kit.UnsetBackFace();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class CullingKitFaceProperty : public SettableProperty
	{
	public:
		CullingKitFaceProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("Face")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFace(_state);
			if (!_isSet)
			{
				_state = HPS::Culling::Face::Back;
			}
			CullingFaceProperty * enumObject_0 = new CullingFaceProperty(tree, _state);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFace(_state);
		}

		void unset() override
		{
			kit.UnsetFace();
		}

	private:
		HPS::CullingKit & kit;
		HPS::Culling::Face _state;
		QTreeWidget * tree;
	};

	class CullingKitVectorProperty : public SettableProperty
	{
	public:
		CullingKitVectorProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("Vector")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVector(_state, _vector);
			if (!_isSet)
			{
				_state = true;
				_vector = HPS::Vector::Unit();
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new VectorProperty("Vector", _vector));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVector(_state, _vector);
		}

		void unset() override
		{
			kit.UnsetVector();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		HPS::Vector _vector;
		QTreeWidget * tree;
	};

	class CullingKitVectorToleranceProperty : public SettableProperty
	{
	public:
		CullingKitVectorToleranceProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("VectorTolerance")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVectorTolerance(_tolerance_degrees);
			if (!_isSet)
			{
				_tolerance_degrees = 0.0f;
			}
			addChild(new FloatProperty("Tolerance Degrees", _tolerance_degrees));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVectorTolerance(_tolerance_degrees);
		}

		void unset() override
		{
			kit.UnsetVectorTolerance();
		}

	private:
		HPS::CullingKit & kit;
		float _tolerance_degrees;
		QTreeWidget * tree;
	};

	class CullingKitFrustumProperty : public SettableProperty
	{
	public:
		CullingKitFrustumProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("Frustum")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFrustum(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFrustum(_state);
		}

		void unset() override
		{
			kit.UnsetFrustum();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class CullingKitVolumeProperty : public SettableProperty
	{
	public:
		CullingKitVolumeProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("Volume")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVolume(_state, _volume);
			if (!_isSet)
			{
				_state = true;
				_volume = HPS::SimpleCuboid(HPS::Point(-1, -1, -1), HPS::Point(1, 1, 1));
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			addChild(new SimpleCuboidProperty("Volume", _volume));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVolume(_state, _volume);
		}

		void unset() override
		{
			kit.UnsetVolume();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		HPS::SimpleCuboid _volume;
		QTreeWidget * tree;
	};

	class CullingKitDistanceProperty : public SettableProperty
	{
	public:
		CullingKitDistanceProperty(
			QTreeWidget * tree,
			HPS::CullingKit & kit)
			: SettableProperty("Distance")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDistance(_state, _max_distance);
			if (!_isSet)
			{
				_state = true;
				_max_distance = 0.0f;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			addChild(new FloatProperty("Max Distance", _max_distance));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDistance(_state, _max_distance);
		}

		void unset() override
		{
			kit.UnsetDistance();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		float _max_distance;
		QTreeWidget * tree;
	};

	class CullingKitProperty : public RootProperty
	{
	public:
		CullingKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCulling(kit);
			auto prop_DeferralExtent = new CullingKitDeferralExtentProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DeferralExtent);
			prop_DeferralExtent->addSubItems();
			auto prop_Extent = new CullingKitExtentProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Extent);
			prop_Extent->addSubItems();
			auto prop_BackFace = new CullingKitBackFaceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BackFace);
			prop_BackFace->addSubItems();
			auto prop_Face = new CullingKitFaceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Face);
			prop_Face->addSubItems();
			auto prop_Vector = new CullingKitVectorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Vector);
			prop_Vector->addSubItems();
			auto prop_VectorTolerance = new CullingKitVectorToleranceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VectorTolerance);
			prop_VectorTolerance->addSubItems();
			auto prop_Frustum = new CullingKitFrustumProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Frustum);
			prop_Frustum->addSubItems();
			auto prop_Volume = new CullingKitVolumeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Volume);
			prop_Volume->addSubItems();
			auto prop_Distance = new CullingKitDistanceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Distance);
			prop_Distance->addSubItems();
		}

		void Apply() override
		{
			key.UnsetCulling();
			key.SetCulling(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CullingKit kit;
	};

	class TextAttributeKitAlignmentProperty : public SettableProperty
	{
	public:
		TextAttributeKitAlignmentProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Alignment")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAlignment(_align, _ref, _justify);
			if (!_isSet)
			{
				_align = HPS::Text::Alignment::BottomLeft;
				_ref = HPS::Text::ReferenceFrame::WorldAligned;
				_justify = HPS::Text::Justification::Left;
			}
			TextAlignmentProperty * enumObject_0 = new TextAlignmentProperty(tree, _align);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			TextReferenceFrameProperty * enumObject_1 = new TextReferenceFrameProperty(tree, _ref);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			TextJustificationProperty * enumObject_2 = new TextJustificationProperty(tree, _justify);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetAlignment(_align, _ref, _justify);
		}

		void unset() override
		{
			kit.UnsetAlignment();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Text::Alignment _align;
		HPS::Text::ReferenceFrame _ref;
		HPS::Text::Justification _justify;
		QTreeWidget * tree;
	};

	class TextAttributeKitBoldProperty : public SettableProperty
	{
	public:
		TextAttributeKitBoldProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Bold")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBold(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBold(_state);
		}

		void unset() override
		{
			kit.UnsetBold();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TextAttributeKitItalicProperty : public SettableProperty
	{
	public:
		TextAttributeKitItalicProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Italic")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowItalic(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetItalic(_state);
		}

		void unset() override
		{
			kit.UnsetItalic();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TextAttributeKitOverlineProperty : public SettableProperty
	{
	public:
		TextAttributeKitOverlineProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Overline")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOverline(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOverline(_state);
		}

		void unset() override
		{
			kit.UnsetOverline();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TextAttributeKitStrikethroughProperty : public SettableProperty
	{
	public:
		TextAttributeKitStrikethroughProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Strikethrough")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowStrikethrough(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetStrikethrough(_state);
		}

		void unset() override
		{
			kit.UnsetStrikethrough();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TextAttributeKitUnderlineProperty : public SettableProperty
	{
	public:
		TextAttributeKitUnderlineProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Underline")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowUnderline(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetUnderline(_state);
		}

		void unset() override
		{
			kit.UnsetUnderline();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TextAttributeKitSlantProperty : public SettableProperty
	{
	public:
		TextAttributeKitSlantProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Slant")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSlant(_angle);
			if (!_isSet)
			{
				_angle = 0.0f;
			}
			addChild(new FloatProperty("Angle", _angle));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSlant(_angle);
		}

		void unset() override
		{
			kit.UnsetSlant();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _angle;
		QTreeWidget * tree;
	};

	class TextAttributeKitLineSpacingProperty : public SettableProperty
	{
	public:
		TextAttributeKitLineSpacingProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("LineSpacing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLineSpacing(_multiplier);
			if (!_isSet)
			{
				_multiplier = 0.0f;
			}
			addChild(new FloatProperty("Multiplier", _multiplier));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLineSpacing(_multiplier);
		}

		void unset() override
		{
			kit.UnsetLineSpacing();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _multiplier;
		QTreeWidget * tree;
	};

	class TextAttributeKitExtraSpaceProperty : public SettableProperty
	{
	public:
		TextAttributeKitExtraSpaceProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("ExtraSpace")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowExtraSpace(_state, _size, _units);
			if (!_isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Size", _size));
			TextSizeUnitsProperty * enumObject_2 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetExtraSpace(_state, _size, _units);
		}

		void unset() override
		{
			kit.UnsetExtraSpace();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		float _size;
		HPS::Text::SizeUnits _units;
		QTreeWidget * tree;
	};

	class TextAttributeKitGreekingProperty : public SettableProperty
	{
	public:
		TextAttributeKitGreekingProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Greeking")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowGreeking(_state, _size, _units, _mode);
			if (!_isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::GreekingUnits::Pixels;
				_mode = HPS::Text::GreekingMode::Lines;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Size", _size));
			TextGreekingUnitsProperty * enumObject_2 = new TextGreekingUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			TextGreekingModeProperty * enumObject_3 = new TextGreekingModeProperty(tree, _mode);
			addChild(enumObject_3);
			enumObject_3->setupChoices();
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetGreeking(_state, _size, _units, _mode);
		}

		void unset() override
		{
			kit.UnsetGreeking();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		float _size;
		HPS::Text::GreekingUnits _units;
		HPS::Text::GreekingMode _mode;
		QTreeWidget * tree;
	};

	class TextAttributeKitSizeToleranceProperty : public SettableProperty
	{
	public:
		TextAttributeKitSizeToleranceProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("SizeTolerance")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSizeTolerance(_state, _size, _units);
			if (!_isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeToleranceUnits::Percent;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Size", _size));
			TextSizeToleranceUnitsProperty * enumObject_2 = new TextSizeToleranceUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSizeTolerance(_state, _size, _units);
		}

		void unset() override
		{
			kit.UnsetSizeTolerance();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		float _size;
		HPS::Text::SizeToleranceUnits _units;
		QTreeWidget * tree;
	};

	class TextAttributeKitSizeProperty : public SettableProperty
	{
	public:
		TextAttributeKitSizeProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Size")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSize(_size, _units);
			if (!_isSet)
			{
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			addChild(new FloatProperty("Size", _size));
			TextSizeUnitsProperty * enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSize(_size, _units);
		}

		void unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _size;
		HPS::Text::SizeUnits _units;
		QTreeWidget * tree;
	};

	class TextAttributeKitFontProperty : public SettableProperty
	{
	public:
		TextAttributeKitFontProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Font")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFont(_name);
			if (!_isSet)
			{
				_name = "name";
			}
			addChild(new UTF8Property("Name", _name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFont(_name);
		}

		void unset() override
		{
			kit.UnsetFont();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::UTF8 _name;
		QTreeWidget * tree;
	};

	class TextAttributeKitTransformProperty : public SettableProperty
	{
	public:
		TextAttributeKitTransformProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Transform")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowTransform(_trans);
			if (!_isSet)
			{
				_trans = HPS::Text::Transform::Transformable;
			}
			TextTransformProperty * enumObject_0 = new TextTransformProperty(tree, _trans);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTransform(_trans);
		}

		void unset() override
		{
			kit.UnsetTransform();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Text::Transform _trans;
		QTreeWidget * tree;
	};

	class TextAttributeKitRendererProperty : public SettableProperty
	{
	public:
		TextAttributeKitRendererProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Renderer")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRenderer(_rend);
			if (!_isSet)
			{
				_rend = HPS::Text::Renderer::Default;
			}
			TextRendererProperty * enumObject_0 = new TextRendererProperty(tree, _rend);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRenderer(_rend);
		}

		void unset() override
		{
			kit.UnsetRenderer();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Text::Renderer _rend;
		QTreeWidget * tree;
	};

	class TextAttributeKitPreferenceProperty : public SettableProperty
	{
	public:
		TextAttributeKitPreferenceProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Preference")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPreference(_cutoff, _units, _smaller, _larger);
			if (!_isSet)
			{
				_cutoff = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
				_smaller = HPS::Text::Preference::Default;
				_larger = HPS::Text::Preference::Default;
			}
			addChild(new FloatProperty("Cutoff", _cutoff));
			TextSizeUnitsProperty * enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			TextPreferenceProperty * enumObject_2 = new TextPreferenceProperty(tree, _smaller);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			TextPreferenceProperty * enumObject_3 = new TextPreferenceProperty(tree, _larger);
			addChild(enumObject_3);
			enumObject_3->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPreference(_cutoff, _units, _smaller, _larger);
		}

		void unset() override
		{
			kit.UnsetPreference();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _cutoff;
		HPS::Text::SizeUnits _units;
		HPS::Text::Preference _smaller;
		HPS::Text::Preference _larger;
		QTreeWidget * tree;
	};

	class TextAttributeKitPathProperty : public SettableProperty
	{
	public:
		TextAttributeKitPathProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Path")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPath(_path);
			if (!_isSet)
			{
				_path = HPS::Vector::Unit();
			}
			addChild(new VectorProperty("Path", _path));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPath(_path);
		}

		void unset() override
		{
			kit.UnsetPath();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Vector _path;
		QTreeWidget * tree;
	};

	class TextAttributeKitSpacingProperty : public SettableProperty
	{
	public:
		TextAttributeKitSpacingProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Spacing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSpacing(_multiplier);
			if (!_isSet)
			{
				_multiplier = 0.0f;
			}
			addChild(new FloatProperty("Multiplier", _multiplier));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSpacing(_multiplier);
		}

		void unset() override
		{
			kit.UnsetSpacing();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _multiplier;
		QTreeWidget * tree;
	};

	class TextAttributeKitBackgroundProperty : public SettableProperty
	{
	public:
		TextAttributeKitBackgroundProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("Background")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackground(_state, _name);
			if (!_isSet)
			{
				_state = true;
				_name = "name";
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UTF8Property("Name", _name));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBackground(_state, _name);
		}

		void unset() override
		{
			kit.UnsetBackground();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		HPS::UTF8 _name;
		QTreeWidget * tree;
	};

	class TextAttributeKitBackgroundStyleProperty : public SettableProperty
	{
	public:
		TextAttributeKitBackgroundStyleProperty(
			QTreeWidget * tree,
			HPS::TextAttributeKit & kit)
			: SettableProperty("BackgroundStyle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBackgroundStyle(_name);
			if (!_isSet)
			{
				_name = "name";
			}
			addChild(new UTF8Property("Name", _name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBackgroundStyle(_name);
		}

		void unset() override
		{
			kit.UnsetBackgroundStyle();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::UTF8 _name;
		QTreeWidget * tree;
	};

	class TextAttributeKitProperty : public RootProperty
	{
	public:
		TextAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowTextAttribute(kit);
			auto prop_Alignment = new TextAttributeKitAlignmentProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Alignment);
			prop_Alignment->addSubItems();
			auto prop_Bold = new TextAttributeKitBoldProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Bold);
			prop_Bold->addSubItems();
			auto prop_Italic = new TextAttributeKitItalicProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Italic);
			prop_Italic->addSubItems();
			auto prop_Overline = new TextAttributeKitOverlineProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Overline);
			prop_Overline->addSubItems();
			auto prop_Strikethrough = new TextAttributeKitStrikethroughProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Strikethrough);
			prop_Strikethrough->addSubItems();
			auto prop_Underline = new TextAttributeKitUnderlineProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Underline);
			prop_Underline->addSubItems();
			auto prop_Slant = new TextAttributeKitSlantProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Slant);
			prop_Slant->addSubItems();
			auto prop_LineSpacing = new TextAttributeKitLineSpacingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_LineSpacing);
			prop_LineSpacing->addSubItems();
			auto prop_Rotation = new TextAttributeKitRotationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Rotation);
			prop_Rotation->addSubItems();
			auto prop_ExtraSpace = new TextAttributeKitExtraSpaceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ExtraSpace);
			prop_ExtraSpace->addSubItems();
			auto prop_Greeking = new TextAttributeKitGreekingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Greeking);
			prop_Greeking->addSubItems();
			auto prop_SizeTolerance = new TextAttributeKitSizeToleranceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SizeTolerance);
			prop_SizeTolerance->addSubItems();
			auto prop_Size = new TextAttributeKitSizeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Size);
			prop_Size->addSubItems();
			auto prop_Font = new TextAttributeKitFontProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Font);
			prop_Font->addSubItems();
			auto prop_Transform = new TextAttributeKitTransformProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Transform);
			prop_Transform->addSubItems();
			auto prop_Renderer = new TextAttributeKitRendererProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Renderer);
			prop_Renderer->addSubItems();
			auto prop_Preference = new TextAttributeKitPreferenceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Preference);
			prop_Preference->addSubItems();
			auto prop_Path = new TextAttributeKitPathProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Path);
			prop_Path->addSubItems();
			auto prop_Spacing = new TextAttributeKitSpacingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Spacing);
			prop_Spacing->addSubItems();
			auto prop_Background = new TextAttributeKitBackgroundProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Background);
			prop_Background->addSubItems();
			auto prop_BackgroundMargins = new TextAttributeKitBackgroundMarginsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BackgroundMargins);
			prop_BackgroundMargins->addSubItems();
			auto prop_BackgroundStyle = new TextAttributeKitBackgroundStyleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BackgroundStyle);
			prop_BackgroundStyle->addSubItems();
		}

		void Apply() override
		{
			key.UnsetTextAttribute();
			key.SetTextAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::TextAttributeKit kit;
	};

	class EdgeAttributeKitPatternProperty : public SettableProperty
	{
	public:
		EdgeAttributeKitPatternProperty(
			QTreeWidget * tree,
			HPS::EdgeAttributeKit & kit)
			: SettableProperty("Pattern")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPattern(_pattern_name);
			if (!_isSet)
			{
				_pattern_name = "pattern_name";
			}
			addChild(new UTF8Property("Pattern Name", _pattern_name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPattern(_pattern_name);
		}

		void unset() override
		{
			kit.UnsetPattern();
		}

	private:
		HPS::EdgeAttributeKit & kit;
		HPS::UTF8 _pattern_name;
		QTreeWidget * tree;
	};

	class EdgeAttributeKitWeightProperty : public SettableProperty
	{
	public:
		EdgeAttributeKitWeightProperty(
			QTreeWidget * tree,
			HPS::EdgeAttributeKit & kit)
			: SettableProperty("Weight")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWeight(_weight, _units);
			if (!_isSet)
			{
				_weight = 0.0f;
				_units = HPS::Edge::SizeUnits::ScaleFactor;
			}
			addChild(new FloatProperty("Weight", _weight));
			EdgeSizeUnitsProperty * enumObject_1 = new EdgeSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetWeight(_weight, _units);
		}

		void unset() override
		{
			kit.UnsetWeight();
		}

	private:
		HPS::EdgeAttributeKit & kit;
		float _weight;
		HPS::Edge::SizeUnits _units;
		QTreeWidget * tree;
	};

	class EdgeAttributeKitHardAngleProperty : public SettableProperty
	{
	public:
		EdgeAttributeKitHardAngleProperty(
			QTreeWidget * tree,
			HPS::EdgeAttributeKit & kit)
			: SettableProperty("HardAngle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowHardAngle(_angle);
			if (!_isSet)
			{
				_angle = 0.0f;
			}
			addChild(new FloatProperty("Angle", _angle));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetHardAngle(_angle);
		}

		void unset() override
		{
			kit.UnsetHardAngle();
		}

	private:
		HPS::EdgeAttributeKit & kit;
		float _angle;
		QTreeWidget * tree;
	};

	class EdgeAttributeKitProperty : public RootProperty
	{
	public:
		EdgeAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowEdgeAttribute(kit);
			auto prop_Pattern = new EdgeAttributeKitPatternProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Pattern);
			prop_Pattern->addSubItems();
			auto prop_Weight = new EdgeAttributeKitWeightProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Weight);
			prop_Weight->addSubItems();
			auto prop_HardAngle = new EdgeAttributeKitHardAngleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_HardAngle);
			prop_HardAngle->addSubItems();
		}

		void Apply() override
		{
			key.UnsetEdgeAttribute();
			key.SetEdgeAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::EdgeAttributeKit kit;
	};

	class CurveAttributeKitBudgetProperty : public SettableProperty
	{
	public:
		CurveAttributeKitBudgetProperty(
			QTreeWidget * tree,
			HPS::CurveAttributeKit & kit)
			: SettableProperty("Budget")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			size_t _budget_st;
			bool _isSet = this->kit.ShowBudget(_budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!_isSet)
			{
				_budget = 0;
			}
			addChild(new UnsignedIntProperty("Budget", _budget));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBudget(_budget);
		}

		void unset() override
		{
			kit.UnsetBudget();
		}

	private:
		HPS::CurveAttributeKit & kit;
		unsigned int _budget;
		QTreeWidget * tree;
	};

	class CurveAttributeKitContinuedBudgetProperty : public SettableProperty
	{
	public:
		CurveAttributeKitContinuedBudgetProperty(
			QTreeWidget * tree,
			HPS::CurveAttributeKit & kit)
			: SettableProperty("ContinuedBudget")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			size_t _budget_st;
			bool _isSet = this->kit.ShowContinuedBudget(_state, _budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!_isSet)
			{
				_state = true;
				_budget = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UnsignedIntProperty("Budget", _budget));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetContinuedBudget(_state, _budget);
		}

		void unset() override
		{
			kit.UnsetContinuedBudget();
		}

	private:
		HPS::CurveAttributeKit & kit;
		bool _state;
		unsigned int _budget;
		QTreeWidget * tree;
	};

	class CurveAttributeKitViewDependentProperty : public SettableProperty
	{
	public:
		CurveAttributeKitViewDependentProperty(
			QTreeWidget * tree,
			HPS::CurveAttributeKit & kit)
			: SettableProperty("ViewDependent")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowViewDependent(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetViewDependent(_state);
		}

		void unset() override
		{
			kit.UnsetViewDependent();
		}

	private:
		HPS::CurveAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class CurveAttributeKitMaximumDeviationProperty : public SettableProperty
	{
	public:
		CurveAttributeKitMaximumDeviationProperty(
			QTreeWidget * tree,
			HPS::CurveAttributeKit & kit)
			: SettableProperty("MaximumDeviation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumDeviation(_deviation);
			if (!_isSet)
			{
				_deviation = 0.0f;
			}
			addChild(new FloatProperty("Deviation", _deviation));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumDeviation(_deviation);
		}

		void unset() override
		{
			kit.UnsetMaximumDeviation();
		}

	private:
		HPS::CurveAttributeKit & kit;
		float _deviation;
		QTreeWidget * tree;
	};

	class CurveAttributeKitMaximumAngleProperty : public SettableProperty
	{
	public:
		CurveAttributeKitMaximumAngleProperty(
			QTreeWidget * tree,
			HPS::CurveAttributeKit & kit)
			: SettableProperty("MaximumAngle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumAngle(_degrees);
			if (!_isSet)
			{
				_degrees = 0.0f;
			}
			addChild(new FloatProperty("Degrees", _degrees));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumAngle(_degrees);
		}

		void unset() override
		{
			kit.UnsetMaximumAngle();
		}

	private:
		HPS::CurveAttributeKit & kit;
		float _degrees;
		QTreeWidget * tree;
	};

	class CurveAttributeKitMaximumLengthProperty : public SettableProperty
	{
	public:
		CurveAttributeKitMaximumLengthProperty(
			QTreeWidget * tree,
			HPS::CurveAttributeKit & kit)
			: SettableProperty("MaximumLength")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumLength(_length);
			if (!_isSet)
			{
				_length = 0.0f;
			}
			addChild(new FloatProperty("Length", _length));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumLength(_length);
		}

		void unset() override
		{
			kit.UnsetMaximumLength();
		}

	private:
		HPS::CurveAttributeKit & kit;
		float _length;
		QTreeWidget * tree;
	};

	class CurveAttributeKitProperty : public RootProperty
	{
	public:
		CurveAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCurveAttribute(kit);
			auto prop_Budget = new CurveAttributeKitBudgetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Budget);
			prop_Budget->addSubItems();
			auto prop_ContinuedBudget = new CurveAttributeKitContinuedBudgetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ContinuedBudget);
			prop_ContinuedBudget->addSubItems();
			auto prop_ViewDependent = new CurveAttributeKitViewDependentProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ViewDependent);
			prop_ViewDependent->addSubItems();
			auto prop_MaximumDeviation = new CurveAttributeKitMaximumDeviationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumDeviation);
			prop_MaximumDeviation->addSubItems();
			auto prop_MaximumAngle = new CurveAttributeKitMaximumAngleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumAngle);
			prop_MaximumAngle->addSubItems();
			auto prop_MaximumLength = new CurveAttributeKitMaximumLengthProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumLength);
			prop_MaximumLength->addSubItems();
		}

		void Apply() override
		{
			key.UnsetCurveAttribute();
			key.SetCurveAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CurveAttributeKit kit;
	};

	class PBRMaterialKitBaseColorMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitBaseColorMapProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("BaseColorMap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBaseColorMap(_texture_name);
			if (!_isSet)
			{
				_texture_name = "texture_name";
			}
			addChild(new UTF8Property("Texture Name", _texture_name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBaseColorMap(_texture_name);
		}

		void unset() override
		{
			kit.UnsetBaseColorMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		QTreeWidget * tree;
	};

	class PBRMaterialKitNormalMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitNormalMapProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("NormalMap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowNormalMap(_texture_name);
			if (!_isSet)
			{
				_texture_name = "texture_name";
			}
			addChild(new UTF8Property("Texture Name", _texture_name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetNormalMap(_texture_name);
		}

		void unset() override
		{
			kit.UnsetNormalMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		QTreeWidget * tree;
	};

	class PBRMaterialKitEmissiveMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitEmissiveMapProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("EmissiveMap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEmissiveMap(_texture_name);
			if (!_isSet)
			{
				_texture_name = "texture_name";
			}
			addChild(new UTF8Property("Texture Name", _texture_name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetEmissiveMap(_texture_name);
		}

		void unset() override
		{
			kit.UnsetEmissiveMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		QTreeWidget * tree;
	};

	class PBRMaterialKitMetalnessMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitMetalnessMapProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("MetalnessMap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMetalnessMap(_texture_name, _channel);
			if (!_isSet)
			{
				_texture_name = "texture_name";
				_channel = HPS::Material::Texture::ChannelMapping::Red;
			}
			addChild(new UTF8Property("Texture Name", _texture_name));
			MaterialTextureChannelMappingProperty * enumObject_1 = new MaterialTextureChannelMappingProperty(tree, _channel);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMetalnessMap(_texture_name, _channel);
		}

		void unset() override
		{
			kit.UnsetMetalnessMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		HPS::Material::Texture::ChannelMapping _channel;
		QTreeWidget * tree;
	};

	class PBRMaterialKitRoughnessMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitRoughnessMapProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("RoughnessMap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRoughnessMap(_texture_name, _channel);
			if (!_isSet)
			{
				_texture_name = "texture_name";
				_channel = HPS::Material::Texture::ChannelMapping::Red;
			}
			addChild(new UTF8Property("Texture Name", _texture_name));
			MaterialTextureChannelMappingProperty * enumObject_1 = new MaterialTextureChannelMappingProperty(tree, _channel);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRoughnessMap(_texture_name, _channel);
		}

		void unset() override
		{
			kit.UnsetRoughnessMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		HPS::Material::Texture::ChannelMapping _channel;
		QTreeWidget * tree;
	};

	class PBRMaterialKitOcclusionMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitOcclusionMapProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("OcclusionMap")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOcclusionMap(_texture_name, _channel);
			if (!_isSet)
			{
				_texture_name = "texture_name";
				_channel = HPS::Material::Texture::ChannelMapping::Red;
			}
			addChild(new UTF8Property("Texture Name", _texture_name));
			MaterialTextureChannelMappingProperty * enumObject_1 = new MaterialTextureChannelMappingProperty(tree, _channel);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOcclusionMap(_texture_name, _channel);
		}

		void unset() override
		{
			kit.UnsetOcclusionMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		HPS::Material::Texture::ChannelMapping _channel;
		QTreeWidget * tree;
	};

	class PBRMaterialKitBaseColorFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitBaseColorFactorProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("BaseColorFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBaseColorFactor(_color);
			if (!_isSet)
			{
				_color = HPS::RGBAColor::Black();
			}
			addChild(new RGBAColorProperty("Color", _color));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBaseColorFactor(_color);
		}

		void unset() override
		{
			kit.UnsetBaseColorFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::RGBAColor _color;
		QTreeWidget * tree;
	};

	class PBRMaterialKitNormalFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitNormalFactorProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("NormalFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowNormalFactor(_factor);
			if (!_isSet)
			{
				_factor = 0.0f;
			}
			addChild(new FloatProperty("Factor", _factor));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetNormalFactor(_factor);
		}

		void unset() override
		{
			kit.UnsetNormalFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
		QTreeWidget * tree;
	};

	class PBRMaterialKitMetalnessFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitMetalnessFactorProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("MetalnessFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMetalnessFactor(_factor);
			if (!_isSet)
			{
				_factor = 0.0f;
			}
			addChild(new FloatProperty("Factor", _factor));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMetalnessFactor(_factor);
		}

		void unset() override
		{
			kit.UnsetMetalnessFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
		QTreeWidget * tree;
	};

	class PBRMaterialKitRoughnessFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitRoughnessFactorProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("RoughnessFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRoughnessFactor(_factor);
			if (!_isSet)
			{
				_factor = 0.0f;
			}
			addChild(new FloatProperty("Factor", _factor));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRoughnessFactor(_factor);
		}

		void unset() override
		{
			kit.UnsetRoughnessFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
		QTreeWidget * tree;
	};

	class PBRMaterialKitOcclusionFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitOcclusionFactorProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("OcclusionFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOcclusionFactor(_factor);
			if (!_isSet)
			{
				_factor = 0.0f;
			}
			addChild(new FloatProperty("Factor", _factor));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOcclusionFactor(_factor);
		}

		void unset() override
		{
			kit.UnsetOcclusionFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
		QTreeWidget * tree;
	};

	class PBRMaterialKitAlphaFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitAlphaFactorProperty(
			QTreeWidget * tree,
			HPS::PBRMaterialKit & kit)
			: SettableProperty("AlphaFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAlphaFactor(_factor, _mask);
			if (!_isSet)
			{
				_factor = 0.0f;
				_mask = true;
			}
			addChild(new FloatProperty("Factor", _factor));
			{
				auto boolProperty = new BoolProperty(tree, "Mask", _mask);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetAlphaFactor(_factor, _mask);
		}

		void unset() override
		{
			kit.UnsetAlphaFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
		bool _mask;
		QTreeWidget * tree;
	};

	class PBRMaterialKitProperty : public RootProperty
	{
	public:
		PBRMaterialKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowPBRMaterial(kit);
			auto prop_BaseColorMap = new PBRMaterialKitBaseColorMapProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BaseColorMap);
			prop_BaseColorMap->addSubItems();
			auto prop_NormalMap = new PBRMaterialKitNormalMapProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_NormalMap);
			prop_NormalMap->addSubItems();
			auto prop_EmissiveMap = new PBRMaterialKitEmissiveMapProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EmissiveMap);
			prop_EmissiveMap->addSubItems();
			auto prop_MetalnessMap = new PBRMaterialKitMetalnessMapProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MetalnessMap);
			prop_MetalnessMap->addSubItems();
			auto prop_RoughnessMap = new PBRMaterialKitRoughnessMapProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_RoughnessMap);
			prop_RoughnessMap->addSubItems();
			auto prop_OcclusionMap = new PBRMaterialKitOcclusionMapProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_OcclusionMap);
			prop_OcclusionMap->addSubItems();
			auto prop_BaseColorFactor = new PBRMaterialKitBaseColorFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BaseColorFactor);
			prop_BaseColorFactor->addSubItems();
			auto prop_NormalFactor = new PBRMaterialKitNormalFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_NormalFactor);
			prop_NormalFactor->addSubItems();
			auto prop_MetalnessFactor = new PBRMaterialKitMetalnessFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MetalnessFactor);
			prop_MetalnessFactor->addSubItems();
			auto prop_RoughnessFactor = new PBRMaterialKitRoughnessFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_RoughnessFactor);
			prop_RoughnessFactor->addSubItems();
			auto prop_OcclusionFactor = new PBRMaterialKitOcclusionFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_OcclusionFactor);
			prop_OcclusionFactor->addSubItems();
			auto prop_AlphaFactor = new PBRMaterialKitAlphaFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_AlphaFactor);
			prop_AlphaFactor->addSubItems();
		}

		void Apply() override
		{
			key.UnsetPBRMaterial();
			key.SetPBRMaterial(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::PBRMaterialKit kit;
	};

	class MarkerKitPointProperty : public BaseProperty
	{
	public:
		MarkerKitPointProperty(
			QTreeWidget * tree,
			HPS::MarkerKit & kit)
			: BaseProperty("Point")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowPoint(_point);
			addChild(new PointProperty("Point", _point));
		}
		void onChildChanged() override
		{
			kit.SetPoint(_point);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::MarkerKit & kit;
		HPS::Point _point;
		QTreeWidget * tree;
	};

	class MarkerKitProperty : public RootProperty
	{
	public:
		MarkerKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::MarkerKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Point = new MarkerKitPointProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Point);
			prop_Point->addSubItems();
			ctrl->addChild(new MarkerKitPriorityProperty(kit));
			ctrl->addChild(new MarkerKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::MarkerKey key;
		HPS::MarkerKit kit;
	};

	class DistantLightKitDirectionProperty : public BaseProperty
	{
	public:
		DistantLightKitDirectionProperty(
			QTreeWidget * tree,
			HPS::DistantLightKit & kit)
			: BaseProperty("Direction")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowDirection(_vector);
			addChild(new VectorProperty("Vector", _vector));
		}
		void onChildChanged() override
		{
			kit.SetDirection(_vector);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::DistantLightKit & kit;
		HPS::Vector _vector;
		QTreeWidget * tree;
	};

	class DistantLightKitCameraRelativeProperty : public BaseProperty
	{
	public:
		DistantLightKitCameraRelativeProperty(
			QTreeWidget * tree,
			HPS::DistantLightKit & kit)
			: BaseProperty("CameraRelative")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowCameraRelative(_state);
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
		}
		void onChildChanged() override
		{
			kit.SetCameraRelative(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::DistantLightKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class DistantLightKitProperty : public RootProperty
	{
	public:
		DistantLightKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::DistantLightKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Direction = new DistantLightKitDirectionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Direction);
			prop_Direction->addSubItems();
			auto prop_Color = new DistantLightKitColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Color);
			prop_Color->addSubItems();
			auto prop_CameraRelative = new DistantLightKitCameraRelativeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraRelative);
			prop_CameraRelative->addSubItems();
			ctrl->addChild(new DistantLightKitPriorityProperty(kit));
			ctrl->addChild(new DistantLightKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::DistantLightKey key;
		HPS::DistantLightKit kit;
	};

	class CuttingSectionAttributeKitCuttingLevelProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitCuttingLevelProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty("CuttingLevel")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCuttingLevel(_level);
			if (!_isSet)
			{
				_level = HPS::CuttingSection::CuttingLevel::Global;
			}
			CuttingSectionCuttingLevelProperty * enumObject_0 = new CuttingSectionCuttingLevelProperty(tree, _level);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCuttingLevel(_level);
		}

		void unset() override
		{
			kit.UnsetCuttingLevel();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::CuttingLevel _level;
		QTreeWidget * tree;
	};

	class CuttingSectionAttributeKitCappingLevelProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitCappingLevelProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty("CappingLevel")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCappingLevel(_level);
			if (!_isSet)
			{
				_level = HPS::CuttingSection::CappingLevel::Segment;
			}
			CuttingSectionCappingLevelProperty * enumObject_0 = new CuttingSectionCappingLevelProperty(tree, _level);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCappingLevel(_level);
		}

		void unset() override
		{
			kit.UnsetCappingLevel();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::CappingLevel _level;
		QTreeWidget * tree;
	};

	class CuttingSectionAttributeKitCappingUsageProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitCappingUsageProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty("CappingUsage")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCappingUsage(_usage);
			if (!_isSet)
			{
				_usage = HPS::CuttingSection::CappingUsage::Visibility;
			}
			CuttingSectionCappingUsageProperty * enumObject_0 = new CuttingSectionCappingUsageProperty(tree, _usage);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCappingUsage(_usage);
		}

		void unset() override
		{
			kit.UnsetCappingUsage();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::CappingUsage _usage;
		QTreeWidget * tree;
	};

	class CuttingSectionAttributeKitMaterialPreferenceProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitMaterialPreferenceProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty("MaterialPreference")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaterialPreference(_preference);
			if (!_isSet)
			{
				_preference = HPS::CuttingSection::MaterialPreference::Explicit;
			}
			CuttingSectionMaterialPreferenceProperty * enumObject_0 = new CuttingSectionMaterialPreferenceProperty(tree, _preference);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaterialPreference(_preference);
		}

		void unset() override
		{
			kit.UnsetMaterialPreference();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::MaterialPreference _preference;
		QTreeWidget * tree;
	};

	class CuttingSectionAttributeKitEdgeWeightProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitEdgeWeightProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty("EdgeWeight")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEdgeWeight(_weight, _units);
			if (!_isSet)
			{
				_weight = 0.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			addChild(new FloatProperty("Weight", _weight));
			LineSizeUnitsProperty * enumObject_1 = new LineSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetEdgeWeight(_weight, _units);
		}

		void unset() override
		{
			kit.UnsetEdgeWeight();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		float _weight;
		HPS::Line::SizeUnits _units;
		QTreeWidget * tree;
	};

	class CuttingSectionAttributeKitToleranceProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitToleranceProperty(
			QTreeWidget * tree,
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty("Tolerance")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowTolerance(_tolerance, _units);
			if (!_isSet)
			{
				_tolerance = 0.0f;
				_units = HPS::CuttingSection::ToleranceUnits::WorldSpace;
			}
			addChild(new FloatProperty("Tolerance", _tolerance));
			CuttingSectionToleranceUnitsProperty * enumObject_1 = new CuttingSectionToleranceUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTolerance(_tolerance, _units);
		}

		void unset() override
		{
			kit.UnsetTolerance();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		float _tolerance;
		HPS::CuttingSection::ToleranceUnits _units;
		QTreeWidget * tree;
	};

	class CuttingSectionAttributeKitProperty : public RootProperty
	{
	public:
		CuttingSectionAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCuttingSectionAttribute(kit);
			auto prop_CuttingLevel = new CuttingSectionAttributeKitCuttingLevelProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CuttingLevel);
			prop_CuttingLevel->addSubItems();
			auto prop_CappingLevel = new CuttingSectionAttributeKitCappingLevelProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CappingLevel);
			prop_CappingLevel->addSubItems();
			auto prop_CappingUsage = new CuttingSectionAttributeKitCappingUsageProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CappingUsage);
			prop_CappingUsage->addSubItems();
			auto prop_MaterialPreference = new CuttingSectionAttributeKitMaterialPreferenceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaterialPreference);
			prop_MaterialPreference->addSubItems();
			auto prop_EdgeWeight = new CuttingSectionAttributeKitEdgeWeightProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EdgeWeight);
			prop_EdgeWeight->addSubItems();
			auto prop_Tolerance = new CuttingSectionAttributeKitToleranceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Tolerance);
			prop_Tolerance->addSubItems();
		}

		void Apply() override
		{
			key.UnsetCuttingSectionAttribute();
			key.SetCuttingSectionAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CuttingSectionAttributeKit kit;
	};

	class CylinderAttributeKitTessellationProperty : public SettableProperty
	{
	public:
		CylinderAttributeKitTessellationProperty(
			QTreeWidget * tree,
			HPS::CylinderAttributeKit & kit)
			: SettableProperty("Tessellation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			size_t _facets_st;
			bool _isSet = this->kit.ShowTessellation(_facets_st);
			_facets = static_cast<unsigned int>(_facets_st);
			if (!_isSet)
			{
				_facets = 0;
			}
			addChild(new UnsignedIntProperty("Facets", _facets));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTessellation(_facets);
		}

		void unset() override
		{
			kit.UnsetTessellation();
		}

	private:
		HPS::CylinderAttributeKit & kit;
		unsigned int _facets;
		QTreeWidget * tree;
	};

	class CylinderAttributeKitOrientationProperty : public SettableProperty
	{
	public:
		CylinderAttributeKitOrientationProperty(
			QTreeWidget * tree,
			HPS::CylinderAttributeKit & kit)
			: SettableProperty("Orientation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOrientation(_orientation);
			if (!_isSet)
			{
				_orientation = HPS::Cylinder::Orientation::Default;
			}
			CylinderOrientationProperty * enumObject_0 = new CylinderOrientationProperty(tree, _orientation);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOrientation(_orientation);
		}

		void unset() override
		{
			kit.UnsetOrientation();
		}

	private:
		HPS::CylinderAttributeKit & kit;
		HPS::Cylinder::Orientation _orientation;
		QTreeWidget * tree;
	};

	class CylinderAttributeKitProperty : public RootProperty
	{
	public:
		CylinderAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCylinderAttribute(kit);
			auto prop_Tessellation = new CylinderAttributeKitTessellationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Tessellation);
			prop_Tessellation->addSubItems();
			auto prop_Orientation = new CylinderAttributeKitOrientationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Orientation);
			prop_Orientation->addSubItems();
		}

		void Apply() override
		{
			key.UnsetCylinderAttribute();
			key.SetCylinderAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CylinderAttributeKit kit;
	};

	class SphereKitCenterProperty : public BaseProperty
	{
	public:
		SphereKitCenterProperty(
			QTreeWidget * tree,
			HPS::SphereKit & kit)
			: BaseProperty("Center")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowCenter(_center);
			addChild(new PointProperty("Center", _center));
		}
		void onChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SphereKit & kit;
		HPS::Point _center;
		QTreeWidget * tree;
	};

	class SphereKitRadiusProperty : public BaseProperty
	{
	public:
		SphereKitRadiusProperty(
			QTreeWidget * tree,
			HPS::SphereKit & kit)
			: BaseProperty("Radius")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowRadius(_radius);
			addChild(new FloatProperty("Radius", _radius));
		}
		void onChildChanged() override
		{
			kit.SetRadius(_radius);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SphereKit & kit;
		float _radius;
		QTreeWidget * tree;
	};

	class SphereKitBasisProperty : public BaseProperty
	{
	public:
		SphereKitBasisProperty(
			QTreeWidget * tree,
			HPS::SphereKit & kit)
			: BaseProperty("Basis")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowBasis(_vertical, _horizontal);
			addChild(new VectorProperty("Vertical", _vertical));
			addChild(new VectorProperty("Horizontal", _horizontal));
		}
		void onChildChanged() override
		{
			kit.SetBasis(_vertical, _horizontal);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SphereKit & kit;
		HPS::Vector _vertical;
		HPS::Vector _horizontal;
		QTreeWidget * tree;
	};

	class SphereKitProperty : public RootProperty
	{
	public:
		SphereKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SphereKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Center = new SphereKitCenterProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Center);
			prop_Center->addSubItems();
			auto prop_Radius = new SphereKitRadiusProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Radius);
			prop_Radius->addSubItems();
			auto prop_Basis = new SphereKitBasisProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Basis);
			prop_Basis->addSubItems();
			ctrl->addChild(new SphereKitPriorityProperty(kit));
			ctrl->addChild(new SphereKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::SphereKey key;
		HPS::SphereKit kit;
	};

	class SphereAttributeKitTessellationProperty : public SettableProperty
	{
	public:
		SphereAttributeKitTessellationProperty(
			QTreeWidget * tree,
			HPS::SphereAttributeKit & kit)
			: SettableProperty("Tessellation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			size_t _facets_st;
			bool _isSet = this->kit.ShowTessellation(_facets_st);
			_facets = static_cast<unsigned int>(_facets_st);
			if (!_isSet)
			{
				_facets = 0;
			}
			addChild(new UnsignedIntProperty("Facets", _facets));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTessellation(_facets);
		}

		void unset() override
		{
			kit.UnsetTessellation();
		}

	private:
		HPS::SphereAttributeKit & kit;
		unsigned int _facets;
		QTreeWidget * tree;
	};

	class SphereAttributeKitProperty : public RootProperty
	{
	public:
		SphereAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSphereAttribute(kit);
			auto prop_Tessellation = new SphereAttributeKitTessellationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Tessellation);
			prop_Tessellation->addSubItems();
		}

		void Apply() override
		{
			key.UnsetSphereAttribute();
			key.SetSphereAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::SphereAttributeKit kit;
	};

	class CharacterAttributeKitFontProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitFontProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Font")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFont(_font);
			if (!_isSet)
			{
				_font = "font";
			}
			addChild(new UTF8Property("Font", _font));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFont(_font);
		}

		void unset() override
		{
			kit.UnsetFont();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		HPS::UTF8 _font;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitColorProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitColorProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Color")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowColor(_color);
			if (!_isSet)
			{
				_color = HPS::RGBColor::Black();
			}
			addChild(new RGBColorProperty("Color", _color));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetColor(_color);
		}

		void unset() override
		{
			kit.UnsetColor();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		HPS::RGBColor _color;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitSizeProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitSizeProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Size")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSize(_size, _units);
			if (!_isSet)
			{
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			addChild(new FloatProperty("Size", _size));
			TextSizeUnitsProperty * enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSize(_size, _units);
		}

		void unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		float _size;
		HPS::Text::SizeUnits _units;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitHorizontalOffsetProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitHorizontalOffsetProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("HorizontalOffset")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowHorizontalOffset(_offset, _units);
			if (!_isSet)
			{
				_offset = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			addChild(new FloatProperty("Offset", _offset));
			TextSizeUnitsProperty * enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetHorizontalOffset(_offset, _units);
		}

		void unset() override
		{
			kit.UnsetHorizontalOffset();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		float _offset;
		HPS::Text::SizeUnits _units;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitVerticalOffsetProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitVerticalOffsetProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("VerticalOffset")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVerticalOffset(_offset, _units);
			if (!_isSet)
			{
				_offset = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			addChild(new FloatProperty("Offset", _offset));
			TextSizeUnitsProperty * enumObject_1 = new TextSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVerticalOffset(_offset, _units);
		}

		void unset() override
		{
			kit.UnsetVerticalOffset();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		float _offset;
		HPS::Text::SizeUnits _units;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitRotationProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitRotationProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Rotation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRotation(_rotation, _fixed);
			if (!_isSet)
			{
				_rotation = 0.0f;
				_fixed = true;
			}
			addChild(new FloatProperty("Rotation", _rotation));
			{
				auto boolProperty = new BoolProperty(tree, "Fixed", _fixed);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRotation(_rotation, _fixed);
		}

		void unset() override
		{
			kit.UnsetRotation();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		float _rotation;
		bool _fixed;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitWidthScaleProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitWidthScaleProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("WidthScale")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWidthScale(_scale);
			if (!_isSet)
			{
				_scale = 0.0f;
			}
			addChild(new FloatProperty("Scale", _scale));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetWidthScale(_scale);
		}

		void unset() override
		{
			kit.UnsetWidthScale();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		float _scale;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitSlantProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitSlantProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Slant")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSlant(_slant);
			if (!_isSet)
			{
				_slant = 0.0f;
			}
			addChild(new FloatProperty("Slant", _slant));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSlant(_slant);
		}

		void unset() override
		{
			kit.UnsetSlant();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		float _slant;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitInvisibleProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitInvisibleProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Invisible")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowInvisible();
			if (!_isSet)
			{
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetInvisible();
		}

		void unset() override
		{
			kit.UnsetInvisible();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitOmittedProperty : public SettableProperty
	{
	public:
		CharacterAttributeKitOmittedProperty(
			QTreeWidget * tree,
			HPS::CharacterAttributeKit & kit)
			: SettableProperty("Omitted")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOmitted();
			if (!_isSet)
			{
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOmitted();
		}

		void unset() override
		{
			kit.UnsetOmitted();
		}

	private:
		HPS::CharacterAttributeKit & kit;
		QTreeWidget * tree;
	};

	class CharacterAttributeKitProperty : public RootProperty
	{
	public:
		CharacterAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCharacterAttribute(kit);
			auto prop_Font = new CharacterAttributeKitFontProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Font);
			prop_Font->addSubItems();
			auto prop_Color = new CharacterAttributeKitColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Color);
			prop_Color->addSubItems();
			auto prop_Size = new CharacterAttributeKitSizeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Size);
			prop_Size->addSubItems();
			auto prop_HorizontalOffset = new CharacterAttributeKitHorizontalOffsetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_HorizontalOffset);
			prop_HorizontalOffset->addSubItems();
			auto prop_VerticalOffset = new CharacterAttributeKitVerticalOffsetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VerticalOffset);
			prop_VerticalOffset->addSubItems();
			auto prop_Rotation = new CharacterAttributeKitRotationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Rotation);
			prop_Rotation->addSubItems();
			auto prop_WidthScale = new CharacterAttributeKitWidthScaleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_WidthScale);
			prop_WidthScale->addSubItems();
			auto prop_Slant = new CharacterAttributeKitSlantProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Slant);
			prop_Slant->addSubItems();
			auto prop_Invisible = new CharacterAttributeKitInvisibleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Invisible);
			prop_Invisible->addSubItems();
			auto prop_Omitted = new CharacterAttributeKitOmittedProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Omitted);
			prop_Omitted->addSubItems();
		}

		void Apply() override
		{
			key.UnsetCharacterAttribute();
			key.SetCharacterAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CharacterAttributeKit kit;
	};

	class CircleKitCenterProperty : public BaseProperty
	{
	public:
		CircleKitCenterProperty(
			QTreeWidget * tree,
			HPS::CircleKit & kit)
			: BaseProperty("Center")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowCenter(_center);
			addChild(new PointProperty("Center", _center));
		}
		void onChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircleKit & kit;
		HPS::Point _center;
		QTreeWidget * tree;
	};

	class CircleKitRadiusProperty : public BaseProperty
	{
	public:
		CircleKitRadiusProperty(
			QTreeWidget * tree,
			HPS::CircleKit & kit)
			: BaseProperty("Radius")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowRadius(_radius);
			addChild(new FloatProperty("Radius", _radius));
		}
		void onChildChanged() override
		{
			kit.SetRadius(_radius);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircleKit & kit;
		float _radius;
		QTreeWidget * tree;
	};

	class CircleKitNormalProperty : public BaseProperty
	{
	public:
		CircleKitNormalProperty(
			QTreeWidget * tree,
			HPS::CircleKit & kit)
			: BaseProperty("Normal")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowNormal(_normal);
			addChild(new VectorProperty("Normal", _normal));
		}
		void onChildChanged() override
		{
			kit.SetNormal(_normal);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircleKit & kit;
		HPS::Vector _normal;
		QTreeWidget * tree;
	};

	class CircleKitProperty : public RootProperty
	{
	public:
		CircleKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::CircleKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Center = new CircleKitCenterProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Center);
			prop_Center->addSubItems();
			auto prop_Radius = new CircleKitRadiusProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Radius);
			prop_Radius->addSubItems();
			auto prop_Normal = new CircleKitNormalProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Normal);
			prop_Normal->addSubItems();
			ctrl->addChild(new CircleKitPriorityProperty(kit));
			ctrl->addChild(new CircleKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CircleKey key;
		HPS::CircleKit kit;
	};

	class CircularArcKitStartProperty : public BaseProperty
	{
	public:
		CircularArcKitStartProperty(
			QTreeWidget * tree,
			HPS::CircularArcKit & kit)
			: BaseProperty("Start")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowStart(_start);
			addChild(new PointProperty("Start", _start));
		}
		void onChildChanged() override
		{
			kit.SetStart(_start);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircularArcKit & kit;
		HPS::Point _start;
		QTreeWidget * tree;
	};

	class CircularArcKitMiddleProperty : public BaseProperty
	{
	public:
		CircularArcKitMiddleProperty(
			QTreeWidget * tree,
			HPS::CircularArcKit & kit)
			: BaseProperty("Middle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowMiddle(_middle);
			addChild(new PointProperty("Middle", _middle));
		}
		void onChildChanged() override
		{
			kit.SetMiddle(_middle);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircularArcKit & kit;
		HPS::Point _middle;
		QTreeWidget * tree;
	};

	class CircularArcKitEndProperty : public BaseProperty
	{
	public:
		CircularArcKitEndProperty(
			QTreeWidget * tree,
			HPS::CircularArcKit & kit)
			: BaseProperty("End")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowEnd(_end);
			addChild(new PointProperty("End", _end));
		}
		void onChildChanged() override
		{
			kit.SetEnd(_end);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircularArcKit & kit;
		HPS::Point _end;
		QTreeWidget * tree;
	};

	class CircularArcKitProperty : public RootProperty
	{
	public:
		CircularArcKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::CircularArcKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Start = new CircularArcKitStartProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Start);
			prop_Start->addSubItems();
			auto prop_Middle = new CircularArcKitMiddleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Middle);
			prop_Middle->addSubItems();
			auto prop_End = new CircularArcKitEndProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_End);
			prop_End->addSubItems();
			ctrl->addChild(new CircularArcKitPriorityProperty(kit));
			ctrl->addChild(new CircularArcKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CircularArcKey key;
		HPS::CircularArcKit kit;
	};

	class CircularWedgeKitStartProperty : public BaseProperty
	{
	public:
		CircularWedgeKitStartProperty(
			QTreeWidget * tree,
			HPS::CircularWedgeKit & kit)
			: BaseProperty("Start")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowStart(_start);
			addChild(new PointProperty("Start", _start));
		}
		void onChildChanged() override
		{
			kit.SetStart(_start);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircularWedgeKit & kit;
		HPS::Point _start;
		QTreeWidget * tree;
	};

	class CircularWedgeKitMiddleProperty : public BaseProperty
	{
	public:
		CircularWedgeKitMiddleProperty(
			QTreeWidget * tree,
			HPS::CircularWedgeKit & kit)
			: BaseProperty("Middle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowMiddle(_middle);
			addChild(new PointProperty("Middle", _middle));
		}
		void onChildChanged() override
		{
			kit.SetMiddle(_middle);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircularWedgeKit & kit;
		HPS::Point _middle;
		QTreeWidget * tree;
	};

	class CircularWedgeKitEndProperty : public BaseProperty
	{
	public:
		CircularWedgeKitEndProperty(
			QTreeWidget * tree,
			HPS::CircularWedgeKit & kit)
			: BaseProperty("End")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowEnd(_end);
			addChild(new PointProperty("End", _end));
		}
		void onChildChanged() override
		{
			kit.SetEnd(_end);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::CircularWedgeKit & kit;
		HPS::Point _end;
		QTreeWidget * tree;
	};

	class CircularWedgeKitProperty : public RootProperty
	{
	public:
		CircularWedgeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::CircularWedgeKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Start = new CircularWedgeKitStartProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Start);
			prop_Start->addSubItems();
			auto prop_Middle = new CircularWedgeKitMiddleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Middle);
			prop_Middle->addSubItems();
			auto prop_End = new CircularWedgeKitEndProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_End);
			prop_End->addSubItems();
			ctrl->addChild(new CircularWedgeKitPriorityProperty(kit));
			ctrl->addChild(new CircularWedgeKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CircularWedgeKey key;
		HPS::CircularWedgeKit kit;
	};

	class InfiniteLineKitFirstProperty : public BaseProperty
	{
	public:
		InfiniteLineKitFirstProperty(
			QTreeWidget * tree,
			HPS::InfiniteLineKit & kit)
			: BaseProperty("First")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowFirst(_first);
			addChild(new PointProperty("First", _first));
		}
		void onChildChanged() override
		{
			kit.SetFirst(_first);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::InfiniteLineKit & kit;
		HPS::Point _first;
		QTreeWidget * tree;
	};

	class InfiniteLineKitSecondProperty : public BaseProperty
	{
	public:
		InfiniteLineKitSecondProperty(
			QTreeWidget * tree,
			HPS::InfiniteLineKit & kit)
			: BaseProperty("Second")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowSecond(_second);
			addChild(new PointProperty("Second", _second));
		}
		void onChildChanged() override
		{
			kit.SetSecond(_second);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::InfiniteLineKit & kit;
		HPS::Point _second;
		QTreeWidget * tree;
	};

	class InfiniteLineKitTypeProperty : public BaseProperty
	{
	public:
		InfiniteLineKitTypeProperty(
			QTreeWidget * tree,
			HPS::InfiniteLineKit & kit)
			: BaseProperty("Type")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowType(_type);
			InfiniteLineTypeProperty * enumObject_0 = new InfiniteLineTypeProperty(tree, _type);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
		}
		void onChildChanged() override
		{
			kit.SetType(_type);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::InfiniteLineKit & kit;
		HPS::InfiniteLine::Type _type;
		QTreeWidget * tree;
	};

	class InfiniteLineKitProperty : public RootProperty
	{
	public:
		InfiniteLineKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::InfiniteLineKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_First = new InfiniteLineKitFirstProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_First);
			prop_First->addSubItems();
			auto prop_Second = new InfiniteLineKitSecondProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Second);
			prop_Second->addSubItems();
			auto prop_Type = new InfiniteLineKitTypeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Type);
			prop_Type->addSubItems();
			ctrl->addChild(new InfiniteLineKitPriorityProperty(kit));
			ctrl->addChild(new InfiniteLineKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::InfiniteLineKey key;
		HPS::InfiniteLineKit kit;
	};

	class SpotlightKitPositionProperty : public BaseProperty
	{
	public:
		SpotlightKitPositionProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseProperty("Position")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowPosition(_position);
			addChild(new PointProperty("Position", _position));
		}
		void onChildChanged() override
		{
			kit.SetPosition(_position);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		HPS::Point _position;
		QTreeWidget * tree;
	};

	class SpotlightKitTargetProperty : public BaseProperty
	{
	public:
		SpotlightKitTargetProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseProperty("Target")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowTarget(_target);
			addChild(new PointProperty("Target", _target));
		}
		void onChildChanged() override
		{
			kit.SetTarget(_target);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		HPS::Point _target;
		QTreeWidget * tree;
	};

	class SpotlightKitOuterConeProperty : public BaseProperty
	{
	public:
		SpotlightKitOuterConeProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseProperty("OuterCone")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowOuterCone(_size, _units);
			addChild(new FloatProperty("Size", _size));
			SpotlightOuterConeUnitsProperty * enumObject_1 = new SpotlightOuterConeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
		}
		void onChildChanged() override
		{
			kit.SetOuterCone(_size, _units);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		float _size;
		HPS::Spotlight::OuterConeUnits _units;
		QTreeWidget * tree;
	};

	class SpotlightKitInnerConeProperty : public BaseProperty
	{
	public:
		SpotlightKitInnerConeProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseProperty("InnerCone")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowInnerCone(_size, _units);
			addChild(new FloatProperty("Size", _size));
			SpotlightInnerConeUnitsProperty * enumObject_1 = new SpotlightInnerConeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
		}
		void onChildChanged() override
		{
			kit.SetInnerCone(_size, _units);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		float _size;
		HPS::Spotlight::InnerConeUnits _units;
		QTreeWidget * tree;
	};

	class SpotlightKitConcentrationProperty : public BaseProperty
	{
	public:
		SpotlightKitConcentrationProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseProperty("Concentration")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowConcentration(_concentration);
			addChild(new FloatProperty("Concentration", _concentration));
		}
		void onChildChanged() override
		{
			kit.SetConcentration(_concentration);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		float _concentration;
		QTreeWidget * tree;
	};

	class SpotlightKitCameraRelativeProperty : public BaseProperty
	{
	public:
		SpotlightKitCameraRelativeProperty(
			QTreeWidget * tree,
			HPS::SpotlightKit & kit)
			: BaseProperty("CameraRelative")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowCameraRelative(_state);
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
		}
		void onChildChanged() override
		{
			kit.SetCameraRelative(_state);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class SpotlightKitProperty : public RootProperty
	{
	public:
		SpotlightKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SpotlightKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Position = new SpotlightKitPositionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Position);
			prop_Position->addSubItems();
			auto prop_Target = new SpotlightKitTargetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Target);
			prop_Target->addSubItems();
			auto prop_Color = new SpotlightKitColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Color);
			prop_Color->addSubItems();
			auto prop_OuterCone = new SpotlightKitOuterConeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_OuterCone);
			prop_OuterCone->addSubItems();
			auto prop_InnerCone = new SpotlightKitInnerConeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_InnerCone);
			prop_InnerCone->addSubItems();
			auto prop_Concentration = new SpotlightKitConcentrationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Concentration);
			prop_Concentration->addSubItems();
			auto prop_CameraRelative = new SpotlightKitCameraRelativeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraRelative);
			prop_CameraRelative->addSubItems();
			ctrl->addChild(new SpotlightKitPriorityProperty(kit));
			ctrl->addChild(new SpotlightKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::SpotlightKey key;
		HPS::SpotlightKit kit;
	};

	class EllipseKitCenterProperty : public BaseProperty
	{
	public:
		EllipseKitCenterProperty(
			QTreeWidget * tree,
			HPS::EllipseKit & kit)
			: BaseProperty("Center")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowCenter(_center);
			addChild(new PointProperty("Center", _center));
		}
		void onChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipseKit & kit;
		HPS::Point _center;
		QTreeWidget * tree;
	};

	class EllipseKitMajorProperty : public BaseProperty
	{
	public:
		EllipseKitMajorProperty(
			QTreeWidget * tree,
			HPS::EllipseKit & kit)
			: BaseProperty("Major")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowMajor(_major);
			addChild(new PointProperty("Major", _major));
		}
		void onChildChanged() override
		{
			kit.SetMajor(_major);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipseKit & kit;
		HPS::Point _major;
		QTreeWidget * tree;
	};

	class EllipseKitMinorProperty : public BaseProperty
	{
	public:
		EllipseKitMinorProperty(
			QTreeWidget * tree,
			HPS::EllipseKit & kit)
			: BaseProperty("Minor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowMinor(_minor);
			addChild(new PointProperty("Minor", _minor));
		}
		void onChildChanged() override
		{
			kit.SetMinor(_minor);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipseKit & kit;
		HPS::Point _minor;
		QTreeWidget * tree;
	};

	class EllipseKitProperty : public RootProperty
	{
	public:
		EllipseKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::EllipseKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Center = new EllipseKitCenterProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Center);
			prop_Center->addSubItems();
			auto prop_Major = new EllipseKitMajorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Major);
			prop_Major->addSubItems();
			auto prop_Minor = new EllipseKitMinorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Minor);
			prop_Minor->addSubItems();
			ctrl->addChild(new EllipseKitPriorityProperty(kit));
			ctrl->addChild(new EllipseKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::EllipseKey key;
		HPS::EllipseKit kit;
	};

	class EllipticalArcKitCenterProperty : public BaseProperty
	{
	public:
		EllipticalArcKitCenterProperty(
			QTreeWidget * tree,
			HPS::EllipticalArcKit & kit)
			: BaseProperty("Center")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowCenter(_center);
			addChild(new PointProperty("Center", _center));
		}
		void onChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		HPS::Point _center;
		QTreeWidget * tree;
	};

	class EllipticalArcKitMajorProperty : public BaseProperty
	{
	public:
		EllipticalArcKitMajorProperty(
			QTreeWidget * tree,
			HPS::EllipticalArcKit & kit)
			: BaseProperty("Major")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowMajor(_major);
			addChild(new PointProperty("Major", _major));
		}
		void onChildChanged() override
		{
			kit.SetMajor(_major);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		HPS::Point _major;
		QTreeWidget * tree;
	};

	class EllipticalArcKitMinorProperty : public BaseProperty
	{
	public:
		EllipticalArcKitMinorProperty(
			QTreeWidget * tree,
			HPS::EllipticalArcKit & kit)
			: BaseProperty("Minor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowMinor(_minor);
			addChild(new PointProperty("Minor", _minor));
		}
		void onChildChanged() override
		{
			kit.SetMinor(_minor);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		HPS::Point _minor;
		QTreeWidget * tree;
	};

	class EllipticalArcKitStartProperty : public BaseProperty
	{
	public:
		EllipticalArcKitStartProperty(
			QTreeWidget * tree,
			HPS::EllipticalArcKit & kit)
			: BaseProperty("Start")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowStart(_start);
			addChild(new FloatProperty("Start", _start));
		}
		void onChildChanged() override
		{
			kit.SetStart(_start);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		float _start;
		QTreeWidget * tree;
	};

	class EllipticalArcKitEndProperty : public BaseProperty
	{
	public:
		EllipticalArcKitEndProperty(
			QTreeWidget * tree,
			HPS::EllipticalArcKit & kit)
			: BaseProperty("End")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowEnd(_end);
			addChild(new FloatProperty("End", _end));
		}
		void onChildChanged() override
		{
			kit.SetEnd(_end);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		float _end;
		QTreeWidget * tree;
	};

	class EllipticalArcKitProperty : public RootProperty
	{
	public:
		EllipticalArcKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::EllipticalArcKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Center = new EllipticalArcKitCenterProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Center);
			prop_Center->addSubItems();
			auto prop_Major = new EllipticalArcKitMajorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Major);
			prop_Major->addSubItems();
			auto prop_Minor = new EllipticalArcKitMinorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Minor);
			prop_Minor->addSubItems();
			auto prop_Start = new EllipticalArcKitStartProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Start);
			prop_Start->addSubItems();
			auto prop_End = new EllipticalArcKitEndProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_End);
			prop_End->addSubItems();
			ctrl->addChild(new EllipticalArcKitPriorityProperty(kit));
			ctrl->addChild(new EllipticalArcKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::EllipticalArcKey key;
		HPS::EllipticalArcKit kit;
	};

	class NURBSSurfaceAttributeKitBudgetProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitBudgetProperty(
			QTreeWidget * tree,
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty("Budget")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			size_t _budget_st;
			bool _isSet = this->kit.ShowBudget(_budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!_isSet)
			{
				_budget = 0;
			}
			addChild(new UnsignedIntProperty("Budget", _budget));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBudget(_budget);
		}

		void unset() override
		{
			kit.UnsetBudget();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		unsigned int _budget;
		QTreeWidget * tree;
	};

	class NURBSSurfaceAttributeKitMaximumDeviationProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumDeviationProperty(
			QTreeWidget * tree,
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty("MaximumDeviation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumDeviation(_deviation);
			if (!_isSet)
			{
				_deviation = 0.0f;
			}
			addChild(new FloatProperty("Deviation", _deviation));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumDeviation(_deviation);
		}

		void unset() override
		{
			kit.UnsetMaximumDeviation();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _deviation;
		QTreeWidget * tree;
	};

	class NURBSSurfaceAttributeKitMaximumAngleProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumAngleProperty(
			QTreeWidget * tree,
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty("MaximumAngle")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumAngle(_degrees);
			if (!_isSet)
			{
				_degrees = 0.0f;
			}
			addChild(new FloatProperty("Degrees", _degrees));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumAngle(_degrees);
		}

		void unset() override
		{
			kit.UnsetMaximumAngle();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _degrees;
		QTreeWidget * tree;
	};

	class NURBSSurfaceAttributeKitMaximumWidthProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumWidthProperty(
			QTreeWidget * tree,
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty("MaximumWidth")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumWidth(_width);
			if (!_isSet)
			{
				_width = 0.0f;
			}
			addChild(new FloatProperty("Width", _width));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumWidth(_width);
		}

		void unset() override
		{
			kit.UnsetMaximumWidth();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _width;
		QTreeWidget * tree;
	};

	class NURBSSurfaceAttributeKitTrimBudgetProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitTrimBudgetProperty(
			QTreeWidget * tree,
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty("TrimBudget")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			size_t _budget_st;
			bool _isSet = this->kit.ShowTrimBudget(_budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!_isSet)
			{
				_budget = 0;
			}
			addChild(new UnsignedIntProperty("Budget", _budget));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTrimBudget(_budget);
		}

		void unset() override
		{
			kit.UnsetTrimBudget();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		unsigned int _budget;
		QTreeWidget * tree;
	};

	class NURBSSurfaceAttributeKitMaximumTrimDeviationProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumTrimDeviationProperty(
			QTreeWidget * tree,
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty("MaximumTrimDeviation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMaximumTrimDeviation(_deviation);
			if (!_isSet)
			{
				_deviation = 0.0f;
			}
			addChild(new FloatProperty("Deviation", _deviation));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMaximumTrimDeviation(_deviation);
		}

		void unset() override
		{
			kit.UnsetMaximumTrimDeviation();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _deviation;
		QTreeWidget * tree;
	};

	class NURBSSurfaceAttributeKitProperty : public RootProperty
	{
	public:
		NURBSSurfaceAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowNURBSSurfaceAttribute(kit);
			auto prop_Budget = new NURBSSurfaceAttributeKitBudgetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Budget);
			prop_Budget->addSubItems();
			auto prop_MaximumDeviation = new NURBSSurfaceAttributeKitMaximumDeviationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumDeviation);
			prop_MaximumDeviation->addSubItems();
			auto prop_MaximumAngle = new NURBSSurfaceAttributeKitMaximumAngleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumAngle);
			prop_MaximumAngle->addSubItems();
			auto prop_MaximumWidth = new NURBSSurfaceAttributeKitMaximumWidthProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumWidth);
			prop_MaximumWidth->addSubItems();
			auto prop_TrimBudget = new NURBSSurfaceAttributeKitTrimBudgetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_TrimBudget);
			prop_TrimBudget->addSubItems();
			auto prop_MaximumTrimDeviation = new NURBSSurfaceAttributeKitMaximumTrimDeviationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_MaximumTrimDeviation);
			prop_MaximumTrimDeviation->addSubItems();
		}

		void Apply() override
		{
			key.UnsetNURBSSurfaceAttribute();
			key.SetNURBSSurfaceAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::NURBSSurfaceAttributeKit kit;
	};

	class PerformanceKitDisplayListsProperty : public SettableProperty
	{
	public:
		PerformanceKitDisplayListsProperty(
			QTreeWidget * tree,
			HPS::PerformanceKit & kit)
			: SettableProperty("DisplayLists")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDisplayLists(_display_list);
			if (!_isSet)
			{
				_display_list = HPS::Performance::DisplayLists::Segment;
			}
			PerformanceDisplayListsProperty * enumObject_0 = new PerformanceDisplayListsProperty(tree, _display_list);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDisplayLists(_display_list);
		}

		void unset() override
		{
			kit.UnsetDisplayLists();
		}

	private:
		HPS::PerformanceKit & kit;
		HPS::Performance::DisplayLists _display_list;
		QTreeWidget * tree;
	};

	class PerformanceKitTextHardwareAccelerationProperty : public SettableProperty
	{
	public:
		PerformanceKitTextHardwareAccelerationProperty(
			QTreeWidget * tree,
			HPS::PerformanceKit & kit)
			: SettableProperty("TextHardwareAcceleration")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowTextHardwareAcceleration(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTextHardwareAcceleration(_state);
		}

		void unset() override
		{
			kit.UnsetTextHardwareAcceleration();
		}

	private:
		HPS::PerformanceKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class PerformanceKitStaticModelProperty : public SettableProperty
	{
	public:
		PerformanceKitStaticModelProperty(
			QTreeWidget * tree,
			HPS::PerformanceKit & kit)
			: SettableProperty("StaticModel")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowStaticModel(_model_type);
			if (!_isSet)
			{
				_model_type = HPS::Performance::StaticModel::Attribute;
			}
			PerformanceStaticModelProperty * enumObject_0 = new PerformanceStaticModelProperty(tree, _model_type);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetStaticModel(_model_type);
		}

		void unset() override
		{
			kit.UnsetStaticModel();
		}

	private:
		HPS::PerformanceKit & kit;
		HPS::Performance::StaticModel _model_type;
		QTreeWidget * tree;
	};

	class PerformanceKitStaticConditionsProperty : public SettableProperty
	{
	public:
		PerformanceKitStaticConditionsProperty(
			QTreeWidget * tree,
			HPS::PerformanceKit & kit)
			: SettableProperty("StaticConditions")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowStaticConditions(_conditions);
			if (!_isSet)
			{
				_conditions = HPS::Performance::StaticConditions::Independent;
			}
			PerformanceStaticConditionsProperty * enumObject_0 = new PerformanceStaticConditionsProperty(tree, _conditions);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetStaticConditions(_conditions);
		}

		void unset() override
		{
			kit.UnsetStaticConditions();
		}

	private:
		HPS::PerformanceKit & kit;
		HPS::Performance::StaticConditions _conditions;
		QTreeWidget * tree;
	};

	class PerformanceKitProperty : public RootProperty
	{
	public:
		PerformanceKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowPerformance(kit);
			auto prop_DisplayLists = new PerformanceKitDisplayListsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DisplayLists);
			prop_DisplayLists->addSubItems();
			auto prop_TextHardwareAcceleration = new PerformanceKitTextHardwareAccelerationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_TextHardwareAcceleration);
			prop_TextHardwareAcceleration->addSubItems();
			auto prop_StaticModel = new PerformanceKitStaticModelProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_StaticModel);
			prop_StaticModel->addSubItems();
			auto prop_StaticConditions = new PerformanceKitStaticConditionsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_StaticConditions);
			prop_StaticConditions->addSubItems();
		}

		void Apply() override
		{
			key.UnsetPerformance();
			key.SetPerformance(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::PerformanceKit kit;
	};

	class HiddenLineAttributeKitAlgorithmProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitAlgorithmProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("Algorithm")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAlgorithm(_algorithm);
			if (!_isSet)
			{
				_algorithm = HPS::HiddenLine::Algorithm::ZBuffer;
			}
			HiddenLineAlgorithmProperty * enumObject_0 = new HiddenLineAlgorithmProperty(tree, _algorithm);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetAlgorithm(_algorithm);
		}

		void unset() override
		{
			kit.UnsetAlgorithm();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		HPS::HiddenLine::Algorithm _algorithm;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitColorProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitColorProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("Color")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowColor(_color);
			if (!_isSet)
			{
				_color = HPS::RGBAColor::Black();
			}
			addChild(new RGBAColorProperty("Color", _color));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetColor(_color);
		}

		void unset() override
		{
			kit.UnsetColor();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		HPS::RGBAColor _color;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitDimFactorProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitDimFactorProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("DimFactor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDimFactor(_zero_to_one);
			if (!_isSet)
			{
				_zero_to_one = 0.0f;
			}
			addChild(new FloatProperty("Zero To One", _zero_to_one));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDimFactor(_zero_to_one);
		}

		void unset() override
		{
			kit.UnsetDimFactor();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _zero_to_one;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitFaceDisplacementProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitFaceDisplacementProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("FaceDisplacement")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaceDisplacement(_buckets);
			if (!_isSet)
			{
				_buckets = 0.0f;
			}
			addChild(new FloatProperty("Buckets", _buckets));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFaceDisplacement(_buckets);
		}

		void unset() override
		{
			kit.UnsetFaceDisplacement();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _buckets;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitLinePatternProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitLinePatternProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("LinePattern")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLinePattern(_pattern);
			if (!_isSet)
			{
				_pattern = "pattern";
			}
			addChild(new UTF8Property("Pattern", _pattern));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLinePattern(_pattern);
		}

		void unset() override
		{
			kit.UnsetLinePattern();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		HPS::UTF8 _pattern;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitLineSortingProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitLineSortingProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("LineSorting")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLineSorting(_state, _threshold, _units);
			if (!_isSet)
			{
				_state = true;
				_threshold = 0.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			addChild(new FloatProperty("Threshold", _threshold));
			LineSizeUnitsProperty * enumObject_2 = new LineSizeUnitsProperty(tree, _units);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLineSorting(_state, _threshold, _units);
		}

		void unset() override
		{
			kit.UnsetLineSorting();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
		float _threshold;
		HPS::Line::SizeUnits _units;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitRenderFacesProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitRenderFacesProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("RenderFaces")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRenderFaces(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRenderFaces(_state);
		}

		void unset() override
		{
			kit.UnsetRenderFaces();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitRenderTextProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitRenderTextProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("RenderText")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowRenderText(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetRenderText(_state);
		}

		void unset() override
		{
			kit.UnsetRenderText();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitSilhouetteCleanupProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitSilhouetteCleanupProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("SilhouetteCleanup")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSilhouetteCleanup(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSilhouetteCleanup(_state);
		}

		void unset() override
		{
			kit.UnsetSilhouetteCleanup();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitTransparencyCutoffProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitTransparencyCutoffProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("TransparencyCutoff")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowTransparencyCutoff(_zero_to_one);
			if (!_isSet)
			{
				_zero_to_one = 0.0f;
			}
			addChild(new FloatProperty("Zero To One", _zero_to_one));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTransparencyCutoff(_zero_to_one);
		}

		void unset() override
		{
			kit.UnsetTransparencyCutoff();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _zero_to_one;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitVisibilityProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitVisibilityProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("Visibility")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVisibility(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVisibility(_state);
		}

		void unset() override
		{
			kit.UnsetVisibility();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitWeightProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitWeightProperty(
			QTreeWidget * tree,
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty("Weight")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWeight(_weight, _units);
			if (!_isSet)
			{
				_weight = 0.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			addChild(new FloatProperty("Weight", _weight));
			LineSizeUnitsProperty * enumObject_1 = new LineSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetWeight(_weight, _units);
		}

		void unset() override
		{
			kit.UnsetWeight();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _weight;
		HPS::Line::SizeUnits _units;
		QTreeWidget * tree;
	};

	class HiddenLineAttributeKitProperty : public RootProperty
	{
	public:
		HiddenLineAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowHiddenLineAttribute(kit);
			auto prop_Algorithm = new HiddenLineAttributeKitAlgorithmProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Algorithm);
			prop_Algorithm->addSubItems();
			auto prop_Color = new HiddenLineAttributeKitColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Color);
			prop_Color->addSubItems();
			auto prop_DimFactor = new HiddenLineAttributeKitDimFactorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DimFactor);
			prop_DimFactor->addSubItems();
			auto prop_FaceDisplacement = new HiddenLineAttributeKitFaceDisplacementProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_FaceDisplacement);
			prop_FaceDisplacement->addSubItems();
			auto prop_LinePattern = new HiddenLineAttributeKitLinePatternProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_LinePattern);
			prop_LinePattern->addSubItems();
			auto prop_LineSorting = new HiddenLineAttributeKitLineSortingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_LineSorting);
			prop_LineSorting->addSubItems();
			auto prop_RenderFaces = new HiddenLineAttributeKitRenderFacesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_RenderFaces);
			prop_RenderFaces->addSubItems();
			auto prop_RenderText = new HiddenLineAttributeKitRenderTextProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_RenderText);
			prop_RenderText->addSubItems();
			auto prop_SilhouetteCleanup = new HiddenLineAttributeKitSilhouetteCleanupProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SilhouetteCleanup);
			prop_SilhouetteCleanup->addSubItems();
			auto prop_TransparencyCutoff = new HiddenLineAttributeKitTransparencyCutoffProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_TransparencyCutoff);
			prop_TransparencyCutoff->addSubItems();
			auto prop_Visibility = new HiddenLineAttributeKitVisibilityProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Visibility);
			prop_Visibility->addSubItems();
			auto prop_Weight = new HiddenLineAttributeKitWeightProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Weight);
			prop_Weight->addSubItems();
		}

		void Apply() override
		{
			key.UnsetHiddenLineAttribute();
			key.SetHiddenLineAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::HiddenLineAttributeKit kit;
	};

	class DrawingAttributeKitPolygonHandednessProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitPolygonHandednessProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("PolygonHandedness")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPolygonHandedness(_handedness);
			if (!_isSet)
			{
				_handedness = HPS::Drawing::Handedness::Right;
			}
			DrawingHandednessProperty * enumObject_0 = new DrawingHandednessProperty(tree, _handedness);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPolygonHandedness(_handedness);
		}

		void unset() override
		{
			kit.UnsetPolygonHandedness();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::Drawing::Handedness _handedness;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitWorldHandednessProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitWorldHandednessProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("WorldHandedness")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWorldHandedness(_handedness);
			if (!_isSet)
			{
				_handedness = HPS::Drawing::Handedness::Right;
			}
			DrawingHandednessProperty * enumObject_0 = new DrawingHandednessProperty(tree, _handedness);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetWorldHandedness(_handedness);
		}

		void unset() override
		{
			kit.UnsetWorldHandedness();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::Drawing::Handedness _handedness;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitDepthRangeProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitDepthRangeProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("DepthRange")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDepthRange(_near, _far);
			if (!_isSet)
			{
				_near = 0.0f;
				_far = 0.0f;
			}
			addChild(new FloatProperty("Near", _near));
			addChild(new FloatProperty("Far", _far));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDepthRange(_near, _far);
		}

		void unset() override
		{
			kit.UnsetDepthRange();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		float _near;
		float _far;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitFaceDisplacementProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitFaceDisplacementProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("FaceDisplacement")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaceDisplacement(_state, _buckets);
			if (!_isSet)
			{
				_state = true;
				_buckets = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new IntProperty("Buckets", _buckets));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFaceDisplacement(_state, _buckets);
		}

		void unset() override
		{
			kit.UnsetFaceDisplacement();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		int _buckets;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitGeneralDisplacementProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitGeneralDisplacementProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("GeneralDisplacement")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowGeneralDisplacement(_state, _buckets);
			if (!_isSet)
			{
				_state = true;
				_buckets = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new IntProperty("Buckets", _buckets));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetGeneralDisplacement(_state, _buckets);
		}

		void unset() override
		{
			kit.UnsetGeneralDisplacement();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		int _buckets;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitVertexDisplacementProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitVertexDisplacementProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("VertexDisplacement")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertexDisplacement(_state, _buckets);
			if (!_isSet)
			{
				_state = true;
				_buckets = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new IntProperty("Buckets", _buckets));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVertexDisplacement(_state, _buckets);
		}

		void unset() override
		{
			kit.UnsetVertexDisplacement();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		int _buckets;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitVertexDecimationProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitVertexDecimationProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("VertexDecimation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertexDecimation(_zero_to_one);
			if (!_isSet)
			{
				_zero_to_one = 0.0f;
			}
			addChild(new FloatProperty("Zero To One", _zero_to_one));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVertexDecimation(_zero_to_one);
		}

		void unset() override
		{
			kit.UnsetVertexDecimation();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		float _zero_to_one;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitVertexRandomizationProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitVertexRandomizationProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("VertexRandomization")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertexRandomization(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVertexRandomization(_state);
		}

		void unset() override
		{
			kit.UnsetVertexRandomization();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitOverlayProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitOverlayProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("Overlay")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowOverlay(_overlay);
			if (!_isSet)
			{
				_overlay = HPS::Drawing::Overlay::Default;
			}
			DrawingOverlayProperty * enumObject_0 = new DrawingOverlayProperty(tree, _overlay);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetOverlay(_overlay);
		}

		void unset() override
		{
			kit.UnsetOverlay();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::Drawing::Overlay _overlay;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitDeferralProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitDeferralProperty(
			QTreeWidget * tree,
			HPS::DrawingAttributeKit & kit)
			: SettableProperty("Deferral")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDeferral(_defer_batch);
			if (!_isSet)
			{
				_defer_batch = 0;
			}
			addChild(new IntProperty("Defer Batch", _defer_batch));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDeferral(_defer_batch);
		}

		void unset() override
		{
			kit.UnsetDeferral();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		int _defer_batch;
		QTreeWidget * tree;
	};

	class DrawingAttributeKitProperty : public RootProperty
	{
	public:
		DrawingAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowDrawingAttribute(kit);
			auto prop_PolygonHandedness = new DrawingAttributeKitPolygonHandednessProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_PolygonHandedness);
			prop_PolygonHandedness->addSubItems();
			auto prop_WorldHandedness = new DrawingAttributeKitWorldHandednessProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_WorldHandedness);
			prop_WorldHandedness->addSubItems();
			auto prop_DepthRange = new DrawingAttributeKitDepthRangeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthRange);
			prop_DepthRange->addSubItems();
			auto prop_FaceDisplacement = new DrawingAttributeKitFaceDisplacementProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_FaceDisplacement);
			prop_FaceDisplacement->addSubItems();
			auto prop_GeneralDisplacement = new DrawingAttributeKitGeneralDisplacementProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_GeneralDisplacement);
			prop_GeneralDisplacement->addSubItems();
			auto prop_VertexDisplacement = new DrawingAttributeKitVertexDisplacementProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VertexDisplacement);
			prop_VertexDisplacement->addSubItems();
			auto prop_VertexDecimation = new DrawingAttributeKitVertexDecimationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VertexDecimation);
			prop_VertexDecimation->addSubItems();
			auto prop_VertexRandomization = new DrawingAttributeKitVertexRandomizationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VertexRandomization);
			prop_VertexRandomization->addSubItems();
			auto prop_OverrideInternalColor = new DrawingAttributeKitOverrideInternalColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_OverrideInternalColor);
			prop_OverrideInternalColor->addSubItems();
			auto prop_Overlay = new DrawingAttributeKitOverlayProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Overlay);
			prop_Overlay->addSubItems();
			auto prop_Deferral = new DrawingAttributeKitDeferralProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Deferral);
			prop_Deferral->addSubItems();
			auto prop_ClipRegion = new DrawingAttributeKitClipRegionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ClipRegion);
			prop_ClipRegion->addSubItems();
		}

		void Apply() override
		{
			key.UnsetDrawingAttribute();
			key.SetDrawingAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::DrawingAttributeKit kit;
	};

	class SelectabilityKitWindowsProperty : public SettableProperty
	{
	public:
		SelectabilityKitWindowsProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Windows")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowWindows(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetWindows(_val);
		}

		void unset() override
		{
			kit.UnsetWindows();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitEdgesProperty : public SettableProperty
	{
	public:
		SelectabilityKitEdgesProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Edges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEdges(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetEdges(_val);
		}

		void unset() override
		{
			kit.UnsetEdges();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitFacesProperty : public SettableProperty
	{
	public:
		SelectabilityKitFacesProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Faces")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaces(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFaces(_val);
		}

		void unset() override
		{
			kit.UnsetFaces();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitLightsProperty : public SettableProperty
	{
	public:
		SelectabilityKitLightsProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Lights")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLights(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLights(_val);
		}

		void unset() override
		{
			kit.UnsetLights();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitLinesProperty : public SettableProperty
	{
	public:
		SelectabilityKitLinesProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Lines")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLines(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLines(_val);
		}

		void unset() override
		{
			kit.UnsetLines();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitMarkersProperty : public SettableProperty
	{
	public:
		SelectabilityKitMarkersProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Markers")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowMarkers(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetMarkers(_val);
		}

		void unset() override
		{
			kit.UnsetMarkers();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitVerticesProperty : public SettableProperty
	{
	public:
		SelectabilityKitVerticesProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Vertices")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertices(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVertices(_val);
		}

		void unset() override
		{
			kit.UnsetVertices();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitTextProperty : public SettableProperty
	{
	public:
		SelectabilityKitTextProperty(
			QTreeWidget * tree,
			HPS::SelectabilityKit & kit)
			: SettableProperty("Text")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowText(_val);
			if (!_isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			SelectabilityValueProperty * enumObject_0 = new SelectabilityValueProperty(tree, _val);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetText(_val);
		}

		void unset() override
		{
			kit.UnsetText();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
		QTreeWidget * tree;
	};

	class SelectabilityKitProperty : public RootProperty
	{
	public:
		SelectabilityKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSelectability(kit);
			auto prop_Windows = new SelectabilityKitWindowsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Windows);
			prop_Windows->addSubItems();
			auto prop_Edges = new SelectabilityKitEdgesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Edges);
			prop_Edges->addSubItems();
			auto prop_Faces = new SelectabilityKitFacesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Faces);
			prop_Faces->addSubItems();
			auto prop_Lights = new SelectabilityKitLightsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Lights);
			prop_Lights->addSubItems();
			auto prop_Lines = new SelectabilityKitLinesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Lines);
			prop_Lines->addSubItems();
			auto prop_Markers = new SelectabilityKitMarkersProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Markers);
			prop_Markers->addSubItems();
			auto prop_Vertices = new SelectabilityKitVerticesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Vertices);
			prop_Vertices->addSubItems();
			auto prop_Text = new SelectabilityKitTextProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Text);
			prop_Text->addSubItems();
		}

		void Apply() override
		{
			key.UnsetSelectability();
			key.SetSelectability(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::SelectabilityKit kit;
	};

	class MarkerAttributeKitSymbolProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitSymbolProperty(
			QTreeWidget * tree,
			HPS::MarkerAttributeKit & kit)
			: SettableProperty("Symbol")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSymbol(_glyph_name);
			if (!_isSet)
			{
				_glyph_name = "glyph_name";
			}
			addChild(new UTF8Property("Glyph Name", _glyph_name));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSymbol(_glyph_name);
		}

		void unset() override
		{
			kit.UnsetSymbol();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		HPS::UTF8 _glyph_name;
		QTreeWidget * tree;
	};

	class MarkerAttributeKitSizeProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitSizeProperty(
			QTreeWidget * tree,
			HPS::MarkerAttributeKit & kit)
			: SettableProperty("Size")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSize(_size, _units);
			if (!_isSet)
			{
				_size = 0.0f;
				_units = HPS::Marker::SizeUnits::ScaleFactor;
			}
			addChild(new FloatProperty("Size", _size));
			MarkerSizeUnitsProperty * enumObject_1 = new MarkerSizeUnitsProperty(tree, _units);
			addChild(enumObject_1);
			enumObject_1->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSize(_size, _units);
		}

		void unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		float _size;
		HPS::Marker::SizeUnits _units;
		QTreeWidget * tree;
	};

	class MarkerAttributeKitDrawingPreferenceProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitDrawingPreferenceProperty(
			QTreeWidget * tree,
			HPS::MarkerAttributeKit & kit)
			: SettableProperty("DrawingPreference")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDrawingPreference(_preference);
			if (!_isSet)
			{
				_preference = HPS::Marker::DrawingPreference::Fastest;
			}
			MarkerDrawingPreferenceProperty * enumObject_0 = new MarkerDrawingPreferenceProperty(tree, _preference);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDrawingPreference(_preference);
		}

		void unset() override
		{
			kit.UnsetDrawingPreference();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		HPS::Marker::DrawingPreference _preference;
		QTreeWidget * tree;
	};

	class MarkerAttributeKitGlyphRotationProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitGlyphRotationProperty(
			QTreeWidget * tree,
			HPS::MarkerAttributeKit & kit)
			: SettableProperty("GlyphRotation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowGlyphRotation(_rotation);
			if (!_isSet)
			{
				_rotation = 0.0f;
			}
			addChild(new FloatProperty("Rotation", _rotation));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetGlyphRotation(_rotation);
		}

		void unset() override
		{
			kit.UnsetGlyphRotation();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		float _rotation;
		QTreeWidget * tree;
	};

	class MarkerAttributeKitProperty : public RootProperty
	{
	public:
		MarkerAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowMarkerAttribute(kit);
			auto prop_Symbol = new MarkerAttributeKitSymbolProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Symbol);
			prop_Symbol->addSubItems();
			auto prop_Size = new MarkerAttributeKitSizeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Size);
			prop_Size->addSubItems();
			auto prop_DrawingPreference = new MarkerAttributeKitDrawingPreferenceProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DrawingPreference);
			prop_DrawingPreference->addSubItems();
			auto prop_GlyphRotation = new MarkerAttributeKitGlyphRotationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_GlyphRotation);
			prop_GlyphRotation->addSubItems();
		}

		void Apply() override
		{
			key.UnsetMarkerAttribute();
			key.SetMarkerAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::MarkerAttributeKit kit;
	};

	class LightingAttributeKitInterpolationAlgorithmProperty : public SettableProperty
	{
	public:
		LightingAttributeKitInterpolationAlgorithmProperty(
			QTreeWidget * tree,
			HPS::LightingAttributeKit & kit)
			: SettableProperty("InterpolationAlgorithm")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowInterpolationAlgorithm(_interpolation);
			if (!_isSet)
			{
				_interpolation = HPS::Lighting::InterpolationAlgorithm::Phong;
			}
			LightingInterpolationAlgorithmProperty * enumObject_0 = new LightingInterpolationAlgorithmProperty(tree, _interpolation);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetInterpolationAlgorithm(_interpolation);
		}

		void unset() override
		{
			kit.UnsetInterpolationAlgorithm();
		}

	private:
		HPS::LightingAttributeKit & kit;
		HPS::Lighting::InterpolationAlgorithm _interpolation;
		QTreeWidget * tree;
	};

	class LightingAttributeKitProperty : public RootProperty
	{
	public:
		LightingAttributeKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowLightingAttribute(kit);
			auto prop_InterpolationAlgorithm = new LightingAttributeKitInterpolationAlgorithmProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_InterpolationAlgorithm);
			prop_InterpolationAlgorithm->addSubItems();
		}

		void Apply() override
		{
			key.UnsetLightingAttribute();
			key.SetLightingAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::LightingAttributeKit kit;
	};

	class VisualEffectsKitPostProcessEffectsEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitPostProcessEffectsEnabledProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("PostProcessEffectsEnabled")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowPostProcessEffectsEnabled(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetPostProcessEffectsEnabled(_state);
		}

		void unset() override
		{
			kit.UnsetPostProcessEffectsEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitAmbientOcclusionEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitAmbientOcclusionEnabledProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("AmbientOcclusionEnabled")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowAmbientOcclusionEnabled(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetAmbientOcclusionEnabled(_state);
		}

		void unset() override
		{
			kit.UnsetAmbientOcclusionEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSilhouetteEdgesEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSilhouetteEdgesEnabledProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SilhouetteEdgesEnabled")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSilhouetteEdgesEnabled(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSilhouetteEdgesEnabled(_state);
		}

		void unset() override
		{
			kit.UnsetSilhouetteEdgesEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitDepthOfFieldEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitDepthOfFieldEnabledProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("DepthOfFieldEnabled")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowDepthOfFieldEnabled(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetDepthOfFieldEnabled(_state);
		}

		void unset() override
		{
			kit.UnsetDepthOfFieldEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitBloomEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitBloomEnabledProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("BloomEnabled")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowBloomEnabled(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetBloomEnabled(_state);
		}

		void unset() override
		{
			kit.UnsetBloomEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitEyeDomeLightingEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitEyeDomeLightingEnabledProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("EyeDomeLightingEnabled")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEyeDomeLightingEnabled(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetEyeDomeLightingEnabled(_state);
		}

		void unset() override
		{
			kit.UnsetEyeDomeLightingEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitTextAntiAliasingProperty : public SettableProperty
	{
	public:
		VisualEffectsKitTextAntiAliasingProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("TextAntiAliasing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowTextAntiAliasing(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetTextAntiAliasing(_state);
		}

		void unset() override
		{
			kit.UnsetTextAntiAliasing();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitLinesAntiAliasingProperty : public SettableProperty
	{
	public:
		VisualEffectsKitLinesAntiAliasingProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("LinesAntiAliasing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLinesAntiAliasing(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLinesAntiAliasing(_state);
		}

		void unset() override
		{
			kit.UnsetLinesAntiAliasing();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitScreenAntiAliasingProperty : public SettableProperty
	{
	public:
		VisualEffectsKitScreenAntiAliasingProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("ScreenAntiAliasing")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowScreenAntiAliasing(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetScreenAntiAliasing(_state);
		}

		void unset() override
		{
			kit.UnsetScreenAntiAliasing();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class VisualEffectsKitShadowMapsProperty : public SettableProperty
	{
	public:
		VisualEffectsKitShadowMapsProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("ShadowMaps")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowShadowMaps(_state, _samples, _resolution, _view_dependent, _jitter);
			if (!_isSet)
			{
				_state = true;
				_samples = 0;
				_resolution = 0;
				_view_dependent = true;
				_jitter = true;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UnsignedIntProperty("Samples", _samples));
			addChild(new UnsignedIntProperty("Resolution", _resolution));
			{
				auto boolProperty = new BoolProperty(tree, "View Dependent", _view_dependent);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			{
				auto boolProperty = new BoolProperty(tree, "Jitter", _jitter);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetShadowMaps(_state, _samples, _resolution, _view_dependent, _jitter);
		}

		void unset() override
		{
			kit.UnsetShadowMaps();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		unsigned int _samples;
		unsigned int _resolution;
		bool _view_dependent;
		bool _jitter;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleShadowProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleShadow")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleShadow(_state, _resolution, _blurring, _ignore_transparency);
			if (!_isSet)
			{
				_state = true;
				_resolution = 0;
				_blurring = 0;
				_ignore_transparency = true;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new UnsignedIntProperty("Resolution", _resolution));
			addChild(new UnsignedIntProperty("Blurring", _blurring));
			{
				auto boolProperty = new BoolProperty(tree, "Ignore Transparency", _ignore_transparency);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSimpleShadow(_state, _resolution, _blurring, _ignore_transparency);
		}

		void unset() override
		{
			kit.UnsetSimpleShadow();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		unsigned int _resolution;
		unsigned int _blurring;
		bool _ignore_transparency;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleShadowPlaneProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowPlaneProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleShadowPlane")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleShadowPlane(_projected_onto);
			if (!_isSet)
			{
				_projected_onto = HPS::Plane(0, 0, 1, 0);
			}
			addChild(new PlaneProperty("Projected Onto", _projected_onto));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSimpleShadowPlane(_projected_onto);
		}

		void unset() override
		{
			kit.UnsetSimpleShadowPlane();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::Plane _projected_onto;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleShadowLightDirectionProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowLightDirectionProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleShadowLightDirection")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleShadowLightDirection(_direction);
			if (!_isSet)
			{
				_direction = HPS::Vector::Unit();
			}
			addChild(new VectorProperty("Direction", _direction));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSimpleShadowLightDirection(_direction);
		}

		void unset() override
		{
			kit.UnsetSimpleShadowLightDirection();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::Vector _direction;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleShadowColorProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowColorProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleShadowColor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleShadowColor(_color);
			if (!_isSet)
			{
				_color = HPS::RGBAColor::Black();
			}
			addChild(new RGBAColorProperty("Color", _color));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSimpleShadowColor(_color);
		}

		void unset() override
		{
			kit.UnsetSimpleShadowColor();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::RGBAColor _color;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleReflectionProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleReflectionProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleReflection")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleReflection(_state, _opacity, _blurring, _fading, _attenuation_near_distance, _attenuation_far_distance);
			if (!_isSet)
			{
				_state = true;
				_opacity = 0.0f;
				_blurring = 0;
				_fading = true;
				_attenuation_near_distance = 0.0f;
				_attenuation_far_distance = 0.0f;
			}
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Opacity", _opacity));
			addChild(new UnsignedIntProperty("Blurring", _blurring));
			{
				auto boolProperty = new BoolProperty(tree, "Fading", _fading);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			addChild(new FloatProperty("Attenuation Near Distance", _attenuation_near_distance));
			addChild(new FloatProperty("Attenuation Far Distance", _attenuation_far_distance));
			boolProperty->enableValidProperties();
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSimpleReflection(_state, _opacity, _blurring, _fading, _attenuation_near_distance, _attenuation_far_distance);
		}

		void unset() override
		{
			kit.UnsetSimpleReflection();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		float _opacity;
		unsigned int _blurring;
		bool _fading;
		float _attenuation_near_distance;
		float _attenuation_far_distance;
		QTreeWidget * tree;
	};

	class VisualEffectsKitSimpleReflectionPlaneProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleReflectionPlaneProperty(
			QTreeWidget * tree,
			HPS::VisualEffectsKit & kit)
			: SettableProperty("SimpleReflectionPlane")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowSimpleReflectionPlane(_projected_onto);
			if (!_isSet)
			{
				_projected_onto = HPS::Plane(0, 0, 1, 0);
			}
			addChild(new PlaneProperty("Projected Onto", _projected_onto));
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetSimpleReflectionPlane(_projected_onto);
		}

		void unset() override
		{
			kit.UnsetSimpleReflectionPlane();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::Plane _projected_onto;
		QTreeWidget * tree;
	};

	class VisualEffectsKitProperty : public RootProperty
	{
	public:
		VisualEffectsKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowVisualEffects(kit);
			auto prop_PostProcessEffectsEnabled = new VisualEffectsKitPostProcessEffectsEnabledProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_PostProcessEffectsEnabled);
			prop_PostProcessEffectsEnabled->addSubItems();
			auto prop_AmbientOcclusionEnabled = new VisualEffectsKitAmbientOcclusionEnabledProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_AmbientOcclusionEnabled);
			prop_AmbientOcclusionEnabled->addSubItems();
			auto prop_SilhouetteEdgesEnabled = new VisualEffectsKitSilhouetteEdgesEnabledProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SilhouetteEdgesEnabled);
			prop_SilhouetteEdgesEnabled->addSubItems();
			auto prop_DepthOfFieldEnabled = new VisualEffectsKitDepthOfFieldEnabledProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthOfFieldEnabled);
			prop_DepthOfFieldEnabled->addSubItems();
			auto prop_BloomEnabled = new VisualEffectsKitBloomEnabledProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_BloomEnabled);
			prop_BloomEnabled->addSubItems();
			auto prop_EyeDomeLightingEnabled = new VisualEffectsKitEyeDomeLightingEnabledProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EyeDomeLightingEnabled);
			prop_EyeDomeLightingEnabled->addSubItems();
			auto prop_EyeDomeLightingBackColor = new VisualEffectsKitEyeDomeLightingBackColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EyeDomeLightingBackColor);
			prop_EyeDomeLightingBackColor->addSubItems();
			auto prop_TextAntiAliasing = new VisualEffectsKitTextAntiAliasingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_TextAntiAliasing);
			prop_TextAntiAliasing->addSubItems();
			auto prop_LinesAntiAliasing = new VisualEffectsKitLinesAntiAliasingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_LinesAntiAliasing);
			prop_LinesAntiAliasing->addSubItems();
			auto prop_ScreenAntiAliasing = new VisualEffectsKitScreenAntiAliasingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ScreenAntiAliasing);
			prop_ScreenAntiAliasing->addSubItems();
			auto prop_ShadowMaps = new VisualEffectsKitShadowMapsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ShadowMaps);
			prop_ShadowMaps->addSubItems();
			auto prop_SimpleShadow = new VisualEffectsKitSimpleShadowProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleShadow);
			prop_SimpleShadow->addSubItems();
			auto prop_SimpleShadowPlane = new VisualEffectsKitSimpleShadowPlaneProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleShadowPlane);
			prop_SimpleShadowPlane->addSubItems();
			auto prop_SimpleShadowLightDirection = new VisualEffectsKitSimpleShadowLightDirectionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleShadowLightDirection);
			prop_SimpleShadowLightDirection->addSubItems();
			auto prop_SimpleShadowColor = new VisualEffectsKitSimpleShadowColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleShadowColor);
			prop_SimpleShadowColor->addSubItems();
			auto prop_SimpleReflection = new VisualEffectsKitSimpleReflectionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleReflection);
			prop_SimpleReflection->addSubItems();
			auto prop_SimpleReflectionPlane = new VisualEffectsKitSimpleReflectionPlaneProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleReflectionPlane);
			prop_SimpleReflectionPlane->addSubItems();
			auto prop_SimpleReflectionVisibility = new VisualEffectsKitSimpleReflectionVisibilityProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SimpleReflectionVisibility);
			prop_SimpleReflectionVisibility->addSubItems();
		}

		void Apply() override
		{
			key.UnsetVisualEffects();
			key.SetVisualEffects(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::VisualEffectsKit kit;
	};

	class PostProcessEffectsKitAmbientOcclusionProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitAmbientOcclusionProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty("AmbientOcclusion")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowAmbientOcclusion(_state, _strength, _quality, _radius, _sharpness);
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Strength", _strength));
			PostProcessEffectsAmbientOcclusionQualityProperty * enumObject_2 = new PostProcessEffectsAmbientOcclusionQualityProperty(tree, _quality);
			addChild(enumObject_2);
			enumObject_2->setupChoices();
			addChild(new FloatProperty("Radius", _radius));
			addChild(new FloatProperty("Sharpness", _sharpness));
			boolProperty->enableValidProperties();
			smartShow();
		}
		void onChildChanged() override
		{
			kit.SetAmbientOcclusion(_state, _strength, _quality, _radius, _sharpness);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _strength;
		HPS::PostProcessEffects::AmbientOcclusion::Quality _quality;
		float _radius;
		float _sharpness;
		QTreeWidget * tree;
	};

	class PostProcessEffectsKitBloomProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitBloomProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty("Bloom")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowBloom(_state, _strength, _blur, _shape);
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Strength", _strength));
			addChild(new UnsignedIntProperty("Blur", _blur));
			PostProcessEffectsBloomShapeProperty * enumObject_3 = new PostProcessEffectsBloomShapeProperty(tree, _shape);
			addChild(enumObject_3);
			enumObject_3->setupChoices();
			boolProperty->enableValidProperties();
			smartShow();
		}
		void onChildChanged() override
		{
			kit.SetBloom(_state, _strength, _blur, _shape);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _strength;
		unsigned int _blur;
		HPS::PostProcessEffects::Bloom::Shape _shape;
		QTreeWidget * tree;
	};

	class PostProcessEffectsKitDepthOfFieldProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitDepthOfFieldProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty("DepthOfField")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowDepthOfField(_state, _strength, _near_distance, _far_distance);
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Strength", _strength));
			addChild(new FloatProperty("Near Distance", _near_distance));
			addChild(new FloatProperty("Far Distance", _far_distance));
			boolProperty->enableValidProperties();
			smartShow();
		}
		void onChildChanged() override
		{
			kit.SetDepthOfField(_state, _strength, _near_distance, _far_distance);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _strength;
		float _near_distance;
		float _far_distance;
		QTreeWidget * tree;
	};

	class PostProcessEffectsKitSilhouetteEdgesProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitSilhouetteEdgesProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty("SilhouetteEdges")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowSilhouetteEdges(_state, _tolerance, _heavy_exterior);
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Tolerance", _tolerance));
			{
				auto boolProperty = new BoolProperty(tree, "Heavy Exterior", _heavy_exterior);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			boolProperty->enableValidProperties();
			smartShow();
		}
		void onChildChanged() override
		{
			kit.SetSilhouetteEdges(_state, _tolerance, _heavy_exterior);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _tolerance;
		bool _heavy_exterior;
		QTreeWidget * tree;
	};

	class PostProcessEffectsKitEyeDomeLightingProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitEyeDomeLightingProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty("EyeDomeLighting")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowEyeDomeLighting(_state, _exponent, _tolerance, _strength);
			auto boolProperty = new ConditionalBoolProperty(tree, "State", _state);
			addChild(boolProperty);
			boolProperty->setupComboBox();
			addChild(new FloatProperty("Exponent", _exponent));
			addChild(new FloatProperty("Tolerance", _tolerance));
			addChild(new FloatProperty("Strength", _strength));
			boolProperty->enableValidProperties();
			smartShow();
		}
		void onChildChanged() override
		{
			kit.SetEyeDomeLighting(_state, _exponent, _tolerance, _strength);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _exponent;
		float _tolerance;
		float _strength;
		QTreeWidget * tree;
	};

	class PostProcessEffectsKitWorldScaleProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitWorldScaleProperty(
			QTreeWidget * tree,
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty("WorldScale")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowWorldScale(_scale);
			addChild(new FloatProperty("Scale", _scale));
		}
		void onChildChanged() override
		{
			kit.SetWorldScale(_scale);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		float _scale;
		QTreeWidget * tree;
	};

	class PostProcessEffectsKitProperty : public RootProperty
	{
	public:
		PostProcessEffectsKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowPostProcessEffects(kit);
			auto prop_AmbientOcclusion = new PostProcessEffectsKitAmbientOcclusionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_AmbientOcclusion);
			prop_AmbientOcclusion->addSubItems();
			auto prop_Bloom = new PostProcessEffectsKitBloomProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Bloom);
			prop_Bloom->addSubItems();
			auto prop_DepthOfField = new PostProcessEffectsKitDepthOfFieldProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_DepthOfField);
			prop_DepthOfField->addSubItems();
			auto prop_SilhouetteEdges = new PostProcessEffectsKitSilhouetteEdgesProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SilhouetteEdges);
			prop_SilhouetteEdges->addSubItems();
			auto prop_EyeDomeLighting = new PostProcessEffectsKitEyeDomeLightingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EyeDomeLighting);
			prop_EyeDomeLighting->addSubItems();
			auto prop_WorldScale = new PostProcessEffectsKitWorldScaleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_WorldScale);
			prop_WorldScale->addSubItems();
		}

		void Apply() override
		{
			key.SetPostProcessEffects(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::PostProcessEffectsKit kit;
	};

	class DebuggingKitResourceMonitorProperty : public BaseProperty
	{
	public:
		DebuggingKitResourceMonitorProperty(
			QTreeWidget * tree,
			HPS::DebuggingKit & kit)
			: BaseProperty("ResourceMonitor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowResourceMonitor(_display);
			{
				auto boolProperty = new BoolProperty(tree, "Display", _display);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
		}
		void onChildChanged() override
		{
			kit.SetResourceMonitor(_display);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::DebuggingKit & kit;
		bool _display;
		QTreeWidget * tree;
	};

	class DebuggingKitProperty : public RootProperty
	{
	public:
		DebuggingKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowDebugging(kit);
			auto prop_ResourceMonitor = new DebuggingKitResourceMonitorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ResourceMonitor);
			prop_ResourceMonitor->addSubItems();
		}

		void Apply() override
		{
			key.SetDebugging(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::DebuggingKit kit;
	};

	class ContourLineKitVisibilityProperty : public SettableProperty
	{
	public:
		ContourLineKitVisibilityProperty(
			QTreeWidget * tree,
			HPS::ContourLineKit & kit)
			: SettableProperty("Visibility")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVisibility(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVisibility(_state);
		}

		void unset() override
		{
			kit.UnsetVisibility();
		}

	private:
		HPS::ContourLineKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ContourLineKitLightingProperty : public SettableProperty
	{
	public:
		ContourLineKitLightingProperty(
			QTreeWidget * tree,
			HPS::ContourLineKit & kit)
			: SettableProperty("Lighting")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowLighting(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetLighting(_state);
		}

		void unset() override
		{
			kit.UnsetLighting();
		}

	private:
		HPS::ContourLineKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ContourLineKitProperty : public RootProperty
	{
	public:
		ContourLineKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowContourLine(kit);
			auto prop_Visibility = new ContourLineKitVisibilityProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Visibility);
			prop_Visibility->addSubItems();
			auto prop_Positions = new ContourLineKitPositionsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Positions);
			prop_Positions->addSubItems();
			auto prop_Colors = new ContourLineKitColorsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Colors);
			prop_Colors->addSubItems();
			auto prop_Patterns = new ContourLineKitPatternsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Patterns);
			prop_Patterns->addSubItems();
			auto prop_Weights = new ContourLineKitWeightsProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Weights);
			prop_Weights->addSubItems();
			auto prop_Lighting = new ContourLineKitLightingProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Lighting);
			prop_Lighting->addSubItems();
		}

		void Apply() override
		{
			key.UnsetContourLine();
			key.SetContourLine(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::ContourLineKit kit;
	};

	class BoundingKitExclusionProperty : public SettableProperty
	{
	public:
		BoundingKitExclusionProperty(
			QTreeWidget * tree,
			HPS::BoundingKit & kit)
			: SettableProperty("Exclusion")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowExclusion(_exclusion);
			if (!_isSet)
			{
				_exclusion = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "Exclusion", _exclusion);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetExclusion(_exclusion);
		}

		void unset() override
		{
			kit.UnsetExclusion();
		}

	private:
		HPS::BoundingKit & kit;
		bool _exclusion;
		QTreeWidget * tree;
	};

	class BoundingKitProperty : public RootProperty
	{
	public:
		BoundingKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowBounding(kit);
			auto prop_Volume = new BoundingKitVolumeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Volume);
			prop_Volume->addSubItems();
			auto prop_Exclusion = new BoundingKitExclusionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Exclusion);
			prop_Exclusion->addSubItems();
		}

		void Apply() override
		{
			key.UnsetBounding();
			key.SetBounding(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::BoundingKit kit;
	};

	class TransformMaskKitCameraRotationProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraRotationProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraRotation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraRotation(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraRotation(_state);
		}

		void unset() override
		{
			kit.UnsetCameraRotation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitCameraScaleProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraScaleProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraScale")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraScale(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraScale(_state);
		}

		void unset() override
		{
			kit.UnsetCameraScale();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitCameraTranslationProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraTranslationProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraTranslation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraTranslation(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraTranslation(_state);
		}

		void unset() override
		{
			kit.UnsetCameraTranslation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitCameraPerspectiveScaleProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraPerspectiveScaleProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraPerspectiveScale")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraPerspectiveScale(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraPerspectiveScale(_state);
		}

		void unset() override
		{
			kit.UnsetCameraPerspectiveScale();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitCameraProjectionProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraProjectionProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraProjection")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraProjection(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraProjection(_state);
		}

		void unset() override
		{
			kit.UnsetCameraProjection();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitCameraOffsetProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraOffsetProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraOffset")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraOffset(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraOffset(_state);
		}

		void unset() override
		{
			kit.UnsetCameraOffset();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitCameraNearLimitProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraNearLimitProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("CameraNearLimit")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowCameraNearLimit(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetCameraNearLimit(_state);
		}

		void unset() override
		{
			kit.UnsetCameraNearLimit();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitModellingMatrixRotationProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixRotationProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("ModellingMatrixRotation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowModellingMatrixRotation(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetModellingMatrixRotation(_state);
		}

		void unset() override
		{
			kit.UnsetModellingMatrixRotation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitModellingMatrixScaleProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixScaleProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("ModellingMatrixScale")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowModellingMatrixScale(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetModellingMatrixScale(_state);
		}

		void unset() override
		{
			kit.UnsetModellingMatrixScale();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitModellingMatrixTranslationProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixTranslationProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("ModellingMatrixTranslation")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowModellingMatrixTranslation(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetModellingMatrixTranslation(_state);
		}

		void unset() override
		{
			kit.UnsetModellingMatrixTranslation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitModellingMatrixOffsetProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixOffsetProperty(
			QTreeWidget * tree,
			HPS::TransformMaskKit & kit)
			: SettableProperty("ModellingMatrixOffset")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowModellingMatrixOffset(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetModellingMatrixOffset(_state);
		}

		void unset() override
		{
			kit.UnsetModellingMatrixOffset();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class TransformMaskKitProperty : public RootProperty
	{
	public:
		TransformMaskKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowTransformMask(kit);
			auto prop_CameraRotation = new TransformMaskKitCameraRotationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraRotation);
			prop_CameraRotation->addSubItems();
			auto prop_CameraScale = new TransformMaskKitCameraScaleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraScale);
			prop_CameraScale->addSubItems();
			auto prop_CameraTranslation = new TransformMaskKitCameraTranslationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraTranslation);
			prop_CameraTranslation->addSubItems();
			auto prop_CameraPerspectiveScale = new TransformMaskKitCameraPerspectiveScaleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraPerspectiveScale);
			prop_CameraPerspectiveScale->addSubItems();
			auto prop_CameraProjection = new TransformMaskKitCameraProjectionProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraProjection);
			prop_CameraProjection->addSubItems();
			auto prop_CameraOffset = new TransformMaskKitCameraOffsetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraOffset);
			prop_CameraOffset->addSubItems();
			auto prop_CameraNearLimit = new TransformMaskKitCameraNearLimitProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_CameraNearLimit);
			prop_CameraNearLimit->addSubItems();
			auto prop_ModellingMatrixRotation = new TransformMaskKitModellingMatrixRotationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ModellingMatrixRotation);
			prop_ModellingMatrixRotation->addSubItems();
			auto prop_ModellingMatrixScale = new TransformMaskKitModellingMatrixScaleProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ModellingMatrixScale);
			prop_ModellingMatrixScale->addSubItems();
			auto prop_ModellingMatrixTranslation = new TransformMaskKitModellingMatrixTranslationProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ModellingMatrixTranslation);
			prop_ModellingMatrixTranslation->addSubItems();
			auto prop_ModellingMatrixOffset = new TransformMaskKitModellingMatrixOffsetProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_ModellingMatrixOffset);
			prop_ModellingMatrixOffset->addSubItems();
		}

		void Apply() override
		{
			key.UnsetTransformMask();
			key.SetTransformMask(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::TransformMaskKit kit;
	};

	class ColorInterpolationKitFaceColorProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitFaceColorProperty(
			QTreeWidget * tree,
			HPS::ColorInterpolationKit & kit)
			: SettableProperty("FaceColor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaceColor(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFaceColor(_state);
		}

		void unset() override
		{
			kit.UnsetFaceColor();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ColorInterpolationKitEdgeColorProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitEdgeColorProperty(
			QTreeWidget * tree,
			HPS::ColorInterpolationKit & kit)
			: SettableProperty("EdgeColor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEdgeColor(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetEdgeColor(_state);
		}

		void unset() override
		{
			kit.UnsetEdgeColor();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ColorInterpolationKitVertexColorProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitVertexColorProperty(
			QTreeWidget * tree,
			HPS::ColorInterpolationKit & kit)
			: SettableProperty("VertexColor")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertexColor(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVertexColor(_state);
		}

		void unset() override
		{
			kit.UnsetVertexColor();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ColorInterpolationKitFaceIndexProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitFaceIndexProperty(
			QTreeWidget * tree,
			HPS::ColorInterpolationKit & kit)
			: SettableProperty("FaceIndex")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowFaceIndex(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetFaceIndex(_state);
		}

		void unset() override
		{
			kit.UnsetFaceIndex();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ColorInterpolationKitEdgeIndexProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitEdgeIndexProperty(
			QTreeWidget * tree,
			HPS::ColorInterpolationKit & kit)
			: SettableProperty("EdgeIndex")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowEdgeIndex(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetEdgeIndex(_state);
		}

		void unset() override
		{
			kit.UnsetEdgeIndex();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ColorInterpolationKitVertexIndexProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitVertexIndexProperty(
			QTreeWidget * tree,
			HPS::ColorInterpolationKit & kit)
			: SettableProperty("VertexIndex")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			bool _isSet = this->kit.ShowVertexIndex(_state);
			if (!_isSet)
			{
				_state = true;
			}
			{
				auto boolProperty = new BoolProperty(tree, "State", _state);
				addChild(boolProperty);
				boolProperty->setupComboBox();
			}
			isSet(_isSet);
		}
	protected:
		void set() override
		{
			kit.SetVertexIndex(_state);
		}

		void unset() override
		{
			kit.UnsetVertexIndex();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
		QTreeWidget * tree;
	};

	class ColorInterpolationKitProperty : public RootProperty
	{
	public:
		ColorInterpolationKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowColorInterpolation(kit);
			auto prop_FaceColor = new ColorInterpolationKitFaceColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_FaceColor);
			prop_FaceColor->addSubItems();
			auto prop_EdgeColor = new ColorInterpolationKitEdgeColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EdgeColor);
			prop_EdgeColor->addSubItems();
			auto prop_VertexColor = new ColorInterpolationKitVertexColorProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VertexColor);
			prop_VertexColor->addSubItems();
			auto prop_FaceIndex = new ColorInterpolationKitFaceIndexProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_FaceIndex);
			prop_FaceIndex->addSubItems();
			auto prop_EdgeIndex = new ColorInterpolationKitEdgeIndexProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_EdgeIndex);
			prop_EdgeIndex->addSubItems();
			auto prop_VertexIndex = new ColorInterpolationKitVertexIndexProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_VertexIndex);
			prop_VertexIndex->addSubItems();
		}

		void Apply() override
		{
			key.UnsetColorInterpolation();
			key.SetColorInterpolation(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::ColorInterpolationKit kit;
	};

	class UpdateOptionsKitUpdateTypeProperty : public BaseProperty
	{
	public:
		UpdateOptionsKitUpdateTypeProperty(
			QTreeWidget * tree,
			HPS::UpdateOptionsKit & kit)
			: BaseProperty("UpdateType")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowUpdateType(_type);
			WindowUpdateTypeProperty * enumObject_0 = new WindowUpdateTypeProperty(tree, _type);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
		}
		void onChildChanged() override
		{
			kit.SetUpdateType(_type);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::UpdateOptionsKit & kit;
		HPS::Window::UpdateType _type;
		QTreeWidget * tree;
	};

	class UpdateOptionsKitTimeLimitProperty : public BaseProperty
	{
	public:
		UpdateOptionsKitTimeLimitProperty(
			QTreeWidget * tree,
			HPS::UpdateOptionsKit & kit)
			: BaseProperty("TimeLimit")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowTimeLimit(_time_limit);
			addChild(new DoubleProperty("Time Limit", _time_limit));
		}
		void onChildChanged() override
		{
			kit.SetTimeLimit(_time_limit);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::UpdateOptionsKit & kit;
		HPS::Time _time_limit;
		QTreeWidget * tree;
	};

	class UpdateOptionsKitProperty : public RootProperty
	{
	public:
		UpdateOptionsKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowUpdateOptions(kit);
			auto prop_UpdateType = new UpdateOptionsKitUpdateTypeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_UpdateType);
			prop_UpdateType->addSubItems();
			auto prop_TimeLimit = new UpdateOptionsKitTimeLimitProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_TimeLimit);
			prop_TimeLimit->addSubItems();
		}

		void Apply() override
		{
			key.SetUpdateOptions(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::UpdateOptionsKit kit;
	};

	class GridKitTypeProperty : public BaseProperty
	{
	public:
		GridKitTypeProperty(
			QTreeWidget * tree,
			HPS::GridKit & kit)
			: BaseProperty("Type")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowType(_type);
			GridTypeProperty * enumObject_0 = new GridTypeProperty(tree, _type);
			addChild(enumObject_0);
			enumObject_0->setupChoices();
		}
		void onChildChanged() override
		{
			kit.SetType(_type);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Grid::Type _type;
		QTreeWidget * tree;
	};

	class GridKitOriginProperty : public BaseProperty
	{
	public:
		GridKitOriginProperty(
			QTreeWidget * tree,
			HPS::GridKit & kit)
			: BaseProperty("Origin")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowOrigin(_origin);
			addChild(new PointProperty("Origin", _origin));
		}
		void onChildChanged() override
		{
			kit.SetOrigin(_origin);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Point _origin;
		QTreeWidget * tree;
	};

	class GridKitFirstPointProperty : public BaseProperty
	{
	public:
		GridKitFirstPointProperty(
			QTreeWidget * tree,
			HPS::GridKit & kit)
			: BaseProperty("FirstPoint")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowFirstPoint(_first_point);
			addChild(new PointProperty("First Point", _first_point));
		}
		void onChildChanged() override
		{
			kit.SetFirstPoint(_first_point);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Point _first_point;
		QTreeWidget * tree;
	};

	class GridKitSecondPointProperty : public BaseProperty
	{
	public:
		GridKitSecondPointProperty(
			QTreeWidget * tree,
			HPS::GridKit & kit)
			: BaseProperty("SecondPoint")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowSecondPoint(_second_point);
			addChild(new PointProperty("Second Point", _second_point));
		}
		void onChildChanged() override
		{
			kit.SetSecondPoint(_second_point);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Point _second_point;
		QTreeWidget * tree;
	};

	class GridKitFirstCountProperty : public BaseProperty
	{
	public:
		GridKitFirstCountProperty(
			QTreeWidget * tree,
			HPS::GridKit & kit)
			: BaseProperty("FirstCount")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowFirstCount(_first_count);
			addChild(new IntProperty("First Count", _first_count));
		}
		void onChildChanged() override
		{
			kit.SetFirstCount(_first_count);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GridKit & kit;
		int _first_count;
		QTreeWidget * tree;
	};

	class GridKitSecondCountProperty : public BaseProperty
	{
	public:
		GridKitSecondCountProperty(
			QTreeWidget * tree,
			HPS::GridKit & kit)
			: BaseProperty("SecondCount")
			, kit(kit)
			, tree(tree)
		{
		}

		void addSubItems()
		{
			this->kit.ShowSecondCount(_second_count);
			addChild(new IntProperty("Second Count", _second_count));
		}
		void onChildChanged() override
		{
			kit.SetSecondCount(_second_count);
			BaseProperty::onChildChanged();
		}

	private:
		HPS::GridKit & kit;
		int _second_count;
		QTreeWidget * tree;
	};

	class GridKitProperty : public RootProperty
	{
	public:
		GridKitProperty(
			QTreeWidgetItem * ctrl,
			HPS::GridKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			auto prop_Type = new GridKitTypeProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Type);
			prop_Type->addSubItems();
			auto prop_Origin = new GridKitOriginProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_Origin);
			prop_Origin->addSubItems();
			auto prop_FirstPoint = new GridKitFirstPointProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_FirstPoint);
			prop_FirstPoint->addSubItems();
			auto prop_SecondPoint = new GridKitSecondPointProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SecondPoint);
			prop_SecondPoint->addSubItems();
			auto prop_FirstCount = new GridKitFirstCountProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_FirstCount);
			prop_FirstCount->addSubItems();
			auto prop_SecondCount = new GridKitSecondCountProperty(ctrl->treeWidget(), kit);
			ctrl->addChild(prop_SecondCount);
			prop_SecondCount->addSubItems();
			ctrl->addChild(new GridKitPriorityProperty(kit));
			ctrl->addChild(new GridKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::GridKey key;
		HPS::GridKit kit;
	};

}
