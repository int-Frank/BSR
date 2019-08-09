#ifndef IWINDOW_H
#define IWINDOW_H

#include <string>
#include "ErrorCodes.h"

namespace Engine
{
  struct WindowProps
  {
    WindowProps()
      : name("BSR")
      , width(1024)
      , height(768)
      , fullscreen(false)
    {}

    std::string name;
    unsigned width;
    unsigned height;
    bool fullscreen;
  };

  class IWindow
  {
  public:

    virtual ~IWindow(){};

    virtual void Update() = 0;

    virtual void SetVSync(bool) = 0;
    virtual bool IsVSync() const = 0;

    virtual bool IsInit() const = 0;
    virtual ErrorCode Init(WindowProps const & props = WindowProps()) = 0;
    virtual void Destroy() = 0;

    virtual void GetDimensions(int & w, int & h) = 0;

  private:

  };
}

#endif