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
#include <regex>

#include "glad/glad.h"

#include "RT_RendererProgram.h"
#include "core_ErrorCodes.h"
#include "core_Log.h"
#include "core_Assert.h"
#include "DgStringFunctions.h"

//TODO Parse uniform blocks, shader storage blocks

//Helpful regex expressions
#define REG_UNIFORM "(?:uniform)"
#define REG_STRUCT "(?:struct)"
#define _OS_ "[\\s\\n\\r]*"
#define _S_ "[\\s\\n\\r]+"
#define VAR "([_a-zA-Z][_a-zA-Z0-9]*)"
#define OARRAY "(?:(?:\\[)" _OS_ "([0-9]+)" _OS_ "(?:\\]))?"
#define SC "[;]"

#define STD140_DECL "(?:layout)" _OS_ "[(]" _OS_ "(?:std140)" _OS_ "[)]"
#define BLOCK_CONTENTS "(?:[^{]*)[{]([^}]*)"

#define UNIFORM_BLOCK_EXPRESSION STD140_DECL _OS_ REG_UNIFORM _S_ VAR BLOCK_CONTENTS

#define VAR_EXPRESSION                            VAR _S_ VAR _OS_ OARRAY _OS_ SC
#define UNIFORM_VAR_EXPRESSION    REG_UNIFORM _S_ VAR _S_ VAR _OS_ OARRAY _OS_ SC
#define STRUCT_EXPRESSION         REG_STRUCT  _S_ VAR BLOCK_CONTENTS

namespace Engine
{
  //--------------------------------------------------------------------------------------------------
  // Parsing helper functions
  //--------------------------------------------------------------------------------------------------
  
  struct varDecl
  {
    std::string type;
    std::string name;
    uint32_t    count;
  };

  typedef Dg::DynamicArray<varDecl> varDeclList;

  static varDeclList __FindDecls(std::string const& a_str, char const * a_regex)
  {
    varDeclList result;
    std::string subject = a_str;

    std::smatch match;
    std::regex r(a_regex);
    while (regex_search(subject, match, r))
    {
      uint32_t count(1);
      if (match.str(3) != "")
        BSR_ASSERT(Dg::StringToNumber<uint32_t>(count, match.str(3), std::dec), "");
      result.push_back(varDecl{match.str(1), match.str(2), count});
      subject = match.suffix().str();
    }
    return result;
  }

  static varDeclList FindDecls(std::string const& a_str)
  {
    return __FindDecls(a_str, VAR_EXPRESSION);
  }
  
  static varDeclList FindUniformDecls(std::string const& a_str)
  {
    return __FindDecls(a_str, UNIFORM_VAR_EXPRESSION);
  }

  

  //--------------------------------------------------------------------------------------------------
  // Helper functions
  //--------------------------------------------------------------------------------------------------

  static bool IsTypeStringResource(const std::string& type)
  {
    if (type == "sampler2D")		return true;
    if (type == "samplerCube")		return true;
    if (type == "sampler2DShadow")	return true;
    return false;
  }

  //--------------------------------------------------------------------------------------------------
  // RT_RendererProgram
  //--------------------------------------------------------------------------------------------------

