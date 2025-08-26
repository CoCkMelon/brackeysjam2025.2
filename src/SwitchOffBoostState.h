#ifndef SWITCH_OFF_BOOST_STATE_H
#define SWITCH_OFF_BOOST_STATE_H

#include "Car.h"
#include "CarState.h"

#include <iostream>

class SwitchOffBoostState : public CarState {
private:
  Car* car;

public:
  SwitchOffBoostState(Car* car);
  ~SwitchOffBoostState();
  void moveCarForward() override;
  void moveCarBackward() override;
  void makeCarJump() override;
  void switchOnBoost() override;
  void switchOffBoost() override;
};

#endif
