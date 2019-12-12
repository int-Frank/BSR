
#include "glad/glad.h"

#include "../Renderer.h"
#include "../IVertexArray.h"
#include "core_Assert.h"
#include "../Memory.h"
#include "../Resource.h"

namespace Engine
{
  class OpenGLVertexArray : public IVertexArray, public Resource
  {
  public:

    OpenGLVertexArray();
    ~OpenGLVertexArray() override;

    void Bind() const override;
    void Unbind() const override;

    void AddVertexBuffer(const Ref<IVertexBuffer>& vertexBuffer) override;
    void SetIndexBuffer(const Ref<IIndexBuffer>& indexBuffer) override;

    const std::vector<Ref<IVertexBuffer>>& GetVertexBuffers() const override;
    const Ref<IIndexBuffer>& GetIndexBuffer() const override;

  private:
    RendererID m_rendererID = 0;
    uint32_t m_vertexBufferIndex = 0;
    std::vector<Ref<IVertexBuffer>> m_vertexBuffers;
    Ref<IIndexBuffer> m_indexBuffer;
  };

  Ref<IVertexArray> IVertexArray::Create()
  {
    return Ref<IVertexArray>(new OpenGLVertexArray());
  }

  static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
  {
    switch (type)
    {
      case ShaderDataType::Float:    return GL_FLOAT;
      case ShaderDataType::Float2:   return GL_FLOAT;
      case ShaderDataType::Float3:   return GL_FLOAT;
      case ShaderDataType::Float4:   return GL_FLOAT;
      case ShaderDataType::Mat3:     return GL_FLOAT;
      case ShaderDataType::Mat4:     return GL_FLOAT;
      case ShaderDataType::Int:      return GL_INT;
      case ShaderDataType::Int2:     return GL_INT;
      case ShaderDataType::Int3:     return GL_INT;
      case ShaderDataType::Int4:     return GL_INT;
      case ShaderDataType::Bool:     return GL_BOOL;
    }

    BSR_ASSERT(false, "Unknown ShaderDataType!");
    return 0;
  }

  OpenGLVertexArray::OpenGLVertexArray()
  {
    RenderState state = RenderState::Create();
    state.Set<RenderState::Attr::Type>(RenderState::Type::Command);
    state.Set<RenderState::Attr::Command>(RenderState::Command::VertexArrayCreate);

    RegisterMe();
    RENDER_SUBMIT(state, [this]() mutable
      {
        glCreateVertexArrays(1, &this->m_rendererID);
        this->DeregisterMe();
      });
  }

  OpenGLVertexArray::~OpenGLVertexArray()
  {
    RenderState state = RenderState::Create();
    state.Set<RenderState::Attr::Type>(RenderState::Type::Command);
    state.Set<RenderState::Attr::Command>(RenderState::Command::VertexArrayDelete);

    RENDER_SUBMIT(state, [rendererID = m_rendererID]() mutable
      {
        glDeleteVertexArrays(1, &rendererID);
      });
  }

  void OpenGLVertexArray::Bind() const
  {
    RenderState state = RenderState::Create();
    state.Set<RenderState::Attr::Type>(RenderState::Type::Command);
    state.Set<RenderState::Attr::Command>(RenderState::Command::VertexArrayBind);

    RENDER_SUBMIT(state, [rendererID = m_rendererID]() mutable
      {
        glBindVertexArray(rendererID);
      });
  }

  void OpenGLVertexArray::Unbind() const
  {
    RenderState state = RenderState::Create();
    state.Set<RenderState::Attr::Type>(RenderState::Type::Command);
    state.Set<RenderState::Attr::Command>(RenderState::Command::VertexArrayUnBind);

    RENDER_SUBMIT(state, []()
      {
        glBindVertexArray(0);
      });
  }

  void OpenGLVertexArray::AddVertexBuffer(Ref<IVertexBuffer> const & a_vertexBuffer)
  {
    BSR_ASSERT(a_vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");

    Renderer::Instance()->BeginNewGroup();

    Bind();
    a_vertexBuffer->Bind();

    RenderState state = RenderState::Create();
    state.Set<RenderState::Attr::Type>(RenderState::Type::Command);
    state.Set<RenderState::Attr::Command>(RenderState::Command::VertexArrayAddVertexBuffer);

    RegisterMe();
    a_vertexBuffer->RegisterMe();
    RENDER_SUBMIT(state, [this, pvb = &*a_vertexBuffer]() mutable
      {
        for (auto it = pvb->GetLayout().begin(); it != pvb->GetLayout().end(); it++)
        {
          auto glBaseType = ShaderDataTypeToOpenGLBaseType(it->type);
          glEnableVertexAttribArray(this->m_vertexBufferIndex);
          if (glBaseType == GL_INT)
          {
            glVertexAttribIPointer(this->m_vertexBufferIndex,
                                   it->GetComponentCount(),
                                   glBaseType,
                                   pvb->GetLayout().GetStride(),
                                   (const void*)(intptr_t)it->offset);
          }
          else
          {
            glVertexAttribPointer(this->m_vertexBufferIndex,
                                  it->GetComponentCount(),
                                  glBaseType,
                                  it->normalized ? GL_TRUE : GL_FALSE,
                                  pvb->GetLayout().GetStride(),
                                  (const void*)(intptr_t)it->offset);
          }
          this->m_vertexBufferIndex++;
        }
        this->DeregisterMe();
        pvb->DeregisterMe();
      });

    Renderer::Instance()->EndCurrentGroup();
    m_vertexBuffers.push_back(a_vertexBuffer);
  }

  void OpenGLVertexArray::SetIndexBuffer(const Ref<IIndexBuffer>& indexBuffer)
  {
    Bind();
    indexBuffer->Bind();

    m_indexBuffer = indexBuffer;
  }
}