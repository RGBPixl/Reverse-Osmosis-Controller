#include "entry.h"
#include "page.h"
#include <Arduino.h>

MenuEntry::MenuEntry(std::initializer_list<MenuPage> pages) {
  for (MenuPage page : pages) {
    this->items.push_back(page);
  }
  this->currentPage = 0;
}

bool MenuEntry::next() {
  this->currentPage++;
  if (this->currentPage >= this->items.size()) {
    this->currentPage = 0;
    return false;
  }
  return true;
}

bool MenuEntry::prev() {
  this->currentPage--;
  if (this->currentPage < 0) {
    this->currentPage = this->items.size() - 1;
    return false;
  }
  return true;
}

void MenuEntry::goTo(int page) {
  this->currentPage = page;
  if (this->currentPage >= this->items.size()) {
    this->currentPage = 0;
  }
}
