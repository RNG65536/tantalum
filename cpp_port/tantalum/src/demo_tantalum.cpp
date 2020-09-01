
#include <GLFW/glfw3.h>
//
#include "gl_context.h"
//
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <random>
#include <thread>

#include "demo_tantalum.h"
#include "gl_utils.h"
#include "imgui_impl_glfw.h"
#include "tantalum_data.h"

using namespace gl;
using namespace globjects;
using namespace glbinding;
using std::cerr;
using std::cout;
using std::endl;

static const std::string              proj_dir     = PROJECT_DIR;
static const std::string              shader_path  = proj_dir + "/shaders/";
static const std::vector<std::string> include_path = {shader_path};

constexpr float                              M_PI = 3.14159265358979323846f;
static std::default_random_engine            rng;
static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
float                                        randf()
{
    return dist(rng);
}

static TantalumData g_data;

const std::vector<float>& wavelengthToRgbTable()
{
    return g_data.wavelength_to_rgb;
}

const std::vector<GasDischargeLines>& gasDischargeLines()
{
    return g_data.lines;
}

// --------------------------------

static int glTypeSize(GLenum type)
{
    switch (type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return 2;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            return 4;
        default:
            return 0;
    }
}

class TTexture : public globjects::Instantiator<TTexture>
{
public:
    TTexture(int         width,
             int         height,
             int         channels,
             bool        isFloat,
             bool        isLinear,
             bool        isClamped,
             const void* texels)
    {
        GLenum coordMode = isClamped ? GL_CLAMP_TO_EDGE : GL_REPEAT;
        this->type       = isFloat ? GL_FLOAT : GL_UNSIGNED_BYTE;
        GLenum formats[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};
        this->format     = formats[channels - 1];

        GLenum internal_formats[] = {GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F};
        this->internal_format =
            isFloat ? internal_formats[channels - 1] : this->format;

        this->width  = width;
        this->height = height;
        this->size   = glm::ivec2(width, height);

        this->glName = Texture::create(GL_TEXTURE_2D);
        this->glName->bind();
        this->glName->image2D(0,
                              this->internal_format,
                              this->width,
                              this->height,
                              0,
                              this->format,
                              this->type,
                              texels);
        this->glName->setParameter(GL_TEXTURE_WRAP_S, coordMode);
        this->glName->setParameter(GL_TEXTURE_WRAP_T, coordMode);
        this->setSmooth(isLinear);

        this->boundUnit = -1;
    }

    void setSmooth(bool smooth)
    {
        auto interpMode = smooth ? GL_LINEAR : GL_NEAREST;
        this->glName->setParameter(GL_TEXTURE_MIN_FILTER, interpMode);
        this->glName->setParameter(GL_TEXTURE_MAG_FILTER, interpMode);
    }

    void copy(const void* texels)
    {
        this->glName->image2D(0,
                              this->internal_format,
                              this->width,
                              this->height,
                              0,
                              this->format,
                              this->type,
                              texels);
    }

    void bind(int unit)
    {
        this->glName->bindActive(unit);
        this->boundUnit = unit;
    }

    std::unique_ptr<Texture> glName;
    GLenum                   type;
    GLenum                   internal_format;
    GLenum                   format;
    int                      width;
    int                      height;
    glm::ivec2               size;
    int                      boundUnit;
};

class TRenderTarget : public globjects::Instantiator<TRenderTarget>
{
public:
    TRenderTarget()
    {
        this->glName = Framebuffer::create();
    }

    void bind()
    {
        this->glName->bind();
    }

    void unbind()
    {
        this->glName->unbind();
    }

    void attachTexture(TTexture* texture, int index)
    {
        this->glName->attachTexture(
            GL_COLOR_ATTACHMENT0 + index, texture->glName.get(), 0);
    }

    void detachTexture(int index)
    {
        this->glName->detach(GL_COLOR_ATTACHMENT0 + index);
    }

    void drawBuffers(int numBufs)
    {
        std::vector<GLenum> buffers;
        for (int i = 0; i < numBufs; i++)
        {
            buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
        }
        glDrawBuffers(buffers.size(), buffers.data());
    }

    std::unique_ptr<Framebuffer> glName;
};

class TShader : public globjects::Instantiator<TShader>
{
public:
    TShader(const std::string& vert,
            const std::string& frag,
            ShaderOrigin       origin = ShaderOrigin::FromFile)
    {
        this->vertex = ManagedShader::create(vert, origin, GL_VERTEX_SHADER);

        this->fragment =
            ManagedShader::create(frag, origin, GL_FRAGMENT_SHADER);

        this->program = Program::create();

        this->program->attach(this->vertex->get(), this->fragment->get());
        this->program->link();

        if (!this->program->isLinked())
        {
            auto info = this->program->infoLog();
            cout << "info : " << info << endl;
            throw std::runtime_error("cannot link program");
        }
    }

    void bind()
    {
        this->program->use();
    }

    int uniformIndex(const std::string& name)
    {
        if (this->uniforms.find(name) == this->uniforms.end())
            this->uniforms[name] = program->getUniformLocation(name);
        return this->uniforms[name];
    }

    void uniformTexture(const std::string& name, TTexture* texture)
    {
        int id = this->uniformIndex(name);
        if (id != -1)
        {
            this->program->setUniform(id, texture->boundUnit);
        }
    }

    void uniformF(const std::string& name, float f)
    {
        int id = this->uniformIndex(name);
        if (id != -1)
        {
            this->program->setUniform(id, f);
        }
    }

    void uniform2F(const std::string& name, float f1, float f2)
    {
        int id = this->uniformIndex(name);
        if (id != -1)
        {
            this->program->setUniform(id, glm::vec2(f1, f2));
        }
    }

    std::unique_ptr<ManagedShader> vertex;
    std::unique_ptr<ManagedShader> fragment;
    std::unique_ptr<Program>       program;
    std::map<std::string, int>     uniforms;
};

