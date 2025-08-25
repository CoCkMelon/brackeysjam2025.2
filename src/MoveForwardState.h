#ifndef MOVE_FORWARD_STATE_H
#define MOVE_FORWARD_STATE_H

#include "Car.h"
#include "CarState.h"

#include <iostream>

class MoveForwardState : public CarState {
public:
  void moveCarForward(Car* car) override;
  void moveCarBackward(Car* car) override;
  void makeCarJump(Car* car) override;
  void switchOnBoost(Car* car) override;
  void switchOffBoost(Car* car) override;
};

#endif
