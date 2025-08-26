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
  ~MoveBackwardState();
  void moveCarForward() override;
  void moveCarBackward() override;
  void makeCarJump() override;
  void switchOnBoost() override;
  void switchOffBoost() override;
};

#endif
