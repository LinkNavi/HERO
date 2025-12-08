// BookmarkManager.h - Manages bookmarks
#ifndef BOOKMARK_MANAGER_H
#define BOOKMARK_MANAGER_H

#include <fstream>
#include <string>
#include <vector>

struct Bookmark {
  std::string title;
  std::string url;
};

class BookmarkManager {
private:
  std::vector<Bookmark> bookmarks;
  std::string bookmarks_file;

  void loadFromFile() {
    std::ifstream file(bookmarks_file);
    if (!file.is_open())
      return;

    std::string title, url;
    while (std::getline(file, title) && std::getline(file, url)) {
      if (!title.empty() && !url.empty()) {
        bookmarks.push_back({title, url});
      }
    }
    file.close();
  }

  void saveToFile() {
    std::ofstream file(bookmarks_file);
    if (!file.is_open())
      return;

    for (const auto &bm : bookmarks) {
      file << bm.title << "\n" << bm.url << "\n";
    }
    file.close();
  }

public:
  BookmarkManager(const std::string &file_path = "bookmarks.txt")
      : bookmarks_file(file_path) {
    loadFromFile();
  }

  void addBookmark(const std::string &title, const std::string &url) {
    bookmarks.push_back({title, url});
    saveToFile();
  }

  void removeBookmark(size_t index) {
    if (index < bookmarks.size()) {
      bookmarks.erase(bookmarks.begin() + index);
      saveToFile();
    }
  }

  bool isBookmarked(const std::string &url) const {
    for (const auto &bm : bookmarks) {
      if (bm.url == url)
        return true;
    }
    return false;
  }

  const std::vector<Bookmark> &getBookmarks() const { return bookmarks; }

  void clear() {
    bookmarks.clear();
    saveToFile();
  }
};

#endif // BOOKMARK_MANAGER_H
