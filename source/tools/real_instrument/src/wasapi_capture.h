#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace real_instrument {
struct DeviceInfo {std::string id; std::string name;};
class WasapiCapture {
public:
    using Callback=std::function<void(const float*,size_t,uint32_t,uint32_t,double,bool)>;
    ~WasapiCapture();
    static std::vector<DeviceInfo> ListDevices();
    void Start(const std::string& deviceId,Callback callback);
    void Stop();
    std::string Error() const;
private:
    std::atomic<bool> stop_{false};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::string error_;
};
}
