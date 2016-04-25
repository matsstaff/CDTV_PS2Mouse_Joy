/*
    Arduino PS/2 mouse + joystick to CDTV IR converter
 
 INPUTS:    PIN     DESCRIPTION
 2     PS/2 CLOCK
 4     PS/2 DATA
 
 A0     JOYSTICK RIGHT (DB9 PIN 4)
 A1     JOYSTICK LEFT (DB9 PIN 3)
 A2     JOYSTICK DOWN (DB9 PIN 2)
 A3     JOYSTICK UP (DB9 PIN 1)
 A4     JOYSTICK BTN2 (DB9 PIN 6)
 A5     JOYSTICK BTN1 (DB9 PIN 5)
 
 OUTPUT:      3     CDTV IR DATA
 
 Uses: INT0 for interrupt driven PS/2 communication.
 Timer2 to generate 40kHz IR carrier (PWM).
 Timer1 (OCR1A) to generate CDTV serial protocol.
 
 The CDTV IR protocol uses a 40kHz carrier when transmitting.
 It sends a 9ms start pulse followed by a 4.5ms pause.
 Then it sends 12 bits, each consisting of a 400us pulse, then
 a 400us (for a 0 bit) or 1200us (for a 1 bit) pause. It then sends
 the same 12 bits inversed. Finally a 400us pulse is sent. 
 
 If a button is held. It sends 'repeat' code every 60ms which is a 9ms
 pulse, followed by a 2.1ms pause and lastly a 400us pulse. 
 
 This work was built on information gathered from:
 http://www.amiga.org/forums/archive/index.php/t-60087.html
 https://github.com/hkzlab/AVR-Experiments/tree/master/samples/m128-ir-cdtv
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 */

#define PS2CLKPIN  2  /* MUST be pin 2, as it uses INT0 to read data */
#define PS2DATAPIN 4  

#define IRDATAPIN  3  /* MUST be pin 3, as it uses OCR2B to output PWM */

// Enable debug output on serial
#define DBG 1

// CDTV CODES
#define CDTV_CODE_1              0x001
#define CDTV_CODE_2              0x021
#define CDTV_CODE_3              0x011
#define CDTV_CODE_4              0x009
#define CDTV_CODE_5              0x029
#define CDTV_CODE_6              0x019
#define CDTV_CODE_7              0x005
#define CDTV_CODE_8              0x025
#define CDTV_CODE_9              0x015
#define CDTV_CODE_0              0x039
#define CDTV_CODE_ESCAPE         0x031
#define CDTV_CODE_ENTER          0x035
#define CDTV_CODE_GENLOCK        0x022
#define CDTV_CODE_CD_TV          0x002
#define CDTV_CODE_POWER          0x012
#define CDTV_CODE_REWIND         0x032
#define CDTV_CODE_PLAY_PAUSE     0x00A
#define CDTV_CODE_FAST_FORWARD   0x01A
#define CDTV_CODE_STOP           0x02A
#define CDTV_CODE_VOLUME_UP      0x006
#define CDTV_CODE_VOLUME_DOWN    0x03A
#define CDTV_CODE_MOUSE_A        0x080
#define CDTV_CODE_MOUSE_B        0x040
#define CDTV_CODE_MOUSE_UP       0x020
#define CDTV_CODE_MOUSE_RIGHT    0x004
#define CDTV_CODE_MOUSE_DOWN     0x010
#define CDTV_CODE_MOUSE_LEFT     0x008
#define CDTV_CODE_JOYSTICK_A     0x880
#define CDTV_CODE_JOYSTICK_B     0x840
#define CDTV_CODE_JOYSTICK_UP    0x820
#define CDTV_CODE_JOYSTICK_RIGHT 0x804
#define CDTV_CODE_JOYSTICK_DOWN  0x810
#define CDTV_CODE_JOYSTICK_LEFT  0x808

void weak_pullup(int pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);
}

void pull_down(int pin){
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}

void await_pin_state(uint8_t pin, uint8_t state){
  uint16_t cnt = 0xFFF;
  while((digitalRead(pin) != state) && cnt--);
}

