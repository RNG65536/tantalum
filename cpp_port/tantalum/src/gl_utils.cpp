#include <lodepng.h>

#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <string>

#include "gl_context.h"
//
#include "gl_utils.h"

using std::cerr;
using std::cout;
using std::endl;
using namespace gl;
using namespace glbinding;
using namespace globjects;

std::unique_ptr<globjects::Texture> load_png(const std::string& filename)
{
    unsigned char* buffer;
    unsigned int   width, height;
    lodepng_decode32_file(&buffer, &width, &height, filename.c_str());
    cout << "loaded png " << width << " x " << height << " from " << filename
         << endl;
    auto tex = globjects::Texture::createDefault(GL_TEXTURE_2D);
    tex->image2D(
        0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    return tex;
}

NamedShaderSource::NamedShaderSource(const std::string& name,
                                     const std::string& src,
                                     bool               from_file)
    : from_file(from_file)
{
    if (from_file)
    {
        // source  = globjects::File::create(src);
        source = globjects::Shader::sourceFromFile(src);
    }
    else
    {
        source = globjects::Shader::sourceFromString(src);
    }
    named = globjects::NamedString::create(name, source.get());
}

NamedShaderSource::NamedShaderSource(const std::string& name,
                                     const std::string& src,
                                     ShaderOrigin       origin)
{
    switch (origin)
    {
        case ShaderOrigin::FromFile:
            from_file = true;
            source    = globjects::Shader::sourceFromFile(src);
            break;
        case ShaderOrigin::FromString:
            from_file = false;
            source    = globjects::Shader::sourceFromString(src);
            break;
        default:
            throw std::runtime_error("unknown origin");
            break;
    }

    named = globjects::NamedString::create(name, source.get());
}

NamedShaderSource::~NamedShaderSource()
{
    // MUST first named then source
    named.reset(nullptr);
    source.reset(nullptr);
}

std::string NamedShaderSource::string() const
{
    return source->string();
}

ManagedShader::ManagedShader(const std::string& src,
                             ShaderOrigin       origin,
                             gl32::GLenum       type)
    : from_file(from_file)
{
    switch (origin)
    {
        case ShaderOrigin::FromFile:
            source = globjects::Shader::sourceFromFile(src);
            break;
        case ShaderOrigin::FromString:
            source = globjects::Shader::sourceFromString(src);
            break;
        default:
            throw std::runtime_error("unknown origin");
            break;
    }

    shader = globjects::Shader::create(type, source.get());
    shader->compile();
    if (!shader->isCompiled())
    {
        cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
        cout << source->string() << endl;
        cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;

        auto info = shader->infoLog();
        cout << "info : " << info << endl;
        throw std::runtime_error("cannot compile shader");
    }
}

ManagedShader::~ManagedShader()
{
    // MUST first shader then source
    shader.reset(nullptr);
    source.reset(nullptr);
}

globjects::Shader* ManagedShader::get()
{
    return shader.get();
}
