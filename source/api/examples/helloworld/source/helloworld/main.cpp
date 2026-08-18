// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>

#include "RdpCaptureApi.h"
#include "example.hpp"
#include "file.hpp"

#include <imgui.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_main.h>

extern "C" {
__declspec(dllexport) extern const UINT D3D12SDKVersion = 614;
}

extern "C" {
__declspec(dllexport) extern const auto* D3D12SDKPath = u8".\\D3D12\\";
}

// Self-capture support via the RDP Capture API. The sample drives the AMD
// developer-mode driver to capture a trace of its own frame for the selected
// feature. All setup must run before the D3D12 device is created (see InitCapture /
// main).
struct CaptureSession
{
    RdpCaptureFnTable  fn{};
    RdpCaptureContext  context = nullptr;
    RdpCaptureFeature  feature = kRdpCaptureFeatureProfiling;
    std::atomic<bool>  inFlight{false};
    std::atomic<float> progress{0.0f};
};

// File extension for the trace produced by each feature.
static const char* FeatureFileExtension(RdpCaptureFeature feature)
{
    switch (feature)
    {
    case kRdpCaptureFeatureMemoryTrace:
        return "rmv";
    case kRdpCaptureFeatureRaytracing:
        return "rra";
    case kRdpCaptureFeatureProfiling:
    default:
        return "rgp";
    }
}

// Short human-readable name for the selected feature (used in the UI).
static const char* FeatureDisplayName(RdpCaptureFeature feature)
{
    switch (feature)
    {
    case kRdpCaptureFeatureMemoryTrace:
        return "RMV";
    case kRdpCaptureFeatureRaytracing:
        return "RRA";
    case kRdpCaptureFeatureProfiling:
    default:
        return "RGP";
    }
}

// True when a new trace can be requested for the selected feature. Profiling and
// raytracing sit at ReadyForCapture between traces, but memory tracing runs
// continuously once connected and reports Capturing, with each dump_trace pulling
// a snapshot out of the live trace.
static bool CaptureReadyToDump(const CaptureSession& session)
{
    const RdpCaptureFeatureStage stage = session.fn.get_feature_stage(session.context, session.feature, 0);
    if (session.feature == kRdpCaptureFeatureMemoryTrace)
    {
        return stage == kRdpCaptureFeatureStageCapturing;
    }
    return stage == kRdpCaptureFeatureStageReadyForCapture;
}

// Progress is reported from internal API threads, so store it atomically for the
// UI thread to read. progress is a [0.0, 1.0] value for the current stage.
static void OnCaptureProgress(void* user_data, const RdpCaptureProgressInfo* progress_info)
{
    auto* session     = static_cast<CaptureSession*>(user_data);
    session->progress = progress_info->progress;
}

static void OnTraceFinished(void*             user_data,
                            RdpCaptureFeature feature,
                            RdpCaptureApiConnectionId /*connection*/,
                            RdpCaptureResult result,
                            uint64_t         size,
                            const uint8_t*   data)
{
    auto* session = static_cast<CaptureSession*>(user_data);

    // data is owned by the API and freed after this returns, so copy it out.
    if (result == kRdpCaptureResultSuccess && data != nullptr && size > 0)
    {
        // Write next to the exe (SDL_GetBasePath) rather than the process CWD.
        const char* basePath = SDL_GetBasePath();
        std::string path     = (basePath != nullptr ? basePath : "");
        path += "helloworld.";
        path += FeatureFileExtension(feature);

        FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "wb") == 0 && file != nullptr)
        {
            fwrite(data, 1, static_cast<size_t>(size), file);
            fclose(file);
        }
    }

    session->inFlight = false;
}

