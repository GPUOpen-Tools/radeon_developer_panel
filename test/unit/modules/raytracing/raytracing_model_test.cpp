//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the Raytracing view model.
//=============================================================================

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QSettings>
#include <QSignalSpy>

#include <ddApi.h>

#include <rra_trace_source.h>
#include <source_status.h>

#include <common/inc/definitions.h>
#include <common/inc/util.h>
#include <raytracing/src/gui/raytracing_module_definitions.h>
#include <raytracing/src/gui/raytracing_userdata_view_model.h>
#include <raytracing/src/gui/raytracing_view_model.h>

#include "../mock/mock_file_opener.h"
#include "../mock/mock_file_stream_provider.h"
#include "../mock/mock_file_utils.h"
#include "../mock/mock_settings.h"

// Mocks / Test Data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr const char* kAppName          = "CoolApp";
static constexpr uint16_t    kMockConnectionId = 1234;

static const devtrace::TraceSourceStatus kDisconnectedStatus = []() {
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kDisconnected);
    return status;
}();

static const devtrace::TraceSourceStatus kIdleStatus = []() {
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kIdle);
    status.current_connections[kMockConnectionId] = devtrace::Api::kUnknown;
    return status;
}();

static const devtrace::TraceSourceStatus kCapturingStatus = []() {
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kCapturing);
    status.current_connections[kMockConnectionId] = devtrace::Api::kUnknown;
    return status;
}();

static const devtrace::TraceSourceStatus kDumpingStatus = []() {
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kDumping);
    status.current_connections[kMockConnectionId] = devtrace::Api::kUnknown;
    return status;
}();

static const devtrace::TraceSourceStatus kProcessingStatus = []() {
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kProcessing);
    status.current_connections[kMockConnectionId] = devtrace::Api::kUnknown;
    return status;
}();

static const devtrace::TraceSourceStatus kUnsupportedStatus = []() {
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kDisabled);
    return status;
}();

static devtrace::TraceSourceStatus MakeStatusWithConnection(devtrace::TraceSourceStage stage, devtrace::Api api = devtrace::Api::kUnknown)
{
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(stage);
    status.current_connections[kMockConnectionId] = api;
    return status;
}

/// @brief Mock implementation of RraTraceSource for testing.
class MockRraTraceSource : public devtrace::RraTraceSource
{
public:
    // Configurable<RraTraceSourceConfig>
    MOCK_METHOD(devtrace::RraTraceSourceConfig&, GetConfig, (), (override));

    // RraTraceSource
    MOCK_METHOD(void, RegisterSupportEvent, (const devtrace::RraTraceSourceSupportEvent&), (override));

    // TriggerableTraceSource
    MOCK_METHOD(devtrace::Result, PrepareForDelayedCapture, (DDConnectionId), (override));
    MOCK_METHOD(devtrace::Result, RequestBeginTrace, (DDConnectionId, uint32_t), (override));
    MOCK_METHOD(void, GetSupportedCaptureModes, (DDConnectionId, std::vector<uint32_t>&), (override));

    // TraceSource
    MOCK_METHOD(devtrace::Result, RequestAbortTrace, (DDConnectionId umd_connection_id), (override));
    MOCK_METHOD(devtrace::Result, RequestAbortProcessing, (), (override));
    MOCK_METHOD(void, RegisterStatusEvent, (const devtrace::TraceSourceStatusEvent&), (override));
    MOCK_METHOD(void, RegisterTraceCompletionEvent, (const devtrace::TraceCompletionEvent&), (override));
    MOCK_METHOD(void, RegisterTraceCaptureProgressEvent, (const devtrace::TraceCaptureProgressEvent&), (override));
    MOCK_METHOD(void, QueryStatus, (), (override));

    // ClientConnectionSubscriberV2
    MOCK_METHOD(void, OnDriverConnected, (const DDConnectionInfo&), (override));
    MOCK_METHOD(void, OnDriverDisconnected, (DDConnectionId), (override));
    MOCK_METHOD(void, OnDriverStateChanged, (DDConnectionId, DD_DRIVER_STATE), (override));
    MOCK_METHOD(void, RouterConnectionStatusChanged, (bool), (override));
};

