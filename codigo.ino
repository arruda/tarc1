/*
 * IRrecord: record and play back IR signals as a minimal
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * An IR LED must be connected to the output PWM pin 3.
 * A button must be connected to the input BUTTON_PIN; this is the
 * send button.
 * A visible LED can be connected to STATUS_PIN to provide status.
 *
 * The logic is:
 * If the button is pressed, send the IR code.
 * If an IR code is received, record it.
 *
 * Version 0.11 September, 2009
 * Copyright 2009 Ken Shirriff
 * http://arcfn.com
 */

#include <IRremote.h>

int RECV_PIN = 11;
int FIRST_BUTTON_PIN = 12;
//================================================================================================================================
// int SECOND_BUTTON_PIN = 3; //@Lanchinho : nao necessariamente neste pino/ casa, pode ser qlqer outra que esteja livre
// int THIRD_BUTTON_PIN = 4;  //@Lanchinho: Vou deixar todas as cagadas, digo alterações feitas por mim separadas por um "========="
//================================================================================================================================
int STATUS_PIN = 13;

IRrecv irrecv(RECV_PIN);
IRsend irsend; //lembrar de por PWM 3 como o IR LED

decode_results results;

//Struct que guarda infos do botao, no caso sobre o codigo IR que ele deve reproduzir quando apertado
typedef struct Button_Info{
    int codeType;// -1 should be the default
    int toggle;  //0 should be the default; The RC5/6 toggle state
    unsigned long codeValue; // The code value if not raw
    unsigned int rawCodes[RAWBUF]; // The durations if raw
    int codeLen; // The length of the code
} Button_Info;

//=====================
//VARIAVEIS GLOBAIS
//=====================
int last_pressed_button = 0; //por default o ultimo botao pressionado vai ser o primeiro

Button_Info button_one = { -1, 0 };// inicializando codeType =-1 e toggle=0;

//lista dos botoes
Button_Info buttons_list[1] = { button_one };

//=====================
//VARIAVEIS GLOBAIS - FIM
//=====================


void setup()
{
    Serial.begin(9600);
    irrecv.enableIRIn(); // Start the receiver
    pinMode(FIRST_BUTTON_PIN, INPUT);
    // pinMode(SECOND_BUTTON_PIN, INPUT);
    // pinMode(THIRD_BUTTON_PIN, INPUT);
    pinMode(STATUS_PIN, OUTPUT);
}

// Storage for the recorded code
// int codeType = -1; // The type of code
// unsigned long codeValue; // The code value if not raw
// unsigned int rawCodes[RAWBUF]; // The durations if raw
// int codeLen; // The length of the code
// int toggle = 0; // The RC5/6 toggle state


// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results, int pressed_button)
{
    buttons_list[pressed_button].codeType = results->decode_type;
    int count = results->rawlen;
    if (buttons_list[pressed_button].codeType == UNKNOWN)
    {
        Serial.println("Received unknown code, saving as raw");
        buttons_list[pressed_button].codeLen = results->rawlen - 1;
        // To store raw codes:
        // Drop first value (gap)
        // Convert from ticks to microseconds
        // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
        for (int i = 1; i <= buttons_list[pressed_button].codeLen; i++)
        {
            if (i % 2)
            {
                // Mark
                buttons_list[pressed_button].rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK - MARK_EXCESS;
                Serial.print(" m");
            }
            else
            {
                // Space
                buttons_list[pressed_button].rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK + MARK_EXCESS;
                Serial.print(" s");
            }
            Serial.print(buttons_list[pressed_button].rawCodes[i - 1], DEC);
        }
        Serial.println("");
    }
    else
    {
        VerifyTypeOfSignal(results, pressed_button);
        Serial.println(results->value, HEX);
        buttons_list[pressed_button].codeValue = results->value;
        buttons_list[pressed_button].codeLen = results->bits;
    }
}

