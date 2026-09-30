#include "ModbusTCP_RU.h"
void callback(word, bool) {}
int main() {
#ifdef TEST_ADDRESS_CALLBACK
  ModbusTCP_RU mb;
  mb.onCoilWrite(0, callback);
#endif
  return 0;
}
