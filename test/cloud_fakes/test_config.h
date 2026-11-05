#pragma once
#include "WolCore.h"
#include "BemfaRootCa.h"
namespace wol {
// Synthetic host-only data, never used for firmware or external connections.
static const Settings kSettings={"TEST_ONLY_NETWORK","TEST_ONLY_PASSWORD",
  "00112233445566778899aabbccddeeff","testOnly006",Protocol::Mqtt,
  Transport::Tls,false,kBemfaRootCa,"02:11:22:33:44:55","",9,3,100,15000,3000,10000};
}
