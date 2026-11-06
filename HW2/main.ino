#include <LiquidCrystal.h>
#include <EEPROM.h>

#define CURRENT_VERSION 1

#define CURRENT 1
#define EXTREMES 0
#define CELSIUS 1
#define FAHRENHEIT 0

// Set these as needed
#define BUTTON_A 2
#define BUTTON_B 3
#define D4 5
#define D5 7
#define D6 8
#define D7 10
#define REGISTER_SELECT 12
#define ENABLE 13
#define SENSOR A5
#define DEBOUNCE_INTERVAL_MS 200

#define trueV 5.057
#define nominalR 4700
#define beta 3950

byte degreeSymbol[8] = {
    B00110,                 // ..##.
    B01001,                 // .#..#
    B01001,                 // .#..#
    B00110,                 // ..##.
    B00000,                 // .....
    B00000,                 // .....
    B00000,                 // .....
};

struct
{
    int version;
    double min;
    double max;
    bool scale;
} memory;

LiquidCrystal lcd(REGISTER_SELECT, ENABLE, D4, D5, D6, D7);
unsigned long lastButtonA = 0;
unsigned long lastButtonB = 0;
unsigned long lastRead = 0;
double read;
bool mode = CURRENT;

// |============================== Interrupts ==============================|

ISR(TIMER1_COMPA_vect)
{
    OCR1A += 62500;
    display();
}

void pressedA()
{
    // Debouncing
    if (millis() - lastButtonA < DEBOUNCE_INTERVAL_MS)
        return;
    lastButtonA = millis();

    mode = !mode;
    display();
}

void pressedB()
{
    // Debouncing
    if (millis() - lastButtonB < DEBOUNCE_INTERVAL_MS)
        return;
    lastButtonB = millis();

    if (mode == CURRENT)
    {
        memory.scale = !memory.scale;
        EEPROM.put(0, memory);
    }
    else
    {
        initializeMemory();
    }
    display();
}

// |========================================================================|

void setup()
{
    Serial.begin(9600);
    
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    
    // Button interrupt setup
    attachInterrupt(digitalPinToInterrupt(BUTTON_A), pressedA, FALLING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_B), pressedB, FALLING);
    
    lcd.createChar(0, degreeSymbol);
    lcd.begin(16, 2);

    // Needed for sensor to stabilize
    lcd.print("Powering on");
    for (int i = 0; i < 3; i++)
    {
        delay(500);
        lcd.print(".");
    }
    delay(500);

    read = getTemp(SENSOR);
    EEPROM.get(0, memory);
    if (memory.version != CURRENT_VERSION)
    {
        Serial.println("updating");
        initializeMemory();
    }
    display();

    // Timer interrupt setup
    noInterrupts();
    TCCR1A = 0;           // Init Timer1
    TCCR1B = 0;           // Init Timer1
    TCCR1B |= B00000100;  // Prescalar = 256
    OCR1A = 62500;        // Timer CompareA Register
    TIMSK1 |= B00000010;  // Enable Timer COMPA Interrupt
    interrupts();
}

float getTemp(uint8_t pin)
{ 
    double V = (trueV*analogRead(pin))/1023;
    double R = (trueV*1000)/V - 1000;
    double temp = (298.15*beta)/(beta + 298.15*log(R/nominalR));
    double celsius = temp - 273.15;
    return celsius;
}

void initializeMemory()
{
    memory.version = CURRENT_VERSION;
    memory.min = read;
    memory.max = read;
    memory.scale = CELSIUS;
    EEPROM.put(0, memory);
}

double celsiusToFahrenheit(double celsius)
{
    return (9 * celsius)/5 + 32;
}

void printDegrees(float x)
{
    lcd.print((memory.scale ? x : celsiusToFahrenheit(x)));
    lcd.write(byte(0));
    lcd.write((memory.scale ? 'C' : 'F'));
}

void display()
{
    lcd.clear();
    if (mode == CURRENT)
    {
        lcd.print("Temperature:");
        lcd.setCursor(0, 1);
        printDegrees(read);
        lcd.setCursor(15, 0);
        lcd.write(0xFF);
    }
    else if (mode == EXTREMES)
    {
        lcd.print("Min: ");
        printDegrees(memory.min);
        lcd.setCursor(0, 1);
        lcd.print("Max: ");
        printDegrees(memory.max);
        lcd.setCursor(15, 1);
        lcd.write(0xFF);
    }
}

void loop()
{
    read = getTemp(SENSOR);
    if (read < memory.min)
        memory.min = read;
    if (read > memory.max)
        memory.max = read;
    EEPROM.put(0, memory);
}