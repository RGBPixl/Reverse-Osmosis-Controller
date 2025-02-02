#pragma once
#include "page.h"
#include <Arduino.h>
#include <vector>

class MenuEntry {
private:
  int currentPage;
  std::vector<MenuPage> pages;

public:
  MenuEntry(std::initializer_list<MenuPage> pagesList) : currentPage(0) {
    for(MenuPage p : pagesList) 
      pages.push_back(p);
  };

public:
  inline void next() { currentPage = currentPage < pages.size()-1 ? currentPage+1 : 0; };
  inline void prev() { currentPage = currentPage > 0 ? currentPage-1 : pages.size()-1; };
  inline void setPage(int page) { currentPage = page < pages.size() - 1 ? page : 0; };

  inline MenuPage &getCurrentPage() { return pages[currentPage]; };
  inline int getCurrentPageInt() { return currentPage; };
  inline void reset() { currentPage = 0; };
  inline int getPageCount() { return pages.size(); };
};