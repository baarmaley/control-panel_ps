//
// Created by mtsvetkov on 28.07.2020.
//

#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace tsvetkov {
class Menu
{
public:
    struct Item
    {
        Item(std::string name, std::function<void()> action) : name(std::move(name)), action(std::move(action)) {}

        std::string name;
        std::function<void()> action;
    };
    std::size_t add_item(std::string name, std::function<void()> action);

    std::string str() const;

    void item(std::size_t i);

private:
    std::size_t next_item_id();

    std::size_t id_counter = 0;

    std::unordered_map<std::size_t, Item> item_actions;
};
} // namespace tsvetkov
