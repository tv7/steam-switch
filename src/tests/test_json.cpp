#include "../core/json.h"
#include "test_util.h"

using namespace ss;

TEST_CASE(json_round_trips_object) {
    bool ok = false;
    auto v = json::parse(R"({"api_keys":{},"overrides":{"440":"7656119"},"owned":{"7656119":[10,20,30]}})", &ok);
    CHECK(ok);
    CHECK(v.isObject());
    auto* ov = v.get("overrides");
    CHECK(ov != nullptr);
    CHECK_EQ(ov->get("440")->asString(), std::string("7656119"));
    auto* owned = v.get("owned")->get("7656119");
    CHECK(owned->isArray());
    CHECK_EQ(owned->arr.size(), (size_t)3);
    CHECK_EQ(owned->arr[1].str, std::string("20"));   // raw number text preserved
}

TEST_CASE(json_handles_escapes_and_bools) {
    bool ok = false;
    auto v = json::parse(R"({"name":"He said \"hi\"","success":true,"n":null})", &ok);
    CHECK(ok);
    CHECK_EQ(v.get("name")->asString(), std::string("He said \"hi\""));
    CHECK(v.get("success")->asBool());
    CHECK(v.get("n")->type == json::Value::Type::Null);
}

TEST_CASE(json_set_preserves_position) {
    auto v = json::Value::makeObject();
    v.set("a", json::Value::makeNumber(1));
    v.set("b", json::Value::makeNumber(2));
    v.set("a", json::Value::makeNumber(9));   // overwrite, not append
    CHECK_EQ(v.obj.size(), (size_t)2);
    CHECK_EQ(v.obj[0].first, std::string("a"));
    CHECK_EQ(v.obj[0].second.num, 9.0);
}
