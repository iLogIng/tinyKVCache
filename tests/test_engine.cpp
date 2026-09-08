#include "kv/engine.hpp"

#include <list>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using kv::Engine;

TEST_CASE("capacity=0 构造抛异常")
{
    REQUIRE_THROWS_AS(Engine(0), std::invalid_argument);
}

TEST_CASE("基本读写: put/get/size 往返")
{
    Engine e(4);
    REQUIRE(e.capacity() == 4);
    REQUIRE(e.size() == 0);

    e.put("a", "1");
    REQUIRE(e.size() == 1);
    REQUIRE(e.get("a") == std::optional<std::string>("1"));
    REQUIRE(e.get("missing") == std::nullopt);
}

TEST_CASE("覆盖不增容量: put 已存在则改值")
{
    Engine e(3);
    e.put("a", "1");
    e.put("b", "2");
    e.put("a", "9");
    REQUIRE(e.size() == 2);
    REQUIRE(e.get("a") == std::optional<std::string>("9"));
}

TEST_CASE("erase 边界: 存在/不存在/释放容量")
{
    Engine e(2);
    e.put("a", "1");
    REQUIRE(e.erase("a") == true);
    REQUIRE(e.size() == 0);
    REQUIRE(e.get("a") == std::nullopt);
    REQUIRE(e.erase("a") == false);
    REQUIRE(e.erase("never") == false);
}

TEST_CASE("clear 后容量复用")
{
    Engine e(2);
    e.put("a", "1");
    e.put("b", "2");
    e.clear();
    REQUIRE(e.size() == 0);
    REQUIRE(e.get("a") == std::nullopt);
    e.put("a", "1");
    e.put("b", "2");
    e.put("c", "3");  // 边界: 满员淘汰 a
    REQUIRE(e.get("a") == std::nullopt);
    REQUIRE(e.size() == 2);
}

TEST_CASE("满员淘汰最久未用: 按插入顺序淘汰最旧")
{
    Engine e(3);
    e.put("a", "1");
    e.put("b", "2");
    e.put("c", "3");
    e.put("d", "4");  // 边界: 第 4 个键淘汰最旧的 a
    REQUIRE(e.get("a") == std::nullopt);
    REQUIRE(e.get("b") != std::nullopt);
    REQUIRE(e.get("c") != std::nullopt);
    REQUIRE(e.get("d") != std::nullopt);
    REQUIRE(e.size() == 3);
}

TEST_CASE("get 刷新 recency: 访问过的最旧键不被淘汰")
{
    Engine e(3);
    e.put("a", "1");
    e.put("b", "2");
    e.put("c", "3");
    e.get("a");          // 刷新 a -> b 成为最旧
    e.put("d", "4");     // 边界: 应淘汰 b
    REQUIRE(e.get("a") != std::nullopt);
    REQUIRE(e.get("b") == std::nullopt);
    REQUIRE(e.get("d") != std::nullopt);
}

TEST_CASE("覆盖已存在键也刷新 recency")
{
    // 缓冲大小为 3
    Engine e(3);
    e.put("a", "1");
    e.put("b", "2");
    e.put("c", "3");
    // 满缓冲
    e.put("a", "9");     // 覆盖刷新 a -> b 成为最旧
    e.put("d", "4");     // 边界: 应淘汰 b
    REQUIRE(e.get("b") == std::nullopt);
    REQUIRE(e.get("a") == std::optional<std::string>("9"));
}

TEST_CASE("capacity=1 边界: 每次插入必淘汰前一键")
{
    // 缓冲大小为 1
    Engine e(1);
    e.put("a", "1");
    REQUIRE(e.size() == 1);
    e.put("b", "2");
    REQUIRE(e.get("a") == std::nullopt);
    REQUIRE(e.get("b") == std::optional<std::string>("2"));
    e.get("b");
    e.put("c", "3");
    REQUIRE(e.get("b") == std::nullopt);
    REQUIRE(e.size() == 1);
}

TEST_CASE("erase 操作不触发淘汰")
{
    Engine e(3);
    e.put("a", "1");
    e.put("b", "2");
    e.put("c", "3");
    e.erase("b");
    e.put("d", "4");     // 边界: 复用 b 的空槽, 不淘汰 a/c
    REQUIRE(e.get("a") != std::nullopt);
    REQUIRE(e.get("c") != std::nullopt);
    REQUIRE(e.get("b") == std::nullopt);
    REQUIRE(e.get("d") != std::nullopt);
    REQUIRE(e.size() == 3);
}

TEST_CASE("items 边界: 与 size 一致、无重复、LRU 序")
{
    Engine e(3);
    REQUIRE(e.items().empty());
    e.put("a", "1");
    e.put("b", "2");
    e.put("c", "3");
    auto items = e.items();  // LRU 序: MRU(c) 在前
    REQUIRE(items.size() == e.size());
    REQUIRE(items[0] == std::make_pair(std::string("c"), std::string("3")));
    REQUIRE(items[2] == std::make_pair(std::string("a"), std::string("1")));
}

TEST_CASE("随机混合操作对照: 状态与顺序一致")
{
    const int capacity = 6;
    const int key_cnt = 20;
    Engine e(static_cast<std::size_t>(capacity));
    std::list<std::pair<std::string, std::string>> sim;  // LRU 序对照模型

    auto model_put = [&](const std::string& k, const std::string& v) {
        for (auto it = sim.begin(); it != sim.end(); ++it) {
            if (it->first == k) {
                it->second = v;
                sim.splice(sim.begin(), sim, it);
                return;
            }
        }
        if (sim.size() == static_cast<std::size_t>(capacity)) {
            sim.pop_back();
        }
        sim.emplace_front(k, v);
    };
    auto model_get = [&](const std::string& k) {
        for (auto it = sim.begin(); it != sim.end(); ++it) {
            if (it->first == k) {
                sim.splice(sim.begin(), sim, it);
                return true;
            }
        }
        return false;
    };
    auto model_del = [&](const std::string& k) {
        for (auto it = sim.begin(); it != sim.end(); ++it) {
            if (it->first == k) {
                sim.erase(it);
                return true;
            }
        }
        return false;
    };

    // 随机化操作
    std::mt19937 rng(12345);
    for (int step = 0; step < 3000; ++step) {
        const std::string key = "k" + std::to_string(rng() % key_cnt);
        const unsigned r = rng() % 100;
        if (r < 50) {
            const std::string val = "v" + std::to_string(rng() % 1000);
            e.put(key, val);
            model_put(key, val);
        }
        else if (r < 75) {
            const bool hit = e.get(key) != std::nullopt;
            REQUIRE(hit == model_get(key));
        }
        else if (r < 95) {
            REQUIRE(e.erase(key) == model_del(key));
        }
        else {
            e.clear();
            sim.clear();
        }

        // 每步核对: 引擎与模型完全一致
        // LRU 序 与 值
        auto items = e.items();
        REQUIRE(items.size() <= static_cast<std::size_t>(capacity));
        REQUIRE(items.size() == sim.size());
        auto sit = sim.begin();
        for (const auto& kv : items) {
            REQUIRE(kv.first == sit->first);
            REQUIRE(kv.second == sit->second);
            ++sit;
        }
    }
}
