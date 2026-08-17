#include "exchangeimportdialog.h"
#include "ui_exchangeimportdialog.h"

#include <QtCore/QTimer>
#include <mutex>

#ifdef USING_EXCHANGE
    #include "sprk_exchange.h"
#endif

std::mutex mtx;

namespace {
    bool isAbsolutePath(char const* path)
    {
        bool starts_with_drive_letter = strlen(path) >= 3 && isalpha(path[0]) && path[1] == ':' && strchr("/\\", path[2]);

        bool starts_with_network_drive_prefix = strlen(path) >= 2 && strchr("/\\", path[0]) && strchr("/\\", path[1]);

        return starts_with_drive_letter || starts_with_network_drive_prefix;
    }
} // namespace

HPS::EventHandler::HandleResult ImportStatusEventHandler::Handle(HPS::Event const* in_event)
{
    if (_progress_dlg) {
        HPS::UTF8 message = static_cast<HPS::ImportStatusEvent const*>(in_event)->import_status_message;
        if (message.IsValid()) {
            bool update_message = true;

            if (message == HPS::UTF8("Import and Tessellation"))
                _progress_dlg->SetMessage(HPS::UTF8("Stage 1/3 : Import and Tessellation"));
            else if (message == HPS::UTF8("Creating Graphics Database"))
                _progress_dlg->SetMessage(HPS::UTF8("Stage 2/3 : Creating Graphics Database"));
            else if (isAbsolutePath(message)) {
                std::string path(message);
                message = path.substr(path.find_last_of("/\\") + 1).c_str();
                _progress_dlg->AddLogEntry(HPS::UTF8(message));
                update_message = false;
            }
            else
                update_message = false;

            if (update_message)
                _progress_dlg->StartStatusTimer();
        }
    }
    return HPS::EventHandler::HandleResult::Handled;
}

ExchangeImportDialog::ExchangeImportDialog(HPS::IONotifier& in_notifier, HPSWidget* in_widget, QWidget* parent):
    QDialog(parent), ui(new Ui::ExchangeImportDialog), widget(in_widget), notifier(in_notifier), keep_dialog_open(false),
    success(false), please_start_status_timer(false)
{
    ui->setupUi(this);

    import_status_event = new ImportStatusEventHandler(this);
    timer = new QTimer();
    status_timer = new QTimer();
}

ExchangeImportDialog::~ExchangeImportDialog()
{
    delete ui;
    delete import_status_event;
    delete timer;
    delete status_timer;
}

void ExchangeImportDialog::showEvent(QShowEvent* /*event*/)
{
    connect(timer, SIGNAL(timeout()), this, SLOT(on_timer()));
    connect(status_timer, SIGNAL(timeout()), this, SLOT(on_status_timer()));

    timer->start(50);
    ui->progressBar->setRange(0, 0);
    ui->importMessage->setText(QString("Stage 1/3 : Import and Tessellation"));

    HPS::Database::GetEventDispatcher().Subscribe(*import_status_event, HPS::Object::ClassID<HPS::ImportStatusEvent>());
}

void ExchangeImportDialog::StartStatusTimer() { please_start_status_timer = true; }

void ExchangeImportDialog::on_cancelButton_clicked()
{
    if (success)
        close();
    else {
        notifier.Cancel();

        timer->stop();
        close();
    }
}

void ExchangeImportDialog::on_keepOpenCheckbox_clicked()
{
    if (ui->keepOpenCheckbox->isChecked())
        keep_dialog_open = true;
    else
        keep_dialog_open = false;
}

void ExchangeImportDialog::on_timer()
{
    try {
        if (please_start_status_timer) {
            status_timer->start(50);
            please_start_status_timer = false;
        }

        {
            // update the import log
            std::lock_guard<std::mutex> lock(mtx);

            if (!log_messages.empty()) {
                for (auto it = log_messages.begin(), e = log_messages.end(); it != e; ++it)
                    ui->importLog->append(QString("Reading ") + QString(*it));
                log_messages.clear();
            }
        }

        HPS::IOResult status;
        status = notifier.Status();

        if (status != HPS::IOResult::InProgress) {
            HPS::Database::GetEventDispatcher().UnSubscribe(*import_status_event);
            timer->stop();

            if (status == HPS::IOResult::Success)
                PerformInitialUpdate();

            if (!keep_dialog_open)
                close();
        }
    }
    catch (HPS::IOException const&) {
        // notifier not yet created
    }
}

void ExchangeImportDialog::on_status_timer()
{
    // update the import message
    if (message.IsValid()) {
        ui->importMessage->setText(QString(message));

        status_timer->stop();
        message = HPS::UTF8();
    }
}

void ExchangeImportDialog::AddLogEntry(HPS::UTF8 const& in_log_entry)
{
    std::lock_guard<std::mutex> lock(mtx);
    log_messages.push_back(in_log_entry);
}

void ExchangeImportDialog::PerformInitialUpdate()
{
#ifdef USING_EXCHANGE
    ui->cancelButton->setEnabled(false);
    ui->importMessage->setText(QString("Stage 3/3 : Performing Initial Update"));

    HPS::CADModel cadModel;
    cadModel = static_cast<HPS::Exchange::ImportNotifier>(notifier).GetCADModel();

    if (!cadModel.Empty()) {
        cadModel.GetModel().GetSegmentKey().GetPerformanceControl().SetStaticModel(HPS::Performance::StaticModel::Attribute);
        widget->AttachView(cadModel.ActivateDefaultCapture().FitWorld());
    }

    HPS::UpdateNotifier updateNotifier = widget->getCanvas()->UpdateWithNotifier(HPS::Window::UpdateType::Exhaustive);
    updateNotifier.Wait();

    ui->cancelButton->setEnabled(true);
    ui->cancelButton->setText(QString("Close Dialog"));
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(100);
    ui->importMessage->setText(QString("Import Complete"));

    success = true;
#endif
}