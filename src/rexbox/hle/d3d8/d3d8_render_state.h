// Xbox D3D8 (XDK 4134) state numbering: D3DRENDERSTATETYPE slots of
// D3D__RenderState and D3DTEXTURESTAGESTATETYPE slots of D3D__TextureState.
// Each block notes its evidence (docs/kb/xbox-d3d8-api.md section 7):
//   confirmed - the slot a lifted original setter writes, or a slot whose
//               value equals what the title passes with a known NV2A method
//               to D3DDevice_SetRenderState_Simple;
//   inferred  - XDK 4134 ordering known from other projects (knowledge
//               only), consistent with the confirmed block boundaries.

#ifndef REXBOX_HLE_D3D8_D3D8_RENDER_STATE_H
#define REXBOX_HLE_D3D8_D3D8_RENDER_STATE_H

#include <cstdint>

namespace rexbox::hle::d3d8
{

// D3D__RenderState slots.
namespace rs
{
// Simple block, 57..81 (confirmed: each slot holds the value the title passes
// with the NV2A method of that state, e.g. slot 57 with 0x354 DEPTH_FUNC; the
// values are in NV2A/GL encoding: comparisons 0x200.., blend factors 0x300..,
// stencil operations 0x1E00..). 77..81 are the polygon-offset states ZBias
// derives (the slots the lifted ZBias setter writes, in this order).
constexpr uint32_t kZFunc = 57;
constexpr uint32_t kAlphaFunc = 58;
constexpr uint32_t kAlphaBlendEnable = 59;
constexpr uint32_t kAlphaTestEnable = 60;
constexpr uint32_t kAlphaRef = 61;
constexpr uint32_t kSrcBlend = 62;
constexpr uint32_t kDestBlend = 63;
constexpr uint32_t kZWriteEnable = 64;
constexpr uint32_t kDitherEnable = 65;
constexpr uint32_t kShadeMode = 66;
constexpr uint32_t kColorWriteEnable = 67;  // per-byte enables, 0x01010101 = all (inferred)
constexpr uint32_t kStencilZFail = 68;
constexpr uint32_t kStencilPass = 69;
constexpr uint32_t kStencilFunc = 70;
constexpr uint32_t kStencilRef = 71;
constexpr uint32_t kStencilMask = 72;
constexpr uint32_t kStencilWriteMask = 73;
constexpr uint32_t kBlendOp = 74;
constexpr uint32_t kBlendColor = 75;
constexpr uint32_t kPolygonOffsetZSlopeScale = 77;
constexpr uint32_t kPolygonOffsetZOffset = 78;
constexpr uint32_t kPointOffsetEnable = 79;
constexpr uint32_t kWireframeOffsetEnable = 80;
constexpr uint32_t kSolidOffsetEnable = 81;

// Deferred block, 82..116 (inferred order; 82 and 117 confirmed as the
// D3D_g_DeferredRenderState / D3D_g_ComplexRenderState boundaries).
constexpr uint32_t kFogEnable = 82;
constexpr uint32_t kFogTableMode = 83;
constexpr uint32_t kFogStart = 84;
constexpr uint32_t kFogEnd = 85;
constexpr uint32_t kFogDensity = 86;
constexpr uint32_t kRangeFogEnable = 87;
constexpr uint32_t kWrap0 = 88;  // .. kWrap3 = 91
constexpr uint32_t kLighting = 92;
constexpr uint32_t kSpecularEnable = 93;
constexpr uint32_t kLocalViewer = 94;
constexpr uint32_t kColorVertex = 95;
constexpr uint32_t kSpecularMaterialSource = 100;
constexpr uint32_t kDiffuseMaterialSource = 101;
constexpr uint32_t kAmbientMaterialSource = 102;
constexpr uint32_t kEmissiveMaterialSource = 103;
constexpr uint32_t kAmbient = 105;
constexpr uint32_t kPointSize = 106;
constexpr uint32_t kSwapFilter = 116;

// Complex block, 117..145 (confirmed: the slot each lifted setter writes).
constexpr uint32_t kPSTextureModes = 117;
constexpr uint32_t kVertexBlend = 118;
constexpr uint32_t kFogColor = 119;
constexpr uint32_t kFillMode = 120;
constexpr uint32_t kBackFillMode = 121;
constexpr uint32_t kTwoSidedLighting = 122;
constexpr uint32_t kNormalizeNormals = 123;
constexpr uint32_t kZEnable = 124;
constexpr uint32_t kStencilEnable = 125;
constexpr uint32_t kStencilFail = 126;
constexpr uint32_t kFrontFace = 127;
constexpr uint32_t kCullMode = 128;
constexpr uint32_t kTextureFactor = 129;
constexpr uint32_t kZBias = 130;
constexpr uint32_t kLogicOp = 131;
constexpr uint32_t kEdgeAntiAlias = 132;
constexpr uint32_t kMultiSampleAntiAlias = 133;
constexpr uint32_t kMultiSampleMask = 134;
constexpr uint32_t kMultiSampleMode = 135;
constexpr uint32_t kMultiSampleRenderTargetMode = 136;
constexpr uint32_t kShadowFunc = 137;
constexpr uint32_t kLineWidth = 138;
constexpr uint32_t kDxt1NoiseEnable = 139;
constexpr uint32_t kYuvEnable = 140;
constexpr uint32_t kOcclusionCullEnable = 141;
constexpr uint32_t kStencilCullEnable = 142;
constexpr uint32_t kRopZCmpAlwaysRead = 143;
constexpr uint32_t kRopZRead = 144;
constexpr uint32_t kDoNotCullUncompressed = 145;
}  // namespace rs

// D3D__TextureState slots, per stage (inferred order; kTexCoordIndex 28,
// kBorderColor 29 and kColorKeyColor 30 confirmed from the lifted setters,
// kBumpEnvMat00..kBumpEnvLOffset as SetTextureState_BumpEnv's Type).
namespace tss
{
constexpr uint32_t kAddressU = 0;
constexpr uint32_t kAddressV = 1;
constexpr uint32_t kAddressW = 2;
constexpr uint32_t kMagFilter = 3;
constexpr uint32_t kMinFilter = 4;
constexpr uint32_t kMipFilter = 5;
constexpr uint32_t kMipMapLodBias = 6;
constexpr uint32_t kMaxMipLevel = 7;
constexpr uint32_t kMaxAnisotropy = 8;
constexpr uint32_t kColorKeyOp = 9;
constexpr uint32_t kColorSign = 10;
constexpr uint32_t kAlphaKill = 11;
constexpr uint32_t kColorOp = 12;
constexpr uint32_t kColorArg0 = 13;
constexpr uint32_t kColorArg1 = 14;
constexpr uint32_t kColorArg2 = 15;
constexpr uint32_t kAlphaOp = 16;
constexpr uint32_t kAlphaArg0 = 17;
constexpr uint32_t kAlphaArg1 = 18;
constexpr uint32_t kAlphaArg2 = 19;
constexpr uint32_t kResultArg = 20;
constexpr uint32_t kTextureTransformFlags = 21;
constexpr uint32_t kBumpEnvMat00 = 22;  // .. 25: 00, 01, 10, 11
constexpr uint32_t kBumpEnvLScale = 26;
constexpr uint32_t kBumpEnvLOffset = 27;
constexpr uint32_t kTexCoordIndex = 28;
constexpr uint32_t kBorderColor = 29;
constexpr uint32_t kColorKeyColor = 30;
}  // namespace tss


}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_RENDER_STATE_H
