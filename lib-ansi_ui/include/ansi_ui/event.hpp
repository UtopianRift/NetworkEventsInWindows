// MIT License
// ansi_ui/include/ansi_ui/event.hpp
#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>

namespace ansi_ui
{

    struct Event
    {
        std::string type;
        std::any payload;
    };

    class EventProcessor
    {
    public:
        using Handler = std::function<void(const Event&)>;

        void subscribe(const std::string& type, Handler h)
        {
            subs_[type].push_back(std::move(h));
        }
        void publish(const Event& e)
        {
            auto it = subs_.find(e.type);
            if (it == subs_.end()) return;
            for (auto& h : it->second) h(e);
        }
    private:
        std::unordered_map<std::string, std::vector<Handler>> subs_;
    };

} // namespace ansi_ui
