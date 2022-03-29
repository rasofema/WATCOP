#include <Servo.h>

#define SERVO_PIN A3

Servo myServo;
int pos;
void setup() {
    myServo.attach(SERVO_PIN);
    for (pos = 0; pos <= 30; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myServo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(100);                       // waits 15ms for the servo to reach the position
  }

  for (pos = 30; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    myServo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(100);                       // waits 15ms for the servo to reach the position
  }

}

void loop() {
    
}
