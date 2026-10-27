// ============================================================================
// 天气详情页：24 小时温度柱状图（上 2/3）+ 未来 7 天列表（下 1/3）
// 屏幕 400×300，1-bit 单色 RLCD，黑底白字
// ============================================================================

#include "custom_lcd_display.h"
#include <esp_log.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <time.h>

LV_FONT_DECLARE(alibaba_puhui_16);
LV_FONT_DECLARE(alibaba_puhui_24);
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);

LV_IMAGE_DECLARE(ui_img_wifi_off);
LV_IMAGE_DECLARE(ui_img_battery_full);

static const char *TAG = "WeatherDetailUI";

static const int KEY_HOUR_INDEX[4] = {0, 6, 12, 18};
static const char* WEEKS_CN[] = {"周日","周一","周二","周三","周四","周五","周六"};

static std::string DateToWeekday(const std::string& ymd) {
    if (ymd.size() < 10) return ymd;
    struct tm tm_ = {};
    int y = 0, m = 0, d = 0;
    if (sscanf(ymd.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return ymd;
    tm_.tm_year = y - 1900;
    tm_.tm_mon = m - 1;
    tm_.tm_mday = d;
    tm_.tm_hour = 12;
    time_t t = mktime(&tm_);
    if (t == (time_t)-1) return ymd;
    localtime_r(&t, &tm_);
    return WEEKS_CN[tm_.tm_wday];
}

void CustomLcdDisplay::SetupWeatherDetailUI() {
    DisplayLockGuard lock(this);

    lv_obj_t *root = lv_screen_active();
    const lv_font_t *font_num  = &alibaba_puhui_16;
    const lv_font_t *font_time = &alibaba_puhui_24;
    const lv_font_t *font_cn   = &font_puhui_16_4;
    const lv_font_t *font_sm   = &font_puhui_14_1;

    // 全屏容器
    weather_detail_page_ = lv_obj_create(root);
    lv_obj_set_size(weather_detail_page_, 400, 300);
    lv_obj_set_pos(weather_detail_page_, 0, 0);
    lv_obj_set_style_bg_color(weather_detail_page_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(weather_detail_page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(weather_detail_page_, 0, 0);
    lv_obj_set_style_pad_all(weather_detail_page_, 0, 0);
    lv_obj_set_style_radius(weather_detail_page_, 0, 0);
    lv_obj_remove_flag(weather_detail_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(weather_detail_page_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *page = weather_detail_page_;

    // ===== 顶部信息层 =====
    wd_time_label_ = lv_label_create(page);
    lv_obj_set_style_text_font(wd_time_label_, font_time, 0);
    lv_obj_set_style_text_color(wd_time_label_, lv_color_white(), 0);
    lv_obj_align(wd_time_label_, LV_ALIGN_TOP_LEFT, 10, 4);
    lv_label_set_text(wd_time_label_, "00:00");

    wd_title_label_ = lv_label_create(page);
    lv_obj_set_style_text_font(wd_title_label_, font_cn, 0);
    lv_obj_set_style_text_color(wd_title_label_, lv_color_white(), 0);
    lv_obj_align(wd_title_label_, LV_ALIGN_TOP_LEFT, 96, 12);
    lv_label_set_text(wd_title_label_, "天气详情");

    // 右上角状态栏
    wd_wifi_icon_img_ = lv_image_create(page);
    lv_image_set_src(wd_wifi_icon_img_, &ui_img_wifi_off);
    lv_obj_align(wd_wifi_icon_img_, LV_ALIGN_TOP_RIGHT, -80, 8);

    wd_battery_icon_img_ = lv_image_create(page);
    lv_image_set_src(wd_battery_icon_img_, &ui_img_battery_full);
    lv_obj_align(wd_battery_icon_img_, LV_ALIGN_TOP_RIGHT, -45, 8);

    wd_battery_pct_label_ = lv_label_create(page);
    lv_obj_set_style_text_font(wd_battery_pct_label_, font_num, 0);
    lv_obj_set_style_text_color(wd_battery_pct_label_, lv_color_white(), 0);
    lv_obj_align(wd_battery_pct_label_, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_label_set_text(wd_battery_pct_label_, "---%");

    // 实时天气一行
    wd_now_label_ = lv_label_create(page);
    lv_obj_set_style_text_font(wd_now_label_, font_cn, 0);
    lv_obj_set_style_text_color(wd_now_label_, lv_color_white(), 0);
    lv_obj_set_width(wd_now_label_, 200);
    lv_obj_align(wd_now_label_, LV_ALIGN_TOP_LEFT, 10, 30);
    lv_label_set_text(wd_now_label_, "--");

    // ===== 上 2/3：24 小时温度柱状图 =====
    lv_obj_t *hour_title = lv_label_create(page);
    lv_obj_set_style_text_font(hour_title, font_sm, 0);
    lv_obj_set_style_text_color(hour_title, lv_color_white(), 0);
    lv_obj_set_style_text_opa(hour_title, LV_OPA_70, 0);
    lv_obj_align(hour_title, LV_ALIGN_TOP_LEFT, 10, 52);
    lv_label_set_text(hour_title, "24 小时温度");

    const int plot_x0 = 14;
    const int plot_w  = 372;
    const int baseline = 195;
    const double slot_w = (double)plot_w / 24.0;
    const int bar_w = 7;

    // 基线
    lv_obj_t *axis_line = lv_obj_create(page);
    lv_obj_set_pos(axis_line, plot_x0, baseline);
    lv_obj_set_size(axis_line, plot_w, 1);
    lv_obj_set_style_bg_color(axis_line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(axis_line, LV_OPA_30, 0);
    lv_obj_set_style_border_width(axis_line, 0, 0);
    lv_obj_set_style_radius(axis_line, 0, 0);
    lv_obj_remove_flag(axis_line, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 24; i++) {
        int bar_x = plot_x0 + (int)(i * slot_w + (slot_w - bar_w) / 2.0);
        wd_hour_bar_[i] = lv_obj_create(page);
        lv_obj_set_pos(wd_hour_bar_[i], bar_x, baseline);
        lv_obj_set_size(wd_hour_bar_[i], bar_w, 0);
        lv_obj_set_style_bg_color(wd_hour_bar_[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(wd_hour_bar_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(wd_hour_bar_[i], 0, 0);
        lv_obj_set_style_radius(wd_hour_bar_[i], 2, 0);
        lv_obj_set_style_pad_all(wd_hour_bar_[i], 0, 0);
        lv_obj_remove_flag(wd_hour_bar_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(wd_hour_bar_[i], LV_OBJ_FLAG_HIDDEN);

        wd_hour_label_[i] = lv_label_create(page);
        lv_obj_set_style_text_font(wd_hour_label_[i], font_num, 0);
        lv_obj_set_style_text_color(wd_hour_label_[i], lv_color_white(), 0);
        lv_obj_set_style_text_align(wd_hour_label_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(wd_hour_label_[i], (int)slot_w);
        lv_obj_set_pos(wd_hour_label_[i], plot_x0 + (int)(i * slot_w), baseline - 16);
        lv_label_set_text(wd_hour_label_[i], "");
        lv_obj_add_flag(wd_hour_label_[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (int k = 0; k < 4; k++) {
        int i = KEY_HOUR_INDEX[k];
        wd_hour_tick_[k] = lv_label_create(page);
        lv_obj_set_style_text_font(wd_hour_tick_[k], font_num, 0);
        lv_obj_set_style_text_color(wd_hour_tick_[k], lv_color_white(), 0);
        lv_obj_set_style_text_opa(wd_hour_tick_[k], LV_OPA_70, 0);
        lv_obj_set_style_text_align(wd_hour_tick_[k], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(wd_hour_tick_[k], (int)slot_w);
        lv_obj_set_pos(wd_hour_tick_[k], plot_x0 + (int)(i * slot_w), baseline + 3);
        lv_label_set_text(wd_hour_tick_[k], "");
        lv_obj_add_flag(wd_hour_tick_[k], LV_OBJ_FLAG_HIDDEN);
    }

    wd_empty_hour_label_ = lv_label_create(page);
    lv_obj_set_style_text_font(wd_empty_hour_label_, font_cn, 0);
    lv_obj_set_style_text_color(wd_empty_hour_label_, lv_color_white(), 0);
    lv_obj_set_style_text_align(wd_empty_hour_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(wd_empty_hour_label_, 300);
    lv_obj_align(wd_empty_hour_label_, LV_ALIGN_TOP_MID, 0, 110);
    lv_label_set_text(wd_empty_hour_label_, "暂无 24 小时天气");

    // ===== 下 1/3：未来 7 天 =====
    lv_obj_t *day_title = lv_label_create(page);
    lv_obj_set_style_text_font(day_title, font_sm, 0);
    lv_obj_set_style_text_color(day_title, lv_color_white(), 0);
    lv_obj_set_style_text_opa(day_title, LV_OPA_70, 0);
    lv_obj_align(day_title, LV_ALIGN_TOP_LEFT, 10, 200);
    lv_label_set_text(day_title, "未来 7 天");

    const int row_y0 = 215;
    const int row_h = 11;
    for (int r = 0; r < 7; r++) {
        int y = row_y0 + r * row_h;

        wd_day_label_[r] = lv_label_create(page);
        lv_obj_set_style_text_font(wd_day_label_[r], font_sm, 0);
        lv_obj_set_style_text_color(wd_day_label_[r], lv_color_white(), 0);
        lv_obj_set_pos(wd_day_label_[r], 10, y);
        lv_obj_set_width(wd_day_label_[r], 48);
        lv_label_set_text(wd_day_label_[r], "—");

        wd_text_label_[r] = lv_label_create(page);
        lv_obj_set_style_text_font(wd_text_label_[r], font_sm, 0);
        lv_obj_set_style_text_color(wd_text_label_[r], lv_color_white(), 0);
        lv_obj_set_pos(wd_text_label_[r], 62, y);
        lv_obj_set_width(wd_text_label_[r], 120);
        lv_label_set_text(wd_text_label_[r], "—");

        wd_temp_label_[r] = lv_label_create(page);
        lv_obj_set_style_text_font(wd_temp_label_[r], font_sm, 0);
        lv_obj_set_style_text_color(wd_temp_label_[r], lv_color_white(), 0);
        lv_obj_set_style_text_align(wd_temp_label_[r], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_width(wd_temp_label_[r], 130);
        lv_obj_align(wd_temp_label_[r], LV_ALIGN_TOP_RIGHT, -10, y);
        lv_label_set_text(wd_temp_label_[r], "--° / --°");
    }

    wd_empty_daily_label_ = lv_label_create(page);
    lv_obj_set_style_text_font(wd_empty_daily_label_, font_sm, 0);
    lv_obj_set_style_text_color(wd_empty_daily_label_, lv_color_white(), 0);
    lv_obj_set_style_text_align(wd_empty_daily_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(wd_empty_daily_label_, 300);
    lv_obj_align(wd_empty_daily_label_, LV_ALIGN_TOP_MID, 0, 250);
    lv_label_set_text(wd_empty_daily_label_, "暂无 7 天天气");
    lv_obj_add_flag(wd_empty_daily_label_, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "天气详情页 UI 创建完成");
}

void CustomLcdDisplay::RefreshWeatherDetailDisplayInternal() {
    if (!weather_detail_page_) return;

    std::vector<HourlyWeather> hourly = WeatherManager::getInstance().GetHourly();
    std::vector<DailyWeather> daily   = WeatherManager::getInstance().GetDaily();
    WeatherData now = WeatherManager::getInstance().getLatestData();

    // 顶部实时天气
    if (wd_now_label_) {
        if (now.valid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s %s %s°C",
                     now.city.c_str(), now.text.c_str(), now.temp.c_str());
            lv_label_set_text(wd_now_label_, buf);
        } else {
            lv_label_set_text(wd_now_label_, "暂无天气数据");
        }
    }

    // 24h 柱状图
    const int plot_x0 = 14;
    const int plot_w  = 372;
    const int baseline = 195;
    const int min_h = 8;
    const int max_h = baseline - 50;
    const double slot_w = (double)plot_w / 24.0;
    const int bar_w = 7;

    if (hourly.empty()) {
        for (int i = 0; i < 24; i++) {
            if (wd_hour_bar_[i]) lv_obj_add_flag(wd_hour_bar_[i], LV_OBJ_FLAG_HIDDEN);
            if (wd_hour_label_[i]) lv_obj_add_flag(wd_hour_label_[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (int k = 0; k < 4; k++) {
            if (wd_hour_tick_[k]) lv_obj_add_flag(wd_hour_tick_[k], LV_OBJ_FLAG_HIDDEN);
        }
        if (wd_empty_hour_label_) lv_obj_remove_flag(wd_empty_hour_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (wd_empty_hour_label_) lv_obj_add_flag(wd_empty_hour_label_, LV_OBJ_FLAG_HIDDEN);

        double min_t = 1e9, max_t = -1e9;
        for (size_t i = 0; i < hourly.size(); i++) {
            double t = atof(hourly[i].temp.c_str());
            if (t < min_t) min_t = t;
            if (t > max_t) max_t = t;
        }
        if (max_t == min_t) max_t = min_t + 1.0;

        for (int i = 0; i < 24; i++) {
            if (i >= (int)hourly.size()) {
                if (wd_hour_bar_[i]) lv_obj_add_flag(wd_hour_bar_[i], LV_OBJ_FLAG_HIDDEN);
                if (wd_hour_label_[i]) lv_obj_add_flag(wd_hour_label_[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            double t = atof(hourly[i].temp.c_str());
            int h = min_h + (int)((max_h - min_h) * (t - min_t) / (max_t - min_t));
            if (h < min_h) h = min_h;
            if (h > max_h) h = max_h;

            int bar_x = plot_x0 + (int)(i * slot_w + (slot_w - bar_w) / 2.0);
            if (wd_hour_bar_[i]) {
                lv_obj_set_pos(wd_hour_bar_[i], bar_x, baseline - h);
                lv_obj_set_size(wd_hour_bar_[i], bar_w, h);
                lv_obj_remove_flag(wd_hour_bar_[i], LV_OBJ_FLAG_HIDDEN);
            }

            bool is_key = false;
            for (int k = 0; k < 4; k++) {
                if (KEY_HOUR_INDEX[k] == i) { is_key = true; break; }
            }
            if (wd_hour_label_[i]) {
                if (is_key) {
                    char tbuf[16];
                    snprintf(tbuf, sizeof(tbuf), "%s°", hourly[i].temp.c_str());
                    lv_obj_set_pos(wd_hour_label_[i],
                                   plot_x0 + (int)(i * slot_w), baseline - h - 15);
                    lv_label_set_text(wd_hour_label_[i], tbuf);
                    lv_obj_remove_flag(wd_hour_label_[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_label_set_text(wd_hour_label_[i], "");
                    lv_obj_add_flag(wd_hour_label_[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        for (int k = 0; k < 4; k++) {
            int i = KEY_HOUR_INDEX[k];
            if (i < (int)hourly.size() && wd_hour_tick_[k]) {
                std::string hour = hourly[i].hour;
                if (hour.size() >= 2) {
                    char tick[4] = {0};
                    snprintf(tick, sizeof(tick), "%s", hour.substr(0, 2).c_str());
                    lv_label_set_text(wd_hour_tick_[k], tick);
                    lv_obj_remove_flag(wd_hour_tick_[k], LV_OBJ_FLAG_HIDDEN);
                }
            } else if (wd_hour_tick_[k]) {
                lv_obj_add_flag(wd_hour_tick_[k], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // 7 天列表
    if (daily.empty()) {
        for (int r = 0; r < 7; r++) {
            if (wd_day_label_[r]) lv_obj_add_flag(wd_day_label_[r], LV_OBJ_FLAG_HIDDEN);
            if (wd_text_label_[r]) lv_obj_add_flag(wd_text_label_[r], LV_OBJ_FLAG_HIDDEN);
            if (wd_temp_label_[r]) lv_obj_add_flag(wd_temp_label_[r], LV_OBJ_FLAG_HIDDEN);
        }
        if (wd_empty_daily_label_) lv_obj_remove_flag(wd_empty_daily_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (wd_empty_daily_label_) lv_obj_add_flag(wd_empty_daily_label_, LV_OBJ_FLAG_HIDDEN);
        for (int r = 0; r < 7; r++) {
            if (r < (int)daily.size()) {
                if (wd_day_label_[r]) {
                    lv_obj_remove_flag(wd_day_label_[r], LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(wd_day_label_[r], DateToWeekday(daily[r].day).c_str());
                }
                if (wd_text_label_[r]) {
                    lv_obj_remove_flag(wd_text_label_[r], LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(wd_text_label_[r], daily[r].text.c_str());
                }
                if (wd_temp_label_[r]) {
                    lv_obj_remove_flag(wd_temp_label_[r], LV_OBJ_FLAG_HIDDEN);
                    char tbuf[32];
                    snprintf(tbuf, sizeof(tbuf), "%s° / %s°",
                             daily[r].temp_min.c_str(), daily[r].temp_max.c_str());
                    lv_label_set_text(wd_temp_label_[r], tbuf);
                }
            } else {
                if (wd_day_label_[r]) lv_obj_add_flag(wd_day_label_[r], LV_OBJ_FLAG_HIDDEN);
                if (wd_text_label_[r]) lv_obj_add_flag(wd_text_label_[r], LV_OBJ_FLAG_HIDDEN);
                if (wd_temp_label_[r]) lv_obj_add_flag(wd_temp_label_[r], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}
