#define IM920_CH 37
#define IM920_MODE 3
#define IM920_TXP 2
#define INTERVAL 10000
#define IM920_RX 6
#define IM920_TX 5
#define IM920_RESET 7
#define DONE 8
#define MAJOR 7615
//#define DISP


#include <Arduino.h>
#include "M5AtomS3.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <Wire.h>
#include "Adafruit_MCP9808.h"

static uint8_t cmdbuf[256];
static uint16_t cmdix = 0;
static uint8_t txbuf[256];
static uint8_t rxbuf[256];
static uint16_t rxix = 0;
static bool cmdready = false;
static bool rxready = false;
static bool im920avail = false;
static bool im920initialized = false;
static bool mcp9808avail = false;
static bool firstsend = true;
static bool button = false;
static uint8_t count = 0;
static uint16_t major = MAJOR;

static uint32_t nodeid = 0xFFFFFFFF;
static uint32_t groupid = 0xFFFFFFFF;
static uint16_t nodenum = 0;

static uint32_t sentTime = 0;
static uint32_t interval = INTERVAL;

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

bool hexToUint32(const char *str, uint32_t *result) {
    uint32_t value = 0;
    int count = 0;

    if (str == NULL || result == NULL) return false;

    while (*str != '\0') {
        uint8_t digit;
        char c = *str++;

        if (count++ >= 8) return false;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        }
        else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        }
        else {
            return false;
        }
        value = (value << 4) | digit;
    }

    if (count == 0) return false;

    *result = value;
    return true;
}

bool rcv_command(char * cmd, uint32_t timeout) {
    bool ready = false;
    uint32_t startTime = millis();
    while(((startTime + timeout) > millis()) && (!cmdready) && (cmdix < 256)) {
        if (Serial.available()) {
            int ch = Serial.read();
            if (ch >= 0x20) {
                cmdbuf[cmdix] = (uint8_t)ch;
                cmdix += 1;
            }
            if (ch == 0x0D) {
                cmdbuf[cmdix] = (uint8_t)0;
                cmdix = 0;
                cmdready = true;
            }
        }
    }
    if (cmdready) {
        strncpy(cmd,(char *)cmdbuf,255);
        ready = cmdready;
        cmdready = false;
    }
    return(ready);
}

bool im920_receive(char * data, uint16_t maxleng, uint32_t timeout) {
    if ((data == NULL) || (maxleng == 0)) return false;
    if (maxleng > sizeof(rxbuf)) maxleng = sizeof(rxbuf);

    bool ready = false;
    uint32_t startTime = millis();
    while (((millis() - startTime) < timeout) && !rxready) {
        while (Serial2.available()) {
            int ch = Serial2.read();
            if (ch == 0x0D) {
                rxbuf[rxix] = 0;
                rxix = 0;
                rxready = true;
                break;
            }
            if ((ch >= 0x20) && (rxix < sizeof(rxbuf) - 1)) {
                rxbuf[rxix] = (uint8_t)ch;
                rxix += 1;
            }
        }
        if (!rxready) delay(1);
    }

    if (rxready) {
        strncpy(data, (char *)rxbuf, maxleng - 1);
        data[maxleng - 1] = 0;
        ready = true;
        rxready = false;
    } else {
        // Do not let a timed-out partial response contaminate the next command.
        rxix = 0;
    }
    return(ready);
}

void im920_flush() {
    delay(20);
    while(Serial2.available()) {
        char c = Serial2.read();
    }
    rxix = 0;
    rxready = false;
}

bool im920_write(char * cmdstr,char * response, uint16_t leng) {
    bool result = false;

//    Serial.printf("Write %s\r\n",cmdstr);
    Serial2.printf("%s\r\n",cmdstr);
    delay(15);
    if (im920_receive(response,leng,1000)) {
        result = true;
//        Serial.printf("Receive OK\r\n");
    }

    return (result);
}

bool im920_set(char * cmdstr) {
    bool result = false;
    char respstr[16];

//    Serial.printf("SET %s\r\n",cmdstr);

    if (im920_write(cmdstr,respstr,10)) {
//        Serial.printf("resp:%s\r\n",respstr);
        delay(20);
        if ((respstr[0] == 'O') && (respstr[1] == 'K')) {
            result = true;
        }
    }

    return (result);
}

bool im920_get_parameter_dec(char * cmdstr,uint32_t * parameter){
    bool result = false;
    char respstr[16];

    if (im920_write(cmdstr,respstr,10)) {
//        Serial.printf("resp:%s\r\n",respstr);
        *parameter = atoi(respstr);
        if (*parameter != 0) {
            result = true;
        }
    }
    return (result);
}

