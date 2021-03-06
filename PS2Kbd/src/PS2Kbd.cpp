#include <Arduino.h>
#include "PS2Kbd.h"
 
//http://retired.beyondlogic.org/keyboard/keybrd.htm

PS2Kbd* PS2Kbd::keyboardPtr;


const char PS2Kbd::chrsNS[]={
    0,    249,  0,    245,  243,  241,  242,  252,  0,    250,  248,  246,  244,  '\t', '`',  0,
    0,    0,    0,    0,    0,    'q',  '1',  0,    0,    0,    'z',   's',  'a',  'w',  '2',  0,
    0,    'c',  'x',  'd',  'e',  '4',  '3',  0,    0,    ' ',  'v',  'f',  't',  'r',  '5',  0,
    0,    'n',  'b',  'h',  'g',  'y',  '6',  0,    0,    0,    'm',  'j',  'u',  '7',  '8',  0,
    0,    ',',  'k',  'i',  'o',  '0',  '9',  0,    0,    '.',  '/',  'l',  ';',  'p',  '-',  0,
    0,    0,    '\'', 0,    '[',  '=',  0,    0,    0,    0,    '\n', ']',  0,    '\\', 0,    0,
    0,    0,    0,    0,    0,    0,    '\b', 0,    0,    '1',  0,    '4',  '7',  0,    0,    0,
    '0',  '.',  '2',  '5',  '6',  '8',  '\033',0,   251,  '+',  '3',  '-',  '*',  '9',  0,    0,
    0,    0,    0,    247,  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0};

const char PS2Kbd::chrsSH[]={
    0,    249,  0,    245,  243,  241,  242,  252,  0,    250,  248,  246,  244,  '\t', '~',  0,
    0,    0,    0,    0,    0,    'Q',  '!',  0,    0,    0,    'Z',  'S',  'A',  'W',  '@',  0,
    0,    'C',  'X',  'D',  'E',  '$',  '#',  0,    0,    ' ',  'V',  'F',  'T',  'R',  '%',  0,
    0,    'N',  'B',  'H',  'G',  'Y',  '^',  0,    0,    0,    'M',  'J',  'U',  '&',  '*',  0,
    0,    '<',  'K',  'I',  'O',  ')',  '(',  0,    0,    '>',  '?',  'L',  ':',  'P',  '_',  0,
    0,    0,    '\"', 0,    '{',  '+',  0,    0,    0,    0,    '\n', '}',  0,    '|',  0,    0,
    0,    0,    0,    0,    0,    0,    '\b', 0,    0,    '1',  0,    '4',  '7',  0,    0,    0,
    '0',  '.',  '2',  '5',  '6',  '8',  '\033',0,   0,    '+',  '3',  '-',  '*',  '9',  0,    0,
    0,    0,    0,    247,  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0};

uint8_t PS2Kbd::getModifiers() {
    return modifs;
}

void PS2Kbd::send(uint8_t x) {
    bool d=true;
    dirOUT=true;
    for(uint8_t i=0;i<8;i++) {
        if((x&(1<<i))!=0) {
            d=!d;
        }
    }
    uint16_t shift=x|(0x200)|(0x100*d);
    digitalWrite(clkPin,LOW);
    delayMicroseconds(60);
    digitalWrite(dataPin,LOW);
    delayMicroseconds(1);
    digitalWrite(clkPin,HIGH);
    for(uint8_t i=0;i<10;i++) {
        while(digitalRead(clkPin));
        while(!digitalRead(clkPin));
        digitalWrite(dataPin,shift&1);
        shift>>=1;
    }
    digitalWrite(dataPin,HIGH);
    while(digitalRead(clkPin));
    while(!digitalRead(clkPin));
    dirOUT=false;
}

bool PS2Kbd::isReading(){
    return reading;
}

uint8_t PS2Kbd::available() {
    return toChar-fromChar;
}

