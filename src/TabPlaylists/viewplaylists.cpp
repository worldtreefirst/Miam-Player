#include "viewplaylists.h"

#include <library/jumptowidget.h>
#include <styling/paintablewidget.h>
#include <libraryorderdialog.h>
#include <musicsearchengine.h>
#include <settingsprivate.h>
#include <settings.h>
#include "dialogs/playlistdialog.h"

#include <QFileDialog>
#include <QProgressBar>
#include <QStandardPaths>

ViewPlaylists::ViewPlaylists(MediaPlayer *mediaPlayer)
	: AbstractViewPlaylists(mediaPlayer)
	, _searchDialog(new SearchDialog(this))
{
	this->setupUi(this);
	stopButton->setMediaPlayer(mediaPlayer);
	playbackModeButton->setToggleShuffleOnly(false);

	paintableWidget->setFrameBorder(false, false, true, false);
	seekSlider->setMediaPlayer(_mediaPlayer);
	tabPlaylists->init(_mediaPlayer);
	widgetSearchBar->setFrameBorder(false, false, true, false);

	connect(tabPlaylists, &TabPlaylist::updatePlaybackModeButton, playbackModeButton, &PlaybackModeButton::updateMode);
	connect(playbackModeButton, &PlaybackModeButton::aboutToChangeCurrentPlaylistPlaybackMode, tabPlaylists, &TabPlaylist::changeCurrentPlaylistPlaybackMode);

	Settings *settings = Settings::instance();
	volumeSlider->setValue(settings->volume() * 100);

	// Buttons
	SettingsPrivate *settingsPrivate = SettingsPrivate::instance();
	for (MediaButton *b : findChildren<MediaButton*>()) {
		if (!b) {
			continue;
		}
		b->setSize(settingsPrivate->buttonsSize());
		b->setIconFromTheme(settings->theme());
		b->setVisible(settingsPrivate->isMediaButtonVisible(b->objectName()));
	}

	searchBar->setFont(settingsPrivate->font(SettingsPrivate::FF_Library));

	/// XXX ?
	bool isEmpty = settingsPrivate->musicLocations().isEmpty();
	if (isEmpty) {
		widgetSearchBar->hide();
		changeHierarchyButton->hide();
		libraryHeader->hide();
		library->hide();
	}

	leftTabs->setCurrentIndex(settingsPrivate->value("leftTabsIndex").toInt());

	// Core
	connect(_mediaPlayer, &MediaPlayer::stateChanged, this, &ViewPlaylists::mediaPlayerStateHasChanged);

	// Main Splitter
	connect(splitter, &QSplitter::splitterMoved, _searchDialog, &SearchDialog::moveSearchDialog);

	connect(searchBar, &LibraryFilterLineEdit::aboutToStartSearch, library->model()->proxy(), &LibraryFilterProxyModel::findMusic);
	connect(settingsPrivate, &SettingsPrivate::librarySearchModeHasChanged, this, [=]() {
		QString text;
		searchBar->setText(text);
		library->model()->proxy()->findMusic(text);
	});

	// Media buttons
	connect(skipBackwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::skipBackward);
	connect(seekBackwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::seekBackward);
	connect(playButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::togglePlayback);
	connect(stopButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::stop);
	connect(seekForwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::seekForward);
	connect(skipForwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::skipForward);

	connect(filesystem, &FileSystemTreeView::folderChanged, addressBar, &AddressBar::init);
	connect(addressBar, &AddressBar::aboutToChangePath, filesystem, &FileSystemTreeView::reloadWithNewPath);
	addressBar->init(settingsPrivate->defaultLocationFileExplorer());

	connect(libraryHeader, &LibraryHeader::aboutToChangeSortOrder, library, &LibraryTreeView::changeSortOrder);

	// Factorize code with lambda slot connected to replicated signal
	auto reloadLibrary = [this]() {
		searchBar->setText(QString());
		_searchDialog->clear();
		this->update();
	};

	connect(libraryHeader, &LibraryHeader::aboutToChangeHierarchyOrder, reloadLibrary);
	connect(changeHierarchyButton, &QPushButton::toggled, this, [=]() {
		LibraryOrderDialog *libraryOrderDialog = new LibraryOrderDialog(this);
		libraryOrderDialog->move(libraryOrderDialog->mapFromGlobal(QCursor::pos()));
		libraryOrderDialog->show();
		connect(libraryOrderDialog, &LibraryOrderDialog::aboutToChangeHierarchyOrder, reloadLibrary);
	});

	for (TreeView *tab : this->findChildren<TreeView*>()) {
		connect(tab, &TreeView::aboutToInsertToPlaylist, tabPlaylists, &TabPlaylist::insertItemsToPlaylist);
		connect(tab, &TreeView::aboutToSendToTagEditor, this, &ViewPlaylists::aboutToSendToTagEditor);
	}

	// Send one folder to the music locations
	connect(filesystem, &FileSystemTreeView::aboutToAddMusicLocations, settingsPrivate, &SettingsPrivate::addMusicLocations);

	// Send music to the tag editor
	connect(tabPlaylists, &TabPlaylist::aboutToSendToTagEditor, this, &ViewPlaylists::aboutToSendToTagEditor);

	// Sliders
	connect(_mediaPlayer, &MediaPlayer::positionChanged, [=] (qint64 pos, qint64 duration) {
		if (duration > 0) {
			seekSlider->setValue(1000 * pos / duration);
			timeLabel->setTime(pos, duration);
		}
	});

	// Volume bar
	connect(volumeSlider, &QSlider::valueChanged, this, [=](int value) {
		_mediaPlayer->setVolume((qreal)value / 100.0);
	});

	connect(qApp, &QApplication::aboutToQuit, this, [=] {
		settingsPrivate->setValue("leftTabsIndex", leftTabs->currentIndex());
		settingsPrivate->setLastActivePlaylistGeometry(tabPlaylists->currentPlayList()->horizontalHeader()->saveState());
		settingsPrivate->sync();
	});

	// Filter the library when user is typing some text to find artist, album or tracks
	connect(searchBar, &LibraryFilterLineEdit::aboutToStartSearch, this, [=](const QString &text) {
		if (settingsPrivate->isExtendedSearchVisible()) {
			if (text.isEmpty()) {
				_searchDialog->clear();
			} else {
				_searchDialog->setSearchExpression(text);
				_searchDialog->moveSearchDialog(0, 0);
				_searchDialog->show();
				_searchDialog->raise();
			}
		}
	});

	connect(settingsPrivate, &SettingsPrivate::languageAboutToChange, this, [=](const QString &newLanguage) {
		QApplication::removeTranslator(&translator);
		translator.load(":/translations/tabPlaylists_" + newLanguage);
		QApplication::installTranslator(&translator);
	});

	// Init language
	translator.load(":/translations/tabPlaylists_" + settingsPrivate->language());
	QApplication::installTranslator(&translator);

	connect(settingsPrivate, &SettingsPrivate::viewPropertyChanged, this, &ViewPlaylists::setViewProperty);

	library->model()->load();

	this->installEventFilter(this);
}