/// @brief Test logger that inherits from MercuryLogger but does nothing.
class TestLogger : public MercuryLogger
{
public:
    TestLogger()
        : MercuryLogger(nullptr)
    {
    }

    std::shared_ptr<devtrace::Logger> WithSource(const std::string& /*source*/) override
    {
        return std::make_shared<TestLogger>();
    }

protected:
    void Verbose(const std::string& /*text*/, uint32_t /*pid*/, DDConnectionId /*umd_connection_id*/) const override
    {
    }
    void Info(const std::string& /*text*/, uint32_t /*pid*/, DDConnectionId /*umd_connection_id*/) const override
    {
    }
    void Warning(const std::string& /*text*/, uint32_t /*pid*/, DDConnectionId /*umd_connection_id*/) const override
    {
    }
    void Error(const std::string& /*text*/, uint32_t /*pid*/, DDConnectionId /*umd_connection_id*/) const override
    {
    }
};

/// @brief Stub implementation of RaytracingUserdataViewModel for testing.
//
/// Since RaytracingUserdataViewModel is final, we create a real instance with
/// null dependencies and set its ray history state via the public handler.
class StubRaytracingUserdataViewModel
{
public:
    static std::shared_ptr<RaytracingUserdataViewModel> Create(bool ray_history_enabled = false)
    {
        auto                                    mapper           = std::make_shared<devtrace::RraUserdataMapper>();
        auto                                    prelaunch_helper = std::shared_ptr<PrelaunchSettingsHelper>(nullptr);
        std::function<void(const std::string&)> apply_fn         = [](const std::string&) {};

        auto view_model = std::make_shared<RaytracingUserdataViewModel>(mapper,
                                                                        nullptr,  // rra_trace_source
                                                                        prelaunch_helper,
                                                                        kRraScenesDefaultParentFolder,
                                                                        apply_fn);

        // Set ray history via the public handler
        view_model->HandleEnableRayHistoryChanged(ray_history_enabled ? Qt::Checked : Qt::Unchecked);

        return view_model;
    }
};

