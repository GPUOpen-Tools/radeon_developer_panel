//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the Crash Analysis view model.
//=============================================================================

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QSettings>
#include <QSignalSpy>

#include <ddApi.h>

#include <rgd_trace_source.h>
#include <source_status.h>

#include <common/inc/definitions.h>
#include <common/inc/util.h>

#include <crashanalysis/src/gui/crash_analysis_module_definitions.h>
#include <crashanalysis/src/gui/crash_analysis_prelaunch_settings_helper.h>
#include <crashanalysis/src/gui/crash_analysis_userdata_view_model.h>
#include <crashanalysis/src/gui/crash_analysis_view_model.h>

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

static devtrace::TraceSourceStatus MakeStatusWithAbortSupported()
{
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(devtrace::TraceSourceStage::kCapturing);
    status.current_connections[kMockConnectionId] = devtrace::Api::kUnknown;
    status.abort_trace_supported                  = true;
    return status;
}

static devtrace::TraceSourceStatus MakeStatusWithConnection(devtrace::TraceSourceStage stage, devtrace::Api api = devtrace::Api::kUnknown)
{
    devtrace::TraceSourceStatus status{};
    status.ChangeStage(stage);
    status.current_connections[kMockConnectionId] = api;
    return status;
}

/// @brief Mock implementation of RgdTraceSource for testing.
class MockRgdTraceSource : public devtrace::RgdTraceSource
{
public:
    // Configurable<RgdTraceSourceConfig>
    MOCK_METHOD(devtrace::RgdTraceSourceConfig&, GetConfig, (), (override));

    // ContinuousTraceSource
    MOCK_METHOD(devtrace::Result, RequestDump, (DDConnectionId connection_id), (override));
    MOCK_METHOD(devtrace::Result, AddMarker, (DDConnectionId connection_id, const std::string& marker), (override));

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

