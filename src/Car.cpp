#include "Car.h"

Car::Car() {
  moveForward = new CarState();
  moveBackward = new CarState();
  jumpUp = new CarState();
  boostOn = new CarState();
  boostOff = new CarState();
}

Car::~Car() {
  delete moveForward;
  delete moveBackward;
  delete jumpUp;
  delete boostOn;
  delete boostOff;
}

void Car::moveCarForward() {
  currentState->moveCarForward();
}

void Car::moveCarBackward() {
  currentState->moveCarBackward();
}

void Car::makeCarJump() {
  currentState->makeCarJump();
}

void Car::switchOnBoost() {
  currentState->switchOnBoost();
}

void Car::switchOffBoost() {
  currentState->switchOffBoost();
}

CarState* Car::getMoveForward() {
  return moveForward;
}

CarState* Car::getMoveBackward() {
  return moveBackward;
}

CarState* Car::getJumpUp() {
  return jumpUp;
}

CarState* Car::getBoostOn() {
  return boostOn;
}

CarState* Car::getBoostOff() {
  return boostOff;
}

void Car::setCurrentState(CarState* newState) {
  currentState = newState;
}
