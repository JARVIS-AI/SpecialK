// ImGui Win32 + DirectX9 binding
// In this binding, ImTextureID is used to store a 'LPDIRECT3DTEXTURE9' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#define NOMINMAX

#include <imgui/imgui.h>
#include <imgui/backends/imgui_d3d9.h>

// DirectX
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

//#include "config.h"
//#include "render.h"

#include <SpecialK/window.h>

#include <atlbase.h>

// Data
static HWND                    g_hWnd             = 0;
static INT64                   g_Time             = 0;
static INT64                   g_TicksPerSecond   = 0;
       LPDIRECT3DDEVICE9       g_pd3dDevice       = nullptr;
static LPDIRECT3DVERTEXBUFFER9 g_pVB              = nullptr;
static LPDIRECT3DINDEXBUFFER9  g_pIB              = nullptr;
static LPDIRECT3DTEXTURE9      g_FontTexture      = nullptr;
static int                     g_VertexBufferSize = 5000,
                               g_IndexBufferSize  = 10000;

struct CUSTOMVERTEX
{
  float    pos [3];
  D3DCOLOR col;
  float    uv  [2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void
ImGui_ImplDX9_RenderDrawLists (ImDrawData* draw_data)
{
  // Avoid rendering when minimized
  ImGuiIO& io =
    ImGui::GetIO ();

  if ( io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f )
    return;

  // Create and grow buffers if needed
  if ((! g_pVB) || g_VertexBufferSize < draw_data->TotalVtxCount )
  {
    if (g_pVB) {
      g_pVB->Release ();
      g_pVB = nullptr;
    }

    g_VertexBufferSize =
      draw_data->TotalVtxCount + 5000;

    if ( g_pd3dDevice->CreateVertexBuffer ( g_VertexBufferSize * sizeof CUSTOMVERTEX,
                                              D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                                                D3DFVF_CUSTOMVERTEX,
                                                  D3DPOOL_DEFAULT,
                                                    &g_pVB,
                                                      nullptr ) < 0 ) {
      return;
    }
  }

  if ((! g_pIB) || g_IndexBufferSize < draw_data->TotalIdxCount)
  {
    if (g_pIB) {
      g_pIB->Release ();
      g_pIB = NULL;
    }

    g_IndexBufferSize = draw_data->TotalIdxCount + 10000;

    if ( g_pd3dDevice->CreateIndexBuffer ( g_IndexBufferSize * sizeof ImDrawIdx,
                                             D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                                               sizeof (ImDrawIdx) == 2 ?
                                                  D3DFMT_INDEX16 :
                                                  D3DFMT_INDEX32,
                                                D3DPOOL_DEFAULT,
                                                  &g_pIB,
                                                    nullptr ) < 0 ) {
      return;
    }
  }

  // Backup the DX9 state
  IDirect3DStateBlock9* d3d9_state_block = nullptr;

  if (g_pd3dDevice->CreateStateBlock ( D3DSBT_ALL, &d3d9_state_block ) < 0 )
    return;

  // Copy and convert all vertices into a single contiguous buffer
  CUSTOMVERTEX* vtx_dst;
  ImDrawIdx*    idx_dst;

  if ( g_pVB->Lock ( 0,
                       (UINT)(draw_data->TotalVtxCount * sizeof CUSTOMVERTEX),
                         (void **)&vtx_dst,
                           D3DLOCK_DISCARD ) < 0 )
    return;

  if ( g_pIB->Lock ( 0,
                       (UINT)(draw_data->TotalIdxCount * sizeof ImDrawIdx),
                         (void **)&idx_dst,
                           D3DLOCK_DISCARD ) < 0 )
    return;

  for ( int n = 0;
            n < draw_data->CmdListsCount;
            n++ )
  {
    const ImDrawList* cmd_list = draw_data->CmdLists [n];
    const ImDrawVert* vtx_src  = cmd_list->VtxBuffer.Data;

    for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
    {
      vtx_dst->pos [0] = vtx_src->pos.x;
      vtx_dst->pos [1] = vtx_src->pos.y;
      vtx_dst->pos [2] = 0.0f;
      vtx_dst->col     = (vtx_src->col & 0xFF00FF00)      |
                        ((vtx_src->col & 0xFF0000) >> 16) |
                        ((vtx_src->col & 0xFF)     << 16);     // RGBA --> ARGB for DirectX9
      vtx_dst->uv  [0] = vtx_src->uv.x;
      vtx_dst->uv  [1] = vtx_src->uv.y;
      vtx_dst++;
      vtx_src++;
    }

    memcpy ( idx_dst,
               cmd_list->IdxBuffer.Data,
                 cmd_list->IdxBuffer.Size * sizeof ImDrawIdx);

    idx_dst += cmd_list->IdxBuffer.Size;
  }

  g_pVB->Unlock ();
  g_pIB->Unlock ();

  g_pd3dDevice->SetStreamSource (0, g_pVB, 0, sizeof CUSTOMVERTEX);
  g_pd3dDevice->SetIndices      (g_pIB);
  g_pd3dDevice->SetFVF          (D3DFVF_CUSTOMVERTEX);

  // Setup viewport
  D3DVIEWPORT9 vp;

  vp.X = vp.Y = 0;
  vp.Width  = (DWORD)io.DisplaySize.x;
  vp.Height = (DWORD)io.DisplaySize.y;
  vp.MinZ   = 0.0f;
  vp.MaxZ   = 1.0f;
  g_pd3dDevice->SetViewport (&vp);

  // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
  g_pd3dDevice->SetPixelShader       (NULL);
  g_pd3dDevice->SetVertexShader      (NULL);

  D3DCAPS9                      caps;
  g_pd3dDevice->GetDeviceCaps (&caps);



  g_pd3dDevice->SetRenderState       (D3DRS_CULLMODE,          D3DCULL_NONE);
  g_pd3dDevice->SetRenderState       (D3DRS_LIGHTING,          FALSE);
  g_pd3dDevice->SetRenderState       (D3DRS_ZENABLE,           TRUE);
  g_pd3dDevice->SetRenderState       (D3DRS_ALPHABLENDENABLE,  TRUE);
  g_pd3dDevice->SetRenderState       (D3DRS_ALPHATESTENABLE,   FALSE);
  g_pd3dDevice->SetRenderState       (D3DRS_BLENDOP,           D3DBLENDOP_ADD);
  g_pd3dDevice->SetRenderState       (D3DRS_SRCBLEND,          D3DBLEND_SRCALPHA);
  g_pd3dDevice->SetRenderState       (D3DRS_DESTBLEND,         D3DBLEND_INVSRCALPHA);
  g_pd3dDevice->SetRenderState       (D3DRS_SCISSORTESTENABLE, TRUE);
  g_pd3dDevice->SetRenderState       (D3DRS_ZENABLE,           FALSE);

  g_pd3dDevice->SetRenderState       (D3DRS_SRGBWRITEENABLE,    FALSE);
  g_pd3dDevice->SetRenderState       (D3DRS_COLORWRITEENABLE,  D3DCOLORWRITEENABLE_RED   | 
                                                               D3DCOLORWRITEENABLE_GREEN | 
                                                               D3DCOLORWRITEENABLE_BLUE  |
                                                               D3DCOLORWRITEENABLE_ALPHA );

  g_pd3dDevice->SetTextureStageState   (0, D3DTSS_COLOROP,     D3DTOP_MODULATE);
  g_pd3dDevice->SetTextureStageState   (0, D3DTSS_COLORARG1,   D3DTA_TEXTURE);
  g_pd3dDevice->SetTextureStageState   (0, D3DTSS_COLORARG2,   D3DTA_DIFFUSE);
  g_pd3dDevice->SetTextureStageState   (0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
  g_pd3dDevice->SetTextureStageState   (0, D3DTSS_ALPHAARG1,   D3DTA_TEXTURE);
  g_pd3dDevice->SetTextureStageState   (0, D3DTSS_ALPHAARG2,   D3DTA_DIFFUSE);
  g_pd3dDevice->SetSamplerState        (0, D3DSAMP_MINFILTER,  D3DTEXF_LINEAR);
  g_pd3dDevice->SetSamplerState        (0, D3DSAMP_MAGFILTER,  D3DTEXF_LINEAR);

  for (UINT i = 1; i < caps.MaxTextureBlendStages; i++) {
    g_pd3dDevice->SetTextureStageState (i, D3DTSS_COLOROP,     D3DTOP_DISABLE);
    g_pd3dDevice->SetTextureStageState (i, D3DTSS_ALPHAOP,     D3DTOP_DISABLE);
  }

  // Setup orthographic projection matrix
  // Being agnostic of whether <d3dx9.h> or <DirectXMath.h> can be used, we aren't relying on D3DXMatrixIdentity()/D3DXMatrixOrthoOffCenterLH() or DirectX::XMMatrixIdentity()/DirectX::XMMatrixOrthographicOffCenterLH()
  {
    const float L = 0.5f,
                R = io.DisplaySize.x + 0.5f,
                T = 0.5f,
                B = io.DisplaySize.y + 0.5f;

    D3DMATRIX mat_identity =
    {
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
    };

    D3DMATRIX mat_projection =
    {
        2.0f/(R-L),   0.0f,         0.0f,  0.0f,
        0.0f,         2.0f/(T-B),   0.0f,  0.0f,
        0.0f,         0.0f,         0.5f,  0.0f,
        (L+R)/(L-R),  (T+B)/(B-T),  0.5f,  1.0f,
    };

    g_pd3dDevice->SetTransform (D3DTS_WORLD,      &mat_identity);
    g_pd3dDevice->SetTransform (D3DTS_VIEW,       &mat_identity);
    g_pd3dDevice->SetTransform (D3DTS_PROJECTION, &mat_projection);
  }

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
      {
        pcmd->UserCallback (cmd_list, pcmd);
      }

      else
      {
        const RECT r = {
          (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y,
          (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w
        };

        g_pd3dDevice->SetTexture           ( 0, (LPDIRECT3DTEXTURE9)pcmd->TextureId );
        g_pd3dDevice->SetScissorRect       ( &r );
        g_pd3dDevice->DrawIndexedPrimitive ( D3DPT_TRIANGLELIST,
                                               vtx_offset,
                                                 0,
                                                   (UINT)cmd_list->VtxBuffer.Size,
                                                     idx_offset,
                                                       pcmd->ElemCount / 3 );
      }

      idx_offset += pcmd->ElemCount;
    }

    vtx_offset += cmd_list->VtxBuffer.Size;
  }

  // Restore the DX9 state
  d3d9_state_block->Apply   ();
  d3d9_state_block->Release ();
}

IMGUI_API
bool
ImGui_ImplDX9_Init (void* hwnd, IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pparams)
{
  g_hWnd       = (HWND)hwnd;
  g_pd3dDevice = device;

  if (! QueryPerformanceFrequency ((LARGE_INTEGER *)&g_TicksPerSecond))
    return false;

  if (! QueryPerformanceCounter   ((LARGE_INTEGER *)&g_Time))
    return false;

  ImGuiIO& io =
    ImGui::GetIO ();


  // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
  io.KeyMap [ImGuiKey_Tab]        = VK_TAB;
  io.KeyMap [ImGuiKey_LeftArrow]  = VK_LEFT;
  io.KeyMap [ImGuiKey_RightArrow] = VK_RIGHT;
  io.KeyMap [ImGuiKey_UpArrow]    = VK_UP;
  io.KeyMap [ImGuiKey_DownArrow]  = VK_DOWN;
  io.KeyMap [ImGuiKey_PageUp]     = VK_PRIOR;
  io.KeyMap [ImGuiKey_PageDown]   = VK_NEXT;
  io.KeyMap [ImGuiKey_Home]       = VK_HOME;
  io.KeyMap [ImGuiKey_End]        = VK_END;
  io.KeyMap [ImGuiKey_Delete]     = VK_DELETE;
  io.KeyMap [ImGuiKey_Backspace]  = VK_BACK;
  io.KeyMap [ImGuiKey_Enter]      = VK_RETURN;
  io.KeyMap [ImGuiKey_Escape]     = VK_ESCAPE;
  io.KeyMap [ImGuiKey_A]          = 'A';
  io.KeyMap [ImGuiKey_C]          = 'C';
  io.KeyMap [ImGuiKey_V]          = 'V';
  io.KeyMap [ImGuiKey_X]          = 'X';
  io.KeyMap [ImGuiKey_Y]          = 'Y';
  io.KeyMap [ImGuiKey_Z]          = 'Z';

  // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
  io.RenderDrawListsFn = ImGui_ImplDX9_RenderDrawLists;
  io.ImeWindowHandle   = g_hWnd;


  float width = 0.0f, height = 0.0f;

  if ( pparams != nullptr )
  {
    width  = (float)pparams->BackBufferWidth;
    height = (float)pparams->BackBufferHeight;
  }

  ImGui::GetIO ().DisplayFramebufferScale = ImVec2 ( width, height );
  //ImGui::GetIO ().DisplaySize             = ImVec2 ( width, height );


  return true;
}

IMGUI_API
void
ImGui_ImplDX9_Shutdown (void)
{
  ImGui_ImplDX9_InvalidateDeviceObjects ( nullptr );
  ImGui::Shutdown                       (         );

  g_pd3dDevice = nullptr;
  g_hWnd       = 0;
}

IMGUI_API
bool
ImGui_ImplDX9_CreateFontsTexture (void)
{
  // Build texture atlas
  ImGuiIO& io =
    ImGui::GetIO ();

  extern void
  SK_ImGui_LoadFonts (void);

  SK_ImGui_LoadFonts ();

  unsigned char* pixels;
  int            width,
                 height,
                 bytes_per_pixel;

  io.Fonts->GetTexDataAsRGBA32 ( &pixels,
                                   &width, &height,
                                     &bytes_per_pixel );

  // Upload texture to graphics system
  g_FontTexture = nullptr;

  if ( g_pd3dDevice->CreateTexture ( width, height,
                                       1, D3DUSAGE_DYNAMIC,
                                          D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT,
                                            &g_FontTexture,
                                              nullptr ) < 0 )
    return false;

  D3DLOCKED_RECT tex_locked_rect;

  if ( g_FontTexture->LockRect ( 0,       &tex_locked_rect,
                                 nullptr, 0 ) != D3D_OK )
    return false;

  for (int y = 0; y < height; y++) {
      memcpy ( (unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y,
                 pixels + (width * bytes_per_pixel) * y,
                   (width * bytes_per_pixel) );
  }

  g_FontTexture->UnlockRect (0);

  // Store our identifier
  io.Fonts->TexID =
    (void *)g_FontTexture;

  return true;
}

bool
ImGui_ImplDX9_CreateDeviceObjects (void)
{
  if (! g_pd3dDevice)
      return false;

  if (! ImGui_ImplDX9_CreateFontsTexture ())
      return false;

  return true;
}

void
ImGui_ImplDX9_InvalidateDeviceObjects (D3DPRESENT_PARAMETERS* pparams)
{
  if (! g_pd3dDevice)
    return;

  if (g_pVB)
  {
    g_pVB->Release ();
    g_pVB = NULL;
  }

  if (g_pIB)
  {
    g_pIB->Release ();
    g_pIB = NULL;
  }

  if ( LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)ImGui::GetIO ().Fonts->TexID )
  {
    tex->Release ();
    ImGui::GetIO ().Fonts->TexID = 0;
  }

  g_FontTexture = NULL;


  float width = 0.0f, height = 0.0f;

  if ( pparams != nullptr )
  {
    width  = (float)pparams->BackBufferWidth;
    height = (float)pparams->BackBufferHeight;
  }

  ImGui::GetIO ().DisplayFramebufferScale = ImVec2 ( width, height );
  ImGui::GetIO ().DisplaySize             = ImVec2 ( width, height );
}

#include <SpecialK/window.h>

#include <windowsx.h>
#include <SpecialK/input/input.h>

IMGUI_API
void
ImGui_ImplDX9_NewFrame (void)
{
  ImGuiIO& io =
    ImGui::GetIO ();

  if (! g_FontTexture)
    ImGui_ImplDX9_CreateDeviceObjects ();

  static HMODULE hModTBFix = GetModuleHandle (L"tbfix.dll");


  // Setup display size (every frame to accommodate for window resizing)
  RECT rect;
  GetClientRect (g_hWnd, &rect);

  io.DisplayFramebufferScale =
    ImVec2 ( (float)(rect.right - rect.left),
               (float)(rect.bottom - rect.top) );


  if (! g_pd3dDevice)
    return;


  CComPtr <IDirect3DSwapChain9> pSwapChain = nullptr;

  if (SUCCEEDED (g_pd3dDevice->GetSwapChain ( 0, &pSwapChain )))
  {
    D3DPRESENT_PARAMETERS pp;

    if (SUCCEEDED (pSwapChain->GetPresentParameters (&pp)))
    {
      if (pp.BackBufferWidth != 0 && pp.BackBufferHeight != 0)
      {
        io.DisplaySize.x = (float)pp.BackBufferWidth;
        io.DisplaySize.y = (float)pp.BackBufferHeight;

        io.DisplayFramebufferScale = ImVec2 ( (float)pp.BackBufferWidth, (float)pp.BackBufferHeight );
      }
    }
  }

  // Setup time step
  INT64 current_time;

  QueryPerformanceCounter ((LARGE_INTEGER *)&current_time);

  io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
  g_Time       =         current_time;

  // Read keyboard modifiers inputs
  io.KeyCtrl   = (GetAsyncKeyState_Original (VK_CONTROL) & 0x8000) != 0;
  io.KeyShift  = (GetAsyncKeyState_Original (VK_SHIFT)   & 0x8000) != 0;
  io.KeyAlt    = (GetAsyncKeyState_Original (VK_MENU)    & 0x8000) != 0;

  io.KeySuper  = false;

  extern void SK_ImGui_PollGamepad (void);
  SK_ImGui_PollGamepad ();

  // Start the frame
  ImGui::NewFrame ();
}
