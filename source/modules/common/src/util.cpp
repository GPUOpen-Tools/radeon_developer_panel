// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Util implementation

#include "util.h"
#include "definitions.h"

#ifdef WIN32
#include <Shlobj.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#undef CopyFile
#undef MessageBox

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QProcess>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QWidget>

#include <qt_common/custom_widgets/message_overlay.h>

static constexpr auto kInvalidPermissionTitle    = "Invalid permissions";
static constexpr auto kInvalidPermissionDesc     = "Output directory is not a writable location";
static constexpr auto kInvalidPathAlertKeyFormat = "invalid-output-path-%1";

QWidget* Util::GetRDPMainWindow()
{
    QWidget* main_window = nullptr;

    for (auto widgets = qApp->allWidgets(); const auto& w : widgets)
    {
        if (w->objectName() == kPanelMainWindow)
        {
            main_window = w;
            break;
        }
    }

    return main_window;
}

void Util::BrowseToFile(const QString& path)
{
    const QFileInfo file_info(path);
    const QString   dir_path = file_info.absoluteDir().absolutePath();

    QDesktopServices::openUrl(QUrl::fromLocalFile(dir_path));
}

bool Util::SerializeMemoryBuffer(const uint8_t* data, const uint32_t offset, const uint32_t size, const QString& filepath)
{
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly))
    {
        QDataStream out(&file);
        out.writeRawData(reinterpret_cast<const char*>(data + offset), static_cast<int>(size));
        file.close();

        return true;
    }

    return false;
}

bool Util::SerializeMemoryBuffer(const uint8_t* data, const uint32_t offset, const uint32_t size, const QString& root, const QString& filename)
{
    QFile file(root + QDir::separator() + filename);
    if (file.open(QIODevice::WriteOnly))
    {
        QDataStream out(&file);
        out.writeRawData(reinterpret_cast<const char*>(data + offset), static_cast<int>(size));
        file.close();

        return true;
    }

    return false;
}

void Util::DeserializeMemoryBuffer(uint8_t* dst, const QString& src, const uint64_t read)
{
    QFile file(src);
    if (file.open(QIODevice::ReadOnly))
    {
        const uint32_t kFileSize    = file.size();
        const uint32_t kBytesToRead = read <= kFileSize ? read : kFileSize;

        QDataStream in(&file);
        in.readRawData(reinterpret_cast<char*>(dst), static_cast<int>(kBytesToRead));
        file.close();
    }
}

QString Util::GetSystemDate(const QDateTime& dt, const QString& format)
{
    QString out = dt.toString();

    if (!format.isEmpty())
    {
        out = dt.toString(format);
    }

    return out;
}

bool Util::CopyFile(const QString& src, const QString& dst, const bool overwrite)
{
    bool success = true;

    if (overwrite)
    {
        if (QFile::exists(dst))
        {
            success = QFile::remove(dst);
        }
    }

    if (success)
    {
        success = QFile::copy(src, dst);
    }

    return success;
}

bool Util::VerifyOutputDirectory(const QString& path, const bool keep_directory, const bool show_alerts)
{
    bool result = true;

    const QString current_output_path = RemoveDirMacros(path);

    if (path.isEmpty())
    {
        if (show_alerts)
        {
            MessageOverlay::CriticalAsync("Invalid output directory path", "Output directory cannot be empty");
        }

        return false;
    }

    if (const QDir dir(current_output_path); !dir.exists())
    {
        if (dir.mkpath(current_output_path))
        {
            // Check if the directory is writable
            if (!IsDirectoryWritable(dir))
            {
                if (show_alerts)
                {
                    MessageOverlay::CriticalAsync(kInvalidPermissionTitle, kInvalidPermissionDesc);
                }

                result = false;
            }

            if (!keep_directory)
            {
                // Delete the directory we just created.
                std::ignore = dir.rmpath(current_output_path);
            }
        }
        else
        {
            if (show_alerts)
            {
                // Add in a key based on the output path to avoid duplicate messages
                MessageOverlay::CriticalAsync("Invalid output directory path",
                                              QString("Failed to create output path: %1").arg(current_output_path),
                                              QString(kInvalidPathAlertKeyFormat).arg(current_output_path));
            }

            result = false;
        }
    }
    else
    {
        // Check if the directory is writable
        if (!IsDirectoryWritable(dir))
        {
            if (show_alerts)
            {
                MessageOverlay::CriticalAsync(kInvalidPermissionTitle, kInvalidPermissionDesc);
            }

            result = false;
        }
    }

    return result;
}

bool Util::IsDirectoryWritable(const QDir& dir)
{
    const QString current_output_path = RemoveDirMacros(dir.absolutePath());
    const auto    no_macro_dir        = QDir(current_output_path);

    // See if the user can write to the selected folder
    QFile      file(no_macro_dir.filePath("temp.txt"));
    const bool success = file.open(QIODevice::WriteOnly);
    if (success)
    {
        // Remove the temp file before moving on
        file.remove();
    }

    return success;
}

