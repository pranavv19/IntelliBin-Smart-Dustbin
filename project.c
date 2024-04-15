#include <LPC17xx.h>
#include <system_LPC17xx.h>

#define PRESCALE 29999999
#define TRIG (1 << 15)     // P0.15
#define ECHO (1 << 16)     // P0.16
#define LED_PIN (1 << 8)   // P0.8
#define STEPPER_CTRL_MASK 0xF << 4  // P0.4 to P0.7 for stepper motor controls

void timer21(int mr){
LPC_TIM0->TCR=0X2;
LPC_TIM0->CTCR=0;
LPC_TIM0->MCR=0X4;
LPC_TIM0->EMR=0X20;
LPC_TIM0->MR0=mr;
LPC_TIM0->PR=7999;
LPC_TIM0->TCR=0X1;

while(!(LPC_TIM0->EMR&0X1));
}






unsigned long int var1;
unsigned long int i, j, k;
int row, x, col, key;
int echoTime = 5000;
float distance = 0;

enum State {
    IDLE,
    ROTATE_CLOCKWISE,
    WAIT,
    ROTATE_ANTICLOCKWISE
};

enum State currentState = IDLE;
int stepCounter = 0;
int waitCounter = 0;

void initTimer0(void);
void startTimer0(void);
float stopTimer0(void);
void delayUS(unsigned int microseconds);
void delay(unsigned int r1);
void controlStepperMotor(int enable);
void rotateOnceClockwise(void);
void rotateOnceAnticlockwise(void);


int main() {
    SystemInit();
    SystemCoreClockUpdate();
    initTimer0();

    // Configuring trigger, echo, light, fan, buzzer
    LPC_PINCON->PINSEL0 &= 0x3fffc0ff;
    LPC_PINCON->PINSEL1 &= 0xfffffffc;

    // Configuring keyboard
    LPC_PINCON->PINSEL3 &= 0;
    LPC_PINCON->PINSEL4 &= 0;
    LPC_GPIO1->FIODIR |= 0 << 16 | 0 << 23; // Direction for ECHO PIN and keyboard
    LPC_GPIO0->FIODIR |= TRIG;
    LPC_GPIO2->FIODIR |= 0xf << 10;
    LPC_GPIO0->FIODIR |= 0x00000070; // Direction for Light, fan, and buzzer
    LPC_GPIO0->FIODIR |= LED_PIN;    // Direction for the LED
    LPC_GPIO0->FIODIR |= STEPPER_CTRL_MASK;  // Direction for stepper motor controls

    LPC_GPIO0->FIOCLR = TRIG; // Ensure TRIG is initially low

    while (1) {
        LPC_GPIO0->FIOSET = TRIG; // Output 10us HIGH on TRIG pin
        delayUS(10);
        LPC_GPIO0->FIOCLR = TRIG;

        while (!(LPC_GPIO0->FIOPIN & ECHO)) {
            // Wait for a HIGH on ECHO pin
        }
        startTimer0();
        while (LPC_GPIO0->FIOPIN & ECHO)
            ; // Wait for a LOW on ECHO pin
        echoTime = stopTimer0();      // Stop Counting
        distance = (0.0343 * echoTime) / 40;

       if (distance < 40) {
            if (currentState == IDLE) {
                LPC_GPIO0->FIOSET = LED_PIN;  // Turn on the LED
                currentState = ROTATE_CLOCKWISE;
                stepCounter = 0;
            }
        } else {
            if (currentState == ROTATE_CLOCKWISE) {
                // Finished opening, move to WAIT state
                currentState = WAIT;
                waitCounter = 0;
            }
        }

        // State machine logic
             switch (currentState) {
            case ROTATE_CLOCKWISE:
                if (stepCounter < 50) {
                    controlStepperMotor(1); // Clockwise rotation
                    stepCounter++;
                } else {
                    currentState = WAIT;
                    stepCounter = 0;  // Reset the step counter
                }
                break;
            case WAIT:
                delay(7000000); // 1 second delay
                waitCounter++;
                if (waitCounter >= 4) { // 4 seconds elapsed
                    currentState = ROTATE_ANTICLOCKWISE;
                    stepCounter = 0;  // Reset the step counter
                }
                break;
            case ROTATE_ANTICLOCKWISE:
                if (stepCounter < 50) {
                    controlStepperMotor(0); // Anticlockwise rotation
                    stepCounter++;
                } else {
                    controlStepperMotor(-1); // Stop the motor
                    currentState = IDLE;
                    LPC_GPIO0->FIOCLR = LED_PIN;  // Turn off the LED
                    stepCounter = 0;  // Reset the step counter
                }
                break;
            default:
                break;
        }

        delay(88000); // Delay between sensor checks
    }
}

void initTimer0(void) {
    // Timer for distance
    LPC_TIM0->CTCR = 0x0;
    LPC_TIM0->PR = 11999999;
    LPC_TIM0->TCR = 0x02; //
    LPC_TIM0->TCR = 0x02; // Reset Timer
}

void startTimer0(void) {
    LPC_TIM0->TCR = 0x02; // Reset Timer
    LPC_TIM0->TCR = 0x01; // Enable timer
}

float stopTimer0(void) {
    LPC_TIM0->TCR = 0x0;
    return LPC_TIM0->TC;
}

void delayUS(unsigned int microseconds) {
    LPC_SC->PCLKSEL0 &= ~(0x3 << 2); // Set PCLK_TIMER0 to divide by 1
    LPC_TIM0->TCR = 0x02;             // Reset timer
    LPC_TIM0->PR = 0;                 // Set prescaler to 0
    LPC_TIM0->MR0 = microseconds - 1; // Set match register for the specified microseconds
    LPC_TIM0->MCR = 0x01;             // Interrupt on match
    LPC_TIM0->TCR = 0x01;             // Enable timer

    while ((LPC_TIM0->IR & 0x01) == 0)
        ; // Wait for interrupt flag
    LPC_TIM0->TCR = 0x00; // Disable timer
    LPC_TIM0->IR = 0x01;
}

void delay(unsigned int r1) {
    volatile int r;
    for (r = 0; r < r1; r++)
        ;
}

void controlStepperMotor(int action) {
    if (action == 1) {
        // Clockwise rotation
        LPC_GPIO0->FIOPIN |= STEPPER_CTRL_MASK;
        for (j = 0; j < 150; j++) {
            rotateOnceClockwise();
        }
    } else if (action == 0) {
        // Anticlockwise rotation
        LPC_GPIO0->FIOPIN |= STEPPER_CTRL_MASK;
        for (j = 0; j < 3; j++) {
            rotateOnceAnticlockwise();
        }
    } else {
        // Stop the motor
        LPC_GPIO0->FIOPIN &= ~STEPPER_CTRL_MASK;
    }
}

void rotateOnceClockwise() {
    var1 = 0x8;
    for (i = 0; i < 4; i++) {
        var1 = var1 << 1;
        LPC_GPIO0->FIOPIN = ~var1;
        for (k = 0; k < 25000; k++)
            ;
    }
}

void rotateOnceAnticlockwise() {
    var1 = 0x100;
    for (i = 0; i < 4; i++) {
        var1 = var1 >> 1;
        LPC_GPIO0->FIOPIN = ~var1;
        for (k = 0; k < 25000; k++)
            ;
    }
}


