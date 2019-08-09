#ifndef LAYER_IMGUI_H
#define LAYER_IMGUI_H

#include "Message.h"
#include "Layer.h"

namespace Engine
{
  //The imgui layer is not responsible for initialising imgui, this is done in the
  //Framework class. The imgui layer is responsible for handling input and rendering.
  class Layer_imgui : public Layer
  {
  public:

    ASSIGN_ID

      Layer_imgui(MessageBus *);
    ~Layer_imgui();

    bool HandleMessage(Message const &);
    void Update(float);
    void Render();
    void NewFrame();

    void DoImGui();

  private:

    void SetMouseButton(uint32_t button, bool down);

    void HandleMouseButtonPressed(Message const &);
    void HandleMouseButtonReleased(Message const &);
    void HandleMouseScrollUp(Message const &);
    void HandleMouseScrollDown(Message const &);
    void HandleMouseMove(Message const &);
    void HandleKeyPressed(Message const &);
    void HandleKeyReleased(Message const &);
    void HandleTextInput(Message const &);
    void HandleWindowEvent(Message const &);

  private:

    float m_dt;
  };
}

#endif