QString Util::RemoveDirMacros(const QString& path)
{
    QString macros_removed = path;

    // Check if the path contains the special macros
    if (macros_removed.contains(kAppNameMacro))
    {
        // Find the first occurrence of the macro in path and grab the parent
        const auto index = macros_removed.indexOf(kAppNameMacro);
        macros_removed   = macros_removed.left(index > 0 ? index - 1 : index);
    }
    else if (macros_removed.contains(kApplicationNameMacro))
    {
        // Find the first occurrence of the macro in path and grab the parent
        const auto index = macros_removed.indexOf(kApplicationNameMacro);
        macros_removed   = macros_removed.left(index > 0 ? index - 1 : index);
    }

    return macros_removed;
}

QString Util::GetSettingsFolder(const QString& folder_name)
{
    QString folder = "";

#ifdef _WIN32
    LPWSTR        path = nullptr;
    const HRESULT hr   = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path);
    Q_UNUSED(hr)
    Q_ASSERT(hr == S_OK);

    folder = QString::fromUtf16(reinterpret_cast<const char16_t*>(path));
    folder.append(QDir::separator());
    folder.append(folder_name);

#else
    struct passwd* pw = getpwuid(getuid());
    if (pw != nullptr)
    {
        const char* homedir = pw->pw_dir;
        folder              = homedir;
    }

    folder.append(QDir::separator());
    folder.append(".");
    folder.append(folder_name);
#endif

    // Make sure the folder exists. If not, create it.
    const std::string dir = folder.toStdString();
    if (!QDir(dir.c_str()).exists())
    {
        const QDir settings_dir;
        if (const bool path_created = settings_dir.mkpath(dir.c_str()); !path_created)
        {
            // TODO: return error
            //ToolUtil::DbgMsg("[RDP] Failed to create settings file directory at %s", dir.c_str());
        }
    }

    return folder;
}

QString Util::GetDefaultOutputPath(const QString& default_trace_output_folder_name)
{
    return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + default_trace_output_folder_name +
                                    QDir::separator() + kAppNameMacro);
}

QString Util::GetPathForFileWithDifferentExtension(const QString& path, const QString& new_extension)
{
    const QFileInfo file_info(path);
    const QString   new_path = file_info.dir().filePath(QString("%1.%2").arg(file_info.completeBaseName(), new_extension));
    return QDir::toNativeSeparators(new_path);
}

void Util::SetCommonQTreeViewProperties(QTreeView* tree_view)
{
    auto* header = tree_view->header();
    // The header should have been set during the constructor,
    // so confirm that is true.
    Q_ASSERT(header != nullptr);
    header->setDefaultAlignment(Qt::AlignLeft);
    header->setSectionsClickable(false);
    header->setResizeContentsPrecision(32);
    header->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    header->setStretchLastSection(true);

    tree_view->setFrameStyle(QFrame::NoFrame);
    tree_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tree_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tree_view->verticalScrollBar()->blockSignals(false);
    tree_view->horizontalScrollBar()->blockSignals(false);
    tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_view->setWordWrap(false);
    tree_view->setAlternatingRowColors(true);
}

void Util::SetCommonQTableViewProperties(QTableView* table_view)
{
    auto* horizontal_header = table_view->horizontalHeader();
    auto* vertical_header   = table_view->verticalHeader();
    // The headers should have been set during the constructor,
    // so confirm that is true.
    Q_ASSERT(horizontal_header != nullptr);
    Q_ASSERT(vertical_header != nullptr);
    horizontal_header->setDefaultAlignment(Qt::AlignLeft);
    horizontal_header->setSectionsClickable(false);
    horizontal_header->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    horizontal_header->setStretchLastSection(true);

    vertical_header->setSectionsClickable(false);
    vertical_header->setVisible(false);
    vertical_header->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);

    table_view->setFrameStyle(QFrame::NoFrame);
    table_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table_view->verticalScrollBar()->blockSignals(false);
    table_view->horizontalScrollBar()->blockSignals(false);
    table_view->setSelectionMode(QAbstractItemView::SingleSelection);
    table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setShowGrid(false);
    table_view->setWordWrap(false);
    table_view->setAlternatingRowColors(true);
}

QString Util::ExpandOutputPathMacros(const QString& path, const QString& application_name)
{
    if (application_name.isEmpty())
    {
        return path;
    }

    if (path.isEmpty())
    {
        return "";
    }

    const QString application_base_name = QFileInfo(application_name).baseName();

    QString output_path = path;
    output_path         = output_path.replace(kAppNameMacro, application_base_name);
    output_path         = output_path.replace(kApplicationNameMacro, application_base_name);

    return QDir::toNativeSeparators(output_path);
}