void ViewPlaylists::addToPlaylist(const QList<QUrl> &tracks)
{
	tabPlaylists->insertItemsToPlaylist(-1, tracks);
}

int ViewPlaylists::selectedTracksInCurrentPlaylist() const
{
	return tabPlaylists->currentPlayList()->selectionModel()->selectedRows().count();
}

void ViewPlaylists::setDatabase(SqlDatabase *db)
{
	connect(db, &SqlDatabase::aboutToLoad, this, [=]() {
		QVBoxLayout *vbox = new QVBoxLayout;
		vbox->setMargin(0);
		vbox->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Preferred, QSizePolicy::Expanding));

		PaintableWidget *paintable = new PaintableWidget(library);
		paintable->setHalfTopBorder(false);
		paintable->setFrameBorder(false, true, true, false);
		vbox->addWidget(paintable);
		QVBoxLayout *vbox2 = new QVBoxLayout;
		vbox2->addWidget(new QLabel(tr("Your library is updating..."), paintable));
		vbox2->addWidget(new QProgressBar(paintable));
		paintable->setLayout(vbox2);
		library->setLayout(vbox);
	});

	connect(db->musicSearchEngine(), &MusicSearchEngine::progressChanged, this, [=](int p) {
		if (QProgressBar *progress = this->findChild<QProgressBar*>()) {
			progress->setValue(p);
		}
	});

	connect(db->musicSearchEngine(), &MusicSearchEngine::searchHasEnded, this, [=]() {
		auto l = library->layout();
		while (!l->isEmpty()) {
			if (QLayoutItem *i = l->takeAt(0)) {
				if (QWidget *w = i->widget()) {
					delete w;
				}
				delete i;
			}
		}
		delete library->layout();
		library->model()->load();
	});
}

