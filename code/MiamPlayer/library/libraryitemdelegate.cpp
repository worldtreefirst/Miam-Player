#include "libraryitemdelegate.h"

#include <cover.h>
#include <settingsprivate.h>
#include "librarytreeview.h"
#include "../playlists/starrating.h"
#include "../model/albumdao.h"
#include "styling/imageutils.h"

#include <QApplication>
#include <QDir>
#include <QImageReader>

#include <memory>

#include <QtDebug>

qreal LibraryItemDelegate::_iconOpacity = 1.0;

LibraryItemDelegate::LibraryItemDelegate(LibraryTreeView *libraryTreeView, LibraryFilterProxyModel *proxy) :
	QStyledItemDelegate(proxy), _libraryTreeView(libraryTreeView), _timer(new QTimer(this))
{
	_proxy = proxy;
	_libraryModel = qobject_cast<QStandardItemModel*>(_proxy->sourceModel());
	_showCovers = SettingsPrivate::instance()->isCoversEnabled();
	_timer->setTimerType(Qt::PreciseTimer);
	_timer->setInterval(10);
	connect(_timer, &QTimer::timeout, this, [=]() {
		_iconOpacity += 0.01;
		_libraryTreeView->viewport()->update();
		if (_iconOpacity >= 1) {
			_timer->stop();
			_iconOpacity = 1.0;
		} else {
			// Restart the timer until r has reached the maximum (1.0)
			_timer->start();
		}
	});
}

void LibraryItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	painter->save();
	auto settings = SettingsPrivate::instance();
	painter->setFont(settings->font(SettingsPrivate::FF_Library));
	QStandardItem *item = _libraryModel.data()->itemFromIndex(_proxy.data()->mapToSource(index));
	QStyleOptionViewItem o = option;
	initStyleOption(&o, index);
	o.palette = QApplication::palette();
	if (QGuiApplication::isLeftToRight()) {
		o.rect.adjust(0, 0, -20, 0);
	} else {
		o.rect.adjust(19, 0, 0, 0);
	}

	// Removes the dotted rectangle to the focused item
	o.state &= ~QStyle::State_HasFocus;
	switch (item->type()) {
	case Miam::IT_Album:
		this->paintRect(painter, o);
		this->drawAlbum(painter, o, static_cast<AlbumItem*>(item));
		break;
	case Miam::IT_Artist:
		this->paintRect(painter, o);
		this->drawArtist(painter, o, static_cast<ArtistItem*>(item));
		break;
	case Miam::IT_Disc:
		this->paintRect(painter, o);
		this->drawDisc(painter, o, static_cast<DiscItem*>(item));
		break;
	case Miam::IT_Separator:
		this->drawLetter(painter, o, static_cast<SeparatorItem*>(item));
		break;
	case Miam::IT_Track: {
		SettingsPrivate::LibrarySearchMode lsm = settings->librarySearchMode();
		if (settings->isBigCoverEnabled() && ((_proxy->filterRegExp().isEmpty() && lsm == SettingsPrivate::LSM_Filter) ||
				lsm == SettingsPrivate::LSM_HighlightOnly)) {
			this->paintCoverOnTrack(painter, o, static_cast<TrackItem*>(item));
		} else {
			this->paintRect(painter, o);
		}
		this->drawTrack(painter, o, static_cast<TrackItem*>(item));
		break;
	}
	default:
		QStyledItemDelegate::paint(painter, o, index);
		break;
	}
	painter->restore();
}

/** Redefined to always display the same height for albums, even for those without one. */
QSize LibraryItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	SettingsPrivate *settings = SettingsPrivate::instance();
	QStandardItem *item = _libraryModel->itemFromIndex(_proxy->mapToSource(index));
	if (settings->isCoversEnabled() && item->type() == Miam::IT_Album) {
		QFontMetrics fmf(settings->font(SettingsPrivate::FF_Library));
		return QSize(option.rect.width(), qMax(fmf.height(), settings->coverSize() + 2));
	} else {
		return QStyledItemDelegate::sizeHint(option, index);
	}
}

