#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStack>

#include "ui_mainwindow.h"
#include "dialogs/customizeoptionsdialog.h"
#include "dialogs/dragdropdialog.h"
#include "dialogs/playlistmanager.h"
#include "library/librarytreeview.h"
#include "mediabutton.h"
#include <mediaplayer.h>
#include "playbackmodewidgetfactory.h"
#include "searchdialog.h"

#include <model/librarysqlmodel.h>

#include "uniquelibrary.h"
#include "sqldatabase.h"

class CustomizeThemeDialog;

/**
 * \brief The MainWindow class is the entry point of this audio player.
 */
class MainWindow : public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT
private:
	/** Dialog for with lots of different sections to customize the look of MiamPlayer. */
	CustomizeThemeDialog *customizeThemeDialog;

	/** Dialog to organize your playlists: saving, exporting, loading. */
	PlaylistManager *playlistManager;

	/** Popup shown to one when tracks are dropped from another application to MiamPlayer. */
	DragDropDialog *dragDropDialog;

	/** Displays and animates the media button "PlaybackMode". */
	PlaybackModeWidgetFactory *playbackModeWidgetFactory;

	/** The MediaPlayer manipulates audio tracks. */
	QSharedPointer<MediaPlayer> _mediaPlayer;

	/** WIP. View object: display all your tracks in a huge and page. */
	UniqueLibrary *_uniqueLibrary;

	/** Model for the Table "tracks". */
	LibrarySqlModel *_librarySqlModel;

	/** Custom SQLite DataBase which defines few useful tables for playlists and tracks. */
	SqlDatabase _sqlDatabase;

	SearchDialog *_searchDialog;

public:
	// Play, pause, stop, etc.
	QList<MediaButton*> mediaButtons;
	CustomizeOptionsDialog *customizeOptionsDialog;

	MainWindow(QWidget *parent = 0);

	void appendToCurrentPlaylist(const QStringList &files);

	void dispatchDrop(QDropEvent *event);

	void init();

	/** Plugins. */
	void loadPlugins();

	QWeakPointer<MediaPlayer> mediaPlayer() const { return _mediaPlayer; }

	AbstractSearchDialog * searchDialog() const { return _searchDialog; }

	/** Set up all actions and behaviour. */
	void setupActions();

	/** Update fonts for menu and context menus. */
	void updateFonts(const QFont &font);

	static QMessageBox::StandardButton showWarning(const QString &target, int count);

protected:
	/** Redefined to be able to retransltate User Interface at runtime. */
	virtual void changeEvent(QEvent *event);

	virtual void closeEvent(QCloseEvent *event);

	virtual void dragEnterEvent(QDragEnterEvent *event);

	virtual void dragMoveEvent(QDragMoveEvent *event);

	virtual void dropEvent(QDropEvent *event);

	virtual bool event(QEvent *event);

	virtual void moveEvent(QMoveEvent *);

public slots:
	void processArgs(const QStringList &args);

private slots:
	void bindShortcut(const QString&, const QKeySequence &keySequence);

	void mediaPlayerStateHasChanged(QMediaPlayer::State state);

	void openFiles();

	void openFolder();

	void showTabPlaylists();

	void showTagEditor();
};

#endif // MAINWINDOW_H
