#ifndef NGD_BTDATA_SNAPSHOT_H
#define NGD_BTDATA_SNAPSHOT_H
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
namespace ngd12 {
// Configuration only: excludes /data/misc/bluetooth/logs. Never follows symlinks.
class DataSnapshot {
 public:
  explicit DataSnapshot(std::vector<std::string> roots) : roots_(roots) {}
  bool Capture();
  bool SaveExclusive(const char* path) const;
  bool LoadVerified(const char* path);
  bool Restore() const;
  bool RestoreOnly(const std::vector<std::string>& selected) const;
  size_t Count() const { return nodes_.size(); }
 private:
  struct Node {struct stat meta{};std::string data;std::map<std::string,std::string> attrs;};
  bool Walk(const std::string& path,std::map<std::string,Node>* nodes,bool capture) const;
  bool Excluded(const std::string& path) const;
  std::vector<std::string> roots_;
  std::map<std::string,Node> nodes_;
  bool captured_=false;
};
}
#endif