bool im920_get_parameter(char * cmdstr,uint32_t * parameter){
    bool result = false;
    char respstr[16];

    if (im920_write(cmdstr,respstr,10)) {
//        Serial.printf("resp:%s\r\n",respstr);

        if (hexToUint32(respstr,parameter)) {
            result = true;
        }
    }
    return (result);
}


bool im920_get_ver(char * verstr) {
    uint32_t res = false;

//    im920_flush();
    if (im920_write("RDVR",verstr,24)) {
        res = true;
    } else {
        res = false;
    }
    return (res);
}

uint32_t im920_get_id() {
    uint32_t res;
    uint32_t value;
    if (im920_get_parameter("RDID",&value)) {
        res = value;
    } else {
        res = 0xFFFFFFFF;
    }
    return (res);
}

uint32_t im920_get_group() {
    uint32_t res;
    uint32_t value;
    if (im920_get_parameter("RDGN",&value)) {
        res = value;
    } else {
        res = 0xFFFFFFFF;
    }
    return (res);
}

uint32_t im920_get_nodeNumber() {
    uint32_t res;
    uint32_t value;
    if (im920_get_parameter("RDNN",&value)) {
        res = value;
    } else {
        res = 0xFFFF;
    }
    return (res);
}

uint32_t im920_get_channel() {
    uint32_t res;
    uint32_t value;
    if (im920_get_parameter_dec("RDCH",&value)) {
        res = value;
    } else {
        res = 0xFFFF;
    }
    return (res);
}

int16_t im920_get_txPower() {
    uint16_t res;
    uint32_t value;
    if (im920_get_parameter("RDPO",&value)) {
        res = value;
    } else {
        res = 0xFFFF;
    }
    return (res);
}

int16_t im920_get_mode() {
    uint16_t res;
    uint32_t value;
    if (im920_get_parameter("RDRT",&value)) {
        res = value;
    } else {
        res = 0xFFFF;
    }
    return (res);
}


bool im920_enable_write() {
    return(im920_set("ENWR"));
}

bool im920_disable_write() {
    return(im920_set("DSWR"));
}

bool im920_param_clear() {
    return(im920_set("PCLR"));
}

bool im920_set_group() {
    char rbuf[16];

    if (!im920_enable_write()) {
        Serial.printf("ENWR failed before STGN\r\n");
        return false;
    }
    Serial.printf("ENWR OK\r\n");

    // A module left in sniffer mode receives registration packets but does
    // not store the group ID. Always leave sniffer mode before STGN.
    if (!im920_set("DSNF")) {
        Serial.printf("DSNF failed before STGN\r\n");
        return false;
    }
    Serial.printf("DSNF OK\r\n");

    im920_flush();
    if (!im920_set("STGN")) {
        Serial.printf("STGN NG\r\n");
        return false;
    }

    Serial.printf("STGN OK; waiting for GRNOREGD\r\n");

    // STGN remains active until a hardware reset. Once the command has been
    // accepted, keep waiting instead of sending ENWR/STGN again and mixing
    // asynchronous GRNOREGD with command responses.
    while (true) {
        if (!im920_receive(rbuf, sizeof(rbuf), 1000)) {
            AtomS3.update();
            continue;
        }

        Serial.printf("STGN Resp %s\r\n", rbuf);
        if (strcmp(rbuf, "GRNOREGD") != 0) {
            Serial.printf("Ignore unexpected response while joining\r\n");
            continue;
        }

        Serial.printf("GRNOREGD received; resetting IM920\r\n");
        digitalWrite(IM920_RESET, LOW);
        delay(100);
        digitalWrite(IM920_RESET, HIGH);
        delay(600);
        im920_flush();

        for (int ii = 0; ii < 3; ++ii) {
            AtomS3.dis.drawpix(0x00FF00);
            AtomS3.update();
            delay(100);
            AtomS3.dis.drawpix(0x000000);
            AtomS3.update();
            delay(100);
        }

        groupid = im920_get_group();
        if (groupid != 0xFFFFFFFF) {
            Serial.printf("Joined to New Group:%08X\r\n", groupid);
            return true;
        } else {
            Serial.printf("Joined to Group Failed\r\n");
            return false;
        }
    }
}

bool im920_set_nodeNumber(uint16_t value) {
    sprintf((char *)txbuf,"STNN %04X",value);
    return(im920_set((char *) txbuf));
}

