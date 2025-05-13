#include <vector>
#include <string>

class SensorSettingsManager {
  private:
    std::vector<std::pair<std::string, bool>> settings;

  public:

    int getSettingCount() const {
        return static_cast<int>(settings.size());
    };

    bool addSetting(const std::string& name, bool defaultValue = false){
        for (const auto& setting : settings) {
            if (setting.first == name) {
                return false;
            }
        }
        settings.emplace_back(name, defaultValue);
        return true; 
    };

    bool removeSetting(const std::string& name) {
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            if (it->first == name) {
                settings.erase(it);
                return true;
            }
        }
        return false;
    };

    const char* getSettingValue(const std::string& name) const{
        for (const auto& setting : settings) {
            if (setting.first == name) {
                return setting.second ? "true" : "false";
            }
        }
        return NULL;
    };
    
    const char* getSettingValue(int index) const {
        if (index >= 0 && index < getSettingCount()) {
            return settings[index].second ? "true" : "false";
        }
        return NULL;
    };

    bool setSettingValue(const std::string& name, const std::string& value) {
        for (auto& setting : settings) {
            if (setting.first == name) {
                // Convert value to boolean
                if (value == "1" || value == "true") {
                    setting.second = true;
                } else {
                    setting.second = false;
                }
                return true;
            }
        }
        return false;
    }

    const char* getSettingName(int index) const {
        if (index >= 0 && index < getSettingCount()){
            return settings[index].first.c_str();
        }
        return NULL;
    };

};