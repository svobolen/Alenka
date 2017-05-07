#ifndef FILTERMANAGER_H
#define FILTERMANAGER_H

#include <QWidget>

class FilterVisualizer;
class OpenDataFile;
class QPlainTextEdit;
class QSlider;

class FilterManager : public QWidget
{
	Q_OBJECT

public:
	explicit FilterManager(QWidget* parent = nullptr);

	void changeFile(OpenDataFile* file);

private:
	OpenDataFile* file;
	FilterVisualizer* filterVisulizer;
	QPlainTextEdit* multipliersEdit;
	QSlider* channelSlider;

private slots:
	void setMultipliersText();
	void applyMultipliers();
};

#endif // FILTERMANAGER_H
