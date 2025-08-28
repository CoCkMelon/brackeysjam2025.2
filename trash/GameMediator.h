#ifndef GAME_MEDIATOR_H
#define GAME_MEDIATOR_H

#include <iostream>
#include <string>
#include "GameManager.h"
class GameMediator : public GameManager {
   private:
    Car* car;

   public:
    GameMediator(Car* car);
    void notify(std::string event) override;
};

#endif
