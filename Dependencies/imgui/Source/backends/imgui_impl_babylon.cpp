#include "backends/imgui_impl_babylon.h"
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/bx.h>
#include <bx/math.h>
#include <bx/allocator.h>

#include <Babylon/Graphics/DeviceContext.h>

#define IMGUI_FONT_REGULAR 0
#define IMGUI_FONT_MONO 1
#define IMGUI_FONT_COUNT 2

#define IMGUI_FLAGS_NONE UINT8_C(0x00)
#define IMGUI_FLAGS_ALPHA_BLEND UINT8_C(0x01)

bx::AllocatorI *m_allocator;
bgfx::VertexLayout m_layout;
bgfx::ProgramHandle m_program;
bgfx::ProgramHandle m_imageProgram;
bgfx::TextureHandle m_texture;
bgfx::UniformHandle s_tex;
bgfx::UniformHandle u_imageLodEnabled;
ImFont *m_font[IMGUI_FONT_COUNT];
bgfx::ViewId m_viewId;

#include "shaders/vs_ocornut_imgui.bin.h"
#include "shaders/fs_ocornut_imgui.bin.h"
#include "shaders/vs_imgui_image.bin.h"
#include "shaders/fs_imgui_image.bin.h"

#include "assets/roboto_regular.ttf.h"
#include "assets/robotomono_regular.ttf.h"
#include "assets/icons_kenney.ttf.h"
#include "assets/icons_font_awesome.ttf.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] =
    {
        BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
        BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
        BGFX_EMBEDDED_SHADER(vs_imgui_image),
        BGFX_EMBEDDED_SHADER(fs_imgui_image),
        BGFX_EMBEDDED_SHADER_END()};

struct FontRangeMerge
{
    const void *data;
    size_t size;
    ImWchar ranges[3];
};

#define ICON_MIN_KI 0xe900
#define ICON_MAX_KI 0xe9e3
#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf2e0

static FontRangeMerge s_fontRangeMerge[] =
    {
        {s_iconsKenneyTtf, sizeof(s_iconsKenneyTtf), {ICON_MIN_KI, ICON_MAX_KI, 0}},
        {s_iconsFontAwesomeTtf, sizeof(s_iconsFontAwesomeTtf), {ICON_MIN_FA, ICON_MAX_FA, 0}},
};

inline bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexLayout &_layout, uint32_t _numIndices)
{
    return _numVertices == bgfx::getAvailTransientVertexBuffer(_numVertices, _layout) && (0 == _numIndices || _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices));
}

static bx::DefaultAllocator allocator;
static Babylon::Graphics::DeviceContext *s_context = nullptr;

bool ImGui_ImplBabylon_Init(uint32_t display_width, uint32_t display_height, float fontSize)
{
    float _fontSize = fontSize;

    bx::AllocatorI *_allocator = &allocator;
    m_allocator = _allocator;
    m_viewId = 255;

    ImGuiIO &io = ImGui::GetIO();

    io.DisplaySize = ImVec2(static_cast<float>( display_width ), static_cast<float>( display_height ));
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = NULL;

    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    m_program = bgfx::createProgram(
        bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_ocornut_imgui"), bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_ocornut_imgui"), true);

    u_imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
    m_imageProgram = bgfx::createProgram(
        bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui_image"), bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui_image"), true);

    m_layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    uint8_t *data;
    int32_t width;
    int32_t height;
    {
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        config.MergeMode = false;
        //			config.MergeGlyphCenterV = true;

        const ImWchar *ranges = io.Fonts->GetGlyphRangesCyrillic();
        m_font[IMGUI_FONT_REGULAR] = io.Fonts->AddFontFromMemoryTTF((void *)s_robotoRegularTtf, sizeof(s_robotoRegularTtf), _fontSize, &config, ranges);
        m_font[IMGUI_FONT_MONO] = io.Fonts->AddFontFromMemoryTTF((void *)s_robotoMonoRegularTtf, sizeof(s_robotoMonoRegularTtf), _fontSize - 3.0f, &config, ranges);

        config.MergeMode = true;
        config.DstFont = m_font[IMGUI_FONT_REGULAR];

        for (uint32_t ii = 0; ii < BX_COUNTOF(s_fontRangeMerge); ++ii)
        {
            const FontRangeMerge &frm = s_fontRangeMerge[ii];

            io.Fonts->AddFontFromMemoryTTF((void *)frm.data, (int)frm.size, _fontSize - 3.0f, &config, frm.ranges);
        }
    }

    io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);

    m_texture = bgfx::createTexture2D(
        (uint16_t)width, (uint16_t)height, false, 1, bgfx::TextureFormat::BGRA8, 0, bgfx::copy(data, width * height * 4));

    return true;
}

void ImGui_ImplBabylon_SetContext(Babylon::Graphics::DeviceContext *context)
{
    s_context = context;
}

void ImGui_ImplBabylon_NewFrame()
{
}