class TVertexBuffer : public globjects::Instantiator<TVertexBuffer>
{
public:
    struct Attribs
    {
        std::string name;
        int         size;
        GLenum      type;
        bool        norm;
        int         offset;
        int         index;
    };

    TVertexBuffer()
    {
        this->elementSize = 0;
    }

    void bind()
    {
        this->glName->bind(GL_ARRAY_BUFFER);
    }

    void addAttribute(const std::string& name, int size, GLenum type, bool norm)
    {
        this->attributes.push_back(
            Attribs{name, size, type, norm, this->elementSize, -1});
        this->elementSize += size * glTypeSize(type);
    }

    void init(int numVerts)
    {
        this->length = numVerts;
        this->glName = Buffer::create();
        this->glName->bind(GL_ARRAY_BUFFER);
        this->glName->setData(
            this->length * this->elementSize, nullptr, GL_STATIC_DRAW);
        this->vao = VertexArray::create();
    }

    void copy(const void* data, int num_bytes)
    {
        if (num_bytes != this->length * this->elementSize)
        {
            throw std::runtime_error(
                "Resizing VBO during copy strongly discouraged");
        }
        this->glName->bind(GL_ARRAY_BUFFER);
        this->glName->setData(num_bytes, data, GL_STATIC_DRAW);
    }

    void draw(TShader* shader, GLenum mode, int length = 0)
    {
        for (int i = 0; i < this->attributes.size(); i++)
        {
            this->attributes[i].index =
                shader->program->getAttributeLocation(this->attributes[i].name);
            if (this->attributes[i].index >= 0)
            {
                auto attr    = this->attributes[i];
                auto binding = vao->binding(i);
                binding->setAttribute(attr.index);
                binding->setBuffer(glName.get(), 0, this->elementSize);
                binding->setFormat(
                    attr.size, attr.type, attr.norm, attr.offset);
                this->vao->enable(attr.index);
            }
        }

        this->vao->drawArrays(mode, 0, length ? length : this->length);

        for (int i = 0; i < this->attributes.size(); i++)
        {
            if (this->attributes[i].index >= 0)
            {
                this->vao->disable(this->attributes[i].index);
                this->attributes[i].index = -1;
            }
        }
    }

    std::unique_ptr<Buffer>      glName;
    std::vector<Attribs>         attributes;
    int                          elementSize = 0;
    int                          length      = 0;
    std::unique_ptr<VertexArray> vao;
};

constexpr float LAMBDA_MIN = 360.0f;
constexpr float LAMBDA_MAX = 750.0f;

class TRayState : public globjects::Instantiator<TRayState>
{
public:
    TRayState(int size)
    {
        this->size = size;

        std::vector<float> posData(size * size * 4);
        std::vector<float> rngData(size * size * 4);
        std::vector<float> rgbData(size * size * 4);

        for (int i = 0; i < size * size; i++)
        {
            float theta        = randf() * M_PI * 2.0f;
            posData[i * 4 + 0] = 0.0f;
            posData[i * 4 + 1] = 0.0f;
            posData[i * 4 + 2] = cos(theta);
            posData[i * 4 + 3] = sin(theta);

            for (int t = 0; t < 4; t++)
            {
                rngData[i * 4 + t] = randf() * 4194167.0f;
            }
            for (int t = 0; t < 4; t++)
            {
                rgbData[i * 4 + t] = 0.0f;
            }
        }

        this->posTex =
            TTexture::create(size, size, 4, true, false, true, posData.data());
        this->rngTex =
            TTexture::create(size, size, 4, true, false, true, rngData.data());
        this->rgbTex =
            TTexture::create(size, size, 4, true, false, true, rgbData.data());
    }

    void bind(TShader* shader)
    {
        this->posTex->bind(0);
        this->rngTex->bind(1);
        this->rgbTex->bind(2);

        shader->uniformTexture("PosData", this->posTex.get());
        shader->uniformTexture("RngData", this->rngTex.get());
        shader->uniformTexture("RgbData", this->rgbTex.get());
    }

    void attach(TRenderTarget* fbo)
    {
        fbo->attachTexture(this->posTex.get(), 0);
        fbo->attachTexture(this->rngTex.get(), 1);
        fbo->attachTexture(this->rgbTex.get(), 2);
    }

    void detach(TRenderTarget* fbo)
    {
        fbo->detachTexture(0);
        fbo->detachTexture(1);
        fbo->detachTexture(2);
    }

    int                       size;
    std::unique_ptr<TTexture> posTex;
    std::unique_ptr<TTexture> rngTex;
    std::unique_ptr<TTexture> rgbTex;
};

enum class ESpectrumType
{
    SPECTRUM_WHITE         = 0,
    SPECTRUM_INCANDESCENT  = 1,
    SPECTRUM_GAS_DISCHARGE = 2,
};

enum class ESpreadType
{
    SPREAD_POINT = 0,
    SPREAD_CONE  = 1,
    SPREAD_BEAM  = 2,
    SPREAD_LASER = 3,
    SPREAD_AREA  = 4,
};

class TRenderer : public globjects::Instantiator<TRenderer>
{
public:
    static constexpr int SPECTRUM_SAMPLES = 256;
    static constexpr int ICDF_SAMPLES     = 1024;

