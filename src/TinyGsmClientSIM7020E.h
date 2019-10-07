/**
 * @file       TinyGsmClientSIM7020E.h
 * @author     Nikolay
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2019 Nikolay
 * @date       Oct 2019
 */

#ifndef TinyGsmClientSIM7020E_h
#define TinyGsmClientSIM7020E_h
//#pragma message("TinyGSM:  TinyGsmClientSIM7020E")

//#define TINY_GSM_DEBUG Serial
//#define TINY_GSM_USE_HEX

#if !defined(TINY_GSM_RX_BUFFER)
  #define TINY_GSM_RX_BUFFER 128
#endif

#define TINY_GSM_MUX_COUNT 5

#include <TinyGsmCommon.h>

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM = "ERROR" GSM_NL;

enum SimStatus {
  SIM_ERROR = 0,
  SIM_READY = 1,
  SIM_LOCKED = 2,
};

enum RegStatus {
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

enum TinyGSMDateTimeFormat {
  DATE_FULL = 0,
  DATE_TIME = 1,
  DATE_DATE = 2
};

class TinyGsmSim7020E
{

public:

  class GsmClient : public Client
  {
    friend class TinyGsmSim7020E;
    typedef TinyGsmFifo<uint8_t, TINY_GSM_RX_BUFFER> RxFifo;

  public:
    GsmClient() {}

    GsmClient(TinyGsmSim7020E& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    virtual ~GsmClient(){}

    bool init(TinyGsmSim7020E* modem, uint8_t mux = 0) {
      this->at = modem;
      this->mux = mux;
      sock_available = 0;
      prev_check = 0;
      sock_connected = false;
      got_data = false;

      at->sockets[mux] = this;

      return true;
    }

  public:
    virtual int connect(const char *host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, &mux, false, timeout_s);
      if (sock_connected) {
        at->sockets[mux] = this;
      }
      return sock_connected;
    }

  TINY_GSM_CLIENT_CONNECT_OVERLOADS()

    virtual void stop(uint32_t maxWaitMs) {
      TINY_GSM_YIELD();
      at->sendAT(GF("+CSOCL="), mux);
      sock_connected = false;
      at->waitResponse();
      rx.clear();
    }
    virtual void stop() { stop(15000L); }

  TINY_GSM_CLIENT_WRITE()

  TINY_GSM_CLIENT_AVAILABLE_NO_MODEM_FIFO()

  TINY_GSM_CLIENT_READ_NO_MODEM_FIFO()

  TINY_GSM_CLIENT_PEEK_FLUSH_CONNECTED()

    /*
    * Extended API
    */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;

  private:
    TinyGsmSim7020E*  at;
    uint8_t         mux;
    uint16_t        sock_available;
    uint32_t        prev_check;
    bool            sock_connected;
    bool            got_data;
    RxFifo          rx;
  };

  class CoAPClient : public GsmClient
  {
  friend class TinyGsmSim7020E;
  #define TINY_GSM_RX_BUFFER 128
  typedef TinyGsmFifo<uint8_t, TINY_GSM_RX_BUFFER> RxFifo;
  public:
    CoAPClient() {}
    CoAPClient(TinyGsmSim7020E& modem, uint8_t mux = 1) {
      init(&modem, mux);
    }
    virtual ~CoAPClient(){}

    bool init(TinyGsmSim7020E* modem, uint8_t mux = 1) {
      this->at = modem;
      this->mux = mux;
      sock_available = 0;
      prev_check = 0;
      sock_connected = false;
      got_data = false;

      at->sockets[mux] = this;

      return true;
    }

    virtual int connect(const char *host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = modemConnect(host, port, &mux, false, timeout_s);
      if (sock_connected) {
        at->sockets[mux] = this;
      }
      return sock_connected;
    }

    TINY_GSM_CLIENT_CONNECT_OVERLOADS()

    virtual void stop(uint32_t maxWaitMs) {
      TINY_GSM_YIELD();
      at->sendAT(GF("+CCOAPDEL="), mux);
      sock_connected = false;
      at->waitResponse();
      rx.clear();
    }
    virtual void stop() { stop(15000L); }

