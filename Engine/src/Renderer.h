//@group Renderer

#ifndef RENDERTEMP_H
#define RENDERTEMP_H

#include <stdint.h>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

#include "Memory.h"

#include "PODArray.h"
#include "RendererAPI.h"
#include "RenderState.h"
#include "Group.h"
#include "RenderCommandQueue.h"
#include "MemBuffer.h"
#include "IWindow.h"

#define RENDER_SUBMIT(...) ::Engine::Renderer::Instance()->Submit(__VA_ARGS__)
#define RENDER_ALLOCATE(size) ::Engine::Renderer::Instance()->Allocate(size)

namespace Engine
{
  class RenderThreadData
  {
    static RenderThreadData* s_instance;
  public:

    static bool Init();
    static void ShutDown();
    static RenderThreadData* Instance();

  public:

    Dg::OpenHashMap<uint64_t, RendererID> IDMap;
    Dg::OpenHashMap<uint64_t, uint32_t>   VAOIndex;
  };

  class Renderer
  {
  public:

    static bool Init(Ref<IWindow>);
    static void ShutDown();
    static Renderer * Instance();

    Renderer();
    ~Renderer();

    static void Clear();
    static void Clear(float r, float g, float b, float a = 1.0f);
    static void SetClearColor(float r, float g, float b, float a = 1.0f);

    //Everything must happen between these two functions.
    void BeginScene();
    void EndScene();

    //Group commands together
    void BeginNewGroup();
    void EndCurrentGroup();

    template<typename FuncT>
    void Submit(RenderState a_state, FuncT&& func)
    {
      static_assert(std::is_trivially_destructible<FuncT>::value, "FuncT must be trivially destructible");
      RenderCommandFn renderCmd = [](void* ptr)
      {
        auto pFunc = (FuncT*)ptr;
        (*pFunc)();
        pFunc->~FuncT();
      };
      a_state.Set(RenderState::Attr::Group, uint64_t(m_group.GetCurrentID()));
      auto pStorageBuffer = s_instance->m_commandQueue.AllocateForCommand(a_state, renderCmd, sizeof(func));
      new (pStorageBuffer) FuncT(std::forward<FuncT>(func));
    }

    bool ShouldExit() const;

    //Main thread
    void SyncAndHoldRenderThread();
    void SwapBuffers();
    void ReleaseRenderThread();

    //Render thread
    void RenderThreadFinishInit();
    void RenderThreadInitFailed();
    void RenderThreadShutDown();
    void FinishRender();
    void ExecuteRenderCommands();

    void * Allocate(uint32_t);

  private:

    bool __Init(Ref<IWindow>);

  private:

    static Renderer * s_instance;

  private:

    enum class ReturnCode
    {
      None,
      Ready,
      Fail,
    };

    RenderCommandQueue m_commandQueue;

    Core::Group m_group;

    std::mutex        m_mutex[2];
    std::atomic<bool> m_shouldExit;
    std::atomic<ReturnCode>  m_treturnCode;
    std::condition_variable m_cv;

    std::thread m_renderThread;

    int m_trender_ind;
    int m_tmain_ind;
  };

}

#endif