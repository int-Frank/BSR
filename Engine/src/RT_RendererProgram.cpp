//@group Renderer/RenderThread

/*
  Original Copyright Yan Chernikov <github.com/TheCherno/Hazel-dev> and contributors.

  The following code is a derivative work of the code from the GitHub project 'Hazel-dev',
  which is licensed under: 

                                  Apache License
                             Version 2.0, January 2004
                          http://www.apache.org/licenses

  This code therefore is also licensed under the terms
  of the Apache License Version 2.0

  Copyright 2017-2019 Frank Hart <frankhart010@gmail.com>
*/

//#include <fstream>
#include "glad/glad.h"

#include "RT_RendererProgram.h"
#include "core_ErrorCodes.h"
#include "core_Log.h"
#include "core_utils.h"
#include "core_Assert.h"
#include "DgStringFunctions.h"

//TODO Parse uniform blocks, shader storage blocks

namespace Engine
{
  //--------------------------------------------------------------------------------------------------
  // RT_RendererProgram
  //--------------------------------------------------------------------------------------------------

  RT_RendererProgram::RT_RendererProgram()
    : m_rendererID(0)
    , m_loaded(false)
  {

  }

  RT_RendererProgram::RT_RendererProgram(impl::ResourceID64 a_id)
    : m_rendererID(0)
    , m_loaded(false)
  {
    Init(a_id);
  }

  RT_RendererProgram::~RT_RendererProgram()
  {
    
  }

  void RT_RendererProgram::Destroy()
  {
    if (m_loaded)
    {
      glDeleteProgram(m_rendererID);
      m_rendererID = 0;

      m_shaderData = Ref<ShaderData>();
      m_uniformLocations.clear();

      m_loaded = false;
    }
  }

  void RT_RendererProgram::Bind() const
  {
    if (m_loaded)
      glUseProgram(m_rendererID);
  }

  void RT_RendererProgram::Unbind() const
  {
    glUseProgram(0);
  }

  bool RT_RendererProgram::Init(impl::ResourceID64 a_id)
  {
    Destroy();
    m_shaderData = Ref<ShaderData>(a_id);
    if (m_shaderData.IsNull())
    {
      LOG_WARN("RT_RendererProgram failed to find shader data resource!");
      return false;
    }

    if (!CompileAndUploadShader())
      return false;

    ResolveUniforms();

    m_loaded = true;
    return m_loaded;
  }

  bool RT_RendererProgram::CompileAndUploadShader()
  {
    if (m_shaderData.IsNull())
      return false;

    bool result = true;
    Dg::DynamicArray<GLuint> shaderRendererIDs;

    GLuint program = glCreateProgram();
    for (int i = 0; i < ShaderDomain_COUNT; i++)
    {
      GLenum type = ShaderDomainToOpenGLType(ShaderDomain(i));
      ShaderDomain domain = static_cast<ShaderDomain>(i);
      std::string const & source = m_shaderData->GetShaderSource().Get(domain);

      if (source.empty())
        continue;

      GLuint shaderRendererID = glCreateShader(type);
      GLchar const * sourceCstr = (GLchar const *)source.c_str();
      glShaderSource(shaderRendererID, 1, &sourceCstr, 0);

      glCompileShader(shaderRendererID);

      GLint isCompiled = 0;
      glGetShaderiv(shaderRendererID, GL_COMPILE_STATUS, &isCompiled);
      if (isCompiled == GL_FALSE)
      {
        GLint maxLength = 0;
        glGetShaderiv(shaderRendererID, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shaderRendererID, maxLength, &maxLength, &infoLog[0]);

        LOG_ERROR("Shader compilation failed:\n{0}", &infoLog[0]);

        // We don't need the shader anymore.
        glDeleteShader(shaderRendererID);

        BSR_ASSERT(false, "Failed");
        result = false;
      }

      shaderRendererIDs.push_back(shaderRendererID);
      glAttachShader(program, shaderRendererID);
    }

    // Link our program
    glLinkProgram(program);

    // Note the different functions here: glGetProgram* instead of glGetShader*.
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
    if (isLinked == GL_FALSE)
    {
      GLint maxLength = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

      // The maxLength includes the NULL character
      std::vector<GLchar> infoLog(maxLength);
      glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
      LOG_ERROR("Shader compilation failed:\n{0}", &infoLog[0]);

      // We don't need the program anymore.
      glDeleteProgram(program);
      // Don't leak shaders either.
      for (auto id : shaderRendererIDs)
        glDeleteShader(id);
      result = false;
    }

    // Always detach shaders after a successful link.
    for (auto id : shaderRendererIDs)
      glDetachShader(program, id);

    m_rendererID = program;

    LOG_DEBUG("Successfully created program");

    return result;
  }