    // RgdTraceSource
    MOCK_METHOD(devtrace::Result, QueueSummaryGeneration, (const std::string&, bool, bool), (override));
    MOCK_METHOD(void, RegisterSupportEvent, (const devtrace::RgdTraceSourceSupportEvent&), (override));
    MOCK_METHOD(void, RegisterNoCrashDetectedEvent, (const devtrace::RgdNoCrashDetectedEvent&), (override));
    MOCK_METHOD(void, RegisterSummaryEvent, (const devtrace::RgpSummaryEvent&), (override));
    MOCK_METHOD(bool, IsCurrentHardwareApu, (), (const, override));
    MOCK_METHOD(bool, IsHardwareCrashAnalysisSupported, (), (const, override));
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

/// @brief Stub implementation of CrashAnalysisUserdataViewModel for testing.
class StubCrashAnalysisUserdataViewModel
{
public:
    static std::shared_ptr<CrashAnalysisUserdataViewModel> Create()
    {
        auto mapper           = std::make_shared<devtrace::RgdUserdataMapper>();
        auto prelaunch_helper = std::shared_ptr<CrashAnalysisPrelaunchSettingsHelper>(nullptr);
        auto apply_fn         = [](const std::string&) {};

        return std::make_shared<CrashAnalysisUserdataViewModel>(mapper,
                                                                nullptr,  // rgd_trace_source
                                                                prelaunch_helper,
                                                                kRgdTracesDefaultParentFolder,
                                                                apply_fn);
    }
};

// Test suite / fixture
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CrashAnalysisViewModelTest : public ::testing::Test
{
protected:
    CrashAnalysisViewModelTest()
    {
        trace_source        = std::make_shared<::testing::NiceMock<MockRgdTraceSource>>();
        file_opener         = std::make_shared<::testing::NiceMock<MockTraceFileOpener>>();
        file_utils          = std::make_shared<::testing::NiceMock<MockFileUtils>>();
        stream_provider     = std::make_shared<::testing::NiceMock<MockFileSystemStreamProvider>>();
        userdata_view_model = StubCrashAnalysisUserdataViewModel::Create();
        logger              = std::make_shared<TestLogger>();
        settings            = MockSettingsFactory::GetSettings();

        // Capture all registered event callbacks
        ON_CALL(*trace_source, RegisterStatusEvent(::testing::_)).WillByDefault([this](const devtrace::TraceSourceStatusEvent& event) {
            status_events_.push_back(event);
        });

        ON_CALL(*trace_source, RegisterTraceCompletionEvent(::testing::_)).WillByDefault([this](const devtrace::TraceCompletionEvent& event) {
            completion_event_ = event;
        });

        ON_CALL(*trace_source, RegisterTraceCaptureProgressEvent(::testing::_)).WillByDefault([this](const devtrace::TraceCaptureProgressEvent& event) {
            progress_event_ = event;
        });

        ON_CALL(*trace_source, RegisterSupportEvent(::testing::_)).WillByDefault([this](const devtrace::RgdTraceSourceSupportEvent& event) {
            support_event_ = event;
        });

        ON_CALL(*trace_source, RegisterNoCrashDetectedEvent(::testing::_)).WillByDefault([this](const devtrace::RgdNoCrashDetectedEvent& event) {
            no_crash_event_ = event;
        });

        ON_CALL(*trace_source, RegisterSummaryEvent(::testing::_)).WillByDefault([this](const devtrace::RgpSummaryEvent& event) { summary_event_ = event; });

        ON_CALL(*trace_source, GetConfig()).WillByDefault(::testing::ReturnRef(mock_config_));
    }

    std::shared_ptr<CrashAnalysisViewModel> MakeModel()
    {
        status_events_.clear();
        return std::make_shared<CrashAnalysisViewModel>(stream_provider, trace_source, file_utils, file_opener, userdata_view_model, settings.get(), logger);
    }

    /// @brief Simulates a status event from the trace source by invoking all registered callbacks.
    void EmitStatusEvent(const devtrace::TraceSourceStatus& new_status, const devtrace::TraceSourceStatus& old_status = {})
    {
        devtrace::TraceSourceStatusEventArgs args{};
        args.new_status = new_status;
        args.old_status = old_status;

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

    /// @brief Simulates a summary event from the trace source.
    void EmitSummaryEvent(devtrace::Result result, const std::string& path, bool automatically_queued, const std::string& error = "")
    {
        if (summary_event_.callback)
        {
            devtrace::RgdSummaryEventArgs args{};
            args.result.result               = result;
            args.result.path                 = path;
            args.result.automatically_queued = automatically_queued;
            args.result.error                = error;
            summary_event_.callback(summary_event_.listener, args);
        }
    }

    /// @brief Simulates a no-crash-detected event from the trace source.
    void EmitNoCrashDetectedEvent()
    {
        if (no_crash_event_.callback)
        {
            no_crash_event_.callback(no_crash_event_.listener);
        }
    }

    /// @brief Simulates a support event from the trace source.
    void EmitSupportEvent(bool is_hardware_crash_analysis_supported, bool is_hardware_apu = false, bool is_gpr_capture_supported = false)
    {
        if (support_event_.callback)
        {
            devtrace::RgdTraceSourceSupportEventArgs args{};
            args.is_hardware_crash_analysis_supported = is_hardware_crash_analysis_supported;
            args.is_hardware_apu                      = is_hardware_apu;
            args.is_gpr_capture_supported             = is_gpr_capture_supported;
            support_event_.callback(support_event_.listener, args);
        }
    }

    // Test utilities
    std::shared_ptr<::testing::NiceMock<MockRgdTraceSource>>           trace_source;
    std::shared_ptr<::testing::NiceMock<MockTraceFileOpener>>          file_opener;
    std::shared_ptr<::testing::NiceMock<MockFileUtils>>                file_utils;
    std::shared_ptr<::testing::NiceMock<MockFileSystemStreamProvider>> stream_provider;
    std::shared_ptr<CrashAnalysisUserdataViewModel>                    userdata_view_model;
    std::shared_ptr<TestLogger>                                        logger;
    std::unique_ptr<QSettings>                                         settings;

    devtrace::RgdTraceSourceConfig mock_config_;

    // Captured event callbacks
    std::vector<devtrace::TraceSourceStatusEvent> status_events_;
    devtrace::TraceCompletionEvent                completion_event_{};
    devtrace::TraceCaptureProgressEvent           progress_event_{};
    devtrace::RgdTraceSourceSupportEvent          support_event_{};
    devtrace::RgdNoCrashDetectedEvent             no_crash_event_{};
    devtrace::RgpSummaryEvent                     summary_event_{};
};

// Basic construction tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, ConstructsSuccessfully)
{
    EXPECT_CALL(*trace_source, RegisterStatusEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterTraceCompletionEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterTraceCaptureProgressEvent(::testing::_)).Times(::testing::AtLeast(1));
    EXPECT_CALL(*trace_source, RegisterSupportEvent(::testing::_)).Times(1);
    EXPECT_CALL(*trace_source, RegisterNoCrashDetectedEvent(::testing::_)).Times(1);
    EXPECT_CALL(*trace_source, RegisterSummaryEvent(::testing::_)).Times(1);

    auto model = MakeModel();
    EXPECT_NE(model, nullptr);
}

// Update tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, UpdateCallsQueryStatus)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);

    EXPECT_CALL(*trace_source, QueryStatus()).Times(1);

    model->Update();

    // Verify model is still valid after update
    EXPECT_NE(model, nullptr);
}

// Status event tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, EmitsUiStatusChangedOnCapturingStatus)
{
    auto       model = MakeModel();
    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);
    QSignalSpy client_status_spy(model.get(), &CrashAnalysisViewModel::ClientStatusChanged);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(client_status_spy.isValid());