void VerifyTypeOfSignal(decode_results *results, int pressed_button)
{
    if (buttons_list[pressed_button].codeType == NEC)
    {
        Serial.print("Received NEC: ");
        if (results->value == REPEAT)
        {
            // Don't record a NEC repeat value as that's useless.
            Serial.println("repeat; ignoring.");
            return;
        }
    }
    else if (buttons_list[pressed_button].codeType == SONY)
    {
        Serial.print("Received SONY: ");
    }
    else if (buttons_list[pressed_button].codeType == RC5)
    {
        Serial.print("Received RC5: ");
    }
    else if (buttons_list[pressed_button].codeType == RC6)
    {
        Serial.print("Received RC6: ");
    }
    else
    {
        Serial.print("Unexpected codeType ");
        Serial.print(buttons_list[pressed_button].codeType, DEC);
        Serial.println("");
    }
}

void sendCode(int repeat, int pressed_button)
{
    if (buttons_list[pressed_button].codeType == NEC)
    {
        sendCodeNec(repeat, pressed_button);
    }
    else if (buttons_list[pressed_button].codeType == SONY)
    {
        sendCodeSony(pressed_button);
    }
    else if (buttons_list[pressed_button].codeType == RC5 || buttons_list[pressed_button].codeType == RC6)
    {
        sendCodeRC5orRC6(repeat, pressed_button);
    }
    else if (buttons_list[pressed_button].codeType == UNKNOWN /* i.e. raw */)
    {
        // Assume 38 KHz
        sendCodeUnknow(pressed_button);
    }
}
void sendCodeNec(int repeat, int pressed_button)
{
    printDadosBotao(pressed_button);
    if (repeat)
    {
        irsend.sendNEC(REPEAT, buttons_list[pressed_button].codeLen);
        Serial.println("Sent NEC repeat");
    }
    else
    {
        irsend.sendNEC(buttons_list[pressed_button].codeType, buttons_list[pressed_button].codeLen);
        Serial.print("Sent NEC ");
        Serial.println(buttons_list[pressed_button].codeType, HEX);
    }

}
void sendCodeSony(int pressed_button)
{
    irsend.sendSony(buttons_list[pressed_button].codeType, buttons_list[pressed_button].codeLen);
    Serial.print("Sent Sony ");
    Serial.println(buttons_list[pressed_button].codeType, HEX);
}
void sendCodeRC5orRC6(int repeat, int pressed_button)
{
    if (!repeat)
    {
        // Flip the toggle bit for a new button press
        buttons_list[pressed_button].toggle = 1 - buttons_list[pressed_button].toggle;
    }
    // Put the toggle bit into the code to send
    buttons_list[pressed_button].codeValue = buttons_list[pressed_button].codeValue & ~(1 << (buttons_list[pressed_button].codeLen - 1));
    buttons_list[pressed_button].codeValue = buttons_list[pressed_button].codeValue | (buttons_list[pressed_button].toggle << (buttons_list[pressed_button].codeLen - 1));
    if (buttons_list[pressed_button].codeType == RC5)
    {
        Serial.print("Sent RC5 ");
        Serial.println(buttons_list[pressed_button].codeValue, HEX);
        irsend.sendRC5(buttons_list[pressed_button].codeValue, buttons_list[pressed_button].codeLen);
    }
    else
    {
        irsend.sendRC6(buttons_list[pressed_button].codeValue, buttons_list[pressed_button].codeLen);
        Serial.print("Sent RC6 ");
        Serial.println(buttons_list[pressed_button].codeValue, HEX);
    }
}

void sendCodeUnknow(int pressed_button)
{
    irsend.sendRaw(buttons_list[pressed_button].rawCodes, buttons_list[pressed_button].codeLen, 38);
    Serial.println("Sent raw");
}



int lastButtonState; //@Lanchinho, seria isto aqui o ultimo estado do btn 1 ?

