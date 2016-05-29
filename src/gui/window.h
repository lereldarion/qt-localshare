#pragma once
#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

#include <QApplication>
#include <QCloseEvent>

#include <QItemSelectionModel>
#include <QSystemTrayIcon>

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>

#include <QAction>
#include <QMenu>
#include <QSplitter>

#include "core/localshare.h"
#include "core/settings.h"
#include "core/server.h"
#include "gui/discovery_subsystem.h"
#include "gui/style.h"
#include "gui/peer_list.h"
#include "gui/transfer_list.h"
#include "gui/transfer_download.h"
#include "gui/transfer_upload.h"

/* Main window of application.
 * Handles most high level GUI functions (the rest is provided by view/models).
 * It also link together functionnality from peer list, transfer list, discovery.
 *
 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
 * visibility. Application can be closed by tray menu -> quit.
 *
 * The Transfer Server should be alive for the lifetime of Window.
 */
class Window : public QMainWindow {
	Q_OBJECT

private:
	Discovery::LocalDnsPeer * local_peer{nullptr};
	Discovery::SubSystem * discovery_subsystem{nullptr};

	QSystemTrayIcon * tray{nullptr};

	QAbstractItemView * peer_list_view{nullptr};
	PeerList::Model * peer_list_model{nullptr};
	Transfer::Model * transfer_list_model{nullptr};

public:
	Window (QWidget * parent = nullptr) : QMainWindow (parent) {
		{
			// Start Server
			auto server = new Transfer::Server (this);
			connect (server, &Transfer::Server::new_connection, this, &Window::incoming_connection);

			// Local peer
			using Discovery::LocalDnsPeer;
			local_peer = new LocalDnsPeer (server->port (), this);
			connect (local_peer, &LocalDnsPeer::username_changed, this, &Window::set_window_title);

			// Restartable discovery system
			discovery_subsystem = new Discovery::SubSystem (local_peer, this);
			setStatusBar (discovery_subsystem);
			connect (discovery_subsystem, &Discovery::SubSystem::new_discovered_peer, this,
			         &Window::new_discovered_peer);
		}

		// Common actions
		auto action_send = new QAction (Icon::send (), tr ("&Send..."), this);
		action_send->setShortcuts (QKeySequence::Open);
		action_send->setEnabled (false);
		action_send->setStatusTip (tr ("Chooses a file to send to selected peers"));
		connect (action_send, &QAction::triggered, this, &Window::action_send_clicked);

		auto action_add_peer = new QAction (Icon::add_peer (), tr ("&Add manual peer"), this);
		action_add_peer->setStatusTip (tr ("Add a peer entry to fill manually"));
		connect (action_add_peer, &QAction::triggered, this, &Window::new_manual_peer);

		auto action_quit = new QAction (Icon::quit (), tr ("&Quit"), this);
		action_quit->setShortcuts (QKeySequence::Quit);
		action_quit->setMenuRole (QAction::QuitRole);
		action_quit->setStatusTip (tr ("Exits the application"));
		connect (action_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

		// Main widget is a splitter
		auto splitter = new QSplitter (Qt::Vertical, this);
		splitter->setChildrenCollapsible (false);
		setCentralWidget (splitter);

		// Peer table
		{
			auto view = new PeerList::View (splitter);
			peer_list_view = view;

			auto model = new PeerList::Model (view);
			view->setModel (model);
			peer_list_model = model;

			connect (view->selectionModel (), &QItemSelectionModel::selectionChanged,
			         [=](const QItemSelection & selection) {
				         action_send->setEnabled (!selection.isEmpty ());
				       });
		}

		// Transfer table
		{
			auto view = new Transfer::View (splitter);
			auto model = new Transfer::Model (view);
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

			auto show_window = new QAction (Icon::restore (), tr ("Show &Window"), menu);
			connect (show_window, &QAction::triggered, this, &QWidget::show);

			menu->addAction (show_window);
			menu->addSeparator ();
			menu->addAction (action_quit);
		}

		// File menu
		{
			auto file = menuBar ()->addMenu (tr ("&Application"));
			file->addAction (action_send);
			file->addAction (action_add_peer);
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

			auto download_path = new QAction (tr ("Set default download &path..."), pref);
			download_path->setStatusTip (tr ("Sets the path used by default to store downloaded files."));
			connect (download_path, &QAction::triggered, [=](void) {
				Settings::DownloadPath path;
				auto new_path =
				    QFileDialog::getExistingDirectory (this, tr ("Set default download path"), path.get ());
				if (!new_path.isEmpty ())
					path.set (new_path);
			});

			auto download_auto = new QAction (tr ("Always &accept downloads"), pref);
			download_auto->setCheckable (true);
			download_auto->setChecked (Settings::DownloadAuto ().get ());
			download_auto->setStatusTip (tr ("Enable automatic accept of all incoming download offers."));
			connect (download_auto, &QAction::triggered,
			         [=](bool checked) { Settings::DownloadAuto ().set (checked); });

			auto change_username = new QAction (tr ("Change username..."), pref);
			change_username->setStatusTip ("Set a new username in settings and discovery");
			connect (change_username, &QAction::triggered, [=](void) {
				QString new_username =
				    QInputDialog::getText (this, tr ("Select new username"), tr ("Username:"),
				                           QLineEdit::Normal, local_peer->get_requested_username ());
				if (!new_username.isEmpty ())
					local_peer->set_requested_username (new_username);
			});

			pref->addAction (use_tray);
			pref->addSeparator ();
			pref->addAction (download_path);
			pref->addAction (download_auto);
			pref->addSeparator ();
			pref->addAction (change_username);
		}

		// Help menu
		{
			auto help = menuBar ()->addMenu (tr ("&Help"));

			auto about_qt = new QAction (tr ("About &Qt"), help);
			about_qt->setMenuRole (QAction::AboutQtRole);
			about_qt->setStatusTip (tr ("Information about Qt"));
			connect (about_qt, &QAction::triggered, qApp, &QApplication::aboutQt);

			auto about = new QAction (tr ("&About %1").arg (Const::app_display_name), help);
			about->setMenuRole (QAction::AboutRole);
			about->setStatusTip (tr ("Information about %1").arg (Const::app_display_name));
			connect (about, &QAction::triggered, this, &Window::show_about);

			help->addAction (about_qt);
			help->addAction (about);
		}

		// Toolbar
		{
			auto tool_bar = addToolBar (tr ("Application"));
			tool_bar->setMovable (false);
			tool_bar->setObjectName ("toolbar");
			tool_bar->addAction (action_send);
			tool_bar->addAction (action_add_peer);
		}

		setUnifiedTitleAndToolBarOnMac (true);
		set_window_title ();
		restoreGeometry (Settings::Geometry ().get ());
		restoreState (Settings::WindowState ().get ());
		show (); // Show everything
	}

	~Window () {
		Settings::Geometry ().set (saveGeometry ());
		Settings::WindowState ().set (saveState ());
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
	void set_window_title (void) {
		auto username = local_peer->get_username ();
		if (username.isEmpty ())
			setWindowTitle (tr ("Unregistered"));
		else
			setWindowTitle (username);
	}

	void tray_activated (QSystemTrayIcon::ActivationReason reason) {
		if (reason == QSystemTrayIcon::DoubleClick)
			setVisible (!isVisible ()); // Toggle window visibility
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
				request_upload (peer_list_model->get_item_t<PeerList::Item *> (index)->get_peer (),
				                filepath);
			}
		}
	}

