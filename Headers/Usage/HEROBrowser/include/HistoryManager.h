// HistoryManager.h - Manages browsing history with navigation
#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include <fstream>
#include <string>
#include <vector>

class HistoryManager {
private:
  std::vector<std::string> history;
  int current_index;
  std::string history_file;

  void loadFromFile() {
    std::ifstream file(history_file);
    if (!file.is_open())
      return;

    std::string url;
    while (std::getline(file, url)) {
      if (!url.empty()) {
        history.push_back(url);
      }
    }
    file.close();
    
    if (!history.empty()) {
      current_index = history.size() - 1;
    }
  }

  void saveToFile() {
    std::ofstream file(history_file);
    if (!file.is_open())
      return;

    for (const auto &url : history) {
      file << url << "\n";
    }
    file.close();
  }

public:
  HistoryManager(const std::string &file_path = "history.txt")
      : current_index(-1), history_file(file_path) {
    loadFromFile();
  }

  void addEntry(const std::string &url) {
    // Remove forward history if we're not at the end
    if (current_index < (int)history.size() - 1) {
      history.resize(current_index + 1);
    }
    
    // Don't add duplicate consecutive entries
    if (!history.empty() && history.back() == url) {
      return;
    }

    history.push_back(url);
    current_index = history.size() - 1;
    saveToFile();
  }

  bool canGoBack() const { return current_index > 0; }

  bool canGoForward() const {
    return current_index < (int)history.size() - 1;
  }

  std::string goBack() {
    if (canGoBack()) {
      current_index--;
      return history[current_index];
    }
    return "";
  }

  std::string goForward() {
    if (canGoForward()) {
      current_index++;
      return history[current_index];
    }
    return "";
  }

  std::string getCurrentURL() const {
    if (current_index >= 0 && current_index < (int)history.size()) {
      return history[current_index];
    }
    return "";
  }

  int getCurrentIndex() const { return current_index; }

  int getHistorySize() const { return history.size(); }

  const std::vector<std::string> &getHistory() const { return history; }

  void clear() {
    history.clear();
    current_index = -1;
    saveToFile();
  }
};

#endif // HISTORY_MANAGER
