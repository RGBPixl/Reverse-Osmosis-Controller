#include "entry.h"
#include "page.h"
#include <Arduino.h>

#ifndef ARRAY_SIZE
  #define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

MenuEntry::MenuEntry(MenuPage menupages[], int countPages) : currentPage(0) {
  for(int i = 0; i < countPages; i++) {
    pages.push_back(menupages[i]);
  }
}