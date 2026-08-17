#pragma once
#include "HPSWidget.h"
#include "QtWidgets/QDialog"
#include <deque>
#include "sprk.h"

namespace Ui {
    class ExchangeImportDialog;
}

class ExchangeImportDialog;

class ImportStatusEventHandler: public HPS::EventHandler {
  public:
    ImportStatusEventHandler(): HPS::EventHandler(), _progress_dlg(nullptr) {}

    ImportStatusEventHandler(ExchangeImportDialog* in_progress_dlg): HPS::EventHandler(), _progress_dlg(in_progress_dlg) {}

    virtual ~ImportStatusEventHandler() { Shutdown(); }

    HandleResult Handle(HPS::Event const* in_event) override;

  private:
    ExchangeImportDialog* _progress_dlg;
};

class ExchangeImportDialog: public QDialog {
    Q_OBJECT

  public:
    explicit ExchangeImportDialog(HPS::IONotifier& in_notifier, HPSWidget* in_widget, QWidget* parent = 0);
    ~ExchangeImportDialog();

    void SetMessage(HPS::UTF8 const& in_message) { message = in_message; }

    void AddLogEntry(HPS::UTF8 const& in_log_entry);

    bool WasImportSuccessful() { return success; }

    void StartStatusTimer();

  private slots:
    void on_cancelButton_clicked();
    void on_keepOpenCheckbox_clicked();
    void on_timer();
    void on_status_timer();

  private:
    Ui::ExchangeImportDialog* ui;
    QTimer* timer;
    QTimer* status_timer;
    HPSWidget* widget;
    HPS::IONotifier& notifier;
    bool keep_dialog_open;
    bool success;
    bool please_start_status_timer;
    HPS::UTF8 message;
    std::deque<HPS::UTF8> log_messages;
    ImportStatusEventHandler* import_status_event;

    void PerformInitialUpdate();
    void showEvent(QShowEvent* event) override;
};