// Initializes the capture API and enables the requested feature. Must be called
// before the first D3D12 device is created. Returns false (leaving the sample to
// run without capture) if the driver is unavailable or another capture context
// already exists.
static bool InitCapture(CaptureSession& session, RdpCaptureFeature feature)
{
    session.feature = feature;

    if (RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR, RDP_CAPTURE_API_VERSION_MINOR, RDP_CAPTURE_API_VERSION_PATCH, &session.fn) !=
        kRdpCaptureResultSuccess)
    {
        return false;
    }

    // A NULL app filter connects to all APIs for the current process (self-capture).
    RdpCaptureContextInitParams init{};
    if (session.fn.initialize(&init, &session.context) != kRdpCaptureResultSuccess)
    {
        return false;
    }

    RdpCaptureFeatureEnableParams enable{};
    enable.feature                                = feature;
    enable.trace_finished_callback.trace_finished = OnTraceFinished;
    enable.trace_finished_callback.user_data      = &session;
    enable.progress_callback.progress_updated     = OnCaptureProgress;
    enable.progress_callback.user_data            = &session;
    if (session.fn.enable_feature(session.context, &enable) != kRdpCaptureResultSuccess)
    {
        return false;
    }

    // Memory Trace has no tunable capture params; the others take their defaults.
    if (feature == kRdpCaptureFeatureProfiling)
    {
        RdpCaptureProfilingParams params{};
        session.fn.profiling.get_default_params(&params);
        session.fn.profiling.set_params(session.context, &params);
    }
    else if (feature == kRdpCaptureFeatureRaytracing)
    {
        RdpCaptureRaytracingParams params{};
        session.fn.raytracing.get_default_params(&params);
        session.fn.raytracing.set_params(session.context, &params);
    }

    return true;
}

static void ShutdownCapture(CaptureSession& session)
{
    if (session.context != nullptr)
    {
        session.fn.destroy(session.context);
    }
}

using namespace DirectX;

XM_ALIGNED_STRUCT(16) Vertex
{
    Vector3 Position;
    Vector4 Color;
};

XM_ALIGNED_STRUCT(256) SceneConstantBuffer
{
    Matrix ModelViewProjection;
};

class HelloWorld final : public Example
{
public:
    explicit HelloWorld(bool fullscreen, CaptureSession* capture = nullptr);
    HelloWorld(const HelloWorld& other)            = delete;
    HelloWorld& operator=(const HelloWorld& other) = delete;

    ~HelloWorld() override;

    bool Load() override;

    void Update(const GameTimer& timer) override;

    void Render(ID3D12GraphicsCommandList* commandList, const GameTimer& timer) override;

    void OnGui() override;

private:
    void CreateRootSignature();

    void CreateBuffers();

    void CreatePipelineState();

    void UpdateUniforms();

    // Begins a capture of the selected feature if the API is ready and none is in flight.
    void RequestCapture();

    winrt::com_ptr<ID3D12RootSignature>  m_rootSignature;
    winrt::com_ptr<ID3D12PipelineState>  m_pipelineState;
    winrt::com_ptr<ID3D12Resource>       m_vertexBuffer;
    winrt::com_ptr<ID3D12Resource>       m_indexBuffer;
    winrt::com_ptr<ID3D12DescriptorHeap> m_cbvDescriptorHeap;
    D3D12_VERTEX_BUFFER_VIEW             m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW              m_indexBufferView;
    winrt::com_ptr<ID3D12Resource>       m_constBuffer;
    SceneConstantBuffer                  m_constBufferData;
    UINT8*                               m_constBufferDataBegin;
    float                                m_cubeRotationY = 0.0f;
    CaptureSession*                      m_capture       = nullptr;
};

HelloWorld::HelloWorld(bool fullscreen, CaptureSession* capture)
    : Example("HelloWorld", 800, 600, fullscreen)
    , m_vertexBufferView()
    , m_constBufferDataBegin(nullptr)
    , m_capture(capture)
{
}

HelloWorld::~HelloWorld() = default;

bool HelloWorld::Load()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvDescriptorHeapDesc = {};
    cbvDescriptorHeapDesc.NumDescriptors             = 1;
    cbvDescriptorHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvDescriptorHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    winrt::check_hresult(m_context->Device()->CreateDescriptorHeap(&cbvDescriptorHeapDesc, IID_PPV_ARGS(&m_cbvDescriptorHeap)));

    CreateRootSignature();

    CreateBuffers();

    CreatePipelineState();

    // Cursor stays visible so it can click the ImGui capture button.
    SDL_ShowCursor();

    return true;
}

void HelloWorld::Update(const GameTimer& timer)
{
    const auto elapsed = static_cast<float>(timer.GetElapsedSeconds());

    // Press F11, or click the ImGui button, to capture the selected feature.
    if (m_keyboard->IsKeyClicked(SDL_SCANCODE_F11))
    {
        RequestCapture();
    }

    m_cubeRotationY += elapsed;
}