void printDadosBotao(int pressed_button){

    // int codeType;// -1 should be the default
    // int toggle;  //0 should be the default; The RC5/6 toggle state
    // unsigned long codeValue; // The code value if not raw
    // unsigned int rawCodes[RAWBUF]; // The durations if raw
    // int codeLen; // The length of the code

    Serial.println("Dados Button");
    Serial.println("codeType:");
    Serial.println(buttons_list[pressed_button].codeType);
    Serial.println("toggle:");
    Serial.println(buttons_list[pressed_button].toggle);
    Serial.println("codeValue:");
    Serial.println(buttons_list[pressed_button].codeValue, HEX);
    Serial.println("codeLen:");
    Serial.println(buttons_list[pressed_button].codeLen);

}
//============================================================
// int lastBtnStateOfBtn2;
// int lastBtnStateOfBtn3;
//============================================================

void loop()
{
    // If button pressed, send the code.
    int firstButtonState = digitalRead(FIRST_BUTTON_PIN);

    //========================================================
    // int secondButtonState = digitalRead(SECOND_BUTTON_PIN);
    // int thirdButtonState = digitalRead(THIRD_BUTTON_PIN);
    //========================================================

    if (lastButtonState == HIGH && firstButtonState == LOW)
    {
        Serial.println("Released");
        irrecv.enableIRIn(); // Re-enable receiver
    }

    if (firstButtonState)
    {
        last_pressed_button = 0; //marca o primeiro botao como sendo o pressionado
        Serial.println("Pressed, sending");
        digitalWrite(STATUS_PIN, HIGH);
        sendCode(lastButtonState == firstButtonState, last_pressed_button);
        digitalWrite(STATUS_PIN, LOW);
        delay(50); // Wait a bit between retransmissions
    }
    else if (irrecv.decode(&results))
    {
        printDadosBotao(0);
        digitalWrite(STATUS_PIN, HIGH);
        storeCode(&results, last_pressed_button);
        irrecv.resume(); // resume receiver
        digitalWrite(STATUS_PIN, LOW);
    }
    lastButtonState = firstButtonState;

//==========================================================================
//Btn2
    // if (lastBtnStateOfBtn2 == HIGH && secondButtonState == LOW)
    // {
    //     Serial.println("Released");
    //     irrecv.enableIRIn(); // Re-enable receiver
    // }

    // if (secondButtonState)
    // {
    //     Serial.println("Pressed, sending");
    //     digitalWrite(STATUS_PIN, HIGH);
    //     sendCode(lastBtnStateOfBtn2 == secondButtonState);
    //     digitalWrite(STATUS_PIN, LOW);
    //     delay(50); // Wait a bit between retransmissions
    // }
    // else if (irrecv.decode(&results))
    // {
    //     digitalWrite(STATUS_PIN, HIGH);
    //     storeCode(&results);
    //     irrecv.resume(); // resume receiver
    //     digitalWrite(STATUS_PIN, LOW);
    // }
    // lastBtnStateOfBtn2 = lastBtnStateOfBtn2;

    // //Btn 3
    // if (lastBtnStateOfBtn3 == HIGH && thirdButtonState == LOW)
    // {
    //     Serial.println("Released");
    //     irrecv.enableIRIn(); // Re-enable receiver
    // }

    // if (thirdButtonState)
    // {
    //     Serial.println("Pressed, sending");
    //     digitalWrite(STATUS_PIN, HIGH);
    //     sendCode(lastBtnStateOfBtn3 == thirdButtonState);
    //     digitalWrite(STATUS_PIN, LOW);
    //     delay(50); // Wait a bit between retransmissions
    // }
    // else if (irrecv.decode(&results))
    // {
    //     digitalWrite(STATUS_PIN, HIGH);
    //     storeCode(&results);
    //     irrecv.resume(); // resume receiver
    //     digitalWrite(STATUS_PIN, LOW);
    // }
    // lastBtnStateOfBtn3 = thirdButtonState;
//===========================================================






}