// Test suite / fixture
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class RaytracingViewModelTest : public ::testing::Test
{
protected:
    RaytracingViewModelTest()
    {
        trace_source        = std::make_shared<::testing::NiceMock<MockRraTraceSource>>();
        file_opener         = std::make_shared<::testing::StrictMock<MockTraceFileOpener>>();
        file_utils          = std::make_shared<::testing::StrictMock<MockFileUtils>>();
        stream_provider     = std::make_shared<::testing::NiceMock<MockFileSystemStreamProvider>>();
        userdata_view_model = StubRaytracingUserdataViewModel::Create(false);
        logger              = std::make_shared<TestLogger>();
        settings            = MockSettingsFactory::GetSettings();
        delay_timer         = std::shared_ptr<devtrace::TraceTimer>(new QTraceTimer());

        // Capture all registered event callbacks
        // Note: Both TraceSourceViewModel and RaytracingViewModel register status events,
        // so we need to capture both callbacks.
        ON_CALL(*trace_source, RegisterStatusEvent(::testing::_)).WillByDefault([this](const devtrace::TraceSourceStatusEvent& event) {
            status_events_.push_back(event);
        });

        ON_CALL(*trace_source, RegisterTraceCompletionEvent(::testing::_)).WillByDefault([this](const devtrace::TraceCompletionEvent& event) {
            completion_event_ = event;
        });

        ON_CALL(*trace_source, RegisterTraceCaptureProgressEvent(::testing::_)).WillByDefault([this](const devtrace::TraceCaptureProgressEvent& event) {
            progress_event_ = event;
        });

        ON_CALL(*trace_source, RegisterSupportEvent(::testing::_)).WillByDefault([this](const devtrace::RraTraceSourceSupportEvent& event) {
            support_event_ = event;
        });

        ON_CALL(*trace_source, GetConfig()).WillByDefault(::testing::ReturnRef(mock_config_));
    }

    std::shared_ptr<RaytracingViewModel> MakeModel()
    {
        status_events_.clear();
        return std::make_shared<RaytracingViewModel>(stream_provider,
                                                     trace_source,
                                                     file_utils,
                                                     file_opener,
                                                     settings.get(),
                                                     userdata_view_model,
                                                     delay_timer,
                                                     std::static_pointer_cast<MercuryLogger>(logger));
    }

    /// @brief Simulates a status event from the trace source by invoking all registered callbacks.
    void EmitStatusEvent(const devtrace::TraceSourceStatus& new_status, const devtrace::TraceSourceStatus& old_status = {})
    {
        devtrace::TraceSourceStatusEventArgs args{};
        args.new_status = new_status;
        args.old_status = old_status;

        // Invoke all registered status event callbacks (both base class and derived)
        for (const auto& event : status_events_)
        {
            if (event.callback)
            {
                event.callback(event.listener, args);
            }
        }
    }

    /// @brief Simulates a trace completion event from the trace source.
    void EmitCompletionEvent(devtrace::TraceCompletionStatus status, const std::string& path, DDConnectionId connection_id = kMockConnectionId)
    {
        if (completion_event_.callback)
        {
            devtrace::TraceCompletionEventArgs args{};
            args.result.status            = status;
            args.result.path              = path;
            args.result.umd_connection_id = connection_id;
            completion_event_.callback(completion_event_.listener, args);
        }
    }

    /// @brief Simulates a support event from the trace source.
    void EmitSupportEvent(bool is_ray_history_supported, bool is_marker_capture_supported = false)
    {
        if (support_event_.callback)
        {
            devtrace::RraTraceSourceSupportEventArgs args{};
            args.is_ray_history_supported    = is_ray_history_supported;
            args.is_marker_capture_supported = is_marker_capture_supported;
            support_event_.callback(support_event_.listener, args);
        }
    }

    // Test utilities
    std::shared_ptr<::testing::NiceMock<MockRraTraceSource>>           trace_source;
    std::shared_ptr<::testing::StrictMock<MockTraceFileOpener>>        file_opener;
    std::shared_ptr<::testing::StrictMock<MockFileUtils>>              file_utils;
    std::shared_ptr<::testing::NiceMock<MockFileSystemStreamProvider>> stream_provider;
    std::shared_ptr<RaytracingUserdataViewModel>                       userdata_view_model;
    std::shared_ptr<TestLogger>                                        logger;
    std::unique_ptr<QSettings>                                         settings;
    std::shared_ptr<devtrace::TraceTimer>                              delay_timer;

    devtrace::RraTraceSourceConfig mock_config_;

    // Captured event callbacks - note: status_events_ is a vector since multiple callbacks may be registered
    std::vector<devtrace::TraceSourceStatusEvent> status_events_;
    devtrace::TraceCompletionEvent                completion_event_{};
    devtrace::TraceCaptureProgressEvent           progress_event_{};
    devtrace::RraTraceSourceSupportEvent          support_event_{};
};

// Basic construction tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, ConstructsSuccessfully)
{
    EXPECT_CALL(*trace_source, RegisterStatusEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterTraceCompletionEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterTraceCaptureProgressEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterSupportEvent(::testing::_)).Times(1);

    auto model = MakeModel();
    EXPECT_NE(model, nullptr);
}

// Update tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, UpdateCallsQueryStatus)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    EXPECT_CALL(*trace_source, QueryStatus()).Times(1);

    model->Update();

    // Verify model is still valid after update
    EXPECT_NE(model, nullptr);
}

// Status event tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, EmitsShowCaptureUiOnIdleStatus)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kIdleStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, EmitsShowProgressUiOnCapturingStatus)
{
    auto       model = MakeModel();
    QSignalSpy progress_ui_spy(model.get(), &RaytracingViewModel::ShowProgressUi);
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);

    ASSERT_TRUE(progress_ui_spy.isValid());
    ASSERT_TRUE(capture_ui_spy.isValid());

    EmitStatusEvent(kCapturingStatus);

    EXPECT_EQ(progress_ui_spy.count(), 1);
    // ShowCaptureUi should not be emitted for capturing
    EXPECT_EQ(capture_ui_spy.count(), 0);
}