    TRenderer(int width, int height, const std::vector<std::string>& scenes)
    {
        this->quadVbo = createQuadVbo();

        this->maxSampleCount       = 100000;
        this->spreadType           = ESpreadType::SPREAD_POINT;
        this->emissionSpectrumType = ESpectrumType::SPECTRUM_WHITE;
        this->emitterTemperature   = 5000.0;
        this->emitterGas           = 0;
        this->currentScene         = 0;
        this->needsReset           = true;

        this->compositeProgram =
            TShader::create(shader_path + "/compose-vert.glsl",
                            shader_path + "/compose-frag.glsl",
                            ShaderOrigin::FromFile);
        this->passProgram = TShader::create(shader_path + "/compose-vert.glsl",
                                            shader_path + "/pass-frag.glsl",
                                            ShaderOrigin::FromFile);
        this->initProgram = TShader::create(shader_path + "/init-vert.glsl",
                                            shader_path + "/init-frag.glsl",
                                            ShaderOrigin::FromFile);
        this->rayProgram  = TShader::create(shader_path + "/ray-vert.glsl",
                                           shader_path + "ray-frag.glsl",
                                           ShaderOrigin::FromFile);
        this->tracePrograms.clear();
        for (int i = 0; i < scenes.size(); i++)
        {
            this->tracePrograms.emplace_back(
                TShader::create(shader_path + "/trace-vert.glsl",
                                shader_path + scenes[i],
                                ShaderOrigin::FromFile));
        }

        this->maxPathLength = 12;

        this->spectrumTable = wavelengthToRgbTable();
        this->spectrum      = TTexture::create(this->spectrumTable.size() / 4,
                                          1,
                                          4,
                                          true,
                                          true,
                                          true,
                                          this->spectrumTable.data());
        this->emission      = TTexture::create(
            SPECTRUM_SAMPLES, 1, 1, true, false, true, nullptr);
        this->emissionIcdf =
            TTexture::create(ICDF_SAMPLES, 1, 1, true, false, true, nullptr);
        this->emissionPdf = TTexture::create(
            SPECTRUM_SAMPLES, 1, 1, true, false, true, nullptr);

        this->raySize = 512;
        this->resetActiveBlock();
        this->rayCount     = this->raySize * this->raySize;
        this->currentState = 0;

        this->rayStates.emplace_back(TRayState::create(this->raySize));
        this->rayStates.emplace_back(TRayState::create(this->raySize));

        this->rayVbo = TVertexBuffer::create();
        this->rayVbo->addAttribute("TexCoord", 3, GL_FLOAT, false);
        this->rayVbo->init(this->rayCount * 2);

        std::vector<float> vboData(this->rayCount * 2 * 3);
        for (int i = 0; i < this->rayCount; i++)
        {
            float u = float((i % this->raySize) + 0.5f) / this->raySize;
            float v = (floor((float)i / this->raySize) + 0.5f) / this->raySize;
            vboData[i * 6 + 0] = vboData[i * 6 + 3] = u;
            vboData[i * 6 + 1] = vboData[i * 6 + 4] = v;
            vboData[i * 6 + 2]                      = 0.0f;
            vboData[i * 6 + 5]                      = 1.0f;
        }
        this->rayVbo->copy(vboData.data(), vboData.size() * sizeof(float));

        this->fbo = TRenderTarget::create();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glBlendFunc(GL_ONE, GL_ONE);

        this->changeResolution(width, height);
        this->setEmitterPos({width / 2, height / 2}, {width / 2, height / 2});
        this->computeEmissionSpectrum();
    }

    void resetActiveBlock()
    {
        // this->activeBlock = 4;
        this->activeBlock = 64;
        // this->activeBlock = 512;
    }

    void setEmissionSpectrumType(ESpectrumType type)
    {
        this->emissionSpectrumType = type;
        this->computeEmissionSpectrum();
    }

    void setEmitterTemperature(float temperature)
    {
        this->emitterTemperature = temperature;
        if (this->emissionSpectrumType == ESpectrumType::SPECTRUM_INCANDESCENT)
            this->computeEmissionSpectrum();
    }

    void setEmitterGas(int gasId)
    {
        this->emitterGas = gasId;
        if (this->emissionSpectrumType == ESpectrumType::SPECTRUM_GAS_DISCHARGE)
            this->computeEmissionSpectrum();
    }

    void computeEmissionSpectrum()
    {
        this->emissionSpectrum.resize(SPECTRUM_SAMPLES);

        switch (this->emissionSpectrumType)
        {
            case ESpectrumType::SPECTRUM_WHITE:
            {
                for (int i = 0; i < SPECTRUM_SAMPLES; i++)
                {
                    this->emissionSpectrum[i] = 1.0f;
                }
            }
            break;
            case ESpectrumType::SPECTRUM_INCANDESCENT:
            {
                float h  = 6.626070040e-34;
                float c  = 299792458.0;
                float kB = 1.3806488e-23;
                float T  = this->emitterTemperature;

                for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
                {
                    float l = (LAMBDA_MIN + (LAMBDA_MAX - LAMBDA_MIN) *
                                                (i + 0.5) / SPECTRUM_SAMPLES) *
                              1e-9;
                    float power =
                        1e-12 * (2.0 * h * c * c) /
                        (l * l * l * l * l * (exp(h * c / (l * kB * T)) - 1.0));

                    this->emissionSpectrum[i] = power;
                }
            }
            break;
            case ESpectrumType::SPECTRUM_GAS_DISCHARGE:
            {
                auto& wavelengths =
                    gasDischargeLines()[this->emitterGas].wavelengths;
                auto& strengths =
                    gasDischargeLines()[this->emitterGas].strengths;

                for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
                    this->emissionSpectrum[i] = 0.0;

                for (int i = 0; i < wavelengths.size(); ++i)
                {
                    int idx =
                        floor((wavelengths[i] - LAMBDA_MIN) /
                              (LAMBDA_MAX - LAMBDA_MIN) * SPECTRUM_SAMPLES);
                    if (idx < 0 || idx >= SPECTRUM_SAMPLES) continue;

                    this->emissionSpectrum[idx] += strengths[i];
                }
            }
            break;
            default:
                throw std::runtime_error("unknown type");
                break;
        }

        this->computeSpectrumIcdf();

        this->emission->bind(0);
        this->emission->copy(this->emissionSpectrum.data());
        this->reset();
    }

