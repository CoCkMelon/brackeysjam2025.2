#ifndef CAR_H
#define CAR_H

#include "CarState.h"
#include "JumpUpState.h"
#include "MoveBackwardState.h"
#include "MoveForwardState.h"
#include "SwitchOffBoostState.h"
#include "SwitchOnBoostState.h"

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
