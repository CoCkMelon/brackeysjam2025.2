#include "Car.h"

Car::Car() {
  throttleState = new CarState(this);
  steeringState = new CarState(this);
  jumpState = new CarState(this);
  boostState = new CarState(this);
}

Car::~Car() {
  delete throttleState;
  delete steeringState;
  delete jumpState;
  delete boostState;
}

void Car::inspectThrottle() {
  state->inspectThrottle();
}
