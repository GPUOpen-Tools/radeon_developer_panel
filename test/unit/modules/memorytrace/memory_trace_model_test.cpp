//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the Memory Trace view model.
//=============================================================================

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QSettings>
#include <QSignalSpy>

#include <ddApi.h>

#include <rmv_trace_source.h>
#include <source_status.h>

#include <common/inc/util.h>
#include <memorytrace/src/gui/memory_trace_module_definitions.h>
#include <memorytrace/src/gui/memory_trace_view_model.h>

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

static devtrace::TraceSourceStatus MakeStatusWithAbortSupport(devtrace::TraceSourceStage stage)
{
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(stage);
    status.current_connections[kMockConnectionId] = devtrace::Api::kUnknown;
    status.abort_trace_supported                  = true;
    return status;
}

/// @brief Mock implementation of RmvTraceSource for testing.
class MockRmvTraceSource : public devtrace::RmvTraceSource
{
public:
    // Configurable<RmvTraceSourceConfig>
    MOCK_METHOD(devtrace::RmvTraceSourceConfig&, GetConfig, (), (override));

    // ContinuousTraceSource
    MOCK_METHOD(devtrace::Result, RequestDump, (DDConnectionId connection_id), (override));
    MOCK_METHOD(devtrace::Result, AddMarker, (DDConnectionId connection_id, const std::string& marker), (override));

    // TraceSource
    MOCK_METHOD(devtrace::Result, RequestAbortTrace, (DDConnectionId umd_connection_id), (override));
    MOCK_METHOD(devtrace::Result, RequestAbortProcessing, (), (override));
    MOCK_METHOD(void, RegisterStatusEvent, (const devtrace::TraceSourceStatusEvent& event), (override));
    MOCK_METHOD(void, RegisterTraceCompletionEvent, (const devtrace::TraceCompletionEvent& event), (override));
    MOCK_METHOD(void, RegisterTraceCaptureProgressEvent, (const devtrace::TraceCaptureProgressEvent& event), (override));
    MOCK_METHOD(void, QueryStatus, (), (override));

    // ClientConnectionSubscriberV2
    MOCK_METHOD(void, OnDriverConnected, (const DDConnectionInfo& connection_info), (override));
    MOCK_METHOD(void, OnDriverDisconnected, (DDConnectionId umd_connection_id), (override));
    MOCK_METHOD(void, OnDriverStateChanged, (DDConnectionId umd_connection_id, DD_DRIVER_STATE state), (override));
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

// Test suite / fixture
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MemoryTraceViewModelTest : public ::testing::Test
{
protected:
    MemoryTraceViewModelTest()
    {
        trace_source    = std::make_shared<::testing::NiceMock<MockRmvTraceSource>>();
        file_opener     = std::make_shared<::testing::StrictMock<MockTraceFileOpener>>();
        file_utils      = std::make_shared<::testing::StrictMock<MockFileUtils>>();
        stream_provider = std::make_shared<::testing::NiceMock<MockFileSystemStreamProvider>>();
        logger          = std::make_shared<TestLogger>();
        settings        = MockSettingsFactory::GetSettings();

        // Capture all registered event callbacks
        // Note: Both TraceSourceViewModel and MemoryTraceViewModel register status events,
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
    }

    std::shared_ptr<MemoryTraceViewModel> MakeModel()
    {
        status_events_.clear();
        return std::make_shared<MemoryTraceViewModel>(stream_provider, trace_source, file_utils, file_opener, settings.get(), logger);
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

    // Test utilities
    std::shared_ptr<::testing::NiceMock<MockRmvTraceSource>>           trace_source;
    std::shared_ptr<::testing::StrictMock<MockTraceFileOpener>>        file_opener;
    std::shared_ptr<::testing::StrictMock<MockFileUtils>>              file_utils;
    std::shared_ptr<::testing::NiceMock<MockFileSystemStreamProvider>> stream_provider;
    std::shared_ptr<TestLogger>                                        logger;
    std::unique_ptr<QSettings>                                         settings;

    // Captured event callbacks - note: status_events_ is a vector since multiple callbacks may be registered
    std::vector<devtrace::TraceSourceStatusEvent> status_events_;
    devtrace::TraceCompletionEvent                completion_event_{};
    devtrace::TraceCaptureProgressEvent           progress_event_{};
};

// Basic construction tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, ConstructsSuccessfully)
{
    EXPECT_CALL(*trace_source, RegisterStatusEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterTraceCompletionEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterTraceCaptureProgressEvent(::testing::_)).Times(::testing::AtLeast(1));

    auto model = MakeModel();
    EXPECT_NE(model, nullptr);
}

// Update tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, UpdateCallsQueryStatus)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    EXPECT_CALL(*trace_source, QueryStatus()).Times(1);

    model->Update();

    // Verify model is still valid after update
    EXPECT_NE(model, nullptr);
}

// Status event tests (equivalent to OnBind and status callback tests)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, EmitsShowCaptureUiOnCapturingStatus)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kCapturingStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, EmitsShowProgressUiOnDumpingStatus)
{
    auto       model = MakeModel();
    QSignalSpy progress_ui_spy(model.get(), &MemoryTraceViewModel::ShowProgressUi);
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);

