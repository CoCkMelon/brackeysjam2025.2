#ifndef CAR_H
#define CAR_H

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
  CarState* getBoostOn();
  CarState* getBoostOff();
  void setCurrentState(State* newState);
};

#endif