    void computeSpectrumIcdf()
    {
        if (this->cdf.empty())
        {
            this->cdf.resize(SPECTRUM_SAMPLES + 1);
            this->pdf.resize(SPECTRUM_SAMPLES);
            this->icdf.resize(ICDF_SAMPLES);
        }

        float sum = 0.0;
        for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
            sum += this->emissionSpectrum[i];

        /* Mix in 10% of a uniform sample distribution to stay on the safe side.
           Especially gas emission spectra with lots of emission lines
           tend to have small peaks that fall through the cracks otherwise */
        float safetyPadding = 0.1;
        float normalization = SPECTRUM_SAMPLES / sum;

        /* Precompute cdf and pdf (unnormalized for now) */
        this->cdf[0] = 0.0;
        for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
        {
            this->emissionSpectrum[i] *= normalization;

            /* Also take into account the observer response when distributing
               samples.
               Otherwise tends to prioritize peaks just barely outside the
               visible spectrum */
            float observerResponse =
                (1.0 / 3.0) * (abs(this->spectrumTable[i * 4]) +
                               abs(this->spectrumTable[i * 4 + 1]) +
                               abs(this->spectrumTable[i * 4 + 2]));

            this->pdf[i] = observerResponse *
                           (this->emissionSpectrum[i] + safetyPadding) /
                           (1.0 + safetyPadding);
            this->cdf[i + 1] = this->pdf[i] + this->cdf[i];
        }

        /* All done! Time to normalize */
        float cdfSum = this->cdf[SPECTRUM_SAMPLES];
        for (int i = 0; i < SPECTRUM_SAMPLES; ++i)
        {
            this->pdf[i] *= SPECTRUM_SAMPLES / cdfSum;
            this->cdf[i + 1] /= cdfSum;
        }
        /* Make sure we don't fall into any floating point pits */
        this->cdf[SPECTRUM_SAMPLES] = 1.0;

        /* Precompute an inverted mapping of the cdf. This is biased!
           Unfortunately we can't really afford to do runtime bisection
           on the GPU, so this will have to do. For our purposes a small
           amount of bias is tolerable anyway. */
        int cdfIdx = 0;
        for (int i = 0; i < ICDF_SAMPLES; ++i)
        {
            float target = std::min(float(i + 1) / ICDF_SAMPLES, 1.0f);
            while (this->cdf[cdfIdx] < target) cdfIdx++;
            this->icdf[i] = (cdfIdx - 1.0) / SPECTRUM_SAMPLES;
        }

        this->emissionIcdf->bind(0);
        this->emissionIcdf->copy(this->icdf.data());
        this->emissionPdf->bind(0);
        this->emissionPdf->copy(this->pdf.data());
    }

    std::vector<float>& getEmissionSpectrum()
    {
        return this->emissionSpectrum;
    }

    void setMaxPathLength(int length)
    {
        this->maxPathLength = length;
        this->reset();
    }

    void setMaxSampleCount(int count)
    {
        this->maxSampleCount = count;
    }

    void changeResolution(int width, int height)
    {
        if (this->width && this->height)
        {
            this->emitterPos[0] =
                (this->emitterPos[0] + 0.5) * width / this->width - 0.5;
            this->emitterPos[1] =
                (this->emitterPos[1] + 0.5) * height / this->height - 0.5;
        }

        this->width  = width;
        this->height = height;
        this->aspect = float(this->width) / float(this->height);

        this->screenBuffer = TTexture::create(
            this->width, this->height, 4, true, false, true, nullptr);
        this->waveBuffer = TTexture::create(
            this->width, this->height, 4, true, false, true, nullptr);

        this->resetActiveBlock();
        this->reset();
    }

    void changeScene(int idx)
    {
        this->resetActiveBlock();
        this->currentScene = idx;
        this->reset();
    }

    void reset()
    {
        if (!this->needsReset) return;
        this->needsReset    = false;
        this->wavesTraced   = 0;
        this->raysTraced    = 0;
        this->samplesTraced = 0;
        this->pathLength    = 0;
        this->elapsedTimes.clear();

        this->fbo->bind();
        this->fbo->drawBuffers(1);
        this->fbo->attachTexture(this->screenBuffer.get(), 0);
        glClear(GL_COLOR_BUFFER_BIT);
        this->fbo->unbind();
    }

    void setSpreadType(ESpreadType type)
    {
        this->resetActiveBlock();
        this->spreadType = type;
        this->computeSpread();
        this->reset();
    }

    void setNormalizedEmitterPos(const glm::vec2& posA, const glm::vec2& posB)
    {
        this->setEmitterPos({posA[0] * this->width, posA[1] * this->height},
                            {posB[0] * this->width, posB[1] * this->height});
    }

    void setEmitterPos(const glm::vec2& posA, const glm::vec2& posB)
    {
        this->emitterPos =
            this->spreadType == ESpreadType::SPREAD_POINT ? posB : posA;
        this->emitterAngle = this->spreadType == ESpreadType::SPREAD_POINT
                                 ? 0.0
                                 : atan2(posB[1] - posA[1], posB[0] - posA[0]);
        this->computeSpread();
        this->reset();
    }

    void computeSpread()
    {
        switch (this->spreadType)
        {
            case ESpreadType::SPREAD_POINT:
                this->emitterPower  = 0.1;
                this->spatialSpread = 0.0;
                this->angularSpread = {0.0, M_PI * 2.0};
                break;
            case ESpreadType::SPREAD_CONE:
                this->emitterPower  = 0.03;
                this->spatialSpread = 0.0;
                this->angularSpread = {this->emitterAngle, M_PI * 0.3};
                break;
            case ESpreadType::SPREAD_BEAM:
                this->emitterPower  = 0.03;
                this->spatialSpread = 0.4;
                this->angularSpread = {this->emitterAngle, 0.0};
                break;
            case ESpreadType::SPREAD_LASER:
                this->emitterPower  = 0.05;
                this->spatialSpread = 0.0;
                this->angularSpread = {this->emitterAngle, 0.0};
                break;
            case ESpreadType::SPREAD_AREA:
                this->emitterPower  = 0.1;
                this->spatialSpread = 0.4;
                this->angularSpread = {this->emitterAngle, M_PI};
                break;
            default:
                throw std::runtime_error("unknown spread type");
                break;
        }
    }

