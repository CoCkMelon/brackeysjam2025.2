#ifndef CAR_H
#define CAR_H

class Car {
public:
  CarState* throttleState;
  CarState* steeringState;
  CarState* jumpState;
  CarState* boostState;
  CarState* currentState;
public:
  Car();
  ~Car();
  void inspectThrottle();
  void inspectSteering();
  void inspectJump();
  void inspectBoost();
  CarState* getThrottleState();
  CarState* getSteeringState();
  CarState* getJumpState();
  CarState* getBoostState();
  CarState* getCurrentState();
};

#endif
