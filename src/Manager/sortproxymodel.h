#ifndef SORTPROXYMODEL_H
#define SORTPROXYMODEL_H

#include <QSortFilterProxyModel>

class SortProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	explicit SortProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
	//virtual ~SortProxyModel() override {}

protected:
	virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

#endif // SORTPROXYMODEL_H