    std::unique_ptr<TVertexBuffer> createQuadVbo()
    {
        auto vbo = TVertexBuffer::create();

        vbo->addAttribute("Position", 3, GL_FLOAT, false);
        vbo->addAttribute("TexCoord", 2, GL_FLOAT, false);
        vbo->init(4);
        float data[] = {1.0,  1.0,  0.0, 1.0, 1.0, -1.0, 1.0,  0.0, 0.0, 1.0,
                        -1.0, -1.0, 0.0, 0.0, 0.0, 1.0,  -1.0, 0.0, 1.0, 0.0};
        vbo->copy(data, sizeof(data));

        return vbo;
    }

    int totalRaysTraced()
    {
        return this->raysTraced;
    }

    int maxRayCount()
    {
        return this->maxPathLength * this->maxSampleCount;
    }

    int totalSamplesTraced()
    {
        return this->samplesTraced;
    }

    float progress()
    {
        return std::min(
            float(this->totalRaysTraced()) / float(this->maxRayCount()), 1.0f);
    }

    bool finished()
    {
        return this->totalSamplesTraced() >= this->maxSampleCount;
    }

    void composite()
    {
        this->screenBuffer->bind(0);
        this->compositeProgram->bind();
        this->compositeProgram->uniformTexture("Frame",
                                               this->screenBuffer.get());
        this->compositeProgram->uniformF(
            "Exposure",
            this->width / float(std::max(this->samplesTraced,
                                         this->raySize * this->activeBlock)));
        this->quadVbo->draw(this->compositeProgram.get(), GL_TRIANGLE_FAN);
    }

    void render(float timestamp)
    {
        this->needsReset = true;
        this->elapsedTimes.push_back(timestamp);

        int current = this->currentState;
        int next    = 1 - current;

        this->fbo->bind();

        glViewport(0, 0, this->raySize, this->raySize);
        glScissor(0, 0, this->raySize, this->activeBlock);
        glEnable(GL_SCISSOR_TEST);
        this->fbo->drawBuffers(3);
        this->rayStates[next]->attach(this->fbo.get());
        this->quadVbo->bind();

        if (this->pathLength == 0)
        {
            this->initProgram->bind();
            this->rayStates[current]->rngTex->bind(0);
            this->spectrum->bind(1);
            this->emission->bind(2);
            this->emissionIcdf->bind(3);
            this->emissionPdf->bind(4);
            this->initProgram->uniformTexture(
                "RngData", this->rayStates[current]->rngTex.get());
            this->initProgram->uniformTexture("Spectrum", this->spectrum.get());
            this->initProgram->uniformTexture("Emission", this->emission.get());
            this->initProgram->uniformTexture("ICDF", this->emissionIcdf.get());
            this->initProgram->uniformTexture("PDF", this->emissionPdf.get());
            this->initProgram->uniform2F(
                "EmitterPos",
                ((this->emitterPos[0] / this->width) * 2.0 - 1.0) *
                    this->aspect,
                1.0 - (this->emitterPos[1] / this->height) * 2.0);
            this->initProgram->uniform2F("EmitterDir",
                                         cos(this->angularSpread[0]),
                                         -sin(this->angularSpread[0]));
            this->initProgram->uniformF("EmitterPower", this->emitterPower);
            this->initProgram->uniformF("SpatialSpread", this->spatialSpread);
            this->initProgram->uniform2F("AngularSpread",
                                         -this->angularSpread[0],
                                         this->angularSpread[1]);
            this->quadVbo->draw(this->initProgram.get(), GL_TRIANGLE_FAN);

            current = 1 - current;
            next    = 1 - next;
            this->rayStates[next]->attach(this->fbo.get());
        }

        auto traceProgram = this->tracePrograms[this->currentScene].get();
        traceProgram->bind();
        this->rayStates[current]->bind(traceProgram);
        this->quadVbo->draw(traceProgram, GL_TRIANGLE_FAN);

        this->rayStates[next]->detach(this->fbo.get());

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, this->width, this->height);

        this->fbo->drawBuffers(1);
        this->fbo->attachTexture(this->waveBuffer.get(), 0);

        if (this->pathLength == 0 || this->wavesTraced == 0)
            glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_BLEND);

        this->rayProgram->bind();
        this->rayStates[current]->posTex->bind(0);
        this->rayStates[next]->posTex->bind(1);
        this->rayStates[current]->rgbTex->bind(2);
        this->rayProgram->uniformTexture(
            "PosDataA", this->rayStates[current]->posTex.get());
        this->rayProgram->uniformTexture("PosDataB",
                                         this->rayStates[next]->posTex.get());
        this->rayProgram->uniformTexture(
            "RgbData", this->rayStates[current]->rgbTex.get());
        this->rayProgram->uniformF("Aspect", this->aspect);
        this->rayVbo->bind();
        this->rayVbo->draw(this->rayProgram.get(),
                           GL_LINES,
                           this->raySize * this->activeBlock * 2);

        this->raysTraced += this->raySize * this->activeBlock;
        this->pathLength += 1;

        this->quadVbo->bind();

        if (this->pathLength == this->maxPathLength || this->wavesTraced == 0)
        {
            this->fbo->attachTexture(this->screenBuffer.get(), 0);

            this->waveBuffer->bind(0);
            this->passProgram->bind();
            this->passProgram->uniformTexture("Frame", this->waveBuffer.get());
            this->quadVbo->draw(this->passProgram.get(), GL_TRIANGLE_FAN);

            if (this->pathLength == this->maxPathLength)
            {
                this->samplesTraced += this->raySize * this->activeBlock;
                this->wavesTraced += 1;
                this->pathLength = 0;

#if 1
                if (this->elapsedTimes.size() > 5)
                {
                    float avgTime = 0;
                    for (int i = 1; i < this->elapsedTimes.size(); ++i)
                        avgTime +=
                            this->elapsedTimes[i] - this->elapsedTimes[i - 1];
                    avgTime /= this->elapsedTimes.size() - 1;

                    /* Let's try to stay at reasonable frame times. Targeting
                       16ms is a bit tricky because there's a lot of variability
                       in how often the browser executes this loop and 16ms
                       might well not be reachable, but 24ms seems to do ok */
                    if (avgTime > 24.0)
                        this->activeBlock = std::max(4, this->activeBlock - 4);
                    else
                        this->activeBlock =
                            std::min(512, this->activeBlock + 4);

                    this->elapsedTimes = {this->elapsedTimes.back()};
                }
#endif
            }
        }

        glDisable(GL_BLEND);

        this->fbo->unbind();

        this->composite();

        this->currentState = next;
    }

    std::unique_ptr<TVertexBuffer> quadVbo;
    ESpreadType                    spreadType;
    ESpectrumType                  emissionSpectrumType;
    float                          emitterTemperature;
    int                            emitterGas;
    int                            currentScene;
    bool                           needsReset;

    std::unique_ptr<TShader>              compositeProgram;
    std::unique_ptr<TShader>              passProgram;
    std::unique_ptr<TShader>              initProgram;
    std::unique_ptr<TShader>              rayProgram;
    std::vector<std::unique_ptr<TShader>> tracePrograms;

    std::vector<float>        spectrumTable;
    std::unique_ptr<TTexture> spectrum;
    std::unique_ptr<TTexture> emission;
    std::unique_ptr<TTexture> emissionIcdf;
    std::unique_ptr<TTexture> emissionPdf;
    std::vector<float>        emissionSpectrum;

    std::vector<float> cdf;
    std::vector<float> pdf;
    std::vector<float> icdf;

    int maxSampleCount;
    int maxPathLength;
    int raySize;
    int rayCount;
    int wavesTraced;
    int raysTraced;
    int samplesTraced;

    int                                     currentState;
    std::vector<std::unique_ptr<TRayState>> rayStates;

    int                pathLength;
    std::vector<float> elapsedTimes;
    glm::vec2          emitterPos;
    float              emitterAngle;
    float              emitterPower;
    float              spatialSpread;
    glm::vec2          angularSpread;

    std::unique_ptr<TVertexBuffer> rayVbo;
    std::unique_ptr<TRenderTarget> fbo;
    std::unique_ptr<TTexture>      screenBuffer;
    std::unique_ptr<TTexture>      waveBuffer;

    int activeBlock;

    int   width;
    int   height;
    float aspect;
};