void HelloWorld::RequestCapture()
{
    if (m_capture == nullptr || m_capture->inFlight)
    {
        return;
    }

    if (CaptureReadyToDump(*m_capture))
    {
        m_capture->progress = 0.0f;
        m_capture->inFlight = true;

        switch (m_capture->feature)
        {
        case kRdpCaptureFeatureMemoryTrace:
            m_capture->fn.memory_trace.dump_trace(m_capture->context, RDP_CAPTURE_API_FIRST_CONNECTION_ID);
            break;
        case kRdpCaptureFeatureRaytracing:
            m_capture->fn.raytracing.begin_trace(m_capture->context, RDP_CAPTURE_API_FIRST_CONNECTION_ID);
            break;
        case kRdpCaptureFeatureProfiling:
        default:
            m_capture->fn.profiling.begin_trace(m_capture->context, RDP_CAPTURE_API_FIRST_CONNECTION_ID);
            break;
        }
    }
}

void HelloWorld::OnGui()
{
    // Pin the window to the top-right of the viewport's work area.
    constexpr float      kPad     = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2         pos      = {viewport->WorkPos.x + viewport->WorkSize.x - kPad, viewport->WorkPos.y + kPad};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {1.0f, 0.0f});

    ImGui::Begin("GPU Capture", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    if (m_capture == nullptr)
    {
        ImGui::TextUnformatted("Capture API unavailable.");
    }
    else
    {
        ImGui::Text("Feature: %s", FeatureDisplayName(m_capture->feature));

        const bool inFlight = m_capture->inFlight;
        const bool ready    = !inFlight && CaptureReadyToDump(*m_capture);

        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Capture Frame"))
        {
            RequestCapture();
        }
        ImGui::EndDisabled();

        ImGui::TextUnformatted(inFlight ? "Capturing..." : (ready ? "Ready" : "Waiting for driver..."));

        if (inFlight)
        {
            ImGui::ProgressBar(m_capture->progress.load(), ImVec2(-1.0f, 0.0f));
        }
    }

    ImGui::End();
}

