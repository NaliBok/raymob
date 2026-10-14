#pragma once
#include <string>
namespace plat {
void Vibrate(int ms);                 // no-op on desktop; Android uses the system Vibrator via JNI
std::string DataFile(const char* name); // writable per-app location (Android: internal storage, private to the app)
}
