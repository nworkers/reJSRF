#include "d3d8_fvf.h"

namespace rexbox::hle::d3d8
{

bool DecodeFvf(uint32_t code, VertexLayout* layout)
{
    if ((code & fvf::kProgramHandle) != 0)
    {
        return false;
    }
    VertexLayout result;
    uint32_t offset = 0;
    const uint32_t position = code & fvf::kPositionMask;
    if (position == fvf::kXyz)
    {
        result.position_components = 3;
        offset = 12;
    }
    else if (position == fvf::kXyzRhw)
    {
        result.position_components = 4;
        result.pretransformed = true;
        offset = 16;
    }
    else if (position >= fvf::kXyzB1 && position <= fvf::kXyzB4)
    {
        result.position_components = 3;
        result.blend_weights = (position - fvf::kXyzRhw) / 2;
        offset = 12 + 4 * result.blend_weights;
    }
    else
    {
        return false;
    }
    if ((code & fvf::kNormal) != 0)
    {
        result.normal = static_cast<int>(offset);
        offset += 12;
    }
    if ((code & fvf::kPointSize) != 0)
    {
        result.point_size = static_cast<int>(offset);
        offset += 4;
    }
    if ((code & fvf::kDiffuse) != 0)
    {
        result.diffuse = static_cast<int>(offset);
        offset += 4;
    }
    if ((code & fvf::kSpecular) != 0)
    {
        result.specular = static_cast<int>(offset);
        offset += 4;
    }
    result.tex_coords = (code & fvf::kTexCountMask) >> fvf::kTexCountShift;
    if (result.tex_coords > kMaxTexCoords)
    {
        return false;
    }
    static const uint32_t kComponents[4] = {2, 3, 4, 1};
    for (uint32_t i = 0; i < result.tex_coords; ++i)
    {
        result.tex_offset[i] = static_cast<int>(offset);
        result.tex_components[i] = kComponents[(code >> (fvf::kTexCoordSizeShift + 2 * i)) & 3];
        offset += 4 * result.tex_components[i];
    }
    result.stride = offset;
    *layout = result;
    return true;
}

}  // namespace rexbox::hle::d3d8