// class TSpectrumRenderer : public globjects::Instantiator<TSpectrumRenderer>
//{
// public:
//    TSpectrumRenderer(const std::vector<float>& spectrum)
//    {
//        this->spectrum = spectrum;
//        this->smooth   = true;
//
//        this->spectrumFill     = new Image();
//        this->spectrumFill.src = 'Spectrum.png';
//        this->spectrumFill.addEventListener('load',
//        this->loadPattern.bind(this)); if (this->spectrumFill.complete)
//        this->loadPattern();
//        m_texture = load_png(proj_dir + "/data/Spectrum.png");
//    }
//
//    std::vector<float> spectrum;
//    bool               smooth;
//    std::unique_ptr<globjects::Texture> m_texture;
//};

class TSceneInfo
{
public:
    std::string shader;
    std::string name;
    glm::vec2   posA;
    glm::vec2   posB;
    ESpreadType spread;
};

class Tantalum : public globjects::Instantiator<Tantalum>
{
public:
    void drawGUI()
    {
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiSetCond_Once);
        ImGui::Begin("");
        // ImGui::Text("Running %.1f FPS", ImGui::GetIO().Framerate);
        int L0 = rays_traced % 1000;
        int L1 = (rays_traced % 1000000) / 1000;
        int L2 = (rays_traced % 1000000000) / 1000000;
        int L3 = (rays_traced / 1000000000);
        if (L3 > 0)
            ImGui::Text("Rays traced : %d,%03d,%03d,%03d", L3, L2, L1, L0);
        else
            ImGui::Text("Rays traced : %d,%03d,%03d", L2, L1, L0);
        ImGui::Text("Resolution : %d x %d",
                    this->renderer->width,
                    this->renderer->height);

        // scene
        static int scene = 0;
        ImGui::Text("Scene:");
        ImGui::RadioButton(scene_infos[0].name.c_str(), &scene, 0);
        ImGui::RadioButton(scene_infos[5].name.c_str(), &scene, 5);
        ImGui::RadioButton(scene_infos[6].name.c_str(), &scene, 6);
        ImGui::RadioButton(scene_infos[3].name.c_str(), &scene, 3);
        ImGui::RadioButton(scene_infos[4].name.c_str(), &scene, 4);
        ImGui::RadioButton(scene_infos[2].name.c_str(), &scene, 2);
        ImGui::RadioButton(scene_infos[1].name.c_str(), &scene, 1);
        if (scene != scene_idx)
        {
            this->selectScene(scene);
        }

        // spread
        static int spread = 0;
        ImGui::Text("Spread:");
        ImGui::RadioButton("Point", &spread, 0);
        ImGui::RadioButton("Cone", &spread, 1);
        ImGui::RadioButton("Beam", &spread, 2);
        ImGui::RadioButton("Laser", &spread, 3);
        ImGui::RadioButton("Area", &spread, 4);
        if (spread != spread_idx)
        {
            this->setSpreadType((ESpreadType)spread);
        }

        static int length = max_path_length;
        ImGui::Text("Light Path Length:");
        ImGui::SliderInt(" ", &max_path_length, 1, 20);
        if (length != max_path_length)
        {
            length = max_path_length;
            this->setMaxPathLength(length);
        }

        static bool inf = true;
        ImGui::Checkbox("Inf", &inf);

        if (ImGui::Button("Reset")) this->renderer->reset();

