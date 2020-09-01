#pragma once

#define USE_GLAD 0

#if USE_GLAD
#include <glad/gl.h>
#else
#include <glbinding-aux/ContextInfo.h>
#include <glbinding-aux/Meta.h>
#include <glbinding-aux/ValidVersions.h>
#include <glbinding-aux/types_to_string.h>
#include <glbinding/AbstractFunction.h>
#include <glbinding/Binding.h>
#include <glbinding/CallbackMask.h>
#include <glbinding/FunctionCall.h>
#include <glbinding/Version.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#include <globjects/Buffer.h>
#include <globjects/Framebuffer.h>
#include <globjects/FramebufferAttachment.h>
#include <globjects/NamedString.h>
#include <globjects/Program.h>
#include <globjects/ProgramPipeline.h>
#include <globjects/Renderbuffer.h>
#include <globjects/Shader.h>
#include <globjects/Texture.h>
#include <globjects/VertexArray.h>
#include <globjects/VertexAttributeBinding.h>
#include <globjects/base/AbstractStringSource.h>
#include <globjects/base/File.h>
#include <globjects/base/Instantiator.h>
#include <globjects/base/StaticStringSource.h>
#include <globjects/base/StringTemplate.h>
#include <globjects/globjects.h>
#include <globjects/logging.h>
//using namespace gl;
//using namespace glbinding;
//using namespace globjects;
#endif

#include <glm/vec2.hpp>
