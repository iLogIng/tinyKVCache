#include "kv/engine.hpp"
#include "kv/journal.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace kv;

namespace {

// 测试用临时日志文件, 析构时删除
struct TempFile
{
    static int counter;
    std::string path;

    TempFile()
    {
        path = "kvtest_" + std::to_string(++counter) + "_" +
               std::to_string(::getpid()) + ".aof";
    }
    ~TempFile() { std::remove(path.c_str()); }
};

int TempFile::counter = 0;

}  // namespace

TEST_CASE("journal 编解码往返: Put/Del/Clear 与二进制 value")
{
    TempFile f;
    {
        Journal j;
        REQUIRE(j.open(f.path));
        REQUIRE(j.append(Op::Put, "a", "1"));
        REQUIRE(j.append(Op::Put, "b", std::string("line1\nline2\0bin", 15)));
        REQUIRE(j.append(Op::Del, "a", ""));
        REQUIRE(j.append(Op::Clear, "", ""));
        REQUIRE(j.append(Op::Put, "c", ""));
    }

    Journal j2;
    REQUIRE(j2.open(f.path));
    std::vector<Record> got;
    REQUIRE(j2.replay([&](const Record& r) { got.push_back(r); }));
    REQUIRE(got.size() == 5);
    REQUIRE(got[0].op == Op::Put);
    REQUIRE(got[0].key == "a");
    REQUIRE(got[0].value == "1");
    REQUIRE(got[1].key == "b");
    REQUIRE(got[1].value == std::string("line1\nline2\0bin", 15));
    REQUIRE(got[2].op == Op::Del);
    REQUIRE(got[3].op == Op::Clear);
    REQUIRE(got[4].op == Op::Put);
    REQUIRE(got[4].value.empty());
}

TEST_CASE("journal 坏尾截断: 丢弃不完整尾部记录")
{
    TempFile f;
    {
        Journal j;
        REQUIRE(j.open(f.path));
        REQUIRE(j.append(Op::Put, "a", "1"));
        REQUIRE(j.append(Op::Put, "b", "2"));
    }
    // 追加半条记录模拟崩溃
    {
        std::ofstream ofs(f.path, std::ios::app | std::ios::binary);
        ofs.write("\x00\x02\x00\x00\x00x", 6);  // kind=Put, klen=2, 只有部分 key
    }

    Journal j;
    REQUIRE(j.open(f.path));
    std::vector<Record> got;
    REQUIRE(j.replay([&](const Record& r) { got.push_back(r); }));
    REQUIRE(got.size() == 2);
    REQUIRE(got[0].key == "a");
    REQUIRE(got[1].key == "b");

    // 截断后可继续正常追加与读取
    REQUIRE(j.append(Op::Put, "c", "3"));
    got.clear();
    REQUIRE(j.replay([&](const Record& r) { got.push_back(r); }));
    REQUIRE(got.size() == 3);
    REQUIRE(got[2].key == "c");
}

TEST_CASE("engine 钩子: attach 后写操作记入日志, 回放与写序列一致")
{
    TempFile f;
    std::vector<Record> ops;
    {
        Journal j;
        REQUIRE(j.open(f.path));
        Engine e(8);
        e.attach(j);
        e.put("a", "1");
        e.put("b", "2");
        e.put("a", "9");  // 覆盖, 记一条 Put
        e.erase("b");
        e.clear();
        e.put("c", "3");
        REQUIRE(j.replay([&](const Record& r) { ops.push_back(r); }));
    }
    REQUIRE(ops.size() == 6);
    REQUIRE(ops[0].op == Op::Put);
    REQUIRE(ops[0].key == "a");
    REQUIRE(ops[3].op == Op::Del);
    REQUIRE(ops[3].key == "b");
    REQUIRE(ops[4].op == Op::Clear);
    REQUIRE(ops[5].op == Op::Put);
    REQUIRE(ops[5].key == "c");

    // 回放到新引擎: 内容一致(写驱动语义, 序列含 clear)
    Journal j2;
    REQUIRE(j2.open(f.path));
    Engine e2(8);
    REQUIRE(e2.replay(j2));
    REQUIRE(e2.get("c") == std::optional<std::string>("3"));
    REQUIRE(e2.get("a") == std::nullopt);
    REQUIRE(e2.size() == 1);
}

TEST_CASE("engine 覆盖与淘汰不额外记日志; 容量一致时写驱动回放")
{
    TempFile f;
    {
        Journal j;
        REQUIRE(j.open(f.path));
        Engine e(3);
        e.attach(j);
        e.put("a", "1");
        e.put("b", "2");
        e.put("c", "3");
        e.put("d", "4");  // 淘汰 a; 淘汰不记日志
        REQUIRE(e.get("a") == std::nullopt);

        Journal jr;
        REQUIRE(jr.open(f.path));
        Engine e2(3);
        REQUIRE(e2.replay(jr));  // 写驱动: put a,b,c,d 重放, 由容量自行淘汰
        REQUIRE(e2.size() == 3);
        REQUIRE(e2.get("d") != std::nullopt);
    }
}
