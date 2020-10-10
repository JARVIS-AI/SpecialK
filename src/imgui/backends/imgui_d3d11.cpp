// ImGui Win32 + DirectX11 binding
// In this binding, ImTextureID is used to store a 'ID3D11ShaderResourceView*' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <SpecialK/stdafx.h>
#include <SpecialK/com_util.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_d3d11.h>
#include <SpecialK/render/dxgi/dxgi_backend.h>
#include <SpecialK/render/d3d11/d3d11_core.h>

#include <shaders/imgui_d3d11_vs.h>
#include <shaders/imgui_d3d11_ps.h>

#include <shaders/steam_d3d11_vs.h>
#include <shaders/steam_d3d11_ps.h>

#include <shaders/uplay_d3d11_vs.h>
#include <shaders/uplay_d3d11_ps.h>

// DirectX
#include <d3d11.h>

bool __SK_ImGui_D3D11_DrawDeferred = false;
bool running_on_12                 = false;

extern void
SK_ImGui_User_NewFrame (void);

// Data
static INT64                    g_Time                  = 0;
static INT64                    g_TicksPerSecond        = 0;

static ImGuiMouseCursor         g_LastMouseCursor       = ImGuiMouseCursor_COUNT;
static bool                     g_HasGamepad            = false;
static bool                     g_WantUpdateHasGamepad  = true;

static HWND                     g_hWnd                  = nullptr;



#define D3D11_SHADER_MAX_INSTANCES_PER_CLASS 256
#define D3D11_MAX_SCISSOR_AND_VIEWPORT_ARRAYS \
  D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE

#include <array>

template <typename _Tp, size_t n> using CComPtrArray = std::array <CComPtr <_Tp>, n>;

template <typename _Type>
struct D3D11ShaderState
{
  CComPtr <_Type>                     Shader;
  CComPtrArray <
    ID3D11Buffer,             D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT
  >                                   Constants;
  CComPtrArray <
    ID3D11ShaderResourceView, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT
  >                                   Resources;
  CComPtrArray <
    ID3D11SamplerState,       D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT
  >                                   Samplers;
  struct {
    UINT                              Count;
    CComPtrArray <
      ID3D11ClassInstance,    D3D11_SHADER_MAX_INSTANCES_PER_CLASS
    >                                 Array;
  } Instances;
};

using _VS = D3D11ShaderState <ID3D11VertexShader>;
using _PS = D3D11ShaderState <ID3D11PixelShader>;
using _GS = D3D11ShaderState <ID3D11GeometryShader>;
using _DS = D3D11ShaderState <ID3D11DomainShader>;
using _HS = D3D11ShaderState <ID3D11HullShader>;
using _CS = D3D11ShaderState <ID3D11ComputeShader>;

// Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
struct SK_IMGUI_D3D11StateBlock {
  struct {
    UINT                              RectCount;
    D3D11_RECT                        Rects [D3D11_MAX_SCISSOR_AND_VIEWPORT_ARRAYS];
  } Scissor;

  struct {
    UINT                              ArrayCount;
    D3D11_VIEWPORT                    Array [D3D11_MAX_SCISSOR_AND_VIEWPORT_ARRAYS];
  } Viewport;

  struct {
    CComPtr <ID3D11RasterizerState>   State;
  } Rasterizer;

  struct {
    CComPtr <ID3D11BlendState>        State;
    FLOAT                             Factor [4];
    UINT                              SampleMask;
  } Blend;

  struct {
    UINT                              StencilRef;
    CComPtr <ID3D11DepthStencilState> State;
  } DepthStencil;

  struct {
    _VS                               Vertex;
    _PS                               Pixel;
    _GS                               Geometry;
    _DS                               Domain;
    _HS                               Hull;
    _CS                               Compute;
  } Shaders;

  struct {
    struct {
      CComPtr <ID3D11Buffer>          Pointer;
      DXGI_FORMAT                     Format;
      UINT                            Offset;
    } Index;

    struct {
      CComPtrArray <ID3D11Buffer,               D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT>
                                      Pointers;
      UINT                            Strides  [D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
      UINT                            Offsets  [D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    } Vertex;
  } Buffers;

  struct {
    CComPtrArray <
      ID3D11RenderTargetView, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT
    >                                 RenderTargetViews;
    CComPtr <
      ID3D11DepthStencilView
    >                                 DepthStencilView;
  } RenderTargets;

  D3D11_PRIMITIVE_TOPOLOGY            PrimitiveTopology;
  CComPtr <ID3D11InputLayout>         InputLayout;

  enum _StateMask : DWORD {
    VertexStage         = 0x0001,
    PixelStage          = 0x0002,
    GeometryStage       = 0x0004,
    HullStage           = 0x0008,
    DomainStage         = 0x0010,
    ComputeStage        = 0x0020,
    RasterizerState     = 0x0040,
    BlendState          = 0x0080,
    OutputMergeState    = 0x0100,
    DepthStencilState   = 0x0200,
    InputAssemblerState = 0x0400,
    ViewportState       = 0x0800,
    ScissorState        = 0x1000,
    RenderTargetState   = 0x2000,
    _StateMask_All      = 0xffffffff
  };

#define        STAGE_INPUT_RESOURCE_SLOT_COUNT \
  D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT
#define        STAGE_CONSTANT_BUFFER_API_SLOT_COUNT \
  D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT
#define        STAGE_SAMPLER_SLOT_COUNT \
  D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT

#define _Stage(StageName) Shaders.##StageName
#define Stage_Get(_Tp)    pDevCtx->##_Tp##Get
#define Stage_Set(_Tp)    pDevCtx->##_Tp##Set

  void Capture ( ID3D11DeviceContext* pDevCtx,
                 DWORD                iStateMask = _StateMask_All )
  {
    if (iStateMask & ScissorState)
       pDevCtx->RSGetScissorRects      ( &Scissor.RectCount,
                                          Scissor.Rects    );
    if (iStateMask & ViewportState)
       pDevCtx->RSGetViewports         ( &Viewport.ArrayCount,
                                          Viewport.Array   );
    if (iStateMask & RasterizerState)
      pDevCtx->RSGetState              ( &Rasterizer.State );

    if (iStateMask & BlendState)
       pDevCtx->OMGetBlendState        ( &Blend.State,
                                          Blend.Factor,
                                         &Blend.SampleMask );

    if (iStateMask & DepthStencilState)
       pDevCtx->OMGetDepthStencilState ( &DepthStencil.State,
                                         &DepthStencil.StencilRef );

    if (iStateMask & RenderTargetState)
    {
      pDevCtx->OMGetRenderTargets ( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                    &RenderTargets.RenderTargetViews [0].p,
                                    &RenderTargets.DepthStencilView.p );
    }

#define _BackupStage(_Tp,StageName)                                               \
  if (iStateMask & StageName##Stage) {                                            \
    Stage_Get(_Tp)ShaderResources   ( 0, STAGE_INPUT_RESOURCE_SLOT_COUNT,         \
                                       &_Stage(StageName).Resources       [0].p );\
    Stage_Get(_Tp)ConstantBuffers   ( 0, STAGE_CONSTANT_BUFFER_API_SLOT_COUNT,    \
                                       &_Stage(StageName).Constants       [0].p );\
    Stage_Get(_Tp)Samplers          ( 0, STAGE_SAMPLER_SLOT_COUNT,                \
                                       &_Stage(StageName).Samplers        [0].p );\
    Stage_Get(_Tp)Shader            (  &_Stage(StageName).Shader,                 \
                                       &_Stage(StageName).Instances.Array [0].p,  \
                                       &_Stage(StageName).Instances.Count       );\
  }

    _BackupStage ( VS, Vertex   );
    _BackupStage ( PS, Pixel    );
    _BackupStage ( GS, Geometry );
    _BackupStage ( DS, Domain   );
    _BackupStage ( HS, Hull     );
    _BackupStage ( CS, Compute  );

    if (iStateMask & InputAssemblerState)
    {
      pDevCtx->IAGetPrimitiveTopology ( &PrimitiveTopology );
      pDevCtx->IAGetIndexBuffer       ( &Buffers.Index.Pointer,
                                        &Buffers.Index.Format,
                                        &Buffers.Index.Offset );
      pDevCtx->IAGetVertexBuffers     ( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                                         &Buffers.Vertex.Pointers [0].p,
                                          Buffers.Vertex.Strides,
                                          Buffers.Vertex.Offsets );
      pDevCtx->IAGetInputLayout       ( &InputLayout );
    }
  }

