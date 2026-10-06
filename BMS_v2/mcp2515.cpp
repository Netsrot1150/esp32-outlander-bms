#include "mcp2515.h"

namespace {
const uint8_t INS_RESET = 0xC0, INS_READ = 0x03, INS_WRITE = 0x02, INS_BITMOD = 0x05,
              INS_READ_STATUS = 0xA0, INS_READ_RX0 = 0x90, INS_READ_RX1 = 0x94,
              INS_LOAD_TX0 = 0x40, INS_RTS_TX0 = 0x81;
const uint8_t REG_CANSTAT = 0x0E, REG_CANCTRL = 0x0F, REG_TEC = 0x1C, REG_REC = 0x1D,
              REG_CNF3 = 0x28, REG_CNF2 = 0x29, REG_CNF1 = 0x2A, REG_CANINTE = 0x2B,
              REG_CANINTF = 0x2C, REG_EFLG = 0x2D, REG_RXB0CTRL = 0x60, REG_RXB1CTRL = 0x70,
              REG_TXB0D0 = 0x36, REG_TXB0D1 = 0x37, REG_RXM0 = 0x20, REG_RXM1 = 0x24;
// Filterregistren RXF0..RXF5. RXF0-1 hör till RXB0, RXF2-5 till RXB1.
const uint8_t REG_RXF[6] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18};
// Masken jämför bit 10-8 (0x6..) och bit 2 i ramtypen. Filtret 0x600 släpper då igenom 0x6X0-0x6X3
// (cellramarna och 0x6X1) men inte 0x6X4 (alltid FF) eller 0x6X5 (CMU:ns egen modulspänning).
// Alla filter är lika, så att båda buffertarna (med rollover) tar emot alla ramar som behövs.
const uint16_t RX_MASK = 0x704;
const uint16_t RX_FILTER = 0x600;
const uint8_t RXM_ALL = 0x60;   // RXBnCTRL.RXM = 11: ta emot allt, filtren av
// 4 MHz i stället för 10 MHz: mer marginal när 5 V-modulen drivs med 3,3 V-signaler.
// Räcker gott: en ram tar ca 40 µs att läsa, CMU:erna sänder ca 1000 ramar/s.
const SPISettings SPI_CFG(4000000, MSBFIRST, SPI_MODE0);
}  // namespace

void Mcp2515::select() {
  _spi->beginTransaction(SPI_CFG);
  digitalWrite(_cs, LOW);
}

void Mcp2515::deselect() {
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
}

uint8_t Mcp2515::readReg(uint8_t addr) {
  select();
  _spi->transfer(INS_READ);
  _spi->transfer(addr);
  const uint8_t v = _spi->transfer(0x00);
  deselect();
  return v;
}

void Mcp2515::writeReg(uint8_t addr, uint8_t val) {
  select();
  _spi->transfer(INS_WRITE);
  _spi->transfer(addr);
  _spi->transfer(val);
  deselect();
}

void Mcp2515::bitModify(uint8_t addr, uint8_t mask, uint8_t val) {
  select();
  _spi->transfer(INS_BITMOD);
  _spi->transfer(addr);
  _spi->transfer(mask);
  _spi->transfer(val);
  deselect();
}

// Standard-id i SIDH/SIDL. EID8/EID0 nollas: för standardramar jämförs de annars mot databyte 0-1.
void Mcp2515::writeId(uint8_t addr, uint16_t id) {
  writeReg(addr, (uint8_t)(id >> 3));
  writeReg(addr + 1, (uint8_t)((id & 0x07) << 5));
  writeReg(addr + 2, 0x00);
  writeReg(addr + 3, 0x00);
}

uint8_t Mcp2515::readStatus() {
  select();
  _spi->transfer(INS_READ_STATUS);
  const uint8_t v = _spi->transfer(0x00);
  deselect();
  return v;
}

