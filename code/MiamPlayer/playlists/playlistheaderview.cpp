#include "playlistheaderview.h"

#include "settingsprivate.h"
#include <QApplication>
#include <QStylePainter>

#include <QtDebug>

QStringList PlaylistHeaderView::labels = QStringList() << "#"
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Title")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Album")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Length")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Artist")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Rating")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Year")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "Source")
													   << QT_TRANSLATE_NOOP("PlaylistHeaderView", "TrackDAO");

PlaylistHeaderView::PlaylistHeaderView(QWidget *parent) :
	QHeaderView(Qt::Horizontal, parent)
{
	this->setHighlightSections(false);
	this->setSectionsMovable(true);
	this->setSectionResizeMode(QHeaderView::Interactive);
	this->setStretchLastSection(true);
	this->setFrameShape(QFrame::NoFrame);

	// Context menu on header of columns
	columns = new QMenu(this);
	connect(columns, &QMenu::triggered, this, [=](const QAction *action) {
		int columnIndex = action->data().toInt();
		this->setSectionHidden(columnIndex, !this->isSectionHidden(columnIndex));
	});

	// Initialize font from settings
	SettingsPrivate *settings = SettingsPrivate::instance();
	this->setFont(settings->font(SettingsPrivate::FF_Playlist));

	connect(settings, &SettingsPrivate::fontHasChanged, this, [=](SettingsPrivate::FontFamily ff, const QFont &newFont) {
		if (ff == SettingsPrivate::FF_Playlist) {
			this->setFont(newFont);
		}
	});
}

void PlaylistHeaderView::setFont(const QFont &newFont)
{
	QFont font = newFont;
	font.setPointSizeF(font.pointSizeF() * 0.8);
	QHeaderView::setFont(newFont);
	int h = fontMetrics().height() * 1.25;
	if (h >= 30) {
		this->setMinimumHeight(h);
		this->setMaximumHeight(h);
	} else {
		this->setMinimumHeight(30);
		this->setMaximumHeight(30);
	}
}

/** Redefined for dynamic translation. */
void PlaylistHeaderView::changeEvent(QEvent *event)
{
	QHeaderView::changeEvent(event);
	if (model() && event->type() == QEvent::LanguageChange) {
		for (int i = 0; i < count(); i++) {
			model()->setHeaderData(i, Qt::Horizontal, tr(labels.at(i).toStdString().data()), Qt::DisplayRole);
		}
	}
}

void PlaylistHeaderView::setModel(QAbstractItemModel *model)
{
	QHeaderView::setModel(model);
	for (int i = 0; i < count(); i++) {
		QString label = labels.at(i);

		// Exclude hidden columns (should be improved?)
		if (label != "TrackDAO") {
			model->setHeaderData(i, Qt::Horizontal, tr(label.toStdString().data()), Qt::DisplayRole);

			// Match actions with columns using index of labels
			QAction *actionColumn = new QAction(label, this);
			actionColumn->setData(i);
			actionColumn->setEnabled(actionColumn->text() != tr("Title"));
			actionColumn->setCheckable(true);
			actionColumn->setChecked(!isSectionHidden(i));

			// Then populate the context menu
			columns->addAction(actionColumn);
		}
	}
}

void PlaylistHeaderView::contextMenuEvent(QContextMenuEvent *event)
{
	// Initialize values for the Header (label and horizontal resize mode)
	for (int i = 0; i < columns->actions().count(); i++) {
		QAction *action = columns->actions().at(i);
		if (action) {
			action->setText(tr(labels.at(i).toStdString().data()));
		}
	}

	for (int i = 0; i < columns->actions().count(); i++) {
		QAction *action = columns->actions().at(i);
		if (action) {
			action->setChecked(!this->isSectionHidden(i));
		}
	}
	columns->exec(mapToGlobal(event->pos()));
}

/** Redefined. */
void PlaylistHeaderView::paintSection(QPainter *, const QRect &rect, int logicalIndex) const
{
	QStylePainter p(this->viewport());
	QStyleOptionHeader opt;
	opt.initFrom(this);
	QLinearGradient vLinearGradient(rect.topLeft(), rect.bottomLeft());
	/// XXX
	QPalette palette = QApplication::palette();
	if (SettingsPrivate::instance()->isCustomColors()) {
		vLinearGradient.setColorAt(0, palette.base().color().lighter(110));
		vLinearGradient.setColorAt(1, palette.base().color());
	} else {
		vLinearGradient.setColorAt(0, palette.base().color());
		vLinearGradient.setColorAt(1, palette.window().color());
	}
	p.fillRect(rect, QBrush(vLinearGradient));
	p.setPen(opt.palette.windowText().color());
	p.drawText(rect.adjusted(5, 0, 0, 0), Qt::AlignCenter, model()->headerData(logicalIndex, Qt::Horizontal).toString());

	if (rect.contains(mapFromGlobal(QCursor::pos()))) {
		p.save();
		p.setPen(palette.highlight().color());
		p.drawLine(rect.x(), rect.y() + rect.height() / 4, rect.x(), rect.y() + 3 * rect.height() / 4);
		p.drawLine(rect.x() + rect.width() - 1, rect.y() + rect.height() / 4, rect.x() + rect.width() - 1, rect.y() + 3 * rect.height() / 4);
		p.restore();
	}

	// Frame line
	p.setPen(QApplication::palette().mid().color());
	p.drawLine(rect.bottomLeft(), rect.bottomRight());

	if (isLeftToRight() && logicalIndex == 0) {
		p.drawLine(rect.topLeft(), rect.bottomLeft());
	} else if (!isLeftToRight() && logicalIndex == count() - 1){
		p.drawLine(rect.topLeft(), rect.bottomLeft());
	}
}