void HelloWorld::Render(ID3D12GraphicsCommandList* commandList, const GameTimer& timer)
{
    UpdateUniforms();

    // Set the root signature
    commandList->SetGraphicsRootSignature(m_rootSignature.get());

    ID3D12DescriptorHeap* heaps[] = {m_cbvDescriptorHeap.get()};
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    commandList->SetGraphicsRootDescriptorTable(0, m_cbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    // Set the pipeline state
    commandList->SetPipelineState(m_pipelineState.get());

    // Set the primitive topology
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set the vertex buffer
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    // Set the index buffer
    commandList->IASetIndexBuffer(&m_indexBufferView);

    // Draw indexed geometry
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void HelloWorld::UpdateUniforms()
{
    auto position    = Vector3(0.0f, 0.0, -10.0f);
    auto rotationX   = 0.0f;
    auto rotationY   = m_cubeRotationY;
    auto scaleFactor = 3.0f;

    const Vector3 xAxis = Vector3::Right;
    const Vector3 yAxis = Vector3::Up;

    Matrix xRot     = Matrix::CreateFromAxisAngle(xAxis, rotationX);
    Matrix yRot     = Matrix::CreateFromAxisAngle(yAxis, rotationY);
    Matrix rotation = xRot * yRot;

    Matrix translation = Matrix::CreateTranslation(position);
    Matrix scale       = Matrix::CreateScale(scaleFactor);
    Matrix model       = scale * rotation * translation;

    m_constBufferData.ModelViewProjection = model * m_camera->viewProjection();
    memcpy(m_constBufferDataBegin, &m_constBufferData, sizeof(m_constBufferData));
}

void HelloWorld::CreateRootSignature()
{
    ID3D12Device* device = m_context->Device();

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    CD3DX12_ROOT_PARAMETER1   rootParams[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr, rootSignatureFlags);

    winrt::com_ptr<ID3DBlob> signature;
    winrt::com_ptr<ID3DBlob> error;
    winrt::check_hresult(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, signature.put(), error.put()));
    winrt::check_hresult(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void HelloWorld::CreatePipelineState()
{
    ID3D12Device* device = m_context->Device();

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                                                   {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    auto rasterDesc                  = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterDesc.CullMode              = D3D12_CULL_MODE_BACK;
    rasterDesc.FrontCounterClockwise = TRUE;

    std::vector<uint8_t> vertexShader;
    std::vector<uint8_t> pixelShader;
    try
    {
        File vs("SimpleShaderVS.bin");
        File ps("SimpleShaderPS.bin");

        vertexShader = vs.ReadAll();
        pixelShader  = ps.ReadAll();
    }
    catch (const std::exception& e)
    {
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout                        = {inputElementDesc, _countof(inputElementDesc)};
    psoDesc.pRootSignature                     = m_rootSignature.get();
    psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(vertexShader.data(), vertexShader.size());
    psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(pixelShader.data(), pixelShader.size());
    psoDesc.RasterizerState                    = rasterDesc;
    psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable      = FALSE;
    psoDesc.DepthStencilState.StencilEnable    = FALSE;
    psoDesc.SampleMask                         = UINT_MAX;
    psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                   = 1;
    psoDesc.RTVFormats[0]                      = m_context->BackBufferFormat();
    psoDesc.SampleDesc.Count                   = 1;
    winrt::check_hresult(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void HelloWorld::CreateBuffers()
{
    ID3D12Device* device = m_context->Device();

    // Define the geometry for a cube.
    constexpr Vertex cubeVertices[] = {
        // Front Face
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

        // Back Face
        {{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

        // Top Face
        {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

        // Bottom Face
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

        // Left Face
        {{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},

        // Right Face
        {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    };

    // Define indices for a cube so that each triangle is front-facing
    constexpr uint16_t cubeIndices[] = {
        0,  1,  2,  0,  2,  3,   // Front Face
        4,  5,  6,  4,  6,  7,   // Back Face
        8,  9,  10, 8,  10, 11,  // Top Face
        12, 13, 14, 12, 14, 15,  // Bottom Face
        16, 17, 18, 16, 18, 19,  // Left Face
        20, 21, 22, 20, 22, 23   // Right Face
    };

    constexpr UINT vertexBufferSize = sizeof(cubeVertices);
    const auto     heapProps        = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto     resourceDesc     = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    winrt::check_hresult(
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBuffer)));
    UINT8*              vertexDataBegin;
    const CD3DX12_RANGE readRange(0, 0);
    winrt::check_hresult(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
    memcpy(vertexDataBegin, cubeVertices, sizeof(cubeVertices));
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes  = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes    = vertexBufferSize;

    constexpr UINT indexBufferSize   = sizeof(cubeIndices);
    const auto     indexHeapProps    = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto     indexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    winrt::check_hresult(device->CreateCommittedResource(
        &indexHeapProps, D3D12_HEAP_FLAG_NONE, &indexResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBuffer)));

    // Copy cube index data to index buffer
    UINT8* indexDataBegin;
    winrt::check_hresult(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indexDataBegin)));
    memcpy(indexDataBegin, cubeIndices, sizeof(cubeIndices));
    m_indexBuffer->Unmap(0, nullptr);
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format         = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes    = indexBufferSize;

    // Create the constant buffer.
    {
        constexpr UINT constantBufferSize = sizeof(SceneConstantBuffer);  // CB size is required to be 256-byte aligned.

        const auto cbHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const auto cbHeapDesc  = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
        winrt::check_hresult(device->CreateCommittedResource(
            &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbHeapDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_constBuffer)));

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = m_constBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes    = constantBufferSize;
        device->CreateConstantBufferView(&cbvDesc, m_cbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay
        winrt::check_hresult(m_constBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constBufferDataBegin)));
        memcpy(m_constBufferDataBegin, &m_constBufferData, sizeof(m_constBufferData));
    }
}

int main(const int argc, char** argv)
{
    bool              fullscreen = false;
    RdpCaptureFeature feature    = kRdpCaptureFeatureProfiling;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--fullscreen") == 0)
        {
            fullscreen = true;
        }
        else if (strcmp(argv[i], "--feature") == 0 && i + 1 < argc)
        {
            const char* name = argv[++i];
            if (_stricmp(name, "rgp") == 0)
            {
                feature = kRdpCaptureFeatureProfiling;
            }
            else if (_stricmp(name, "rmv") == 0)
            {
                feature = kRdpCaptureFeatureMemoryTrace;
            }
            else if (_stricmp(name, "rra") == 0)
            {
                feature = kRdpCaptureFeatureRaytracing;
            }
            else
            {
                fprintf(stderr, "Unknown --feature '%s'; expected rgp, rmv, or rra. Defaulting to rgp.\n", name);
            }
        }
    }
    // Capture setup must run before the D3D12 device is created (during HelloWorld
    // construction), since for self-capture the process connects at device creation.
    CaptureSession capture;
    const bool     captureReady = InitCapture(capture, feature);

    const auto example = std::make_unique<HelloWorld>(fullscreen, captureReady ? &capture : nullptr);
    const int  result  = example->Run(argc, argv);

    ShutdownCapture(capture);
    return result;
}
