#include "../core/vdf.h"
#include "test_util.h"

using namespace ss;

TEST_CASE(vdf_parses_nested) {
    auto v = vdf::loads(R"("root"
{
    "name"   "Half-Life"
    "users"
    {
        "123"  { "AccountName" "alice" }
    }
})");
    auto* root = v.get("root");
    CHECK(root != nullptr);
    CHECK_EQ(root->getStr("name"), std::string("Half-Life"));
    auto* users = root->get("users");
    CHECK(users != nullptr);
    auto* u = users->get("123");
    CHECK(u != nullptr);
    CHECK_EQ(u->getStr("AccountName"), std::string("alice"));
}

TEST_CASE(vdf_handles_escapes_and_comments) {
    auto v = vdf::loads(R"("k"
{
    // a comment line
    "path"  "C:\\Games\\Steam"
    "quote" "say \"hi\""
})");
    auto* k = v.get("k");
    CHECK(k != nullptr);
    CHECK_EQ(k->getStr("path"), std::string("C:\\Games\\Steam"));
    CHECK_EQ(k->getStr("quote"), std::string("say \"hi\""));
}

TEST_CASE(vdf_round_trips) {
    std::string src = R"("InstallConfigStore"
{
	"Software"
	{
		"Valve"
		{
			"Steam"
			{
				"AlwaysShowUserChooser"		"1"
			}
		}
	}
})";
    auto v = vdf::loads(src);
    auto out = vdf::dumps(v);
    auto v2 = vdf::loads(out);
    // Re-parsing the serialized form yields the same value.
    auto* s = v2.get("InstallConfigStore");
    CHECK(s != nullptr);
    s = s->getCI("software"); CHECK(s != nullptr);
    s = s->getCI("valve");    CHECK(s != nullptr);
    s = s->getCI("steam");    CHECK(s != nullptr);
    CHECK_EQ(s->getStr("AlwaysShowUserChooser"), std::string("1"));
}

TEST_CASE(vdf_set_overwrites_in_place) {
    auto v = vdf::loads(R"("users" { "1" { "MostRecent" "1" } "2" { "MostRecent" "1" } })");
    auto* users = v.get("users");
    users->get("1")->setStr("MostRecent", "0");
    users->get("2")->setStr("MostRecent", "1");
    CHECK_EQ(users->get("1")->getStr("MostRecent"), std::string("0"));
    CHECK_EQ(users->get("2")->getStr("MostRecent"), std::string("1"));
    // Overwrite keeps a single entry (no duplicate key appended).
    int count = 0;
    for (auto& kv : users->get("1")->map) if (kv.first == "MostRecent") ++count;
    CHECK_EQ(count, 1);
}

TEST_CASE(vdf_ci_lookup) {
    auto v = vdf::loads(R"("UserLocalConfigStore" { "apps" { "440" { "Playtime" "120" } } })");
    auto* node = v.getCI("userlocalconfigstore");
    CHECK(node != nullptr);
    node = node->getCI("APPS"); CHECK(node != nullptr);
    node = node->getCI("440");  CHECK(node != nullptr);
    CHECK_EQ(node->getStr("Playtime"), std::string("120"));
}
