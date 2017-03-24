#include "mbed.h"
#include "rtos.h"
#include <stdlib.h>
#include "math.h"
#include "ctype.h"

//Photointerrupter input pins
#define I1pin D2
#define I2pin D11
#define I3pin D12

//Incremental encoder input pins
#define CHA   D7
#define CHB   D8

//Motor Drive output pins   //Mask in output byte
#define L1Lpin D4           //0x01
#define L1Hpin D5           //0x02
#define L2Lpin D3           //0x04
#define L2Hpin D6           //0x08
#define L3Lpin D9           //0x10
#define L3Hpin D10          //0x20

//Mapping from sequential drive states to motor phase outputs
/*
State   L1  L2  L3
0       H   -   L
1       -   H   L
2       L   H   -
3       L   -   H
4       -   L   H
5       H   L   -
6       -   -   -
7       -   -   -
*/
//Drive state to output table
const int8_t driveTable[] = {0x12,0x18,0x09,0x21,0x24,0x06,0x00,0x00};

//Mapping from interrupter inputs to sequential rotor states. 0x00 and 0x07 are not valid
const int8_t stateMap[] = {0x07,0x05,0x03,0x04,0x01,0x00,0x02,0x07};
//const int8_t stateMap[] = {0x07,0x01,0x03,0x02,0x05,0x00,0x04,0x07}; //Alternative if phase order of input or drive is reversed

//Phase lead to make motor spin
const int8_t leadf = -2;  //2 for forwards, -2 for backwards
const int8_t leadb = 2;
int8_t lead ;
//Status LED
DigitalOut led1(LED1);

//Photointerrupter inputs
DigitalIn I1(I1pin);
DigitalIn I2(I2pin);
DigitalIn I3(I3pin);

//CHA photointerrupters
DigitalIn C1(CHA);
DigitalIn C2(CHB);

//Motor Drive outputs
PwmOut L1L(L1Lpin);
DigitalOut L1H(L1Hpin);
PwmOut L2L(L2Lpin);
DigitalOut L2H(L2Hpin);
PwmOut L3L(L3Lpin);
DigitalOut L3H(L3Hpin);

//interrupt for rising edge
InterruptIn risebig1(I1pin);
InterruptIn risebig2(I2pin);
InterruptIn risebig3(I3pin);
InterruptIn risesmall1(CHA);
InterruptIn risesmall2(CHB);

//timer for calculating speed
Timer t;

double pi = 3.1415926535897; // define pi

//..................................Global Variables for thread runMotor.............................................................
double velocity = 0.0; //initialise velocity
double si = 0.0;
double integral = 0.0;
double prevDelta=0;
double goalV=0.0  ; //always in revs per milli second
double delta = 0.0;
double ii = 0.0;
double d = 0.0;
double pwmd = 0.0;
double pwmv = 0.0;
double pwm = 0.0; //to be fed to movecw or moveccw

//..................................Global Variables for thread Main...................................................................


//Initialise the serial port
RawSerial pc(SERIAL_TX, SERIAL_RX);

//number of revs
double n = 0.0;
double s = 0.0; //distance to go
double counter = 0.0; //define counter


//define my initial and final times
double initial_time = 0.0 ;
double next_instance = 20.0;


//........................................Variables for puttinput.................................................
char charctr; //ch
double invert_R =1;//R_inv
double j = 0; //i
double int_R =0 ;//R_int
double decimal_R = 0;//R_dec
double decimal_V = 0;//V_dec
double decimal_i = 0;//i_dec
double invert_V = 1;//V_inv
double k = 0;//j
int int_V = 0;//V_int
double decimal_j = 0;//j_dec
double decimal_v = 0;//v_dec
double R_holder = 0;//R
double V_holder = 0;//V
int mode_sett;//mode


typedef enum {
    STATE0 =0, STATE1, STATE2, STATE3, STATE4,
    } STATE;

//...................................................................................................................................
//Set a given drive state
void motorOut(int8_t driveState, double pwm){

    //Lookup the output byte from the drive state.
    int8_t driveOut = driveTable[driveState & 0x07];

    //Turn off first
    if (~driveOut & 0x01) L1L = 0;
    if (~driveOut & 0x02) L1H = 1;
    if (~driveOut & 0x04) L2L= 0;
    if (~driveOut & 0x08) L2H = 1;
    if (~driveOut & 0x10) L3L= 0;
    if (~driveOut & 0x20) L3H = 1;

    //Then turn on
    if (driveOut & 0x01) L1L.write(pwm);
    if (driveOut & 0x02) L1H = 0;
    if (driveOut & 0x04) L2L.write(pwm);
    if (driveOut & 0x08) L2H = 0;
    if (driveOut & 0x10) L3L.write(pwm);
    if (driveOut & 0x20) L3H = 0;
    }

    //Convert photointerrupter inputs to a rotor state
