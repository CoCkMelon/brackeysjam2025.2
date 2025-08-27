#ifndef PLAYER_H
#define PLAYER_H

#include <string>

class Player {
public:
  virtual ~Player() =  default;
  virtual void notify(std::string event) = 0;
};

#endif