bool Mcp2515::begin(SPIClass& spi, uint8_t csPin) {
  _spi = &spi;
  _cs = csPin;
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  select();
  _spi->transfer(INS_RESET);
  deselect();
  delay(50);
  _initCanstat = readReg(REG_CANSTAT);

  // SPI-test: TXB0D0/D1 är skrivbara i alla lägen. Visar om SPI fungerar även när reset inte biter.
  writeReg(REG_TXB0D0, 0x55);
  writeReg(REG_TXB0D1, 0xAA);
  _spiOk = readReg(REG_TXB0D0) == 0x55 && readReg(REG_TXB0D1) == 0xAA;

  if ((_initCanstat & 0xE0) != 0x80) {
    // Reset gick inte fram (5 V-modulen tar emot 3,3 V-nivåer på marginalen). Begär
    // konfigurationsläge via CANCTRL i stället – samma väg som vid lägesbyte under drift.
    for (int i = 0; i < 20 && (readReg(REG_CANSTAT) & 0xE0) != 0x80; i++) {
      writeReg(REG_CANCTRL, 0x80);
      delay(5);
    }
    if ((readReg(REG_CANSTAT) & 0xE0) != 0x80) {
      _initStep = MCP_STEP_NO_SPI;
      return false;
    }
  }

  writeReg(REG_CNF1, 0x00);       // 8 MHz, 500 kbit/s – samma värden som AA_MCP2515 Config_8MHz_500kbps
  writeReg(REG_CNF2, 0x91);
  writeReg(REG_CNF3, 0x01);
  if (readReg(REG_CNF2) != 0x91) {
    _initStep = MCP_STEP_BAD_READBACK;
    return false;
  }
  writeId(REG_RXM0, RX_MASK);     // masker och filter går bara att skriva i konfigurationsläge
  writeId(REG_RXM1, RX_MASK);
  for (uint8_t i = 0; i < 6; i++) writeId(REG_RXF[i], RX_FILTER);
  const uint8_t rxm = _acceptAll ? RXM_ALL : 0x00;
  writeReg(REG_RXB0CTRL, rxm | 0x04);   // BUKT=1 (rollover till RXB1)
  writeReg(REG_RXB1CTRL, rxm);
  writeReg(REG_CANINTF, 0x00);
  writeReg(REG_CANINTE, 0x03);    // RX0IE | RX1IE
  writeReg(REG_CANCTRL, 0x00);    // normalläge, CLKOUT av
  for (int i = 0; i < 200; i++) {
    if ((readReg(REG_CANSTAT) & 0xE0) == 0x00) {
      _initStep = MCP_STEP_OK;
      return true;
    }
    delay(1);
  }
  // Normalläge begärt men ännu inte bekräftat. Kretsen byter själv när bussen tillåter
  // (samma beteende som AA_MCP2515), så vi fortsätter och visar läget i diagnostiken.
  _initStep = MCP_STEP_MODE_PENDING;
  return true;
}

uint8_t Mcp2515::canstat() { return readReg(REG_CANSTAT); }
uint8_t Mcp2515::canctrl() { return readReg(REG_CANCTRL); }

bool Mcp2515::read(CanFrame& f) {
  const uint8_t st = readStatus();
  uint8_t ins;
  if (st & 0x01) ins = INS_READ_RX0;
  else if (st & 0x02) ins = INS_READ_RX1;
  else return false;

  uint8_t b[13];
  select();
  _spi->transfer(ins);
  for (uint8_t i = 0; i < sizeof(b); i++) b[i] = _spi->transfer(0x00);
  deselect();   // READ RX BUFFER nollställer RXnIF när CS går hög

  f.ext = (b[1] & 0x08) != 0;
  if (f.ext) {
    f.id = ((uint32_t)b[0] << 21) | ((uint32_t)(b[1] & 0xE0) << 13) | ((uint32_t)(b[1] & 0x03) << 16) |
           ((uint32_t)b[2] << 8) | b[3];
    f.rtr = (b[4] & 0x40) != 0;
  } else {
    f.id = ((uint32_t)b[0] << 3) | (b[1] >> 5);
    f.rtr = (b[1] & 0x10) != 0;
  }
  f.len = b[4] & 0x0F;
  if (f.len > 8) f.len = 8;
  memcpy(f.data, &b[5], 8);
  return true;
}

bool Mcp2515::send(uint16_t id, const uint8_t* data, uint8_t len) {
  if (len > 8) len = 8;
  if (readStatus() & 0x04) return false;   // TXB0CTRL.TXREQ: förra ramen har inte gått iväg
  select();
  _spi->transfer(INS_LOAD_TX0);
  _spi->transfer((uint8_t)(id >> 3));
  _spi->transfer((uint8_t)((id & 0x07) << 5));
  _spi->transfer(0x00);
  _spi->transfer(0x00);
  _spi->transfer(len);
  for (uint8_t i = 0; i < len; i++) _spi->transfer(data[i]);
  deselect();
  select();
  _spi->transfer(INS_RTS_TX0);
  deselect();
  return true;
}

void Mcp2515::abortTx() {
  bitModify(REG_CANCTRL, 0x10, 0x10);   // ABAT
  delayMicroseconds(200);
  bitModify(REG_CANCTRL, 0x10, 0x00);
}

uint8_t Mcp2515::tec() { return readReg(REG_TEC); }
uint8_t Mcp2515::rec() { return readReg(REG_REC); }
uint8_t Mcp2515::eflg() { return readReg(REG_EFLG); }

void Mcp2515::setAcceptAll(bool all) {
  _acceptAll = all;
  if (!_spi) return;
  const uint8_t rxm = all ? RXM_ALL : 0x00;   // RXM-bitarna går att ändra i normalläge
  bitModify(REG_RXB0CTRL, RXM_ALL, rxm);
  bitModify(REG_RXB1CTRL, RXM_ALL, rxm);
}

uint8_t Mcp2515::takeOverflows() {
  const uint8_t e = readReg(REG_EFLG);
  const uint8_t n = (uint8_t)(((e & 0x40) ? 1 : 0) + ((e & 0x80) ? 1 : 0));
  if (n) bitModify(REG_EFLG, 0xC0, 0x00);
  return n;
}