  void Apply ( ID3D11DeviceContext* pDevCtx,
               DWORD                iStateMask = _StateMask_All )
  {
    if (iStateMask & InputAssemblerState)
    {
        pDevCtx->IASetPrimitiveTopology ( PrimitiveTopology );
        pDevCtx->IASetIndexBuffer       ( Buffers.Index.Pointer,
                                          Buffers.Index.Format,
                                          Buffers.Index.Offset );
                                          Buffers.Index.Pointer = nullptr;
        pDevCtx->IASetVertexBuffers     ( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                                         &Buffers.Vertex.Pointers [0].p,
                                          Buffers.Vertex.Strides,
                                          Buffers.Vertex.Offsets );
                              std::fill ( Buffers.Vertex.Pointers.begin (),
                                          Buffers.Vertex.Pointers.end   (),
                                            nullptr );

        pDevCtx->IASetInputLayout       ( InputLayout );
                                          InputLayout = nullptr;
    }

  #define _RestoreStage(_Tp,StageName)                                              \
    if (iStateMask & StageName##Stage) {                                            \
      Stage_Set(_Tp)Shader           ( _Stage(StageName).Shader,                    \
                                      &_Stage(StageName).Instances.Array  [0].p,    \
                                       _Stage(StageName).Instances.Count );         \
                           std::fill ( _Stage(StageName).Instances.Array.begin ( ), \
                                       _Stage(StageName).Instances.Array.end   ( ), \
                                        nullptr );                                  \
      Stage_Set(_Tp)Samplers         ( 0, STAGE_SAMPLER_SLOT_COUNT,                 \
                                        &_Stage(StageName).Samplers        [0].p ); \
                             std::fill ( _Stage(StageName).Samplers.begin  ( ),     \
                                         _Stage(StageName).Samplers.end    ( ),     \
                                           nullptr );                               \
      Stage_Set(_Tp)ConstantBuffers  ( 0, STAGE_CONSTANT_BUFFER_API_SLOT_COUNT,     \
                                        &_Stage(StageName).Constants       [0].p ); \
                             std::fill ( _Stage(StageName).Constants.begin ( ),     \
                                         _Stage(StageName).Constants.end   ( ),     \
                                           nullptr );                               \
      Stage_Set(_Tp)ShaderResources  ( 0, STAGE_INPUT_RESOURCE_SLOT_COUNT,          \
                                        &_Stage(StageName).Resources       [0].p ); \
                             std::fill ( _Stage(StageName).Resources.begin ( ),     \
                                         _Stage(StageName).Resources.end   ( ),     \
                                           nullptr );                               \
    }

    _RestoreStage ( CS, Compute  );
    _RestoreStage ( HS, Hull     );
    _RestoreStage ( DS, Domain   );
    _RestoreStage ( GS, Geometry );
    _RestoreStage ( PS, Pixel    );
    _RestoreStage ( VS, Vertex   );

    if (iStateMask & RenderTargetState)
    {
      pDevCtx->OMSetRenderTargets ( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                    &RenderTargets.RenderTargetViews [0].p,
                                     RenderTargets.DepthStencilView.p );

      std::fill ( RenderTargets.RenderTargetViews.begin (),
                  RenderTargets.RenderTargetViews.end   (),
                    nullptr );
    }

    if (iStateMask & DepthStencilState)
    {  pDevCtx->OMSetDepthStencilState ( DepthStencil.State,
                                         DepthStencil.StencilRef );
                                         DepthStencil.State = nullptr;
    }

    if (iStateMask & BlendState)
    {  pDevCtx->OMSetBlendState        ( Blend.State,
                                         Blend.Factor,
                                         Blend.SampleMask);
                                         Blend.State        = nullptr;
    }

    if (iStateMask & RasterizerState)
    {
      pDevCtx->RSSetState              ( Rasterizer.State );
                                         Rasterizer.State   = nullptr;
    }

    if (iStateMask & ViewportState)
       pDevCtx->RSSetViewports         ( Viewport.ArrayCount, Viewport.Array );

    if (iStateMask & ScissorState)
       pDevCtx->RSSetScissorRects      ( Scissor.RectCount,   Scissor.Rects  );
    }
};


static constexpr           UINT _MAX_BACKBUFFERS        = 1;

struct SK_ImGui_D3D11_BackbufferResourceIsolation {
  ID3D11VertexShader*       pVertexShader         = nullptr;
  ID3D11VertexShader*       pVertexShaderSteamHDR = nullptr;
  ID3D11VertexShader*       pVertexShaderuPlayHDR = nullptr;
  ID3D11PixelShader*        pPixelShaderuPlayHDR  = nullptr;
  ID3D11PixelShader*        pPixelShaderSteamHDR  = nullptr;
  ID3D11InputLayout*        pInputLayout          = nullptr;
  ID3D11Buffer*             pVertexConstantBuffer = nullptr;
  ID3D11Buffer*             pPixelConstantBuffer  = nullptr;
  ID3D11PixelShader*        pPixelShader          = nullptr;
  ID3D11SamplerState*       pFontSampler_clamp    = nullptr;
  ID3D11SamplerState*       pFontSampler_wrap     = nullptr;
  ID3D11ShaderResourceView* pFontTextureView      = nullptr;
  ID3D11RasterizerState*    pRasterizerState      = nullptr;
  ID3D11BlendState*         pBlendState           = nullptr;
  ID3D11DepthStencilState*  pDepthStencilState    = nullptr;

  SK_ComPtr
    < ID3D11Texture2D >     pBackBuffer           = nullptr;

  ID3D11Buffer*             pVB                   = nullptr;
  ID3D11Buffer*             pIB                   = nullptr;

  int                       VertexBufferSize      = 5000,
                            IndexBufferSize       = 10000;
} _Frame [_MAX_BACKBUFFERS];

static UINT                     g_frameIndex            = 0;//UINT_MAX;
static UINT                     g_numFramesInSwapChain  = 1;
static UINT                     g_frameBufferWidth      = 0UL;
static UINT                     g_frameBufferHeight     = 0UL;

std::pair <BOOL*, BOOL>
SK_ImGui_FlagDrawing_OnD3D11Ctx (UINT dev_idx);

static void
ImGui_ImplDX11_CreateFontsTexture (void);

ID3D11RenderTargetView*
SK_D3D11_GetHDRHUDView (void)
{
  //if (g_pHDRHUDView == nullptr)
  //{
  //  bool
  //  ImGui_ImplDX11_CreateDeviceObjects (void);
  //  ImGui_ImplDX11_CreateDeviceObjects ();
  //}
  //
  //if (g_pHDRHUDView != nullptr)
  //{
  //  SK_RenderBackend& rb =
  //    SK_GetCurrentRenderBackend ();
  //
  //  return g_pHDRHUDView;
  //}
  //
  //SK_ReleaseAssert (false)

  // Uh oh, user wanted an update but it was not possible
  //   now they get nothing!
  return nullptr;
}

extern ID3D11ShaderResourceView*
SK_D3D11_GetHDRHUDTexture (void)
{
  //if (g_pHDRHUDTexView == nullptr)
  //{
  //  bool
  //  ImGui_ImplDX11_CreateDeviceObjects (void);
  //  ImGui_ImplDX11_CreateDeviceObjects ();
  //}
  //
  //if (g_pHDRHUDTexView != nullptr)
  //{
  //  return g_pHDRHUDTexView;
  //}
  //
  //SK_ReleaseAssert (false)

  return nullptr;
}


struct VERTEX_CONSTANT_BUFFER
{
  float mvp [4][4];

  // scRGB allows values > 1.0, sRGB (SDR) simply clamps them
  float luminance_scale [4]; // For HDR displays,    1.0 = 80 Nits
                             // For SDR displays, >= 1.0 = 80 Nits
  float steam_luminance [4];
};


struct HISTOGRAM_DISPATCH_CBUFFER
{
  uint32_t imageWidth;
  uint32_t imageHeight;

  float    minLuminance;
  float    maxLuminance;

  uint32_t numLocalZones;

