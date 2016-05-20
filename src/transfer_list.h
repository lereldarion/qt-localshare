#ifndef TRANSFER_LIST_H
#define TRANSFER_LIST_H

#include <QDebug>

#include <QApplication>
#include <QFlags>
#include <QStyle>
#include <QStyleOptionProgressBar>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QHeaderView>

#include "button_delegate.h"
#include "struct_item_model.h"
#include "style.h"

namespace Transfer {

/* Base transfer item class.
 * Subclassed by upload or download.
 */
class Item : public StructItem {
	Q_OBJECT

public:
	// Fields that are supported
	enum Field { FilenameField, PeerField, SizeField, ProgressField, StatusField, NbFields };

	// Buttons (see src/button_delegate.h)
	enum Role { ButtonRole = ButtonDelegate::ButtonRole };
	enum Button {
		NoButton = 0x0,
		AcceptButton = 0x1 << 0,
		CancelButton = 0x1 << 1,
		ChangeDownloadPathButton = 0x1 << 2,
		DeleteButton = 0x1 << 3
	};
	Q_DECLARE_FLAGS (Buttons, Button);

public:
	Item (QObject * parent = nullptr) : StructItem (NbFields, parent) {}

	virtual void button_clicked (int field, Button btn) = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS (Item::Buttons);

/* Transfer list model.
 * Adds headers and dispatch button_clicked.
 */
class Model : public StructItemModel {
	Q_OBJECT

public:
	Model (QObject * parent = nullptr) : StructItemModel (Item::NbFields, parent) {}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal))
			return {};
		switch (section) {
		case Item::FilenameField:
			return tr ("File");
		case Item::PeerField:
			return tr ("Peer");
		case Item::SizeField:
			return tr ("File size");
		case Item::ProgressField:
			return tr ("Transferred");
		case Item::StatusField:
			return tr ("Status");
		default:
			return {};
		}
	}

public slots:
	void button_clicked (const QModelIndex & index, int btn) {
		if (has_item (index))
			return get_item_t<Item *> (index)->button_clicked (index.column (), Item::Button (btn));
	}
};

inline QString size_to_string (qint64 size) {
	// Find correct unit to fit size.
	double num = size;
	double increment = 1024.0;
	static QString suffixes[] = {QObject::tr ("B"),
	                             QObject::tr ("KiB"),
	                             QObject::tr ("MiB"),
	                             QObject::tr ("GiB"),
	                             QObject::tr ("TiB"),
	                             QObject::tr ("PiB"),
	                             {}};
	int unit_idx = 0;
	while (num >= increment && !suffixes[unit_idx + 1].isEmpty ()) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + suffixes[unit_idx];
}

/* Paints a progressbar for the Progress field.
 */
class ProgressBarDelegate : public QStyledItemDelegate {
public:
	ProgressBarDelegate (QObject * parent = nullptr) : QStyledItemDelegate (parent) {}

	void paint (QPainter * painter, const QStyleOptionViewItem & option,
	            const QModelIndex & index) const Q_DECL_OVERRIDE {
		switch (index.column ()) {
		case Item::ProgressField: {
			// Draw a progress bar
			QStyleOptionProgressBar opt;
			init_progress_bar_style (opt, option, index);
			QApplication::style ()->drawControl (QStyle::CE_ProgressBar, &opt, painter);
		} break;
		default:
			QStyledItemDelegate::paint (painter, option, index);
		}
	}

	QSize sizeHint (const QStyleOptionViewItem & option,
	                const QModelIndex & index) const Q_DECL_OVERRIDE {
		switch (index.column ()) {
		case Item::ProgressField: {
			QStyleOptionProgressBar opt;
			init_progress_bar_style (opt, option, index);
			return QApplication::style ()->sizeFromContents (QStyle::CT_ProgressBar, &opt, QSize ());
		} break;
		default:
			return QStyledItemDelegate::sizeHint (option, index);
		}
	}

private:
	void init_progress_bar_style (QStyleOptionProgressBar & option, const QStyleOption & from,
	                              const QModelIndex & index) const {
		option.QStyleOption::operator= (from); // Take palette, ..., AND rect
		option.minimum = 0;
		option.maximum = 100;
		option.progress = -1;
		auto value = index.data (Qt::DisplayRole);
		if (value.canConvert<int> ()) {
			auto v = value.toInt ();
			option.progress = v;
			option.text = QString ("%1%").arg (v);
			option.textVisible = true;
		}
	}
};

/* Setup the ButtonDelegate buttons.
 */
class Delegate : public ButtonDelegate {
public:
	Delegate (QObject * parent = nullptr) : ButtonDelegate (parent) {
		set_inner_delegate (new ProgressBarDelegate (this));

		// Setup our specific buttons
		supported_buttons << SupportedButton{Item::AcceptButton, Icon::accept ()}
		                  << SupportedButton{Item::CancelButton, Icon::cancel ()}
		                  << SupportedButton{Item::ChangeDownloadPathButton,
		                                     Icon::change_download_path ()}
		                  << SupportedButton{Item::DeleteButton, Icon::delete_transfer ()};
	}
};

/* View, setup style and link to delegate.
 */
class View : public QTreeView {
private:
	Delegate * delegate{nullptr};

public:
	View (QWidget * parent = nullptr) : QTreeView (parent) {
		setAlternatingRowColors (true);
		setRootIsDecorated (false);
		setSelectionBehavior (QAbstractItemView::SelectRows);
		setSelectionMode (QAbstractItemView::NoSelection);
		setSortingEnabled (true);
		setMouseTracking (true);

		delegate = new Delegate (this);
		setItemDelegate (delegate);
	}

	void setModel (Model * model) {
		QTreeView::setModel (model);
		connect (delegate, &Delegate::button_clicked, model, &Model::button_clicked);

		auto h = header ();
		h->setStretchLastSection (false);
		h->setSectionResizeMode (QHeaderView::ResizeToContents);
		h->setSectionResizeMode (Item::ProgressField, QHeaderView::Stretch);
	}
};
}

#endif
