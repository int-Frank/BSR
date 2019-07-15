#ifndef GAME_H
#define GAME_H

#include "Types.h"
#include "MessageBus.h"
#include "SystemStack.h"
#include "Framework/Framework.h"

class System;
class IWindow;

class Game
{
public:

  Game();
  ~Game();

  static Game * Instance();

  static void Init();
  static void ShutDown();
  static bool IsInitialised();

  void Run();
  void RequestQuit();

private:
  Game(Game const &) = delete;
  Game & operator=(Game const &) = delete;

  void InitWindow();

private:

  static Game * s_instance;

private:

  bool m_shouldQuit;
  IWindow * m_window;

  SystemStack m_systemStack;
  MessageBus  m_msgBus;
};

#endif