inline int8_t readRotorState(){
    return stateMap[I1 + 2*I2 + 4*I3];
    }

//Basic synchronisation routine
int8_t motorHome() {

    //Put the motor in drive state 0 and wait for it to stabilise
    motorOut(0, 1.0);
    wait(2.0);

    //Get the rotor state
    return readRotorState();
}

//function to call on iterrupt of CHA
void gettimes (){
    initial_time = next_instance;
    next_instance = t.read();
    counter++;
    }

//function to get revs
void counter_(){
    counter++;
    }

//move clockwise
void movecw (int8_t intState1, int8_t intStateOld1,int8_t orState1, double pwm ){
        intState1 = readRotorState();
        if (intState1 != intStateOld1) {
            intStateOld1 = intState1;
            motorOut(((intState1-orState1+lead+6)%6), pwm); //+6 to make sure the remainder is positive
        }

    }

//function for runmotor thread when v and r suplied
void docontrol(){
    int8_t orState = 0;    //Rotot offset at motor state 0
    int8_t intState = 0;
    int8_t intStateOld = 0;

    //k constants for controllers
    double gv = 1.0/3.9, kpv = 4.0, kiv = 0.00001;
    double gd = 1.0/180.0, kpd = 2.4, kdd = 0.3285, kid = 10.0;


    //Run the motor synchronisation
    orState = motorHome();

    while (1){
    pwmv = gv * ((kpv * delta  ) + (kiv * ii));
            pwmd = gd * ((kpd * s) - ( kdd * (velocity)) + kid);
            //pwmd = (1.0/30.0) * ((2.63 * s) - ( 0.3285 * (velocity)) + 10.0);
            if (pwmv < pwmd){
                pwm = pwmv;
                    }
            else {
            pwm = pwmd;
            }
            //pc.printf ("%f, %f, %f \t %f \t%f \t %f \t %f \n" , (4.0 * delta), (0.20 *ii), pwm,pwmv,pwmd,(velocity ), (counter/117.0));
            //pc.printf (" %f \n" , (s));



            if (pwm > 1.0){
                pwm=1.0;
                }
            else if(pwm < -1.0){
                pwm = -1.0;
                }
    movecw (intState,intStateOld,orState,pwm );

    si =s;
    prevDelta=delta;
    Thread::wait (10.0);
    }
    }

void setvals(){
    while (1){
    velocity = 1.0  /((next_instance - initial_time)*117.0);
            s =  n - (counter/117.0);
            //pc.printf ("%f \t %f \t %f \n " , next_instance , initial_time,counter/117.0);
            integral = (s - si) ;
            delta=goalV-velocity;
            //d=prevDelta-delta/(next_instance -initial_time);
            ii += delta*(next_instance -initial_time);
            Thread::wait (10.0);
    }

    }


//function for runmotor thread when r suplied
void docontrolr(){
    int8_t orState = 0;    //Rotot offset at motor state 0
    int8_t intState = 0;
    int8_t intStateOld = 0;

    //k constants for controllers
    double gv = 1.0/4.0, kpv = 4.0, kiv = 0.00001;
    double gd = 1.0/250.0, kpd = 2.4, kdd = 3.4, kid = 16.0;

    //Run the motor synchronisation
    orState = motorHome();

    while (1){
    pwmv = gv * ((kpv * delta  ) + (kiv * ii));
            pwmd = gd * ((kpd * s) - ( kdd * (velocity)) + kid);
            //pwmd = (1.0/30.0) * ((2.63 * s) - ( 0.3285 * (velocity)) + 10.0);
            if (pwmv < pwmd){
                pwm = pwmv;
                    }
            else {
            pwm = pwmd;
            }
            //pc.printf ("%f, %f, %f \t %f \t%f \t %f \t %f \n" , (4.0 * delta), (0.20 *ii), pwm,pwmv,pwmd,(velocity ), (counter/117.0));
            //pc.printf (" %f \n" , (s));



            if (pwm > 1.0){
                pwm=1.0;
                }
            else if(pwm < -1.0){
                pwm = -1.0;
                }
    movecw (intState,intStateOld,orState,pwm );

    si =s;
    prevDelta=delta;
    Thread::wait (10.0);
    }
    }