bool ViewPlaylists::viewProperty(SettingsPrivate::ViewProperty vp) const
{
	switch (vp) {
	case SettingsPrivate::VP_MediaControls:
	case SettingsPrivate::VP_SearchArea:
	case SettingsPrivate::VP_PlaylistFeature:
	case SettingsPrivate::VP_HasAreaForRescan:
		return true;
	default:
		return AbstractView::viewProperty(vp);
	}
}

void ViewPlaylists::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange) {
		this->retranslateUi(this);
	} else {
		AbstractViewPlaylists::changeEvent(event);
	}
}

/** Open a new Dialog where one can add a folder to current playlist. */
void ViewPlaylists::openFolder(const QString &dir) const
{
	Settings::instance()->setValue("lastOpenedLocation", dir);
	QDirIterator it(dir, QDirIterator::Subdirectories);
	QStringList suffixes = FileHelper::suffixes(FileHelper::All, false);
	QList<QUrl> localTracks;
	while (it.hasNext()) {
		it.next();
		if (suffixes.contains(it.fileInfo().suffix())) {
			localTracks << QUrl::fromLocalFile(it.filePath());
		}
	}
	if (Miam::showWarning(tr("playlist"), localTracks.count()) == QMessageBox::Ok) {
		tabPlaylists->insertItemsToPlaylist(-1, localTracks);
	}
}

void ViewPlaylists::saveCurrentPlaylists()
{
	SettingsPrivate *settingsPrivate = SettingsPrivate::instance();
	QList<uint> list = settingsPrivate->lastPlaylistSession();
	list.clear();
	for (int i = 0; i < tabPlaylists->count(); i++) {
		Playlist *p = tabPlaylists->playlist(i);
		bool isOverwritting = p->id() != 0;
		uint id = tabPlaylists->playlistManager()->savePlaylist(p, isOverwritting, true);
		if (id != 0) {
			list.append(id);
		}
	}

	int idx = tabPlaylists->currentIndex();
	Playlist *p = tabPlaylists->playlist(idx);
	int m = p->mediaPlaylist()->playbackMode();

	settingsPrivate->setLastPlaylistSession(list);
	settingsPrivate->setValue("lastActiveTab", idx);
	settingsPrivate->setValue("lastActivePlaylistMode", m);
}

void ViewPlaylists::addExtFolders(const QList<QDir> &folders)
{
	bool isEmpty = tabPlaylists->currentPlayList()->mediaPlaylist()->isEmpty();

	QStringList tracks;
	for (QDir folder : folders) {
		QDirIterator it(folder.absolutePath(), FileHelper::suffixes(FileHelper::Standard, true), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			if (it.fileInfo().isFile()) {
				tracks << it.fileInfo().absoluteFilePath();
			}
		}
	}
	tracks.sort(Qt::CaseInsensitive);
	QList<QUrl> urls;
	for (QString t : tracks) {
		urls << QUrl::fromLocalFile(t);
	}
	tabPlaylists->insertItemsToPlaylist(-1, urls);

	// Automatically plays the first track
	if (isEmpty) {
		_mediaPlayer->setPlaylist(tabPlaylists->currentPlayList()->mediaPlaylist());
		_mediaPlayer->play();
	}
}

void ViewPlaylists::addPlaylist()
{
	tabPlaylists->addPlaylist();
}

void ViewPlaylists::initFileExplorer(const QDir &dir)
{
	addressBar->init(dir);
}

void ViewPlaylists::moveTracksDown()
{
	if (tabPlaylists->currentPlayList()) {
		tabPlaylists->currentPlayList()->moveTracksDown();
	}
}

void ViewPlaylists::moveTracksUp()
{
	if (tabPlaylists->currentPlayList()) {
		tabPlaylists->currentPlayList()->moveTracksUp();
	}
}

