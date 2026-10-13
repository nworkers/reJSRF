#include "d3d8_vsh.h"

#include <cstdio>
#include <cstring>

namespace rexbox::hle::d3d8::vsh
{

namespace
{

uint32_t Field(const uint32_t words[4], int dword, int low, int width)
{
    return (words[dword] >> low) & ((1u << width) - 1u);
}

// A swizzle field holds x, y, z, w in that order from the top, 2 bits each.
void DecodeSwizzle(uint32_t field, uint8_t out[4])
{
    out[0] = static_cast<uint8_t>((field >> 6) & 3);
    out[1] = static_cast<uint8_t>((field >> 4) & 3);
    out[2] = static_cast<uint8_t>((field >> 2) & 3);
    out[3] = static_cast<uint8_t>(field & 3);
}

const char* const kMacNames[16] = {"nop", "mov", "mul", "add", "mad", "dp3", "dph", "dp4",
                                   "dst", "min", "max", "slt", "sge", "arl", "mac14", "mac15"};
const char* const kIluNames[8] = {"nop", "mov", "rcp", "rcc", "rsq", "exp", "log", "lit"};
const char* const kOutputNames[out::kCount] = {"oPos", "o1",   "o2",   "oD0",  "oD1",  "oFog", "oPts",
                                               "oB0",  "oB1",  "oT0",  "oT1",  "oT2",  "oT3"};

std::string MaskText(uint8_t mask)
{
    std::string text;
    if (mask & 8)
    {
        text += 'x';
    }
    if (mask & 4)
    {
        text += 'y';
    }
    if (mask & 2)
    {
        text += 'z';
    }
    if (mask & 1)
    {
        text += 'w';
    }
    return text;
}

std::string SwizzleText(const uint8_t swizzle[4])
{
    static const char kComponents[] = "xyzw";
    std::string text;
    for (int i = 0; i < 4; ++i)
    {
        text += kComponents[swizzle[i]];
    }
    return text;
}

bool IdentitySwizzle(const uint8_t swizzle[4])
{
    return swizzle[0] == 0 && swizzle[1] == 1 && swizzle[2] == 2 && swizzle[3] == 3;
}

std::string SourceText(const Instruction& in, const Source& source)
{
    std::string text = source.negate ? "-" : "";
    char buffer[32];
    switch (source.mux)
    {
    case mux::kTemp:
        std::snprintf(buffer, sizeof(buffer), source.temp == 12 ? "R12" : "R%u", source.temp);
        break;
    case mux::kInput:
        std::snprintf(buffer, sizeof(buffer), "v%u", in.input);
        break;
    case mux::kConstant:
        std::snprintf(buffer, sizeof(buffer), in.relative ? "c[a0.x+%u]" : "c[%u]", in.constant);
        break;
    default:
        std::snprintf(buffer, sizeof(buffer), "?mux%u", source.mux);
        break;
    }
    text += buffer;
    if (!IdentitySwizzle(source.swizzle))
    {
        text += "." + SwizzleText(source.swizzle);
    }
    return text;
}

// Which sources each unit's op reads (MAC: A, B, C; ILU: C).
bool MacReadsA(uint8_t op)
{
    return op != mac::kNop;
}

bool MacReadsB(uint8_t op)
{
    return op == mac::kMul || op == mac::kMad || op == mac::kDp3 || op == mac::kDph || op == mac::kDp4 ||
           op == mac::kDst || op == mac::kMin || op == mac::kMax || op == mac::kSlt || op == mac::kSge;
}

bool MacReadsC(uint8_t op)
{
    return op == mac::kAdd || op == mac::kMad;
}

// --- GLSL ----------------------------------------------------------------------

std::string Number(uint32_t value)
{
    return std::to_string(value);
}

std::string GlslSource(const Instruction& in, const Source& source, uint32_t* used_inputs)
{
    std::string base;
    switch (source.mux)
    {
    case mux::kTemp:
        base = source.temp == 12 ? "oPos" : (source.temp < kTemps ? "R" + Number(source.temp) : "R0");
        break;
    case mux::kInput:
        base = "v" + Number(in.input);
        *used_inputs |= 1u << in.input;
        break;
    case mux::kConstant:
        base = in.relative ? "c[clamp(a0 + " + Number(in.constant) + ", 0, 191)]" : "c[" + Number(in.constant) + "]";
        break;
    default:
        base = "vec4(0.0)";
        break;
    }
    if (!IdentitySwizzle(source.swizzle))
    {
        base += "." + SwizzleText(source.swizzle);
    }
    return source.negate ? "(-" + base + ")" : base;
}

std::string MacExpression(uint8_t op)
{
    switch (op)
    {
    case mac::kMov:
        return "A";
    case mac::kMul:
        return "A * B";
    case mac::kAdd:
        return "A + C";
    case mac::kMad:
        return "A * B + C";
    case mac::kDp3:
        return "vec4(dot(A.xyz, B.xyz))";
    case mac::kDph:
        return "vec4(dot(A.xyz, B.xyz) + B.w)";
    case mac::kDp4:
        return "vec4(dot(A, B))";
    case mac::kDst:
        return "vec4(1.0, A.y * B.y, A.z, B.w)";
    case mac::kMin:
        return "min(A, B)";
    case mac::kMax:
        return "max(A, B)";
    case mac::kSlt:
        return "vec4(lessThan(A, B))";
    case mac::kSge:
        return "vec4(greaterThanEqual(A, B))";
    default:
        return "vec4(0.0)";
    }
}

std::string IluExpression(uint8_t op)
{
    switch (op)
    {
    case ilu::kMov:
        return "C";
    case ilu::kRcp:
        return "vec4(1.0 / C.x)";
    case ilu::kRcc:
        return "vec4(rcc(C.x))";
    case ilu::kRsq:
        return "vec4(inversesqrt(abs(C.x)))";
    case ilu::kExp:
        return "vec4(exp2(floor(C.x)), C.x - floor(C.x), exp2(C.x), 1.0)";
    case ilu::kLog:
        return "nv_log(C.x)";
    case ilu::kLit:
        return "nv_lit(C)";
    default:
        return "vec4(0.0)";
    }
}

std::string TempName(uint8_t temp)
{
    return temp == 12 ? "oPos" : (temp < kTemps ? "R" + Number(temp) : std::string());
}

std::string OutputName(uint8_t address)
{
    switch (address)
    {
    case out::kPosition:
        return "oPos";
    case out::kDiffuse:
        return "oD0";
    case out::kSpecular:
        return "oD1";
    case out::kFog:
        return "oFog";
    case out::kPointSize:
        return "oPts";
    case out::kBackDiffuse:
        return "oB0";
    case out::kBackSpecular:
        return "oB1";
    default:
        if (address >= out::kTexture0 && address < out::kTexture0 + 4)
        {
            return "oT" + Number(address - out::kTexture0);
        }
        return std::string();
    }
}

std::string MaskedWrite(const std::string& destination, uint8_t mask, const char* value)
{
    const std::string components = MaskText(mask);
    return "        " + destination + "." + components + " = " + value + "." + components + ";\n";
}

}  // namespace

Instruction Decode(const uint32_t words[4])
{
    Instruction in;
    in.ilu = static_cast<uint8_t>(Field(words, 1, 25, 3));
    in.mac = static_cast<uint8_t>(Field(words, 1, 21, 4));
    in.constant = static_cast<uint8_t>(Field(words, 1, 13, 8));
    in.input = static_cast<uint8_t>(Field(words, 1, 9, 4));

    in.a.negate = Field(words, 1, 8, 1) != 0;
    DecodeSwizzle(Field(words, 1, 0, 8), in.a.swizzle);
    in.a.temp = static_cast<uint8_t>(Field(words, 2, 28, 4));
    in.a.mux = static_cast<uint8_t>(Field(words, 2, 26, 2));

    in.b.negate = Field(words, 2, 25, 1) != 0;
    DecodeSwizzle(Field(words, 2, 17, 8), in.b.swizzle);
    in.b.temp = static_cast<uint8_t>(Field(words, 2, 13, 4));
    in.b.mux = static_cast<uint8_t>(Field(words, 2, 11, 2));

    in.c.negate = Field(words, 2, 10, 1) != 0;
    DecodeSwizzle(Field(words, 2, 2, 8), in.c.swizzle);
    in.c.temp = static_cast<uint8_t>((Field(words, 2, 0, 2) << 2) | Field(words, 3, 30, 2));
    in.c.mux = static_cast<uint8_t>(Field(words, 3, 28, 2));

    in.mac_mask = static_cast<uint8_t>(Field(words, 3, 24, 4));
    in.temp = static_cast<uint8_t>(Field(words, 3, 20, 4));
    in.ilu_mask = static_cast<uint8_t>(Field(words, 3, 16, 4));
    in.out_mask = static_cast<uint8_t>(Field(words, 3, 12, 4));
    in.out_to_o = Field(words, 3, 11, 1) != 0;
    in.out_address = static_cast<uint8_t>(Field(words, 3, 3, 8));
    in.out_from_ilu = Field(words, 3, 2, 1) != 0;
    in.relative = Field(words, 3, 1, 1) != 0;
    in.final = Field(words, 3, 0, 1) != 0;
    return in;
}

std::string Disassemble(const Instruction& in)
{
    std::string text;
    const auto append = [&text](const std::string& part) {
        if (!text.empty())
        {
            text += " + ";
        }
        text += part;
    };
    const auto output = [&in]() {
        if (!in.out_to_o)
        {
            return "c[" + Number(in.out_address) + "]." + MaskText(in.out_mask);
        }
        const std::string name = in.out_address < out::kCount ? kOutputNames[in.out_address]
                                                               : "o" + Number(in.out_address);
        return name + "." + MaskText(in.out_mask);
    };
    if (in.mac != mac::kNop)
    {
        std::string dests;
        if (in.mac_mask != 0 && in.mac != mac::kArl)
        {
            dests = (in.temp == 12 ? std::string("R12") : "R" + Number(in.temp)) + "." + MaskText(in.mac_mask);
        }
        if (in.mac == mac::kArl)
        {
            dests = "a0.x";
        }
        if (in.out_mask != 0 && !in.out_from_ilu)
        {
            dests += (dests.empty() ? "" : ", ") + output();
        }
        std::string part = std::string(kMacNames[in.mac]) + " " + (dests.empty() ? "-" : dests);
        part += ", " + SourceText(in, in.a);
        if (MacReadsB(in.mac))
        {
            part += ", " + SourceText(in, in.b);
        }
        if (MacReadsC(in.mac))
        {
            part += ", " + SourceText(in, in.c);
        }
        append(part);
    }
    if (in.ilu != ilu::kNop)
    {
        std::string dests;
        if (in.ilu_mask != 0)
        {
            const uint8_t temp = in.mac != mac::kNop ? 1 : in.temp;
            dests = (temp == 12 ? std::string("R12") : "R" + Number(temp)) + "." + MaskText(in.ilu_mask);
        }
        if (in.out_mask != 0 && in.out_from_ilu)
        {
            dests += (dests.empty() ? "" : ", ") + output();
        }
        append(std::string(kIluNames[in.ilu]) + " " + (dests.empty() ? "-" : dests) + ", " + SourceText(in, in.c));
    }
    if (text.empty())
    {
        text = "nop";
    }
    if (in.final)
    {
        text += "  ; final";
    }
    return text;
}

bool ReadProgram(const uint8_t* bytes, size_t size, Program* program, std::string* error)
{
    *program = Program{};
    if (bytes == nullptr || size < 4)
    {
        *error = "no program header";
        return false;
    }
    uint32_t header = 0;
    std::memcpy(&header, bytes, 4);
    program->type = static_cast<uint16_t>(header & 0xFFFF);
    const uint32_t count = header >> 16;
    if (count == 0 || count > kMaxInstructions)
    {
        *error = "instruction count " + Number(count) + " out of range";
        return false;
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        const size_t offset = 4 + static_cast<size_t>(i) * 16;
        if (offset + 16 > size)
        {
            *error = "program runs past the readable memory";
            return false;
        }
        std::array<uint32_t, 4> words = {};
        std::memcpy(words.data(), bytes + offset, 16);
        program->words.push_back(words);
        if (Decode(words.data()).final)
        {
            break;
        }
    }
    return true;
}

bool ParseDeclaration(const std::vector<uint32_t>& tokens, Declaration* declaration, std::string* error)
{
    *declaration = Declaration{};
    uint32_t stream = 0;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const uint32_t token = tokens[i];
        if (token == 0xFFFFFFFFu)
        {
            return true;
        }
        const uint32_t kind = token >> 29;
        switch (kind)
        {
        case 0:  // D3DVSD_NOP
            break;
        case 1:  // D3DVSD_STREAM(n)
            stream = token & 0xF;
            declaration->streams |= 1u << stream;
            break;
        case 2:  // D3DVSD_REG(reg, type) or D3DVSD_SKIP(count)
            if ((token & (1u << 28)) != 0)
            {
                declaration->stream_bytes[stream] =
                    static_cast<uint16_t>(declaration->stream_bytes[stream] + ((token >> 16) & 0xF) * 4);
                break;
            }
            else
            {
                VertexElement element;
                element.reg = static_cast<uint8_t>(token & 0x1F);
                element.stream = static_cast<uint8_t>(stream);
                element.type = static_cast<uint8_t>((token >> 16) & 0xFF);
                element.offset = declaration->stream_bytes[stream];
                const uint8_t count = static_cast<uint8_t>(element.type >> 4);
                uint32_t bytes = 0;
                switch (element.type & 0x0F)
                {
                case 2:
                    element.kind = ElementKind::kFloat;
                    bytes = 4u * count;
                    break;
                case 1:
                    element.kind = ElementKind::kNormShort;
                    bytes = 2u * count;
                    break;
                case 5:
                    element.kind = ElementKind::kShort;
                    bytes = 2u * count;
                    break;
                case 4:
                    element.kind = ElementKind::kNormByte;
                    bytes = count;
                    break;
                case 0:
                    element.kind = ElementKind::kD3dColor;
                    bytes = 4;
                    break;
                default:
                    *error = "register type " + Number(element.type) + " is not handled yet";
                    return false;
                }
                if (element.type == vsdt::kNone)
                {
                    break;  // the register keeps its default; nothing is read
                }
                if (element.reg >= kInputs || count == 0 || count > 4)
                {
                    *error = "register v" + Number(element.reg) + " type " + Number(element.type) + " is invalid";
                    return false;
                }
                element.components = element.kind == ElementKind::kD3dColor ? 4 : count;
                if (element.kind == ElementKind::kD3dColor)
                {
                    declaration->bgra_inputs |= 1u << element.reg;
                }
                declaration->stream_bytes[stream] = static_cast<uint16_t>(declaration->stream_bytes[stream] + bytes);
                declaration->elements.push_back(element);
            }
            break;
        default:
            *error = "declaration token " + Number(token) + " (kind " + Number(kind) + ") is not handled yet";
            return false;
        }
    }
    *error = "no D3DVSD_END";
    return false;
}

