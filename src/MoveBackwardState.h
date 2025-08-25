#ifndef MOVE_BACKWARD_STATE_H
#define MOVE_BACKWARD_STATE_H

#include "CarState.h"
#include "Car.h"

#include <iostream>

class MoveBackwardState : public CarState {
private:
  Car* car;

public:
  MoveBackwardState(Car* car);
  void moveCarForward();
  void moveCarBackward();
  void makeCarJump();
  void switchOnBoost();
  void switchOffBoost();
};

#endif