    ASSERT_TRUE(progress_ui_spy.isValid());
    ASSERT_TRUE(capture_ui_spy.isValid());

    EmitStatusEvent(kDumpingStatus);

    EXPECT_EQ(progress_ui_spy.count(), 1);
    // ShowCaptureUi should not be emitted for dumping
    EXPECT_EQ(capture_ui_spy.count(), 0);
}

TEST_F(MemoryTraceViewModelTest, EmitsShowCaptureUiOnDisconnected)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kDisconnectedStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, EmitsShowCaptureUiOnUnsupported)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kUnsupportedStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, EmitsShowCaptureUiOnProcessing)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    EmitStatusEvent(kProcessingStatus);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, TransitionsFromCapturingToDumping)
{
    auto       model = MakeModel();
    QSignalSpy progress_ui_spy(model.get(), &MemoryTraceViewModel::ShowProgressUi);

    // First emit capturing status
    EmitStatusEvent(kCapturingStatus);
    EXPECT_EQ(progress_ui_spy.count(), 0);

    // Then emit dumping status
    EmitStatusEvent(kDumpingStatus, kCapturingStatus);
    EXPECT_EQ(progress_ui_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, TransitionsFromDumpingToCapturing)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);

    // First emit dumping status
    EmitStatusEvent(kDumpingStatus);

    // Clear the spy from initial capture ui call
    int initial_count = capture_ui_spy.count();

    // Then emit capturing status
    EmitStatusEvent(kCapturingStatus, kDumpingStatus);
    EXPECT_GT(capture_ui_spy.count(), initial_count);
}

// RequestDump tests (equivalent to Dump tests)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, RequestDumpCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    // Set up a connection so GetCaptureTarget returns a valid ID
    EmitStatusEvent(kCapturingStatus);

    ASSERT_GE(ui_status_spy.count(), 1);

    EXPECT_CALL(*trace_source, RequestDump(kMockConnectionId)).Times(1).WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->RequestDump();
}

TEST_F(MemoryTraceViewModelTest, RequestDumpDoesNothingWithNoConnection)
{
    auto model = MakeModel();

    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);
    QSignalSpy trace_complete_spy(model.get(), &MemoryTraceViewModel::TraceComplete);

    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_complete_spy.isValid());

    // Don't set up any connections - capture_target_ remains 0
    EXPECT_CALL(*trace_source, RequestDump(::testing::_)).Times(0);

    model->RequestDump();

    // Verify no signals were emitted since no action was taken
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(0, trace_complete_spy.count());
}

TEST_F(MemoryTraceViewModelTest, RequestDumpGracefullyFails)
{
    auto model = MakeModel();

    // Set up a connection
    EmitStatusEvent(kCapturingStatus);

    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);

    EXPECT_CALL(*trace_source, RequestDump(kMockConnectionId)).Times(1).WillOnce(::testing::Return(devtrace::Result::kFailure));

    model->RequestDump();

    // Verify no crash occurred and no unexpected signals were emitted
    EXPECT_EQ(0, trace_failed_spy.count());
}

// RequestMarker tests (equivalent to AddMarker tests)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, RequestMarkerCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    // Set up a connection
    EmitStatusEvent(kCapturingStatus);

    ASSERT_GE(ui_status_spy.count(), 1);

    EXPECT_CALL(*trace_source, AddMarker(kMockConnectionId, std::string("test_marker"))).Times(1).WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->RequestMarker("test_marker");
}

TEST_F(MemoryTraceViewModelTest, RequestMarkerDoesNothingWithNoConnection)
{
    auto model = MakeModel();

    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);

    ASSERT_TRUE(trace_failed_spy.isValid());

    EXPECT_CALL(*trace_source, AddMarker(::testing::_, ::testing::_)).Times(0);

    model->RequestMarker("test_marker");

    // Verify no signals were emitted since no action was taken
    EXPECT_EQ(0, trace_failed_spy.count());
}