bool im920_set_channel(uint16_t value) {
    sprintf((char *)txbuf,"STCH %02d",value);
    return(im920_set((char *) txbuf));
}

bool im920_set_txPower(uint16_t value) {
    sprintf((char *)txbuf,"STPO %d",value);
    return(im920_set((char *) txbuf));
}

bool im920_set_mode(uint16_t value) {
    sprintf((char *)txbuf,"STRT %d",value);
    return(im920_set((char *) txbuf));
}


bool im920_receive_data(uint8_t * data,uint16_t * length, uint16_t * nodeid,int16_t * rssi) {
    bool result = false;
    delay(10);
    uint16_t maxleng = 255;
    uint16_t rcvleng = 0;
    if (Serial2.available()) {
        uint32_t startTime = millis();
        while(((startTime + 1000) > millis()) && (!rxready) && (rxix < maxleng)) {
            if (Serial2.available()) {
                int ch = Serial2.read();
                if (ch >= 0x20) {
                    rxbuf[rxix] = (uint8_t)ch;
                    rxix += 1;
                }
                if (ch == 0x0D) {
                    rxbuf[rxix] = (uint8_t)0;
                    rcvleng = rxix;
                    rxix = 0;
                    rxready = true;
                }
            }
            delay(1);
        }
        if (rxready) {
            if ((rcvleng > 13) && (rxbuf[2] == ',') && (rxbuf[7] == ',') && (rxbuf[10] == ':')) {
                uint16_t iix = 11;
                int ii = 0;
                bool dataerror = false;
                while ((iix < rcvleng) && !dataerror) {
                    char dat[3];
                    dat[0] = rxbuf[iix];
                    dat[1] = rxbuf[iix + 1];
                    dat[2] = 0;
                    uint32_t value;
                    if (hexToUint32(dat,&value)) {
                        data[ii] = value & 0x000000ff;
                        ii += 1;
                    } else {
                        dataerror = true;
                    }
                    iix += 3;
                }
                if ((ii > 0) && (!dataerror)) {
                    result = true;
                    *length = ii;
                    char nid[5];
                    nid[0] = rxbuf[3];
                    nid[1] = rxbuf[4];
                    nid[2] = rxbuf[5];
                    nid[3] = rxbuf[6];
                    nid[4] = 0;
                    uint32_t value;
                    hexToUint32(nid,&value);
                    *nodeid = value & 0x0000ffff;
                    nid[0] = rxbuf[8];
                    nid[1] = rxbuf[9];
                    nid[2] = 0;
                    hexToUint32(nid,&value);
                    *rssi = (value & 0x000000ff) | 0xff00;
                }
            }
            rxready = false;
        }
    }
    return(result);
}


bool im920_Transmit(uint8_t * data, uint16_t leng) {
    bool result = false;
    rxix = 0;
    Serial2.printf("TXDA ");
    for (int ii = 0;ii < leng; ii++) {
        Serial2.printf("%02X",data[ii]);
    }
    Serial2.printf("\r\n");

    char response[8];
    delay(500);
    if (im920_receive(response,3,5000)) {
        if ((response[0] == 'O') && (response[1] == 'K')) {
            result = true;
        }
    }
    return (result);
}

bool im920_init(uint8_t ch,uint8_t md, uint8_t po, uint16_t ndnum) {
    bool resp = true;

    if (im920_enable_write()) {
//        Serial.printf("Write Enable OK\r\n");
    } else {
        resp = false;
//        Serial.printf("Write Enable NG\r\n");
    }

    if (im920_set_mode(md)) {
//        Serial.printf("Set Mode OK\r\n");
//        Serial.printf("Mode:%d\r\n",im920_get_mode());
        if (im920_set_channel(ch)) {
            Serial.printf("Set Channel OK\r\n");
            Serial.printf("Channel:%d\r\n",im920_get_channel());
        } else {
            resp = false;
        }
    } else {
//        resp = false;
        if (im920_set_channel(7)) {
            Serial.printf("Set Channel OK\r\n");
            Serial.printf("Channel:%d\r\n",im920_get_channel());
        } else {
            resp = false;
        }
    }


    if (im920_set_txPower(po)) {
//        Serial.printf("Set txPower OK\r\n");
//        Serial.printf("txPower:%d\r\n",im920_get_txPower());
    } else {
        resp = false;
    }

    if (im920_set_nodeNumber(ndnum)) {
//        Serial.printf("Set NodeNumber OK\r\n");
//        Serial.printf("nodeNumber:%04X\r\n",im920_get_nodeNumber());
    } else {
        resp = false;
    }

    return (resp);
}


