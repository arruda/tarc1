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
int FIRST_BUTTON_PIN = 2;
//================================================================================================================================
// int SECOND_BUTTON_PIN = 3; //@Lanchinho : nao necessariamente neste pino/ casa, pode ser qlqer outra que esteja livre
// int THIRD_BUTTON_PIN = 4;  //@Lanchinho: Vou deixar todas as cagadas, digo alterações feitas por mim separadas por um "========="
//================================================================================================================================
int STATUS_PIN = 13;

IRrecv irrecv(RECV_PIN);
IRsend irsend; //lembrar de por PWM 3 como o IR LED

decode_results results;

//=====================
//VARIAVEIS GLOBAIS
//=====================
int ultimo_botao_press = FIRST_BUTTON_PIN; //por default o ultimo botao pressionado vai ser o primeiro
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
int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
unsigned int rawCodes[RAWBUF]; // The durations if raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state

// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results)
{
    codeType = results->decode_type;
    int count = results->rawlen;
    if (codeType == UNKNOWN)
    {
        Serial.println("Received unknown code, saving as raw");
        codeLen = results->rawlen - 1;
        // To store raw codes:
        // Drop first value (gap)
        // Convert from ticks to microseconds
        // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
        for (int i = 1; i <= codeLen; i++)
        {
            if (i % 2)
            {
                // Mark
                rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK - MARK_EXCESS;
                Serial.print(" m");
            }
            else
            {
                // Space
                rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK + MARK_EXCESS;
                Serial.print(" s");
            }
            Serial.print(rawCodes[i - 1], DEC);
        }
        Serial.println("");
    }
    else
    {
        VerifyTypeOfSignal(results, codeType);
        Serial.println(results->value, HEX);
        codeValue = results->value;
        codeLen = results->bits;
    }
}
void VerifyTypeOfSignal(decode_results *results, int codeType)
{
    if (codeType == NEC)
    {
        Serial.print("Received NEC: ");
        if (results->value == REPEAT)
        {
            // Don't record a NEC repeat value as that's useless.
            Serial.println("repeat; ignoring.");
            return;
        }
    }
    else if (codeType == SONY)
    {
        Serial.print("Received SONY: ");
    }
    else if (codeType == RC5)
    {
        Serial.print("Received RC5: ");
    }
    else if (codeType == RC6)
    {
        Serial.print("Received RC6: ");
    }
    else
    {
        Serial.print("Unexpected codeType ");
        Serial.print(codeType, DEC);
        Serial.println("");
    }
}

void sendCode(int repeat)
{
    if (codeType == NEC)
    {
        sendCodeNec(repeat);
    }
    else if (codeType == SONY)
    {
        sendCodeSony();
    }
    else if (codeType == RC5 || codeType == RC6)
    {
        sendCodeRC5orRC6(repeat);
    }
    else if (codeType == UNKNOWN /* i.e. raw */)
    {
        // Assume 38 KHz
        sendCodeUnknow();
    }
}
void sendCodeNec(int repeat)
{

    if (repeat)
    {
        irsend.sendNEC(REPEAT, codeLen);
        Serial.println("Sent NEC repeat");
    }
    else
    {
        irsend.sendNEC(codeValue, codeLen);
        Serial.print("Sent NEC ");
        Serial.println(codeValue, HEX);
    }

}
void sendCodeSony()
{
    irsend.sendSony(codeValue, codeLen);
    Serial.print("Sent Sony ");
    Serial.println(codeValue, HEX);
}
void sendCodeRC5orRC6(int repeat)
{
    if (!repeat)
    {
        // Flip the toggle bit for a new button press
        toggle = 1 - toggle;
    }
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5)
    {
        Serial.print("Sent RC5 ");
        Serial.println(codeValue, HEX);
        irsend.sendRC5(codeValue, codeLen);
    }
    else
    {
        irsend.sendRC6(codeValue, codeLen);
        Serial.print("Sent RC6 ");
        Serial.println(codeValue, HEX);
    }
}

void sendCodeUnknow()
{
    irsend.sendRaw(rawCodes, codeLen, 38);
    Serial.println("Sent raw");
}



int lastButtonState; //@Lanchinho, seria isto aqui o ultimo estado do btn 1 ?


//============================================================
int lastBtnStateOfBtn2;
int lastBtnStateOfBtn3;
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
        Serial.println("Pressed, sending");
        digitalWrite(STATUS_PIN, HIGH);
        sendCode(lastButtonState == firstButtonState);
        digitalWrite(STATUS_PIN, LOW);
        delay(50); // Wait a bit between retransmissions
    }
    else if (irrecv.decode(&results))
    {
        digitalWrite(STATUS_PIN, HIGH);
        storeCode(&results);
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
