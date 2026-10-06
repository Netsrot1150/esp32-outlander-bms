#include "test_framework.h"
#include "settings_json.h"
#include <cstring>

static const char* apply(Settings& s, const char* json) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return settingsFromJson(s, doc.as<JsonObjectConst>());
}

TEST(sjson_maskerar_losenord_och_skriver_ip) {
  Settings s; settingsDefaults(s);
  std::strcpy(s.staPass, "hemligt123");
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), true);
  CHECK(std::strcmp(doc["apPass"].as<const char*>(), "********") == 0);
  CHECK(std::strcmp(doc["staPass"].as<const char*>(), "********") == 0);
  CHECK(std::strcmp(doc["staIp"].as<const char*>(), "192.168.1.89") == 0);
  CHECK_EQ(doc["balStartmV"].as<int>(), 3900);
  CHECK(doc["staStatic"].as<bool>());
  std::strcpy(s.staPass, "");
  JsonDocument doc2;
  settingsToJson(s, doc2.to<JsonObject>(), true);
  CHECK(std::strcmp(doc2["staPass"].as<const char*>(), "") == 0);   // tomt lösenord visas som tomt
}

TEST(sjson_delvis_uppdatering) {
  Settings s; settingsDefaults(s);
  CHECK(apply(s, "{\"balStartmV\":3950,\"tempOffset\":50}") == nullptr);
  CHECK_EQ(s.balStartmV, 3950);
  CHECK_EQ(s.tempOffset, 50);
  CHECK_EQ(s.balStartDeltamV, 20);
}

TEST(sjson_avvisar_fel_och_lamnar_orort) {
  Settings s; settingsDefaults(s);
  const char* err = apply(s, "{\"balStartmV\":70000}");
  CHECK(err && std::strcmp(err, "balStartmV") == 0);
  err = apply(s, "{\"balStartmV\":3950,\"balStopDeltamV\":25}");
  CHECK(err && std::strcmp(err, "balStopDeltamV") == 0);
  CHECK_EQ(s.balStartmV, 3900);   // inget ändrades
  err = apply(s, "{\"staStatic\":1}");
  CHECK(err && std::strcmp(err, "staStatic") == 0);
  err = apply(s, "{\"apSsid\":\"0123456789012345678901234567890123456789\"}");
  CHECK(err && std::strcmp(err, "apSsid") == 0);
  err = apply(s, "{\"staIp\":\"10.0.0.256\"}");
  CHECK(err && std::strcmp(err, "staIp") == 0);
}

TEST(sjson_losenord_och_ip) {
  Settings s; settingsDefaults(s);
  char before[65]; std::strcpy(before, s.apPass);
  CHECK(apply(s, "{\"apPass\":\"********\"}") == nullptr);
  CHECK(std::strcmp(s.apPass, before) == 0);
  CHECK(apply(s, "{\"apPass\":\"nytt-losen-123\",\"staIp\":\"10.0.0.5\"}") == nullptr);
  CHECK(std::strcmp(s.apPass, "nytt-losen-123") == 0);
  CHECK_EQ(s.staIp[0], 10); CHECK_EQ(s.staIp[3], 5);
}

TEST(sjson_stromfalt) {
  Settings s; settingsDefaults(s);
  CHECK(apply(s, "{\"curEnabled\":true,\"curSensX100\":3300,\"capacityAhX10\":380}") == nullptr);
  CHECK(s.curEnabled); CHECK_EQ(s.curSensX100, 3300); CHECK_EQ(s.capacityAhX10, 380);
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>(), true);
  CHECK_EQ(doc["curZeromV"].as<int>(), 1650);
  CHECK(doc["curEnabled"].as<bool>());
}