    EmitStatusEvent(kCapturingStatus);

    EXPECT_GE(ui_status_spy.count(), 1);
    EXPECT_GE(client_status_spy.count(), 1);
}

TEST_F(CrashAnalysisViewModelTest, EmitsUiStatusChangedOnDisconnected)
{
    auto       model = MakeModel();
    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);
    QSignalSpy client_status_spy(model.get(), &CrashAnalysisViewModel::ClientStatusChanged);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(client_status_spy.isValid());

    EmitStatusEvent(kDisconnectedStatus);

    EXPECT_GE(ui_status_spy.count(), 1);
    EXPECT_GE(client_status_spy.count(), 1);
    // Disconnected status should disable the UI
    EXPECT_FALSE(ui_status_spy.last()[0].toBool());
}

TEST_F(CrashAnalysisViewModelTest, EmitsUiStatusChangedOnUnsupported)
{
    auto       model = MakeModel();
    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);
    QSignalSpy client_status_spy(model.get(), &CrashAnalysisViewModel::ClientStatusChanged);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(client_status_spy.isValid());

    EmitStatusEvent(kUnsupportedStatus);

    EXPECT_GE(ui_status_spy.count(), 1);
    EXPECT_GE(client_status_spy.count(), 1);
    // Unsupported status should disable the UI
    EXPECT_FALSE(ui_status_spy.last()[0].toBool());
}

// RequestAbort tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, RequestAbortCallsTraceSourceWhenAbortSupported)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);

    // First emit a status with abort_trace_supported = true
    EmitStatusEvent(MakeStatusWithAbortSupported());

    ASSERT_GE(ui_status_spy.count(), 1);

    EXPECT_CALL(*trace_source, RequestAbortTrace()).Times(1).WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->RequestAbort();
}

TEST_F(CrashAnalysisViewModelTest, RequestAbortDoesNothingWhenAbortNotSupported)
{
    auto model = MakeModel();

    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);

    // Default status does not have abort supported
    EmitStatusEvent(kDisconnectedStatus);

    ASSERT_GE(ui_status_spy.count(), 1);

    EXPECT_CALL(*trace_source, RequestAbortTrace()).Times(0);

    model->RequestAbort();
}

