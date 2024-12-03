#pragma once
#include "page.h"
#include <Arduino.h>
#include <Vector.h>

class MenuEntry {
private:
  Vector<MenuPage> items;
  int currentPage;

public:
  MenuEntry(std::initializer_list<MenuPage> pages);
  bool next();
  bool prev();
  void goTo(int page);

  inline MenuPage getCurrentPage(){ return this->items[this->currentPage]; };
  inline int getCurrentPageInt(){ return this->currentPage; };
  inline void reset() { this->currentPage = 0; };
};