/** Albums have covers usually. */
void LibraryItemDelegate::drawAlbum(QPainter *painter, QStyleOptionViewItem &option, AlbumItem *item) const
{
	/// XXX: reload cover with high resolution when one has increased coverSize (every 64px)
	static QImageReader imageReader;
	SettingsPrivate *settings = SettingsPrivate::instance();
	int coverSize = settings->coverSize();

	QString coverPath;
	if (settings->isCoversEnabled() && _showCovers) {
		coverPath = item->data(Miam::DF_CoverPath).toString();
		if (!_loadedCovers.contains(item) && !coverPath.isEmpty()) {
			FileHelper fh(coverPath);
			// If it's an inner cover, load it
			if (FileHelper::suffixes().contains(fh.fileInfo().suffix())) {
				// qDebug() << Q_FUNC_INFO << "loading internal cover from file";
				std::unique_ptr<Cover> cover(fh.extractCover());
				QPixmap p;
				if (cover && p.loadFromData(cover->byteArray(), cover->format())) {
					p = p.scaled(coverSize, coverSize);
					if (!p.isNull()) {
						item->setIcon(p);
						_loadedCovers.insert(item, true);
					}
				}
			} else {
				// qDebug() << Q_FUNC_INFO << "loading external cover from harddrive";
				imageReader.setFileName(QDir::fromNativeSeparators(coverPath));
				imageReader.setScaledSize(QSize(coverSize, coverSize));
				item->setIcon(QPixmap::fromImage(imageReader.read()));
				_loadedCovers.insert(item, true);
			}
		}
	}

	if (settings->isCoversEnabled()) {
		painter->save();
		QRect cover;
		if (QGuiApplication::isLeftToRight()) {
			cover = QRect(option.rect.x() + 1, option.rect.y() + 1, coverSize, coverSize);
		} else {
			cover = QRect(option.rect.width() + 19 - coverSize - 1, option.rect.y() + 1, coverSize, coverSize);
		}
		// If font size is greater than the cover, align it
		if (coverSize < option.rect.height() - 2) {
			painter->translate(0, (option.rect.height() - 1 - coverSize) / 2);
		}

		if (coverPath.isEmpty()) {
			if (_iconOpacity <= 0.25) {
				painter->setOpacity(_iconOpacity);
			} else {
				painter->setOpacity(0.25);
			}
			painter->drawPixmap(cover, QPixmap(":/icons/disc"));
		} else {
			painter->setOpacity(_iconOpacity);
			QPixmap p = option.icon.pixmap(QSize(coverSize, coverSize));
			painter->drawPixmap(cover, p);
		}
		painter->restore();
	}

	// Add an icon on the right if album is from some remote location
	bool isRemote = item->data(Miam::DF_IsRemote).toBool();
	int offsetWidth = 0;
	if (isRemote) {
		int iconSize = 31;
		QRect iconRemoteRect(option.rect.x() + option.rect.width() - (iconSize + 4),
							 (option.rect.height() - iconSize)/ 2 + option.rect.y() + 2,
							 iconSize,
							 iconSize);
		QPixmap iconRemote(item->iconPath());
		painter->save();
		painter->setOpacity(0.5);
		painter->drawPixmap(iconRemoteRect, iconRemote);
		painter->restore();
		offsetWidth = iconSize;
	}

	option.textElideMode = Qt::ElideRight;
	QRect rectText;
	if (settings->isCoversEnabled()) {
		// It's possible to have missing covers in your library, so we need to keep alignment.
		if (QGuiApplication::isLeftToRight()) {
			rectText = QRect(option.rect.x() + coverSize + 5,
							 option.rect.y(),
							 option.rect.width() - (coverSize + 7) - offsetWidth,
							 option.rect.height() - 1);
		} else {
			rectText = QRect(option.rect.x(), option.rect.y(), option.rect.width() - coverSize - 5, option.rect.height());
		}
	} else {
		rectText = QRect(option.rect.x() + 5, option.rect.y(), option.rect.width() - 5, option.rect.height());
	}
	QFontMetrics fmf(settings->font(SettingsPrivate::FF_Library));
	QString s = fmf.elidedText(option.text, Qt::ElideRight, rectText.width());
	this->paintText(painter, option, rectText, s, item);
}

void LibraryItemDelegate::drawArtist(QPainter *painter, QStyleOptionViewItem &option, ArtistItem *item) const
{
	auto settings = SettingsPrivate::instance();
	QFontMetrics fmf(settings->font(SettingsPrivate::FF_Library));
	option.textElideMode = Qt::ElideRight;
	QRect rectText;
	QString s;
	if (QGuiApplication::isLeftToRight()) {
		QPoint topLeft(option.rect.x() + 5, option.rect.y());
		rectText = QRect(topLeft, option.rect.bottomRight());
		QString custom = item->data(Miam::DF_CustomDisplayText).toString();
		if (!custom.isEmpty() && settings->isReorderArtistsArticle()) {
			/// XXX: paint articles like ", the" in gray? Could be nice
			s = fmf.elidedText(custom, Qt::ElideRight, rectText.width());
		} else {
			s = fmf.elidedText(option.text, Qt::ElideRight, rectText.width());
		}
	} else {
		rectText = QRect(option.rect.x(), option.rect.y(), option.rect.width() - 5, option.rect.height());
		s = fmf.elidedText(option.text, Qt::ElideRight, rectText.width());
	}
	this->paintText(painter, option, rectText, s, item);
}

