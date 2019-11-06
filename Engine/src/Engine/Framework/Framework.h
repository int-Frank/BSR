#ifndef EN_FRAMEWORK_H
#define EN_FRAMEWORK_H

#include <memory>
#include <string>

#include "core_ErrorCodes.h"

#include "../IWindow.h"
#include "../IEventPoller.h"
#include "../IMouseController.h"
#include "../IShader.h"
#include "../Buffer.h"

namespace Engine
{
  class Framework
  {
  public:

    struct ImGui_InitData
    {
      int window_w;
      int window_h;
    };

  public:

    Framework();
    ~Framework();

    static Framework * Instance();
    static Core::ErrorCode Init();
    static Core::ErrorCode ShutDown();

    //There can only be one of these objects...
    std::shared_ptr<IWindow>          GetWindow();
    std::shared_ptr<IEventPoller>     GetEventPoller();
    std::shared_ptr<IMouseController> GetMouseController();

    //Audio
    //IAudio *       GetAudio();

    //These can only be initialized after the window has been created.
    bool InitImGui(ImGui_InitData const &);

  private:

    //These are implemented in the relevent cpp files
    void InitWindow();
    void InitEventPoller();
    void InitMouseController();

  private:

    void SetWindow(IWindow *);
    void SetEventPoller(IEventPoller *);
    void SetMouseController(IMouseController *);

  private:

    static Framework * s_instance;

    class PIMPL;
    PIMPL * m_pimpl;

  };
}


#endif