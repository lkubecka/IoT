//extern "C" {
 // #include "soc/pcnt_struct.h"
//}
#include "driver/pcnt.h"

/* LED pin */
const gpio_num_t ledPin = GPIO_NUM_2;
/* pin that is attached to interrupt */
const byte interruptPin = GPIO_NUM_12;
/* hold the state of LED when toggling */
const gpio_num_t pulsePin = GPIO_NUM_12;

int16_t flow0Counter = 45;

volatile byte state = HIGH;


#define PCNT_TEST_UNIT     PCNT_UNIT_0
#define PCNT_H_LIM_VAL     10
#define PCNT_L_LIM_VAL     -10
#define PCNT_THRESH1_VAL   5
#define PCNT_THRESH0_VAL   -5

/* interrupt function toggle the LED */
/* fungovalo do cca 300kHz vstupního kmitočtu */
void handleInterrupt() {
  state = !state;
  digitalWrite(ledPin, state);
  Serial.println("blink");
}

void setup() {
  Serial.begin(115200);
    
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, state);
  /* set the interrupt pin as input pullup*/
  
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, RISING);
  
  pinMode(pulsePin,INPUT_PULLUP);

  Serial.println("Configured");
 // initPcnt();
}

void loop() {
 // Serial.println("Smycka");
 // readPcntCounter_0();
  delay(1000);
}



/* interrupt function for PCNT */
void IRAM_ATTR pcnt_example_intr_handler(void* arg) {
  Serial.println("pcnt_example_intr_handler");
  uint32_t intr_status;
  intr_status = PCNT.int_st.val;
  int i;

  state = !state;
  digitalWrite(ledPin, state);

  /* smažeme příznak přerušení */
  for(i = 0; i < PCNT_UNIT_MAX; i++) {
    if(intr_status & (BIT(i))) {
      PCNT.int_clr.val = BIT(i);
    }
  }
}

//initialize counter for flow0
void initPcnt() {

  esp_err_t error;

  Serial.println("Inicializace pulse totalizeru");

  pcnt_config_t pcnt_config = {
    pulsePin, // Pulse input gpio_num, if you want to use gpio16, pulse_gpio_num = 16, a negative value will be ignored
    PCNT_PIN_NOT_USED, // Control signal input gpio_num, a negative value will be ignored
    PCNT_MODE_KEEP, // PCNT low control mode
    PCNT_MODE_KEEP, // PCNT high control mode
    PCNT_COUNT_INC, // PCNT positive edge count mode
    PCNT_COUNT_DIS, // PCNT negative edge count mode
    PCNT_H_LIM_VAL, // Maximum counter value
    PCNT_L_LIM_VAL, // Minimum counter value
    PCNT_TEST_UNIT, // PCNT unit number
    PCNT_CHANNEL_0, // the PCNT channel
  };

  if(pcnt_unit_config(&pcnt_config) == ESP_OK) //init unit
    Serial.println("Config Unit_0 = ESP_OK");

  /*Configure input filter value*/
  pcnt_set_filter_value(PCNT_TEST_UNIT, 100);
  /*Enable input filter*/
  pcnt_filter_enable(PCNT_TEST_UNIT);

  /*Set value for watch point thresh1*/
  pcnt_set_event_value(PCNT_TEST_UNIT, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
  /*Enable watch point event of thresh1*/
  //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_THRES_1);
  pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_THRES_1);
  /*Set value for watch point thresh0*/
  pcnt_set_event_value(PCNT_TEST_UNIT, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
  /*Enable watch point event of thresh0*/
  //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_THRES_0);
  pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_THRES_0);
  /*Enable watch point event of h_lim*/
  //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_H_LIM);
  pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_H_LIM);
  /*Enable watch point event of l_lim*/
  //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_L_LIM);
  pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_L_LIM);
  /*Enable watch point event of zero*/
  //pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_ZERO);
  pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_ZERO);

  /*Pause counter*/
  pcnt_counter_pause(PCNT_TEST_UNIT);
  /*Reset counter value*/
  pcnt_counter_clear(PCNT_TEST_UNIT);
  /*Register ISR handler*/
  pcnt_isr_register(pcnt_example_intr_handler, NULL, 0, NULL);
  /*Enable interrupt for PCNT unit*/
  pcnt_intr_enable(PCNT_TEST_UNIT);
  /*Resume counting*/
  pcnt_counter_resume(PCNT_TEST_UNIT);

}

void readPcntCounter_0() {
 // if(pcnt_get_counter_value(PCNT_UNIT_0, &flow0Counter) == ESP_OK)
 //   Serial.println("Read pcnt unit 0");
  delay(10);
  Serial.print("flow0Counts = ");
  Serial.println(flow0Counter);
}
