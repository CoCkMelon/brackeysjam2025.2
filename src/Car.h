#ifndef CAR_H
#define CAR_H

#include "CarState.h"
#include "MoveForwardState.h"
#include "MoveBackwardState.h"
#include "JumpUpState.h"
#include "SwitchOnBoostState.h"
#include "SwitchOffBoostState.h"

class Car {
public:
  CarState* moveForward;
  CarState* moveBackward;
  CarState* jumpUp;
  CarState* boostOn;
  CarState* boostOff;
  CarState* currentState;
public:
  Car();
  ~Car();
  void moveCarForward();
  void moveCarBackward();
  void makeCarJump();
  void switchOnBoost();
  void switchOffBoost();
  CarState* getMoveForward();
  CarState* getMoveBackward();
  CarState* getJumpUp();
  CarState* getBoostOn();
  CarState* getBoostOff();
  void setCurrentState(CarState* newState);
};

#endif