	// Peer creation

	void new_manual_peer (void) {
		auto item = new PeerList::ManualItem (peer_list_model);
		connect (item, &PeerList::Item::request_upload, this, &Window::request_upload);
		peer_list_model->append (item);
	}

	void new_discovered_peer (Discovery::DnsPeer * peer) {
		auto item = new PeerList::DiscoveryItem (peer);
		connect (item, &PeerList::Item::request_upload, this, &Window::request_upload);
		peer_list_model->append (item);
	}

	// Transfer creation

	void request_upload (const Peer & peer, const QString & filepath) {
		auto upload =
		    new Transfer::UploadOld (peer, filepath, local_peer->get_username (), transfer_list_model);
		transfer_list_model->append (upload);
	}

	void incoming_connection (QAbstractSocket * connection) {
		auto download = new Transfer::DownloadOld (connection);
		transfer_list_model->append (download);
	}

	// About message

	void show_about (void) {
		auto msg = new QMessageBox (this);
		msg->setAttribute (Qt::WA_DeleteOnClose);
		msg->setIconPixmap (
		    Icon::app ().pixmap (style ()->pixelMetric (QStyle::PM_MessageBoxIconSize)));
		msg->setWindowTitle (tr ("About %1").arg (Const::app_display_name));
		msg->setText (tr ("<p>%1 v%2 is a small file sharing application for the local network.</p>")
		                  .arg (Const::app_display_name)
		                  .arg (Const::app_version));
		msg->setInformativeText (
		    tr ("<p>It is designed to easily send files to peers across the local network. "
		        "It can be viewed as a netcat with auto discovery of peers and a nice interface. "
		        "Drag & drop a file on a peer, "
		        "or select peers and click on send to initiate a transfer. "
		        "It also supports manually adding peers by ip/hostname/port, "
		        "but this will not work if the destination is behind firewalls.</p>"

		        "<p>Be careful of the automatic download option. "
		        "It prevents you from rejecting unwanted file offers, "
		        "and could allow attackers to fill your disk. "
		        "As a general rule, be careful if you use %1 on a public network.</p>"

		        "<p>Without automatic download, you must accept each transfer manually. "
		        "Before accepting, you can change the destination by clicking the directory icon. "
		        "You can also change the default destination in the preferences.</p>"

		        "<p>If using the system tray icon, %1 acts like a small daemon. "
		        "Hiding/closing the window only reduces it to the system tray. "
		        "It can be useful for long transfers, but do not forget to close it !</p>"

		        "<p>Copyright (C) 2016 François Gindraud.</p>"
		        "<p><a href=\"https://github.com/lereldarion/qt-localshare\">Github Link</a></p>")
		        .arg (Const::app_display_name));
		msg->exec ();
	}
};

#endif