//function for runmotor thread when v suplied
void docontrolv(){
    int8_t orState = 0;    //Rotot offset at motor state 0
    int8_t intState = 0;
    int8_t intStateOld = 0;

    //k constants for controllers
    double gv = 1.0/4.0, kpv = 4.0, kiv = 0.00001;
    double gd = 1.0/250.0, kpd = 2.4, kdd = 0.3285, kid = 10.0;

    //Run the motor synchronisation
    orState = motorHome();

    while (1){
    pwmv = gv * ((kpv * delta  ) + (kiv * ii));
            pwmd = gd * ((kpd * s) - ( kdd * (velocity)) + kid);
            //pwmd = (1.0/30.0) * ((2.63 * s) - ( 0.3285 * (velocity)) + 10.0);

            //pc.printf ("%f, %f, %f \t %f \t%f \t %f \t %f \n" , (4.0 * delta), (0.20 *ii), pwm,pwmv,pwmd,(velocity ), (counter/117.0));
            //pc.printf (" %f \n" , (s));
        pwm = pwmv;


            if (pwm > 1.0){
                pwm=1.0;
                }
            else if(pwm < -1.0){
                pwm = -1.0;
                }
    movecw (intState,intStateOld,orState,pwm );

    si =s;
    prevDelta=delta;
    Thread::wait (10.0);
    }
    }





//..............................................puttyinput.......................................................
void Puttyinput(){
    STATE current_state = STATE0;
    while (1) {
        printf("Input command: \n\r");
        while ((charctr = pc.getc()) != '\r') {
            pc.putc(charctr);
            switch (current_state) {
            case STATE0:
                if (charctr == 'R')
                    current_state = STATE1;
                else if (charctr == 'V')
                    current_state = STATE3;
                break;
            case STATE1:

                if (charctr == '-') {
                    current_state = STATE1;
                    invert_R = -1;
                } else if (charctr == '.')
                    current_state = STATE2;
                else if (charctr == 'V')
                    current_state = STATE3;
                else if (isdigit(charctr)){
                    j++;
                    current_state = STATE1;
                    int_R = int_R*10 + (double)int(charctr - 48);
                }
                break;
            case STATE2:
                if (charctr == 'V')
                    current_state = STATE3;
                else if (isdigit(charctr)) {
                    decimal_i++;
                    current_state = STATE2;
                   if(decimal_i == 1){
                        decimal_R = decimal_R + (double)int(charctr-48)/10;
                    }
                    if(decimal_i == 2){
                        decimal_R = decimal_R +(double)int(charctr-48)/100;
                    }
                }
                break;
            case STATE3:
                if (charctr == '-') {
                    current_state = STATE3;
                    invert_V = -1;
                } else if (charctr == '.')
                    current_state = STATE4;
                else if (isdigit(charctr)) {
                    k++;
                    current_state = STATE3;
                   int_V = int_V*10 + (double)int(charctr-48);
                }
                break;
            case STATE4:
                if (isdigit(charctr)) {
                    decimal_j++;
                    current_state = STATE4;
                  if (decimal_j == 1) {
                        decimal_V = decimal_V + (double) int(charctr - 48) / 10;
                    }
                if(decimal_j == 2){
                        decimal_V = decimal_V + (double) int(charctr - 48) / 100;
                    }
                }
                break;
            default:
                current_state = STATE0;
                break;
            }
        }   //end inner while

        R_holder = invert_R * (int_R + decimal_R);
        V_holder = invert_V * (int_V + decimal_V);


        if(R_holder!=0 && V_holder!= 0) mode_sett = 3;
        if(R_holder!=0 && V_holder==0) mode_sett=2;
        if(R_holder==0 && V_holder!=0) mode_sett=1;

        break;
    }
    n = (float)R_holder;
    goalV = (float)V_holder;
    }


//define thread to run control
Thread runMotor  (osPriorityNormal, 800, NULL);
Thread setVals (osPriorityNormal, 800, NULL);
Thread onlyV (osPriorityNormal, 400, NULL);
Thread onlyR (osPriorityNormal, 800, NULL);
//


int main(){

    Puttyinput();
    if (n >= 0) {
        lead = leadf;

        }
    else {
        n = - 1.0 * n;
        lead = leadb;
        }
    t.start();



    //define my initial and final times
    initial_time = t.read();
    wait (1.0);
    next_instance =  t.read();

    //interrupt sequence for counting
    risesmall1.rise(&gettimes );

    if (n != 0.0 and goalV != 0.0 ){
        runMotor.start(docontrol);
        }
    else if (n == 0.0){
        onlyV.start(docontrolv);
        }
    else if (goalV == 0.0){
        goalV = 15.0;
        onlyR.start(docontrolr);
        }
    setVals.start(setvals);
    //pc.printf ("%A \n" , runMotor.stack_size());
    //pc.printf ("%f \t %f \t %f \n " , s,n,counter);

    while (1){

            //pc.printf ("%f, %f, %f \t %f \t%f \t %f \t %f \n" , (4.0 * delta), (0.00001 *ii), pwm,pwmv,pwmd,(velocity ), (counter/117.0));
            Thread::wait(500.0);
              }


    }
