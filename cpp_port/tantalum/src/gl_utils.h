#pragma once

#include <glbinding/gl32/enum.h>
#include <globjects/base/Instantiator.h>

#include <string>

namespace globjects
{
class NamedString;
class AbstractStringSource;
class Shader;
class Texture;
}  // namespace globjects

enum class ShaderOrigin
{
    FromFile,
    FromString,
};

struct NamedShaderSource : public globjects::Instantiator<NamedShaderSource>
{
    NamedShaderSource(const std::string& name,
                      const std::string& src,
                      bool               from_file);

    NamedShaderSource(const std::string& name,
                      const std::string& src,
                      ShaderOrigin       origin);

    ~NamedShaderSource();

    std::string string() const;

    std::unique_ptr<globjects::NamedString>          named;
    std::unique_ptr<globjects::AbstractStringSource> source;
    bool                                             from_file;
};

struct ManagedShader : public globjects::Instantiator<ManagedShader>
{
    ManagedShader(const std::string& src,
                  ShaderOrigin       origin,
                  gl32::GLenum       type);

    ~ManagedShader();

    globjects::Shader* get();

    std::unique_ptr<globjects::Shader>               shader;
    std::unique_ptr<globjects::AbstractStringSource> source;
    bool                                             from_file;
};

std::unique_ptr<globjects::Texture> load_png(const std::string& filename);
