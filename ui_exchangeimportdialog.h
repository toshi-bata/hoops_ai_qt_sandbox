#ifndef UI_EXCHANGEIMPORTDIALOG_H
#define UI_EXCHANGEIMPORTDIALOG_H

#include <QtCore/QVariant>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    #include <QtWidgets/QAction>
#endif
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>

QT_BEGIN_NAMESPACE

class Ui_ExchangeImportDialog {
  public:
    QProgressBar* progressBar;
    QLabel* importMessage;
    QTextBrowser* importLog;
    QCheckBox* keepOpenCheckbox;
    QPushButton* cancelButton;

    void setupUi(QDialog* ExchangeImportDialog)
    {
        if (ExchangeImportDialog->objectName().isEmpty())
            ExchangeImportDialog->setObjectName(QStringLiteral("ExchangeImportDialog"));
        ExchangeImportDialog->resize(400, 300);
        ExchangeImportDialog->setSizeGripEnabled(false);
        ExchangeImportDialog->setModal(true);
        progressBar = new QProgressBar(ExchangeImportDialog);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setGeometry(QRect(30, 20, 341, 23));
        progressBar->setValue(24);
        progressBar->setTextVisible(false);
        progressBar->setInvertedAppearance(false);
        importMessage = new QLabel(ExchangeImportDialog);
        importMessage->setObjectName(QStringLiteral("importMessage"));
        importMessage->setGeometry(QRect(30, 50, 341, 20));
        importMessage->setAlignment(Qt::AlignCenter);
        importLog = new QTextBrowser(ExchangeImportDialog);
        importLog->setObjectName(QStringLiteral("importLog"));
        importLog->setGeometry(QRect(30, 80, 341, 151));
        keepOpenCheckbox = new QCheckBox(ExchangeImportDialog);
        keepOpenCheckbox->setObjectName(QStringLiteral("keepOpenCheckbox"));
        keepOpenCheckbox->setGeometry(QRect(30, 250, 151, 21));
        cancelButton = new QPushButton(ExchangeImportDialog);
        cancelButton->setObjectName(QStringLiteral("cancelButton"));
        cancelButton->setGeometry(QRect(194, 250, 181, 23));
        cancelButton->setAutoDefault(false);

        retranslateUi(ExchangeImportDialog);

        QMetaObject::connectSlotsByName(ExchangeImportDialog);
    } // setupUi

    void retranslateUi(QDialog* ExchangeImportDialog)
    {
        ExchangeImportDialog->setWindowTitle(QApplication::translate("ExchangeImportDialog", "Importing File", 0));
        importMessage->setText(QApplication::translate("ExchangeImportDialog", "Import Message", 0));
        keepOpenCheckbox->setText(QApplication::translate("ExchangeImportDialog", "Keep Dialog Open", 0));
        cancelButton->setText(QApplication::translate("ExchangeImportDialog", "Cancel Import", 0));
    } // retranslateUi
};

namespace Ui {
    class ExchangeImportDialog: public Ui_ExchangeImportDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EXCHANGEIMPORTDIALOG_H