void ViewportConstants(uint32_t x, uint32_t y, uint32_t width, uint32_t height, float scale[4], float offset[4])
{
    const float half_width = static_cast<float>(width) * 0.5f;
    const float half_height = static_cast<float>(height) * 0.5f;
    scale[0] = half_width;
    scale[1] = -half_height;
    scale[2] = 1.0f;
    scale[3] = 1.0f;
    offset[0] = static_cast<float>(x) + half_width;
    offset[1] = static_cast<float>(y) + half_height;
    offset[2] = 0.0f;
    offset[3] = 0.0f;
}

std::string TranslateToGlsl(const Program& program, uint32_t bgra_inputs, bool gles, uint32_t* used_inputs)
{
    *used_inputs = 0;
    std::string body;
    for (const auto& words : program.words)
    {
        const Instruction in = Decode(words.data());
        body += "    {\n";
        // Both units read their sources before either writes.
        if (MacReadsA(in.mac))
        {
            body += "        vec4 A = " + GlslSource(in, in.a, used_inputs) + ";\n";
        }
        if (MacReadsB(in.mac))
        {
            body += "        vec4 B = " + GlslSource(in, in.b, used_inputs) + ";\n";
        }
        if (MacReadsC(in.mac) || in.ilu != ilu::kNop)
        {
            body += "        vec4 C = " + GlslSource(in, in.c, used_inputs) + ";\n";
        }
        if (in.mac == mac::kArl)
        {
            body += "        a0 = int(floor(A.x + 0.001));\n";
        }
        else if (in.mac != mac::kNop)
        {
            body += "        vec4 m = " + MacExpression(in.mac) + ";\n";
            const std::string dest = TempName(in.temp);
            if (in.mac_mask != 0 && !dest.empty())
            {
                body += MaskedWrite(dest, in.mac_mask, "m");
            }
        }
        if (in.ilu != ilu::kNop)
        {
            body += "        vec4 i = " + IluExpression(in.ilu) + ";\n";
            // Paired with a MAC op, the ILU writes R1 whatever the temp field says.
            const std::string dest = TempName(in.mac != mac::kNop ? 1 : in.temp);
            if (in.ilu_mask != 0 && !dest.empty())
            {
                body += MaskedWrite(dest, in.ilu_mask, "i");
            }
        }
        if (in.out_mask != 0 && in.out_to_o)
        {
            const bool from_ilu = in.out_from_ilu && in.ilu != ilu::kNop;
            const bool from_mac = !in.out_from_ilu && in.mac != mac::kNop && in.mac != mac::kArl;
            const std::string dest = OutputName(in.out_address);
            if ((from_ilu || from_mac) && !dest.empty())
            {
                body += MaskedWrite(dest, in.out_mask, from_ilu ? "i" : "m");
            }
        }
        // A write to c[] (read/write and state shaders) changes no uniform here.
        body += "    }\n";
        if (in.final)
        {
            break;
        }
    }

    std::string text = gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
    std::string inputs_fixup;
    for (uint32_t i = 0; i < kInputs; ++i)
    {
        if (*used_inputs & (1u << i))
        {
            // A D3DCOLOR arrives as B, G, R, A bytes.
            const bool bgra = (bgra_inputs & (1u << i)) != 0;
            text += "layout(location = " + Number(i) + ") in vec4 " + (bgra ? "a" : "v") + Number(i) + ";\n";
            if (bgra)
            {
                inputs_fixup += "    vec4 v" + Number(i) + " = a" + Number(i) + ".zyxw;\n";
            }
        }
    }
    text += "uniform vec4 c[192];\n"
            "out vec4 v_diffuse;\n"
            "out vec4 v_specular;\n"
            "out vec4 v_tex0;\n"
            "out vec4 v_tex1;\n"
            "out vec4 v_tex2;\n"
            "out vec4 v_tex3;\n"
            "out float v_fog;\n"
            "float rcc(float x)\n"
            "{\n"
            "    float f = 1.0 / x;\n"
            "    return sign(f) * clamp(abs(f), 5.42101e-20, 1.884467e19);\n"
            "}\n"
            "vec4 nv_log(float x)\n"
            "{\n"
            "    float ax = abs(x);\n"
            "    if (ax == 0.0) return vec4(-1.0e38, 1.0, -1.0e38, 1.0);\n"
            "    float e = floor(log2(ax));\n"
            "    return vec4(e, ax / exp2(e), log2(ax), 1.0);\n"
            "}\n"
            "vec4 nv_lit(vec4 s)\n"
            "{\n"
            "    float p = clamp(s.w, -127.9961, 127.9961);\n"
            "    return vec4(1.0, max(s.x, 0.0), s.x > 0.0 ? pow(max(s.y, 0.0), p) : 0.0, 1.0);\n"
            "}\n"
            "void main()\n"
            "{\n"
            "    vec4 R0 = vec4(0.0), R1 = vec4(0.0), R2 = vec4(0.0), R3 = vec4(0.0);\n"
            "    vec4 R4 = vec4(0.0), R5 = vec4(0.0), R6 = vec4(0.0), R7 = vec4(0.0);\n"
            "    vec4 R8 = vec4(0.0), R9 = vec4(0.0), R10 = vec4(0.0), R11 = vec4(0.0);\n"
            "    vec4 oPos = vec4(0.0);\n"
            // Colours default opaque; unwritten texture coordinates are (0, 0, 0, 1).
            "    vec4 oD0 = vec4(0.0, 0.0, 0.0, 1.0), oD1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    vec4 oB0 = vec4(0.0, 0.0, 0.0, 1.0), oB1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    vec4 oFog = vec4(0.0), oPts = vec4(0.0);\n"
            "    vec4 oT0 = vec4(0.0, 0.0, 0.0, 1.0), oT1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    vec4 oT2 = vec4(0.0, 0.0, 0.0, 1.0), oT3 = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "    int a0 = 0;\n";
    text += inputs_fixup;
    text += body;
    // The runtime epilogue leaves screen coordinates in oPos.xyz
    // (clip.xyz / w * c[58] + c[59]) and clip w in oPos.w; undo it. D3D's
    // clip z runs 0..w, GL's -w..w.
    text += "    vec3 scale = mix(c[58].xyz, vec3(1.0), equal(c[58].xyz, vec3(0.0)));\n"
            "    vec3 offset = c[59].xyz;\n"
            "    vec3 clip = (oPos.xyz - offset) / scale * oPos.w;\n"
            "    gl_Position = vec4(clip.x, clip.y, clip.z * 2.0 - oPos.w, oPos.w);\n"
            "    v_diffuse = clamp(oD0, 0.0, 1.0);\n"
            "    v_specular = clamp(oD1, 0.0, 1.0);\n"
            "    v_tex0 = oT0;\n"
            "    v_tex1 = oT1;\n"
            "    v_tex2 = oT2;\n"
            "    v_tex3 = oT3;\n"
            "    v_fog = oFog.x;\n"
            "}\n";
    return text;
}

}  // namespace rexbox::hle::d3d8::vsh