void ps2_write_byte(unsigned char data){
  unsigned char i;
  unsigned char parity = 1;

  // Disable interrupt 0
  EIMSK &= ~(1<<INT0);

  pull_down(PS2CLKPIN);
  delayMicroseconds(200);
  pull_down(PS2DATAPIN);
  delayMicroseconds(10);

  weak_pullup(PS2CLKPIN);
  delayMicroseconds(10);

  /* wait for device to take control of clock */
  await_pin_state(PS2CLKPIN, LOW);
  await_pin_state(PS2CLKPIN, HIGH);

  // clear to send data
  for(i=0; i < 8; i++) {
    if(data & 0x01) {
      weak_pullup(PS2DATAPIN);
    } 
    else {
      pull_down(PS2DATAPIN);
    }
    // wait for clock
    await_pin_state(PS2CLKPIN, LOW);
    await_pin_state(PS2CLKPIN, HIGH);

    parity = parity ^ (data & 0x01);
    data = data >> 1;
  }
  // parity bit
  if (parity) {
    weak_pullup(PS2DATAPIN);
  } else {
    pull_down(PS2DATAPIN);
  }
  await_pin_state(PS2CLKPIN, HIGH);
  await_pin_state(PS2CLKPIN, LOW);

  // stop bit
  weak_pullup(PS2DATAPIN);

  await_pin_state(PS2CLKPIN, HIGH);
  await_pin_state(PS2CLKPIN, LOW);

  // Read ACK
  await_pin_state(PS2CLKPIN, HIGH);
  await_pin_state(PS2DATAPIN, HIGH);

  // Clear interrupt flag
  EIFR |= (1 << INTF0);

  // Re-enable interrupt 0
  EIMSK |= (1<<INT0);
}

volatile uint8_t ps2_head = 0;
volatile uint8_t ps2_tail = 0;
uint8_t ps2_recv_buffer[16];

uint8_t ps2_available(){
  uint8_t avail;
  avail = ps2_tail - ps2_head;
  return avail & 0xF;
}

uint8_t ps2_get_data(){
  uint8_t data = 0;
  if(ps2_tail != ps2_head){
    data = ps2_recv_buffer[ps2_head];
    ps2_head = (ps2_head + 1) & 0xF;
  }
  return data;
}

void ps2_recv_isr(){
  static uint8_t bitcount = 0;
  static uint8_t data;
  static uint8_t last = 0;
  uint8_t curr = millis();

  if(curr-last > 5){  // This adds quite some overhead to the ISR
    bitcount = 0;     // and should not be an issue.
  }                   // However, it ensures recovery if we get out of sync
  last = curr;

  data >>= 1;
  if(digitalRead(PS2DATAPIN) == HIGH){
    data |= 0x80;
  }
  bitcount++;

  if(bitcount == 9){
    uint8_t t_tail = (ps2_tail + 1) & 0xF;
    if(t_tail != ps2_head){
      ps2_recv_buffer[ps2_tail] = data;
      ps2_tail = t_tail;
    }
  } 
  else if(bitcount == 11) {
    bitcount = 0;
  }
}

uint8_t ps2_read_byte(){
  unsigned long start = millis();

  while(!ps2_available()){
    if(millis() - start > 500){
      return 0;
    }
  }
#if DBG
  Serial.print("Received: 0x");
  Serial.println(ps2_recv_buffer[ps2_head], HEX);
#endif
  return ps2_get_data();
}

boolean ps2_mouse_init(){
  unsigned char i;
  boolean init;

  pinMode(PS2CLKPIN, INPUT);
  digitalWrite(PS2CLKPIN, HIGH);
  pinMode(PS2DATAPIN, INPUT);
  digitalWrite(PS2DATAPIN, HIGH);

  delay(100);

  attachInterrupt(0, ps2_recv_isr, FALLING);
  interrupts();

  for(i=0; i<3; i++){
    init = 1;
    ps2_write_byte(0xFF);  // reset
    init &= (ps2_read_byte() == 0xFA); // Read ACK
    init &= (ps2_read_byte() == 0xAA); // Read selftest
    init &= (ps2_read_byte() == 0x00); // Read ID
    delay(10);
  }

  ps2_write_byte(0xF4);  // enable

  init &= (ps2_read_byte() == 0xFA); //  Read ACK

  return init;
}

uint16_t ps2_get_state(){
  static uint8_t ps2state = 0;

  ps2state = ps2state & 0xC0;

  while(ps2_available() >= 3){
    uint8_t ps2status = ps2_get_data();
    if((ps2status & 0xC) == 0x8){
      int8_t ps2dx = ps2_get_data();
      int8_t ps2dy = ps2_get_data();
      ps2state = (ps2status & 0x3) << 6;
      if(ps2dx > 0){
        ps2state |= 0x4;
      } 
      else if(ps2dx < 0){
        ps2state |= 0x8;
      } 
      if(ps2dy > 0){
        ps2state |= 0x20;
      } 
      else if(ps2dy < 0){
        ps2state |= 0x10;
      } 
    }
  }

  return (uint16_t) ps2state;
}

