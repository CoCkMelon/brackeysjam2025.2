#include "Car.h"

Car::Car() {
  throttleState = new CarState(this);
  steeringState = new CarState(this);
  jumpState = new CarState(this);
  boostState = new CarState(this);
}


