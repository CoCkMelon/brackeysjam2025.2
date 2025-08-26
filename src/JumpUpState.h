#ifndef JUMP_UP_STATE_H
#define JUMP_UP_STATE_H

#include "Car.h"
#include "CarState.h"

class JumpUpState : public CarState {
private:
  Car* car;

public:
  JumpUpState(Car* car);
  ~JumpUpState();
  void moveCarForward() override;
  void moveCarBackward() override;
  void makeCarJump() override;
  void switchOnBoost() override;
  void switchOffBoost() override;
};

#endif