  public:
    TINY_GSM_CLIENT_AVAILABLE_NO_MODEM_FIFO()

    TINY_GSM_CLIENT_READ_NO_MODEM_FIFO()

    TINY_GSM_CLIENT_PEEK_FLUSH_CONNECTED()

    virtual size_t write(const uint8_t *buf, size_t size) { 
      TINY_GSM_YIELD(); 
      at->maintain(); 
      return modemSend(buf, size, mux); 
    } 
    
    virtual size_t write(uint8_t c) {
      return write(&c, 1);
    }
    
    virtual size_t write(const char *str) { 
      if (str == NULL) return 0; 
      return write((const uint8_t *)str, strlen(str)); 
    }

  private:
    bool modemConnect(const char* host, uint16_t port, uint8_t *mux, 
                                bool ssl = false, int timeout_s = 75){
      if (ssl) {
        DBG(GF("SSL not yet supported on this module!"));
        return false;
      }
      uint32_t timeout_ms = ((uint32_t)timeout_s)*1000;
      int newMux = 1;

      at->sendAT(GF("+CCOAPNEW=\""), host, "\",", port, ",1"); //1 = AT+CGACT? id of PDP context
      if(at->waitResponse(timeout_ms, GF("+CCOAPNEW: ")) != 1){
        return false;
      }
      newMux = at->stream.readStringUntil('\n').toInt();
      *mux = newMux;
      at->waitResponse(100);
      DBG(GF("### CoAP App created: "), newMux);
      return true;
    }

    int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
      DBG(GF("### CoAP SEND: "), (char*)buff, ", LEN: ", len, ", ID: ", mux);
      size_t coap_len = len / 2;
      at->sendAT(GF("+CCOAPSEND="), mux, ',', coap_len, ",\"", (char*)buff, "\"");
      if (at->waitResponse(2000, GSM_OK) != 1){
        DBG(GF("### CoAP SEND: FAILED"));
        return 0;
      }
      return len;
    }
};

public:

  TinyGsmSim7020E(Stream& stream)
    : stream(stream)
  {
    memset(sockets, 0, sizeof(sockets));
  }

  virtual ~TinyGsmSim7020E() {}

  bool begin(const char* pin = NULL) {
    return init(pin);
  }

  TINY_GSM_MODEM_TEST_AT()

