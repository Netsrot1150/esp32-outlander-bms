#include "test_framework.h"
#include "config.h"

TEST(smoke_config) {
  CHECK_EQ(MAX_CMU_ID, 15);
  CHECK_EQ(CELLS_PER_CMU, 8);
  CHECK_EQ(TEMPS_PER_CMU, 6);
  CHECK_EQ(CAN_ID_BALANCE, 0x3C3);
  CHECK(sizeof(DEFAULT_AP_SSID) > 1);
}