TEST_F(RaytracingViewModelTest, EmitsShowProgressUiOnDumpingStatus)
{
    auto       model = MakeModel();
    QSignalSpy progress_ui_spy(model.get(), &RaytracingViewModel::ShowProgressUi);
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);

    ASSERT_TRUE(progress_ui_spy.isValid());
    ASSERT_TRUE(capture_ui_spy.isValid());

    EmitStatusEvent(kDumpingStatus);

    EXPECT_EQ(progress_ui_spy.count(), 1);
    // ShowCaptureUi should not be emitted for dumping
    EXPECT_EQ(capture_ui_spy.count(), 0);
}

TEST_F(RaytracingViewModelTest, EmitsShowProgressUiOnProcessingStatus)
{
    auto       model = MakeModel();
    QSignalSpy progress_ui_spy(model.get(), &RaytracingViewModel::ShowProgressUi);
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);

    ASSERT_TRUE(progress_ui_spy.isValid());
    ASSERT_TRUE(capture_ui_spy.isValid());

    EmitStatusEvent(kProcessingStatus);

    EXPECT_EQ(progress_ui_spy.count(), 1);
    // ShowCaptureUi should not be emitted for processing
    EXPECT_EQ(capture_ui_spy.count(), 0);
}

TEST_F(RaytracingViewModelTest, EmitsShowCaptureUiOnDisconnected)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kDisconnectedStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, EmitsShowCaptureUiOnUnsupported)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kUnsupportedStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, TransitionsFromIdleToCapturing)
{
    auto       model = MakeModel();
    QSignalSpy progress_ui_spy(model.get(), &RaytracingViewModel::ShowProgressUi);

    // First emit idle status
    EmitStatusEvent(kIdleStatus);
    EXPECT_EQ(progress_ui_spy.count(), 0);

    // Then emit capturing status
    EmitStatusEvent(kCapturingStatus, kIdleStatus);
    EXPECT_EQ(progress_ui_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, TransitionsFromCapturingToIdle)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);

    // First emit capturing status
    EmitStatusEvent(kCapturingStatus);

    // Clear the spy from initial capture ui call
    int initial_count = capture_ui_spy.count();

    // Then emit idle status
    EmitStatusEvent(kIdleStatus, kCapturingStatus);
    EXPECT_GT(capture_ui_spy.count(), initial_count);
}

// Support event tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, EmitsTraceSupportChangedOnSupportEvent)
{
    auto model = MakeModel();

    qRegisterMetaType<devtrace::RraTraceSourceSupportEventArgs>("devtrace::RraTraceSourceSupportEventArgs");
    QSignalSpy support_spy(model.get(), &RaytracingViewModel::TraceSupportChanged);

    EmitSupportEvent(true);

    EXPECT_EQ(support_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, EmitsTraceSupportChangedWithFalse)
{
    auto model = MakeModel();

    qRegisterMetaType<devtrace::RraTraceSourceSupportEventArgs>("devtrace::RraTraceSourceSupportEventArgs");
    QSignalSpy support_spy(model.get(), &RaytracingViewModel::TraceSupportChanged);

    EmitSupportEvent(false);

    EXPECT_EQ(support_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, EmitsTraceSupportChangedWithMarkerCaptureSupported)
{
    auto model = MakeModel();

    qRegisterMetaType<devtrace::RraTraceSourceSupportEventArgs>("devtrace::RraTraceSourceSupportEventArgs");
    QSignalSpy support_spy(model.get(), &RaytracingViewModel::TraceSupportChanged);

    EmitSupportEvent(true, true);

    ASSERT_EQ(support_spy.count(), 1);
    auto args = support_spy.takeFirst().at(0).value<devtrace::RraTraceSourceSupportEventArgs>();
    EXPECT_TRUE(args.is_marker_capture_supported);
}

TEST_F(RaytracingViewModelTest, EmitsTraceSupportChangedWithMarkerCaptureUnsupported)
{
    auto model = MakeModel();

    qRegisterMetaType<devtrace::RraTraceSourceSupportEventArgs>("devtrace::RraTraceSourceSupportEventArgs");
    QSignalSpy support_spy(model.get(), &RaytracingViewModel::TraceSupportChanged);

    EmitSupportEvent(true, false);

    ASSERT_EQ(support_spy.count(), 1);
    auto args = support_spy.takeFirst().at(0).value<devtrace::RraTraceSourceSupportEventArgs>();
    EXPECT_FALSE(args.is_marker_capture_supported);
}

// RequestBeginTrace tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, RequestBeginTraceCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    // Set up a connection first by simulating a status update with connections
    EmitStatusEvent(kIdleStatus);

    ASSERT_GE(ui_status_spy.count(), 1);

    EXPECT_CALL(*trace_source, RequestBeginTrace(kMockConnectionId, ::testing::_)).Times(1).WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->RequestBeginTrace();
}

TEST_F(RaytracingViewModelTest, RequestBeginTraceDoesNothingWithNoConnection)
{
    auto model = MakeModel();

    QSignalSpy trace_failed_spy(model.get(), &RaytracingViewModel::TraceFailed);
    QSignalSpy trace_complete_spy(model.get(), &RaytracingViewModel::TraceComplete);

    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_complete_spy.isValid());

    // Don't set up any connections - capture_target_ remains 0
    EXPECT_CALL(*trace_source, RequestBeginTrace(::testing::_, ::testing::_)).Times(0);

    model->RequestBeginTrace();

    // Verify no signals were emitted since no action was taken
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(0, trace_complete_spy.count());
}