// Open file tests - CrashAnalysis has special behavior: OnOpenFile queues summary generation
// or opens text editor, not the tool directly
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, OnOpenFileWithTextEditorWithMissingExecutableEmitsSignal)
{
    settings->setValue("txt_editor_path", "missing_txt_editor_path");

    auto       model = MakeModel();
    QSignalSpy txt_editor_missing_spy(model.get(), &CrashAnalysisViewModel::TextEditorMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file.txt")), ::testing::Eq(QString("missing_txt_editor_path"))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFileWithTextEditor("trace_file.txt");

    ASSERT_EQ(1, txt_editor_missing_spy.count());
    EXPECT_EQ("missing_txt_editor_path", txt_editor_missing_spy.first()[0].toString());
}

TEST_F(CrashAnalysisViewModelTest, OnOpenFileWithTextEditorWithEmptyPathUsesEmptyExePath)
{
    // Don't set txt_editor_path in settings

    auto       model = MakeModel();
    QSignalSpy txt_editor_missing_spy(model.get(), &CrashAnalysisViewModel::TextEditorMissing);

    EXPECT_CALL(*file_opener, Open(::testing::Eq(QString("trace_file.txt")), ::testing::Eq(QString(""))))
        .Times(1)
        .WillOnce(::testing::Return(TraceFileOpenerResult::kMissingExecutable));

    model->OnOpenFileWithTextEditor("trace_file.txt");

    ASSERT_EQ(1, txt_editor_missing_spy.count());
}

// Trace completion tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, EmitsTraceCompleteOnSuccess)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &CrashAnalysisViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &CrashAnalysisViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kCompleted, "trace_file.rgd");

    EXPECT_EQ(1, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(CrashAnalysisViewModelTest, EmitsTraceFailedOnError)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &CrashAnalysisViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &CrashAnalysisViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EXPECT_CALL(*file_utils, RemoveFile(::testing::Eq(QString("trace_file.rgd")))).Times(1);

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kError, "trace_file.rgd");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(1, trace_failed_spy.count());
    EXPECT_EQ(0, trace_aborted_spy.count());
}

TEST_F(CrashAnalysisViewModelTest, EmitsTraceAbortedOnAbort)
{
    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &CrashAnalysisViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);
    QSignalSpy trace_aborted_spy(model.get(), &CrashAnalysisViewModel::TraceAborted);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());
    ASSERT_TRUE(trace_aborted_spy.isValid());

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kAborted, "trace_file.rgd");

    EXPECT_EQ(0, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
    EXPECT_EQ(1, trace_aborted_spy.count());
}

TEST_F(CrashAnalysisViewModelTest, DoesNotAutoOpenTraceWhenCompleted)
{
    // CrashAnalysis ShouldAutoOpen always returns false - RGD files need special handling
    settings->setValue("auto_open_traces", true);
    settings->setValue("rgd_path", "exe_path");

    auto       model = MakeModel();
    QSignalSpy trace_complete_spy(model.get(), &CrashAnalysisViewModel::TraceComplete);
    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);

    ASSERT_TRUE(trace_complete_spy.isValid());
    ASSERT_TRUE(trace_failed_spy.isValid());

    // The file opener should NOT be called even with auto_open_traces enabled
    // because CrashAnalysis overrides ShouldAutoOpen to return false
    EXPECT_CALL(*file_opener, Open(::testing::_, ::testing::_)).Times(0);

    EmitCompletionEvent(devtrace::TraceCompletionStatus::kCompleted, "trace_file.rgd");

    EXPECT_EQ(1, trace_complete_spy.count());
    EXPECT_EQ(0, trace_failed_spy.count());
}

// Summary generation tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, QueueSummaryGenerationCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);

    EXPECT_CALL(*trace_source, QueueSummaryGeneration(::testing::Eq("test_path.rgd"), true, false))
        .Times(1)
        .WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->QueueSummaryGeneration("test_path.rgd", true, false);

    // Verify no error signal was emitted
    EXPECT_EQ(0, trace_failed_spy.count());
}

TEST_F(CrashAnalysisViewModelTest, QueueJsonSummaryGenerationCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);

    EXPECT_CALL(*trace_source, QueueSummaryGeneration(::testing::Eq("test_path.rgd"), false, true))
        .Times(1)
        .WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->QueueSummaryGeneration("test_path.rgd", false, true);

    // Verify no error signal was emitted
    EXPECT_EQ(0, trace_failed_spy.count());
}