void LibraryItemDelegate::drawDisc(QPainter *painter, QStyleOptionViewItem &option, DiscItem *item) const
{
	option.state = QStyle::State_None;
	QPointF p1 = option.rect.bottomLeft(), p2 = option.rect.bottomRight();
	p1.setX(p1.x() + 2);
	p2.setX(p2.x() - 2);
	painter->setPen(Qt::gray);
	painter->drawLine(p1, p2);
	QStyledItemDelegate::paint(painter, option, item->index());
}

void LibraryItemDelegate::drawLetter(QPainter *painter, QStyleOptionViewItem &option, SeparatorItem *item) const
{
	// One cannot interact with an alphabetical separator
	option.state = QStyle::State_None;
	option.font.setBold(true);
	QPointF p1 = option.rect.bottomLeft(), p2 = option.rect.bottomRight();
	p1.setX(p1.x() + 2);
	p2.setX(p2.x() - 2);
	painter->setPen(Qt::gray);
	painter->drawLine(p1, p2);
	QStyledItemDelegate::paint(painter, option, item->index());
}

void LibraryItemDelegate::drawTrack(QPainter *painter, QStyleOptionViewItem &option, TrackItem *track) const
{
	/// XXX: it will be a piece of cake to add an option that one can customize how track number will be displayed
	/// QString title = settings->libraryItemTitle();
	/// for example: zero padding
	auto settings = SettingsPrivate::instance();
	if (settings->isStarDelegates()) {
		int r = track->data(Miam::DF_Rating).toInt();
		QStyleOptionViewItem copy(option);
		copy.rect = QRect(0, option.rect.y(), option.rect.x(), option.rect.height());
		/// XXX: create an option to display stars right to the text, and fade them if text is too large
		//copy.rect = QRect(option.rect.x() + option.rect.width() - option.rect.height() * 5, option.rect.y(), option.rect.height() * 5, option.rect.height());
		StarRating starRating(r);
		if (r > 0) {
			starRating.paintStars(painter, copy, StarRating::ReadOnly);
		} else if (settings->isShowNeverScored()) {
			starRating.paintStars(painter, copy, StarRating::NoStarsYet);
		}
	}
	int trackNumber = track->data(Miam::DF_TrackNumber).toInt();
	if (trackNumber > 0) {
		option.text = QString("%1").arg(trackNumber, 2, 10, QChar('0')).append(". ").append(track->text());
	} else {
		option.text = track->text();
	}
	QFontMetrics fmf(SettingsPrivate::instance()->font(SettingsPrivate::FF_Library));
	option.textElideMode = Qt::ElideRight;
	QString s;
	QRect rectText;
	if (QGuiApplication::isLeftToRight()) {
		QPoint topLeft(option.rect.x() + 5, option.rect.y());
		rectText = QRect(topLeft, option.rect.bottomRight());
		s = fmf.elidedText(option.text, Qt::ElideRight, rectText.width());
	} else {
		rectText = QRect(option.rect.x(), option.rect.y(), option.rect.width() - 5, option.rect.height());
		s = fmf.elidedText(option.text, Qt::ElideRight, rectText.width());
	}
	this->paintText(painter, option, rectText, s, track);
}