unsigned char PS2Kbd::read() {
    if(fromChar>=toChar)return '\0';
    return charBuffer[fromChar++];
}

uint8_t PS2Kbd::availableRaw() {
    return toRaw-fromRaw;
}

unsigned char PS2Kbd::readRaw() {
    if(fromRaw>=toRaw)return '\0';
    return keyScancodeBuffer[fromRaw++];
}

void PS2Kbd::waitACK() {
    while(!ACK);
    ACK=false;
}


void PS2Kbd::tryUpdateLEDs() {
    if(!updLEDs)return;
    updLEDs=false;
    kstate=0;
    send(0xed);
    delay(100);
    send(scrlk|(numlk<<1)|(cpslk<<2));
}

void PS2Kbd::bufferWriteScancode(uint8_t scc) {
    if(toRaw + 1 >= sizeof(keyScancodeBuffer)) {
        toRaw = 0;
        fromRaw = 0;
    }
    keyScancodeBuffer[toRaw++]=scc;
}

void PS2Kbd::bufferWriteChar(char ch) {
    if(toChar + 1 >= sizeof(charBuffer)) {
        toChar = 0;
        fromChar = 0;
    }
    charBuffer[toChar++]=ch;
}

void PS2Kbd::setLeds(uint8_t d) {
    send(0xed);
    send(d&7);
}