  float    RGB_to_xyY [4][4];
};


extern void
SK_ImGui_LoadFonts (void);

#include <SpecialK/tls.h>

// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
IMGUI_API
void
ImGui_ImplDX11_RenderDrawData (ImDrawData* draw_data)
{
  ImGuiIO& io =
    ImGui::GetIO ();

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.swapchain)
    return;

  if (! rb.device)
    return;

  if (! rb.d3d11.immediate_ctx)
    return;

  //if (! rb.d3d11.deferred_ctx)
  //  return;

  SK_ComQIPtr <ID3D11Device>        pDevice    (      rb.device       );
  SK_ComQIPtr <ID3D11DeviceContext> pDevCtx    (rb.d3d11.immediate_ctx);
  SK_ComQIPtr <IDXGISwapChain>      pSwapChain (   rb.swapchain       );
  SK_ComQIPtr <IDXGISwapChain3>     pSwap3     (   rb.swapchain       );

  UINT currentBuffer =
   (g_frameIndex % g_numFramesInSwapChain);

  auto&& _P =
    _Frame [currentBuffer];

  if (! _P.pVertexConstantBuffer)
    return;

  if (! _P.pPixelConstantBuffer)
    return;

  SK_ComPtr <ID3D11Texture2D> pBackBuffer = nullptr;

  if ( FAILED (
         pSwapChain->GetBuffer (currentBuffer, IID_PPV_ARGS (&pBackBuffer.p) )
              )
     ) return;

  SK_TLS* pTLS =
    SK_TLS_Bottom ();

  // This thread and all commands coming from it are currently related to the
  //   UI, we don't want to track the majority of our own state changes
  //     (and more importantly, resources created on any D3D11 device).
  SK_ScopedBool auto_bool (&pTLS->imgui->drawing);
                            pTLS->imgui->drawing = true;

  // Flag the active device command context (not the calling thread) as currently
  //   participating in UI rendering.
  auto flag_result =
    SK_ImGui_FlagDrawing_OnD3D11Ctx (
      SK_D3D11_GetDeviceContextHandle (pDevCtx)
    );

  SK_ScopedBool auto_bool0 (flag_result.first);
                           *flag_result.first = flag_result.second;

  D3D11_TEXTURE2D_DESC   backbuffer_desc = { };
  pBackBuffer->GetDesc (&backbuffer_desc);

  io.DisplaySize.x             = static_cast <float> (backbuffer_desc.Width);
  io.DisplaySize.y             = static_cast <float> (backbuffer_desc.Height);

  io.DisplayFramebufferScale.x = static_cast <float> (backbuffer_desc.Width);
  io.DisplayFramebufferScale.y = static_cast <float> (backbuffer_desc.Height);

  // Create and grow vertex/index buffers if needed
  if ((! _P.pVB             ) ||
         _P.VertexBufferSize < draw_data->TotalVtxCount)
  {
    SK_COM_ValidateRelease ((IUnknown **)&_P.pVB);

    _P.VertexBufferSize =
      draw_data->TotalVtxCount + 5000;

    D3D11_BUFFER_DESC desc
                        = { };

    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth      = _P.VertexBufferSize * sizeof (ImDrawVert);
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags      = 0;

    if (pDevice->CreateBuffer (&desc, nullptr, &_P.pVB) < 0 ||
                                             (! _P.pVB))
      return;
  }

  if ((! _P.pIB            ) ||
         _P.IndexBufferSize < draw_data->TotalIdxCount)
  {
    SK_COM_ValidateRelease ((IUnknown **)&_P.pIB);

    _P.IndexBufferSize =
      draw_data->TotalIdxCount + 10000;

    D3D11_BUFFER_DESC desc
                        = { };

    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth      = _P.IndexBufferSize * sizeof (ImDrawIdx);
    desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (pDevice->CreateBuffer (&desc, nullptr, &_P.pIB) < 0 ||
                                             (! _P.pIB))
      return;
  }

  // Copy and convert all vertices into a single contiguous buffer
  D3D11_MAPPED_SUBRESOURCE vtx_resource = { },
                           idx_resource = { };

