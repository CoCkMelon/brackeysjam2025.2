#ifndef MOVE_FORWARD_STATE_H
#define MOVE_FORWARD_STATE_H

#include "Car.h"
#include "CarState.h"

#include <iostream>

class MoveForwardState : public CarState {
private:
  Car* car;

public:
  MoveForwardState(Car* car);
  ~MoveForwardState();
  void moveCarForward() override;
  void moveCarBackward() override;
  void makeCarJump() override;
  void switchOnBoost() override;
  void switchOffBoost() override;
};

#endif