        ImGui::End();
    }

    Tantalum(int width, int height)
    {
        initialize(width, height);
    }

    ~Tantalum()
    {
    }

    void draw(int width, int height)
    {
        auto cur = std::chrono::steady_clock::now();
        auto dur = cur - last;

        this->renderer->render(
            std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());

        last = cur;

        rays_traced = this->renderer->totalRaysTraced();
        // cout << std::min(rays_traced, (size_t)this->renderer->maxRayCount())
        // << "/"
        //     << this->renderer->maxRayCount() << " rays traced" << endl;
    }

    void setMaxSampleCount(int exponent100)
    {
        int sampleCount = floor(pow(10, exponent100 * 0.01));
        this->renderer->setMaxSampleCount(sampleCount);
    }

    void setEmitterPos(const glm::vec2& a, const glm::vec2& b)
    {
        this->renderer->setEmitterPos(a, b);
    }

    void setSpreadType(ESpreadType type)
    {
        this->renderer->setSpreadType(type);
        spread_idx = (int)type;
    }

    void setMaxPathLength(int length)
    {
        max_path_length = length;
        this->renderer->setMaxPathLength(length);
    }

    void selectScene(size_t idx)
    {
        scene_idx = idx % this->scene_infos.size();
        this->renderer->changeScene(scene_idx);
        this->renderer->setSpreadType(scene_infos[scene_idx].spread);
        this->renderer->setNormalizedEmitterPos(scene_infos[scene_idx].posA,
                                                scene_infos[scene_idx].posB);
    }

    void changeResolution(int w, int h)
    {
        this->renderer->changeResolution(w, h);
    }

    const std::string& getSceneName() const
    {
        return scene_infos[scene_idx].name;
    }

protected:
    void initialize(int width, int height)
    {
        // headers (these must be loaded)
        bsdf = NamedShaderSource::create(
            "/bsdf.glsl", shader_path + "/bsdf.glsl", ShaderOrigin::FromFile);

        csg_intersect =
            NamedShaderSource::create("/csg-intersect.glsl",
                                      shader_path + "/csg-intersect.glsl",
                                      ShaderOrigin::FromFile);

        intersect = NamedShaderSource::create("/intersect.glsl",
                                              shader_path + "/intersect.glsl",
                                              ShaderOrigin::FromFile);

        preamble = NamedShaderSource::create("/preamble.glsl",
                                             shader_path + "/preamble.glsl",
                                             ShaderOrigin::FromFile);

        rand = NamedShaderSource::create(
            "/rand.glsl", shader_path + "/rand.glsl", ShaderOrigin::FromFile);

        trace_vert_named =
            NamedShaderSource::create("/trace-vert.glsl",
                                      shader_path + "/trace-vert.glsl",
                                      ShaderOrigin::FromFile);
        trace_frag_named =
            NamedShaderSource::create("/trace-frag.glsl",
                                      shader_path + "/trace-frag.glsl",
                                      ShaderOrigin::FromFile);

        test_float_texture_support();

        std::vector<std::string> scenes = {
            "/scene1.glsl",
            "/scene2.glsl",
            "/scene3.glsl",
            "/scene4.glsl",
            "/scene5.glsl",
            "/scene6.glsl",
            "/scene7.glsl",
        };

        this->resolution = glm::ivec2(width, height);
        this->renderer =
            TRenderer::create(this->resolution.x, this->resolution.y, scenes);

        this->setMaxSampleCount(700);

        max_path_length = 12;
        this->renderer->setMaxPathLength(max_path_length);

        scene_idx = 0;
        this->renderer->changeScene(scene_idx);

        this->renderer->setNormalizedEmitterPos({0.5f, 0.5f}, {0.5f, 0.5f});

        auto map = [](float a, float b) {
            return glm::vec2(a * 0.5 / 1.78 + 0.5, -b * 0.5 + 0.5);
        };

        scene_infos = {{"scene1",
                        "Lenses",
                        {0.5, 0.5},
                        {0.5, 0.5},
                        ESpreadType::SPREAD_POINT},
                       {"scene2",
                        "Rough Mirror Spheres",
                        {0.25, 0.125},
                        {0.5, 0.66},
                        ESpreadType::SPREAD_LASER},
                       {"scene3",
                        "Cornell Box",
                        {0.5, 0.101},
                        {0.5, 0.2},
                        ESpreadType::SPREAD_AREA},
                       {"scene4",
                        "Prism",
                        {0.1, 0.65},
                        {0.4, 0.4},
                        ESpreadType::SPREAD_LASER},
                       {"scene5",
                        "Cardioid",
                        {0.2, 0.5},
                        {0.2, 0.5},
                        ESpreadType::SPREAD_POINT},
                       {"scene6",
                        "Spheres",
                        map(-1.59, 0.65),
                        map(0.65, -0.75),
                        ESpreadType::SPREAD_BEAM},
                       {"scene7",
                        "Playground",
                        {0.3, 0.52},
                        {0.3, 0.52},
                        ESpreadType::SPREAD_POINT}};
    }

    void test_float_texture_support()
    {
        //
        auto shader = TShader::create(shader_path + "/blend-test-vert.glsl",
                                      shader_path + "/blend-test-frag.glsl",
                                      ShaderOrigin::FromFile);
        auto packShader =
            TShader::create(shader_path + "/blend-test-vert.glsl",
                            shader_path + "/blend-test-pack-frag.glsl",
                            ShaderOrigin::FromFile);
        float tex_data[] = {-6.0, 10.0, 30.0, 2.0};
        auto  target = TTexture::create(1, 1, 4, true, false, false, tex_data);
        auto  fbo    = TRenderTarget::create();
        auto  vbo    = TVertexBuffer::create();
        vbo->addAttribute("Position", 3, GL_FLOAT, false);
        vbo->init(4);
        vbo->bind();
        float vert_data[] = {
            1.0, 1.0, 0.0, -1.0, 1.0, 0.0, -1.0, -1.0, 0.0, 1.0, -1.0, 0.0};
        vbo->copy(vert_data, sizeof(vert_data));

        glViewport(0, 0, 1, 1);

        fbo->bind();
        fbo->drawBuffers(1);
        fbo->attachTexture(target.get(), 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        shader->bind();
        vbo->draw(shader.get(), GL_TRIANGLE_FAN, 0);
        vbo->draw(shader.get(), GL_TRIANGLE_FAN, 0);

        fbo->unbind();
        glDisable(GL_BLEND);

        packShader->bind();
        target->bind(0);
        packShader->uniformTexture("Tex", target.get());
        vbo->draw(packShader.get(), GL_TRIANGLE_FAN);

        uint8_t pixels[4] = {0, 0, 0, 0};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        if (pixels[0] != 8 || pixels[1] != 128 || pixels[2] != 16 ||
            pixels[3] != 4)
        {
            cout << "Floating point blending test failed. Result was " << pixels
                 << " but should have been 8, 128, 16, 4" << endl;
            throw std::runtime_error(
                "Your platform does not support floating point attachments");
        }
    }

protected:
    std::unique_ptr<NamedShaderSource> bsdf;
    std::unique_ptr<NamedShaderSource> csg_intersect;
    std::unique_ptr<NamedShaderSource> intersect;
    std::unique_ptr<NamedShaderSource> preamble;
    std::unique_ptr<NamedShaderSource> rand;
    std::unique_ptr<NamedShaderSource> trace_frag_named;
    std::unique_ptr<NamedShaderSource> trace_vert_named;

    glm::ivec2                 resolution;
    std::unique_ptr<TRenderer> renderer;

    std::chrono::steady_clock::time_point last;

    std::vector<TSceneInfo> scene_infos;

    size_t scene_idx       = 0;
    size_t spread_idx      = 0;
    int    max_path_length = 0;
    size_t rays_traced     = 0;
};