void setup() {

    auto cfg = M5.config();
    AtomS3.begin(cfg,true);
    pinMode(IM920_RESET,OUTPUT);
    digitalWrite(IM920_RESET,LOW);
    digitalWrite(DONE,LOW);
    pinMode(DONE,OUTPUT);
    digitalWrite(DONE,LOW);

    AtomS3.dis.setBrightness(100);
    AtomS3.dis.drawpix(0x000000);
    Serial.begin();
    cmdix = 0;
    rxix = 0;
    cmdready = false;
    rxready = false;
    im920avail = false;
    im920initialized = false;
    nodeid = 0;
    nodenum = 0;
    button = false;
    mcp9808avail = false;
    count = 0;
    major = MAJOR;

    Wire.begin(2,1,100000UL);
    delay(50);

    digitalWrite(IM920_RESET,HIGH);
    Serial2.begin(19200, SERIAL_8N1, IM920_RX,IM920_TX);
    Serial.printf("\r\n\nStart UART\r\n");


    if (tempsensor.begin(0x18,&Wire)) {
        Serial.printf("MCP9808 OK!\r\n");
        mcp9808avail = true;
        tempsensor.setResolution(3);
    } else {
        Serial.printf("MCP9808 not found!\r\n");
    }

#ifdef DISP
    AtomS3.Display.clear();
    AtomS3.Display.setTextSize(2);
    AtomS3.Display.setCursor(0,0);
    AtomS3.Display.setTextColor(WHITE);
    AtomS3.Display.printf("Init IM920\r\n");
    if (mcp9808avail) {
        AtomS3.Display.printf("MCP9808 OK\r\n");
    }
#endif

    char resp[256];

    if (!im920initialized) {
        delay(200);
        im920_flush();
        while (!im920_get_ver(resp)) {
            Serial.printf("IM920 not Found\r\n%s\r\n",resp);
            delay(2000);
        }
    }
    im920avail = true;

    nodeid = im920_get_id();
    nodenum = im920_get_nodeNumber();
    groupid = im920_get_group();
    
    Serial.printf("IM920ID:%08X\r\n",nodeid);
    Serial.printf("Node Number:%04X\r\n",nodenum);
    Serial.printf("GroupID:%08X\r\n",groupid);

#ifdef DISP
    AtomS3.Display.clear();
#endif

    delay(500);

    firstsend = true;

}