TEST_F(CrashAnalysisViewModelTest, QueueBothSummariesCallsTraceSource)
{
    auto model = MakeModel();

    QSignalSpy trace_failed_spy(model.get(), &CrashAnalysisViewModel::TraceFailed);

    EXPECT_CALL(*trace_source, QueueSummaryGeneration(::testing::Eq("test_path.rgd"), true, true))
        .Times(1)
        .WillOnce(::testing::Return(devtrace::Result::kSuccess));

    model->QueueSummaryGeneration("test_path.rgd", true, true);

    // Verify no error signal was emitted
    EXPECT_EQ(0, trace_failed_spy.count());
}

// Support event tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, EmitsHardwareCrashAnalysisSupportedChangedOnSupportEvent)
{
    auto model = MakeModel();

    QSignalSpy support_spy(model.get(), &CrashAnalysisViewModel::HardwareCrashAnalysisSupportedChanged);
    QSignalSpy apu_spy(model.get(), &CrashAnalysisViewModel::IsCurrentHardwareApuChanged);

    ASSERT_TRUE(support_spy.isValid());
    ASSERT_TRUE(apu_spy.isValid());

    EmitSupportEvent(true, false);

    EXPECT_EQ(support_spy.count(), 1);
    EXPECT_TRUE(support_spy.last()[0].toBool());
    EXPECT_EQ(apu_spy.count(), 1);
    EXPECT_FALSE(apu_spy.last()[0].toBool());
}

TEST_F(CrashAnalysisViewModelTest, EmitsIsCurrentHardwareApuChangedOnSupportEvent)
{
    auto model = MakeModel();

    QSignalSpy support_spy(model.get(), &CrashAnalysisViewModel::HardwareCrashAnalysisSupportedChanged);
    QSignalSpy apu_spy(model.get(), &CrashAnalysisViewModel::IsCurrentHardwareApuChanged);

    ASSERT_TRUE(support_spy.isValid());
    ASSERT_TRUE(apu_spy.isValid());

    EmitSupportEvent(false, true);

    EXPECT_EQ(apu_spy.count(), 1);
    EXPECT_TRUE(apu_spy.last()[0].toBool());
    EXPECT_EQ(support_spy.count(), 1);
    EXPECT_FALSE(support_spy.last()[0].toBool());
}

TEST_F(CrashAnalysisViewModelTest, EmitsGprCaptureSupportedChangedOnSupportEvent)
{
    auto model = MakeModel();

    QSignalSpy gpr_spy(userdata_view_model.get(), &CrashAnalysisUserdataViewModel::GprCaptureSupportedChanged);

    ASSERT_TRUE(gpr_spy.isValid());

    EmitSupportEvent(true, false, true);

    EXPECT_EQ(gpr_spy.count(), 1);
    EXPECT_TRUE(gpr_spy.last()[0].toBool());

    EmitSupportEvent(true, false, false);

    EXPECT_EQ(gpr_spy.count(), 2);
    EXPECT_FALSE(gpr_spy.last()[0].toBool());
}

// API-specific status tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(CrashAnalysisViewModelTest, HandlesStatusWithDx12Api)
{
    auto       model = MakeModel();
    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);
    QSignalSpy client_status_spy(model.get(), &CrashAnalysisViewModel::ClientStatusChanged);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(client_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kDirectX12);
    EmitStatusEvent(status);

    EXPECT_GE(ui_status_spy.count(), 1);
    EXPECT_GE(client_status_spy.count(), 1);
}

TEST_F(CrashAnalysisViewModelTest, HandlesStatusWithVulkanApi)
{
    auto       model = MakeModel();
    QSignalSpy ui_status_spy(model.get(), &CrashAnalysisViewModel::UiStatusChanged);
    QSignalSpy client_status_spy(model.get(), &CrashAnalysisViewModel::ClientStatusChanged);

    ASSERT_TRUE(ui_status_spy.isValid());
    ASSERT_TRUE(client_status_spy.isValid());

    auto status = MakeStatusWithConnection(devtrace::TraceSourceStage::kCapturing, devtrace::Api::kVulkan);
    EmitStatusEvent(status);

    EXPECT_GE(ui_status_spy.count(), 1);
    EXPECT_GE(client_status_spy.count(), 1);
}
