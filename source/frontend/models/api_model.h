// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Api model definition

#ifndef RDP_SOURCE_FRONTEND_MODELS_API_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_API_MODEL_H_

#include <cstdint>

#include <dev_trace_common.h>

#include "system_info_model.h"

#define DECL_API(name) name = static_cast<uint8_t>(devtrace::Api::name)

namespace rdp
{
    class Module;
    class ModuleModel;
    class Workflow;

    /// @brief Maintains list of supported API
    class ApiModel final : public QAbstractItemModel, public std::enable_shared_from_this<ApiModel>
    {
        Q_OBJECT
    public:
        /// @brief API types support for capture.
        ///
        /// These are directly convertable to devtrace::Api except for kWorkflowSupported which does not have a mapping.
        enum class Api : uint8_t
        {
            DECL_API(kUnknown),    ///< Unknown API
            DECL_API(kDirectX12),  ///< DirectX 12
            DECL_API(kDirectX11),  ///< DirectX 11 (and 10) on PAL
            DECL_API(kDirectX9),   ///< DirectX 9 on PAL

            DECL_API(kVulkan),  ///< Vulkan
            DECL_API(kOpenCl),  ///< OpenCL
            DECL_API(kHip),     ///< HIP on PAL
            DECL_API(kOpenGl),  ///< OpenGL
            DECL_API(kCount),   ///< API count

            kWorkflowSupported = static_cast<uint8_t>(devtrace::Api::kCount) + 1  ///< Any API is supported for connection using this type
        };

        /// @brief Constructor.
        /// @param [in] system_info_model Handle to system info model.
        /// @param [in] parent The parent object.
        explicit ApiModel(const std::weak_ptr<SystemInfoModel>& system_info_model, QObject* parent = nullptr);

        /// @brief Destructor
        ~ApiModel() Q_DECL_OVERRIDE;

        /// @brief Load model
        void Load();

        /// @brief Checks if the specified Api is supported for this platform
        /// @param [in] api The Api to check support for
        /// @return true if supported, false otherwise
        bool IsSupported(Api api) const;

        /// @brief Gets the index of API in supported list.
        /// @param [in] api The API
        /// @return index in list or 0 if not supported
        int GetIndexOf(Api api);

        /// @brief Gets the name for specified api
        /// @param [in] api The api
        /// @return The string name for specified Api
        static QString GetName(Api api);

        /// @brief Gets an Api value for specified driver description
        /// @param [in] driver_description The driver description
        /// @return The Api for specified driver description string
        static Api GetApiFromDriverDescription(const QString& driver_description);

    signals:
        /// @brief Signal api model was loaded
        /// @param [in] model The api model
        void Loaded(std::shared_ptr<ApiModel> model);

        /// @brief Emitted when the API count changes.
        /// @param [in] count The new number of available APIs.
        void ApiCountChanged(int count);

    private slots:
        /// @brief Called when the current system information changes for any reason.
        void OnSystemInfoChanged();

    protected:
        /// @brief QAbstractListModel::data() implementation
        /// @param [in] index The index to query data for
        /// @param [in] role The data role
        /// @return variant data for specified role
        QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::headerData() override
        /// @param [in] section The header section to return data for
        /// @param [in] orientation The orientation state of header
        /// @param [in] role The data role
        /// @return data for header
        QVariant headerData(int section, Qt::Orientation orientation, int role) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::flags() override
        /// @param [in] row The row to create index for
        /// @param [in] column The column to create index for
        /// @param [in] parent The parent of index
        /// @return new model index
        QModelIndex index(int row, int column, const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::parent() override
        /// @param [in] index The index to return parent for
        /// @return parent index
        QModelIndex parent(const QModelIndex& index) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::rowCount() override
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::columnCount() override
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::setData() override
        /// @param [in] index The index to set internal data for
        /// @param [in] value The new data value
        /// @param [in] role The role to set new data for
        /// @return True if internal data changed
        bool setData(const QModelIndex& index, const QVariant& value, int role) Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::insertRows() override
        /// @param [in] row The row to start insert from
        /// @param [in] count The number of rows to insert
        /// @param [in] parent The parent index
        /// @return True is rows inserted
        bool insertRows(int row, int count, const QModelIndex& parent) Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::removeRows() override
        /// @param [in] row The row to start removal from
        /// @param [in] count The number of subsequent rows to remove
        /// @param [in] parent The parent index
        /// @return True is rows removed
        bool removeRows(int row, int count, const QModelIndex& parent) Q_DECL_OVERRIDE;

        std::vector<Api>               supported_;          ///< List of supported APIs
        std::weak_ptr<SystemInfoModel> system_info_model_;  ///< Handle to system info model
        std::weak_ptr<ModuleModel>     module_model_;       ///< Handle to module model
        std::weak_ptr<const Workflow>  workflow_;           ///< Workflow to constrain API support
    };
}  // namespace rdp

#endif