  bool init(const char* pin = NULL) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);

    setLog(false);

    if (!testAT()) {
      return false;
    }
    sendAT(GF("+IFC=0,0;+IPR=115200"));
    waitResponse(GSM_OK);
    DBG(GF("### Modem:"), getModemName());

    int ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    }
    // if the sim is ready, or it's locked but no pin has been provided, return true
    else {
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  bool setLog(bool state){
    sendAT(GF("+CEREG="), state?2:0);
    waitResponse();

    sendAT(GF("+CGEREP="), state?1:0);
    waitResponse();

    sendAT(GF("&W"));
    waitResponse();
    return true;
  }

  String getModemName() {
    String name = "SIMCom SIM7020";
    sendAT(GF("+GMM"));
    String res2;
    if (waitResponse(1000L, res2) != 1) {
      return name;
    }
    res2.replace(GSM_NL "OK" GSM_NL, "");
    res2.replace("_", " ");
    res2.trim();

    name = res2;
    DBG("### Modem:", name);
    return name;
  }

  String getIMSI(){
    sendAT(GF("+CIMI"));
    if (waitResponse(GF(GSM_NL)) != 1) {
      return "";
    }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

TINY_GSM_MODEM_SET_BAUD_IPR()

TINY_GSM_MODEM_MAINTAIN_LISTEN()

  bool factoryDefault() {
    sendAT(GF("&F0&W"));  // Factory + Reset + Echo Off + Write
    waitResponse();
    sendAT(GF("+IPR=0"));   // Auto-baud
    waitResponse();
    sendAT(GF("+IFC=0,0")); // No Flow Control
    waitResponse();
    sendAT(GF("+ICF=3,3")); // 8 data 0 parity 1 stop
    waitResponse();
    sendAT(GF("+CSCLK=0")); // Disable Slow Clock
    waitResponse();
    sendAT(GF("&W"));       // Write configuration
    return waitResponse() == 1;
  }

TINY_GSM_MODEM_GET_INFO_ATI()

  bool hasSSL() {
    return false;
  }

  bool hasWifi() {
    return false;
  }

  bool hasGPRS() {
    return true;
  }

  bool restart() {
    if (!testAT()) {
      return false;
    }
    sendAT(GF("+CRESET"));
    if (waitResponse(10000) != 1) {
      return false;
    }
    sendAT(GF("+CPSMS=0"));
    if (waitResponse() != 1) {
      return false;
    }
    delay(2000);
    return init();
  }

  bool poweroff() {
    sendAT(GF("+CPOWD=1"));
    return waitResponse(10000L, GF("NORMAL POWER DOWN")) == 1;
  }

  bool radioOff() {
    sendAT(GF("+CFUN=0"));
    if (waitResponse(10000L) != 1) {
      return false;
    }
    delay(3000);
    return true;
  }

  bool radioOn() {
    sendAT(GF("+CFUN=1"));
    if (waitResponse(10000L) != 1) {
      return false;
    }
    delay(3000);
    return true;
  }

  /*
    Enable slow clock automatically. When there is no interrupt (on
    air and hardware such as GPIO interrupt or data in serial port),
    module can enter sleep mode. Otherwise, it will quit sleep mode.
  */
  bool sleepEnable(bool enable = true) {
    sendAT(GF("+CSCLK="), enable?2:0);
    return waitResponse() == 1;
  }

TINY_GSM_MODEM_SIM_UNLOCK_CPIN()

TINY_GSM_MODEM_GET_SIMCCID_CCID()

TINY_GSM_MODEM_GET_IMEI_GSN()

  SimStatus getSimStatus(unsigned long timeout_ms = 10000L) {
    for (unsigned long start = millis(); millis() - start < timeout_ms; ) {
      sendAT(GF("+CPIN?"));
      if (waitResponse(GF(GSM_NL "+CPIN:")) != 1) {
        delay(1000);
        continue;
      }
      int status = waitResponse(GF("READY"), GF("SIM PIN"), GF("SIM PUK"), GF("NOT INSERTED"), GF("NOT READY"));
      waitResponse();
      switch (status) {
        case 2:
        case 3:  return SIM_LOCKED;
        case 1:  return SIM_READY;
        default: return SIM_ERROR;
      }
    }
    return SIM_ERROR;
  }

TINY_GSM_MODEM_GET_REGISTRATION_XREG(CEREG)

TINY_GSM_MODEM_GET_OPERATOR_COPS()

  /*
   * Generic network functions
   */

TINY_GSM_MODEM_GET_CSQ()

  bool isNetworkConnected() {
    if (!testAT()) {
      return false;
    }
    RegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

  /*
  waitForNetwork    - CEREG?
  waitForRegStatus  - CEREG?
  waitForAttStatus  - CGATT?
  waitForPDPStatus  - CGACT?
  */
  bool waitForRegStatus(unsigned long timeout_ms = 120000L) {
    return waitForNetwork(timeout_ms);
  }

  bool waitForAttStatus(unsigned long timeout_ms = 120000L) {
    for (unsigned long start = millis(); millis() - start < timeout_ms; ) {
      if (isGprsConnected()) {
        return true;
      }
      delay(250);
    }
    return false;
  }

  bool waitForPDPStatus(unsigned long timeout_ms = 120000L) {
    for (unsigned long start = millis(); millis() - start < timeout_ms; ) {
      if (isPDPConnected()) {
        return true;
      }
      delay(250);
    }
    return false;
  }

  bool isPDPConnected() {
    sendAT(GF("+CGACT?"));
    if (waitResponse(GF(GSM_NL "+CGACT: 1,1")) != 1) { \
      return false; 
    }
    waitResponse();
    return true;
  }

TINY_GSM_MODEM_WAIT_FOR_NETWORK()

  /*
   * GPRS functions
   */

  bool gprsConnect(const char* apn, const char* user = NULL, const char* pwd = NULL) {
    
    if(apn && strlen(apn)>0) {
      sendAT(GF("+CFUN=0"));
      if (waitResponse(15000L) != 1) {
        return false;
      }
      if(user && strlen(user)>0) {
        sendAT(GF("*MCGDEFCONT=\"IP\",\""), apn, GF("\",\""), user, GF("\",\""), pwd, GF("\""));
      } else {
        sendAT(GF("*MCGDEFCONT=\"IP\",\""), apn, '"');
      }
      if (waitResponse(1000L) != 1) {
        return false;
      }
      sendAT(GF("+CFUN=1"));
      if (waitResponse(10000L) != 1) {
        return false;
      }
    }

    if(!isNetworkConnected()){
      DBG(GF("### Radio Off/On starting ..."));
      radioOff();
      radioOn();
      DBG(GF("### Radio Off/On completed"));
    }

    if (!waitForRegStatus()){
      DBG(GF("### RegStatus: Failed"));
      return false;
    }else{
      DBG(GF("### RegStatus: OK"));
    }

    DBG(GF("### SignalQuality: "), getSignalQuality());

    if (!waitForAttStatus()){
      DBG(GF("### AttStatus: Failed"));
      return false;
    }else{
      DBG(GF("### AttStatus: OK"));
    }

    if (!isPDPConnected()){
      DBG(GF("### PDP Activating"));
      sendAT(GF("+CGACT=1,1"));
      waitResponse(60000L);
    }
    
    if (!waitForPDPStatus()){
      DBG(GF("### PDPStatus: Failed"));
      return false;
    }else{
      DBG(GF("### PDPStatus: OK"));
    }

    sendAT(GF("+CGCONTRDP")); 
    if (waitResponse(GSM_OK) != 1) {
      return false;
    }

    // Configure Domain Name Server (DNS)
    sendAT(GF("+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\""));
    if (waitResponse() != 1) {
      return false;
    }
    return true;
  }

  bool gprsDisconnect() {
    sendAT(GF("+CGACT=0,1"));   // Deactivate PDP 
    if (waitResponse(60000L) != 1)
      return false;

    sendAT(GF("+CGATT=0"));     // Detach
    if (waitResponse(60000L) != 1)
      return false;
    
    sendAT(GF("+CGCONTRDP"));   // Release IP
    if (waitResponse(60000L) != 1)
      return false;
    return true;
  }

TINY_GSM_MODEM_GET_GPRS_IP_CONNECTED()

  String getLocalIP() {
    //sendAT(GF("+IPADDR"));  // Inquire Socket PDP address
    sendAT(GF("+CGPADDR=1"));  // Show PDP address
    if (waitResponse(GF(GSM_NL "+CGPADDR:")) != 1) {
      return "";
    }
    streamSkipUntil('"');
    String res = stream.readStringUntil('"');
    return res;
  }

  IPAddress localIP() {
    return TinyGsmIpFromString(getLocalIP());
  }

  /*
   * Phone Call functions
   */

  bool setGsmBusy(bool busy = true) TINY_GSM_ATTR_NOT_AVAILABLE;
  bool callAnswer() TINY_GSM_ATTR_NOT_AVAILABLE;

  // Returns true on pick-up, false on error/busy
  bool callNumber(const String& number) TINY_GSM_ATTR_NOT_AVAILABLE;
  bool callHangup() TINY_GSM_ATTR_NOT_AVAILABLE;

  // 0-9,*,#,A,B,C,D
  bool dtmfSend(char cmd, int duration_ms = 100) TINY_GSM_ATTR_NOT_AVAILABLE;

  /*
   * Messaging functions
   */

  String sendUSSD(const String& code) TINY_GSM_ATTR_NOT_AVAILABLE;
  bool sendSMS(const String& number, const String& text) TINY_GSM_ATTR_NOT_AVAILABLE;
  bool sendSMS_UTF16(const String& number, const void* text, size_t len) TINY_GSM_ATTR_NOT_AVAILABLE;


  /*
   * Location functions
   */

  String getGsmLocation() TINY_GSM_ATTR_NOT_AVAILABLE;

  /*
   * Time functions
   */

  String getGSMDateTime(TinyGSMDateTimeFormat format) {
    sendAT(GF("+CCLK?"));
    if (waitResponse(10000L, GF("+CCLK:"), GF("ERROR:")) != 1) {
      waitResponse(GSM_OK);
      return "";
    }

    String res;
    
    switch(format) {
      case DATE_FULL:
        res = stream.readStringUntil('\n');
      break;
      case DATE_TIME:
        streamSkipUntil(',');
        res = stream.readStringUntil('\n');
      break;
      case DATE_DATE:
        res = stream.readStringUntil(',');
      break;
    }
    waitResponse(GSM_OK);
    res.trim();
    return res;
  }

  /*
   * Battery & temperature functions
   */

  // Use: float vBatt = modem.getBattVoltage() / 1000.0;
  uint16_t getBattVoltage() {
    sendAT(GF("+CBC"));
    if (waitResponse(GF(GSM_NL "+CBC:")) != 1) {
      return 0;
    }
    streamSkipUntil(','); // Skip battery charge level
    // return voltage in mV
    uint16_t res = stream.readStringUntil(',').toInt();
    // Wait for final OK
    waitResponse();
    return res;
  }

  int8_t getBattPercent() {
    sendAT(GF("+CBC"));
    if (waitResponse(GF(GSM_NL "+CBC:")) != 1) {
      return false;
    }
    // Read battery charge level
    int res = stream.readStringUntil(',').toInt();
    // Wait for final OK
    waitResponse();
    return res;
  }

  uint8_t getBattChargeState() TINY_GSM_ATTR_NOT_AVAILABLE;
  bool getBattStats(uint8_t &chargeState, int8_t &percent, uint16_t &milliVolts) TINY_GSM_ATTR_NOT_AVAILABLE;
  float getTemperature() TINY_GSM_ATTR_NOT_AVAILABLE;

  /*
   * SNTP server functions
   */

  bool SNTPStart(String server = "88.147.254.234", String TZ = "+03") {
    sendAT(GF("+CSNTPSTART=\""), server, GF("\",\""), TZ, GF("\""));
    if (waitResponse(60000L, GF(GSM_NL "+CSNTP:")) != 1) {
      return false;
    }
    return true;
  }

  bool SNTPStop(){
    sendAT(GF("+CSNTPSTOP"));
    if(waitResponse(GSM_OK) != 1){
      return false;
    }
    return true;
  }

  /*
   * Client related functions
   */

protected:

  virtual bool modemConnect(const char* host, uint16_t port, uint8_t *mux,
                    bool ssl = false, int timeout_s = 75){
    if (ssl) {
      DBG("### SSL not yet supported on this module!");
    }

    sendAT(GF("+CSOC=1,1,1"));
    if (waitResponse(60000L, GF("+CSOC: ")) != 1){
      return false;
    }
    int newMux = stream.readStringUntil('\n').toInt();
    waitResponse(100);

    DBG(GF("### +CSOC: OK, "), newMux);

    // sendAT(GF("+CSORCVFLAG=1"));
    // waitResponse(2000L, GSM_OK);

    // sendAT(GF("+CSOSENDFLAG=1"));
    // waitResponse(2000L, GSM_OK);

    // sendAT(GF("+CSOALIVE=0,1"));
    // waitResponse(2000L, GSM_OK);

    sendAT(GF("+CSOCON=0,"), port, GF(",\""), host, GF("\""));
    if (waitResponse(60000L, GSM_OK) != 1){
      DBG(GF("### +CSOCON=0: FAILED"));
      return false;
    }
    waitResponse(100);
    DBG(GF("### +CSOCON=0: OK"));
    *mux = newMux;
    return true;
  }

  const char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  String hexStr(const char *data, int len)
  {
    String s = "";
    for (int i = 0; i < len; ++i) {
      s += hexmap[(data[i] & 0xF0) >> 4];
      s += hexmap[data[i] & 0x0F];
      //DBG(GF("### HEX: "), data[i], s[2 * i], s[2 * i + 1]);
    }
    return s;
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    DBG(GF("### CMD SEND: "), (char*)buff, ", LEN: ", len, ", MUX: ", mux);
    if(!modemGetConnected(mux)){
      DBG(GF("### SOCKET state FAILED"));
      return 0;
    }else{
      DBG(GF("### SOCKET state OK"));
    }
    String msg = hexStr((char*)buff, len);
    sendAT(GF("+CSOSEND="), mux, ',', msg.length(),",\"", msg, "\"");
    if (waitResponse(2000, GSM_OK) != 1){
      DBG(GF("### CMD SEND: FAILED"));
      return 0;
    }
    return len;
  }

  // int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
  //   DBG(GF("### CMD SEND: "), (char*)buff, ", LEN: ", len, ", MUX: ", mux);
  //   waitResponse(100);
  //   if(!modemGetConnected(mux)){
  //     DBG(GF("### SOCKET state FAILED"));
  //     return 0;
  //   }else{
  //     DBG(GF("### SOCKET state OK"));
  //   }
  //   sendAT(GF("+CSODSEND="), mux, ',', (uint16_t)len);
  //   uint8_t n = waitResponse(5000, GF(GSM_NL ">"), GF("ERROR"));
  //   DBG(GF("### N: "), n);
  //   if (n != 1) {
  //     DBG(GF("### > FAILED"));
  //     waitResponse();
  //     return 0;
  //   }
  //   stream.write((uint8_t*)buff, len);
  //   stream.flush();

  //   n = waitResponse(3000, GF("DATA ACCEPT:"));
  //   DBG(GF("### N: "), n);
  //   if (n != 1) {
  //     DBG(GF("### DATA ACCEPT FAILED"));
  //     stream.write("\r\nAT+CSORCVFLAG=1\r\n");
  //     n = waitResponse(5000, NULL, NULL, GF("DATA ACCEPT:"));
  //     DBG(GF("### N: "), n);
  //     if (n != 3){
  //       return 0;
  //     }
  //     DBG(GF("### SECOND ACCEPT OK"));
  //   }
  //   streamSkipUntil('\n');
  //   waitResponse(100, NULL, NULL);
  //   return len;
  // }

  bool modemGetConnected(uint8_t mux) {
    stream.write("\r\n");
    sendAT(GF("+CSOSTATUS="), mux);
    if(waitResponse(2000, GF("+CSOSTATUS:")) != 1){
      DBG(GF("### +CSOSTATUS get failed"));
      return false;
    }
    streamSkipUntil(','); // Skip mux
    uint8_t state = stream.readStringUntil('\n').toInt();
    if (state != 2){
      DBG(GF("### +CSOSTATUS state failed, "), state);
      waitResponse(100);
      return false;
    }
    waitResponse(100);
    return true;
  }

public:

  /*
   Utilities
   */

TINY_GSM_MODEM_STREAM_UTILITIES()

  // TODO: Optimize this!
  uint8_t waitResponse(uint32_t timeout_ms, String& data,
                       GsmConstStr r1=GFP(GSM_OK), GsmConstStr r2=GFP(GSM_ERROR),
                       GsmConstStr r3=NULL, GsmConstStr r4=NULL, GsmConstStr r5=NULL)
  {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
    data.reserve(64);
    int index = 0;
    unsigned long startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        int a = stream.read();
        if (a <= 0) continue; // Skip 0x00 bytes, just in case
        data += (char)a;
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
        } else if (data.endsWith(GF(GSM_NL "+CIPRXGET:"))) {
          String mode = stream.readStringUntil(',');
          if (mode.toInt() == 1) {
            int mux = stream.readStringUntil('\n').toInt();
            if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
              sockets[mux]->got_data = true;
            }
            data = "";
            DBG("### Got Data:", mux);
          } else {
            data += mode;
          }
        } else if (data.endsWith(GF(GSM_NL "+RECEIVE:"))) {
          int mux = stream.readStringUntil(',').toInt();
          int len = stream.readStringUntil('\n').toInt();
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->got_data = true;
            sockets[mux]->sock_available = len;
          }
          data = "";
          DBG("### Got Data:", len, "on", mux);
        } else if (data.endsWith(GF("CLOSED" GSM_NL))) {
          int nl = data.lastIndexOf(GSM_NL, data.length()-8);
          int coma = data.indexOf(',', nl+2);
          int mux = data.substring(nl+2, coma).toInt();
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->sock_connected = false;
          }
          data = "";
          DBG("### Closed: ", mux);
        } else if (data.endsWith(GF(GSM_NL "+CSONMI:"))){
            int mux = stream.readStringUntil(',').toInt();
            int len = stream.readStringUntil(',').toInt();
            DBG("### +CSONMI SOCKET: ", mux, ", ERROR: ", len);
            
            int len_orig = len;
            if (len > sockets[mux]->rx.free()) {
              DBG("### Buffer overflow: ", len, "->", sockets[mux]->rx.free());
            } else {
              DBG("### Got: ", len, "->", sockets[mux]->rx.free());
            }
            while (len--) {
              TINY_GSM_MODEM_STREAM_TO_MUX_FIFO_WITH_DOUBLE_TIMEOUT
            }
            sockets[mux]->got_data = true;
            sockets[mux]->sock_available = len;
            if (len_orig > sockets[mux]->available()) {
              DBG("### Fewer characters received than expected: ", sockets[mux]->available(), " vs ", len_orig);
            }
            data = "";
            DBG("### Got Data:", len_orig, "on", mux);
        } else if (data.endsWith(GF("+CSOERR:"))) {
          int mux = stream.readStringUntil(',').toInt();
          int err_code = stream.readStringUntil('\n').toInt();
          DBG("### +CSOERR SOCKET: ", mux, ", ERROR: ", err_code);
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT) {
            sockets[mux]->sock_connected = false;
          }
          data = "";
          DBG("### Closed: ", mux);
        } else if (data.endsWith(GF(GSM_NL "SEND:"))){
          int mux = stream.readStringUntil(',').toInt();
          int len = stream.readStringUntil('\n').toInt();
          data = "";
          DBG(GF("SEND: MUX: "), mux, ", LEN: ", len);
        } else if (data.endsWith(GF("+CCOAPNMI:"))) {
          int mux = stream.readStringUntil(',').toInt();
          int len = stream.readStringUntil(',').toInt();
          DBG("### +COAP ID: ", mux, ", LEN: ", len);
          int len_orig = len;
          len *=2;
          if (len > sockets[mux]->rx.free()) {
            DBG("### Buffer overflow: ", len, "->", sockets[mux]->rx.free());
          } else {
            DBG("### Got: ", len, "->", sockets[mux]->rx.free());
          }
          while (len--) {
            TINY_GSM_MODEM_STREAM_TO_MUX_FIFO_WITH_DOUBLE_TIMEOUT
          }
          sockets[mux]->got_data = true;
          sockets[mux]->sock_available = len;
          if (len_orig > sockets[mux]->available()) {
            DBG("### Fewer characters received than expected: ", sockets[mux]->available(), " vs ", len_orig);
          }
          data = "";
          DBG("### Got Data:", len_orig, "on", mux);
        }
      }
    } while (millis() - startMillis < timeout_ms);
finish:
    if (!index) {
      data.trim();
      if (data.length()) {
        DBG("### Unhandled:", data, ", INDEX: ", index);
      }
      data = "";
    }
    return index;
  }

  uint8_t waitResponse(uint32_t timeout_ms,
                       GsmConstStr r1=GFP(GSM_OK), GsmConstStr r2=GFP(GSM_ERROR),
                       GsmConstStr r3=NULL, GsmConstStr r4=NULL, GsmConstStr r5=NULL)
  {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  uint8_t waitResponse(GsmConstStr r1=GFP(GSM_OK), GsmConstStr r2=GFP(GSM_ERROR),
                       GsmConstStr r3=NULL, GsmConstStr r4=NULL, GsmConstStr r5=NULL)
  {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

public:
  Stream&       stream;

protected:
  GsmClient*    sockets[TINY_GSM_MUX_COUNT];
};

#endif
