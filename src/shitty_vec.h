#pragma once
#include <Arduino.h>

template <typename T>
class ShittyVec {
public:
  uint len;
  T[16] items;

  ShittyVec(std::initializer_list<T> items);

  const T& operator[](int) const;
  //Shark really wanted this we never actually use it... ._.
  const void push(T) const;
};