void PS2Kbd::interruptHandler() {
    Serial.println("interrupt");
static uint32_t prev_ms=0;

    if(dirOUT)return;
    
    uint32_t now_ms =micros();
    uint32_t time= now_ms - prev_ms;

uint8_t data = digitalRead(dataPin);
if(time > 70){
	prev_ms = now_ms;
    shift>>=1;
    shift|=(data<<10);
    if(++rc==11){
        rc=0;
        if((shift&0x401)==0x400){
            uint8_t data=(shift>>1)&0xff;
            bufferWriteScancode(data);
            switch(kstate) {
                case 0:
                    //pressed key (nothing special expected)
                    switch(data) {
                        case 0xE0:
                            kstate = 1;
                            break;
                        case 0xE1:
                            /*
                            * Don't care about this code...
                            * ...or...
                            * well, key Pause/Break has 8 bytes (not bits!) long scancode.
                            * Luckily it's the only key (as far as I know) starting with 0xE1 so it can skip next 7 bytes.
                            * The whole scancode is: E1 14 77 E1 F0 14 F0 77
                            */
                            kstate = 2;
                            skipCount = 7;
                            break;
                        case 0xF0:
                            //code meaning next byte will be key which was released
                            kstate = 3;
                            break;
                        case 0x58:
                            //caps lock
                            cpslk = !cpslk;
                            break;
                        case 0x7E:
                            //num lock
                            numlk = !numlk;
                            break;
                        case 0x77:
                            //scroll lock
                            scrlk = !scrlk;
                            break;
                        case 0x11:
                            //left alt
                            modifs|=L_ALT;
                            break;
                        case 0x12:
                            //left shift
                            modifs|=L_SHIFT;
                            break;
                        case 0x14:
                            //left ctrl
                            modifs|=L_CTRL;
                            break;
                        case 0x59:
                            //rght shift
                            modifs|=R_SHIFT;
                            kstate = 0; 
                            break;
                        default:
                            if(modifs&SHIFT) {
                                bufferWriteChar(chrsSH[data]);
                            }
                            else if (cpslk && chrsNS[data] < 127){
                                bufferWriteChar(toUpperCase(chrsNS[data]));
                            }
                            else {
                                bufferWriteChar(chrsNS[data]);
                            }
                            break;
                    }
                    break;
                case 1:
                    //pressed (or released) extended code key (first byte was 0xE0)
                    switch(data) {
                        case 0xF0:
                            kstate = 4;
                            break;
                        case 0x11:
                            //rght alt
                            modifs|=R_ALT;
                            kstate = 0; 
                            break;
                        case 0x14:
                            //rght ctrl
                            modifs|=R_CTRL;
                            kstate = 0; 
                            break;
                        case 0x4a:
                            //slash on numpad
                            bufferWriteChar('/');
                            kstate = 0; 
                            break;
                        case 0x5a:
                            //home
                            bufferWriteChar('\n');
                            kstate = 0; 
                            break;
                        case 0x6b:
                            //left arrow
                            bufferWriteChar('\x80');
                            kstate = 0; 
                            break;
                        case 0x6c:
                            //home
                            bufferWriteChar('\r');
                            kstate = 0; 
                            break;
                        case 0x69:
                            //end
                            bufferWriteChar('\x88');
                            kstate = 0; 
                            break;
                        case 0x70:
                            //page down
                            bufferWriteChar('\x85');
                            kstate = 0; 
                            break;
                        case 0x71:
                            //delete
                            bufferWriteChar('\x7F');
                            kstate = 0; 
                            break;
                        case 0x72:
                            //down arrow
                            bufferWriteChar('\x81');
                            kstate = 0; 
                            break;
                        case 0x74:
                            //right arrow
                            bufferWriteChar('\x82');
                            kstate = 0; 
                            break;
                        case 0x75:
                            //up arrow
                            bufferWriteChar('\x83');
                            kstate = 0; 
                            break;
                        case 0x7a:
                            //page down
                            bufferWriteChar('\x87');
                            kstate = 0; 
                            break;
                        case 0x7d:
                            //page up
                            bufferWriteChar('\x86');
                            kstate = 0; 
                            break;
                        default:
                            kstate = 0; 
                    }
                    break;
                case 2:
                    //pressed key starting with 0xE1 - should be Pause/Break 
                    if(skipCount <= 0) {
                        kstate = 0;
                        bufferWriteChar('\x89');
                    }
                    else {
                        skipCount--;
                    }
                    break;
                case 3:
                    //released key
                    switch(data) {
                        case 0x11:
                            //left alt
                            modifs&=!L_ALT;
                            break;
                        case 0x12:
                            //left shift
                            modifs&=!L_SHIFT;
                            break;
                        case 0x14:
                            //left ctrl
                            modifs&=!L_CTRL;
                            break;
                        case 0x59:
                            //right shift
                            modifs&=!R_SHIFT;
                            break;
                    }
                    kstate = 0;
                    break;
                case 4:
                    //released extended code key
                    switch(data) {
                        case 0x11:
                            //right alt
                            modifs&=!R_ALT;
                            break;
                        case 0x14:
                            //right ctrl
                            modifs&=!R_CTRL;
                            break;
                    }   
                    kstate = 0;
                    break;
            }
        }
        shift=0;
    }
}
    
}

void PS2Kbd::clearBuffers() {
    toRaw = 0;
    fromRaw = 0;
    toChar = 0;
    fromChar = 0;
}

void PS2Kbd::begin() {
    pinMode(dataPin,INPUT_PULLUP);
    pinMode(clkPin,INPUT_PULLUP);
    keyboardPtr = this;
    attachInterrupt(digitalPinToInterrupt(clkPin), kbdInterrupt, FALLING);
    reading = true;
}

PS2Kbd::PS2Kbd(uint8_t dataPin, uint8_t clkPin)
    :dataPin(dataPin),
    clkPin(clkPin),
    shift(0),
    modifs(0),
    cpslk(false),
    scrlk(false),
    numlk(false),
    dirOUT(false),
    kstate(0),
    skipCount(0),
    rc(0),
    CHARS(0x90),
    fromChar(0),
    toChar(0),
    fromRaw(0),
    toRaw(0),
    ACK(false),
    updLEDs(false)

{}

void PS2Kbd::kbdInterrupt() {
    keyboardPtr->interruptHandler();
}

PS2Kbd::~PS2Kbd() {
    detachInterrupt(clkPin);
}