// Open file tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, OpenFileWithMissingExecutableEmitsSignal)
{
    settings->setValue("rmv_path", "missing_exe_path");

    auto       model = MakeModel();
    QSignalSpy exe_missing_spy(model.get(), &MemoryTraceViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("missing_exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFile("trace_file");

    ASSERT_EQ(1, exe_missing_spy.count());
    EXPECT_EQ("missing_exe_path", exe_missing_spy.first()[0].toString());
}

TEST_F(MemoryTraceViewModelTest, OpenFileSucceeds)
{
    settings->setValue("rmv_path", "exe_path");

    auto model = MakeModel();

    QSignalSpy exe_missing_spy(model.get(), &MemoryTraceViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kSuccess));

    model->OnOpenFile("trace_file");

    // Verify no error signals were emitted on success
    EXPECT_EQ(0, exe_missing_spy.count());
}

TEST_F(MemoryTraceViewModelTest, OpenFileWithEmptyPathUsesEmptyExePath)
{
    // Don't set rmv_path in settings

    auto       model = MakeModel();
    QSignalSpy exe_missing_spy(model.get(), &MemoryTraceViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString(""))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFile("trace_file");

    ASSERT_EQ(1, exe_missing_spy.count());
}

TEST_F(MemoryTraceViewModelTest, OpenFileInvalidExePath)
{
    settings->setValue("rmv_path", "exe_path");

    auto       model = MakeModel();
    QSignalSpy exe_missing_spy(model.get(), &MemoryTraceViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFile("trace_file");

    ASSERT_EQ(1, exe_missing_spy.count());
    EXPECT_EQ("exe_path", exe_missing_spy.first()[0].toString());
}

TEST_F(MemoryTraceViewModelTest, OpenFileFailedToLaunch)
{
    settings->setValue("rmv_path", "exe_path");

    auto model = MakeModel();

    QSignalSpy exe_missing_spy(model.get(), &MemoryTraceViewModel::ExecutableMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kFailedToLaunch));

    model->OnOpenFile("trace_file");

    // FailedToLaunch is different from MissingExecutable - no signal should be emitted
    EXPECT_EQ(0, exe_missing_spy.count());
}

// Trace completion tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, EmitsTraceCompleteOnSuccess)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &MemoryTraceViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &MemoryTraceViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kCompleted, "trace_file");

    EXPECT_EQ(1, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(MemoryTraceViewModelTest, EmitsTraceFailedOnError)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &MemoryTraceViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &MemoryTraceViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EXPECT_CALL(*file_utils, RemoveFile(::testing::Eq(QString("trace_file")))).Times(1);

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kError, "trace_file");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(1, trace_failed_spy.count());
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(MemoryTraceViewModelTest, EmitsTraceAbortedOnAbort)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &MemoryTraceViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &MemoryTraceViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kAborted, "trace_file");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(1, trace_aborted_spy.count());
}

TEST_F(MemoryTraceViewModelTest, RemovesFileOnFailure)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &MemoryTraceViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &MemoryTraceViewModel::TraceFailed);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());

    EXPECT_CALL(*file_utils, RemoveFile(::testing::Eq(QString("trace_file")))).Times(1);

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kError, "trace_file");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(1, trace_failed_spy.count());
}

TEST_F(MemoryTraceViewModelTest, AutoOpensTraceWhenEnabled)
{
    settings->setValue("auto_open_traces", true);
    settings->setValue("rmv_path", "exe_path");

    auto model = MakeModel();

    QSignalSpy trace_complete_spy(model.get(), &MemoryTraceViewModel::TraceComplete);

    ASSERT_TRUE(trace_complete_spy.isValid());

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file")), ::testing::Eq(QString("exe_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kSuccess));

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kCompleted, "trace_file");

    EXPECT_EQ(1, trace_complete_spy.count());
}

// RequestAbort tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, RequestAbortCallsTraceSourceWhenSupported)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);
    QSignalSpy trace_aborted_spy(model.get(), &MemoryTraceViewModel::TraceAborted);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    // Set up status with abort_trace_supported = true
    auto status = MakeStatusWithAbortSupport(devtrace::TraceSourceStage::kCapturing);
    EmitStatusEvent(status);

    ASSERT_GE(ui_status_spy.count(), 1);

    EXPECT_CALL(*trace_source, RequestAbortTrace()).Times(1).WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->RequestAbort();

    // Abort request was made but completion event hasn't been emitted yet
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(MemoryTraceViewModelTest, RequestAbortDoesNothingWhenNotSupported)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);
    QSignalSpy trace_aborted_spy(model.get(), &MemoryTraceViewModel::TraceAborted);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    // Status without abort support (default)
    EmitStatusEvent(kCapturingStatus);

    ASSERT_GE(ui_status_spy.count(), 1);

    // Should not call RequestAbortTrace since abort_trace_supported is false
    EXPECT_CALL(*trace_source, RequestAbortTrace()).Times(0);

    model->RequestAbort();

    // Verify no abort signal was emitted
    EXPECT_EQ(0, trace_aborted_spy.count());
}

// API-specific status tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(MemoryTraceViewModelTest, HandlesStatusWithDx12Api)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kDirectX12);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, HandlesStatusWithVulkanApi)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kVulkan);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, HandlesStatusWithOpenClApi)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kOpenCl);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, HandlesStatusWithHipApi)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kHip);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}

TEST_F(MemoryTraceViewModelTest, HandlesStatusWithOpenGlApi)
{
    auto       model = MakeModel();
    QSignalSpy capture_ui_spy(model.get(), &MemoryTraceViewModel::ShowCaptureUi);
    QSignalSpy ui_status_spy(model.get(), &MemoryTraceViewModel::UiStatusChanged);

    ASSERT_TRUE(capture_ui_spy.isValid());
    ASSERT_TRUE(ui_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kOpenGl);
    EmitStatusEvent(status);

    EXPECT_GE(capture_ui_spy.count(), 1);
    EXPECT_GE(ui_status_spy.count(), 1);
}