void ViewPlaylists::openFiles()
{
	QString audioFiles = tr("Audio files");
	Settings *settings = Settings::instance();
	QString lastOpenedLocation;
	QString defaultMusicLocation = QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first();
	if (settings->value("lastOpenedLocation").toString().isEmpty()) {
		lastOpenedLocation = defaultMusicLocation;
	} else {
		lastOpenedLocation = settings->value("lastOpenedLocation").toString();
	}

	audioFiles.append(" (" + FileHelper::suffixes(FileHelper::Standard, true).join(" ") + ")");
	audioFiles.append(";;Game Music Emu (" + FileHelper::suffixes(FileHelper::GameMusicEmu, true).join(" ") + ");;");
	audioFiles.append(tr("Every file type (*)"));

	QStringList files = QFileDialog::getOpenFileNames(this, tr("Choose some files to open"), lastOpenedLocation,
													  audioFiles);
	if (files.isEmpty()) {
		settings->setValue("lastOpenedLocation", defaultMusicLocation);
	} else {
		QFileInfo fileInfo(files.first());
		settings->setValue("lastOpenedLocation", fileInfo.absolutePath());
		QList<QUrl> tracks;
		for (QString file : files) {
			tracks << QUrl::fromLocalFile(file);
		}
		tabPlaylists->insertItemsToPlaylist(-1, tracks);
	}
}

void ViewPlaylists::openFolderPopup()
{
	Settings *settings = Settings::instance();
	QString lastOpenedLocation;
	QString defaultMusicLocation = QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first();
	if (settings->value("lastOpenedLocation").toString().isEmpty()) {
		lastOpenedLocation = defaultMusicLocation;
	} else {
		lastOpenedLocation = settings->value("lastOpenedLocation").toString();
	}
	QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a folder to open"), lastOpenedLocation);
	if (dir.isEmpty()) {
		settings->setValue("lastOpenedLocation", defaultMusicLocation);
	} else {
		this->openFolder(dir);
	}
}

void ViewPlaylists::openPlaylistManager()
{
	PlaylistDialog *playlistDialog = new PlaylistDialog(this);
	playlistDialog->setPlaylists(tabPlaylists->playlists());
	connect(playlistDialog, &PlaylistDialog::aboutToLoadPlaylist, tabPlaylists, &TabPlaylist::loadPlaylist);
	connect(playlistDialog, &PlaylistDialog::aboutToDeletePlaylist, tabPlaylists, &TabPlaylist::deletePlaylist);
	connect(playlistDialog, &PlaylistDialog::aboutToRenamePlaylist, tabPlaylists, &TabPlaylist::renamePlaylist);
	connect(playlistDialog, &PlaylistDialog::aboutToRenameTab, tabPlaylists, &TabPlaylist::renameTab);
	connect(playlistDialog, &PlaylistDialog::aboutToSavePlaylist, tabPlaylists, &TabPlaylist::savePlaylist);
	playlistDialog->exec();
}

void ViewPlaylists::removeCurrentPlaylist()
{
	tabPlaylists->removeCurrentPlaylist();
}

void ViewPlaylists::removeSelectedTracks()
{
	if (tabPlaylists->currentPlayList()) {
		tabPlaylists->currentPlayList()->removeSelectedTracks();
	}
}

void ViewPlaylists::setViewProperty(SettingsPrivate::ViewProperty vp, QVariant value)
{
	switch (vp) {
	case SettingsPrivate::VP_MediaControls:
		for (MediaButton *b : findChildren<MediaButton*>()) {
			b->setSize(value.toInt());
		}
		break;
	case SettingsPrivate::VP_SearchArea:
		if (_searchDialog) {
			_searchDialog->clear();
		}
		searchBar->clear();
		break;
	default:
		break;
	}
}

void ViewPlaylists::volumeSliderDecrease()
{
	volumeSlider->setValue(volumeSlider->value() - 5);
}

void ViewPlaylists::volumeSliderIncrease()
{
	volumeSlider->setValue(volumeSlider->value() + 5);
}

void ViewPlaylists::mediaPlayerStateHasChanged(QMediaPlayer::State state)
{
	if (state == QMediaPlayer::PlayingState) {
		QString iconPath;
		if (SettingsPrivate::instance()->hasCustomIcon("pauseButton")) {
			iconPath = SettingsPrivate::instance()->customIcon("pauseButton");
		} else {
			iconPath = ":/player/" + Settings::instance()->theme() + "/pause";
		}
		playButton->setIcon(QIcon(iconPath));
		seekSlider->setEnabled(true);
	} else {
		playButton->setIcon(QIcon(":/player/" + Settings::instance()->theme() + "/play"));
		seekSlider->setDisabled(state == QMediaPlayer::StoppedState);
		if (state == QMediaPlayer::StoppedState) {
			seekSlider->setValue(0);
			timeLabel->setTime(0, 0);
		}
	}
	// Remove bold font when player has stopped
	tabPlaylists->currentPlayList()->viewport()->update();
	seekSlider->update();
}