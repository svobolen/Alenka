#include "codeeditdialog.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QFile>

#include <string>
#include <regex>

using namespace std;

namespace
{

const regex commentre(R"((/\*([^*]|(\*+[^*/]))*\*+/)|(//.*))");

string stripComments(const QString& code)
{
	return regex_replace(code.toStdString(), commentre, "");
}

QString headerString;

}// namespace

CodeEditDialog::CodeEditDialog(QWidget* parent) : QDialog(parent)
{
	setWindowFlags(Qt::Window);
	setMinimumWidth(500);
	setMinimumHeight(500);

	QVBoxLayout* box = new QVBoxLayout;

	const char* help =
R"(Input and output:
	float out = 0;
	float in(int channelIndex);

Identifiers of the form "_*_" and all OpenCL reserved names and keywords are forbidden.

Definitions included in the source code that you can use:)";

	box->addWidget(new QLabel(help)); // TODO: Move these to help sub dialog.

	if (headerString.isNull())
	{
		QFile headerFile(":/montageHeader.cl");
		headerFile.open(QIODevice::ReadOnly);
		headerString = stripComments(headerFile.readAll()).c_str();
		headerString = headerString.trimmed();
	}

	QTextEdit* header = new QTextEdit(this);
	header->setPlainText(headerString);
	header->setReadOnly(true);
	box->addWidget(header);

	box->addWidget(new QLabel("Enter code here:", this));

	editor = new QTextEdit(this);
	box->addWidget(editor);

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	box->addWidget(buttonBox);

	setLayout(box);
}

CodeEditDialog::~CodeEditDialog()
{
}

QString CodeEditDialog::getText() const
{
	return editor->toPlainText();
}

void CodeEditDialog::errorMessageDialog(const QString& message, QWidget* parent)
{
	// TODO: Make a better error dialog.
	QMessageBox messageBox(parent);
	messageBox.setWindowTitle("Compilation Error");
	messageBox.setText("The code entered is not valid." + QString(200, ' '));
	messageBox.setDetailedText(message);
	messageBox.setIcon(QMessageBox::Critical);
	messageBox.exec();
}

void CodeEditDialog::setText(const QString& text)
{
	editor->setPlainText(text);
}

void CodeEditDialog::done(int r)
{
	if (QDialog::Accepted == r)
	{
		QString message;

		if (validator.validate(getText(), &message))
		{
			QDialog::done(r);
		}
		else
		{
			errorMessageDialog(message, this);
		}
	}
	else
	{
		QDialog::done(r);
	}
}