void LibraryItemDelegate::paintCoverOnTrack(QPainter *painter, const QStyleOptionViewItem &opt, const TrackItem *track) const
{
	SettingsPrivate *settings = SettingsPrivate::instance();
	const QImage *image = _libraryTreeView->expandedCover(track->parent());
	if (image && !image->isNull()) {
		// Copy QStyleOptionViewItem to be able to expand it to the left, and take the maximum available space
		QStyleOptionViewItem option(opt);
		option.rect.setX(0);

		int totalHeight = track->model()->rowCount(track->parent()->index()) * option.rect.height();
		QImage scaled;
		QRect subRect;
		if (totalHeight > option.rect.width()) {
			scaled = image->scaledToWidth(option.rect.width());
			subRect = option.rect.translated(option.rect.width() - scaled.width(), -option.rect.y() + option.rect.height() * track->row());
		} else {
			scaled = image->scaledToHeight(totalHeight);
			int dx = option.rect.width() - scaled.width();
			subRect = option.rect.translated(-dx, -option.rect.y() + option.rect.height() * track->row());
		}

		// Fill with white when there are too much tracks to paint (height of all tracks is greater than the scaled image)
		QImage subImage = scaled.copy(subRect);
		if (scaled.height() < subRect.y() + subRect.height()) {
			subImage.fill(option.palette.base().color());
		}

		painter->save();
		painter->setOpacity(1 - settings->bigCoverOpacity());
		painter->drawImage(option.rect, subImage);

		// Over paint black pixel in white
		QRect t(option.rect.x(), option.rect.y(), option.rect.width() - scaled.width(), option.rect.height());
		QImage white(t.size(), QImage::Format_ARGB32);
		white.fill(option.palette.base().color());
		painter->setOpacity(1.0);
		painter->drawImage(t, white);

		// Create a mix with 2 images: first one is a 3 pixels subimage of the album cover which is expanded to the left border
		// The second one is a computer generated gradient focused on alpha channel
		QImage leftBorder = scaled.copy(0, subRect.y(), 3, option.rect.height());
		if (!leftBorder.isNull()) {

			// Because the expanded border can look strange to one, is blurred with some gaussian function
			leftBorder = leftBorder.scaled(t.width(), option.rect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			leftBorder = ImageUtils::blurred(leftBorder, leftBorder.rect(), 10, false);
			painter->setOpacity(1 - settings->bigCoverOpacity());
			painter->drawImage(t, leftBorder);

			QLinearGradient linearAlphaBrush(0, 0, leftBorder.width(), 0);
			linearAlphaBrush.setColorAt(0, QApplication::palette().base().color());
			linearAlphaBrush.setColorAt(1, Qt::transparent);

			painter->setOpacity(1.0);
			painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
			painter->setPen(Qt::NoPen);
			painter->setBrush(linearAlphaBrush);
			painter->drawRect(t);
		}
		painter->restore();
	}

	// Display a light selection rectangle when one is moving the cursor
	painter->save();
	QColor color = opt.palette.highlight().color();
	color.setAlphaF(0.66);
	if (opt.state.testFlag(QStyle::State_MouseOver) && !opt.state.testFlag(QStyle::State_Selected)) {
		if (settings->isCustomColors()) {
			painter->setPen(opt.palette.highlight().color().darker(100));
			painter->setBrush(color.lighter());
		} else {
			painter->setPen(opt.palette.highlight().color());
			painter->setBrush(color.lighter(160));
		}
		painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));
	} else if (opt.state.testFlag(QStyle::State_Selected)) {
		// Display a not so light rectangle when one has chosen an item. It's darker than the mouse over
		if (settings->isCustomColors()) {
			painter->setPen(opt.palette.highlight().color().darker(150));
			painter->setBrush(color);
		} else {
			painter->setPen(opt.palette.highlight().color());
			painter->setBrush(color.lighter(150));
		}
		painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));
	}
	painter->restore();
}

void LibraryItemDelegate::paintRect(QPainter *painter, const QStyleOptionViewItem &option) const
{
	// Display a light selection rectangle when one is moving the cursor
	if (option.state.testFlag(QStyle::State_MouseOver) && !option.state.testFlag(QStyle::State_Selected)) {
		painter->save();
		if (SettingsPrivate::instance()->isCustomColors()) {
			painter->setPen(option.palette.highlight().color().darker(100));
			painter->setBrush(option.palette.highlight().color().lighter());
		} else {
			painter->setPen(option.palette.highlight().color());
			painter->setBrush(option.palette.highlight().color().lighter(160));
		}
		painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
		painter->restore();
	} else if (option.state.testFlag(QStyle::State_Selected)) {
		// Display a not so light rectangle when one has chosen an item. It's darker than the mouse over
		painter->save();
		if (SettingsPrivate::instance()->isCustomColors()) {
			painter->setPen(option.palette.highlight().color().darker(150));
			painter->setBrush(option.palette.highlight().color());
		} else {
			painter->setPen(option.palette.highlight().color());
			painter->setBrush(option.palette.highlight().color().lighter(150));
		}
		painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
		painter->restore();
	}
}

/** Check if color needs to be inverted then paint text. */
void LibraryItemDelegate::paintText(QPainter *p, const QStyleOptionViewItem &opt, const QRect &rectText, const QString &text, const QStandardItem *item) const
{
	p->save();
	if (text.isEmpty()) {
		p->setPen(opt.palette.mid().color());
		QFontMetrics fmf(SettingsPrivate::instance()->font(SettingsPrivate::FF_Library));
		p->drawText(rectText, Qt::AlignVCenter, fmf.elidedText(tr("(empty)"), Qt::ElideRight, rectText.width()));
	} else {
		if (opt.state.testFlag(QStyle::State_Selected) || opt.state.testFlag(QStyle::State_MouseOver)) {
			if ((opt.palette.highlight().color().lighter(160).saturation() - opt.palette.highlightedText().color().saturation()) < 128) {
				p->setPen(opt.palette.text().color());
			} else {
				p->setPen(opt.palette.highlightedText().color());
			}
		}
		if (item->data(Miam::DF_Highlighted).toBool()) {
			QFont f = p->font();
			f.setBold(true);
			p->setFont(f);
		}
		p->drawText(rectText, Qt::AlignVCenter, text);
	}
	p->restore();
}

void LibraryItemDelegate::displayIcon(bool b)
{
	_showCovers = b;
	if (_showCovers) {
		_timer->start();
	} else {
		_iconOpacity = 0;
	}
}
