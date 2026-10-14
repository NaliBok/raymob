#include "platform.h"
#include <cstdlib>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
#include "raymob.h"     // from raymob: app/src/main/cpp/deps/raymob/raymob.h  (VibrateMS, GetAppStoragePath, ...)

namespace plat {
void Vibrate(int ms) { if (ms > 0) VibrateMS((uint64_t)ms); }
std::string DataFile(const char* name) {
  char* base = GetAppStoragePath();          // app-private storage (/data/data/<package>/files), heap string
  std::string p = base ? std::string(base) + "/" + name : std::string(name);
  free(base);
  return p;
}
}
#else
namespace plat {
void Vibrate(int) {}
std::string DataFile(const char* name) { return name; }
}
#endif