  void RT_RendererProgram::ResolveUniforms()
  {
    if (m_shaderData.IsNull())
      return;

    glUseProgram(m_rendererID);

    for (ShaderUniformDeclaration & uniform : m_shaderData->GetUniforms())
       m_uniformLocations.push_back(GetUniformLocation(uniform.GetName()));

    /*uint32_t sampler = 0;
    for (size_t i = 0; i < m_resources.size(); i++)
    {
      ShaderResourceDeclaration* resource = m_resources[i];
      int32_t location = GetUniformLocation(resource->GetName());

      if (resource->GetCount() == 1)
      {
        resource->SetRegister(sampler);
        if (location != -1)
          UploadUniformInt(location, sampler);

        sampler++;
      }
      else if (resource->GetCount() > 1)
      {
        resource->SetRegister(0);
        uint32_t count = resource->GetCount();
        int* samplers = new int[count];
        for (uint32_t s = 0; s < count; s++)
          samplers[s] = s;
        UploadUniformIntArray(resource->GetName(), samplers, count);
        delete[] samplers;
      }
    }*/
  }

  int32_t RT_RendererProgram::GetUniformLocation(std::string const & a_name) const
  {
    int32_t result = glGetUniformLocation(m_rendererID, a_name.c_str());
    if (result == -1)
      LOG_WARN("Could not find uniform '{0}' in shader", a_name.c_str());
    return result;
  }

  void RT_RendererProgram::UploadUniformBuffer(byte const* a_pbuf)
  {
    if (m_shaderData.IsNull() || a_pbuf == nullptr)
      return;

    //TODO should this be here, or do we leave it up to the user to bind before uploading uniforms?
    Bind();

    for (size_t i = 0; i < m_shaderData->GetUniforms().size(); i++)
    {
      ShaderUniformDeclaration const * pdecl = &m_shaderData->GetUniforms()[i];
      uint32_t offset = pdecl->GetDataOffset();
      UniformBufferElementHeader::IntType headerData = 
        *static_cast<UniformBufferElementHeader::IntType const *>(Core::AdvancePtr(a_pbuf, offset));
      UniformBufferElementHeader header(headerData);
      if (header.GetSize() == 0)
        continue;

      void const * buf = Core::AdvancePtr(a_pbuf, offset + sizeof(UniformBufferElementHeader::IntType));
      uint32_t count = header.GetSize() / SizeOfShaderDataType(pdecl->GetType());
      UploadUniform(uint32_t(i), buf, count);
    }
  }

  void RT_RendererProgram::UploadUniformSingle(int a_location, ShaderDataType a_type,  void const* a_pbuf)
  {
    switch (a_type)
    {
      case ShaderDataType::BOOL:
      {
        glUniform1i(a_location, *static_cast<int const*>(a_pbuf));
        break;
      }
      case ShaderDataType::FLOAT:
      {
        glUniform1f(a_location, *static_cast<float const*>(a_pbuf));
        break;
      }
    }
  }

  void RT_RendererProgram::UploadUniformArray(int a_location, ShaderDataType a_type,
                                              void const* a_pbuf, uint32_t a_count)
  {
    switch (a_type)
    {
      case ShaderDataType::BOOL:
      {
        glUniform1iv(a_location, a_count, static_cast<int const*>(a_pbuf));
        break;
      }
      case ShaderDataType::FLOAT:
      {
        glUniform1fv(a_location, a_count, static_cast<float const*>(a_pbuf));
        break;
      }
    }
  }

  void RT_RendererProgram::UploadUniform(uint32_t a_index, void const * a_pbuf, uint32_t a_count)
  {
    ShaderUniformDeclaration const* pdecl = &m_shaderData->GetUniforms()[a_index];
    BSR_ASSERT(a_count <= pdecl->GetCount());

    if (pdecl->IsArray())
      UploadUniformArray(m_uniformLocations[a_index], pdecl->GetType(), a_pbuf, a_count);
    else
      UploadUniformSingle(m_uniformLocations[a_index], pdecl->GetType(), a_pbuf);
  }

  void RT_RendererProgram::UploadUniform(std::string const& a_name, void const* a_pbuf, uint32_t a_size)
  {
    if (m_shaderData.IsNull() || a_pbuf == nullptr)
      return;

    uint32_t index = m_shaderData->FindUniformIndex(a_name);
    if (index == INVALID_INDEX)
    {
      LOG_WARN("Failed to find Uniform '{}'", a_name.c_str());
      return;
    }

    ShaderUniformDeclaration * pdecl = &m_shaderData->GetUniforms()[index];
    uint32_t elementSize = SizeOfShaderDataType(pdecl->GetType());
    uint32_t count = a_size / elementSize;

    //TODO should this be here, or do we leave it up to the user to bind before uploading uniforms?
    Bind();

    UploadUniform(index, a_pbuf, count);
  }
}
