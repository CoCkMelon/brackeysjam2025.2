#ifndef SWITCH_ON_BOOST_STATE_H
#define SWTICH_ON_BOOST_STATE_H

#include "Car.h"
#include "CarState.h"

#include <iostream>

class SwitchOnBoostState : public CarState {
private:
  Car* car;

public:
  SwitchOnBoostState(Car* car);
  ~SwitchOnBoostState();
  void moveCarForward() override;
  void moveCarBackward() override;
  void makeCarJump() override;
  void switchOnBoost() override;
  void switchOffBoost() override;
};

#endif
