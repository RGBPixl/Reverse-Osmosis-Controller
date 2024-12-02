#pragma once
#include <Arduino.h>
#include "../shitty_vec.h"
#include "page.h"

class MenuEntry {
private:
  ShittyVec<MenuPage> items;
  int currentPage;
public:
  MenuEntry(std::initializer_list<MenuPage> pages);
  bool next();
  bool prev();
  void reset();
  void goTo(int page);
  MenuPage getCurrentPage();
  int getCurrentPageInt();
};