TEST_F(RaytracingViewModelTest, RequestBeginTraceGracefullyFails)
{
    auto model = MakeModel();

    // Set up a connection
    EmitStatusEvent(kIdleStatus);

    QSignalSpy trace_failed_spy(model.get(), &RaytracingViewModel::TraceFailed);

    EXPECT_CALL(*trace_source, RequestBeginTrace(kMockConnectionId, ::testing::_)).Times(1).WillOnce(::testing::Return(devtrace::Result::kFailure));

    model->RequestBeginTrace();

    // Verify no crash occurred and no unexpected signals were emitted
    EXPECT_EQ(0, trace_failed_spy.count());
}

// RequestAbort tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, RequestAbortCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy trace_aborted_spy(model.get(), &RaytracingViewModel::TraceAborted);

    ASSERT_TRUE(trace_aborted_spy.isValid());

    EXPECT_CALL(*trace_source, RequestAbortTrace(0)).Times(1).WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->RequestAbort();

    // Abort request was made but completion event hasn't been emitted yet
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(RaytracingViewModelTest, RequestAbortGracefullyFails)
{
    auto model = MakeModel();

    QSignalSpy trace_aborted_spy(model.get(), &RaytracingViewModel::TraceAborted);

    EXPECT_CALL(*trace_source, RequestAbortTrace()).Times(1).WillOnce(::testing::Return(devtrace::Result::kFailure));

    model->RequestAbort();

    // Verify no crash occurred and no unexpected signals were emitted
    EXPECT_EQ(0, trace_aborted_spy.count());
}

