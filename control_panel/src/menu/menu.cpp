//
// Created by mtsvetkov on 28.07.2020.
//
#include "menu.hpp"

#include <sstream>

namespace tsvetkov {
std::size_t Menu::add_item(std::string name, std::function<void()> action)
{
    std::size_t id = next_item_id();
    item_actions.emplace(
        std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(std::move(name), std::move(action)));
    return id;
}

std::string Menu::str() const
{
    std::stringstream ss;
    std::vector<std::size_t> keys;
    keys.reserve(item_actions.size());
    std::transform(item_actions.begin(), item_actions.end(), std::back_inserter(keys), [](const auto& pair) {
        return pair.first;
    });
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys) {
        ss << key << ". " << item_actions.find(key)->second.name << std::endl;
    }
    return std::move(ss.str());
}

std::size_t Menu::next_item_id()
{
    return id_counter++;
}

void Menu::item(std::size_t i)
{
    auto it = item_actions.find(i);
    if (it == item_actions.end()) {
        return;
    }
    it->second.action();
}

} // namespace tsvetkov