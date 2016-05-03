#ifndef WINDOW_H
#define WINDOW_H

#include "localshare.h"
#include "discovery.h"
#include "settings.h"
#include "style.h"
#include "transfer_model.h"
#include "transfer.h"
#include "peer_list.h"

#include <QItemSelectionModel>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTreeView>
#include <QSplitter>
#include <QAction>
#include <QMenu>
#include <QStatusBar>
#include <QCloseEvent>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>

class Window : public QMainWindow {
	Q_OBJECT

	/* Main window of application.
	 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
	 * visibility. Application can be closed by tray menu -> quit.
	 */
private:
	QSystemTrayIcon * tray{nullptr};

	QAbstractItemView * peer_list_view{nullptr};
	PeerListModel * peer_list_model{nullptr};
	Transfer::ListModel * transfer_list_model{nullptr};

public:
	Window (const QString & blah, QWidget * parent = nullptr) : QMainWindow (parent) {
		// Start Server
		auto server = new Transfer::Server (this);
		qDebug () << "server" << server->port ();
		connect (server, &Transfer::Server::new_connection, this, &Window::incoming_connection);

		// Discovery setup
		auto username = Settings::Username ().get () + blah;
		auto service = new Discovery::Service (username, Const::service_name, server->port (), this);
		connect (service, &Discovery::Service::registered, this, &Window::service_registered);

		// Window
		setWindowTitle (Const::app_name);

		// Common actions
		auto action_send = new QAction (Icon::send (), tr ("&Send..."), this);
		action_send->setShortcuts (QKeySequence::Open);
		action_send->setEnabled (false);
		action_send->setStatusTip (tr ("Chooses a file to send to selected peers"));
		connect (action_send, &QAction::triggered, this, &Window::action_send_clicked);

		auto action_quit = new QAction (Icon::quit (), tr ("&Quit"), this);
		action_quit->setShortcuts (QKeySequence::Quit);
		action_quit->setMenuRole (QAction::QuitRole);
		action_quit->setStatusTip (tr ("Exits the application"));
		connect (action_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

		// Main widget = splitter
		auto splitter = new QSplitter (Qt::Vertical, this);
		splitter->setChildrenCollapsible (false);
		setCentralWidget (splitter);

		// Peer table
		{
			auto view = new QTreeView (splitter);
			view->setAlternatingRowColors (true);
			view->setRootIsDecorated (false);
			view->setAcceptDrops (true);
			view->setDropIndicatorShown (true);
			view->setSelectionBehavior (QAbstractItemView::SelectRows);
			view->setSelectionMode (QAbstractItemView::ExtendedSelection);
			view->setSortingEnabled (true);
			view->setStatusTip (
			    tr ("List of discovered peers (select at least one to enable File/Send...)"));
			peer_list_view = view;

			auto model = new PeerListModel (view);
			view->setModel (model);
			peer_list_model = model;

			connect (view->selectionModel (), &QItemSelectionModel::selectionChanged,
			         [=](const QItemSelection & selection) {
				         action_send->setEnabled (!selection.isEmpty ());
				       });
		}

		// Transfer table
		{
			auto view = new QTreeView (splitter);
			view->setAlternatingRowColors (true);
			view->setRootIsDecorated (false);
			view->setSelectionBehavior (QAbstractItemView::SelectRows);
			view->setSelectionMode (QAbstractItemView::NoSelection);
			view->setSortingEnabled (true);

			auto model = new Transfer::ListModel (view);
			view->setModel (model);
			transfer_list_model = model;
		}

		// System tray
		auto setting_show_tray = Settings::UseTray ().get ();
		tray = new QSystemTrayIcon (this);
		tray->setIcon (Icon::app ());
		tray->setVisible (setting_show_tray);
		connect (tray, &QSystemTrayIcon::activated, this, &Window::tray_activated);

		{
			auto menu = new QMenu (this); // cannot be child of tray (not Widget)
			tray->setContextMenu (menu);

			auto show_window = new QAction (tr ("Show &Window"), menu);
			connect (show_window, &QAction::triggered, this, &QWidget::show);

			menu->addAction (show_window);
			menu->addSeparator ();
			menu->addAction (action_quit);
		}

		// File menu
		{
			auto file = menuBar ()->addMenu (tr ("&File"));
			file->addAction (action_send);
			file->addSeparator ();
			file->addAction (action_quit);
		}

		// Preferences menu
		{
			auto pref = menuBar ()->addMenu (tr ("&Preferences"));

			auto use_tray = new QAction (tr ("Use System &Tray"), pref);
			use_tray->setCheckable (true);
			use_tray->setChecked (setting_show_tray);
			use_tray->setStatusTip (tr ("Enables use of persistent system tray icon"));
			connect (use_tray, &QAction::triggered, tray, &QSystemTrayIcon::setVisible);
			connect (use_tray, &QAction::triggered,
			         [=](bool checked) { Settings::UseTray ().set (checked); });

			pref->addAction (use_tray);
		}

		// Help menu
		{
			auto help = menuBar ()->addMenu (tr ("&Help"));

			auto about_qt = new QAction (tr ("About &Qt"), help);
			about_qt->setMenuRole (QAction::AboutQtRole);
			about_qt->setStatusTip (tr ("Information about Qt"));
			connect (about_qt, &QAction::triggered, qApp, &QApplication::aboutQt);

			auto about = new QAction (tr ("&About Localshare"), help);
			about->setMenuRole (QAction::AboutRole);
			about->setStatusTip (tr ("Information about Localshare"));
			connect (about, &QAction::triggered, [=] {
				QMessageBox::about (this, tr ("About Localshare"),
				                    tr ("Localshare is a small file sharing app for the local network."));
				// TODO improve description
			});

			help->addAction (about_qt);
			help->addAction (about);
		}

		// Status bar
		statusBar ()->showMessage (tr ("Starting up..."));

		show (); // Show everything

		// FIXME remove (test)
		peer_added (Peer{"NSA", "nsa.gov", QHostAddress ("192.44.29.1"), 42});
		peer_added (Peer{"ANSSI", "anssi.fr", QHostAddress ("8.8.8.8"), 1000});
	}

protected:
	void closeEvent (QCloseEvent * event) {
		if (event->spontaneous () && isVisible () && tray->isVisible ()) {
			// If tray is used, and user asked to close the window, then hide it instead.
			hide ();
			event->ignore ();
			// It accepts the close if the window was not visible (ex: OSX close request from menu)
		} else {
			event->accept ();
		}
	}

private slots:
	void service_registered (QString username) {
		// Complete the window when final username has been received from service registration
		setWindowTitle (QString ("%1 - %2").arg (Const::app_name).arg (username));
		statusBar ()->showMessage (tr ("Service registered as %1").arg (username));

		// Start browsing
		auto browser = new Discovery::Browser (username, Const::service_name, peer_list_model);
		connect (browser, &Discovery::Browser::added, this, &Window::peer_added);
		connect (browser, &Discovery::Browser::removed, this, &Window::peer_removed);
	}

	void tray_activated (QSystemTrayIcon::ActivationReason reason) {
		switch (reason) {
		case QSystemTrayIcon::DoubleClick: // Only double click
			setVisible (!isVisible ());      // Toggle window visibility
			break;
		default:
			break;
		}
	}

	void action_send_clicked (void) {
		// Select and send file to selection if clicked
		auto filepath = QFileDialog::getOpenFileName (this, tr ("Choose file to send..."));
		if (filepath.isEmpty ())
			return;
		auto selection = peer_list_view->selectionModel ()->selectedIndexes ();
		for (auto & index : selection) {
			if (index.column () == 0 && peer_list_model->has_item (index)) {
				// TreeView selected row generates 4 selection items ; only keep 1 per row
				auto item = peer_list_model->get_item_t<PeerItem *> (index);
				Q_ASSERT (item != nullptr);
				request_upload (item->get_peer (), filepath);
			}
		}
	}

	void peer_added (const Peer & peer) {
		auto item = new PeerItem (peer, peer_list_model);
		connect (item, &PeerItem::request_upload, this, &Window::request_upload);
		peer_list_model->append (item);
	}

	void peer_removed (const QString & username) { peer_list_model->delete_peer (username); }

	void request_upload (const Peer & peer, const QString & filepath) {
		auto upload = new Transfer::Upload (peer, filepath, transfer_list_model);
		transfer_list_model->append (upload);
	}

	void incoming_connection (QAbstractSocket * connection) {
		auto download = new Transfer::Download (connection);
		transfer_list_model->append (download);
	}
};

#endif