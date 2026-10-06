#pragma once
// Minimal MCP2515-drivrutin: 8 MHz kristall, 500 kbit/s, rollover RXB0 -> RXB1,
// INT-pinnen går låg när en ram finns. send() väntar aldrig på att ramen gått iväg.
// Mottagningsfiltret släpper bara igenom CMU-ramarna 0x6X1, 0x6X2 och 0x6X3. 0x6X4 (alltid FF), 0x6X5
// (CMU:ns egen modulspänning) och övriga ramar (t.ex. identifieringsramarna 0x5xx) tas bort i kretsen. setAcceptAll(true) tar
// emot allt, vilket används under CAN-inspelning.
#include <Arduino.h>
#include <SPI.h>

struct CanFrame {
  uint32_t id;
  uint8_t  len;
  uint8_t  data[8];
  bool     ext;
  bool     rtr;
};

// Resultat av senaste begin(), visas i diagnostiken.
enum McpInitStep : uint8_t {
  MCP_STEP_NOT_RUN = 0,       // begin() har inte körts
  MCP_STEP_OK = 1,            // normalläge bekräftat
  MCP_STEP_MODE_PENDING = 2,  // konfigurerad, normalläge begärt men inte bekräftat
  MCP_STEP_NO_SPI = 3,        // går inte att få i konfigurationsläge – se spiOk
  MCP_STEP_BAD_READBACK = 4,  // registret gick inte att skriva/läsa tillbaka
};

class Mcp2515 {
 public:
  bool begin(SPIClass& spi, uint8_t csPin);   // true = SPI fungerar och kretsen är konfigurerad
  uint8_t initStep() const { return _initStep; }
  uint8_t initCanstat() const { return _initCanstat; }
  bool spiOk() const { return _spiOk; }   // registertest vid senaste begin()
  uint8_t canstat();
  uint8_t canctrl();
  bool read(CanFrame& f);                                    // false = ingen ram väntar
  bool send(uint16_t id, const uint8_t* data, uint8_t len);  // false = TXB0 upptagen
  void abortTx();
  uint8_t tec();
  uint8_t rec();
  uint8_t eflg();
  uint8_t takeOverflows();                                   // läser och nollställer RX0OVR/RX1OVR
  void setAcceptAll(bool all);                               // gäller direkt och vid nästa begin()

 private:
  SPIClass* _spi = nullptr;
  uint8_t _cs = 0;
  uint8_t _initStep = MCP_STEP_NOT_RUN;
  uint8_t _initCanstat = 0;
  bool _spiOk = false;
  bool _acceptAll = false;
  void writeId(uint8_t addr, uint16_t id);
  void select();
  void deselect();
  uint8_t readReg(uint8_t addr);
  void writeReg(uint8_t addr, uint8_t val);
  void bitModify(uint8_t addr, uint8_t mask, uint8_t val);
  uint8_t readStatus();
};
