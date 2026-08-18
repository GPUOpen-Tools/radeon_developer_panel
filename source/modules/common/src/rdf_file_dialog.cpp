// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDF File Dialog class implementation

#include "rdf_file_dialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

#include "formatting.h"
#include "model/rdf_file_model.h"
#include "model/rdf_file_proxy_model.h"

#include "ui_rdf_file_dialog.h"

uint32_t RdfFileDialog::instance_count_ = 0;

static constexpr int kPositionOffsetX = 200;

namespace
{
    class ChunkDataDialog : public QDialog
    {
    public:
        ChunkDataDialog(const QString& title, const QByteArray& data, QWidget* parent = nullptr)
            : QDialog(parent)
        {
            setWindowTitle(title);
            setMinimumSize(600, 400);
            auto* layout = new QVBoxLayout(this);
            auto* text   = new QPlainTextEdit(this);
            text->setReadOnly(true);

            // JSON format
            QJsonParseError err;
            QJsonDocument   doc = QJsonDocument::fromJson(data, &err);
            if (err.error == QJsonParseError::NoError && (doc.isObject() || doc.isArray()))
            {
                QString pretty = doc.toJson(QJsonDocument::Indented);
                text->setPlainText(pretty);
            }
            // fallback ASCII
            else
            {
                QString   display;
                const int bytes_per_line = 64;  // wrap lines for readability
                for (int i = 0; i < data.size(); ++i)
                {
                    unsigned char c = static_cast<unsigned char>(data[i]);
                    display += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
                    if ((i + 1) % bytes_per_line == 0)
                    {
                        display += '\n';
                    }
                }
                if (!display.endsWith('\n'))
                {
                    display += '\n';
                }
                text->setPlainText(display);
            }
            layout->addWidget(text);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
            layout->addWidget(buttons);
        }
    };
}  // namespace

RdfFileDialog::RdfFileDialog(const QString& title, const QString& path, QWidget* parent)
    : QDialog(parent)
    , ui_(new Ui::RdfFileDialog)
{
    setFocusPolicy(Qt::StrongFocus);
    ui_->setupUi(this);
    ui_->file_view->setSortingEnabled(true);
    ui_->file_view->horizontalHeader()->setSectionsClickable(true);

    QFileInfo file_info(path);
    setWindowTitle(title + " - " + file_info.fileName());
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    RdfFileProxyModel* proxy_model = new RdfFileProxyModel(this);
    RdfFileModel*      model       = new RdfFileModel(path);
    proxy_model->setSourceModel(model);
    ui_->file_view->setModel(proxy_model);

    if (model->IsLoadOk())
    {
        ui_->total_size_label->setText(QString("Uncompressed Size: %1").arg(Formatting::FormatBytesPow2(model->GetTotalSize(), 1)));
    }
    else
    {
        ui_->total_size_label->setText(QString("Failed to load file: %1").arg(model->GetErrorString()));
        ui_->file_view->setEnabled(false);
    }

    ui_->lineEdit->setClearButtonEnabled(true);
    ui_->lineEdit->setPlaceholderText("Search...");
    ui_->lineEdit->addAction(QIcon(":/magnifying-glass-solid.svg"), QLineEdit::ActionPosition::LeadingPosition);
    ui_->lineEdit->setFocusPolicy(Qt::ClickFocus);
    connect(ui_->lineEdit, &QLineEdit::textChanged, proxy_model, QOverload<const QString&>::of(&QSortFilterProxyModel::setFilterRegularExpression));

    // Context menu to view chunk data when available
    ui_->file_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_->file_view, &QWidget::customContextMenuRequested, this, [this, proxy_model, model](const QPoint& pos) {
        QModelIndex proxy_index = ui_->file_view->indexAt(pos);
        if (!proxy_index.isValid())
        {
            return;
        }
        QModelIndex source_index = proxy_model->mapToSource(proxy_index);
        int         row          = source_index.row();
        if (!model->HasChunkData(row))
        {
            return;  // No data to show
        }

        QMenu    menu(this);
        QAction* view_data_action = menu.addAction("View Chunk Data...");
        QAction* chosen           = menu.exec(ui_->file_view->viewport()->mapToGlobal(pos));
        if (chosen == view_data_action)
        {
            QByteArray      data       = model->GetChunkData(row);
            QString         identifier = model->GetChunkIdentifier(row);
            ChunkDataDialog dlg(QString("Chunk Data: %1").arg(identifier), data, this);
            dlg.exec();
        }
    });

    instance_count_++;
}

void RdfFileDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    auto geometry = this->geometry();
    geometry.moveLeft(kPositionOffsetX * instance_count_);

    setGeometry(geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

void RdfFileDialog::closeEvent(QCloseEvent* event)
{
    QDialog::closeEvent(event);

    instance_count_--;
}

void RdfFileDialog::keyPressEvent(QKeyEvent* event)
{
    QDialog::keyPressEvent(event);

    if (event->matches(QKeySequence::Delete))
    {
        close();
    }
}
