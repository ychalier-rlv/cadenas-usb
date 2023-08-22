#include <Servo.h>
#include "pitches.h"

#define NLOCKS 8

#define PIN_LED_LOCK_1 2
#define PIN_LED_LOCK_2 3
#define PIN_LED_LOCK_3 4
#define PIN_LED_LOCK_4 5
#define PIN_LED_LOCK_5 7
#define PIN_LED_LOCK_6 8
#define PIN_LED_LOCK_7 9
#define PIN_LED_LOCK_8 12
#define PIN_BUTTON 6
#define PIN_SERVO 10
#define PIN_BUZZER 11

#define LONG_BUTTON_PRESS_DURATION 3000 //ms

#define BAUDRATE 9600

#define BUFFER_SIZE 4

#define OP_LOCK 97  //a
#define OP_UNLOCK 98  //b
#define OP_FAILED_ATTEMPT 99  //c
#define OP_RESET 100  //d
#define OP_RELEASE 101  //e
#define OP_READY 102  //f

#define STATE_CLOSED 0
#define STATE_RELEASED 1

const byte BUFFER_START = 33; // !
const byte BUFFER_END = 63; // ?

Servo servo;

const int pin_leds[NLOCKS] = {PIN_LED_LOCK_1, PIN_LED_LOCK_2, PIN_LED_LOCK_3, PIN_LED_LOCK_4, PIN_LED_LOCK_5, PIN_LED_LOCK_6, PIN_LED_LOCK_7, PIN_LED_LOCK_8};
bool locks[NLOCKS];
int button_state_current = HIGH;
int button_state_previous = HIGH;
bool button_pressed = false;
unsigned long button_down_timestamp = 0;
unsigned long button_press_duration = 0;
byte buffer[BUFFER_SIZE];
int buffer_cursor = -1;
int state = STATE_CLOSED;
int locks_total_memory = 0;

void buffer_reset()
{
    buffer_cursor = -1;
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        buffer[i] = 0;
    }
}

void buffer_queue(int x)
{
    buffer_cursor = (buffer_cursor + 1) % BUFFER_SIZE;
    buffer[buffer_cursor] = x;
}

byte buffer_read(int i)
{
    return buffer[(buffer_cursor + i + 1) % BUFFER_SIZE];
}

void parse_buffer()
{
    if (buffer_read(0) != BUFFER_START) return;
    if (buffer_read(3) != BUFFER_END) return;
    handle_operation(buffer_read(1), buffer_read(2));
}

void tune_lock()
{
    tone(PIN_BUZZER, 294, 90);
    delay(110);
    tone(PIN_BUZZER, 220, 80);
    delay(80);
    tone(PIN_BUZZER, 110, 90);
    delay(90);   
}

void tune_unlock()
{
    //https://gist.github.com/bboyho/0fcd284f411b1cfc7274336fde6abb45
    tone(PIN_BUZZER, NOTE_G4, 35);
    delay(35);
    tone(PIN_BUZZER, NOTE_G5, 35);
    delay(35);
    tone(PIN_BUZZER, NOTE_G6, 35);
    delay(35);
    noTone(PIN_BUZZER);
}

void tune_success()
{
    //https://gist.github.com/bboyho/0fcd284f411b1cfc7274336fde6abb45
    tone(PIN_BUZZER, NOTE_E6, 125);
    delay(130);
    tone(PIN_BUZZER, NOTE_G6, 125);
    delay(130);
    tone(PIN_BUZZER, NOTE_E7, 125);
    delay(130);
    tone(PIN_BUZZER, NOTE_C7, 125);
    delay(130);
    tone(PIN_BUZZER, NOTE_D7, 125);
    delay(130);
    tone(PIN_BUZZER, NOTE_G7, 125);
    delay(125);
    noTone(PIN_BUZZER);
}

void tune_ready()
{
    tone(PIN_BUZZER, NOTE_B5, 100);
    delay(100);
    tone(PIN_BUZZER, NOTE_E6, 850);
    delay(800);
    noTone(PIN_BUZZER);
    delay(500);
    tone(PIN_BUZZER, NOTE_B5, 100);
    delay(100);
    tone(PIN_BUZZER, NOTE_E6, 850);
    delay(800);
    noTone(PIN_BUZZER);
    delay(500);
}

void force_clear()
{
    for (int i = 0; i < NLOCKS; i++)
    {
        locks[i] = false;
    }
    update();
}

void force_open()
{
    for (int i = 0; i < NLOCKS; i++)
    {
        locks[i] = true;
    }
    update();
}

void handle_operation(byte op, byte arg)
{
    Serial.println("ok");
    int lock_number = arg - 97;
    if (lock_number < 0 || lock_number >= NLOCKS) return;
    switch(op)
    {
        case OP_LOCK:
            locks[lock_number] = false;
            break;
        case OP_UNLOCK:
            locks[lock_number] = true;
            break;
        case OP_FAILED_ATTEMPT:
            tune_lock();
            break;
        case OP_RESET:
            force_clear();
            break;
        case OP_RELEASE:
            force_open();
            break;
        case OP_READY:
            tune_ready();
            break;
        default:
            break;
    }
    update();
}

void update()
{
    bool all_locks = true;
    int locks_total = 0;
    for (int i = 0; i < NLOCKS; i++)
    {
        digitalWrite(pin_leds[i], locks[i]);
        all_locks = all_locks && locks[i];
        if (locks[i])
        {
            locks_total++;   
        }
    }
    
    if (locks_total > locks_total_memory && locks_total == NLOCKS)
    {
        tune_success();
        delay(100);
    }
    else if (locks_total > locks_total_memory)
    {
        for (int i = 0; i < locks_total; i++)
        {
            tune_unlock();
            delay(10);
        }
        delay(10 * (NLOCKS - locks_total));
    }
    else if (locks_total < locks_total_memory)
    {
        tune_lock();
    }
    locks_total_memory = locks_total;

    if (all_locks && state == STATE_CLOSED)
    {
        // Release the key
        servo.write(180);
        state = STATE_RELEASED;
    }
    else if (!all_locks && state == STATE_RELEASED)
    {
        // Close the key
        servo.write(0);
        state = STATE_CLOSED;
    }
    
}

void setup()
{
    for (int i = 0; i < NLOCKS; i++)
    {
        pinMode(pin_leds[i], OUTPUT);
        locks[i] = false;
    }
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    buffer_reset();
    Serial.begin(BAUDRATE);
    servo.attach(PIN_SERVO);
    servo.write(0);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    if (button_pressed)
    {
        button_press_duration = millis() - button_down_timestamp;
        if (button_press_duration > LONG_BUTTON_PRESS_DURATION)
        {
            force_open();
        }
    }
    button_state_current = digitalRead(PIN_BUTTON);
    if (button_state_current != button_state_previous)
    {
        if (button_state_current == LOW)
        {
            // Pressing button
            button_down_timestamp = millis();
            button_pressed = true;
        }
        else
        {
            // Releasing button
            button_pressed = false;
            if (button_press_duration <= LONG_BUTTON_PRESS_DURATION)
            {
                force_clear();
            }
        }
        button_state_previous = button_state_current;
    }
    int bytes_to_read = Serial.available();
    for (int i = 0; i < bytes_to_read; i++)
    {
        byte incoming_byte = Serial.read();
        buffer_queue(incoming_byte);
        parse_buffer();
    }

}
