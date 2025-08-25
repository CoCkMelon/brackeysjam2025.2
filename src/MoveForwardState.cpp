#include "MoveForwardState.h"

void MoveForwardState::moveCarForward(Car* car) override {
  std::cout << "Already moving forward.\n";
}

void MoveForwardState::moveCarBackward(Car* car) override {
  std::cout << "Switching from moving forward to backward.\n";
  car->setCurrentState(car->getMoveBackward());
}

void MoveForwardState::makeCarJump(Car* car) override {
  std::cout << "Jumping while moving forward.\n";
  car->setCurrentState(car->getJumpUp());
}

void MoveForwardState::switchOnBoost(Car* car) override {
  std::cout << "Activating boost while moving forward.\n";
  car->setCurrentState(car->getBoostOn());
}

void MoveForwardState::swtichOffBoost(Car* car) override {
  std::cout << "Boost already off while moving forward.\n";
}
