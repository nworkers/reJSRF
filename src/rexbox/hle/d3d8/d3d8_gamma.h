// The display gamma ramp (pure, no GL; verified by rexbox_d3d8_probe). The
// Xbox D3DGAMMARAMP is BYTE red[256], green[256], blue[256]: what the RAMDAC
// palette maps each 8-bit channel value to on the way to the TV (confirmed
// from the lifted D3DDevice_SetGammaRamp; docs/kb/xbox-d3d8-api.md section 11).
// The title fades whole screens through it.

#ifndef REXBOX_HLE_D3D8_D3D8_GAMMA_H
#define REXBOX_HLE_D3D8_D3D8_GAMMA_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

struct GammaRamp
{
    static constexpr size_t kEntries = 256;
    static constexpr size_t kBytes = kEntries * 3;  // red, green, blue tables in this order

    GammaRamp();  // identity

    bool IsIdentity() const;

    std::array<uint8_t, kBytes> bytes;
};

// Maps the R, G and B bytes of `pixels` RGBA8 texels through `ramp`; alpha is kept.
void ApplyGammaRamp(const GammaRamp& ramp, uint8_t* rgba, size_t pixels);

// The ramp as a 256x1 RGBA8 row (texel i = red[i], green[i], blue[i], 255),
// the lookup texture of the present shader.
std::array<uint8_t, GammaRamp::kEntries * 4> GammaRampTexels(const GammaRamp& ramp);

// The present shaders: a fullscreen triangle from gl_VertexID that samples
// u_image and looks each channel up in u_ramp. `gles` selects "#version 300 es".
std::string GammaPresentVertexShader(bool gles);
std::string GammaPresentFragmentShader(bool gles);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GAMMA_H
