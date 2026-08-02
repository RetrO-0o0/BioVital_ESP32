import processing.serial.*;

Serial myPort;
int[] vals = new int[800];
int xPos = 0;

void setup() {
  size(800, 400);
  background(0);
  
  println(Serial.list());
  myPort = new Serial(this, Serial.list()[0], 115200);
  myPort.bufferUntil('\n');
}

void draw() {

}

void serialEvent(Serial p) {
    String s = p.readStringUntil('\n');
    if (s != null) {
       s = trim(s);
       if (s.length() > 0) {
         int val = int(s);
         
         stroke(0, 255, 0);
         line(xPos, height, xPos, (height - map(val ,0, 4095, 0, height)));
         
         xPos++;
         if (xPos >= width) {
           xPos = 0;
           background(0);
         }
       }
    }
}
