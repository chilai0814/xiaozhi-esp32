#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "esp_http_client.h"

// 天气数据结构（实时天气，保留原有字段）
struct WeatherData {
    std::string city;     // 城市名
    std::string temp;     // 温度（字符串）
    std::string text;     // 天气描述（如"晴"、"多云"）
    std::string update_time;
    bool valid = false;
};

// 24 小时预报单条
struct HourlyWeather {
    std::string hour;   // "HH:00"
    std::string temp;   // 温度字符串
    std::string text;   // 天气描述
};

// 未来 7 天预报单条
struct DailyWeather {
    std::string day;       // "YYYY-MM-DD"
    std::string text;      // 白天天气
    std::string temp_min;  // 最低温
    std::string temp_max;  // 最高温
};

// 天气管理器：通过和风天气 API 获取实时 + 24h + 7d 预报
class WeatherManager {
public:
    static WeatherManager& getInstance();

    // 更新实时天气（含 IP 定位，阻塞，应在后台任务调用）
    bool update();

    // 获取最新实时天气（线程安全，直接读缓存）
    WeatherData getLatestData() { return latest_data_; }

    // 设置 API 密钥和主机
    void setApiConfig(const char* key, const char* host);

    // AI 侧 MCP 回写天气
    bool updateFromExternal(const std::string& city,
                            const std::string& weather_text,
                            const std::string& temperature,
                            const std::string& update_time = "");

    // ===== 24h / 7d 预报接口 =====
    std::vector<HourlyWeather> GetHourly() const;
    std::vector<DailyWeather> GetDaily() const;
    // 后台拉取 24h+7d（阻塞，应在后台任务调用）
    bool FetchForecast();
    // 供 data_update_task 周期调用（内部 30 分钟冷却）
    bool RefreshForecastIfIdle();

private:
    WeatherManager();
    WeatherData latest_data_;

    std::string api_key_;
    std::string api_host_;

    // 预报缓存（互斥保护）
    mutable std::mutex forecast_mutex_;
    std::vector<HourlyWeather> hourly_;
    std::vector<DailyWeather> daily_;

    double cached_lat_ = 31.23;   // 默认兜底：苏州
    double cached_lon_ = 120.62;
    bool location_valid_ = false;
    uint32_t last_attempt_ms_ = 0;

    bool ResolveLocation(double& lat, double& lon);
    bool FetchHourly();
    bool FetchDaily();
    struct cJSON* PerformGetForecast(const char* url);

    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
    void parseWeatherJson(const char* json_data);
};
