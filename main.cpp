#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <io.h>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <memory>

#include <conio.h>

#include "json.h"
#include "timer.h"
#include "mmap.h"
#include "fnv.h"

struct SubClass {
    SubClass(json::object_t const& obj) {
    }
};

class Provider { // std::vector<Provider> get_array<Provider>("game/EndingTimeProvider/dict");
    friend class json::doc_t; // Json::JPath
    Provider(json::object_t const& obj) {
    }

public:
    Provider(std::string_view const _id, int64_t const _ts = 0) : id{_id}, ts(_ts) {}
    Provider(std::string&& _id, int64_t const _ts = 0) : id{ std::move(_id) }, ts(_ts) {}
    Provider(std::string const&, int64_t const = 0) = delete;

private:
    std::string id;
    int64_t ts{0};
};

int main() {
    //std::string text{ " null " };
    //std::string text{ "true" };
    //std::string text{ "false" };
    //std::string text{ "100" };
    //std::string text{ "200.2" };
    //std::string text{ "\"test\"" };
    //std::string text{ "{\"num\":10}" };

    auto [str, ok] = json::unescape("\\\"1\\u00323\\r\\n\\\"");

    //var = nul,bol,num,str
    //val = var,obj,arr
    //arr = [*val]
    //mbr = str:val
    //obj = {*mbr}

    // @/[] -> arr of obj only?
    // 10 vs "10"?
    // get<int>, get<str>, get_array<int>, get_array<str>
    // pair<inst, bool> get<class>?
    // friend class json for private cstr<json*>?

    // game/ShopSlotBadgeState/SavedInfoStorage/Forest/Unlocked@AmberVase
    // game/ShopSlotBadgeState/SavedInfoStorage/Forest/Unlocked@4
    // game/ShopSlotBadgeState/SavedInfoStorage/Forest/Unlocked[AmberVase]
    // game/ShopSlotBadgeState/SavedInfoStorage/Forest/Unlocked[4]

    // {"root":[{"A":"1","B":2},{"A":"2","C":4}]} => root@A=2&C=4 root[A=2&C=4]
    // {"root":["A","B","C","D"]}
    // doc.get_array<str>("root")
    // doc.get_array<typ>("root") typ::cstr(json* j) { for (auto m: mbrs) ; }

    // adv
    // /@1/ <- array only
    // root@A=2&C=4 => root[A=2&C=4]
    // a/(b|c)/d

    // fix: "NessyRewarder" -> },{ bug
    //*
    //*/

    uint32_t h16a = "123"_fnv1a16;
    uint32_t h24a = "123"_fnv1a24;
    uint32_t h32a = "123"_fnv1a32;
    uint64_t h64a = "123"_fnv1a64;

    try {
        auto f0 = tmx::ms();
        //*
        std::string text;
        int fi = _open("test/sample1.json", _O_BINARY | _O_RDONLY);
        if (!fi) {
            std::cout << "can't open file\n";
            return -1;
        }
        size_t len = _filelength(fi);
        text.resize(len);
        if (static_cast<int>(len) != _read(fi, text.data(), static_cast<uint32_t>(len))) {
            std::cout << "missed file data\n";
            return -2;
        }
        _close(fi);//*/
        json::doc_t doc(std::move(text));
        //json::doc_t doc(text);
        f0 = tmx::ms() - f0;

        auto f1 = tmx::ms();
        auto cpy = doc.clone();
        f1 = tmx::ms() - f1;

        FILE* fo = fopen("out.json", "w");
        auto f2 = tmx::ms();
        cpy.serialize(fo);
        f2 = tmx::ms() - f2;
        fclose(fo);

        std::cout << "parse ms: " << f0 << "; clone ms: " << f1 << "; file serialization ms: " << f2 << std::endl;

        auto f3 = tmx::ms();
        int fd = _open("test/sample1.json", _O_BINARY | _O_RDONLY);
        size_t length = _filelength(fd);
        char const* ptr = static_cast<char const*>(mmap(0, length, PROT_READ, MAP_PRIVATE, fd, 0));
        _close(fd);
        std::string_view sv(ptr, length);
        json::doc_t mdoc(sv, true);
        f3 = tmx::ms() - f3;
        munmap(static_cast<void*>(const_cast<char*>(ptr)), length);

        std::cout << "parse mmap ms: " << f3 << "; doc mem: " << mdoc.memory() << std::endl;

        auto res = doc.find("Image/Thumbnail/Url");
        auto res_obj = doc.get_array<Provider>("game/EndingTimeProvider/dict");
        auto res_str = doc.get_array<std::string_view>("game/ShopSlotBadgeState/SavedInfoStorage/Forest/Unlocked");
        auto buildings = doc.find("islands/@territories/@buildings/@_id=value2");
        //a/b/c {a: {b: {c:...}...}}
        //a/@b/@c {a: [{b:[{c:...}...]}]}
        // /@+-#/ /@name/ /*@/
        //a.@b.@c.@-1.@0.*@.*@

        {
            class base_t {
            public:
                base_t() = default;
                virtual void print() = 0;
            };
            class factory_t;
            class cname : public base_t {
                friend class factory_t;
                friend class json::jpath_t;
                cname(json::jpath_t const jp)
                    : i{ jp.get<int>("i") }
                    , f{ jp.get<double>("f") }
                    , s{ jp.get<std::string_view>("s") }
                {}
            public:
                cname() = default;
                cname(cname&&) = default;
                void print() override { std::cout << "i:" << i << "; f:" << f << "; s:" << s << std::endl; }
            private:
                int i; double f; std::string s;
            };

            class factory_t {
            public:
                static std::unique_ptr<base_t> create(std::string_view const type, json::jpath_t const jp) {
                    if (type == "cname")
                        return std::unique_ptr<base_t>(static_cast<base_t*>(std::make_unique<cname>(std::move(cname(jp))).release()));
                    return nullptr;
                }
            };

            auto cn = json::doc_t(std::string("{\"cname\":{\"i\":1,\"f\":2.0,\"s\":\"text\"}}")).find("cname").get<cname>();
            auto cv = json::doc_t(std::string("{\"cname\":{\"i\":1,\"f\":2.0,\"s\":\"text\"}}")).find("").create<factory_t, base_t>();
            for (auto const& o: cv)
                o->print();
        }
    } catch (std::exception& ex) {
        std::cout << "error: " << ex.what() << std::endl;
    }

    _getch();
    return 0;
}
