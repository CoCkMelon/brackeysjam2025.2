#ifndef CAR_STATE_H
#define CAR_STATE_H

class Car;

class CarState {
   public:
    virtual ~CarState() {}
    virtual void moveCarForward() = 0;
    virtual void moveCarBackward() = 0;
    virtual void makeCarJump() = 0;
    virtual void switchOnBoost() = 0;
    virtual void switchOffBoost() = 0;
};

#endif
