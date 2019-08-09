#ifndef OPENGLCONTEXT_H
#define OPENGLCONTEXT_H

#include "../Engine/IGraphicsContext.h"

struct SDL_Window;

namespace Engine
{
  typedef void *SDL_GLContext;

  class OpenGLContext : public IGraphicsContext
  {
  public:

    ~OpenGLContext();
    OpenGLContext(SDL_Window *);

    ErrorCode Init() override;
    void SwapBuffers() override;

  private:

    SDL_Window *  m_pWindow;
    SDL_GLContext m_context;
  };
}


#endif