void loop() {
    int16_t ch = 0;
    int16_t rt = 0;
    int16_t po = 0;

    button = false;

    AtomS3.update();
    if (AtomS3.BtnA.wasPressed()) {
        button = true;
    }
    if (AtomS3.BtnA.isPressed()) {
        button = true;
    }
    if (AtomS3.BtnA.wasReleased()) {
        button = true;
    }
    if (button) {
        button = false;
        Serial.printf("Group ID Update\r\n");
        if (im920_param_clear()) {
            im920initialized = false;
            Serial.printf("IM920 Parameters Clear\r\n");
            AtomS3.dis.drawpix(0xff0000);
            AtomS3.update();
            delay(200);
        }
    }

    char rspbuf[256];
    char commandbuf[256];
    if (rcv_command(commandbuf,100)) {
        Serial.printf("> %s\r\n",commandbuf);
//        AtomS3.Lcd.printf("> %s\r\n",commandbuf);
        Serial2.printf("%s\r\n",commandbuf);
        if (im920_receive(rspbuf,255,1000)) {
            Serial.printf("%s\r\n",rspbuf);
//            AtomS3.Lcd.printf("%s\r\n",rspbuf);
        }
    }

    if (!im920initialized) {
        nodenum = nodeid & 0x0000FFFF;
        if (im920_init(IM920_CH,IM920_MODE,IM920_TXP,nodenum)) {
            Serial.printf("IM920 init OK!\r\n");
            ch = im920_get_channel();
            Serial.printf("  Channel:%d\r\n",ch);
            po = im920_get_txPower();
            Serial.printf("  Tx Power:%d\r\n",po);
            rt = im920_get_mode();
            Serial.printf("  Mode:%d\r\n",rt);
            Serial.printf("  NodeNumber:%04X\r\n\n",im920_get_nodeNumber());
#ifdef DISP
            AtomS3.Display.clear();
            AtomS3.Display.setTextSize(2);
            AtomS3.Display.setCursor(0,0);
            AtomS3.Display.setTextColor(WHITE);
            AtomS3.Display.printf("ID");
            AtomS3.Display.setCursor(30,0);
            AtomS3.Display.printf("%08X",nodeid);
            AtomS3.Display.setCursor(0,18);
            AtomS3.Display.printf("NN ");
            AtomS3.Display.printf("%04X",nodenum);
            AtomS3.Display.setCursor(0,36);
            AtomS3.Display.printf("Gr");
            AtomS3.Display.setCursor(30,36);
            AtomS3.Display.printf("%08X",groupid);
            AtomS3.Display.setCursor(0,54);
            AtomS3.Display.printf("CH:%02X",ch);
            AtomS3.Display.setCursor(0,72);
            AtomS3.Display.printf("Mode:%d",rt);
#endif
            groupid = im920_get_group();
            if (groupid == 0xFFFFFFFF) {
#ifdef DISP
                AtomS3.Display.setTextSize(2);
                AtomS3.Display.setTextColor(RED);
                AtomS3.Display.fillRect(0,36,128,18,BLACK);
                AtomS3.Display.setCursor(0,36);
                AtomS3.Display.printf("Grp Seach");
#endif
                AtomS3.dis.drawpix(0xff0000);
                AtomS3.update();

                while (!im920_set_group()){
                    Serial.printf("Network not Joined!\r\n");
                    delay(5000) ;
                }
                groupid = im920_get_group();
                Serial.printf("GroupID:%08X\r\n",groupid);
                AtomS3.dis.drawpix(0x00ff00);
                AtomS3.update();

#ifdef DISP
                AtomS3.Display.setTextSize(2);
                AtomS3.Display.setTextColor(GREEN);
                AtomS3.Display.fillRect(0,36,128,18,BLACK);
                AtomS3.Display.setCursor(0,36);
                AtomS3.Display.printf("Gr");
                AtomS3.Display.setCursor(30,36);
                AtomS3.Display.printf("%08X",groupid);
#endif
            } else {
                im920initialized = true;
                firstsend = true;
            }
        } else {
            Serial.printf("IM920 init NG!\r\n");
        }
        im920_disable_write();
    }

    uint16_t length;
    int16_t rssi;
    uint16_t txnodeid;
    uint8_t rxdata[256];
    if (im920_receive_data((uint8_t *)rxdata,&length,&txnodeid,&rssi)) {
        Serial.printf("Received from %04X %dbytes  rssi %d\r\n" , txnodeid,length,rssi);
        for (int ii = 0; ii <length; ii ++) {
            Serial.printf("%02X ",rxdata[ii]);
        }
        Serial.printf("\r\n");
    }

    uint32_t currentTime = millis();
    if ((currentTime - sentTime >= INTERVAL) || firstsend) {
        sentTime = millis();
        firstsend = false;
        Serial.printf("Measure Data %d\r\n",sentTime);

        float tc = tempsensor.readTempC();
        Serial.printf("Temperature %6.2f\r\n",tc);

        int16_t it = (int16_t)(10 * tc);
    
#ifdef DISP
        AtomS3.Display.setTextColor(WHITE);
        AtomS3.Display.setTextSize(3);
        AtomS3.Display.fillRect(0,96,128,32,BLACK);
        AtomS3.Display.setCursor(0,96);
        AtomS3.Display.printf("%6.2fc",tc);
#endif

        AtomS3.dis.drawpix(0x0000FF);
        AtomS3.update();
        uint8_t txdata[8];
        txdata[0] = (major >> 8) & 0x00ff;
        txdata[1] = (major >> 0) & 0x00ff;
        txdata[2] = (nodenum >> 8) & 0x00ff;
        txdata[3] = (nodenum >> 0) & 0x00ff;
        txdata[4] = count;
        txdata[5] = 0x02;
        txdata[6] = (it >> 8) & 0x00ff;;
        txdata[7] = (it >> 0) & 0x00ff;
        if (im920_Transmit(txdata,8)) {
            Serial.printf("Tx OK\r\n");
            AtomS3.dis.drawpix(0x000000);
            AtomS3.update();
            delay(100);
            digitalWrite(DONE,HIGH);
            Serial.printf("Power Off\r\n");
            delay(1000);
        } else {
            Serial.printf("Tx Fail\r\n");
            AtomS3.dis.drawpix(0xFF0000);
            AtomS3.update();
        }
        count ++;

    }

}
