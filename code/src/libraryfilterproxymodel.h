#ifndef LIBRARYFILTERPROXYMODEL_H
#define LIBRARYFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class LibraryFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	LibraryFilterProxyModel(QObject *parent = 0);

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

protected:
	/** Redefined from QSortFilterProxyModel. */
	bool filterAcceptsRow(int sourceRow, const QModelIndex &parent) const;	

private:
	bool filterAcceptsRowItself(int sourceRow, const QModelIndex &sourceParent) const;
	bool hasAcceptedChildren(int sourceRow, const QModelIndex &sourceParent) const;

signals:
	void aboutToExpand(const QModelIndex &) const;
	
public slots:
	void loadCovers(const QModelIndex &index);
};

#endif // LIBRARYFILTERPROXYMODEL_H
