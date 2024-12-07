#include "entry.h"
#include "page.h"
#include <Arduino.h>

MenuEntry::MenuEntry(std::initializer_list<MenuPage> pages) : currentPage(0) {
  for (MenuPage page : pages) {
    this->pages.push_back(page);
  }
}