#include "SwitchOnBoostState.h"

SwitchOnBoostState::SwitchOnBoostState(Car* car) {
  this->car = car;
}

SwitchOnBoostState::~SwitchOnBoostState() {
  delete car;
}

void SwitchOnBoostState::moveCarForward() override {
  std::cout << "Switching from boost on to moving forward.\n";
  car->setCurrentState(car->getMoveForward() override);
}

void SwitchOnBoostState::moveCarBackward() override {
  std::cout << "Switching from boost on to move backward.\n";
  car->setCurrentState(car->getMoveBackward() override);
}

void SwitchOnBoostState::makeCarJump() override {
  std::cout << "Switching from boost on to making car jump.\n";
  car->setCurrentState(car->getJumpUp() override);
}

void SwitchOnBoostState::switchOnBoost() override {
  std::cout << "Boost is already on.\n";
}

void SwitchOnBoostState::switchOffBoost() override {
  std::cout << "Switching from boost on to boost off.\n";
  car->setCurrentState(car->getBoostOff());
}