class TMouseListener : public globjects::Instantiator<TMouseListener>
{
public:
    TMouseListener(
        const std::function<void(const glm::vec2&, const glm::vec2&)>& callback)
    {
        this->callback = callback;
    }

    void mouseDown(const Event& evt)
    {
        this->mouseStart = this->mapMouseEvent(evt);
        this->callback(this->mouseStart, this->mouseStart);
    }

    void mouseUp(const Event& evt)
    {
    }

    void mouseMove(const Event& evt)
    {
        this->callback(this->mouseStart, this->mapMouseEvent(evt));
    }

    glm::vec2 mapMouseEvent(const Event& evt)
    {
        return glm::vec2(evt.x, evt.y);
    }

    std::function<void(const glm::vec2&, const glm::vec2&)> callback;
    glm::vec2                                               mouseStart;
};

class TKeyboardListener : public globjects::Instantiator<TKeyboardListener>
{
public:
    TKeyboardListener(const std::function<void(int)>& callback)
    {
        this->callback = callback;
    }

    void keyDown(const Event& evt)
    {
        this->callback(evt.key);
    }

    void keyUp(const Event& evt)
    {
    }

    std::function<void(int)> callback;
};

namespace
{
auto                               g_size = glm::ivec2{};
std::unique_ptr<Tantalum>          g_tant;
std::unique_ptr<TMouseListener>    g_mouse;
std::unique_ptr<TKeyboardListener> g_keyboard;
int                                g_scene  = 0;
int                                g_length = 0;
}  // namespace

DemoTantalum::DemoTantalum()
{
}

void DemoTantalum::onDraw()
{
    glViewport(0, 0, g_size.x, g_size.y);

    g_tant->draw(g_size.x, g_size.y);

    Buffer::unbind(GL_ARRAY_BUFFER);
    Program::release();
    VertexArray::unbind();

    ImGui_ImplGlfw_NewFrame();
    g_tant->drawGUI();
    ImGui::Render();
}

void DemoTantalum::onInitialize(int width, int height)
{
    g_size = glm::ivec2(width, height);

    g_tant = Tantalum::create(width, height);
    g_mouse =
        TMouseListener::create([](const glm::vec2& a, const glm::vec2& b) {
            return g_tant->setEmitterPos(a, b);
        });
    g_keyboard = TKeyboardListener::create([](int key) {
        switch (key)
        {
            case GLFW_KEY_1:
                g_tant->setSpreadType(ESpreadType::SPREAD_POINT);
                break;
            case GLFW_KEY_2:
                g_tant->setSpreadType(ESpreadType::SPREAD_CONE);
                break;
            case GLFW_KEY_3:
                g_tant->setSpreadType(ESpreadType::SPREAD_BEAM);
                break;
            case GLFW_KEY_4:
                g_tant->setSpreadType(ESpreadType::SPREAD_LASER);
                break;
            case GLFW_KEY_5:
                g_tant->setSpreadType(ESpreadType::SPREAD_AREA);
                break;
            case GLFW_KEY_Z:
                g_scene -= 1;
                g_tant->selectScene(g_scene);
                break;
            case GLFW_KEY_X:
                g_scene += 1;
                g_tant->selectScene(g_scene);
                break;
            case GLFW_KEY_A:
                g_length -= 1;
                g_tant->setMaxPathLength(g_length);
                break;
            case GLFW_KEY_S:
                g_length += 1;
                g_tant->setMaxPathLength(g_length);
                break;
            default:
                break;
        }
    });

    cout << "initialized" << endl;
}

void DemoTantalum::onDeinitialize()
{
    g_tant.reset(nullptr);
    g_mouse.reset(nullptr);
    g_keyboard.reset(nullptr);
}

void DemoTantalum::onReshape(int width, int height)
{
    this->GlfwWindow::onReshape(width, height);
    g_tant->changeResolution(width, height);
}

static int prevX, prevY;
static int cursorX, cursorY;
static int cur_botton = -1;

int DemoTantalum::handle(const Event& e)
{
    switch (e.type)
    {
        case EventType::PushEvent:
            prevX = e.x;
            prevY = e.y;
            if (e.button == GLFW_MOUSE_BUTTON_RIGHT)
            {
                g_mouse->mouseDown(e);
            }
            return 1;
        case EventType::DragEvent:
            if (e.button == GLFW_MOUSE_BUTTON_RIGHT)
            {
                g_mouse->mouseMove(e);
            }
            else if (e.button == GLFW_MOUSE_BUTTON_LEFT)
            {
            }
            prevX = e.x;
            prevY = e.y;

            return 1;
        case EventType::MouseWheelEvent:
        {
        }
            return 1;

        case EventType::KeyboardEvent:
            g_keyboard->keyDown(e);
        default:
            return 0;
    }
    return 0;
}