  RT_RendererProgram::RT_RendererProgram()
    : m_rendererID(0)
    , m_loaded(false)
  {

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

      m_uniformBuffer.Clear();
      m_shaderSource.Clear();

      for (auto ptr : m_resources)
        delete ptr;

      for (auto ptr : m_structs)
        delete ptr;

      m_resources.clear();
      m_structs.clear();

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

  bool RT_RendererProgram::Init(ShaderSource const & a_source)
  {
    return Load(a_source);
  }

  void RT_RendererProgram::SetShaderSource(ShaderSource const & a_source)
  {
    m_shaderSource = a_source;
  }

  bool RT_RendererProgram::Load(ShaderSource const & a_source)
  {
    Destroy();
    SetShaderSource(a_source);
    Parse();

    m_uniformBuffer.Log();

    for (auto ptr : m_structs)
      ptr->Log();

    if (!CompileAndUploadShader())
      return false;

    ResolveUniforms();
    //ValidateUniforms();

    m_loaded = true;
    return m_loaded;
  }

  void RT_RendererProgram::Parse()
  {
    for (int i = 0; i < ShaderDomain_COUNT; i++)
    {
      ExtractStructs(ShaderDomain(i));
      ExtractUniforms(ShaderDomain(i));
      //ExtractUniformBlocks(ShaderDomain(i));
    }
  }

  void RT_RendererProgram::ExtractStructs(ShaderDomain a_domain)
  {
    std::string subject = m_shaderSource.Get(a_domain);

    std::smatch match;
    std::regex r(STRUCT_EXPRESSION);
    while (regex_search(subject, match, r))
    {
      ShaderStruct* newStruct = new ShaderStruct(match.str(1), a_domain);
      varDeclList vars = FindDecls(match.str(2));

      for (auto const & var : vars)
      {
        ShaderUniformDeclaration* field = nullptr;
        ShaderDataType dataType = StringToShaderDataType(var.type);
        if (dataType == ShaderDataType::NONE) //might be a previously defined struct
        {
          ShaderStruct* pStruct = FindStruct(var.type, a_domain);
          if (pStruct)
            field = new ShaderUniformDeclaration(pStruct, var.name, var.count);
          else
          {
            LOG_WARN("Unrecognised field '{}' in struct '{}' while parsing glsl struct.", var.type.c_str(), match.str(1).c_str());
            continue;
          }
        }
        else
          field = new ShaderUniformDeclaration(dataType, var.name, var.count);
        newStruct->AddField(field);
      }
      m_structs.push_back(newStruct);
      subject = match.suffix().str();
    }
  }

  void RT_RendererProgram::ExtractUniformBlocks(ShaderDomain a_domain)
  {
    std::string subject = m_shaderSource.Get(a_domain);

    std::smatch match;
    std::regex r(UNIFORM_BLOCK_EXPRESSION);
    while (regex_search(subject, match, r))
    {

    }
  }

  void RT_RendererProgram::ExtractUniforms(ShaderDomain a_domain)
  {
    std::string subject = m_shaderSource.Get(a_domain);
    varDeclList vars = FindUniformDecls(subject);

    for (auto const & var : vars)
    {
      if (IsTypeStringResource(var.type))
      {
        ShaderResourceDeclaration* pDecl = new ShaderResourceDeclaration(StringToShaderResourceType(var.type), var.name, var.count);
        m_resources.push_back(pDecl);
      }
      else
      {
        ShaderDataType t = StringToShaderDataType(var.type);
        ShaderUniformDeclaration* pDecl = nullptr;

        if (t == ShaderDataType::NONE)
        {
          ShaderStruct* pStruct = FindStruct(var.type, a_domain);
          BSR_ASSERT(pStruct, "Undefined struct!");
          pDecl = new ShaderUniformDeclaration(pStruct, var.name, var.count);
        }
        else
          pDecl = new ShaderUniformDeclaration(t, var.name, var.count);
        pDecl->GetDomains().AddDomain(a_domain);

        PushUniform(pDecl);
      }
    }
  }

  void RT_RendererProgram::PushUniform(ShaderUniformDeclaration * a_pDecl)
  {
    ShaderUniformList & uniforms = m_uniformBuffer.GetUniformDeclarations();
    for (ShaderUniformDeclaration* pDecl: m_uniformBuffer.GetUniformDeclarations())
    {
      if (*a_pDecl == *pDecl)
      {
        pDecl->GetDomains().AddDomains(a_pDecl->GetDomains());
        return;
      }
    }
    m_uniformBuffer.PushUniform(a_pDecl);
  }

  ShaderStruct * RT_RendererProgram::FindStruct(std::string const& a_name, ShaderDomain a_domain)
  {
    for (ShaderStruct * ptr : m_structs)
    {
      if (a_name == ptr->GetName() && a_domain == ptr->GetDomain())
        return ptr;
    }
    return nullptr;
  }

  bool RT_RendererProgram::CompileAndUploadShader()
  {
    bool result = true;
    Dg::DynamicArray<GLuint> shaderRendererIDs;

    GLuint program = glCreateProgram();
    for (int i = 0; i < ShaderDomain_COUNT; i++)
    {
      GLenum type = ShaderDomainToOpenGLType(ShaderDomain(i));
      ShaderDomain domain = static_cast<ShaderDomain>(i);
      std::string const & source = m_shaderSource.Get(domain);

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
    glUseProgram(m_rendererID);

    for (ShaderUniformDeclaration* pUniform: m_uniformBuffer.GetUniformDeclarations())
    {
      if (pUniform->GetType() == ShaderDataType::STRUCT)
      {
        ShaderStruct const & s = pUniform->GetShaderUniformStruct();
        Dg::DynamicArray<ShaderUniformDeclaration*> const & fields = s.GetFields();
        for (size_t k = 0; k < fields.size(); k++)
        {
          ShaderUniformDeclaration * field = fields[k];
          field->SetLocation(GetUniformLocation(pUniform->GetName() + "." + field->GetName()));
        }
      }
      else
        pUniform->SetLocation(GetUniformLocation(pUniform->GetName()));
    }

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

  ShaderUniformDeclaration * RT_RendererProgram::FindUniform(std::string const& a_name)
  {
    for (auto pUniform : m_uniformBuffer.GetUniformDeclarations())
    {
      if (pUniform->GetName() == a_name)
        return pUniform;
    }
    return nullptr;
  }

  void RT_RendererProgram::UploadUniform(std::string const & a_name, void const * a_pbuf)
  {
    ShaderUniformDeclaration * pdecl = FindUniform(a_name);
    if (pdecl == nullptr)
    {
      LOG_WARN("Failed to find Uniform '{}'", a_name.c_str());
      return;
    }

    if (pdecl->GetCount() == 0)
      return;

    glUseProgram(m_rendererID);

    switch (pdecl->GetType())
    {
      case ShaderDataType::BOOL:
      {
        if (pdecl->GetCount() == 1)
          glUniform1i(pdecl->GetLocation(), *static_cast<int const *>(a_pbuf));
        else
          glUniform1iv(pdecl->GetLocation(), pdecl->GetCount(), static_cast<int const *>(a_pbuf));
      }
    }
  }

  void RT_RendererProgram::ValidateUniforms()
  {

  }
}