void ImGui_ImplBabylon_RenderDrawData(ImDrawData *_drawData)
{
    if (s_context == nullptr)
        return;

    arcana::make_task(s_context->AfterRenderScheduler(), arcana::cancellation_source::none(),
                      [_drawData{*_drawData}]()
                      {
                          // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
                          int fb_width = (int)(_drawData.DisplaySize.x * _drawData.FramebufferScale.x);
                          int fb_height = (int)(_drawData.DisplaySize.y * _drawData.FramebufferScale.y);
                          
                          if (fb_width <= 0 || fb_height <= 0)
                              return;

                          bgfx::setViewName(m_viewId, "ImGui");
                          bgfx::setViewMode(m_viewId, bgfx::ViewMode::Sequential);

                          const bgfx::Caps *caps = bgfx::getCaps();
                          {
                              float ortho[16];
                              float x = _drawData.DisplayPos.x;
                              float y = _drawData.DisplayPos.y;
                              float width = _drawData.DisplaySize.x;
                              float height = _drawData.DisplaySize.y;

                              bx::mtxOrtho(ortho, x, x + width, y + height, y, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
                              bgfx::setViewTransform(m_viewId, NULL, ortho);
                              bgfx::setViewRect(m_viewId, 0, 0, uint16_t(width), uint16_t(height));
                          }

                          const ImVec2 clipPos = _drawData.DisplayPos;         // (0,0) unless using multi-viewports
                          const ImVec2 clipScale = _drawData.FramebufferScale; // (1,1) unless using retina display which are often (2,2)

                          // Render command lists
                          for (int32_t ii = 0, num = _drawData.CmdListsCount; ii < num; ++ii)
                          {
                              bgfx::TransientVertexBuffer tvb;
                              bgfx::TransientIndexBuffer tib;

                              const ImDrawList *drawList = _drawData.CmdLists[ii];
                              uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
                              uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();

                              if (!checkAvailTransientBuffers(numVertices, m_layout, numIndices))
                              {
                                  // not enough space in transient buffer just quit drawing the rest...
                                  break;
                              }

                              bgfx::allocTransientVertexBuffer(&tvb, numVertices, m_layout);
                              bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

                              ImDrawVert *verts = (ImDrawVert *)tvb.data;
                              bx::memCopy(verts, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));

                              ImDrawIdx *indices = (ImDrawIdx *)tib.data;
                              bx::memCopy(indices, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

                              bgfx::Encoder *encoder = bgfx::begin();

                              for (const ImDrawCmd *cmd = drawList->CmdBuffer.begin(), *cmdEnd = drawList->CmdBuffer.end(); cmd != cmdEnd; ++cmd)
                              {
                                  if (cmd->UserCallback)
                                  {
                                      cmd->UserCallback(drawList, cmd);
                                  }
                                  else if (0 != cmd->ElemCount)
                                  {
                                      uint64_t state = 0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

                                      bgfx::TextureHandle th = m_texture;
                                      bgfx::ProgramHandle program = m_program;

                                      if (NULL != cmd->TextureId)
                                      {
                                          union
                                          {
                                              ImTextureID ptr;
                                              struct
                                              {
                                                  bgfx::TextureHandle handle;
                                                  uint8_t flags;
                                                  uint8_t mip;
                                              } s;
                                          } texture = {cmd->TextureId};
                                          state |= 0 != (IMGUI_FLAGS_ALPHA_BLEND & texture.s.flags)
                                                       ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                                                       : BGFX_STATE_NONE;

                                          th = texture.s.handle;
                                          if (0 != texture.s.mip)
                                          {
                                              const float lodEnabled[4] = {float(texture.s.mip), 1.0f, 0.0f, 0.0f};
                                              bgfx::setUniform(u_imageLodEnabled, lodEnabled);
                                              program = m_imageProgram;
                                          }
                                      }
                                      else
                                      {
                                          state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
                                      }

                                      ImVec4 clipRect = cmd->ClipRect;

                                      if (clipRect.x < fb_width && clipRect.y < fb_height && clipRect.z >= 0.0f && clipRect.w >= 0.0f)
                                      {
                                          const uint16_t xx = uint16_t(bx::max(clipRect.x, 0.0f));
                                          const uint16_t yy = uint16_t(bx::max(clipRect.y, 0.0f));
                                          encoder->setScissor(xx, yy, uint16_t(bx::min(clipRect.z, 65535.0f) - xx), uint16_t(bx::min(clipRect.w, 65535.0f) - yy));

                                          encoder->setState(state);
                                          encoder->setTexture(0, s_tex, th);
                                          encoder->setVertexBuffer(0, &tvb, cmd->VtxOffset, numVertices);
                                          encoder->setIndexBuffer(&tib, cmd->IdxOffset, cmd->ElemCount);
                                          encoder->submit(m_viewId, program);
                                      }
                                  }
                              }

                              bgfx::end(encoder);
                          }
                      });
}

void ImGui_ImplBabylon_Shutdown()
{
    bgfx::destroy(s_tex);
    bgfx::destroy(m_texture);
    bgfx::destroy(u_imageLodEnabled);
    bgfx::destroy(m_imageProgram);
    bgfx::destroy(m_program);
    m_allocator = NULL;
}