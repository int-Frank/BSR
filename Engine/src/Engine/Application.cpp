#include <exception>

#include "MessageBus.h"
#include "LayerStack.h"
#include "Framework/Framework.h"

#include "core_ErrorCodes.h"
#include "Application.h"
#include "Options.h"
#include "core_Log.h"
#include "IWindow.h"
#include "core_Assert.h"
#include "Message.h"
#include "Memory.h"

#include "Layer_Console.h"
#include "Layer_InputHandler.h"
#include "Layer_Window.h"
#include "Layer_ImGui.h"

namespace Engine
{
  class Application::PIMPL
  {
  public:

    PIMPL()
      : msgBus(layerStack)
      , pWindow(nullptr)
      , shouldQuit(false)
    {
    
    }

    bool shouldQuit;
    Ref<IWindow> pWindow;

    LayerStack        layerStack;
    MessageBus        msgBus;
  };

  //------------------------------------------------------------------------------------
  // Application...
  //------------------------------------------------------------------------------------
  Application * Application::s_instance = nullptr;

  void Application::InitWindow()
  {
    m_pimpl->pWindow = Framework::Instance()->GetWindow();
    if (m_pimpl->pWindow.IsNull())
      throw std::runtime_error("GetWindow() has returned a null pointer!");

    if (m_pimpl->pWindow->Init() != Core::EC_None)
      throw std::runtime_error("Failed to initialise window!");
  }

  Application * Application::Instance()
  {
    return s_instance;
  }

  Application::Application(Opts const & a_opts)
    : m_pimpl(new PIMPL())
  {
    BSR_ASSERT(s_instance == nullptr, "Error, Application already created!");
    s_instance = this;

    if (a_opts.loggerType == E_UseFileLogger)
      Core::impl::Logger::Init_file(a_opts.loggerName.c_str(), a_opts.logFile.c_str());
    else
      Core::impl::Logger::Init_stdout(a_opts.loggerName.c_str());

    MessageTranslator::AddDefaultTranslators();

    if (Framework::Init() != Core::EC_None)
      throw std::runtime_error("Failed to initialise framework!");

    InitWindow();

    Framework::ImGui_InitData imguiData;
    m_pimpl->pWindow->GetDimensions(imguiData.window_w, imguiData.window_h);
    Framework::Instance()->InitImGui(imguiData);

    m_pimpl->layerStack.PushLayer(new Layer_InputHandler(&m_pimpl->msgBus), Layer_InputHandler::GetID());
    m_pimpl->layerStack.PushLayer(new Layer_Window(&m_pimpl->msgBus, m_pimpl->pWindow), Layer_Window::GetID());
    m_pimpl->layerStack.PushLayer(new Layer_Console(&m_pimpl->msgBus), Layer_Console::GetID());
    m_pimpl->layerStack.PushLayer(new Layer_imgui(&m_pimpl->msgBus), Layer_imgui::GetID());

    LOG_TRACE("Application initialised!");
  }

  Application::~Application()
  {
    if (Framework::ShutDown() != Core::EC_None)
      LOG_ERROR("Failed to shut down framework!");

    s_instance = nullptr;

    LOG_TRACE("Shutdown complete!");
  }

  void Application::Run()
  {
    while (!m_pimpl->shouldQuit)
    {
      float dt = 1.0f / 60.0f;
      TREFCLEAR();

      m_pimpl->msgBus.DispatchMessages();

      for (auto it = m_pimpl->layerStack.begin(); it != m_pimpl->layerStack.end(); it++)
        it->second->Update(dt);

      Layer_imgui * imguiLayer = static_cast<Layer_imgui*>(m_pimpl->layerStack.GetLayer(Layer_imgui::GetID()));
      imguiLayer->NewFrame();
      for (auto it = m_pimpl->layerStack.begin(); it != m_pimpl->layerStack.end(); it++)
        it->second->DoImGui();

      auto it = m_pimpl->layerStack.end();
      while (it != m_pimpl->layerStack.begin())
      {
        it--;
        it->second->Render();
      }
    }
  }

  void Application::RequestQuit()
  {
    m_pimpl->shouldQuit = true;
  }
}
