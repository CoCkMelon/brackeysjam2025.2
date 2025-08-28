#ifndef GAME_MEDIATOR_H
#define GAME_MEDIATOR_H

#include "GameManager.h"
#include <string>
#include <iostream>
class GameMediator : public GameManager {
private:
  Car* car;

public:
  GameMediator(Car* car);
  void notify(std::string event) override;
};

#endif