  if (pDevCtx->Map (_P.pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
    return;

  if (pDevCtx->Map (_P.pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
  {
    // If for some reason the first one succeeded, but this one failed.... unmap the first one
    //   then abandon all hope.
    pDevCtx->Unmap (_P.pVB, 0);
    return;
  }

  auto* vtx_dst = static_cast <ImDrawVert *> (vtx_resource.pData);
  auto* idx_dst = static_cast <ImDrawIdx  *> (idx_resource.pData);

  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list =
      draw_data->CmdLists [n];

    if (config.imgui.render.disable_alpha)
    {
      for (INT i = 0; i < cmd_list->VtxBuffer.Size; i++)
      {
        ImU32 color =
          ImColor (cmd_list->VtxBuffer.Data [i].col);

        uint8_t alpha = (((color & 0xFF000000U) >> 24U) & 0xFFU);

        // Boost alpha for visibility
        if (alpha < 93 && alpha != 0)
          alpha += (93  - alpha) / 2;

        float a = ((float)                       alpha / 255.0f);
        float r = ((float)((color & 0xFF0000U) >> 16U) / 255.0f);
        float g = ((float)((color & 0x00FF00U) >>  8U) / 255.0f);
        float b = ((float)((color & 0x0000FFU)       ) / 255.0f);

        color =                    0xFF000000U  |
                ((UINT)((r * a) * 255U) << 16U) |
                ((UINT)((g * a) * 255U) <<  8U) |
                ((UINT)((b * a) * 255U)       );

        cmd_list->VtxBuffer.Data[i].col =
          (ImVec4)ImColor (color);
      }
    }

    memcpy (idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof (ImDrawIdx));
    memcpy (vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof (ImDrawVert));

    vtx_dst += cmd_list->VtxBuffer.Size;
    idx_dst += cmd_list->IdxBuffer.Size;
  }

  pDevCtx->Unmap (_P.pIB, 0);
  pDevCtx->Unmap (_P.pVB, 0);

  // Setup orthographic projection matrix into our constant buffer
  {
    float L = 0.0f;
    float R = io.DisplaySize.x;
    float B = io.DisplaySize.y;
    float T = 0.0f;

    alignas (__m128d) float mvp [4][4] =
    {
      { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
      { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
      { 0.0f,         0.0f,           0.5f,       0.0f },
      { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };

    D3D11_MAPPED_SUBRESOURCE mapped_resource;

    if (! _P.pVertexConstantBuffer)
      return ImGui_ImplDX11_InvalidateDeviceObjects ();

    if (pDevCtx->Map (_P.pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
      return;

    auto* constant_buffer =
      static_cast <VERTEX_CONSTANT_BUFFER *> (mapped_resource.pData);

    memcpy         (&constant_buffer->mvp, mvp, sizeof (mvp));

    static bool hdr_display = false;

    hdr_display =
          hdr_display ||
      (rb.isHDRCapable () && (rb.framebuffer_flags & SK_FRAMEBUFFER_FLAG_HDR));

    SK_ReleaseAssert (hdr_display ==
                      (rb.isHDRCapable () && (rb.framebuffer_flags & SK_FRAMEBUFFER_FLAG_HDR))
    );

    if (! hdr_display)
    {
      constant_buffer->luminance_scale [0] = 1.0f; constant_buffer->luminance_scale [1] = 1.0f;
      constant_buffer->luminance_scale [2] = 0.0f; constant_buffer->luminance_scale [3] = 0.0f;
      constant_buffer->steam_luminance [0] = 1.0f; constant_buffer->steam_luminance [1] = 1.0f;
      constant_buffer->steam_luminance [2] = 1.0f; constant_buffer->steam_luminance [3] = 1.0f;
    }

    else
    {
      extern float __SK_HDR_Luma;
      extern float __SK_HDR_Exp;

      float luma = 0.0f,
            exp  = 0.0f;

      luma = __SK_HDR_Luma;
      exp  = __SK_HDR_Exp;

      SK_RenderBackend::scan_out_s::SK_HDR_TRANSFER_FUNC eotf =
        rb.scanout.getEOTF ();

      bool bEOTF_is_PQ =
        (eotf == SK_RenderBackend::scan_out_s::SMPTE_2084);

      constant_buffer->luminance_scale [0] = ( bEOTF_is_PQ ? -80.0f * rb.ui_luminance :
                                                                      rb.ui_luminance );
      constant_buffer->luminance_scale [1] = 2.2f;
      constant_buffer->luminance_scale [2] = ( bEOTF_is_PQ ? 1.0f : luma );
      constant_buffer->luminance_scale [3] = ( bEOTF_is_PQ ? 1.0f : exp  );
      constant_buffer->steam_luminance [0] = ( bEOTF_is_PQ ? -80.0f * config.steam.overlay_hdr_luminance :
                                                                      config.steam.overlay_hdr_luminance );
      constant_buffer->steam_luminance [1] = 2.2f;//( bEOTF_is_PQ ? 1.0f : (rb.ui_srgb ? 2.2f :
                                                  //                                     1.0f));
      constant_buffer->steam_luminance [2] = ( bEOTF_is_PQ ? -80.0f * config.uplay.overlay_luminance :
                                                                      config.uplay.overlay_luminance );
      constant_buffer->steam_luminance [3] = 2.2f;//( bEOTF_is_PQ ? 1.0f : (rb.ui_srgb ? 2.2f :
                                                  //                1.0f));
    }

    pDevCtx->Unmap (_P.pVertexConstantBuffer, 0);

    if (pDevCtx->Map (_P.pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
      return;

    ((float *)mapped_resource.pData)[2] = hdr_display ? (float)backbuffer_desc.Width  : 0.0f;
    ((float *)mapped_resource.pData)[3] = hdr_display ? (float)backbuffer_desc.Height : 0.0f;

    pDevCtx->Unmap (_P.pPixelConstantBuffer, 0);
  }


  SK_ComPtr <ID3D11RenderTargetView> pRenderTargetView = nullptr;

  D3D11_TEXTURE2D_DESC   tex2d_desc = { };
  pBackBuffer->GetDesc (&tex2d_desc);

  // SRGB Correction for UIs
  switch (tex2d_desc.Format)
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
                           = { };

      rtdesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView.p );
    } break;

    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
                           = { };

      rtdesc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView.p );
    } break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
                           = { };

      rtdesc.Format        = DXGI_FORMAT_R16G16B16A16_FLOAT;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView.p );
    } break;

    case DXGI_FORMAT_R10G10B10A2_UNORM:
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtdesc
        = { };

      rtdesc.Format        = DXGI_FORMAT_R10G10B10A2_UNORM;
      rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      pDevice->CreateRenderTargetView (pBackBuffer, &rtdesc, &pRenderTargetView.p );
    } break;

    default:
     pDevice->CreateRenderTargetView (pBackBuffer, nullptr, &pRenderTargetView.p );
  }

  if (! pRenderTargetView)
    return;

  SK_IMGUI_D3D11StateBlock
    sb             = { };
    sb.Capture (pDevCtx);

  // pcmd->TextureId may not point to a valid object anymore, so we do this...
  auto orig_se =
    SK_SEH_ApplyTranslator(
      SK_FilteringStructuredExceptionTranslator(
        EXCEPTION_ACCESS_VIOLATION
      )
    );
  try
  {
  pDevCtx->OMSetRenderTargets ( 1,
                                 &pRenderTargetView,
                                    nullptr );

  // Setup viewport
  D3D11_VIEWPORT vp = { };

  vp.Height   = io.DisplaySize.y;
  vp.Width    = io.DisplaySize.x;
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  vp.TopLeftX = vp.TopLeftY = 0.0f;

  pDevCtx->RSSetViewports (1, &vp);

  // Bind shader and vertex buffers
  unsigned int stride = sizeof (ImDrawVert);
  unsigned int offset = 0;

  pDevCtx->IASetInputLayout       (_P.pInputLayout);
  pDevCtx->IASetVertexBuffers     (0, 1, &_P.pVB, &stride, &offset);
  pDevCtx->IASetIndexBuffer       (_P.pIB, sizeof (ImDrawIdx) == 2 ?
                                             DXGI_FORMAT_R16_UINT  :
                                             DXGI_FORMAT_R32_UINT,
                                               0 );
  pDevCtx->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  pDevCtx->GSSetShader            (nullptr, nullptr, 0);
  pDevCtx->HSSetShader            (nullptr, nullptr, 0);
  pDevCtx->DSSetShader            (nullptr, nullptr, 0);

  pDevCtx->VSSetShader            (_P.pVertexShader, nullptr, 0);
  pDevCtx->VSSetConstantBuffers   (0, 1, &_P.pVertexConstantBuffer);


  pDevCtx->PSSetShader            (_P.pPixelShader, nullptr, 0);
  pDevCtx->PSSetSamplers          (0, 1, &_P.pFontSampler_clamp);
  pDevCtx->PSSetConstantBuffers   (0, 1, &_P.pPixelConstantBuffer);

  // Setup render state
  const float blend_factor [4] = { 0.f, 0.f,
                                   0.f, 1.f };

  pDevCtx->OMSetBlendState        (_P.pBlendState, blend_factor, 0xffffffff);
  pDevCtx->OMSetDepthStencilState (_P.pDepthStencilState,        0);
  pDevCtx->RSSetState             (_P.pRasterizerState);

  // Render command lists
  int vtx_offset = 0;
  int idx_offset = 0;

  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list =
      draw_data->CmdLists [n];

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
    {
      const ImDrawCmd* pcmd =
        &cmd_list->CmdBuffer [cmd_i];

      if (pcmd->UserCallback)
          pcmd->UserCallback (cmd_list, pcmd);

      else
      {
        const D3D11_RECT r = {
          static_cast <LONG> (pcmd->ClipRect.x), static_cast <LONG> (pcmd->ClipRect.y),
          static_cast <LONG> (pcmd->ClipRect.z), static_cast <LONG> (pcmd->ClipRect.w)
        };

        extern ID3D11ShaderResourceView*
          SK_HDR_GetUnderlayResourceView (void);

        ID3D11ShaderResourceView* views [2] =
        {
           *(ID3D11ShaderResourceView **)&pcmd->TextureId,
           SK_HDR_GetUnderlayResourceView ()
        };

        pDevCtx->PSSetSamplers        (0, 1, &pTLS->d3d11->uiSampler_wrap);
        pDevCtx->PSSetShaderResources (0, 2, views);
        pDevCtx->RSSetScissorRects    (1, &r);

        pDevCtx->DrawIndexed          (pcmd->ElemCount, idx_offset, vtx_offset);
      }

      pDevCtx->PSSetShaderResources   (0, 0, nullptr);

      idx_offset += pcmd->ElemCount;
    }

    vtx_offset += cmd_list->VtxBuffer.Size;
  }

  // Last-ditch effort to get the HDR post-process done before the UI.
  void SK_HDR_SnapshotSwapchain (void);
       SK_HDR_SnapshotSwapchain (    );

  pDevCtx->OMSetRenderTargets ( 0,
                          nullptr, nullptr );
  }
  catch (const SK_SEH_IgnoredException&)
  {
    sb.Apply (pDevCtx);
  }
  SK_SEH_RemoveTranslator (orig_se);

#if 0
  __SK_ImGui_D3D11_DrawDeferred = false;
  if (__SK_ImGui_D3D11_DrawDeferred)
  {
    ComPtr <ID3D11CommandList> pCmdList = nullptr;

    if ( SUCCEEDED (
           pDevCtx->FinishCommandList ( TRUE,
                                          &pCmdList.Get () )
                   )
       )
    {
      rb.d3d11.immediate_ctx->ExecuteCommandList (
        pCmdList.p, TRUE
      );
    }

    else
      pDevCtx->Flush ();
  }
#endif
}

#include <SpecialK/config.h>

static void
ImGui_ImplDX11_CreateFontsTexture (void)
{
  static auto& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComQIPtr <ID3D11Device> pDev (rb.device);

  if (! pDev.p) // Try again later
    return;

  SK_TLS* pTLS =
    SK_TLS_Bottom ();

  if (! pTLS) return;

  auto _BuildForSlot = [&](UINT slot) -> void
  {
  // Do not dump ImGui font textures
  SK_ScopedBool auto_bool (&pTLS->imgui->drawing);
                            pTLS->imgui->drawing = true;

  SK_ScopedBool decl_tex_scope (
    SK_D3D11_DeclareTexInjectScope (pTLS)
  );

  // Build texture atlas
  ImGuiIO& io (
    ImGui::GetIO ()
  );

  static bool           init   = false;
  static unsigned char* pixels = nullptr;
  static int            width  = 0,
                        height = 0;

  // Only needs to be done once, the raw pixels are API agnostic
  if (! init)
  {
    SK_ImGui_LoadFonts ();

    io.Fonts->GetTexDataAsRGBA32 ( &pixels,
                                   &width, &height );
  }

  auto&& _P =
    _Frame [slot];

  // Upload texture to graphics system
  {
    D3D11_TEXTURE2D_DESC
      desc                                = { };
      desc.Width                          = width;
      desc.Height                         = height;
      desc.MipLevels                      = 1;
      desc.ArraySize                      = 1;
      desc.Format                         = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.SampleDesc.Count               = 1;
      desc.Usage                          = D3D11_USAGE_IMMUTABLE;
      desc.BindFlags                      = D3D11_BIND_SHADER_RESOURCE;
      desc.CPUAccessFlags                 = 0;

    D3D11_SUBRESOURCE_DATA
      subResource                         = { };
      subResource.pSysMem                 = pixels;
      subResource.SysMemPitch             = desc.Width * 4;
      subResource.SysMemSlicePitch        = 0;

    SK_ComPtr <ID3D11Texture2D>
                      pFontTexture = nullptr;

    if ( SUCCEEDED (
           pDev->CreateTexture2D ( &desc,
                                     &subResource,
                                       &pFontTexture.p ) ) )
    {
      SK_D3D11_SetDebugName (pFontTexture, "ImGui Font Texture");

      // Create texture view
      D3D11_SHADER_RESOURCE_VIEW_DESC
        srvDesc = { };
        srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;

      if ( SUCCEEDED (
             pDev->CreateShaderResourceView ( pFontTexture,
                                                &srvDesc,
                                                  &_P.pFontTextureView ) ) )
      {
        SK_D3D11_SetDebugName (_P.pFontTextureView, "ImGui Font SRV");

        // Store our identifier
        io.Fonts->TexID =
          _P.pFontTextureView;

        // Create texture sampler
        D3D11_SAMPLER_DESC
          sampler_desc                    = { };
          sampler_desc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
          sampler_desc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
          sampler_desc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
          sampler_desc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
          sampler_desc.MipLODBias         = 0.f;
          sampler_desc.ComparisonFunc     = D3D11_COMPARISON_ALWAYS;
          sampler_desc.MinLOD             = 0.f;
          sampler_desc.MaxLOD             = 0.f;

        if ( SUCCEEDED (
               pDev->CreateSamplerState ( &sampler_desc,
                                            &_P.pFontSampler_clamp ) ) )
        { pTLS->d3d11->uiSampler_clamp =     _P.pFontSampler_clamp;

          sampler_desc = { };

          sampler_desc.Filter             = D3D11_FILTER_ANISOTROPIC;
          sampler_desc.AddressU           = D3D11_TEXTURE_ADDRESS_MIRROR;
          sampler_desc.AddressV           = D3D11_TEXTURE_ADDRESS_MIRROR;
          sampler_desc.AddressW           = D3D11_TEXTURE_ADDRESS_MIRROR;
          sampler_desc.MipLODBias         = 0.f;
          sampler_desc.ComparisonFunc     = D3D11_COMPARISON_ALWAYS;
          sampler_desc.MinLOD             = 0.f;
          sampler_desc.MaxLOD             = 0.f;

          init = true;

          if ( SUCCEEDED (
                 pDev->CreateSamplerState ( &sampler_desc,
                                              &_P.pFontSampler_wrap ) ) )
          { pTLS->d3d11->uiSampler_wrap =      _P.pFontSampler_wrap; }
        }
      }
    }
  }
  };

  for ( auto i = 0 ; i < _MAX_BACKBUFFERS ; ++i )
  {
    _BuildForSlot (i);
  }
}

HRESULT
SK_D3D11_Inject_uPlayHDR ( _In_ ID3D11DeviceContext  *pDevCtx,
                           _In_ UINT                  IndexCount,
                           _In_ UINT                  StartIndexLocation,
                           _In_ INT                   BaseVertexLocation,
                           _In_ D3D11_DrawIndexed_pfn pfnD3D11DrawIndexed )
{
  auto&& _P =
    _Frame [g_frameIndex % g_numFramesInSwapChain];

  if ( _P.pVertexShaderSteamHDR != nullptr &&
       _P.pVertexConstantBuffer != nullptr &&
       _P.pPixelShaderSteamHDR  != nullptr    )
  {
    auto flag_result =
      SK_ImGui_FlagDrawing_OnD3D11Ctx (
        SK_D3D11_GetDeviceContextHandle (pDevCtx)
      );

    SK_ScopedBool auto_bool0 (flag_result.first);
                             *flag_result.first = flag_result.second;

    // Seriously, D3D, WTF? Let me first query the number of these and then decide
    //   whether I want to dedicate TLS or heap memory to this task.
    UINT NumStackDestroyingInstances_ProbablyZero0 = 0,
         NumStackDestroyingInstances_ProbablyZero1 = 0;

    ID3D11ClassInstance *GiantListThatDestroysTheStack0 [253] = { };
    ID3D11ClassInstance *GiantListThatDestroysTheStack1 [253] = { };

    SK_ComPtr <ID3D11PixelShader>  pOrigPixShader;
    SK_ComPtr <ID3D11VertexShader> pOrigVtxShader;
    SK_ComPtr <ID3D11Buffer>       pOrigVtxCB;

    pDevCtx->VSGetShader          ( &pOrigVtxShader.p,
                                     GiantListThatDestroysTheStack0,
                                   &NumStackDestroyingInstances_ProbablyZero0 );
    pDevCtx->PSGetShader          ( &pOrigPixShader.p,
                                   GiantListThatDestroysTheStack1,
                                   &NumStackDestroyingInstances_ProbablyZero1 );

    pDevCtx->VSGetConstantBuffers ( 0, 1, &pOrigVtxCB );
    pDevCtx->VSSetShader          ( _P.pVertexShaderuPlayHDR,
                                      nullptr, 0 );
    pDevCtx->PSSetShader          ( _P.pPixelShaderuPlayHDR,
                                      nullptr, 0 );
    pDevCtx->VSSetConstantBuffers ( 0, 1,
                                    &_P.pVertexConstantBuffer );
    pfnD3D11DrawIndexed ( pDevCtx, IndexCount, StartIndexLocation, BaseVertexLocation );
    pDevCtx->VSSetConstantBuffers (0, 1, &pOrigVtxCB);

    pDevCtx->PSSetShader ( pOrigPixShader,
                           GiantListThatDestroysTheStack1,
                           NumStackDestroyingInstances_ProbablyZero1 );

    pDevCtx->VSSetShader ( pOrigVtxShader,
                           GiantListThatDestroysTheStack0,
                           NumStackDestroyingInstances_ProbablyZero0 );

    for ( UINT i = 0 ; i < NumStackDestroyingInstances_ProbablyZero0 ; ++i )
    {
      if (GiantListThatDestroysTheStack0 [i] != nullptr)
          GiantListThatDestroysTheStack0 [i]->Release ();
    }

    for ( UINT j = 0 ; j < NumStackDestroyingInstances_ProbablyZero1 ; ++j )
    {
      if (GiantListThatDestroysTheStack1 [j] != nullptr)
          GiantListThatDestroysTheStack1 [j]->Release ();
    }

    return
      S_OK;
  }

  return
    E_NOT_VALID_STATE;
}

HRESULT
SK_D3D11_InjectSteamHDR ( _In_ ID3D11DeviceContext *pDevCtx,
                          _In_ UINT                 VertexCount,
                          _In_ UINT                 StartVertexLocation,
                          _In_ D3D11_Draw_pfn       pfnD3D11Draw )
{
  auto&& _P =
    _Frame [g_frameIndex % g_numFramesInSwapChain];

  auto flag_result =
    SK_ImGui_FlagDrawing_OnD3D11Ctx (
      SK_D3D11_GetDeviceContextHandle (pDevCtx)
    );

  SK_ScopedBool auto_bool0 (flag_result.first);
                           *flag_result.first = flag_result.second;

  if ( _P.pVertexShaderSteamHDR != nullptr &&
       _P.pVertexConstantBuffer != nullptr &&
       _P.pPixelShaderSteamHDR  != nullptr    )
  {
    // Seriously, D3D, WTF? Let me first query the number of these and then decide
    //   whether I want to dedicate TLS or heap memory to this task.
    UINT NumStackDestroyingInstances_ProbablyZero0 = 0,
         NumStackDestroyingInstances_ProbablyZero1 = 0;

    ID3D11ClassInstance *GiantListThatDestroysTheStack0 [253] = { };
    ID3D11ClassInstance *GiantListThatDestroysTheStack1 [253] = { };

    SK_ComPtr <ID3D11PixelShader>  pOrigPixShader;
    SK_ComPtr <ID3D11VertexShader> pOrigVtxShader;
    SK_ComPtr <ID3D11Buffer>       pOrigVtxCB;

    pDevCtx->VSGetShader          ( &pOrigVtxShader.p,
                                     GiantListThatDestroysTheStack0,
                                   &NumStackDestroyingInstances_ProbablyZero0 );
    pDevCtx->PSGetShader          ( &pOrigPixShader.p,
                                   GiantListThatDestroysTheStack1,
                                   &NumStackDestroyingInstances_ProbablyZero1 );

    pDevCtx->VSGetConstantBuffers ( 0, 1, &pOrigVtxCB );
    pDevCtx->VSSetShader          ( _P.pVertexShaderSteamHDR,
                                      nullptr, 0 );
    pDevCtx->PSSetShader          ( _P.pPixelShaderSteamHDR,
                                      nullptr, 0 );
    pDevCtx->VSSetConstantBuffers ( 0, 1,
                                    &_P.pVertexConstantBuffer );
    pfnD3D11Draw ( pDevCtx, VertexCount, StartVertexLocation );
    pDevCtx->VSSetConstantBuffers (0, 1, &pOrigVtxCB);

    pDevCtx->PSSetShader ( pOrigPixShader,
                           GiantListThatDestroysTheStack1,
                           NumStackDestroyingInstances_ProbablyZero1 );

    pDevCtx->VSSetShader ( pOrigVtxShader,
                           GiantListThatDestroysTheStack0,
                           NumStackDestroyingInstances_ProbablyZero0 );

    for ( UINT i = 0 ; i < NumStackDestroyingInstances_ProbablyZero0 ; ++i )
    {
      if (GiantListThatDestroysTheStack0 [i] != nullptr)
          GiantListThatDestroysTheStack0 [i]->Release ();
    }

    for ( UINT j = 0 ; j < NumStackDestroyingInstances_ProbablyZero1 ; ++j )
    {
      if (GiantListThatDestroysTheStack1 [j] != nullptr)
          GiantListThatDestroysTheStack1 [j]->Release ();
    }
    //SK_IMGUI_D3D11StateBlock
    //  sb;
    //  sb.Capture (pDevCtx, SK_IMGUI_D3D11StateBlock::VertexStage |
    //                       SK_IMGUI_D3D11StateBlock::PixelStage);
    //
    //pDevCtx->VSSetShader          ( _P.pVertexShaderSteamHDR,
    //                                  nullptr, 0 );
    //pDevCtx->PSSetShader          ( _P.pPixelShaderSteamHDR,
    //                                  nullptr, 0 );
    //pDevCtx->VSSetConstantBuffers ( 0, 1,
    //                                &_P.pVertexConstantBuffer );
    //
    //pfnD3D11Draw ( pDevCtx, VertexCount, StartVertexLocation );
    //
    //sb.Apply (pDevCtx, SK_IMGUI_D3D11StateBlock::VertexStage |
    //                   SK_IMGUI_D3D11StateBlock::PixelStage);

    return
      S_OK;
  }

  return
    E_NOT_VALID_STATE;
}

bool
ImGui_ImplDX11_CreateDeviceObjectsForBackbuffer (UINT idx)
{
  auto&& _P =
    _Frame [idx];

  SK_TLS* pTLS =
    SK_TLS_Bottom ();

  if (! pTLS) return false;

  SK_ScopedBool auto_bool (&pTLS->imgui->drawing);

  // Do not dump ImGui font textures
  pTLS->imgui->drawing = true;

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  SK_ComQIPtr <ID3D11Device> pDev (rb.device);

  ///if (rb.api != SK_RenderAPI::D3D11On12)
  ///{
    if (! rb.device)
      return false;

    if (! rb.d3d11.immediate_ctx)
      return false;

    if (! rb.swapchain)
      return false;
  ///}

  ///if (rb.api == SK_RenderAPI::D3D11On12)
  ///{
  ///  rb.d3d11.wrapper_dev->QueryInterface <ID3D11Device> (&pDev.p);
  ///
  ///                              rb.d3d11.immediate_ctx = nullptr;
  ///  pDev->GetImmediateContext (&rb.d3d11.immediate_ctx.p);
  ///}

  auto flag_result =
    SK_ImGui_FlagDrawing_OnD3D11Ctx (
      SK_D3D11_GetDeviceContextHandle (rb.d3d11.immediate_ctx)
    );

  SK_ScopedBool auto_bool0 (flag_result.first);
                           *flag_result.first = flag_result.second;

  if (! pDev)
    return false;

  if ( pDev->CreateVertexShader ( (DWORD *)(imgui_d3d11_vs_bytecode),
                                    sizeof (imgui_d3d11_vs_bytecode) /
                                    sizeof (imgui_d3d11_vs_bytecode [0]),
                                      nullptr,
                                        &_P.pVertexShader ) != S_OK )
    return false;

  SK_D3D11_SetDebugName (_P.pVertexShader, "ImGui Vertex Shader");

  if ( pDev->CreateVertexShader ( (DWORD *)(steam_d3d11_vs_bytecode),
                                    sizeof (steam_d3d11_vs_bytecode) /
                                    sizeof (steam_d3d11_vs_bytecode [0]),
                                      nullptr,
                                        &_P.pVertexShaderSteamHDR ) != S_OK )
    return false;

  SK_D3D11_SetDebugName (_P.pVertexShaderSteamHDR, "Steam Overlay HDR Vertex Shader");

  if ( pDev->CreateVertexShader ( (DWORD *)(uplay_d3d11_vs_bytecode),
                                    sizeof (uplay_d3d11_vs_bytecode) /
                                    sizeof (uplay_d3d11_vs_bytecode [0]),
                                      nullptr,
                                        &_P.pVertexShaderuPlayHDR ) != S_OK )
    return false;

  SK_D3D11_SetDebugName (_P.pVertexShaderuPlayHDR, "uPlay Overlay HDR Vertex Shader");

  // Create the input layout
  D3D11_INPUT_ELEMENT_DESC local_layout [] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (size_t)(&((ImDrawVert *)nullptr)->pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (size_t)(&((ImDrawVert *)nullptr)->uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (size_t)(&((ImDrawVert *)nullptr)->col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  if ( pDev->CreateInputLayout ( local_layout, 3,
                                  (DWORD *)(imgui_d3d11_vs_bytecode),
                                    sizeof (imgui_d3d11_vs_bytecode) /
                                    sizeof (imgui_d3d11_vs_bytecode [0]),
                                      &_P.pInputLayout ) != S_OK )
  {
    return false;
  }

  // Create the constant buffers
  {
    D3D11_BUFFER_DESC desc = { };

    desc.ByteWidth         = sizeof (VERTEX_CONSTANT_BUFFER);
    desc.Usage             = D3D11_USAGE_DYNAMIC;
    desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags         = 0;

    pDev->CreateBuffer (&desc, nullptr, &_P.pVertexConstantBuffer);

    desc.ByteWidth         = sizeof (float) * 4;
    desc.Usage             = D3D11_USAGE_DYNAMIC;
    desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags         = 0;

    pDev->CreateBuffer (&desc, nullptr, &_P.pPixelConstantBuffer);

    SK_D3D11_SetDebugName (_P.pVertexConstantBuffer, "ImGui Vertex Constant Buffer");
    SK_D3D11_SetDebugName (_P.pPixelConstantBuffer,  "ImGui Pixel Constant Buffer");
  }

  if ( pDev->CreatePixelShader ( (DWORD *) (imgui_d3d11_ps_bytecode),
                                    sizeof (imgui_d3d11_ps_bytecode) /
                                    sizeof (imgui_d3d11_ps_bytecode [0]),
                                      nullptr,
                                        &_P.pPixelShader ) != S_OK )
  {
    return false;
  }

  SK_D3D11_SetDebugName (_P.pPixelShader, "ImGui Pixel Shader");

  if ( pDev->CreatePixelShader ( (DWORD *) (steam_d3d11_ps_bytecode),
                                    sizeof (steam_d3d11_ps_bytecode) /
                                    sizeof (steam_d3d11_ps_bytecode [0]),
                                     nullptr,
                                       &_P.pPixelShaderSteamHDR ) != S_OK )
  {
    return false;
  }

  SK_D3D11_SetDebugName (_P.pPixelShaderSteamHDR, "Steam Overlay HDR Pixel Shader");

  if ( pDev->CreatePixelShader ( (DWORD *) (uplay_d3d11_ps_bytecode),
                                    sizeof (uplay_d3d11_ps_bytecode) /
                                    sizeof (uplay_d3d11_ps_bytecode [0]),
                                      nullptr,
                                        &_P.pPixelShaderuPlayHDR ) != S_OK )
  {
    return false;
  }

  SK_D3D11_SetDebugName (_P.pPixelShaderuPlayHDR, "uPlay Overlay HDR Pixel Shader");

  // Create the blending setup
  {
    D3D11_BLEND_DESC desc                       = {   };

    desc.AlphaToCoverageEnable                  = false;
    desc.RenderTarget [0].BlendEnable           =  true;
    desc.RenderTarget [0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget [0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget [0].BlendOp               = D3D11_BLEND_OP_ADD;
    desc.RenderTarget [0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    desc.RenderTarget [0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    desc.RenderTarget [0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    desc.RenderTarget [0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    pDev->CreateBlendState (&desc, &_P.pBlendState);
    SK_D3D11_SetDebugName  (_P.pBlendState, "ImGui Blend State");

    //desc.RenderTarget [0].BlendEnable           = true;
    //desc.RenderTarget [0].SrcBlend              = D3D11_BLEND_ONE;
    //desc.RenderTarget [0].DestBlend             = D3D11_BLEND_ONE;
    //desc.RenderTarget [0].BlendOp               = D3D11_BLEND_OP_ADD;
    //desc.RenderTarget [0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    //desc.RenderTarget [0].DestBlendAlpha        = D3D11_BLEND_ONE;
    //desc.RenderTarget [0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    //desc.IndependentBlendEnable                 = true;
  }

  // Create the rasterizer state
  {
    D3D11_RASTERIZER_DESC desc = { };

    desc.FillMode        = D3D11_FILL_SOLID;
    desc.CullMode        = D3D11_CULL_NONE;
    desc.ScissorEnable   = true;
    desc.DepthClipEnable = true;

    pDev->CreateRasterizerState (&desc, &_P.pRasterizerState);
    SK_D3D11_SetDebugName (_P.pRasterizerState, "ImGui Rasterizer State");
  }

  // Create depth-stencil State
  {
    D3D11_DEPTH_STENCIL_DESC desc = { };

    desc.DepthEnable              = false;
    desc.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc                = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable            = false;
    desc.FrontFace.StencilFailOp  = desc.FrontFace.StencilDepthFailOp =
                                    desc.FrontFace.StencilPassOp      =
                                    D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc    = D3D11_COMPARISON_ALWAYS;
    desc.BackFace                 = desc.FrontFace;

    pDev->CreateDepthStencilState (&desc, &_P.pDepthStencilState);
    SK_D3D11_SetDebugName (_P.pDepthStencilState, "ImGui Depth/Stencil State");
  }

  return true;
}

bool
ImGui_ImplDX11_CreateDeviceObjects (void)
{
  static auto& rb =
    SK_GetCurrentRenderBackend ();

  ImGui_ImplDX11_InvalidateDeviceObjects ();

  SK_ComQIPtr <IDXGISwapChain>
        pSwap   (rb.swapchain);
  if (! pSwap)
    return false;

  DXGI_SWAP_CHAIN_DESC
                   swapDesc = { };
  pSwap->GetDesc (&swapDesc);

  g_numFramesInSwapChain =
    _MAX_BACKBUFFERS;

  ImGui_ImplDX11_CreateFontsTexture ();

  for ( UINT i = 0 ; i < g_numFramesInSwapChain ; ++i )
  {
    if (! ImGui_ImplDX11_CreateDeviceObjectsForBackbuffer (i))
    {
      ImGui_ImplDX11_InvalidateDeviceObjects ();

      return false;
    }
  }

  return true;
}


using SK_ImGui_ResetCallback_pfn = void (__stdcall *)(void);

SK_LazyGlobal <std::vector <SK_ComPtr <IUnknown>>>       external_resources;
SK_LazyGlobal <std::set    <SK_ImGui_ResetCallback_pfn>> reset_callbacks;

__declspec (dllexport)
void
__stdcall
SKX_ImGui_RegisterResource (IUnknown* pRes)
{
  external_resources->push_back (pRes);
}


__declspec (dllexport)
void
__stdcall
SKX_ImGui_RegisterResetCallback (SK_ImGui_ResetCallback_pfn pCallback)
{
  reset_callbacks->emplace (pCallback);
}

__declspec (dllexport)
void
__stdcall
SKX_ImGui_UnregisterResetCallback (SK_ImGui_ResetCallback_pfn pCallback)
{
  if (reset_callbacks->count (pCallback))
      reset_callbacks->erase (pCallback);
}

void
SK_ImGui_ResetExternal (void)
{
  external_resources->clear ();

  for ( auto reset_fn : reset_callbacks.get () )
  {
    reset_fn ();
  }
}


void
ImGui_ImplDX11_InvalidateDeviceObjects (void)
{
  SK_TLS* pTLS =
    SK_TLS_Bottom ();

  SK_ImGui_ResetExternal ();

  auto _CleanupBackbuffer = [&](UINT index) -> void
  {
    auto&& _P = _Frame [index];

    _P.VertexBufferSize = 5000;
    _P.IndexBufferSize  = 10000;

    if (_P.pFontSampler_clamp)    { _P.pFontSampler_clamp->Release    (); _P.pFontSampler_clamp = nullptr; }
    if (_P.pFontSampler_wrap)     { _P.pFontSampler_wrap->Release     (); _P.pFontSampler_wrap  = nullptr; }
    if (_P.pFontTextureView)      { _P.pFontTextureView->Release      (); _P.pFontTextureView   = nullptr;  ImGui::GetIO ().Fonts->TexID = nullptr; }
    if (_P.pIB)                   { _P.pIB->Release                   ();              _P.pIB   = nullptr; }
    if (_P.pVB)                   { _P.pVB->Release                   ();              _P.pVB   = nullptr; }

    if (_P.pBlendState)           { _P.pBlendState->Release           ();           _P.pBlendState = nullptr; }
    if (_P.pDepthStencilState)    { _P.pDepthStencilState->Release    ();    _P.pDepthStencilState = nullptr; }
    if (_P.pRasterizerState)      { _P.pRasterizerState->Release      ();      _P.pRasterizerState = nullptr; }
    if (_P.pPixelShader)          { _P.pPixelShader->Release          ();          _P.pPixelShader = nullptr; }
    if (_P.pPixelShaderuPlayHDR)  { _P.pPixelShaderuPlayHDR->Release  (); _P.pPixelShaderuPlayHDR  = nullptr; }
    if (_P.pPixelShaderSteamHDR)  { _P.pPixelShaderSteamHDR->Release  (); _P.pPixelShaderSteamHDR  = nullptr; }
    if (_P.pVertexConstantBuffer) { _P.pVertexConstantBuffer->Release (); _P.pVertexConstantBuffer = nullptr; }
    if (_P.pPixelConstantBuffer)  { _P.pPixelConstantBuffer->Release  (); _P.pPixelConstantBuffer  = nullptr; }
    if (_P.pInputLayout)          { _P.pInputLayout->Release          ();          _P.pInputLayout = nullptr; }
    if (_P.pVertexShader)         { _P.pVertexShader->Release         ();         _P.pVertexShader = nullptr; }
    if (_P.pVertexShaderSteamHDR) { _P.pVertexShaderSteamHDR->Release (); _P.pVertexShaderSteamHDR = nullptr; }
    if (_P.pVertexShaderuPlayHDR) { _P.pVertexShaderuPlayHDR->Release (); _P.pVertexShaderuPlayHDR = nullptr; }

    _P.pBackBuffer = nullptr;
  };

  for ( UINT i = 0 ; i < _MAX_BACKBUFFERS ; ++i )
    _CleanupBackbuffer (i);

  pTLS->d3d11->uiSampler_clamp = nullptr;
  pTLS->d3d11->uiSampler_wrap  = nullptr;
}

bool
ImGui_ImplDX11_Init ( IDXGISwapChain* pSwapChain,
                      ID3D11Device*,
                      ID3D11DeviceContext* )
{
  static bool first = true;

  if (first)
  {
    g_TicksPerSecond  =
      SK_GetPerfFreq ( ).QuadPart;
    g_Time            =
      SK_QueryPerf   ( ).QuadPart;

    first = false;
  }

  ImGuiIO& io =
    ImGui::GetIO ();

  DXGI_SWAP_CHAIN_DESC      swap_desc = { };
  if (pSwapChain != nullptr)
  {   pSwapChain->GetDesc (&swap_desc); }

  g_numFramesInSwapChain =  _MAX_BACKBUFFERS;
//g_numFramesInSwapChain = swap_desc.BufferCount;
  g_frameBufferWidth     = swap_desc.BufferDesc.Width;
  g_frameBufferHeight    = swap_desc.BufferDesc.Height;
  g_hWnd                 = swap_desc.OutputWindow;
  io.ImeWindowHandle     = g_hWnd;

  io.KeyMap [ImGuiKey_Tab]        = VK_TAB;
  io.KeyMap [ImGuiKey_LeftArrow]  = VK_LEFT;
  io.KeyMap [ImGuiKey_RightArrow] = VK_RIGHT;
  io.KeyMap [ImGuiKey_UpArrow]    = VK_UP;
  io.KeyMap [ImGuiKey_DownArrow]  = VK_DOWN;
  io.KeyMap [ImGuiKey_PageUp]     = VK_PRIOR;
  io.KeyMap [ImGuiKey_PageDown]   = VK_NEXT;
  io.KeyMap [ImGuiKey_Home]       = VK_HOME;
  io.KeyMap [ImGuiKey_End]        = VK_END;
  io.KeyMap [ImGuiKey_Insert]     = VK_INSERT;
  io.KeyMap [ImGuiKey_Delete]     = VK_DELETE;
  io.KeyMap [ImGuiKey_Backspace]  = VK_BACK;
  io.KeyMap [ImGuiKey_Space]      = VK_SPACE;
  io.KeyMap [ImGuiKey_Enter]      = VK_RETURN;
  io.KeyMap [ImGuiKey_Escape]     = VK_ESCAPE;
  io.KeyMap [ImGuiKey_A]          = 'A';
  io.KeyMap [ImGuiKey_C]          = 'C';
  io.KeyMap [ImGuiKey_V]          = 'V';
  io.KeyMap [ImGuiKey_X]          = 'X';
  io.KeyMap [ImGuiKey_Y]          = 'Y';
  io.KeyMap [ImGuiKey_Z]          = 'Z';

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  return
    rb.device != nullptr;
}

void
ImGui_ImplDX11_Shutdown (void)
{
  ImGui_ImplDX11_InvalidateDeviceObjects ();
  ImGui::Shutdown                        ();
}

#include <SpecialK/window.h>

void
SK_ImGui_PollGamepad (void);

void
ImGui_ImplDX11_NewFrame (void)
{
  // Setup time step
  INT64 current_time;

  SK_QueryPerformanceCounter (
    reinterpret_cast <LARGE_INTEGER *> (&current_time)
  );

  auto& io =
    ImGui::GetIO ();

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.device)
    return;

  auto flag_result =
    SK_ImGui_FlagDrawing_OnD3D11Ctx (
      SK_D3D11_GetDeviceContextHandle (rb.d3d11.immediate_ctx)
    );

  SK_ScopedBool auto_bool0 (flag_result.first);
                           *flag_result.first = flag_result.second;

  auto&& _Pool =
    _Frame [g_frameIndex % g_numFramesInSwapChain];

  if (! _Pool.pFontSampler_clamp)
    ImGui_ImplDX11_CreateDeviceObjects ();

  if (io.Fonts->Fonts.empty ())
  {
    ImGui_ImplDX11_CreateFontsTexture ();

    if (io.Fonts->Fonts.empty ())
      return;
  }

  io.DeltaTime =
    std::min ( 1.0f,
    std::max ( 0.0f, static_cast <float> (
                    (static_cast <long double> (                       current_time) -
                     static_cast <long double> (std::exchange (g_Time, current_time))) /
                     static_cast <long double> (               g_TicksPerSecond      ) ) )
    );

  // Setup display size (every frame to accommodate for window resizing)
  //io.DisplaySize =
    //ImVec2 ( g_frameBufferWidth,
               //g_frameBufferHeight );

  // Read keyboard modifiers inputs
  io.KeyCtrl   = (io.KeysDown [VK_CONTROL]) != 0;
  io.KeyShift  = (io.KeysDown [VK_SHIFT])   != 0;
  io.KeyAlt    = (io.KeysDown [VK_MENU])    != 0;

  io.KeySuper  = false;

  // For games that hijack the mouse cursor using DirectInput 8.
  //
  //  -- Acquire actually means release their exclusive ownership : )
  //
  //if (SK_ImGui_WantMouseCapture ())
  //  SK_Input_DI8Mouse_Acquire ();
  //else
  //  SK_Input_DI8Mouse_Release ();


  // Update OS mouse cursor with the cursor requested by imgui
  //ImGuiMouseCursor mouse_cursor =
  //           io.MouseDrawCursor ? ImGuiMouseCursor_None  :
  //                                ImGui::GetMouseCursor ( );
  //
  //if (g_LastMouseCursor != mouse_cursor)
  //{
  //    g_LastMouseCursor = mouse_cursor;
  //    ImGui_ImplWin32_UpdateMouseCursor();
  //}

  SK_ImGui_PollGamepad ();

  //// Start the frame
  SK_ImGui_User_NewFrame ();
}

void
ImGui_ImplDX11_Resize ( IDXGISwapChain *This,
                        UINT            BufferCount,
                        UINT            Width,
                        UINT            Height,
                        DXGI_FORMAT     NewFormat,
                        UINT            SwapChainFlags )
{
  UNREFERENCED_PARAMETER (BufferCount);
  UNREFERENCED_PARAMETER (NewFormat);
  UNREFERENCED_PARAMETER (SwapChainFlags);
  UNREFERENCED_PARAMETER (Width);
  UNREFERENCED_PARAMETER (Height);
  UNREFERENCED_PARAMETER (This);

  static auto& rb =
    SK_GetCurrentRenderBackend ();

  if (! rb.device)
    return;

  SK_TLS *pTLS =
    SK_TLS_Bottom ();

  SK_ScopedBool auto_bool (&pTLS->imgui->drawing);

  // Do not dump ImGui font textures
  pTLS->imgui->drawing = true;

  assert (This == rb.swapchain);

  SK_ComPtr <ID3D11Device>        pDev    = nullptr;
  SK_ComPtr <ID3D11DeviceContext> pDevCtx = nullptr;

  if (rb.d3d11.immediate_ctx != nullptr)
  {
    HRESULT hr0 = rb.device->QueryInterface <ID3D11Device> (&pDev.p);
    HRESULT hr1 = rb.d3d11.immediate_ctx->QueryInterface
          <ID3D11DeviceContext>(&pDevCtx.p);

    auto flag_result =
      SK_ImGui_FlagDrawing_OnD3D11Ctx (
        SK_D3D11_GetDeviceContextHandle (pDevCtx)
      );

    SK_ScopedBool auto_bool0 (flag_result.first);
                             *flag_result.first = flag_result.second;

    if (SUCCEEDED (hr0) && SUCCEEDED (hr1))
    {
      ImGui_ImplDX11_InvalidateDeviceObjects ();
    }
  }
}