#define IRFREQ 40000
#define IR_ENABLE()  pinMode(IRDATAPIN, OUTPUT)
#define IR_DISABLE()  pinMode(IRDATAPIN, INPUT)
void cdtv_init_transmitter(){
  // Make timer2 generate 40kHz PWM on OCB2 (pin 3)
  pinMode(IRDATAPIN, INPUT);                         // Start with output disabled
  TCCR2A = _BV (WGM20) | _BV (WGM21) | _BV (COM2B1); // fast PWM, clear OC2B on compare match (i.e. non inverting PWM output on OC2B)
  TCCR2B = _BV (WGM22) | _BV (CS21);                 // fast PWM, prescaler of 8
  OCR2A =  ((F_CPU / 8) / IRFREQ) - 1;               // zero relative 
  OCR2B = ((OCR2A + 1) / 4) - 1;                     // 25% duty cycle

  // Setup timer1 to generate timer interrupts (CTC mode)
  noInterrupts(); // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 0xFFFF; // compare match register 16MHz/8
  TCCR1B |= (1 << WGM12); // CTC mode
  //  TCCR1B |= (1 << CS11); // 8 prescaler (but start disabled)
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  interrupts(); // enable all interrupts
}

enum transmitstates {
  ir_idle=0,
  ir_start,
  ir_transmit,
  ir_end_pulse=ir_transmit+48,
  ir_stop,
  ir_repeat
};

volatile uint16_t cdtv_ir_code=0;
volatile uint8_t tx_state=0;
ISR(TIMER1_COMPA_vect) {
  static uint16_t data=0;

  if(tx_state == ir_start){
    IR_DISABLE();
    data = cdtv_ir_code;
    OCR1A = 9000;
    tx_state++;
  } 
  else if(tx_state>=ir_transmit && tx_state<ir_end_pulse){
    if(tx_state & 0x1){
      uint8_t ss;
      IR_DISABLE();
      if(tx_state == ir_transmit + 25){
        data = ~data;
      }
      ss = (tx_state - ir_transmit) >> 1;
      if(ss >= 12){
        ss -= 12;
      }
      OCR1A = (data>>ss & 0x1) ? 2400 : 800;
    } 
    else {
      IR_ENABLE();
      OCR1A = 800;
    }
    tx_state++;
  } 
  else if(tx_state==ir_end_pulse){
    IR_ENABLE();
    OCR1A = 800;
    tx_state++;
  } 
  else if(tx_state==ir_stop){
    IR_DISABLE();
    TCCR1B &= ~(1 << CS11); // disable timer
    tx_state = ir_idle;
  } 
  else if(tx_state==ir_repeat){
    IR_DISABLE();
    OCR1A = 4200;
    tx_state=ir_end_pulse;
  }
}

void cdtv_send_code(uint16_t code){
#if DBG
  Serial.print("Sending IR: 0b");
  Serial.println(code, BIN);
#endif
  cdtv_ir_code=code;
  tx_state=ir_start;
  OCR1A=18000;
  TCNT1 = 0;
  IR_ENABLE();
  TCCR1B |= (1 << CS11); 
}

void cdtv_send_repeat(){
#if DBG
  Serial.println("Repeat");
#endif
  tx_state=ir_repeat;
  OCR1A=18000;
  TCNT1 = 0;
  IR_ENABLE();
  TCCR1B |= (1 << CS11);
}

void joystick_init(){
  weak_pullup(A0);
  weak_pullup(A1);
  weak_pullup(A2);
  weak_pullup(A3);
  weak_pullup(A4);
  weak_pullup(A5);
}

uint16_t joystick_get_state(){
  uint8_t joy_s = (~PINC & 0x3F);
  return joy_s ? (0x800UL | joy_s) : 0;
}

void setup(){
  boolean mouseinit;

#if DBG
  Serial.begin(115200);
  Serial.println("Init PS/2 mouse");
#endif

  mouseinit=ps2_mouse_init();

#if DBG
  if(mouseinit){
    Serial.println("PS/2 mouse initialized");
  } 
  else {
    Serial.println("No PS/2 mouse found");
  }
#endif

#if DBG
  Serial.println("Init joystick");
#endif

  joystick_init();

#if DBG
  Serial.println("Init transmitter");
#endif

  cdtv_init_transmitter();

#if DBG
  Serial.println("Init done");
#endif

}

void loop(){
  static unsigned long last = 0;
  static uint16_t cdtv_last_cmd = 0;
  unsigned long curr = millis();
  unsigned long diff = curr-last;

  if(diff > 59){
    uint16_t joy_state=joystick_get_state();
    uint16_t ps2_state=ps2_get_state();
    uint16_t my_state = joy_state ? joy_state : ps2_state;

    if(my_state){
      if(my_state != cdtv_last_cmd){
        cdtv_send_code(my_state);
      } 
      else {
        cdtv_send_repeat();
      }
      last = curr;
    }
    cdtv_last_cmd = my_state;
  }

}