// Open file tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, OpenFileWithMissingExecutableEmitsSignal)
{
    settings->setValue("rra_path", "missing_exe_path");

    auto       model = MakeModel();
    QSignalSpy exe_missing_spy(model.get(), &RaytracingViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("missing_exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFile("trace_file");

    ASSERT_EQ(1, exe_missing_spy.count());
    EXPECT_EQ("missing_exe_path", exe_missing_spy.first()[0].toString());
}

TEST_F(RaytracingViewModelTest, OpenFileSucceeds)
{
    settings->setValue("rra_path", "exe_path");

    auto model = MakeModel();

    QSignalSpy exe_missing_spy(model.get(), &RaytracingViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kSuccess));

    model->OnOpenFile("trace_file");

    // Verify no error signals were emitted on success
    EXPECT_EQ(0, exe_missing_spy.count());
}

TEST_F(RaytracingViewModelTest, OpenFileWithEmptyPathUsesEmptyExePath)
{
    // Don't set rra_path in settings

    auto       model = MakeModel();
    QSignalSpy exe_missing_spy(model.get(), &RaytracingViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString(""))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFile("trace_file");

    ASSERT_EQ(1, exe_missing_spy.count());
}

TEST_F(RaytracingViewModelTest, OpenFileInvalidExePath)
{
    settings->setValue("rra_path", "exe_path");

    auto       model = MakeModel();
    QSignalSpy exe_missing_spy(model.get(), &RaytracingViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFile("trace_file");

    ASSERT_EQ(1, exe_missing_spy.count());
    EXPECT_EQ("exe_path", exe_missing_spy.first()[0].toString());
}

TEST_F(RaytracingViewModelTest, OpenFileFailedToLaunch)
{
    settings->setValue("rra_path", "exe_path");

    auto model = MakeModel();

    QSignalSpy exe_missing_spy(model.get(), &RaytracingViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kFailedToLaunch));

    model->OnOpenFile("trace_file");

    // FailedToLaunch is different from MissingExecutable - no signal should be emitted
    EXPECT_EQ(0, exe_missing_spy.count());
}

// Trace completion tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, EmitsTraceCompleteOnSuccess)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &RaytracingViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &RaytracingViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &RaytracingViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kCompleted, "trace_file.rra");

    EXPECT_EQ(1, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(RaytracingViewModelTest, EmitsTraceFailedOnError)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &RaytracingViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &RaytracingViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &RaytracingViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EXPECT_CALL(*file_utils, RemoveFile(::testing::Eq(QString("trace_file.rra")))).Times(1);

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kError, "trace_file.rra");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(1, trace_failed_spy.count());
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(RaytracingViewModelTest, EmitsTraceAbortedOnAbort)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &RaytracingViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &RaytracingViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &RaytracingViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kAborted, "trace_file.rra");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(1, trace_aborted_spy.count());
}

TEST_F(RaytracingViewModelTest, RemovesFileOnFailure)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &RaytracingViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &RaytracingViewModel::TraceFailed);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());

    EXPECT_CALL(*file_utils, RemoveFile(::testing::Eq(QString("trace_file.rra")))).Times(1);

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kError, "trace_file.rra");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(1, trace_failed_spy.count());
}

TEST_F(RaytracingViewModelTest, AutoOpensTraceWhenEnabled)
{
    settings->setValue("auto_open_traces", true);
    settings->setValue("rra_path", "exe_path");

    auto model = MakeModel();

    QSignalSpy trace_complete_spy(model.get(), &RaytracingViewModel::TraceComplete);

    ASSERT_TRUE(trace_complete_spy.isValid());

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file.rra")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kSuccess));

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kCompleted, "trace_file.rra");

    EXPECT_EQ(1, trace_complete_spy.count());
}

// UI enablement tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, ShouldEnableUiForIdleStage)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    EmitStatusEvent(kIdleStatus);

    ASSERT_GE(ui_status_spy.count(), 1);
    EXPECT_TRUE(ui_status_spy.last()[0].toBool());
}

TEST_F(RaytracingViewModelTest, ShouldDisableUiForCapturingStage)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    EmitStatusEvent(kCapturingStatus);

    ASSERT_GE(ui_status_spy.count(), 1);
    EXPECT_FALSE(ui_status_spy.last()[0].toBool());
}

TEST_F(RaytracingViewModelTest, ShouldDisableUiForDisconnectedStage)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    EmitStatusEvent(kDisconnectedStatus);

    ASSERT_GE(ui_status_spy.count(), 1);
    EXPECT_FALSE(ui_status_spy.last()[0].toBool());
}

TEST_F(RaytracingViewModelTest, ShouldDisableUiForDumpingStage)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    EmitStatusEvent(kDumpingStatus);

    ASSERT_GE(ui_status_spy.count(), 1);
    EXPECT_FALSE(ui_status_spy.last()[0].toBool());
}

TEST_F(RaytracingViewModelTest, ShouldDisableUiForProcessingStage)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    EmitStatusEvent(kProcessingStatus);

    ASSERT_GE(ui_status_spy.count(), 1);
    EXPECT_FALSE(ui_status_spy.last()[0].toBool());
}

// API-specific status tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(RaytracingViewModelTest, HandlesStatusWithDx12Api)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kIdle, devtrace::Api::kDirectX12);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(RaytracingViewModelTest, HandlesStatusWithVulkanApi)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &RaytracingViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &RaytracingViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kIdle, devtrace::Api::kVulkan);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}
