#define MB_MAX_CLIENTS 1
#include "ModbusTCP_RU.h"
FakeSocket sockets[MAX_SOCK_NUM];
uint32_t fakeMillis = 0;
unsigned beginCalls = 0;
bool fakeW5100 = false;
FakeEthernet Ethernet;
int main() { ModbusTCP_RU mb; return mb.Coil(0); }
