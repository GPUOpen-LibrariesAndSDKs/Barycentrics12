//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "D3D12Sample.h"

#include <dxgi1_4.h>
#include "d3dx12.h"
#include <iostream>
#include <d3dcompiler.h>
#include <algorithm>

#include "Window.h"

#ifdef max
#undef max
#endif

using namespace Microsoft::WRL;

namespace AMD
{
namespace
{
struct RenderEnvironment
{
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain> swapChain;
};

///////////////////////////////////////////////////////////////////////////////
/**
Create everything we need for rendering, this includes a device, a command queue,
and a swap chain.
*/
RenderEnvironment CreateDeviceAndSwapChainHelper (
    _In_opt_ IDXGIAdapter* adapter,
    D3D_FEATURE_LEVEL minimumFeatureLevel,
    _In_ const DXGI_SWAP_CHAIN_DESC* swapChainDesc)
{
    RenderEnvironment result;

#ifdef DEBUG
    // Enable the D3D12 debug layer.
    {
        ComPtr<ID3D12Debug> debugController;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
        if (hr == S_OK)
        {
            debugController->EnableDebugLayer();
        }
    }
#endif

    auto hr = D3D12CreateDevice (adapter, minimumFeatureLevel,
        IID_PPV_ARGS (&result.device));

    if (FAILED (hr))
    {
        throw std::runtime_error ("Device creation failed.");
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = result.device->CreateCommandQueue (&queueDesc, IID_PPV_ARGS (&result.queue));

    if (FAILED (hr))
    {
        throw std::runtime_error ("Command queue creation failed.");
    }

    ComPtr<IDXGIFactory4> dxgiFactory;

    UINT Flags = 0;
#ifdef DEBUG
    Flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    hr = CreateDXGIFactory2(Flags, IID_PPV_ARGS(&dxgiFactory));

    if (FAILED (hr))
    {
        throw std::runtime_error ("DXGI factory creation failed.");
    }

    // Must copy into non-const space
    DXGI_SWAP_CHAIN_DESC swapChainDescCopy = *swapChainDesc;
    hr = dxgiFactory->CreateSwapChain (
        result.queue.Get (),
        &swapChainDescCopy,
        &result.swapChain
        );

    if (FAILED (hr))
    {
        throw std::runtime_error ("Swap chain creation failed.");
    }

    return result;
}
}   // namespace

///////////////////////////////////////////////////////////////////////////////
D3D12Sample::D3D12Sample () :
    m_agsContext( nullptr )
{
    agsInit( &m_agsContext, nullptr, nullptr );
}

///////////////////////////////////////////////////////////////////////////////
D3D12Sample::~D3D12Sample ()
{
    agsDeInit( m_agsContext );
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::PrepareRender ()
{
    m_commandAllocators [m_currentBackBuffer]->Reset ();

    auto commandList = m_commandLists [m_currentBackBuffer].Get ();
    commandList->Reset (
        m_commandAllocators [m_currentBackBuffer].Get (), nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle;
    CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted (renderTargetHandle,
        m_renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart (),
        m_currentBackBuffer, m_renderTargetViewDescriptorSize);

    commandList->OMSetRenderTargets (1, &renderTargetHandle, true, nullptr);
    commandList->RSSetViewports (1, &m_viewport);
    commandList->RSSetScissorRects (1, &m_rectScissor);

    // Transition back buffer
    D3D12_RESOURCE_BARRIER barrier;
    barrier.Transition.pResource = m_renderTargets [m_currentBackBuffer].Get ();
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier (1, &barrier);

    static const float clearColor [] =
    {
        0.042f, 0.042f, 0.042f,
        1
    };

    commandList->ClearRenderTargetView (renderTargetHandle,
        clearColor, 0, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Render ()
{
    PrepareRender ();
    
    auto commandList = m_commandLists [m_currentBackBuffer].Get ();

    RenderImpl (commandList);
    
    FinalizeRender ();
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::RenderImpl (ID3D12GraphicsCommandList* commandList)
{
    // Set our state (shaders, etc.)
    commandList->SetPipelineState (m_pso.Get ());

    // Set our root signature
    commandList->SetGraphicsRootSignature (m_rootSignature.Get ());
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::FinalizeRender ()
{
    // Transition the swap chain back to present
    D3D12_RESOURCE_BARRIER barrier;
    barrier.Transition.pResource = m_renderTargets [m_currentBackBuffer].Get ();
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    auto commandList = m_commandLists [m_currentBackBuffer].Get ();
    commandList->ResourceBarrier (1, &barrier);

    commandList->Close ();

    // Execute our commands
    ID3D12CommandList* commandLists [] = { commandList };
    m_commandQueue->ExecuteCommandLists (std::extent<decltype(commandLists)>::value, commandLists);
}

namespace
{
void WaitForFence (ID3D12Fence* fence, UINT64 completionValue, HANDLE waitEvent)
{
    if (fence->GetCompletedValue () < completionValue)
    {
        fence->SetEventOnCompletion (completionValue, waitEvent);
        WaitForSingleObject (waitEvent, INFINITE);
    }
}
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Run ()
{
    Initialize ();

    for (;;)
    {
        WaitForFence (m_frameFences[GetQueueSlot ()].Get (),
            m_fenceValues[GetQueueSlot ()], m_frameFenceEvents[GetQueueSlot ()]);

        if (m_window->MessagePump() == false)
        {
            break;
        }

        Render();
        Present();
    }

    // Drain the queue, wait for everything to finish
    for (int i = 0; i < GetQueueSlotCount (); ++i)
    {
        WaitForFence (m_frameFences[i].Get (), m_fenceValues[i], m_frameFenceEvents[i]);
    }

    Shutdown ();
}

///////////////////////////////////////////////////////////////////////////////
/**
Setup all render targets. This creates the render target descriptor heap and
render target views for all render targets.

This function does not use a default view but instead changes the format to
_SRGB.
*/
void D3D12Sample::SetupRenderTargets ()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = GetQueueSlotCount ();
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_device->CreateDescriptorHeap (&heapDesc, IID_PPV_ARGS (&m_renderTargetDescriptorHeap));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ m_renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart () };

    for (int i = 0; i < GetQueueSlotCount (); ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC viewDesc;
        viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipSlice = 0;
        viewDesc.Texture2D.PlaneSlice = 0;

        m_device->CreateRenderTargetView (m_renderTargets [i].Get (), &viewDesc,
            rtvHandle);

        rtvHandle.Offset (m_renderTargetViewDescriptorSize);
    }
}

///////////////////////////////////////////////////////////////////////////////
/**
Present the current frame by swapping the back buffer, then move to the
next back buffer and also signal the fence for the current queue slot entry.
*/
void D3D12Sample::Present ()
{
    m_swapChain->Present (1, 0);

    // Mark the fence for the current frame.
    const auto fenceValue = m_currentFenceValue;
    m_commandQueue->Signal (m_frameFences [m_currentBackBuffer].Get (), fenceValue);
    m_fenceValues[m_currentBackBuffer] = fenceValue;
    ++m_currentFenceValue;

    // Take the next back buffer from our chain
    m_currentBackBuffer = (m_currentBackBuffer + 1) % GetQueueSlotCount ();
}

///////////////////////////////////////////////////////////////////////////////
/**
Set up swap chain related resources, that is, the render target view, the
fences, and the descriptor heap for the render target.
*/
void D3D12Sample::SetupSwapChain ()
{
    m_currentFenceValue = 1;

    // Create fences for each frame so we can protect resources and wait for
    // any given frame
    for (int i = 0; i < GetQueueSlotCount (); ++i)
    {
        m_frameFenceEvents [i] = CreateEvent (nullptr, FALSE, FALSE, nullptr);
        m_fenceValues [i] = 0;
        m_device->CreateFence (0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS (&m_frameFences [i]));
    }

    for (int i = 0; i < GetQueueSlotCount (); ++i)
    {
        m_swapChain->GetBuffer (i, IID_PPV_ARGS (&m_renderTargets [i]));
    }

    SetupRenderTargets ();
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Initialize ()
{
    m_window.reset (new Window ("AMD Barycentrics12", 1280, 720));

    CreateDeviceAndSwapChain ();
    CreateAllocatorsAndCommandLists ();
    CreateViewportScissor ();
    
    // Create our upload fence, command list and command allocator
    // This will be only used while creating the mesh buffer and the texture
    // to upload data to the GPU.
    ComPtr<ID3D12Fence> uploadFence;
    m_device->CreateFence (0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&uploadFence));

    ComPtr<ID3D12CommandAllocator> uploadCommandAllocator;
    m_device->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS (&uploadCommandAllocator));
    ComPtr<ID3D12GraphicsCommandList> uploadCommandList;
    m_device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadCommandAllocator.Get (), nullptr,
        IID_PPV_ARGS (&uploadCommandList));

    InitializeImpl (uploadCommandList.Get ());

    uploadCommandList->Close ();

    // Execute the upload and finish the command list
    ID3D12CommandList* commandLists [] = { uploadCommandList.Get () };
    m_commandQueue->ExecuteCommandLists (std::extent<decltype(commandLists)>::value, commandLists);
    m_commandQueue->Signal (uploadFence.Get (), 1);

    auto waitEvent = CreateEvent (nullptr, FALSE, FALSE, nullptr);

    if (waitEvent == NULL)
    {
        throw std::runtime_error ("Could not create wait event.");
    }

    WaitForFence (uploadFence.Get (), 1, waitEvent);

    // Cleanup our upload handle
    uploadCommandAllocator->Reset ();

    CloseHandle (waitEvent);
}

void D3D12Sample::InitializeImpl (ID3D12GraphicsCommandList * /*uploadCommandList*/)
{
}


void D3D12Sample::ShutdownImpl()
{
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::Shutdown ()
{
    ShutdownImpl();

    for (auto event : m_frameFenceEvents)
    {
        CloseHandle (event);
    }
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateDeviceAndSwapChain ()
{
    // Enable the debug layers when in debug mode
    // If this fails, install the Graphics Tools for Windows. On Windows 10,
    // open settings, Apps, Apps & Features, Optional features, Add Feature,
    // and add the graphics tools
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    D3D12GetDebugInterface (IID_PPV_ARGS (&debugController));
    debugController->EnableDebugLayer ();
#endif

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ::ZeroMemory (&swapChainDesc, sizeof (swapChainDesc));

    swapChainDesc.BufferCount = GetQueueSlotCount ();
    // This is _UNORM but we'll use a _SRGB view on this. See
    // SetupRenderTargets() for details, it must match what
    // we specify here
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferDesc.Width = m_window->GetWidth ();
    swapChainDesc.BufferDesc.Height = m_window->GetHeight ();
    swapChainDesc.OutputWindow = m_window->GetHWND ();
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Windowed = true;

    auto renderEnv = CreateDeviceAndSwapChainHelper (nullptr, D3D_FEATURE_LEVEL_11_0,
        &swapChainDesc);

    m_device = renderEnv.device;
    m_commandQueue = renderEnv.queue;
    m_swapChain = renderEnv.swapChain;

    m_renderTargetViewDescriptorSize =
        m_device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    SetupSwapChain ();
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateAllocatorsAndCommandLists ()
{
    for (int i = 0; i < GetQueueSlotCount (); ++i)
    {
        m_device->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS (&m_commandAllocators [i]));
        m_device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators [i].Get (), nullptr,
            IID_PPV_ARGS (&m_commandLists [i]));
        m_commandLists [i]->Close ();
    }
}

///////////////////////////////////////////////////////////////////////////////
void D3D12Sample::CreateViewportScissor ()
{
    m_rectScissor = { 0, 0, m_window->GetWidth (), m_window->GetHeight () };

    m_viewport =
    {
        0.0f, 0.0f,
        static_cast<float>(m_window->GetWidth ()),
        static_cast<float>(m_window->GetHeight ()),
        0.0f, 1.0f
    };
}
}   // namespace AMD
